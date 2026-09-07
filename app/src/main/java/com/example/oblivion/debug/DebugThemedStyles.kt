package com.example.oblivion.debug

import android.graphics.Color

/**
 * Theme and color management for the debug UI.
 * Supports dark/light themes with customizable colors.
 */
object DebugThemedStyles {

    /**
     * Theme color set for the debug menu.
     */
    data class Theme(
        val name: String,
        val overlayBackground: Int,
        val panelBackground: Int,
        val tabBarBackground: Int,
        val tabActiveBackground: Int,
        val tabInactiveBackground: Int,
        val tabActiveText: Int,
        val tabInactiveText: Int,
        val buttonBackground: Int,
        val buttonPressedBackground: Int,
        val buttonText: Int,
        val sectionHeaderBackground: Int,
        val sectionHeaderText: Int,
        val sliderTrack: Int,
        val sliderThumb: Int,
        val checkboxChecked: Int,
        val checkboxUnchecked: Int,
        val inputBackground: Int,
        val inputText: Int,
        val inputHint: Int,
        val divider: Int,
        val errorText: Int,
        val successText: Int,
        val warningText: Int
    )

    val DARK_THEME = Theme(
        name = "dark",
        overlayBackground = Color.argb(191, 0, 0, 0),       // 75% black
        panelBackground = Color.argb(230, 25, 25, 35),       // dark blue-gray
        tabBarBackground = Color.argb(230, 30, 30, 45),       // slightly lighter
        tabActiveBackground = Color.argb(230, 102, 179, 102), // green
        tabInactiveBackground = Color.argb(204, 51, 51, 77),  // muted blue-gray
        tabActiveText = Color.WHITE,
        tabInactiveText = Color.argb(204, 179, 179, 179),     // light gray
        buttonBackground = Color.argb(204, 77, 77, 89),       // medium gray
        buttonPressedBackground = Color.argb(230, 128, 179, 255), // light blue
        buttonText = Color.WHITE,
        sectionHeaderBackground = Color.argb(204, 45, 45, 60),
        sectionHeaderText = Color.argb(230, 200, 200, 220),
        sliderTrack = Color.argb(153, 100, 100, 120),         // 60% gray-blue
        sliderThumb = Color.argb(230, 102, 179, 102),         // green
        checkboxChecked = Color.argb(230, 102, 179, 102),     // green
        checkboxUnchecked = Color.argb(153, 100, 100, 100),   // gray
        inputBackground = Color.argb(204, 40, 40, 55),
        inputText = Color.WHITE,
        inputHint = Color.argb(128, 150, 150, 170),
        divider = Color.argb(77, 100, 100, 120),              // 30% gray
        errorText = Color.argb(230, 255, 80, 80),             // red
        successText = Color.argb(230, 80, 255, 80),           // green
        warningText = Color.argb(230, 255, 200, 80)           // amber
    )

    val LIGHT_THEME = Theme(
        name = "light",
        overlayBackground = Color.argb(128, 0, 0, 0),        // 50% black
        panelBackground = Color.argb(242, 245, 245, 250),     // near white
        tabBarBackground = Color.argb(242, 235, 235, 240),
        tabActiveBackground = Color.argb(242, 66, 133, 66),   // darker green
        tabInactiveBackground = Color.argb(230, 210, 210, 220),
        tabActiveText = Color.WHITE,
        tabInactiveText = Color.argb(204, 80, 80, 80),
        buttonBackground = Color.argb(230, 220, 220, 230),
        buttonPressedBackground = Color.argb(230, 100, 150, 220),
        buttonText = Color.argb(230, 40, 40, 40),
        sectionHeaderBackground = Color.argb(230, 225, 225, 235),
        sectionHeaderText = Color.argb(230, 50, 50, 60),
        sliderTrack = Color.argb(153, 180, 180, 200),
        sliderThumb = Color.argb(230, 66, 133, 66),
        checkboxChecked = Color.argb(230, 66, 133, 66),
        checkboxUnchecked = Color.argb(153, 180, 180, 180),
        inputBackground = Color.argb(230, 255, 255, 255),
        inputText = Color.argb(230, 30, 30, 30),
        inputHint = Color.argb(128, 150, 150, 160),
        divider = Color.argb(77, 180, 180, 190),
        errorText = Color.argb(230, 200, 50, 50),
        successText = Color.argb(230, 50, 160, 50),
        warningText = Color.argb(230, 200, 150, 30)
    )

