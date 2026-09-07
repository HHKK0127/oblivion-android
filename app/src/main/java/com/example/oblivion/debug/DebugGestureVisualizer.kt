package com.example.oblivion.debug

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.util.Log
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration

/**
 * Custom View overlay for gesture visualization and detection.
 * Renders touch trails, detects taps, swipes, double taps, and long presses.
 */
class DebugGestureVisualizer(context: Context) : View(context) {

    companion object {
        private const val TAG = "DebugGestureVisualizer"
        private const val TRAIL_FADE_DURATION_MS = 1000L
        private const val DOUBLE_TAP_TIMEOUT_MS = 300L
        private const val LONG_PRESS_TIMEOUT_MS = 500L
        private const val MAX_TRAIL_POINTS = 500
    }

    /**
     * Represents a single point in a gesture trail.
     */
    data class TrailPoint(
        val x: Float,
        val y: Float,
        val timestamp: Long
    )

    /**
     * Types of gestures that can be detected.
     */
    enum class GestureType {
        TAP,
        DOUBLE_TAP,
        LONG_PRESS,
        SWIPE_LEFT,
        SWIPE_RIGHT,
        SWIPE_UP,
        SWIPE_DOWN
    }

    // Gesture detection state
    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop
    private var touchDownX = 0f
    private var touchDownY = 0f
    private var touchDownTime = 0L
    private var lastTapTime = 0L
    private var longPressRunnable: Runnable? = null
    private var longPressDetected = false

    // Trail rendering
    private val trailPoints = mutableListOf<TrailPoint>()
    private val trailPath = Path()

