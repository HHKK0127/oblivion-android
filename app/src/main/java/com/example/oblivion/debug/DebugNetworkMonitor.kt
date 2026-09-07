package com.example.oblivion.debug

import android.util.Log
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

/**
 * Network monitoring with ICMP ping via /system/bin/ping and HTTP fallback.
 * Provides latency measurement and connection status.
 */
class DebugNetworkMonitor {

    companion object {
        private const val TAG = "DebugNetworkMonitor"
        private const val PING_TIMEOUT_MS = 3000
        private const val HTTP_TIMEOUT_MS = 5000
        private const val POLL_INTERVAL_MS = 5000L
        private const val HISTORY_SIZE = 30
        private val PING_TARGETS = listOf("8.8.8.8", "1.1.1.1", "8.8.4.4")
        private const val HTTP_TARGET = "http://www.gstatic.com/generate_204"
    }

    /**
     * Network latency reading.
     */
    data class LatencyReading(
        val timestamp: Long,
        val pingLatencyMs: Long,
        val httpLatencyMs: Long,
        val pingSuccess: Boolean,
        val httpSuccess: Boolean,
        val target: String
    )

    // Monitoring state
    @Volatile
    private var isMonitoring = false
    private var monitorThread: Thread? = null

    // History
    private val latencyHistory = mutableListOf<LatencyReading>()

    // Callback
    private var onUpdate: ((LatencyReading) -> Unit)? = null

    /**
     * Start network monitoring.
     */
    fun startMonitoring() {
        if (isMonitoring) {
            Log.w(TAG, "Already monitoring network")
            return
        }

        isMonitoring = true
        monitorThread = Thread({
            Log.i(TAG, "Network monitoring thread started")
            while (isMonitoring && !Thread.currentThread().isInterrupted) {
                try {
                    val reading = measureLatency()
                    synchronized(latencyHistory) {
                        latencyHistory.add(reading)
                        if (latencyHistory.size > HISTORY_SIZE) {
                            latencyHistory.removeAt(0)
                        }
                    }
                    onUpdate?.invoke(reading)
                    Thread.sleep(POLL_INTERVAL_MS)
                } catch (e: InterruptedException) {
                    Log.d(TAG, "Network monitoring thread interrupted")
                    Thread.currentThread().interrupt()
                    break
                } catch (e: Exception) {
                    Log.e(TAG, "Error measuring network latency: ${e.message}", e)
                    try {
                        Thread.sleep(POLL_INTERVAL_MS)
                    } catch (ie: InterruptedException) {
                        Thread.currentThread().interrupt()
                        break
                    }
                }
            }
            Log.i(TAG, "Network monitoring thread ended")
        }, "DebugNetworkMonitor").apply {
            isDaemon = true
            start()
        }
    }

    /**
     * Stop network monitoring.
     */
    fun stopMonitoring() {
        if (!isMonitoring) return

        isMonitoring = false
        monitorThread?.interrupt()
        try {
            monitorThread?.join(POLL_INTERVAL_MS)
        } catch (e: InterruptedException) {
            Log.d(TAG, "Thread join interrupted")
        }
        monitorThread = null
        Log.i(TAG, "Network monitoring stopped")
    }

    /**
     * Measure latency using ICMP ping and HTTP fallback.
     */
    private fun measureLatency(): LatencyReading {
        var pingLatency = -1L
        var pingSuccess = false
        var pingTarget = ""

        // Try ICMP ping first
        for (target in PING_TARGETS) {
            val (success, latency) = icmpPing(target)
            if (success) {
                pingLatency = latency
                pingSuccess = true
                pingTarget = target
                break
            }
        }

        // HTTP fallback
        val (httpSuccess, httpLatency) = httpLatency()

        return LatencyReading(
            timestamp = System.currentTimeMillis(),
            pingLatencyMs = pingLatency,
            httpLatencyMs = httpLatency,
            pingSuccess = pingSuccess,
            httpSuccess = httpSuccess,
            target = if (pingSuccess) pingTarget else HTTP_TARGET
        )
    }

