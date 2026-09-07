package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.util.ArrayDeque
import java.util.Deque

/**
 * Lightweight HTTP interceptor view that records requests issued through
 * [recordRequest] and supports synthetic latency probes.
 */
class DebugNetworkInterceptor(private val context: Context) {

    companion object {
        private const val TAG = "DebugNetworkInterceptor"
        private const val MAX_RECORDS = 200
    }

    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile private var isAttached = false
    private val records: Deque<RequestRecord> = ArrayDeque(MAX_RECORDS)
    private val lock = Any()

    private lateinit var contentText: TextView
    private lateinit var scrollView: ScrollView

    private data class RequestRecord(
        val timestamp: Long,
        val url: String,
        val method: String,
        val status: Int,
        val durationMs: Long,
        val error: String?
    )

    fun build(): View {
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1A.toInt())
            setPadding(8, 8, 8, 8)
        }

        val title = TextView(context).apply {
            text = "Network Interceptor"
            setTextColor(0xFFFF8844.toInt())
            textSize = 14f
            setTypeface(typeface, Typeface.BOLD)
        }
        root.addView(title)

        val buttonRow = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }

        val pingButton = Button(context).apply {
            text = "Ping localhost"
            setOnClickListener { issueProbe("http://127.0.0.1/") }
        }
        buttonRow.addView(pingButton)

        val clearButton = Button(context).apply {
            text = "Clear"
            setOnClickListener {
                synchronized(lock) { records.clear() }
                refresh()
            }
        }
        buttonRow.addView(clearButton)

        root.addView(buttonRow)

        scrollView = ScrollView(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        contentText = TextView(context).apply {
            setBackgroundColor(0xFF000000.toInt())
            setTextColor(0xFFCCCCCC.toInt())
            typeface = Typeface.MONOSPACE
            textSize = 9f
            setPadding(8, 8, 8, 8)
        }
        scrollView.addView(contentText)
        root.addView(scrollView)

        return root
    }

    fun start() {
        isAttached = true
        refresh()
    }

    fun stop() {
        isAttached = false
    }

    fun cleanup() {
        stop()
    }

    /**
     * Records an externally-issued HTTP request. Safe to call from any thread.
     */
    fun recordRequest(url: String, method: String, status: Int, durationMs: Long, error: String? = null) {
        val record = RequestRecord(
            timestamp = System.currentTimeMillis(),
            url = url,
            method = method,
            status = status,
            durationMs = durationMs,
            error = error
        )
        synchronized(lock) {
            if (records.size >= MAX_RECORDS) records.pollFirst()
            records.addLast(record)
        }
        postRefresh()
    }

    private fun issueProbe(url: String) {
        Thread({
            val started = System.currentTimeMillis()
            try {
                val conn = (URL(url).openConnection() as? HttpURLConnection)
                if (conn != null) {
                    conn.requestMethod = "GET"
                    conn.connectTimeout = 2000
                    conn.readTimeout = 2000
                    val code = conn.responseCode
                    conn.disconnect()
                    val elapsed = System.currentTimeMillis() - started
                    recordRequest(url, "GET", code, elapsed, null)
                } else {
                    val elapsed = System.currentTimeMillis() - started
                    recordRequest(url, "GET", -1, elapsed, "not http")
                }
            } catch (e: Exception) {
                val elapsed = System.currentTimeMillis() - started
                recordRequest(url, "GET", -1, elapsed, e.javaClass.simpleName + ": " + e.message)
            }
        }, "DebugNetworkInterceptorProbe").apply { isDaemon = true }.start()
    }

    private fun postRefresh() {
        if (!isAttached) return
        mainHandler.post { refresh() }
    }

    private fun refresh() {
        if (!isAttached || !::contentText.isInitialized) return
        val snapshot = synchronized(lock) { records.toList() }
        if (snapshot.isEmpty()) {
            contentText.text = "(no requests yet)"
            return
        }
        val sb = StringBuilder()
        for (r in snapshot) {
            val marker = if (r.status in 200..299) "OK" else "X "
            sb.append(marker).append(' ')
            sb.append(r.method).append(' ')
            sb.append(r.status).append(' ')
            sb.append(r.durationMs).append("ms ")
            sb.append(r.url).append('\n')
            if (r.error != null) {
                sb.append("   ").append(r.error).append('\n')
            }
        }
        contentText.text = sb.toString()
        scrollView.post {
            if (isAttached) {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }
}
