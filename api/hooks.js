/* global ApplicationURLEvent */
/**
 * @module hooks
 *
 * An interface for registering callbacks for various hooks in
 * the runtime.
 *
 * Example usage:
 * ```js
 * import hooks from 'socket:hooks'
 *
 * hooks.onLoad(() => {
 *   // called when the runtime context has loaded,
 *   // but not ready or initialized
 * })
 *
 * hooks.onReady(() => {
 *   // called when the runtime is ready, but not initialized
 * })
 *
 * hooks.onInit(() => {
 *   // called when the runtime is initialized
 * })
 *
 * hooks.onError((event) => {
 *   // called when 'error', 'messageerror', and 'unhandledrejection' error
 *   // events are dispatched on the global object
 * })
 *
 * hooks.onData((event) => {
 *   // called when 'data' events are dispatched on the global object
 * })
 *
 * hooks.onMessage((event) => {
 *   // called when 'message' events are dispatched on the global object
 * })
 *
 * hooks.onOnline((event) => {
 *   // called when 'online' events are dispatched on the global object
 * })
 *
 * hooks.onOffline((event) => {
 *   // called when 'offline' events are dispatched on the global object
 * })
 *
 * hooks.onLanguageChange((event) => {
 *   // called when 'languagechange' events are dispatched on the global object
 * })
 *
 * hooks.onPermissionChange((event) => {
 *   // called when 'permissionchange' events are dispatched on the global object
 * })
 *
 * hooks.onNotificationResponse((event) => {
 *   // called when 'notificationresponse' events are dispatched on the global object
 * })
 *
 * hooks.onNotificationPresented((event) => {
 *   // called when 'notificationpresented' events are dispatched on the global object
 * })
 *
 * hooks.onApplicationURL((event) => {
 *   // called when 'applicationurl' events are dispatched on the global object
 * })
 *
 * hooks.onApplicationResume((event) => {
 *   // called when 'applicationresume' events are dispatched on the global object
 * })
 *
 * hooks.onApplicationPause((event) => {
 *   // called when 'applicationpause' events are dispatched on the global object
 * })
 * ```
 */
import { Event, CustomEvent, ErrorEvent, MessageEvent } from './events.js'
import { toProperCase } from './util.js'
import location from './location.js'

/**
 * @typedef {{ signal?: AbortSignal }} WaitOptions
 */

function addEventListener (target, type, callback, ...args) {
  target.addEventListener(type, callback, ...args)
}

function addEventListenerOnce (target, type, callback) {
  target.addEventListener(type, callback, { once: true })
}

async function waitForEvent (target, type, timeout = -1) {
  return await new Promise((resolve) => {
    if (timeout > -1) {
      setTimeout(resolve, timeout)
    }
    addEventListenerOnce(target, type, resolve)
  })
}

function dispatchEvent (target, event) {
  queueMicrotask(() => target.dispatchEvent(event))
}

function dispatchInitEvent (target) {
  dispatchEvent(target, new InitEvent())
}

function dispatchLoadEvent (target) {
  dispatchEvent(target, new LoadEvent())
}

function dispatchReadyEvent (target) {
  dispatchEvent(target, new ReadyEvent())
}

function proxyGlobalEvents (global, target) {
  for (const type of GLOBAL_EVENTS) {
    try {
      const globalObject = GLOBAL_TOP_LEVEL_EVENTS.includes(type)
        ? global.top ?? global
        : global

      addEventListener(globalObject, type, (event) => {
        const { type, data, detail = null, error } = event
        const { origin } = location

        if (type === 'applicationurl') {
          dispatchEvent(target, new ApplicationURLEvent(type, {
            origin,
            data: event.data,
            url: event.url.toString()
          }))
        } else if (type === 'error' || error) {
          const { message, filename = import.meta.url || globalThis.location.href } = error || {}
          dispatchEvent(target, new ErrorEvent(type, {
            message,
            filename,
            error,
            detail,
            origin
          }))
        } else if (data || type === 'message') {
          dispatchEvent(target, new MessageEvent(type, event))
        } else if (detail) {
          dispatchEvent(target, new CustomEvent(type, event))
        } else {
          dispatchEvent(target, new Event(type, event))
        }
      })
    } catch (err) {}
  }
}

// state
let isGlobalLoaded = false

export const RUNTIME_INIT_EVENT_NAME = '__runtime_init__'

export const GLOBAL_EVENTS = [
  RUNTIME_INIT_EVENT_NAME,
  'applicationurl',
  'applicationpause',
  'applicationresume',
  'data',
  'error',
  'init',
  'languagechange',
  'load',
  'message',
  'messageerror',
  'notificationpresented',
  'notificationresponse',
  'offline',
  'online',
  'permissionchange',
  'unhandledrejection'
]

