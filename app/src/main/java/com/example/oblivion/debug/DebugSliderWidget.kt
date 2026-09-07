package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.util.TypedValue
import android.view.View
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView

/**
 * Slider debug widget for numeric value adjustment.
 * Extends BaseDebugWidget for consistent debug menu integration.
 */
class DebugSliderWidget(
    id: String,
    private val label: String,
    private val minValue: Int = 0,
    private val maxValue: Int = 100,
    initialValue: Int = 50,
    private val onValueChanged: ((Int) -> Unit)? = null
) : BaseDebugWidget(id) {

    private var currentValue = initialValue
    private var valueLabel: TextView? = null
    private var seekBar: SeekBar? = null

    override fun createView(context: Context): View {
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val dp8 = DebugButtonWidget.dpToPx(context, 8)

        val container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp8, dp4, dp8, dp4)
        }

        // Label row with value
        val labelRow = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        val labelText = TextView(context).apply {
            text = label
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        labelRow.addView(labelText)

        valueLabel = TextView(context).apply {
            text = "$currentValue"
            setTextColor(Color.parseColor("#88AAFF"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
        }
        labelRow.addView(valueLabel)

        container.addView(labelRow)

        // SeekBar
        seekBar = SeekBar(context).apply {
            max = maxValue - minValue
            progress = currentValue - minValue
            setPadding(dp4, dp4, dp4, dp4)

            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                    currentValue = progress + minValue
                    valueLabel?.text = "$currentValue"
                    if (fromUser) {
                        onValueChanged?.invoke(currentValue)
                    }
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        container.addView(seekBar)

        // Min/Max labels
        val rangeRow = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        val minLabel = TextView(context).apply {
            text = "$minValue"
            setTextColor(Color.GRAY)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        rangeRow.addView(minLabel)

        val maxLabel = TextView(context).apply {
            text = "$maxValue"
            setTextColor(Color.GRAY)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        rangeRow.addView(maxLabel)

        container.addView(rangeRow)

        widgetView = container
        return container
    }

    fun setValue(value: Int) {
        currentValue = value.coerceIn(minValue, maxValue)
        seekBar?.progress = currentValue - minValue
        valueLabel?.text = "$currentValue"
    }

    fun getValue(): Int = currentValue
}
