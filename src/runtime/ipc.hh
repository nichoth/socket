#ifndef SOCKET_RUNTIME_IPC_H
#define SOCKET_RUNTIME_IPC_H

#include "webview/preload.hh"
#include "webview/client.hh"
#include "queued_response.hh"
#include "unique_client.hh"
#include "crypto.hh"
#include "bytes.hh"
#include "core.hh"
#include "http.hh"
#include "url.hh"

namespace ssc::runtime::bridge {
  // forward
  class Bridge;
}

namespace ssc::runtime::ipc {
  /**
   * A `Client` represents a unique caller of the IPC channel in a webview
   * or the runtime.
   */
  struct Client : public webview::Client {
    using webview::Client::Client;

    Client (const webview::Client& client)
      : webview::Client(client)
    {}
  };

  struct MessageCancellation {
    void (*handler)(void*) = nullptr;
    void* data = nullptr;
  };

  class Message {
    public:
      using Seq = String;

      bytes::BufferQueue buffer;
      Client client;
      URL uri;

      String value = "";
      String name = "";
      String href = "";
      int index = -1;
      Seq seq = "";

      bool isHTTP = false;

      SharedPointer<MessageCancellation> cancel = nullptr;

      Message () = default;
      Message (const String& source, bool decodeValues);
      Message (const String& source);
      Message (const Message& message);
      Message (Message&& message);

      Message& operator = (const Message&);
      Message& operator = (Message&&);

      bool has (const String& key) const;
      bool contains (const String& key) const;
      const String& at (const String& key) const;
      const String get (const String& key) const;
      const String get (const String& key, const String& fallback) const;
      const Map<String, String> dump () const;
      const String str () const;
      const Map<String, String> map () const;
      const JSON::Object json () const;
  };

  class Result {
    public:
      class Err {
        public:
          Message message;
          Message::Seq seq;
          JSON::Any value;
          Err () = default;
          Err (const Message&, const char*);
          Err (const Message&, const String&);
          Err (const Message&, const JSON::Any&);
      };

      class Data {
        public:
          Message message;
          Message::Seq seq;
          JSON::Any value;
          QueuedResponse queuedResponse;
          Data () = default;
          Data (const Message&, const JSON::Any&);
          Data (const Message&, const JSON::Any&, const QueuedResponse&);
      };

      Message message;
      Message::Seq seq = "-1";
      uint64_t id = crypto::rand64();
      String source = "";
      String token = "";
      JSON::Any value = nullptr;
      JSON::Any data = nullptr;
      JSON::Any err = nullptr;
      http::Headers headers;
      QueuedResponse queuedResponse;

      Result () = default;
      Result (const Result&) = default;
      Result (const JSON::Any&, const String& token = "");
      Result (const Err& error);
      Result (const Data& data);
      Result (const Message::Seq&, const Message&);
      Result (const Message::Seq&, const Message&, const JSON::Any&);
      Result (
        const Message::Seq&,
        const Message&,
        const JSON::Any&,
        const QueuedResponse&
      );

      const String str () const;
      const JSON::Any json () const;
  };

  class Router {
    public:
      using ReplyCallback = Function<void(const Result)>;
      using ResultCallback = Function<void(Result)>;
      using MessageCallback = Function<void(
        const Message,
        Router*,
        ReplyCallback
      )>;

      struct MessageCallbackContext {
        bool async = true;
        MessageCallback callback;
      };

      struct MessageCallbackListenerContext {
        uint64_t token;
        MessageCallback callback;
      };

      using Table = Map<String, MessageCallbackContext>;
      using Listeners = Map<String, Vector<MessageCallbackListenerContext>>;

    private:
      Table preserved;

    public:
      context::Dispatcher& dispatcher;
      bridge::Bridge& bridge;

      Listeners listeners;
      Mutex mutex;
      Table table;

      // owned by bridge
      Router (bridge::Bridge&);

      // the `Router` instance is strictly owned and cannot be copied or moved
      Router () = delete;
      Router (const Router&) = delete;
      Router (const Router&&) = delete;
      Router (Router&&) = delete;
      Router& operator = (const Router&) = delete;
      Router& operator = (Router&&) = delete;

      void init ();
      void mapRoutes ();
      void preserveCurrentTable ();
      uint64_t listen (const String& name, const MessageCallback callback);
      bool unlisten (const String& name, uint64_t token);
      void map (const String& name, const MessageCallback callback);
      void map (const String& name, bool async, const MessageCallback callback);
      void unmap (const String& name);
      bool invoke (const String& uri, const ResultCallback callback);
      bool invoke (const String& uri, SharedPointer<unsigned char[]> bytes, size_t size);
      bool invoke (const String&, SharedPointer<unsigned char[]>, size_t, const ResultCallback);
      bool invoke (const Message&, SharedPointer<unsigned char[]>, size_t, const ResultCallback);
  };

  /**
   * The `Bridge` interface for an IPC channel.
   */
  class IBridge {
    public:
      context::RuntimeContext& context;
      context::Dispatcher& dispatcher;
      Map<String, String> userConfig;
      Client client;
      int index = 0;

      IBridge (
        context::Dispatcher& dispatcher,
        context::RuntimeContext& context,
        const Client& client,
        Map<String, String> userConfig
      ) : dispatcher(dispatcher),
          userConfig(userConfig),
          context(context),
          client(client)
      {}

      virtual ~IBridge () {};
      virtual bool active () const = 0;
      virtual bool emit (const String&, const String& = "") = 0;
      virtual bool emit (const String&, const JSON::Any& = {}) = 0;
      virtual bool send (const Message::Seq&, const String&, const QueuedResponse& = {}) = 0;
      virtual bool send (const Message::Seq& seq, const JSON::Any& json, const QueuedResponse& = {}) = 0;
      virtual bool route ( const String&, SharedPointer<unsigned char[]>, size_t) = 0;
      virtual bool route (const String&, SharedPointer<unsigned char[]>, size_t, const Router::ResultCallback) = 0;
      virtual bool dispatch (const context::DispatchCallback) = 0;
      virtual Runtime* getRuntime () = 0;
      virtual const Runtime* getRuntime () const = 0;
  };
}
#endif
