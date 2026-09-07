package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.util.TypedValue
import android.view.View
import android.widget.CheckBox
import android.widget.CompoundButton

/**
 * Checkbox debug widget for boolean toggles.
 * Extends BaseDebugWidget for consistent debug menu integration.
 */
class DebugCheckboxWidget(
    id: String,
    private val label: String,
    initialValue: Boolean = false,
    private val onCheckedChanged: ((Boolean) -> Unit)? = null
) : BaseDebugWidget(id) {

    private var isChecked = initialValue
    private var checkbox: CheckBox? = null

    override fun createView(context: Context): View {
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val dp8 = DebugButtonWidget.dpToPx(context, 8)

        checkbox = CheckBox(context).apply {
            text = label
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            isChecked = this@DebugCheckboxWidget.isChecked
            setPadding(dp4, dp8, dp4, dp8)

            setOnCheckedChangeListener { _, checked ->
                this@DebugCheckboxWidget.isChecked = checked
                onCheckedChanged?.invoke(checked)
            }
        }

        widgetView = checkbox
        return checkbox!!
    }

    fun setChecked(checked: Boolean) {
        isChecked = checked
        checkbox?.isChecked = checked
    }

    fun isChecked(): Boolean = isChecked
}