    private val themes = mapOf(
        "dark" to DARK_THEME,
        "light" to LIGHT_THEME
    )

    private var currentThemeName = "dark"

    /**
     * Get the current active theme.
     */
    fun current(): Theme = themes[currentThemeName] ?: DARK_THEME

    /**
     * Set the active theme by name.
     * @return true if theme was found and applied, false otherwise.
     */
    fun setTheme(name: String): Boolean {
        val theme = themes[name] ?: return false
        currentThemeName = name
        return true
    }

    /**
     * Get a theme by name, falling back to dark theme.
     */
    fun getTheme(name: String): Theme = themes[name] ?: DARK_THEME

    /**
     * List all available theme names.
     */
    fun availableThemes(): List<String> = themes.keys.toList()

    /**
     * Tab-specific accent colors used to differentiate tab content areas.
     */
    object TabAccents {
        val PLAYER    = Color.argb(204, 77, 128, 179)   // blue
        val COMBAT    = Color.argb(204, 179, 77, 77)    // red
        val ITEMS     = Color.argb(204, 153, 128, 51)   // gold
        val MAGIC     = Color.argb(204, 128, 51, 179)   // purple
        val QUEST     = Color.argb(204, 179, 128, 51)   // amber
        val NPC       = Color.argb(204, 102, 128, 77)   // green
        val DIALOGUE  = Color.argb(204, 153, 102, 153)  // mauve
        val WORLD     = Color.argb(204, 77, 153, 153)   // teal
        val SAVE      = Color.argb(204, 128, 128, 77)   // olive
        val SYSTEM    = Color.argb(204, 102, 102, 128)   // slate
        val SOUND     = Color.argb(204, 153, 77, 153)   // orchid
        val ASSETS    = Color.argb(204, 77, 153, 128)   // seafoam
        val LOGS      = Color.argb(204, 128, 102, 77)   // brown

        fun forTabIndex(index: Int): Int = when (index) {
            0  -> PLAYER
            1  -> COMBAT
            2  -> ITEMS
            3  -> MAGIC
            4  -> QUEST
            5  -> NPC
            6  -> DIALOGUE
            7  -> WORLD
            8  -> SAVE
            9  -> SYSTEM
            10 -> SOUND
            11 -> ASSETS
            12 -> LOGS
            else -> Color.argb(204, 100, 100, 100)
        }
    }

    /**
     * Convert dp to px density-independent pixels helper.
     * Use context.resources.displayMetrics.density in actual view code.
     */
    fun dpToPx(dp: Float, density: Float): Int = (dp * density + 0.5f).toInt()

    /**
     * Adjust alpha of a color.
     */
    fun withAlpha(color: Int, alpha: Int): Int {
        return Color.argb(alpha, Color.red(color), Color.green(color), Color.blue(color))
    }

    /**
     * Lighten a color by a factor (0.0 to 1.0).
     */
    fun lighten(color: Int, factor: Float): Int {
        val r = (Color.red(color) + (255 - Color.red(color)) * factor).toInt().coerceIn(0, 255)
        val g = (Color.green(color) + (255 - Color.green(color)) * factor).toInt().coerceIn(0, 255)
        val b = (Color.blue(color) + (255 - Color.blue(color)) * factor).toInt().coerceIn(0, 255)
        return Color.argb(Color.alpha(color), r, g, b)
    }

    /**
     * Darken a color by a factor (0.0 to 1.0).
     */
    fun darken(color: Int, factor: Float): Int {
        val r = (Color.red(color) * (1 - factor)).toInt().coerceIn(0, 255)
        val g = (Color.green(color) * (1 - factor)).toInt().coerceIn(0, 255)
        val b = (Color.blue(color) * (1 - factor)).toInt().coerceIn(0, 255)
        return Color.argb(Color.alpha(color), r, g, b)
    }
}