const GLOBAL_TOP_LEVEL_EVENTS = [
  'applicationurl',
  'applicationpause',
  'applicationresume',
  'data',
  'languagechange',
  'notificationpresented',
  'notificationresponse',
  'offline',
  'online',
  'permissionchange',
  'unhandledrejection'
]

/**
 * An event dispatched when the runtime has been initialized.
 */
export class InitEvent extends Event {
  constructor () { super('init') }
}

/**
 * An event dispatched when the runtime global has been loaded.
 */
export class LoadEvent extends Event {
  constructor () { super('load') }
}

/**
 * An event dispatched when the runtime is considered ready.
 */
export class ReadyEvent extends Event {
  constructor () { super('ready') }
}

/**
 * An event dispatched when the runtime has been initialized.
 */
export class RuntimeInitEvent extends Event {
  constructor () { super(RUNTIME_INIT_EVENT_NAME) }
}

/**
 * An interface for registering callbacks for various hooks in
 * the runtime.
 */
// eslint-disable-next-line new-parens
export class Hooks extends EventTarget {
  /**
   * @ignore
   */
  static GLOBAL_EVENTS = GLOBAL_EVENTS

  /**
   * @ignore
   */
  static InitEvent = InitEvent

  /**
   * @ignore
   */
  static LoadEvent = LoadEvent

  /**
   * @ignore
   */
  static ReadyEvent = ReadyEvent

  /**
   * @ignore
   */
  static RuntimeInitEvent = RuntimeInitEvent

  /**
   * `Hooks` class constructor
   * @ignore
   */
  constructor () {
    super()
    this.#init()
  }

  /**
   * An array of all global events listened to in various hooks
   */
  get globalEvents () {
    return GLOBAL_EVENTS
  }

  /**
   * Reference to global object
   * @type {object}
   */
  get global () {
    return globalThis
  }

  /**
   * Returns `document` in global.
   * @type {Document}
   */
  get document () {
    return this.global.document ?? null
  }

  /**
   * Returns `document` in global.
   * @type {Window}
   */
  get window () {
    return this.global.window ?? null
  }

  /**
   * Predicate for determining if the global document is ready.
   * @type {boolean}
   */
  get isDocumentReady () {
    return this.document?.readyState === 'complete'
  }

  /**
   * Predicate for determining if the global object is ready.
   * @type {boolean}
   */
  get isGlobalReady () {
    return isGlobalLoaded
  }

  /**
   * Predicate for determining if the runtime is ready.
   * @type {boolean}
   */
  get isRuntimeReady () {
    return Boolean(globalThis.__RUNTIME_INIT_NOW__)
  }

  /**
   * Predicate for determining if everything is ready.
   * @type {boolean}
   */
  get isReady () {
    return this.isRuntimeReady && (this.isWorkerContext || this.isDocumentReady)
  }

  /**
   * Predicate for determining if the runtime is working online.
   * @type {boolean}
   */
  get isOnline () {
    return this.global.navigator?.onLine || false
  }

  /**
   * Predicate for determining if the runtime is in a Worker context.
   * @type {boolean}
   */
  get isWorkerContext () {
    return Boolean(!this.document && !this.global.window && this.global.self)
  }

  /**
   * Predicate for determining if the runtime is in a Window context.
   * @type {boolean}
   */
  get isWindowContext () {
    return Boolean(this.document && this.global.window)
  }

  /**
   * Internal hooks initialization.
   * Event order:
   *  1. 'DOMContentLoaded'
   *  2. 'load'
   *  3. 'init'
   * @ignore
   * @private
   */
  async #init () {
    const { isWorkerContext, document, global } = this
    const readyState = document?.readyState

    proxyGlobalEvents(global, this)

    // if runtime is initialized, then 'DOMContentLoaded' (document),
    // 'load' (window), and the 'init' (window) events have all been dispatched
    // prior to hook initialization
    if (this.isRuntimeReady) {
      dispatchLoadEvent(this)
      dispatchInitEvent(this)
      dispatchReadyEvent(this)
      return
    }

    addEventListenerOnce(global, RUNTIME_INIT_EVENT_NAME, () => {
      dispatchInitEvent(this)
      dispatchReadyEvent(this)
    })

    if (!isWorkerContext && readyState !== 'complete') {
      const pending = []
      pending.push(waitForEvent(global, 'load', 500))
      if (document) {
        pending.push(waitForEvent(document, 'DOMContentLoaded'))
      }

      await Promise.race(pending)
    }

