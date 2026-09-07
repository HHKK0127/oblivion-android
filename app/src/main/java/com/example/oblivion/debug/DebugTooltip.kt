package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView

/**
 * Tooltip system for debug menu widgets.
 * Shows help text on long press.
 */
class DebugTooltip(private val context: Context) {

    private var tooltipView: TextView? = null
    private var parentLayout: FrameLayout? = null
    private val handler = Handler(Looper.getMainLooper())
    private var hideRunnable: Runnable? = null

    fun attachTo(parent: FrameLayout) {
        parentLayout = parent
    }

    /**
     * Attach a long-press tooltip to a view.
     */
    fun bindTo(view: View, text: String, durationMs: Long = 3000) {
        view.setOnLongClickListener {
            show(view, text, durationMs)
            true
        }
    }

    /**
     * Show tooltip near the given anchor view.
     */
    fun show(anchor: View, text: String, durationMs: Long = 3000) {
        hide()

        val parent = parentLayout ?: return

        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp12 = DebugButtonWidget.dpToPx(context, 12)

        tooltipView = TextView(context).apply {
            this.text = text
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            setTextColor(Color.WHITE)
            setPadding(dp12, dp8, dp12, dp8)

            val bg = GradientDrawable().apply {
                setColor(Color.parseColor("#DD222222"))
                cornerRadius = DebugButtonWidget.dpToPx(context, 4).toFloat()
                setStroke(1, Color.parseColor("#666666"))
            }
            background = bg

            // Position below the anchor
            val location = IntArray(2)
            anchor.getLocationOnScreen(location)
            val parentLocation = IntArray(2)
            parent.getLocationOnScreen(parentLocation)

            val x = location[0] - parentLocation[0]
            val y = location[1] - parentLocation[1] + anchor.height + dp8

            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                leftMargin = x
                topMargin = y
            }
        }

        parent.addView(tooltipView)

        // Auto-hide after duration
        hideRunnable = Runnable { hide() }
        handler.postDelayed(hideRunnable!!, durationMs)
    }

    /**
     * Hide the current tooltip.
     */
    fun hide() {
        hideRunnable?.let { handler.removeCallbacks(it) }
        hideRunnable = null
        tooltipView?.let {
            (it.parent as? FrameLayout)?.removeView(it)
        }
        tooltipView = null
    }
}
