#include "core.hh"
#include "modules/fs.hh"

namespace SSC {
#if SOCKET_RUNTIME_PLATFORM_LINUX
  struct UVSource {
    GSource base; // should ALWAYS be first member
    gpointer tag;
    Core *core;
  };
#endif

  Post Core::getPost (uint64_t id) {
    if (this->posts.find(id) == this->posts.end()) {
      return Post{};
    }

    return posts.at(id);
  }

  void Core::shutdown () {
    if (this->isShuttingDown || this->isPaused) {
      return;
    }

    this->isShuttingDown = true;
    this->pause();

  #if !SOCKET_RUNTIME_PLATFORM_IOS
    this->childProcess.shutdown();
  #endif

    this->stopEventLoop();
    this->isShuttingDown = false;
  }

  void Core::resume () {
    if (!this->isPaused) {
      return;
    }

    this->isPaused = false;
    this->runEventLoop();

    if (this->options.features.useUDP) {
      this->udp.resumeAllSockets();
    }

    if (options.features.useNetworkStatus) {
      this->networkStatus.start();
    }

    if (options.features.useConduit) {
      this->conduit.start();
    }

    if (options.features.useNotifications) {
      this->notifications.start();
    }
  }

  void Core::pause () {
    if (this->isPaused) {
      return;
    }

    this->isPaused = true;

    if (this->options.features.useUDP) {
      this->udp.pauseAllSockets();
    }

    if (options.features.useNetworkStatus) {
      this->networkStatus.stop();
    }

    if (options.features.useConduit) {
      this->conduit.stop();
    }

    if (options.features.useNotifications) {
      this->notifications.stop();
    }

  #if !SOCKET_RUNTIME_PLATFORM_ANDROID
    this->pauseEventLoop();
  #endif
  }

  void Core::stop () {
    Lock lock(this->mutex);
    this->stopEventLoop();
  #if SOCKET_RUNTIME_PLATFORM_LINUX
    if (this->gsource) {
      const auto id = g_source_get_id(this->gsource);
      if (id > 0) {
        g_source_remove(id);
      }

      g_object_unref(this->gsource);
      this->gsource = nullptr;
      this->didInitGSource = false;
    }
  #endif
  }

  bool Core::hasPost (uint64_t id) {
    return posts.find(id) != posts.end();
  }

  bool Core::hasPostBody (const char* body) {
    if (body == nullptr) return false;
    for (const auto& tuple : posts) {
      auto post = tuple.second;
      if (post.body.get() == body) return true;
    }
    return false;
  }

  void Core::expirePosts () {
    Lock lock(this->mutex);
    const auto now = std::chrono::system_clock::now()
      .time_since_epoch()
      .count();

    Vector<uint64_t> ids;
    for (auto const &tuple : posts) {
      auto id = tuple.first;
      auto post = tuple.second;

      if (post.ttl < now) {
        ids.push_back(id);
      }
    }

    for (auto const id : ids) {
      removePost(id);
    }
  }

  void Core::putPost (uint64_t id, Post p) {
    Lock lock(this->mutex);
    p.ttl = std::chrono::time_point_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() +
      std::chrono::milliseconds(32 * 1024)
    )
      .time_since_epoch()
      .count();

