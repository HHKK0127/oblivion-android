package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.BufferedReader
import java.io.InputStreamReader
import java.util.ArrayDeque
import java.util.Deque

/**
 * Real-time logcat reader with level-based filtering and auto-scroll.
 * Uses Runtime.exec to spawn logcat and consumes its stdout on a daemon thread.
 */
class DebugLogcatReader(private val context: Context) {

    companion object {
        private const val TAG = "DebugLogcatReader"
        private const val MAX_LINES = 2000
        private const val POLL_INTERVAL_MS = 250L
    }

    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile private var isReading = false
    @Volatile private var isAttached = false
    @Volatile private var minLevel: Char = 'V'

    private var logcatProcess: Process? = null
    private var readerThread: Thread? = null
    private var pollRunnable: Runnable? = null

    private val lines: Deque<String> = ArrayDeque(MAX_LINES)
    private val lock = Any()

    private lateinit var logTextView: TextView
    private lateinit var scrollView: ScrollView
    private lateinit var levelButton: Button
    private lateinit var clearButton: Button
    private lateinit var rootView: LinearLayout

    /**
     * Build the logcat reader UI.
     */
    fun build(): View {
        rootView = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1A.toInt())
            setPadding(8, 8, 8, 8)
        }

        val title = TextView(context).apply {
            text = "Logcat Reader"
            setTextColor(0xFFFF8844.toInt())
            textSize = 14f
            setTypeface(typeface, Typeface.BOLD)
        }
        rootView.addView(title)

        val buttonRow = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        levelButton = Button(context).apply {
            text = "Level: V"
            setOnClickListener { cycleLevel() }
        }
        buttonRow.addView(levelButton)

        clearButton = Button(context).apply {
            text = "Clear"
            setOnClickListener {
                synchronized(lock) { lines.clear() }
                refreshText()
            }
        }
        buttonRow.addView(clearButton)
        rootView.addView(buttonRow)

        scrollView = ScrollView(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        logTextView = TextView(context).apply {
            setBackgroundColor(0xFF000000.toInt())
            setTextColor(Color.WHITE)
            typeface = Typeface.MONOSPACE
            textSize = 10f
            setPadding(8, 8, 8, 8)
        }
        scrollView.addView(logTextView)
        rootView.addView(scrollView)

        return rootView
    }

    /**
     * Start the logcat reader; safe to call multiple times.
     */
    fun start() {
        if (isReading) return
        isReading = true
        isAttached = true
        spawnLogcat()
        schedulePoll()
    }

    /**
     * Stop the logcat reader but keep the UI intact.
     */
    fun stop() {
        isReading = false
        val runnable = pollRunnable
        pollRunnable = null
        runnable?.let { mainHandler.removeCallbacks(it) }

        val proc = logcatProcess
        logcatProcess = null
        try {
            proc?.destroy()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to destroy logcat process: ${e.message}")
        }

        val thread = readerThread
        readerThread = null
        thread?.interrupt()
    }

    /**
     * Stop and detach; called from onDetachedFromWindow.
     */
    fun cleanup() {
        stop()
        isAttached = false
    }

    private fun cycleLevel() {
        minLevel = when (minLevel) {
            'V' -> 'D'
            'D' -> 'I'
            'I' -> 'W'
            'W' -> 'E'
            else -> 'V'
        }
        levelButton.text = "Level: $minLevel"
    }

    private fun spawnLogcat() {
        val thread = Thread({
            try {
                val proc = Runtime.getRuntime().exec(arrayOf("logcat", "-v", "threadtime"))
                logcatProcess = proc
                val reader = BufferedReader(InputStreamReader(proc.inputStream))
                var line = reader.readLine()
                while (line != null && isReading) {
                    appendLine(line)
                    line = reader.readLine()
                }
                try { reader.close() } catch (_: Exception) {}
            } catch (e: Exception) {
                Log.e(TAG, "logcat read failed: ${e.message}")
            }
        }, "DebugLogcatReader")
        readerThread = thread
        thread.isDaemon = true
        thread.start()
    }

    private fun appendLine(raw: String) {
        synchronized(lock) {
            if (lines.size >= MAX_LINES) {
                lines.pollFirst()
            }
            lines.addLast(raw)
        }
    }

    private fun schedulePoll() {
        val runnable = Runnable { poll() }
        pollRunnable = runnable
        mainHandler.postDelayed(runnable, POLL_INTERVAL_MS)
    }

    private fun poll() {
        if (!isReading || !isAttached) return
        refreshText()
        schedulePoll()
    }

    private fun refreshText() {
        if (!isAttached || !::logTextView.isInitialized) return
        val snapshot = synchronized(lock) { lines.toList() }
        val builder = SpannableStringBuilder()
        for (raw in snapshot) {
            val color = colorFor(raw)
            if (color == Color.WHITE) {
                builder.append(raw).append('\n')
            } else {
                val start = builder.length
                builder.append(raw).append('\n')
                val end = builder.length
                builder.setSpan(
                    ForegroundColorSpan(color),
                    start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                )
            }
        }
        // Apply filter by stripping lines below the minimum level.
        val filtered = applyFilter(builder)
        logTextView.text = filtered
        scrollView.post {
            if (isAttached) {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    private fun applyFilter(builder: SpannableStringBuilder): CharSequence {
        if (minLevel == 'V') return builder
        val threshold = when (minLevel) {
            'D' -> 'D'
            'I' -> 'I'
            'W' -> 'W'
            'E' -> 'E'
            else -> 'V'
        }
        val text = builder.toString()
        val out = SpannableStringBuilder()
        var idx = 0
        while (idx < text.length) {
            val nl = text.indexOf('\n', idx)
            val end = if (nl < 0) text.length else nl + 1
            val line = text.substring(idx, end)
            val level = extractLevel(line)
            if (level != null && level >= threshold) {
                // Copy span info too
                val spanStart = out.length
                out.append(line)
                val spanEnd = out.length
                val spans = builder.getSpans(idx, end - 1, ForegroundColorSpan::class.java)
                for (sp in spans) {
                    val color = sp.foregroundColor
                    if (color != Color.WHITE) {
                        out.setSpan(
                            ForegroundColorSpan(color),
                            spanStart, spanEnd, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
                        )
                    }
                }
            }
            idx = end
        }
        return out
    }

    private fun extractLevel(line: String): Char? {
        // threadtime format: "MM-DD HH:MM:SS.mmm  PID  TID LEVEL TAG: message"
        val parts = line.split(' ')
        if (parts.size < 4) return null
        return parts[3].firstOrNull()
    }

    private fun colorFor(line: String): Int {
        val level = extractLevel(line) ?: return Color.WHITE
        return when (level) {
            'V' -> 0xFF888888.toInt()
            'D' -> Color.WHITE
            'I' -> 0xFF00FF00.toInt()
            'W' -> 0xFFFFFF00.toInt()
            'E' -> 0xFFFF4444.toInt()
            else -> Color.WHITE
        }
    }
}
