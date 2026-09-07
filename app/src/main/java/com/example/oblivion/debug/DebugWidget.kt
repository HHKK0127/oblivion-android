package com.example.oblivion.debug

import android.content.Context
import android.view.View

/**
 * Base interface for all debug menu widgets.
 * Inspired by ImGui's widget pattern - each widget manages its own state and rendering.
 */
interface DebugWidget {
    /** Unique identifier for this widget */
    val id: String

    /** Whether this widget is currently enabled/interactive */
    var enabled: Boolean

    /** Create and return the Android View for this widget */
    fun createView(context: Context): View

    /** Get the underlying Android View (null if not yet created) */
    fun getView(): View?

    /** Update the widget's display state */
    fun update()
}

/**
 * Abstract base implementation of DebugWidget with common functionality.
 */
abstract class BaseDebugWidget(
    override val id: String
) : DebugWidget {

    override var enabled: Boolean = true
        set(value) {
            field = value
            // Use post to ensure UI updates happen on the UI thread
            getView()?.post {
                getView()?.isEnabled = value
                getView()?.alpha = if (value) 1.0f else 0.5f
            }
        }

    protected var widgetView: View? = null

    override fun getView(): View? = widgetView

    override fun update() {
        // Use post to ensure UI thread safety
        widgetView?.let { view ->
            view.post {
                view.isEnabled = enabled
                view.alpha = if (enabled) 1.0f else 0.5f
            }
        }
    }

    /**
     * Cleanup widget resources. Override in subclasses if needed.
     */
    open fun cleanup() {
        widgetView = null
    }
}
