package com.example.oblivion.debug

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.BatteryManager
import android.os.Build
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.StatFs
import android.provider.Settings
import android.util.TypedValue
import android.view.Display
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Debug viewer that displays mobile device status information.
 * Battery, network, memory, storage, display details.
 */
class DebugMobileStatus(
    private val context: Context
) {
    private var container: LinearLayout? = null
    private val statusItems = mutableMapOf<String, TextView>()
    private val handler = Handler(Looper.getMainLooper())
    private var refreshRunnable: Runnable? = null
    private var isUpdating = false

    fun build(): LinearLayout {
        container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#FF1A1A1A"))
        }

        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)

        // Section: Battery
        val batterySection = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp8, dp4, dp8, dp4)
        }
        val batteryTitle = TextView(context).apply {
            text = "=== Battery ==="
            setTextColor(Color.parseColor("#4DFF4D"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            setTypeface(null, android.graphics.Typeface.BOLD)
        }
        batterySection.addView(batteryTitle)

        // Battery: XX% / Charging: STATUS
        val batteryStatus = createStatusItem("batteryStatus")
        batterySection.addView(batteryStatus)

        // Voltage: XXXV / Temperature: XXC
        val batteryDetails = createStatusItem("batteryDetails")
        batterySection.addView(batteryDetails)

        container?.addView(batterySection)

        // Section: Network
        val networkSection = createSection("=== Network ===")
        val networkType = createStatusItem("networkType")
        val networkSpeed = createStatusItem("networkSpeed")
        val networkDetails = createStatusItem("networkDetails")
        networkSection.addView(networkType)
        networkSection.addView(networkSpeed)
        networkSection.addView(networkDetails)
        container?.addView(networkSection)

        // Section: Memory
        val memorySection = createSection("=== Memory ===")
        val memoryUsed = createStatusItem("memoryUsed")
        val memoryDetails = createStatusItem("memoryDetails")
        memorySection.addView(memoryUsed)
        memorySection.addView(memoryDetails)
        container?.addView(memorySection)

        // Section: Storage
        val storageSection = createSection("=== Storage ===")
        val storageUsed = createStatusItem("storageUsed")
        val storageDetails = createStatusItem("storageDetails")
        storageSection.addView(storageUsed)
        storageSection.addView(storageDetails)
        container?.addView(storageSection)

        // Section: Display
        val displaySection = createSection("=== Display ===")
        val displayResolution = createStatusItem("displayResolution")
        val displayDensity = createStatusItem("displayDensity")
        val displayRefresh = createStatusItem("displayRefresh")
        displaySection.addView(displayResolution)
        displaySection.addView(displayDensity)
        displaySection.addView(displayRefresh)
        container?.addView(displaySection)

        // Section: System
        val systemSection = createSection("=== System ===")
        val systemModel = createStatusItem("systemModel")
        val systemAndroid = createStatusItem("systemAndroid")
        val systemApi = createStatusItem("systemApi")
        systemSection.addView(systemModel)
        systemSection.addView(systemAndroid)
        systemSection.addView(systemApi)
        container?.addView(systemSection)

        updateAll()
        return container!!
    }

    private fun createSection(title: String): LinearLayout {
        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val section = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp8, dp4, dp8, dp4)
        }
        val titleView = TextView(context).apply {
            text = title
            setTextColor(Color.parseColor("#4DFF4D"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            setTypeface(null, android.graphics.Typeface.BOLD)
        }
        section.addView(titleView)
        return section
    }

    private fun createStatusItem(key: String): TextView {
        val dp2 = DebugButtonWidget.dpToPx(context, 2)
        val tv = TextView(context).apply {
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(dp2, dp2, dp2, dp2)
        }
        statusItems[key] = tv
        return tv
    }

    private fun updateAll() {
        try {
            updateBattery()
            updateNetwork()
            updateMemory()
            updateStorage()
            updateDisplay()
            updateSystem()
        } catch (_: Exception) {
            // Silently handle permission errors
        }
    }

    private fun updateBattery() {
        val filter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        val batteryStatus = context.registerReceiver(null, filter)

        val level = batteryStatus?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = batteryStatus?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        val pct = if (level >= 0 && scale > 0) (level * 100 / scale) else -1

        val status = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val charging = when (status) {
            BatteryManager.BATTERY_STATUS_CHARGING -> "CHARGING"
            BatteryManager.BATTERY_STATUS_FULL -> "FULL"
            else -> "NOT CHARGING"
        }

        statusItems["batteryStatus"]?.text = "Battery: $pct% / Charging: $charging"

        val voltage = (batteryStatus?.getIntExtra(BatteryManager.EXTRA_VOLTAGE, 0) ?: 0) / 1000.0
        val temp = (batteryStatus?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0) / 10.0
        statusItems["batteryDetails"]?.text = String.format("Voltage: %.2fV / Temperature: %.1fC", voltage, temp)
    }

    @Suppress("DEPRECATION")
    private fun updateNetwork() {
        val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        val network = cm?.activeNetwork
        val caps = network?.let { cm.getNetworkCapabilities(it) }

        val type = when {
            caps == null -> "DISCONNECTED"
            caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "WiFi"
            caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "Mobile"
            caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> "Ethernet"
            else -> "Other"
        }

        statusItems["networkType"]?.text = "Type: $type"

        if (caps != null) {
            val downMbps = caps.linkDownstreamBandwidthKbps / 1000
            val upMbps = caps.linkUpstreamBandwidthKbps / 1000
            statusItems["networkSpeed"]?.text = "Speed: ${downMbps}Mbps down / ${upMbps}Mbps up"
        } else {
            statusItems["networkSpeed"]?.text = "Speed: N/A"
        }

        statusItems["networkDetails"]?.text = "Metered: ${cm?.isActiveNetworkMetered ?: "N/A"}"
    }

    private fun updateMemory() {
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        val memInfo = ActivityManager.MemoryInfo()
        am?.getMemoryInfo(memInfo)

        val totalMB = memInfo.totalMem / (1024 * 1024)
        val availMB = memInfo.availMem / (1024 * 1024)
        val usedMB = totalMB - availMB
        val pct = if (totalMB > 0) (usedMB * 100 / totalMB) else 0

        statusItems["memoryUsed"]?.text = "Used: ${usedMB}MB / ${totalMB}MB ($pct%)"
        statusItems["memoryDetails"]?.text = "Low Memory: ${memInfo.lowMemory} / Threshold: ${memInfo.threshold / (1024 * 1024)}MB"
    }

    private fun updateStorage() {
        val stat = StatFs(Environment.getExternalStorageDirectory().path)
        val totalGB = stat.totalBytes / (1024.0 * 1024.0 * 1024.0)
        val availGB = stat.availableBytes / (1024.0 * 1024.0 * 1024.0)
        val usedGB = totalGB - availGB
        val pct = if (totalGB > 0) (usedGB * 100 / totalGB).toInt() else 0

        statusItems["storageUsed"]?.text = String.format("Used: %.1fGB / %.1fGB (%d%%)", usedGB, totalGB, pct)
        statusItems["storageDetails"]?.text = "Available: ${String.format("%.1fGB", availGB)}"
    }

    @Suppress("DEPRECATION")
    private fun updateDisplay() {
        val display = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            context.display
        } else {
            (context.getSystemService(Context.WINDOW_SERVICE) as? android.view.WindowManager)?.defaultDisplay
        }

        val metrics = context.resources.displayMetrics
        statusItems["displayResolution"]?.text = "Resolution: ${metrics.widthPixels} x ${metrics.heightPixels}"
        statusItems["displayDensity"]?.text = "Density: ${metrics.densityDpi}dpi (${metrics.density}x)"
        statusItems["displayRefresh"]?.text = "Refresh Rate: ${display?.refreshRate?.toInt() ?: "N/A"}Hz"
    }

    private fun updateSystem() {
        statusItems["systemModel"]?.text = "Model: ${Build.MANUFACTURER} ${Build.MODEL}"
        statusItems["systemAndroid"]?.text = "Android: ${Build.VERSION.RELEASE}"
        statusItems["systemApi"]?.text = "API Level: ${Build.VERSION.SDK_INT}"
    }

    /**
     * Start periodic updates.
     */
    fun startUpdating(intervalMs: Long = 5000) {
        if (isUpdating) return
        isUpdating = true
        refreshRunnable = object : Runnable {
            override fun run() {
                if (!isUpdating) return
                updateAll()
                handler.postDelayed(this, intervalMs)
            }
        }
        handler.postDelayed(refreshRunnable!!, intervalMs)
    }

    /**
     * Stop periodic updates.
     */
    fun stopUpdating() {
        isUpdating = false
        refreshRunnable?.let { handler.removeCallbacks(it) }
        refreshRunnable = null
    }

    /**
     * Manual refresh.
     */
    fun refresh() {
        updateAll()
    }
}