    isGlobalLoaded = true
    dispatchLoadEvent(this)
  }

  /**
   * Wait for a hook event to occur.
   * @template {Event | T extends Event}
   * @param {string|function} nameOrFunction
   * @param {WaitOptions=} [options]
   * @return {Promise<T>}
   */
  async wait (nameOrFunction, options = null) {
    const signal = options?.signal ?? null

    if (typeof nameOrFunction === 'string') {
      const name = /** @type {string} */ (nameOrFunction)
      const method = `on${toProperCase(name.toLowerCase())}`

      if (typeof this[method] === 'function') {
        return await new Promise((resolve) => {
          const removeEventListener = this[method](resolve)

          if (signal?.aborted) {
            removeEventListener()
          } else if (signal) {
            addEventListenerOnce(signal, 'abort', removeEventListener)
          }
        })
      }
    } else if (typeof nameOrFunction === 'function') {
      const descriptor = Object.getOwnPropertyDescriptor(
        this.constructor.prototype,
        /** @type {function} */ (nameOrFunction).name
      )

      if (descriptor?.value === nameOrFunction) {
        return await new Promise((resolve) => {
          const removeEventListener = /** @type {function} */ (nameOrFunction)
            .call(this, resolve)

          if (signal?.aborted) {
            removeEventListener()
          } else if (signal) {
            addEventListenerOnce(signal, 'abort', removeEventListener)
          }
        })
      }
    }

    throw new TypeError(`${nameOrFunction} is not a valid hook to wait for`)
  }

  /**
   * Wait for the global Window, Document, and Runtime to be ready.
   * The callback function is called exactly once.
   * @param {function} callback
   * @return {function}
   */
  onReady (callback) {
    if (this.isReady) {
      const timeout = setTimeout(callback)
      return () => clearTimeout(timeout)
    } else {
      addEventListenerOnce(this, 'ready', callback)
      return () => this.removeEventListener('ready', callback)
    }
  }

  /**
   * Wait for the global Window and Document to be ready. The callback
   * function is called exactly once.
   * @param {function} callback
   * @return {function}
   */
  onLoad (callback) {
    if (this.isGlobalReady) {
      const timeout = setTimeout(callback)
      return () => clearTimeout(timeout)
    } else {
      addEventListenerOnce(this, 'load', callback)
      return () => this.removeEventListener('load', callback)
    }
  }

  /**
   * Wait for the runtime to be ready. The callback
   * function is called exactly once.
   * @param {function} callback
   * @return {function}
   */
  onInit (callback) {
    if (this.isRuntimeReady) {
      const timeout = setTimeout(callback)
      return () => clearTimeout(timeout)
    } else {
      addEventListenerOnce(this, 'init', callback)
      return () => this.removeEventListener('init', callback)
    }
  }

  /**
   * Calls callback when a global exception occurs.
   * 'error', 'messageerror', and 'unhandledrejection' events are handled here.
   * @param {function} callback
   * @return {function}
   */
  onError (callback) {
    this.addEventListener('error', callback)
    this.addEventListener('messageerror', callback)
    this.addEventListener('unhandledrejection', callback)
    return () => {
      this.removeEventListener('error', callback)
      this.removeEventListener('messageerror', callback)
      this.removeEventListener('unhandledrejection', callback)
    }
  }

  /**
   * Subscribes to the global data pipe calling callback when
   * new data is emitted on the global Window.
   * @param {function} callback
   * @return {function}
   */
  onData (callback) {
    this.addEventListener('data', callback)
    return () => this.removeEventListener('data', callback)
  }

  /**
   * Subscribes to global messages likely from an external `postMessage`
   * invocation.
   * @param {function} callback
   * @return {function}
   */
  onMessage (callback) {
    this.addEventListener('message', callback)
    return () => this.removeEventListener('message', callback)
  }

  /**
   * Calls callback when runtime is working online.
   * @param {function} callback
   * @return {function}
   */
  onOnline (callback) {
    this.addEventListener('online', callback)
    return () => this.removeEventListener('online', callback)
  }

  /**
   * Calls callback when runtime is not working online.
   * @param {function} callback
   * @return {function}
   */
  onOffline (callback) {
    this.addEventListener('offline', callback)
    return () => this.removeEventListener('offline', callback)
  }

  /**
   * Calls callback when runtime user preferred language has changed.
   * @param {function} callback
   * @return {function}
   */
  onLanguageChange (callback) {
    this.addEventListener('languagechange', callback)
    return () => this.removeEventListener('languagechange', callback)
  }

  /**
   * Calls callback when an application permission has changed.
   * @param {function} callback
   * @return {function}
   */
  onPermissionChange (callback) {
    this.addEventListener('permissionchange', callback)
    return () => this.removeEventListener('permissionchange', callback)
  }

  /**
   * Calls callback in response to a displayed `Notification`.
   * @param {function} callback
   * @return {function}
   */
  onNotificationResponse (callback) {
    this.addEventListener('notificationresponse', callback)
    return () => this.removeEventListener('notificationresponse', callback)
  }

  /**
   * Calls callback when a `Notification` is presented.
   * @param {function} callback
   * @return {function}
   */
  onNotificationPresented (callback) {
    this.addEventListener('notificationpresented', callback)
    return () => this.removeEventListener('notificationpresented', callback)
  }

  /**
   * Calls callback when a `ApplicationURL` is opened.
   * @param {function} callback
   * @return {function}
   */
  onApplicationURL (callback) {
    this.addEventListener('applicationurl', callback)
    return () => this.removeEventListener('applicationurl', callback)
  }

  /**
   * Calls callback when an `ApplicationPause` is dispatched.
   * @param {function} callback
   * @return {function}
   */
  onApplicationPause (callback) {
    this.addEventListener('applicationpause', callback)
    return () => this.removeEventListener('applicationpause', callback)
  }

  /**
   * Calls callback when an `ApplicationResume` is dispatched.
   * @param {function} callback
   * @return {function}
   */
  onApplicationResume (callback) {
    this.addEventListener('applicationresume', callback)
    return () => this.removeEventListener('applicationresume', callback)
  }
}

