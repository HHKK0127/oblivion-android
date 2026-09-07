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
                    applyEnabledState()
                }
            }

        protected var widgetView: View? = null

        override fun getView(): View? = widgetView

        /**
         * Apply enabled state to the widget view and recursively to child ViewGroups.
         * Must be called on the UI thread.
         */
        private fun applyEnabledState() {
            val view = widgetView ?: return
            view.isEnabled = enabled
            view.alpha = if (enabled) 1.0f else 0.5f
            // Recursively apply to child ViewGroups
            if (view is android.view.ViewGroup) {
                for (i in 0 until view.childCount) {
                    val child = view.getChildAt(i)
                    child.isEnabled = enabled
                    child.alpha = if (enabled) 1.0f else 0.5f
                    if (child is android.view.ViewGroup) {
                        applyEnabledStateRecursive(child)
                    }
                }
            }
        }

        private fun applyEnabledStateRecursive(group: android.view.ViewGroup) {
            for (i in 0 until group.childCount) {
                val child = group.getChildAt(i)
                child.isEnabled = enabled
                child.alpha = if (enabled) 1.0f else 0.5f
                if (child is android.view.ViewGroup) {
                    applyEnabledStateRecursive(child)
                }
            }
        }

        override fun update() {
            // Use post{} for UI thread safety
            widgetView?.post {
                applyEnabledState()
            }
        }

        /**
         * Cleanup widget resources. Override in subclasses if needed.
         */
        open fun cleanup() {
            widgetView?.let { view ->
                view.alpha = 1.0f
                view.isEnabled = true
            }
            widgetView = null
        }
    }
