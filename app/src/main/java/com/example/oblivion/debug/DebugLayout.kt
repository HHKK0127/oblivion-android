package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.util.TypedValue
import android.view.View
import android.widget.GridLayout
import android.widget.LinearLayout
import android.widget.TextView
import kotlin.math.roundToInt

/**
 * Layout manager for debug menu widgets.
 * Provides ImGui-style layout helpers: Columns, Spacing, Separator.
 */
class DebugLayout(private val context: Context) {

    enum class ColumnCount(val value: Int) {
        ONE(1),
        TWO(2),
        THREE(3),
        FOUR(4)
    }

    private var currentColumnCount = ColumnCount.TWO
    private var widgetIndex = 0
    private val widgets = mutableListOf<DebugWidget>()
    private var gridLayout: GridLayout? = null

    /**
     * Set the number of columns for the grid layout.
     */
    fun setColumns(count: ColumnCount) {
        currentColumnCount = count
        gridLayout?.columnCount = count.value
    }

    /**
     * Add a widget to the layout.
     * Automatically calculates row/column position.
     */
    fun addWidget(widget: DebugWidget): DebugWidget {
        widgets.add(widget)
        return widget
    }

    /**
     * Add a separator line between widget groups.
     */
    fun addSeparator(): DebugWidget {
        val separator = DebugSeparatorWidget("sep_${widgetIndex++}")
        widgets.add(separator)
        return separator
    }

    /**
     * Add a label (non-interactive text) to the layout.
     */
    fun addLabel(text: String, color: Int = Color.parseColor("#AAAAAA")): DebugWidget {
        val label = DebugLabelWidget("label_${widgetIndex++}", text, color)
        widgets.add(label)
        return label
    }

    /**
     * Build all widgets into a GridLayout and return it.
     */
    fun build(): GridLayout {
        gridLayout = GridLayout(context).apply {
            columnCount = currentColumnCount.value
            useDefaultMargins = true
        }

        val parentWidth = context.resources.displayMetrics.widthPixels -
            DebugButtonWidget.dpToPx(context, 32) // padding
        val btnWidth = (parentWidth - DebugButtonWidget.dpToPx(context, 4) * (currentColumnCount.value - 1)) / currentColumnCount.value

        widgets.forEachIndexed { index, widget ->
            val view = widget.createView(context)
            val row = index / currentColumnCount.value
            val col = index % currentColumnCount.value

            val dp2 = DebugButtonWidget.dpToPx(context, 2)
            val params = GridLayout.LayoutParams(
                GridLayout.spec(row),
                GridLayout.spec(col)
            ).apply {
                width = if (widget is DebugSeparatorWidget) GridLayout.LayoutParams.MATCH_PARENT else btnWidth
                height = GridLayout.LayoutParams.WRAP_CONTENT
                setMargins(dp2, dp2, dp2, dp2)
            }
            view.layoutParams = params
            gridLayout?.addView(view)
        }

        return requireNotNull(gridLayout) { "GridLayout not initialized" }
    }

    /**
     * Get all widgets in this layout.
     */
    fun getWidgets(): List<DebugWidget> = widgets.toList()

    /**
     * Clear all widgets and reset.
     */
    fun clear() {
        widgets.clear()
        widgetIndex = 0
        gridLayout?.removeAllViews()
    }
}

/**
 * Separator widget - draws a horizontal line.
 */
class DebugSeparatorWidget(id: String) : BaseDebugWidget(id) {

    override fun createView(context: Context): View {
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val separator = View(context).apply {
            setBackgroundColor(Color.parseColor("#444444"))
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                DebugButtonWidget.dpToPx(context, 1)
            ).apply {
                topMargin = dp4
                bottomMargin = dp4
            }
        }
        widgetView = separator
        return separator
    }
}

/**
 * Label widget - non-interactive text display.
 */
class DebugLabelWidget(
    id: String,
    private val text: String,
    private val textColor: Int = Color.parseColor("#AAAAAA")
) : BaseDebugWidget(id) {

    override fun createView(context: Context): View {
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val label = TextView(context).apply {
            this.text = this@DebugLabelWidget.text
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            setTextColor(textColor)
            setPadding(dp4, dp4, dp4, dp4)
        }
        widgetView = label
        return label
    }
}