    /**
     * Execute ICMP ping using /system/bin/ping.
     * @return Pair of (success, latency in ms)
     */
    private fun icmpPing(host: String): Pair<Boolean, Long> {
        var process: Process? = null
        try {
            val command = arrayOf("/system/bin/ping", "-c", "1", "-W", "${PING_TIMEOUT_MS / 1000}", host)
            process = Runtime.getRuntime().exec(command)

            val reader = BufferedReader(InputStreamReader(process.inputStream))
            val startTime = System.currentTimeMillis()

            // Read ping output
            var line: String?
            var latency = -1L
            while (reader.readLine().also { line = it } != null) {
                // Parse latency from "time=XX.X ms" format
                val timeMatch = Regex("time=(\\d+\\.?\\d*)\\s*ms").find(line ?: "")
                if (timeMatch != null) {
                    latency = timeMatch.groupValues[1].toLongOrNull() ?: -1L
                }
            }

            val exitCode = process.waitFor()
            val elapsed = System.currentTimeMillis() - startTime

            return if (exitCode == 0 && latency > 0) {
                Pair(true, latency)
            } else {
                Pair(false, elapsed)
            }
        } catch (e: Exception) {
            Log.d(TAG, "ICMP ping to $host failed: ${e.message}")
            return Pair(false, -1L)
        } finally {
            process?.destroy()
        }
    }

    /**
     * Measure HTTP latency as fallback.
     * @return Pair of (success, latency in ms)
     */
    private fun httpLatency(): Pair<Boolean, Long> {
        var connection: HttpURLConnection? = null
        try {
            val startTime = System.currentTimeMillis()
            val url = URL(HTTP_TARGET)
            connection = url.openConnection() as HttpURLConnection
            connection.apply {
                connectTimeout = HTTP_TIMEOUT_MS
                readTimeout = HTTP_TIMEOUT_MS
                requestMethod = "HEAD"
                instanceFollowRedirects = false
                setRequestProperty("User-Agent", "OblivionAndroid-Debug/1.0")
            }

            connection.connect()
            val latency = System.currentTimeMillis() - startTime

            // HTTP 204 is expected for gstatic.com/generate_204
            val success = connection.responseCode in 200..299
            return Pair(success, latency)
        } catch (e: Exception) {
            Log.d(TAG, "HTTP latency measurement failed: ${e.message}")
            return Pair(false, -1L)
        } finally {
            connection?.disconnect()
        }
    }

    /**
     * Get latency history (thread-safe).
     */
    fun getHistory(): List<LatencyReading> {
        synchronized(latencyHistory) {
            return latencyHistory.toList()
        }
    }

    /**
     * Get the latest latency reading.
     */
    fun getLatest(): LatencyReading? {
        synchronized(latencyHistory) {
            return latencyHistory.lastOrNull()
        }
    }

    /**
     * Get average ping latency from recent readings.
     */
    fun getAveragePingLatency(): Long {
        synchronized(latencyHistory) {
            val successful = latencyHistory.filter { it.pingSuccess && it.pingLatencyMs > 0 }
            return if (successful.isNotEmpty()) {
                successful.map { it.pingLatencyMs }.average().toLong()
            } else -1L
        }
    }

    /**
     * Get average HTTP latency from recent readings.
     */
    fun getAverageHttpLatency(): Long {
        synchronized(latencyHistory) {
            val successful = latencyHistory.filter { it.httpSuccess && it.httpLatencyMs > 0 }
            return if (successful.isNotEmpty()) {
                successful.map { it.httpLatencyMs }.average().toLong()
            } else -1L
        }
    }

    /**
     * Set callback for latency updates.
     */
    fun setOnUpdateListener(listener: (LatencyReading) -> Unit) {
        onUpdate = listener
    }

    /**
     * Check if monitoring is active.
     */
    fun isActive(): Boolean = isMonitoring

    /**
     * Clear history.
     */
    fun clearHistory() {
        synchronized(latencyHistory) {
            latencyHistory.clear()
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
            monitorThread?.join(PING_TIMEOUT_MS.toLong() * 2L)
        } catch (e: InterruptedException) {
            Log.d(TAG, "Thread join interrupted during cleanup")
        }
        monitorThread = null
        Log.d(TAG, "Cleanup complete")
    }
}
