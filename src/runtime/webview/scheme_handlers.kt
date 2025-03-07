// vim: set sw=2:
package socket.runtime.webview

import java.io.PipedInputStream
import java.io.PipedOutputStream
import java.lang.Thread
import java.util.concurrent.Semaphore

import kotlin.concurrent.thread

import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse

import socket.runtime.app.App
import socket.runtime.app.AppActivity
import socket.runtime.bridge.Bridge
import socket.runtime.debug.console
import socket.runtime.ipc.Message

open class SchemeHandlers (val bridge: Bridge) {
  open class Request (val bridge: Bridge, val request: WebResourceRequest) {
    val response = Response(this)
    val body: ByteArray? by lazy {
      try {
        val seq = this.getHeader("runtime-xhr-seq")
          ?: this.request.url.getQueryParameter("seq")

        if (seq != null && this.bridge.buffers.contains(seq)) {
          val buffer = this.bridge.buffers[seq]
          if (request.method == "POST" || request.method == "PUT" || request.method == "PATCH") {
            this.bridge.buffers.remove(seq)
          }
          buffer
        } else {
          null
        }
      } catch (_: Exception) {
        null
      }
    }

    fun getScheme (): String {
      val url = this.request.url
      val app = App.getInstance()
      val bundleIdentifier = app.getUserConfigValue("meta_bundle_identifier")

      if (
        (url.scheme == "https" || url.scheme == "http") &&
        url.host == bundleIdentifier
      ) {
        return "socket"
      }

      return this.request.url.scheme ?: ""
    }

    fun getMethod (): String {
      return this.request.method ?: ""
    }

    fun getHostname (): String {
      return this.request.url.host ?: ""
    }

    fun getPathname (): String {
      return this.request.url.path ?: ""
    }

    fun getQuery (): String {
      return this.request.url.query ?: ""
    }

    fun getHeaders (): String {
      var headers = ""
      for (entry in request.requestHeaders) {
        headers += "${entry.key}: ${entry.value}\n"
      }
      return headers
    }

    fun getHeader (name: String): String? {
      return request.requestHeaders.get(name)
    }

    fun getUrl (): String {
      return this.request.url.toString().replace("https:", "socket:")
    }

    fun getWebResourceResponse (): WebResourceResponse? {
      return this.response.response
    }

    fun waitForFinishedResponse () {
      this.response.waitForFinish()
    }
  }

  open class Response (val request: Request) {
    val stream = PipedOutputStream()
    var mimeType = "application/octet-stream"
    val response = WebResourceResponse(
      mimeType,
      null,
      PipedInputStream(this.stream)
    )

    val headers = mutableMapOf<String, String>()
    val buffers = mutableListOf<ByteArray>()
    val semaphore = Semaphore(0)

    var pendingWrites = 0
    var finished = false

    fun setStatus (statusCode: Int, statusText: String) {
      val headers = this.headers
      val mimeType = this.mimeType

      this.response.apply {
        setStatusCodeAndReasonPhrase(statusCode, statusText)
        setResponseHeaders(headers)
        setMimeType(mimeType)
      }
    }

    fun setHeader (name: String, value: String) {
      if (name.lowercase() == "content-type") {
        this.mimeType = value
        this.response.setMimeType(value)
      } else if (name.lowercase() != "content-length") {
        this.headers.remove(name)

        if (this.response.responseHeaders != null) {
          this.response.responseHeaders.remove(name)
        }

        this.headers += mapOf(name to value)
        this.response.responseHeaders = this.headers
      }
    }

    fun write (bytes: ByteArray) {
      this.buffers += bytes
    }

    fun write (string: String) {
      this.write(string.toByteArray())
    }

    fun finish () {
      if (!this.finished) {
        this.finished = true
        this.semaphore.release()
      }
    }

    fun waitForFinish () {
      this.semaphore.acquireUninterruptibly()
      this.semaphore.release()
      if (this.finished) {
        val stream = this.stream
        val buffers = this.buffers
        thread {
          try {
            for (bytes in buffers) {
              stream.write(bytes)
            }
          } catch (_: Exception) {}

          try {
            stream.flush()
          } catch (_: Exception) {}

          try {
            stream.close()
          } catch (_: Exception) {}
        }
      }
    }
  }

  fun handleRequest (webResourceRequest: WebResourceRequest): WebResourceResponse? {
    val request = Request(this.bridge, webResourceRequest)

    try {
      if (this.handleRequest(this.bridge.index, request)) {
        request.waitForFinishedResponse()
        return request.getWebResourceResponse()
      }
    } catch (e: Exception) {
      console.debug(e.toString())
    }

    return null
  }

  fun hasHandlerForScheme (scheme: String): Boolean {
    return this.hasHandlerForScheme(this.bridge.index, scheme)
  }

  @Throws(Exception::class)
  external fun handleRequest (index: Int, request: Request): Boolean

  @Throws(Exception::class)
  external fun hasHandlerForScheme (index: Int, scheme: String): Boolean
}
