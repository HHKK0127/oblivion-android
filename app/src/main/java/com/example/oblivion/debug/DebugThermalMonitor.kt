package com.example.oblivion.debug

import android.util.Log
import java.io.File
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.FileInputStream

/**
 * Thermal monitoring by reading from /sys/class/thermal/ sysfs interface.
 * Runs on a background thread with proper interrupt handling.
 * Thread-safe thermal history with synchronized blocks.
 */
class DebugThermalMonitor {

    companion object {
        private const val TAG = "DebugThermalMonitor"
        private const val THERMAL_BASE_PATH = "/sys/class/thermal/"
        private const val POLL_INTERVAL_MS = 2000L
        private const val MAX_HISTORY_SIZE = 60

        // Regex patterns for parsing thermal zone data
        private val THERMAL_ZONE_PATTERN = Regex("thermal_zone(\\d+)")
        private val TEMP_PATTERN = Regex("temp:\\s*(\\d+)")
        private val TYPE_PATTERN = Regex("type:\\s*(.+)")
        private val MODE_PATTERN = Regex("mode:\\s*(.+)")
    }

    /**
     * Data class representing a single thermal zone reading.
     */
    data class ThermalZone(
        val zoneIndex: Int,
        val type: String,
        val temperatureCelsius: Float,
        val mode: String = ""
    )

    /**
     * Thermal snapshot with timestamp.
     */
    data class ThermalSnapshot(
        val timestamp: Long,
        val zones: List<ThermalZone>,
        val maxTemperature: Float,
        val averageTemperature: Float
    )

    // Thread-safe history
    private val thermalHistory = mutableListOf<ThermalSnapshot>()

    // Monitoring thread
    private var monitorThread: Thread? = null
    @Volatile
    private var isMonitoring = false

    // Callback for temperature updates
    private var onUpdate: ((ThermalSnapshot) -> Unit)? = null

    /**
     * Start thermal monitoring on a background thread.
     */
    fun startMonitoring() {
        if (isMonitoring) {
            Log.w(TAG, "Already monitoring thermal data")
            return
        }

        isMonitoring = true
        monitorThread = Thread({
            Log.i(TAG, "Thermal monitoring thread started")
            while (isMonitoring && !Thread.currentThread().isInterrupted) {
                try {
                    val snapshot = readThermalData()
                    if (snapshot != null) {
                        synchronized(thermalHistory) {
                            thermalHistory.add(snapshot)
                            if (thermalHistory.size > MAX_HISTORY_SIZE) {
                                thermalHistory.removeAt(0)
                            }
                        }
                        onUpdate?.invoke(snapshot)
                    }
                    Thread.sleep(POLL_INTERVAL_MS)
                } catch (e: InterruptedException) {
                    Log.d(TAG, "Thermal monitoring thread interrupted")
                    Thread.currentThread().interrupt()
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error reading thermal data: ${e.message}", e)
                    try {
                        Thread.sleep(POLL_INTERVAL_MS)
                    } catch (ie: InterruptedException) {
                        Thread.currentThread().interrupt()
                        break
                    }
                }
            }
            Log.i(TAG, "Thermal monitoring thread ended")
        }, "DebugThermalMonitor").apply {
            isDaemon = true
            start()
        }
    }

    /**
     * Stop thermal monitoring.
     */
    fun stopMonitoring() {
        if (!isMonitoring) return

        isMonitoring = false
        monitorThread?.interrupt()
        monitorThread = null
        Log.i(TAG, "Thermal monitoring stopped")
    }