/**
 * `Hooks` single instance.
 * @ignore
 */
const hooks = new Hooks()

/**
 * Wait for a hook event to occur.
 * @template {Event | T extends Event}
 * @param {string|function} nameOrFunction
 * @return {Promise<T>}
 */
export async function wait (nameOrFunction) {
  return await hooks.wait(nameOrFunction)
}

/**
 * Wait for the global Window, Document, and Runtime to be ready.
 * The callback function is called exactly once.
 * @param {function} callback
 * @return {function}
 */
export function onReady (callback) {
  return hooks.onReady(callback)
}

/**
 * Wait for the global Window and Document to be ready. The callback
 * function is called exactly once.
 * @param {function} callback
 * @return {function}
 */
export function onLoad (callback) {
  return hooks.onLoad(callback)
}

/**
 * Wait for the runtime to be ready. The callback
 * function is called exactly once.
 * @param {function} callback
 * @return {function}
 */
export function onInit (callback) {
  return hooks.onInit(callback)
}

/**
 * Calls callback when a global exception occurs.
 * 'error', 'messageerror', and 'unhandledrejection' events are handled here.
 * @param {function} callback
 * @return {function}
 */
export function onError (callback) {
  return hooks.onError(callback)
}

/**
 * Subscribes to the global data pipe calling callback when
 * new data is emitted on the global Window.
 * @param {function} callback
 * @return {function}
 */
export function onData (callback) {
  return hooks.onData(callback)
}

/**
 * Subscribes to global messages likely from an external `postMessage`
 * invocation.
 * @param {function} callback
 * @return {function}
 */
export function onMessage (callback) {
  return hooks.onMessage(callback)
}

/**
 * Calls callback when runtime is working online.
 * @param {function} callback
 * @return {function}
 */
export function onOnline (callback) {
  return hooks.onOnline(callback)
}

/**
 * Calls callback when runtime is not working online.
 * @param {function} callback
 * @return {function}
 */
export function onOffline (callback) {
  return hooks.onOffline(callback)
}

/**
 * Calls callback when runtime user preferred language has changed.
 * @param {function} callback
 * @return {function}
 */
export function onLanguageChange (callback) {
  return hooks.onLanguageChange(callback)
}

/**
 * Calls callback when an application permission has changed.
 * @param {function} callback
 * @return {function}
 */
export function onPermissionChange (callback) {
  return hooks.onPermissionChange(callback)
}

/**
 * Calls callback in response to a presented `Notification`.
 * @param {function} callback
 * @return {function}
 */
export function onNotificationResponse (callback) {
  return hooks.onNotificationResponse(callback)
}

/**
 * Calls callback when a `Notification` is presented.
 * @param {function} callback
 * @return {function}
 */
export function onNotificationPresented (callback) {
  return hooks.onNotificationPresented(callback)
}

/**
 * Calls callback when a `ApplicationURL` is opened.
 * @param {function(ApplicationURLEvent)} callback
 * @return {function}
 */
export function onApplicationURL (callback) {
  return hooks.onApplicationURL(callback)
}

/**
 * Calls callback when a `ApplicationPause` is dispatched.
 * @param {function} callback
 * @return {function}
 */
export function onApplicationPause (callback) {
  return hooks.onApplicationPause(callback)
}

/**
 * Calls callback when a `ApplicationResume` is dispatched.
 * @param {function} callback
 * @return {function}
 */
export function onApplicationResume (callback) {
  return hooks.onApplicationResume(callback)
}

export default hooks
