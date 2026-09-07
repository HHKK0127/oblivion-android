package com.example.oblivion.debug

import android.content.Context
import android.content.SharedPreferences
import android.util.Log

/**
 * SharedPreferences wrapper for persisting debug menu state.
 * Uses commit() for important saves, apply() for frequent updates.
 */
class DebugStorage(context: Context) {

    companion object {
        private const val TAG = "DebugStorage"
        private const val PREFS_NAME = "oblivion_debug_prefs"
        private const val KEY_LAST_TAB = "last_tab"
        private const val KEY_THEME = "theme"
        private const val KEY_SCROLL_OFFSET = "scroll_offset_"
        private const val KEY_DRAG_ORDER = "drag_order_"
        private const val KEY_PANEL_VISIBLE = "panel_visible"
        private const val KEY_FPS_MONITOR = "fps_monitor_enabled"
        private const val KEY_THERMAL_MONITOR = "thermal_monitor_enabled"
        private const val KEY_GESTURE_VIZ = "gesture_viz_enabled"
        private const val KEY_NETWORK_MONITOR = "network_monitor_enabled"
    }

    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    /**
     * Save last active tab index. Uses commit() for persistence guarantee.
     */
    fun saveLastTab(tabIndex: Int) {
        prefs.edit().putInt(KEY_LAST_TAB, tabIndex).commit()
        Log.d(TAG, "Saved last tab: $tabIndex")
    }

    /**
     * Get last active tab index. Returns 0 (Player) as default.
     */
    fun getLastTab(): Int {
        return prefs.getInt(KEY_LAST_TAB, 0)
    }

    /**
     * Save theme preference. Uses commit() for persistence.
     */
    fun saveTheme(themeName: String) {
        prefs.edit().putString(KEY_THEME, themeName).commit()
        Log.d(TAG, "Saved theme: $themeName")
    }

    /**
     * Get saved theme. Returns "dark" as default.
     */
    fun getTheme(): String {
        return prefs.getString(KEY_THEME, "dark") ?: "dark"
    }

    /**
     * Save scroll offset for a specific tab. Uses apply() for frequent updates.
     */
    fun saveScrollOffset(tabIndex: Int, offset: Float) {
        prefs.edit().putFloat(KEY_SCROLL_OFFSET + tabIndex, offset).apply()
    }

    /**
     * Get scroll offset for a specific tab. Returns 0f as default.
     */
    fun getScrollOffset(tabIndex: Int): Float {
        return prefs.getFloat(KEY_SCROLL_OFFSET + tabIndex, 0f)
    }

    /**
     * Save drag order for widgets in a tab. Uses commit() for important data.
     */
    fun saveDragOrder(tabIndex: Int, order: List<Int>) {
        val orderStr = order.joinToString(",")
        prefs.edit().putString(KEY_DRAG_ORDER + tabIndex, orderStr).commit()
        Log.d(TAG, "Saved drag order for tab $tabIndex: $orderStr")
    }

    /**
     * Get drag order for widgets in a tab. Returns null if no saved order.
     */
    fun getDragOrder(tabIndex: Int): List<Int>? {
        val orderStr = prefs.getString(KEY_DRAG_ORDER + tabIndex, null) ?: return null
        return try {
            orderStr.split(",").map { it.trim().toInt() }
        } catch (e: NumberFormatException) {
            Log.w(TAG, "Failed to parse drag order: $orderStr", e)
            null
        }
    }

    /**
     * Save panel visibility state. Uses apply() for non-critical state.
     */
    fun savePanelVisible(visible: Boolean) {
        prefs.edit().putBoolean(KEY_PANEL_VISIBLE, visible).apply()
    }

    /**
     * Get panel visibility state. Returns false as default.
     */
    fun isPanelVisible(): Boolean {
        return prefs.getBoolean(KEY_PANEL_VISIBLE, false)
    }

    /**
     * Save FPS monitor state. Uses apply().
     */
    fun saveFpsMonitorEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_FPS_MONITOR, enabled).apply()
    }

    /**
     * Get FPS monitor enabled state.
     */
    fun isFpsMonitorEnabled(): Boolean {
        return prefs.getBoolean(KEY_FPS_MONITOR, false)
    }

    /**
     * Save thermal monitor state. Uses apply().
     */
    fun saveThermalMonitorEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_THERMAL_MONITOR, enabled).apply()
    }

    /**
     * Get thermal monitor enabled state.
     */
    fun isThermalMonitorEnabled(): Boolean {
        return prefs.getBoolean(KEY_THERMAL_MONITOR, false)
    }

    /**
     * Save gesture visualizer state. Uses apply().
     */
    fun saveGestureVizEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_GESTURE_VIZ, enabled).apply()
    }

    /**
     * Get gesture visualizer enabled state.
     */
    fun isGestureVizEnabled(): Boolean {
        return prefs.getBoolean(KEY_GESTURE_VIZ, false)
    }

    /**
     * Save network monitor state. Uses apply().
     */
    fun saveNetworkMonitorEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_NETWORK_MONITOR, enabled).apply()
    }

    /**
     * Get network monitor enabled state.
     */
    fun isNetworkMonitorEnabled(): Boolean {
        return prefs.getBoolean(KEY_NETWORK_MONITOR, false)
    }

    /**
     * Clear all stored debug preferences.
     */
    fun clearAll() {
        prefs.edit().clear().commit()
        Log.i(TAG, "Cleared all debug preferences")
    }
}
