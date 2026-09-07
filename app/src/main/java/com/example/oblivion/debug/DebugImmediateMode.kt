package com.example.oblivion.debug

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.Log

/**
 * ImGui-style immediate mode UI system for debug overlays.
 * Provides button, checkbox, slider, and collapsing header widgets
 * with state caching and frame-based rendering.
 */
class DebugImmediateMode {

    companion object {
        private const val TAG = "DebugImmediateMode"
        private const val WIDGET_HEIGHT = 40f
        private const val PADDING = 8f
        private const val CORNER_RADIUS = 4f
    }

    /**
     * Cached state for a single widget.
     */
    data class WidgetState(
        var boolValue: Boolean = false,
        var floatValue: Float = 0f,
        var isExpanded: Boolean = false,
        var clickConsumed: Boolean = false,
        var lastFrameId: Long = -1
    )

    // State cache: widget ID -> state
    private val stateCache = mutableMapOf<String, WidgetState>()

    // Current frame ID for cache invalidation
    private var currentFrameId: Long = 0

    // Cursor position for layout
    private var cursorX = 0f
    private var cursorY = 0f
    private var contentWidth = 0f

    // Paint objects
    private val bgPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 40, 40, 50)
        style = Paint.Style.FILL
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 28f
    }
    private val buttonPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 70, 70, 90)
        style = Paint.Style.FILL
    }
    private val buttonPressedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 100, 150, 220)
        style = Paint.Style.FILL
    }
    private val checkboxPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 100, 100, 100)
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val checkboxCheckedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(230, 102, 179, 102)
        style = Paint.Style.FILL
    }
    private val sliderTrackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(150, 100, 100, 120)
        style = Paint.Style.FILL
    }
    private val sliderThumbPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(230, 102, 179, 102)
        style = Paint.Style.FILL
    }
    private val headerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(200, 50, 50, 65)
        style = Paint.Style.FILL
    }
    private val dividerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(80, 100, 100, 120)
        style = Paint.Style.STROKE
        strokeWidth = 1f
    }

    private val rect = RectF()

    /**
     * Begin a new frame. Must be called before any widget calls.
     */
    fun beginFrame(x: Float, y: Float, width: Float) {
        currentFrameId++
        cursorX = x
        cursorY = y
        contentWidth = width
    }

    /**
     * Get or create widget state for the given ID.
     */
    private fun getOrCreateState(id: String): WidgetState {
        return stateCache.getOrPut(id) {
            WidgetState().also { it.lastFrameId = currentFrameId }
        }
    }

    /**
     * Render a button widget.
     * @return true if the button was clicked this frame.
     */
    fun button(id: String, label: String, canvas: Canvas, touchX: Float, touchY: Float,
               isTouchDown: Boolean, isTouchUp: Boolean, theme: DebugThemedStyles.Theme): Boolean {
        val state = getOrCreateState(id)
        val buttonHeight = WIDGET_HEIGHT
        val buttonWidth = contentWidth - PADDING * 2

        // Draw button background
        rect.set(cursorX, cursorY, cursorX + buttonWidth, cursorY + buttonHeight)
        val isPressed = state.clickConsumed && isTouchDown
        val paint = if (state.clickConsumed) buttonPressedPaint else buttonPaint
        canvas.drawRoundRect(rect, CORNER_RADIUS, CORNER_RADIUS, paint)

        // Draw button text
        val textX = cursorX + PADDING
        val textY = cursorY + buttonHeight / 2f - (textPaint.descent() + textPaint.ascent()) / 2f
        textPaint.color = theme.buttonText
        canvas.drawText(label, textX, textY, textPaint)

        // Check for click
        var clicked = false
        if (isTouchDown && rect.contains(touchX, touchY)) {
            state.clickConsumed = true
        }
        if (isTouchUp && state.clickConsumed && rect.contains(touchX, touchY)) {
            clicked = true
            state.clickConsumed = false
        }
        if (isTouchUp && state.clickConsumed) {
            state.clickConsumed = false
        }

        cursorY += buttonHeight + PADDING
        return clicked
    }

    /**
     * Render a checkbox widget.
     * @return true if the checkbox value changed this frame.
     */
    fun checkbox(id: String, label: String, canvas: Canvas, touchX: Float, touchY: Float,
                 isTouchUp: Boolean, theme: DebugThemedStyles.Theme): Boolean {
        val state = getOrCreateState(id)
        val boxSize = 24f
        val totalHeight = WIDGET_HEIGHT

        // Draw checkbox box
        val boxLeft = cursorX + PADDING
        val boxTop = cursorY + (totalHeight - boxSize) / 2f
        rect.set(boxLeft, boxTop, boxLeft + boxSize, boxTop + boxSize)

        if (state.boolValue) {
            canvas.drawRoundRect(rect, 3f, 3f, checkboxCheckedPaint)
        } else {
            canvas.drawRoundRect(rect, 3f, 3f, checkboxPaint)
        }

        // Draw checkmark if checked
        if (state.boolValue) {
            val checkPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = Color.WHITE
                style = Paint.Style.STROKE
                strokeWidth = 3f
                strokeCap = Paint.Cap.ROUND
            }
            canvas.drawLine(boxLeft + 5f, boxTop + boxSize / 2f,
                boxLeft + boxSize / 2f - 1f, boxTop + boxSize - 6f, checkPaint)
            canvas.drawLine(boxLeft + boxSize / 2f - 1f, boxTop + boxSize - 6f,
                boxLeft + boxSize - 4f, boxTop + 5f, checkPaint)
        }

        // Draw label
        textPaint.color = theme.buttonText
        val textX = boxLeft + boxSize + PADDING
        val textY = cursorY + totalHeight / 2f - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText(label, textX, textY, textPaint)

        // Check for click on the entire row
        var changed = false
        val hitRect = RectF(cursorX, cursorY, cursorX + contentWidth, cursorY + totalHeight)
        if (isTouchUp && hitRect.contains(touchX, touchY)) {
            state.boolValue = !state.boolValue
            changed = true
        }

        cursorY += totalHeight + PADDING
        return changed
    }

    /**
     * Render a slider widget.
     * @return true if the slider value changed this frame.
     */
    fun slider(id: String, label: String, min: Float, max: Float, canvas: Canvas,
               touchX: Float, touchY: Float, isTouchDown: Boolean, isTouchMove: Boolean,
               theme: DebugThemedStyles.Theme): Boolean {
        val state = getOrCreateState(id)
        val sliderHeight = WIDGET_HEIGHT
        val trackHeight = 8f
        val thumbRadius = 12f

        // Initialize value if needed
        if (state.floatValue < min || state.floatValue > max) {
            state.floatValue = (min + max) / 2f
        }

        // Draw label with current value
        textPaint.color = theme.buttonText
        val labelText = "$label: ${String.format("%.1f", state.floatValue)}"
        val textY = cursorY + 20f
        canvas.drawText(labelText, cursorX + PADDING, textY, textPaint)

        // Draw track
        val trackY = cursorY + sliderHeight - trackHeight - 4f
        val trackLeft = cursorX + PADDING
        val trackRight = cursorX + contentWidth - PADDING
        rect.set(trackLeft, trackY, trackRight, trackY + trackHeight)
        canvas.drawRoundRect(rect, trackHeight / 2f, trackHeight / 2f, sliderTrackPaint)

        // Draw filled portion
        val fraction = (state.floatValue - min) / (max - min)
        val thumbX = trackLeft + (trackRight - trackLeft) * fraction
        rect.set(trackLeft, trackY, thumbX, trackY + trackHeight)
        sliderThumbPaint.color = DebugThemedStyles.TabAccents.forTabIndex(0)
        canvas.drawRoundRect(rect, trackHeight / 2f, trackHeight / 2f, sliderThumbPaint)

        // Draw thumb
        canvas.drawCircle(thumbX, trackY + trackHeight / 2f, thumbRadius, sliderThumbPaint)

        // Handle touch interaction
        var changed = false
        val hitRect = RectF(cursorX, cursorY, cursorX + contentWidth, cursorY + sliderHeight)
        if ((isTouchDown || isTouchMove) && hitRect.contains(touchX, touchY)) {
            val newFraction = ((touchX - trackLeft) / (trackRight - trackLeft)).coerceIn(0f, 1f)
            val newValue = min + newFraction * (max - min)
            if (kotlin.math.abs(newValue - state.floatValue) > 0.01f) {
                state.floatValue = newValue
                changed = true
            }
        }

        cursorY += sliderHeight + PADDING
        return changed
    }

    /**
     * Render a collapsing header widget.
     * @return true if the section is expanded (content should be rendered).
     */
    fun collapsingHeader(id: String, label: String, canvas: Canvas, touchX: Float,
                         touchY: Float, isTouchUp: Boolean, theme: DebugThemedStyles.Theme): Boolean {
        val state = getOrCreateState(id)
        val headerHeight = WIDGET_HEIGHT

        // Draw header background
        rect.set(cursorX, cursorY, cursorX + contentWidth, cursorY + headerHeight)
        canvas.drawRoundRect(rect, CORNER_RADIUS, CORNER_RADIUS, headerPaint)

        // Draw expand/collapse arrow
        textPaint.color = theme.sectionHeaderText
        val arrow = if (state.isExpanded) "\u25BC" else "\u25B6" // ▼ or ▶
        canvas.drawText(arrow, cursorX + PADDING, cursorY + headerHeight / 2f -
            (textPaint.descent() + textPaint.ascent()) / 2f, textPaint)

        // Draw label
        val labelX = cursorX + PADDING + 30f
        canvas.drawText(label, labelX, cursorY + headerHeight / 2f -
            (textPaint.descent() + textPaint.ascent()) / 2f, textPaint)

        // Check for click
        if (isTouchUp && rect.contains(touchX, touchY)) {
            state.isExpanded = !state.isExpanded
        }

        cursorY += headerHeight + PADDING

        // Draw divider
        canvas.drawLine(cursorX, cursorY, cursorX + contentWidth, cursorY, dividerPaint)
        cursorY += PADDING

        return state.isExpanded
    }

    /**
     * Render a label (non-interactive text).
     */
    fun label(text: String, canvas: Canvas, theme: DebugThemedStyles.Theme,
              color: Int = theme.buttonText, textSize: Float = 28f) {
        textPaint.color = color
        textPaint.textSize = textSize
        val textY = cursorY + textSize / 2f - (textPaint.descent() + textPaint.ascent()) / 2f
        canvas.drawText(text, cursorX + PADDING, textY, textPaint)
        cursorY += textSize + PADDING
        textPaint.textSize = 28f // Reset
    }

    /**
     * Add vertical spacing.
     */
    fun spacing(amount: Float = PADDING) {
        cursorY += amount
    }

    /**
     * Get the current Y cursor position (for tracking total content height).
     */
    fun getCursorY(): Float = cursorY

    /**
     * Reset button click states after consumption.
     * Should be called at end of frame.
     */
    fun resetClickStates() {
        for (entry in stateCache) {
            entry.value.clickConsumed = false
        }
    }

    /**
     * Get the current frame ID.
     */
    fun getFrameId(): Long = currentFrameId

    /**
     * Clear all cached state. Call when the menu is hidden or recreated.
     */
    fun clearState() {
        stateCache.clear()
        currentFrameId = 0
        Log.d(TAG, "State cache cleared")
    }

    /**
     * Get the total content height rendered this frame.
     */
    fun getContentHeight(): Float = cursorY

    /**
     * Set the checkbox value directly (for restoring saved state).
     */
    fun setCheckboxValue(id: String, value: Boolean) {
        val state = getOrCreateState(id)
        state.boolValue = value
    }

    /**
     * Get the checkbox value.
     */
    fun getCheckboxValue(id: String): Boolean {
        return getOrCreateState(id).boolValue
    }

    /**
     * Set the slider value directly (for restoring saved state).
     */
    fun setSliderValue(id: String, value: Float) {
        val state = getOrCreateState(id)
        state.floatValue = value
    }

    /**
     * Get the slider value.
     */
    fun getSliderValue(id: String): Float {
        return getOrCreateState(id).floatValue
    }

    /**
     * Check if a collapsing header is expanded.
     */
    fun isHeaderExpanded(id: String): Boolean {
        return getOrCreateState(id).isExpanded
    }

    /**
     * Set a collapsing header expanded state.
     */
    fun setHeaderExpanded(id: String, expanded: Boolean) {
        val state = getOrCreateState(id)
        state.isExpanded = expanded
    }
}
