package com.example.oblivion.debug

import android.util.Log
import android.view.Choreographer

/**
 * FPS monitor using Choreographer.FrameCallback for accurate frame timing.
 * Tracks frame count, jank frames (frames exceeding 16.67ms), and provides
 * real-time FPS statistics.
 */
class DebugFpsMonitor {

    companion object {
        private const val TAG = "DebugFpsMonitor"
        private const val TARGET_FRAME_TIME_NS = 16_666_667L // ~60fps in nanoseconds
        private const val JANK_THRESHOLD_NS = 20_000_000L    // 20ms threshold for jank
        private const val STATS_UPDATE_INTERVAL_NS = 1_000_000_000L // 1 second
    }

    // Callback reference for proper cleanup
    private var frameCallback: Choreographer.FrameCallback? = null
    private var choreographer: Choreographer? = null

    // Monitoring state
    private var isMonitoring = false

    // Frame tracking
    private var frameCount = 0L
    private var jankCount = 0L
    private var totalFrameTimeNs = 0L
    private var lastFrameTimeNs = 0L
    private var statsStartTimeNs = 0L

    // Current statistics
    private var currentFps = 0f
    private var currentAvgFrameTimeMs = 0f
    private var currentJankPercentage = 0f
    private var currentMinFrameTimeMs = Float.MAX_VALUE
    private var currentMaxFrameTimeMs = 0f

    // Callback for stats updates
    private var onStatsUpdated: ((FpsStats) -> Unit)? = null

    /**
     * FPS statistics data class.
     */
    data class FpsStats(
        val fps: Float,
        val avgFrameTimeMs: Float,
        val minFrameTimeMs: Float,
        val maxFrameTimeMs: Float,
        val jankPercentage: Float,
        val totalFrames: Long,
        val jankFrames: Long
    )

    /**
     * Start monitoring FPS. Uses Choreographer for accurate frame timing.
     */
    fun startMonitoring() {
        if (isMonitoring) {
            Log.w(TAG, "Already monitoring, ignoring startMonitoring()")
            return
        }

        // Clean up any previous callback before creating a new one
        stopMonitoring()
        Thread.sleep(50)

        choreographer = Choreographer.getInstance()
        isMonitoring = true
        frameCount = 0L
        jankCount = 0L
        totalFrameTimeNs = 0L
        lastFrameTimeNs = 0
        currentMinFrameTimeMs = Float.MAX_VALUE
        currentMaxFrameTimeMs = 0f
        statsStartTimeNs = System.nanoTime()

        frameCallback = object : Choreographer.FrameCallback {
            override fun doFrame(frameTimeNanos: Long) {
                // Early return if monitoring was stopped
                if (!isMonitoring) return

                // Null-check choreographer for safety
                val choreo = choreographer ?: return

                if (lastFrameTimeNs > 0) {
                    val frameDurationNs = frameTimeNanos - lastFrameTimeNs
                    val frameDurationMs = frameDurationNs / 1_000_000f

                    // Track frame
                    frameCount++
                    totalFrameTimeNs += frameDurationNs

                    // Track min/max
                    if (frameDurationMs < currentMinFrameTimeMs) {
                        currentMinFrameTimeMs = frameDurationMs
                    }
                    if (frameDurationMs > currentMaxFrameTimeMs) {
                        currentMaxFrameTimeMs = frameDurationMs
                    }

                    // Track jank
                    if (frameDurationNs > JANK_THRESHOLD_NS) {
                        jankCount++
                    }

                    // Update stats every second
                    val elapsedNs = frameTimeNanos - statsStartTimeNs
                    if (elapsedNs >= STATS_UPDATE_INTERVAL_NS) {
                        updateStats(elapsedNs)
                        statsStartTimeNs = frameTimeNanos
                        resetCounters()
                    }
                }

                lastFrameTimeNs = frameTimeNanos

                // Request next frame callback using local reference
                if (isMonitoring) {
                    choreo.postFrameCallback(this)
                }
            }
        }

        choreographer?.postFrameCallback(frameCallback!!)
        Log.i(TAG, "FPS monitoring started")
    }

    /**
     * Stop monitoring FPS. Properly cleans up the callback.
     */
    fun stopMonitoring() {
        if (!isMonitoring) {
            Log.w(TAG, "Not monitoring, ignoring stopMonitoring()")
            return
        }

        // Set flag first to prevent re-posting from callback
        isMonitoring = false

        // Safe callback removal with null checks and try-catch
        val callback = frameCallback
        val choreo = choreographer
        if (callback != null && choreo != null) {
            try {
                choreo.removeFrameCallback(callback)
            } catch (e: Exception) {
                Log.w(TAG, "Error removing frame callback: ${e.message}")
            }
        }
        frameCallback = null
        choreographer = null
        Log.i(TAG, "FPS monitoring stopped. Total frames: $frameCount, Jank: $jankCount")
    }

    /**
     * Update FPS statistics from accumulated frame data.
     */
    private fun updateStats(elapsedNs: Long) {
        if (frameCount == 0L) return

        val elapsedSec = elapsedNs / 1_000_000_000f
        currentFps = frameCount / elapsedSec
        currentAvgFrameTimeMs = (totalFrameTimeNs / frameCount) / 1_000_000f
        currentJankPercentage = if (frameCount > 0L) {
            (jankCount.toFloat() / frameCount) * 100f
        } else 0f

        val stats = FpsStats(
            fps = currentFps,
            avgFrameTimeMs = currentAvgFrameTimeMs,
            minFrameTimeMs = if (currentMinFrameTimeMs == Float.MAX_VALUE) 0f else currentMinFrameTimeMs,
            maxFrameTimeMs = currentMaxFrameTimeMs,
            jankPercentage = currentJankPercentage,
            totalFrames = frameCount,
            jankFrames = jankCount
        )

        onStatsUpdated?.invoke(stats)
    }

    /**
     * Reset frame counters for next measurement interval.
     */
    private fun resetCounters() {
        frameCount = 0L
        jankCount = 0L
        totalFrameTimeNs = 0L
        currentMinFrameTimeMs = Float.MAX_VALUE
        currentMaxFrameTimeMs = 0f
    }

    /**
     * Set callback for real-time stats updates.
     */
    fun setOnStatsUpdatedListener(listener: (FpsStats) -> Unit) {
        onStatsUpdated = listener
    }

    /**
     * Get the current FPS value.
     */
    fun getCurrentFps(): Float = currentFps

    /**
     * Check if currently monitoring.
     */
    fun isActive(): Boolean = isMonitoring

    /**
     * Get current statistics snapshot.
     */
    fun getStats(): FpsStats {
        return FpsStats(
            fps = currentFps,
            avgFrameTimeMs = currentAvgFrameTimeMs,
            minFrameTimeMs = if (currentMinFrameTimeMs == Float.MAX_VALUE) 0f else currentMinFrameTimeMs,
            maxFrameTimeMs = currentMaxFrameTimeMs,
            jankPercentage = currentJankPercentage,
            totalFrames = frameCount,
            jankFrames = jankCount
        )
    }

    /**
     * Cleanup all resources.
     */
    fun cleanup() {
        stopMonitoring()
        onStatsUpdated = null
        Log.d(TAG, "Cleanup complete")
    }
}
