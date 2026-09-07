package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.util.TypedValue
import android.view.View
import android.widget.Button
import android.widget.GridLayout
import kotlin.math.roundToInt

/**
 * Debug button widget - executes a console command when tapped.
 * Provides visual feedback (flash) on press.
 */
class DebugButtonWidget(
    id: String,
    private val label: String,
    private val command: String,
    private val baseColor: Int = Color.parseColor("#4D6680"),
    private val onClick: ((String) -> Unit)? = null
) : BaseDebugWidget(id) {

    private var button: Button? = null

    override fun createView(context: Context): View {
        val dp8 = dpToPx(context, 8)
        val dp4 = dpToPx(context, 4)

        button = Button(context).apply {
            text = label
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setTextColor(Color.WHITE)
            setBackgroundColor(darkenColor(baseColor, 0.45f))
            setPadding(dp8, dp4, dp8, dp4)
            isAllCaps = false

            setOnClickListener {
                if (!enabled) return@setOnClickListener
                // Flash feedback
                setBackgroundColor(Color.parseColor("#5588FF"))
                postDelayed({ setBackgroundColor(darkenColor(baseColor, 0.45f)) }, 150)
                onClick?.invoke(command)
            }
        }
        widgetView = button
        return requireNotNull(button) { "Button not initialized" }
    }

    fun setGridLayoutPosition(row: Int, col: Int, width: Int) {
        button?.let { btn ->
            val dp2 = dpToPx(btn.context, 2)
            val params = GridLayout.LayoutParams(
                GridLayout.spec(row),
                GridLayout.spec(col)
            ).apply {
                this.width = width
                height = GridLayout.LayoutParams.WRAP_CONTENT
                setMargins(dp2, dp2, dp2, dp2)
            }
            btn.layoutParams = params
        }
    }

    companion object {
        fun dpToPx(context: Context, dp: Int): Int {
            return TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                dp.toFloat(),
                context.resources.displayMetrics
            ).roundToInt()
        }

        fun darkenColor(color: Int, factor: Float): Int {
            val r = (Color.red(color) * factor).toInt().coerceIn(0, 255)
            val g = (Color.green(color) * factor).toInt().coerceIn(0, 255)
            val b = (Color.blue(color) * factor).toInt().coerceIn(0, 255)
            return Color.rgb(r, g, b)
        }
    }
}