    /**
     * Read thermal data from /sys/class/thermal/ sysfs.
     */
    private fun readThermalData(): ThermalSnapshot? {
        val thermalDir = File(THERMAL_BASE_PATH)
        if (!thermalDir.exists() || !thermalDir.isDirectory) {
            Log.w(TAG, "Thermal directory not found: $THERMAL_BASE_PATH")
            return null
        }

        val zones = mutableListOf<ThermalZone>()

        try {
            val entries = thermalDir.listFiles() ?: return null

            for (entry in entries) {
                val name = entry.name
                val zoneMatch = THERMAL_ZONE_PATTERN.find(name) ?: continue
                val zoneIndex = zoneMatch.groupValues[1].toIntOrNull() ?: continue

                // Read temperature
                val tempFile = File(entry, "temp")
                val tempValue = if (tempFile.exists()) {
                    readSysfsFile(tempFile.absolutePath)?.let { raw ->
                        val parsed = raw.trim().toFloatOrNull()
                        // Some drivers report millidegrees
                        if (parsed != null && parsed > 1000) parsed / 1000f else parsed
                    }
                } else null

                // Read type
                val typeFile = File(entry, "type")
                val type = if (typeFile.exists()) {
                    readSysfsFile(typeFile.absolutePath)?.trim() ?: "unknown"
                } else "unknown"

                // Read mode
                val modeFile = File(entry, "mode")
                val mode = if (modeFile.exists()) {
                    readSysfsFile(modeFile.absolutePath)?.trim() ?: ""
                } else ""

                if (tempValue != null) {
                    zones.add(ThermalZone(zoneIndex, type, tempValue, mode))
                }
            }
        } catch (e: SecurityException) {
            Log.e(TAG, "Permission denied reading thermal data: ${e.message}")
            return null
        } catch (e: Exception) {
            Log.e(TAG, "Error reading thermal zones: ${e.message}", e)
            return null
        }

        if (zones.isEmpty()) return null

        val maxTemp = zones.maxOfOrNull { it.temperatureCelsius } ?: 0f
        val avgTemp = zones.map { it.temperatureCelsius }.average().toFloat()

        return ThermalSnapshot(
            timestamp = System.currentTimeMillis(),
            zones = zones,
            maxTemperature = maxTemp,
            averageTemperature = avgTemp
        )
    }

    /**
     * Read a sysfs file safely using BufferedReader.
     */
    private fun readSysfsFile(path: String): String? {
        return try {
            BufferedReader(InputStreamReader(FileInputStream(path), Charsets.UTF_8)).use { reader ->
                reader.readLine()
            }
        } catch (e: Exception) {
            Log.d(TAG, "Failed to read sysfs file: $path - ${e.message}")
            null
        }
    }

    /**
     * Get the thermal history (thread-safe snapshot).
     */
    fun getHistory(): List<ThermalSnapshot> {
        synchronized(thermalHistory) {
            return thermalHistory.toList()
        }
    }

    /**
     * Get the latest thermal snapshot.
     */
    fun getLatest(): ThermalSnapshot? {
        synchronized(thermalHistory) {
            return thermalHistory.lastOrNull()
        }
    }

    /**
     * Get the current maximum temperature across all zones (thread-safe).
     */
    fun getMaxTemperature(): Float {
        val snapshot = synchronized(thermalHistory) {
            thermalHistory.lastOrNull()
        }
        return snapshot?.maxTemperature ?: 0f
    }

    /**
     * Set callback for thermal updates.
     */
    fun setOnUpdateListener(listener: (ThermalSnapshot) -> Unit) {
        onUpdate = listener
    }

    /**
     * Check if monitoring is active.
     */
    fun isActive(): Boolean = isMonitoring

    /**
     * Clear thermal history.
     */
    fun clearHistory() {
        synchronized(thermalHistory) {
            thermalHistory.clear()
        }
    }

    /**
     * Cleanup all resources.
     */
    fun cleanup() {
        stopMonitoring()
        clearHistory()
        onUpdate = null
        try {
            monitorThread?.join(POLL_INTERVAL_MS)
        } catch (e: InterruptedException) {
            Log.d(TAG, "Thread join interrupted during cleanup")
        }
        monitorThread = null
        Log.d(TAG, "Cleanup complete")
    }
}
