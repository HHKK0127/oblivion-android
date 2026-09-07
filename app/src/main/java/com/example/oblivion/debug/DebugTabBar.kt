package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.util.TypedValue
import android.view.View
import android.widget.Button
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import kotlin.math.roundToInt

/**
 * Tab bar widget for debug menu navigation.
 * Supports scrolling for many tabs (22 tabs).
 */
class DebugTabBar(
    private val context: Context,
    private val onTabSelected: (Int) -> Unit
) {
    data class TabItem(
        val name: String,
        val color: Int,
        val badge: String? = null // Optional badge text (e.g., count)
    )

    private val tabs = mutableListOf<TabItem>()
    private val tabButtons = mutableListOf<Button>()
    private var container: HorizontalScrollView? = null
    private var tabBar: LinearLayout? = null
    private var currentIndex = 0

    fun addTab(name: String, color: Int, badge: String? = null) {
        tabs.add(TabItem(name, color, badge))
    }

    fun build(): HorizontalScrollView {
        container = HorizontalScrollView(context).apply {
            isHorizontalScrollBarEnabled = false
        }

        tabBar = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val dp6 = DebugButtonWidget.dpToPx(context, 6)
        val dp12 = DebugButtonWidget.dpToPx(context, 12)

        tabs.forEachIndexed { index, tab ->
            val btn = Button(context).apply {
                text = if (tab.badge != null) "${tab.name}(${tab.badge})" else tab.name
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
                setTextColor(Color.WHITE)
                setBackgroundColor(
                    if (index == currentIndex) tab.color
                    else DebugButtonWidget.darkenColor(tab.color, 0.6f)
                )
                setPadding(dp12, dp6, dp12, dp6)
                isAllCaps = false

                val params = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    marginEnd = dp4
                }
                layoutParams = params

                setOnClickListener { selectTab(index) }
            }
            tabBar?.addView(btn)
            tabButtons.add(btn)
        }

        container?.addView(tabBar)
        return container!!
    }

    fun selectTab(index: Int) {
        if (index !in tabs.indices) return
        currentIndex = index

        // Update button colors
        tabButtons.forEachIndexed { i, btn ->
            btn.setBackgroundColor(
                if (i == index) tabs[i].color
                else DebugButtonWidget.darkenColor(tabs[i].color, 0.6f)
            )
        }

        onTabSelected(index)
    }

    /**
     * Select tab without triggering the callback (for programmatic use).
     */
    fun selectTabSilently(index: Int) {
        if (index !in tabs.indices) return
        currentIndex = index

        // Update button colors only
        tabButtons.forEachIndexed { i, btn ->
            btn.setBackgroundColor(
                if (i == index) tabs[i].color
                else DebugButtonWidget.darkenColor(tabs[i].color, 0.6f)
            )
        }
    }

    fun getCurrentIndex(): Int = currentIndex

    fun getTabCount(): Int = tabs.size

    /**
     * Update badge text for a specific tab.
     */
    fun updateBadge(index: Int, badge: String?) {
        if (index !in tabs.indices) return
        val tab = tabs[index]
        tabs[index] = tab.copy(badge = badge)
        tabButtons[index].text = if (badge != null) "${tab.name}($badge)" else tab.name
    }
}
