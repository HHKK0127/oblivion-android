package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.os.Debug
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.util.ArrayDeque
import java.util.Deque

/**
 * In-process memory profiler that samples Runtime and Debug memory metrics.
 * Tracks history for a textual sparkline and exposes a force-GC button.
 */
class DebugMemoryProfiler(private val context: Context) {

    companion object {
        private const val TAG = "DebugMemoryProfiler"
        private const val MAX_SAMPLES = 120
        private const val SAMPLE_INTERVAL_MS = 1000L
    }

    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile private var isSampling = false
    @Volatile private var isAttached = false

    private var sampleRunnable: Runnable? = null
    private val samples: Deque<Sample> = ArrayDeque(MAX_SAMPLES)
    private val lock = Any()

    private lateinit var rootView: LinearLayout
    private lateinit var summaryText: TextView
    private lateinit var historyText: TextView

    private data class Sample(
        val totalUsed: Long,
        val totalMax: Long,
        val nativeAlloc: Long,
        val javaHeap: Long
    )

    fun build(): View {
        rootView = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1A.toInt())
            setPadding(8, 8, 8, 8)
        }

        val title = TextView(context).apply {
            text = "Memory Profiler"
            setTextColor(0xFFFF8844.toInt())
            textSize = 14f
            setTypeface(typeface, Typeface.BOLD)
        }
        rootView.addView(title)

        summaryText = TextView(context).apply {
            setTextColor(Color.WHITE)
            typeface = Typeface.MONOSPACE
            textSize = 11f
            setPadding(0, 8, 0, 8)
        }
        rootView.addView(summaryText)

        val buttonRow = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }

        val gcButton = Button(context).apply {
            text = "Force GC"
            setOnClickListener {
                try {
                    System.gc()
                    System.runFinalization()
                    System.gc()
                } catch (e: Exception) {
                    Log.w(TAG, "GC trigger failed: ${e.message}")
                }
                refreshSummary()
            }
        }
        buttonRow.addView(gcButton)

        val dumpButton = Button(context).apply {
            text = "Dump HPROF"
            setOnClickListener {
                try {
                    val info = Debug.MemoryInfo()
                    Debug.getMemoryInfo(info)
                    val text = "PssTotal=${info.totalPss}KB Dalvik=${info.dalvikPss}KB Native=${info.nativePss}KB"
                    appendEvent(text)
                } catch (e: Exception) {
                    Log.w(TAG, "MemoryInfo failed: ${e.message}")
                }
            }
        }
        buttonRow.addView(dumpButton)

        rootView.addView(buttonRow)

        val scroll = ScrollView(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        historyText = TextView(context).apply {
            setBackgroundColor(0xFF000000.toInt())
            setTextColor(0xFFCCCCCC.toInt())
            typeface = Typeface.MONOSPACE
            textSize = 9f
            setPadding(8, 8, 8, 8)
        }
        scroll.addView(historyText)
        rootView.addView(scroll)

        return rootView
    }

    fun start() {
        if (isSampling) return
        isSampling = true
        isAttached = true
        scheduleNext()
        refreshSummary()
    }

    fun stop() {
        isSampling = false
        val r = sampleRunnable
        sampleRunnable = null
        r?.let { mainHandler.removeCallbacks(it) }
    }

    fun cleanup() {
        stop()
        isAttached = false
    }

    private fun scheduleNext() {
        val r = Runnable {
            if (!isSampling) return@Runnable
            takeSample()
            scheduleNext()
        }
        sampleRunnable = r
        mainHandler.postDelayed(r, SAMPLE_INTERVAL_MS)
    }

    private fun takeSample() {
        val rt = Runtime.getRuntime()
        val totalUsed = rt.totalMemory() - rt.freeMemory()
        val totalMax = rt.maxMemory()
        val nativeAlloc = Debug.getNativeHeapAllocatedSize()
        val javaHeap = rt.totalMemory()
        val sample = Sample(totalUsed, totalMax, nativeAlloc, javaHeap)
        synchronized(lock) {
            if (samples.size >= MAX_SAMPLES) samples.pollFirst()
            samples.addLast(sample)
        }
        refreshHistory()
    }

    private fun appendEvent(text: String) {
        if (!isAttached || !::historyText.isInitialized) return
        val current = historyText.text?.toString().orEmpty()
        historyText.text = "$current\n[$text]"
    }

    private fun refreshSummary() {
        if (!isAttached || !::summaryText.isInitialized) return
        val rt = Runtime.getRuntime()
        val used = rt.totalMemory() - rt.freeMemory()
        val total = rt.totalMemory()
        val max = rt.maxMemory()
        summaryText.text = buildString {
            append("Java used:  ${formatMb(used)}\n")
            append("Java total: ${formatMb(total)}\n")
            append("Java max:   ${formatMb(max)}\n")
            append("Native:     ${formatMb(Debug.getNativeHeapAllocatedSize())}\n")
            append("Java heap:  ${formatMb(Runtime.getRuntime().totalMemory())}")
        }
    }

    private fun refreshHistory() {
        if (!isAttached || !::historyText.isInitialized) return
        val snapshot = synchronized(lock) { samples.toList() }
        if (snapshot.isEmpty()) {
            historyText.text = "(no samples yet)"
            return
        }
        val maxUsed = snapshot.maxOf { it.totalUsed }.coerceAtLeast(1L)
        val out = StringBuilder()
        out.append("used/max sparkline (each char = ${formatMb(maxUsed / 32).coerceAtLeast("1B")})\n")
        for (sample in snapshot) {
            val used = sample.totalUsed
            val native = sample.nativeAlloc
            out.append(barFor(used, maxUsed))
            out.append("  java=").append(formatMb(used))
            out.append("  native=").append(formatMb(native))
            out.append('\n')
        }
        historyText.text = out.toString()
    }

    private fun barFor(value: Long, max: Long): String {
        val width = 32
        val scaled = ((value.toDouble() / max.toDouble()) * width).toInt().coerceIn(0, width)
        val sb = StringBuilder(width)
        for (i in 0 until width) {
            sb.append(if (i < scaled) '#' else '.')
        }
        return sb.toString()
    }

    private fun formatMb(bytes: Long): String {
        if (bytes < 1024) return "${bytes}B"
        val kb = bytes / 1024.0
        if (kb < 1024.0) return String.format("%.1fKB", kb)
        val mb = kb / 1024.0
        return String.format("%.2fMB", mb)
    }
}