    // Paint objects
    private val trailPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 0, 200, 255)
        style = Paint.Style.STROKE
        strokeWidth = 4f
        strokeCap = Paint.Cap.ROUND
        strokeJoin = Paint.Join.ROUND
    }
    private val dotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(255, 255, 100, 100)
        style = Paint.Style.FILL
    }
    private val gestureLabelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(230, 255, 255, 255)
        textSize = 32f
        textAlign = Paint.Align.CENTER
    }
    private val labelBackgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(180, 0, 0, 0)
        style = Paint.Style.FILL
    }

    // Gesture callback
    private var onGestureDetected: ((GestureType, Float, Float) -> Unit)? = null

    // Pending gesture labels for display
    private val gestureLabels = mutableListOf<Pair<String, Long>>()

    init {
        // Ensure touch events are received
        isClickable = true
        isFocusable = true
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        // Request parent not to intercept touch events
        parent.requestDisallowInterceptTouchEvent(true)

        val x = event.x
        val y = event.y
        val currentTime = System.currentTimeMillis()

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                touchDownX = x
                touchDownY = y
                touchDownTime = currentTime
                longPressDetected = false

                // Add trail point
                addTrailPoint(x, y, currentTime)

                // Start long press detection
                cancelLongPressDetection()
                longPressRunnable = Runnable {
                    if (!longPressDetected) {
                        longPressDetected = true
                        addGestureLabel("LONG PRESS", x, y)
                        onGestureDetected?.invoke(GestureType.LONG_PRESS, x, y)
                        Log.d(TAG, "Long press detected at ($x, $y)")
                    }
                }
                postDelayed(longPressRunnable, LONG_PRESS_TIMEOUT_MS)

                invalidate()
                return true
            }

            MotionEvent.ACTION_MOVE -> {
                if (!longPressDetected) {
                    // Check if moved beyond touch slop (cancel long press)
                    val dx = x - touchDownX
                    val dy = y - touchDownY
                    if (dx * dx + dy * dy > touchSlop * touchSlop) {
                        cancelLongPressDetection()
                    }
                }

                addTrailPoint(x, y, currentTime)
                invalidate()
                return true
            }

            MotionEvent.ACTION_UP -> {
                cancelLongPressDetection()

                val dx = x - touchDownX
                val dy = y - touchDownY
                val distance = Math.sqrt((dx * dx + dy * dy).toDouble()).toFloat()
                val duration = currentTime - touchDownTime

                if (!longPressDetected) {
                    if (distance < touchSlop && duration < 300) {
                        // Tap detected - check for double tap
                        if (currentTime - lastTapTime < DOUBLE_TAP_TIMEOUT_MS) {
                            addGestureLabel("DOUBLE TAP", x, y)
                            onGestureDetected?.invoke(GestureType.DOUBLE_TAP, x, y)
                            Log.d(TAG, "Double tap detected at ($x, $y)")
                            lastTapTime = 0L
                        } else {
                            addGestureLabel("TAP", x, y)
                            onGestureDetected?.invoke(GestureType.TAP, x, y)
                            Log.d(TAG, "Tap detected at ($x, $y)")
                            lastTapTime = currentTime
                        }
                    } else if (distance > touchSlop * 3 && duration < 500) {
                        // Swipe detection
                        val angle = Math.atan2(dy.toDouble(), dx.toDouble())
                        val gesture = when {
                            angle > -Math.PI / 4 && angle < Math.PI / 4 -> GestureType.SWIPE_RIGHT
                            angle > Math.PI / 4 && angle < 3 * Math.PI / 4 -> GestureType.SWIPE_DOWN
                            angle < -Math.PI / 4 && angle > -3 * Math.PI / 4 -> GestureType.SWIPE_UP
                            else -> GestureType.SWIPE_LEFT
                        }
                        val label = when (gesture) {
                            GestureType.SWIPE_LEFT -> "SWIPE LEFT"
                            GestureType.SWIPE_RIGHT -> "SWIPE RIGHT"
                            GestureType.SWIPE_UP -> "SWIPE UP"
                            GestureType.SWIPE_DOWN -> "SWIPE DOWN"
                            else -> "SWIPE"
                        }
                        val midX = (touchDownX + x) / 2f
                        val midY = (touchDownY + y) / 2f
                        addGestureLabel(label, midX, midY)
                        onGestureDetected?.invoke(gesture, midX, midY)
                        Log.d(TAG, "$label detected from ($touchDownX,$touchDownY) to ($x,$y)")
                    }
                }

                invalidate()
                return true
            }

            MotionEvent.ACTION_CANCEL -> {
                cancelLongPressDetection()
                return true
            }
        }

        return super.onTouchEvent(event)
    }

    /**
     * Add a point to the trail.
     */
    private fun addTrailPoint(x: Float, y: Float, timestamp: Long) {
        trailPoints.add(TrailPoint(x, y, timestamp))
        if (trailPoints.size > MAX_TRAIL_POINTS) {
            trailPoints.removeAt(0)
        }
    }

    /**
     * Add a gesture label for display.
     */
    private fun addGestureLabel(label: String, x: Float, y: Float) {
        gestureLabels.add(Pair(label, System.currentTimeMillis()))
        // Keep only recent labels
        if (gestureLabels.size > 5) {
            gestureLabels.removeAt(0)
        }
    }

    /**
     * Cancel pending long press detection.
     */
    private fun cancelLongPressDetection() {
        longPressRunnable?.let { removeCallbacks(it) }
        longPressRunnable = null
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val currentTime = System.currentTimeMillis()

        // Draw trail with fade-out
        trailPath.reset()
        var isFirst = true
        var lastValidPoint: TrailPoint? = null

        for (point in trailPoints) {
            val age = currentTime - point.timestamp
            if (age > TRAIL_FADE_DURATION_MS) continue

            // Calculate alpha based on age
            val alpha = ((1.0f - age.toFloat() / TRAIL_FADE_DURATION_MS) * 200).toInt().coerceIn(0, 200)
            trailPaint.alpha = alpha

            if (isFirst) {
                trailPath.moveTo(point.x, point.y)
                isFirst = false
            } else {
                trailPath.lineTo(point.x, point.y)
            }

            // Draw dot at each point
            dotPaint.alpha = alpha
            canvas.drawCircle(point.x, point.y, 3f, dotPaint)

            lastValidPoint = point
        }

        // Draw the trail path
        if (!isFirst) {
            canvas.drawPath(trailPath, trailPaint)
        }

        // Draw gesture labels
        val labelIterator = gestureLabels.iterator()
        while (labelIterator.hasNext()) {
            val (label, time) = labelIterator.next()
            val age = currentTime - time
            if (age > 1500) {
                labelIterator.remove()
                continue
            }

            val alpha = ((1.0f - age / 1500f) * 255).toInt().coerceIn(0, 255)
            val yOffset = -age / 10f // Float upward

            // Draw label background
            gestureLabelPaint.alpha = alpha
            labelBackgroundPaint.alpha = (alpha * 0.7f).toInt()
            val textWidth = gestureLabelPaint.measureText(label)
            val labelX = width / 2f
            val labelY = height / 2f + yOffset

            canvas.drawRect(
                labelX - textWidth / 2 - 10f, labelY - 25f,
                labelX + textWidth / 2 + 10f, labelY + 10f,
                labelBackgroundPaint
            )
            canvas.drawText(label, labelX, labelY, gestureLabelPaint)
        }

        // Clean up old trail points
        trailPoints.removeAll { currentTime - it.timestamp > TRAIL_FADE_DURATION_MS }

        // Continue animation if there are active trails or labels
        if (trailPoints.isNotEmpty() || gestureLabels.isNotEmpty()) {
            postInvalidateDelayed(16) // ~60fps animation
        }
    }

    /**
     * Set callback for gesture detection.
     */
    fun setOnGestureDetectedListener(listener: (GestureType, Float, Float) -> Unit) {
        onGestureDetected = listener
    }

    /**
     * Clear all trails and labels.
     */
    fun clearTrails() {
        trailPoints.clear()
        gestureLabels.clear()
        invalidate()
    }

    /**
     * Clean up resources.
     */
    fun cleanup() {
        cancelLongPressDetection()
        trailPoints.clear()
        gestureLabels.clear()
        onGestureDetected = null
        Log.d(TAG, "Cleanup complete")
    }
}
