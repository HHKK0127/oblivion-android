package com.example.oblivion.debug

import android.app.ActivityManager
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.os.Build
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.util.DisplayMetrics
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.util.Locale

/**
 * Static-and-live system info panel.
 * Shows device, runtime, graphics, and storage details.
 */
class DebugSystemInfo(private val context: Context) {

    companion object {
        private const val TAG = "DebugSystemInfo"
        private const val REFRESH_INTERVAL_MS = 5000L
    }

    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile private var isTicking = false
    @Volatile private var isAttached = false

    private var tickRunnable: Runnable? = null
    private lateinit var contentText: TextView

    fun build(): View {
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF1A1A1A.toInt())
            setPadding(8, 8, 8, 8)
        }

        val title = TextView(context).apply {
            text = "System Info"
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

        contentText = TextView(context).apply {
            setBackgroundColor(0xFF000000.toInt())
            setTextColor(Color.WHITE)
            typeface = Typeface.MONOSPACE
            textSize = 10f
            setPadding(8, 8, 8, 8)
        }
        scroll.addView(contentText)
        root.addView(scroll)

        return root
    }

    fun start() {
        if (isTicking) return
        isTicking = true
        isAttached = true
        refresh()
        scheduleNext()
    }

    fun stop() {
        isTicking = false
        val r = tickRunnable
        tickRunnable = null
        r?.let { mainHandler.removeCallbacks(it) }
    }

    fun cleanup() {
        stop()
        isAttached = false
    }

    private fun scheduleNext() {
        val r = Runnable {
            if (!isTicking) return@Runnable
            refresh()
            scheduleNext()
        }
        tickRunnable = r
        mainHandler.postDelayed(r, REFRESH_INTERVAL_MS)
    }

    private fun refresh() {
        if (!isAttached || !::contentText.isInitialized) return
        val rt = Runtime.getRuntime()
        contentText.text = buildString {
            append("== Device ==\n")
            append("Manufacturer: ${Build.MANUFACTURER}\n")
            append("Model:        ${Build.MODEL}\n")
            append("Brand:        ${Build.BRAND}\n")
            append("Product:      ${Build.PRODUCT}\n")
            append("Hardware:     ${Build.HARDWARE}\n")
            append("Board:        ${Build.BOARD}\n")
            append("SoC:          ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}\n\n")

            append("== OS ==\n")
            append("Release:      ${Build.VERSION.RELEASE}\n")
            append("SDK:          ${Build.VERSION.SDK_INT}\n")
            append("Security:     ${Build.VERSION.SECURITY_PATCH}\n")
            append("Build ID:     ${Build.ID}\n")
            append("Build Fp:     ${Build.FINGERPRINT}\n")
            append("Locale:       ${Locale.getDefault()}\n\n")

            append("== Display ==\n")
            appendDisplay(this)

            append("\n== Runtime ==\n")
            append("Cores:        ${rt.availableProcessors()}\n")
            append("Java max:     ${rt.maxMemory() / (1024 * 1024)}MB\n")
            append("Java total:   ${rt.totalMemory() / (1024 * 1024)}MB\n")
            append("Java free:    ${rt.freeMemory() / (1024 * 1024)}MB\n")

            append("\n== Storage ==\n")
            appendStorage(this)

            append("\n== ActivityManager ==\n")
            appendActivityManager(this)
        }
    }

    private fun appendDisplay(out: StringBuilder) {
        try {
            val dm: DisplayMetrics = context.resources.displayMetrics
            out.append("Resolution:   ${dm.widthPixels}x${dm.heightPixels}\n")
            out.append("Density:      ${dm.density} (${dm.densityDpi}dpi)\n")
            out.append("Refresh:      ${context.display?.refreshRate ?: "?"}Hz\n")
        } catch (e: Exception) {
            Log.w(TAG, "display metrics failed: ${e.message}")
        }
    }

    private fun appendStorage(out: StringBuilder) {
        try {
            val ext = Environment.getDataDirectory()
            out.append("Data:         ${formatBytes(ext.totalSpace)} / ${formatBytes(ext.usableSpace)}\n")
            val stat = Environment.getExternalStorageState()
            out.append("Ext state:    $stat\n")
        } catch (e: Exception) {
            Log.w(TAG, "storage query failed: ${e.message}")
        }
        try {
            val cache = context.cacheDir
            val totalSize = folderSize(cache)
            out.append("Cache files:  $totalSize bytes\n")
        } catch (e: Exception) {
            Log.w(TAG, "cache size failed: ${e.message}")
        }
    }

    private fun appendActivityManager(out: StringBuilder) {
        try {
            val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            val info = ActivityManager.MemoryInfo()
            am?.getMemoryInfo(info)
            if (info != null) {
                out.append("System avail: ${formatBytes(info.availMem)}\n")
                out.append("System total: ${formatBytes(info.totalMem)}\n")
                out.append("Low memory:   ${info.lowMemory}\n")
                out.append("Threshold:    ${formatBytes(info.threshold)}\n")
            }
        } catch (e: Exception) {
            Log.w(TAG, "am query failed: ${e.message}")
        }
    }

    private fun folderSize(dir: File): Long {
        var total = 0L
        val children = dir.listFiles() ?: return 0L
        for (child in children) {
            total += if (child.isDirectory) folderSize(child) else child.length()
        }
        return total
    }

    private fun formatBytes(bytes: Long): String {
        if (bytes < 1024) return "${bytes}B"
        val kb = bytes / 1024.0
        if (kb < 1024.0) return String.format("%.1fKB", kb)
        val mb = kb / 1024.0
        if (mb < 1024.0) return String.format("%.1fMB", mb)
        val gb = mb / 1024.0
        return String.format("%.2fGB", gb)
    }
}