    this->posts.insert_or_assign(id, p);
  }

  void Core::removePost (uint64_t id) {
    Lock lock(this->mutex);

    if (this->posts.find(id) != this->posts.end()) {
      // debug("remove post %ld", this->posts.at(id).body.use_count());
      posts.erase(id);
    }
  }

  String Core::createPost (String seq, String params, Post post) {
    if (post.id == 0) {
      post.id = rand64();
    }

    auto sid = std::to_string(post.id);
    auto js = createJavaScript("post-data.js",
      "const globals = await import('socket:internal/globals');              \n"
      "const id = `" + sid + "`;                                             \n"
      "const seq = `" + seq + "`;                                            \n"
      "const workerId = `" + post.workerId + "`.trim() || null;              \n"
      "const headers = `" + trim(post.headers) + "`                          \n"
      "  .trim()                                                             \n"
      "  .split(/[\\r\\n]+/)                                                 \n"
      "  .filter(Boolean)                                                    \n"
      "  .map((header) => header.trim());                                    \n"
      "                                                                      \n"
      "let params = `" + params + "`;                                        \n"
      "                                                                      \n"
      "try {                                                                 \n"
      "  params = JSON.parse(params);                                        \n"
      "} catch (err) {                                                       \n"
      "  console.error(err.stack || err, params);                            \n"
      "}                                                                     \n"
      "                                                                      \n"
      "globals.get('RuntimeXHRPostQueue').dispatch(                          \n"
      "  id,                                                                 \n"
      "  seq,                                                                \n"
      "  params,                                                             \n"
      "  headers,                                                            \n"
      "  { workerId }                                                        \n"
      ");                                                                    \n"
    );

    putPost(post.id, post);
    return js;
  }

  void Core::removeAllPosts () {
    Lock lock(this->mutex);
    Vector<uint64_t> ids;

    for (auto const &tuple : posts) {
      auto id = tuple.first;
      ids.push_back(id);
    }

    for (auto const id : ids) {
      removePost(id);
    }
  }

#if SOCKET_RUNTIME_PLATFORM_LINUX
  // @see https://api.gtkd.org/glib.c.types.GSourceFuncs.html
  static GSourceFuncs loopSourceFunctions = {
    .prepare = [](GSource *source, gint *timeout) -> gboolean {
      auto core = reinterpret_cast<UVSource *>(source)->core;
      if (!core->isLoopRunning) {
        return false;
      }

      if (!core->isLoopAlive()) {
        return true;
      }

      *timeout = core->getEventLoopTimeout();
      return *timeout == 0;
    },

    .check = [](GSource* source) -> gboolean {
      auto core = reinterpret_cast<UVSource *>(source)->core;
      auto tag = reinterpret_cast<UVSource *>(source)->tag;
      const auto timeout = core->getEventLoopTimeout();

      if (timeout == 0) {
        return true;
      }

      const auto condition = g_source_query_unix_fd(source, tag);
      return (
        ((condition & G_IO_IN) == G_IO_IN) ||
        ((condition & G_IO_OUT) == G_IO_OUT)
      );
    },

    .dispatch = [](
      GSource *source,
      GSourceFunc callback,
      gpointer user_data
    ) -> gboolean {
      auto core = reinterpret_cast<UVSource *>(source)->core;
      auto loop = core->getEventLoop();
      uv_run(loop, UV_RUN_NOWAIT);
      return G_SOURCE_CONTINUE;
    }
  };
