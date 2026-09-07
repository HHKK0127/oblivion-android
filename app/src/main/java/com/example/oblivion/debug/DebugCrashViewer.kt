package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * UI for displaying crash history collected by DebugCrashHandler.
 */
class DebugCrashViewer(private val context: Context) {

    companion object {
        private const val TAG = "DebugCrashViewer"
        private val dateFmt = SimpleDateFormat("MM-dd HH:mm:ss", Locale.US)
    }

    private var isAttached = false
    private lateinit var listView: TextView

    fun build(): View {
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1A.toInt())
            setPadding(8, 8, 8, 8)
        }

        val title = TextView(context).apply {
            text = "Crash History"
            setTextColor(0xFFFF8844.toInt())
            textSize = 14f
            setTypeface(typeface, Typeface.BOLD)
        }
        root.addView(title)

        val scroll = ScrollView(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }

        listView = TextView(context).apply {
            setBackgroundColor(0xFF000000.toInt())
            setTextColor(0xFFFF4444.toInt())
            typeface = Typeface.MONOSPACE
            textSize = 10f
            setPadding(8, 8, 8, 8)
        }
        scroll.addView(listView)
        root.addView(scroll)

        isAttached = true
        refresh()
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
        isAttached = false
    }

    private fun refresh() {
        if (!isAttached || !::listView.isInitialized) return
        try {
            val entries = DebugCrashHandler.getCrashHistory()
            if (entries.isEmpty()) {
                listView.text = "(No crashes recorded)"
                return
            }
            val sb = StringBuilder()
            for ((i, entry) in entries.withIndex()) {
                val ts = dateFmt.format(Date(entry.timestamp))
                sb.append("#${i + 1} [$ts] ${entry.exceptionType}\n")
                sb.append("Thread: ${entry.threadName}\n")
                sb.append("Device: ${entry.deviceInfo}\n")
                if (entry.message.isNotEmpty()) {
                    sb.append("Msg: ${entry.message}\n")
                }
                val lines = entry.stackTrace.split('\n').take(8)
                for (line in lines) {
                    sb.append("  ").append(line).append('\n')
                }
                sb.append('\n')
            }
            listView.text = sb.toString()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load crash history: ${e.message}")
            listView.text = "Error loading crash history"
        }
    }
}