#endif

  void Core::initEventLoop () {
    if (didLoopInit) {
      return;
    }

    didLoopInit = true;
    Lock lock(this->mutex);
    uv_loop_init(&this->eventLoop);
    uv_loop_set_data(&this->eventLoop, reinterpret_cast<void*>(this));
    this->eventLoopAsync.data = reinterpret_cast<void*>(this);

    uv_async_init(&this->eventLoop, &this->eventLoopAsync, [](uv_async_t *handle) {
      auto core = reinterpret_cast<Core*>(handle->data);

      while (true) {
        Function<void()> dispatch = nullptr;

        do {
          Lock lock(core->mutex);
          if (core->eventLoopDispatchQueue.size() > 0) {
            dispatch = core->eventLoopDispatchQueue.front();
            core->eventLoopDispatchQueue.pop();
          }
        } while (0);

        if (dispatch == nullptr) {
          break;
        }

        dispatch();
      }
    });

  #if SOCKET_RUNTIME_PLATFORM_LINUX
    if (!this->options.dedicatedLoopThread && !this->didInitGSource) {
      if (this->gsource) {
        const auto id = g_source_get_id(this->gsource);
        if (id > 0) {
          g_source_remove(id);
        }

        g_object_unref(this->gsource);
        this->gsource = nullptr;
      }

      this->gsource = g_source_new(&loopSourceFunctions, sizeof(UVSource));

      UVSource *uvsource = reinterpret_cast<UVSource*>(gsource);
      uvsource->core = this;
      uvsource->tag = g_source_add_unix_fd(
        this->gsource,
        uv_backend_fd(&this->eventLoop),
        (GIOCondition) (G_IO_IN | G_IO_OUT | G_IO_ERR)
      );

      g_source_set_priority(this->gsource, G_PRIORITY_HIGH);
      g_source_attach(this->gsource, nullptr);
      this->didInitGSource = true;
    }
  #endif
  }

  uv_loop_t* Core::getEventLoop () {
    this->initEventLoop();
    return &this->eventLoop;
  }

  int Core::getEventLoopTimeout () {
    auto loop = this->getEventLoop();
    uv_update_time(loop);
    return uv_backend_timeout(loop);
  }

  bool Core::isLoopAlive () {
    return uv_loop_alive(this->getEventLoop());
  }

  void Core::pauseEventLoop() {
  #if !SOCKET_RUNTIME_PLATFORM_LINUX
    // wait for drain of event loop dispatch queue
    while (true) {
      Lock lock(this->mutex);
      if (this->eventLoopDispatchQueue.size() == 0) {
        break;
      }
    }
  #endif

    this->isLoopRunning = false;
    do {
      Lock lock(this->mutex);
      uv_stop(&this->eventLoop);
    } while (0);

  #if !SOCKET_RUNTIME_PLATFORM_APPLE
    #if SOCKET_RUNTIME_PLATFORM_LINUX
      if (this->options.dedicatedLoopThread) {
    #endif
      if (this->eventLoopThread != nullptr) {
        if (this->isPollingEventLoop && eventLoopThread->joinable()) {
          this->eventLoopThread->join();
        }

        delete this->eventLoopThread;
        this->eventLoopThread = nullptr;
      }
    #if SOCKET_RUNTIME_PLATFORM_LINUX
      }
    #endif
  #endif
  }

  void Core::stopEventLoop() {
    if (this->isLoopRunning) {
      return;
    }

    this->isLoopRunning = false;
    Lock lock(this->mutex);
    uv_stop(&eventLoop);
  #if !SOCKET_RUNTIME_PLATFORM_APPLE
    #if SOCKET_RUNTIME_PLATFORM_LINUX
      if (this->options.dedicatedLoopThread) {
    #endif
      if (eventLoopThread != nullptr) {
        if (this->isPollingEventLoop && eventLoopThread->joinable()) {
          eventLoopThread->join();
        }

        delete eventLoopThread;
        eventLoopThread = nullptr;
      }
    #if SOCKET_RUNTIME_PLATFORM_LINUX
      }
    #endif
  #endif

    uv_loop_close(&eventLoop);
  }

  void Core::sleepEventLoop (int64_t ms) {
    if (ms > 0) {
      auto timeout = this->getEventLoopTimeout();
      ms = timeout > ms ? timeout : ms;
      msleep(ms);
    }
  }

  void Core::sleepEventLoop () {
    this->sleepEventLoop(this->getEventLoopTimeout());
  }

  void Core::signalDispatchEventLoop () {
    Lock lock(this->mutex);
    this->initEventLoop();
    this->runEventLoop();
    uv_async_send(&this->eventLoopAsync);
  }

  void Core::dispatchEventLoop (EventLoopDispatchCallback callback) {
    {
      Lock lock(this->mutex);
      this->eventLoopDispatchQueue.push(callback);
    }

    this->signalDispatchEventLoop();
  }

  static void pollEventLoop (Core *core) {
    core->isPollingEventLoop = true;
    auto loop = core->getEventLoop();

    while (core->isLoopRunning) {
      core->sleepEventLoop(EVENT_LOOP_POLL_TIMEOUT);

      do {
        uv_run(loop, UV_RUN_DEFAULT);
      } while (core->isLoopRunning && core->isLoopAlive());
    }

    core->isPollingEventLoop = false;
    core->isLoopRunning = false;
  }

  void Core::runEventLoop () {
    if (
      this->isShuttingDown ||
      this->isLoopRunning ||
      this->isPaused
    ) {
      return;
    }

    this->isLoopRunning = true;
    this->isPaused = false;

    this->initEventLoop();
    this->dispatchEventLoop([=, this]() {
      this->initTimers();
      this->startTimers();
    });

  #if SOCKET_RUNTIME_PLATFORM_APPLE
    Lock lock(this->mutex);
    dispatch_async(this->eventLoopQueue, ^{
      pollEventLoop(this);
    });
  #else
  #if SOCKET_RUNTIME_PLATFORM_LINUX
    if (this->options.dedicatedLoopThread) {
  #endif
    Lock lock(this->mutex);
    // clean up old thread if still running
    if (this->eventLoopThread != nullptr) {
      if (!this->isPollingEventLoop && this->eventLoopThread->joinable()) {
        this->eventLoopThread->join();
      }

      delete this->eventLoopThread;
      this->eventLoopThread = nullptr;
    }

    this->eventLoopThread = new std::thread(
      &pollEventLoop,
      this
    );
  #if SOCKET_RUNTIME_PLATFORM_LINUX
    }
  #endif
  #endif
  }

  struct Timer {
    uv_timer_t handle;
    bool repeated = false;
    bool started = false;
    uint64_t timeout = 0;
    uint64_t interval = 0;
    uv_timer_cb invoke;
  };

  static Timer releaseStrongReferenceDescriptors = {
    .repeated = true,
    .timeout = 1024, // in milliseconds
    .invoke = [](uv_timer_t *handle) {
      auto core = reinterpret_cast<Core *>(handle->data);
      Vector<CoreFS::ID> ids;
      String msg = "";

      {
        Lock lock(core->fs.mutex);
        for (auto const &tuple : core->fs.descriptors) {
          ids.push_back(tuple.first);
        }
      }

      for (auto const id : ids) {
        Lock lock(core->fs.mutex);
        if (!core->fs.descriptors.contains(id)) {
          continue;
        }

        auto desc = core->fs.descriptors.at(id);

        if (desc == nullptr) {
          core->fs.descriptors.erase(id);
          continue;
        }

        if (desc->isRetained() || !desc->isStale()) {
          continue;
        }

        if (desc->isDirectory()) {
          core->fs.closedir("", id, [](auto seq, auto msg, auto post) {});
        } else if (desc->isFile()) {
          core->fs.close("", id, [](auto seq, auto msg, auto post) {});
        } else {
          // free
          core->fs.descriptors.erase(id);
        }
      }
    }
  };

  #define RELEASE_STRONG_REFERENCE_SHARED_POINTER_BUFFERS_RESOLUTION 8

  static Timer releaseStrongReferenceSharedPointerBuffers = {
    .repeated = true,
    .timeout = RELEASE_STRONG_REFERENCE_SHARED_POINTER_BUFFERS_RESOLUTION, // in milliseconds
    .invoke = [](uv_timer_t *handle) {
      auto core = reinterpret_cast<Core *>(handle->data);
      static constexpr auto resolution = RELEASE_STRONG_REFERENCE_SHARED_POINTER_BUFFERS_RESOLUTION;
      Lock lock(core->mutex);
      for (int i = 0; i < core->sharedPointerBuffers.size(); ++i) {
        auto entry = &core->sharedPointerBuffers[i];
        if (entry == nullptr) {
          continue;
        }

        // expired
        if (entry->ttl <= resolution) {
          entry->pointer = nullptr;
          entry->ttl = 0;
          if (i == core->sharedPointerBuffers.size() - 1) {
            core->sharedPointerBuffers.pop_back();
            break;
          }
        } else {
          entry->ttl = entry->ttl - resolution;
        }
      }

      while (
        core->sharedPointerBuffers.size() > 0 &&
        core->sharedPointerBuffers.back().pointer == nullptr
      ) {
        core->sharedPointerBuffers.pop_back();
      }

      if (core->sharedPointerBuffers.size() == 0) {
        uv_timer_stop(&releaseStrongReferenceSharedPointerBuffers.handle);
      }
    }
  };

  void Core::initTimers () {
    if (this->didTimersInit) {
      return;
    }

    Lock lock(this->mutex);

    auto loop = this->getEventLoop();

    Vector<Timer*> timersToInit = {
      &releaseStrongReferenceDescriptors,
      &releaseStrongReferenceSharedPointerBuffers
    };

    for (const auto& timer : timersToInit) {
      uv_timer_init(loop, &timer->handle);
      timer->handle.data = (void *) this;
    }

    this->didTimersInit = true;
  }

  void Core::startTimers () {
    Lock lock(this->mutex);

    Vector<Timer*> timersToStart = {
      &releaseStrongReferenceDescriptors,
      &releaseStrongReferenceSharedPointerBuffers
    };

    for (const auto &timer : timersToStart) {
      if (timer->started) {
        uv_timer_again(&timer->handle);
      } else {
        timer->started = 0 == uv_timer_start(
          &timer->handle,
          timer->invoke,
          timer->timeout,
          !timer->repeated
            ? 0
            : timer->interval > 0
              ? timer->interval
              : timer->timeout
        );
      }
    }

    this->didTimersStart = true;
  }

  void Core::stopTimers () {
    if (this->didTimersStart == false) {
      return;
    }

    Lock lock(this->mutex);

    Vector<Timer*> timersToStop = {
      &releaseStrongReferenceDescriptors,
      &releaseStrongReferenceSharedPointerBuffers
    };

    for (const auto& timer : timersToStop) {
      if (timer->started) {
        uv_timer_stop(&timer->handle);
      }
    }

    this->didTimersStart = false;
  }

  const CoreTimers::ID Core::setTimeout (
    uint64_t timeout,
    const CoreTimers::TimeoutCallback& callback
  ) {
    return this->timers.setTimeout(timeout, callback);
  }

  const CoreTimers::ID Core::setImmediate (
    const CoreTimers::ImmediateCallback& callback
  ) {
    return this->timers.setImmediate(callback);
  }

  const CoreTimers::ID Core::setInterval (
    uint64_t interval,
    const CoreTimers::IntervalCallback& callback
  ) {
    return this->timers.setInterval(interval, callback);
  }

  bool Core::clearTimeout (const CoreTimers::ID id) {
    return this->timers.clearTimeout(id);
  }

  bool Core::clearImmediate (const CoreTimers::ID id) {
    return this->timers.clearImmediate(id);
  }

  bool Core::clearInterval (const CoreTimers::ID id) {
    return this->timers.clearInterval(id);
  }

  void Core::retainSharedPointerBuffer (
    SharedPointer<char[]> pointer,
    unsigned int ttl
  ) {
    if (pointer == nullptr) {
      return;
    }

    Lock lock(this->mutex);

    this->sharedPointerBuffers.emplace_back(SharedPointerBuffer {
      pointer,
      ttl
    });

    uv_timer_again(&releaseStrongReferenceSharedPointerBuffers.handle);
  }

  void Core::releaseSharedPointerBuffer (SharedPointer<char[]> pointer) {
    if (pointer == nullptr) {
      return;
    }

    Lock lock(this->mutex);
    for (auto& entry : this->sharedPointerBuffers) {
      if (entry.pointer.get() == pointer.get()) {
        entry.pointer = nullptr;
        entry.ttl = 0;
        return;
      }
    }
  }
}
