package com.example.oblivion.debug

import android.content.ClipData
import android.content.ClipDescription
import android.os.Build
import android.util.Log
import android.view.DragEvent
import android.view.View
import android.view.ViewGroup
import android.widget.GridLayout

/**
 * Drag and drop reordering of widgets in a GridLayout.
 * Supports API 24+ for startDragAndDrop with fallback to startDrag on older APIs.
 */
class DebugDragDrop(private val gridLayout: GridLayout) {

    companion object {
        private const val TAG = "DebugDragDrop"
        private val MIME_TYPE = ClipDescription.MIMETYPE_TEXT_PLAIN
    }

    // Track dragging state
    var isDragging: Boolean = false
        private set

    // Current widget order (index -> original widget index)
    private val widgetOrder = mutableListOf<Int>()

    // Callback for order changes
    private var onOrderChanged: ((List<Int>) -> Unit)? = null

    // Drag shadow builder
    private inner class DebugDragShadowBuilder(view: View) : View.DragShadowBuilder(view) {
        override fun onProvideShadowMetrics(outShadowSize: android.graphics.Point,
                                            outShadowTouchPoint: android.graphics.Point) {
            val view = getView()
            if (view != null) {
                outShadowSize.set(view.width / 2, view.height / 2)
                outShadowTouchPoint.set(view.width / 4, view.height / 4)
            } else {
                outShadowSize.set(0, 0)
                outShadowTouchPoint.set(0, 0)
            }
        }
    }

    /**
     * Initialize drag and drop for the given grid layout.
     * @param initialOrder Optional initial widget order. If null, uses sequential order.
     */
    fun initialize(initialOrder: List<Int>? = null) {
        val childCount = gridLayout.childCount
        widgetOrder.clear()

        if (initialOrder != null && initialOrder.size == childCount) {
            widgetOrder.addAll(initialOrder)
        } else {
            for (i in 0 until childCount) {
                widgetOrder.add(i)
            }
        }

        applyOrder()
        setupDragListeners()
        Log.d(TAG, "Initialized with ${widgetOrder.size} widgets, order: $widgetOrder")
    }

    /**
     * Set up drag listeners on each child view.
     */
    private fun setupDragListeners() {
        for (i in 0 until gridLayout.childCount) {
            val child = gridLayout.getChildAt(i)
            if (child == null) continue

            val longClickListener = View.OnLongClickListener { view ->
                startDrag(view)
                true
            }

            // Save any existing long-click listener before overwriting
            if (child.hasOnLongClickListeners()) {
                viewListeners[i] = null // caller's listener is replaced
            }
            viewListeners[i] = longClickListener
            child.setOnLongClickListener(longClickListener)

            val dragListener = View.OnDragListener { view, event ->
                handleDragEvent(view, event)
            }
            dragListeners[i] = dragListener
            child.setOnDragListener(dragListener)
        }
    }

    /**
     * Start a drag operation on the given view.
     */
    private fun startDrag(view: View) {
        val index = gridLayout.indexOfChild(view)
        if (index < 0) return

        val clipData = ClipData.newPlainText("widget_index", index.toString())
        val shadow = DebugDragShadowBuilder(view)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // API 24+: use startDragAndDrop
            view.startDragAndDrop(clipData, shadow, index, View.DRAG_FLAG_OPAQUE)
        } else {
            @Suppress("DEPRECATION")
            view.startDrag(clipData, shadow, index, 0)
        }

        isDragging = true
        view.alpha = 0.5f // Dim the dragged view
        Log.d(TAG, "Started drag on widget index $index")
    }

    /**
     * Handle drag events on target views.
     */
    private fun handleDragEvent(targetView: View, event: DragEvent): Boolean {
        when (event.action) {
            DragEvent.ACTION_DRAG_STARTED -> {
                return event.clipDescription.hasMimeType(MIME_TYPE)
            }

            DragEvent.ACTION_DRAG_ENTERED -> {
                targetView.alpha = 0.7f
                return true
            }

            DragEvent.ACTION_DRAG_EXITED -> {
                targetView.alpha = 1.0f
                return true
            }

            DragEvent.ACTION_DRAG_LOCATION -> {
                return true
            }

            DragEvent.ACTION_DROP -> {
                val fromIndex = event.localState as? Int ?: return false
                val toIndex = gridLayout.indexOfChild(targetView)

                if (fromIndex != toIndex && fromIndex >= 0 && toIndex >= 0) {
                    reorderWidgets(fromIndex, toIndex)
                    Log.d(TAG, "Dropped widget from $fromIndex to $toIndex")
                }

                targetView.alpha = 1.0f
                return true
            }

            DragEvent.ACTION_DRAG_ENDED -> {
                endDrag()
                return true
            }

            else -> return false
        }
    }

    /**
     * Reorder widgets from one position to another.
     */
    private fun reorderWidgets(fromIndex: Int, toIndex: Int) {
        if (!validateOrder()) {
            Log.w(TAG, "Widget order validation failed, resetting")
            resetOrder()
            return
        }

        // Update the order list
        val fromOrder = widgetOrder[fromIndex]
        widgetOrder.removeAt(fromIndex)
        widgetOrder.add(toIndex, fromOrder)

        applyOrder()
        onOrderChanged?.invoke(widgetOrder.toList())
        Log.d(TAG, "New widget order: $widgetOrder")
    }

    /**
     * Preserve tag and listener state before applyOrder clears and re-adds views.
     */
    private data class SavedViewState(
        val tag: Any?,
        val longClickListener: View.OnLongClickListener?,
        val dragListener: View.OnDragListener?,
        val isClickable: Boolean,
        val isLongClickable: Boolean,
        val alpha: Float
    )

    // Track original listeners per child index to avoid reflection
    private val viewListeners = mutableMapOf<Int, View.OnLongClickListener?>()
    private val dragListeners = mutableMapOf<Int, View.OnDragListener?>()

    private fun saveViewState(view: View): SavedViewState {
        val idx = gridLayout.indexOfChild(view)
        return SavedViewState(
            tag = view.tag,
            longClickListener = viewListeners[idx],
            dragListener = dragListeners[idx],
            isClickable = view.isClickable,
            isLongClickable = view.isLongClickable,
            alpha = view.alpha
        )
    }

    private fun restoreViewState(view: View, state: SavedViewState) {
        view.tag = state.tag
        state.longClickListener?.let { view.setOnLongClickListener(it) }
        state.dragListener?.let { view.setOnDragListener(it) }
        view.isClickable = state.isClickable
        view.isLongClickable = state.isLongClickable
        view.alpha = state.alpha
    }

    private fun applyOrder() {
        // Save state for each child before removal
        val savedStates = mutableListOf<SavedViewState>()
        val children = mutableListOf<View>()
        for (i in 0 until gridLayout.childCount) {
            gridLayout.getChildAt(i)?.let { view ->
                savedStates.add(saveViewState(view))
                children.add(view)
            }
        }

        // Re-add children in the specified order
        gridLayout.removeAllViews()
        for (orderIndex in widgetOrder) {
            if (orderIndex in 0 until children.size) {
                val child = children[orderIndex]
                restoreViewState(child, savedStates[orderIndex])
                gridLayout.addView(child)
            }
        }
    }

    /**
     * Validate that the current widget order is consistent.
     */
    private fun validateOrder(): Boolean {
        val grid = gridLayout
        if (grid == null) {
            Log.w(TAG, "validateOrder: gridLayout is null")
            return false
        }
        val childCount = grid.childCount
        if (widgetOrder.size != childCount) return false

        // Check that all indices are valid
        for (index in widgetOrder) {
            if (index < 0 || index >= childCount) return false
        }

        // Check for duplicates
        if (widgetOrder.toSet().size != widgetOrder.size) return false

        return true
    }

    /**
     * Reset widget order to sequential.
     */
    fun resetOrder() {
        widgetOrder.clear()
        for (i in 0 until gridLayout.childCount) {
            widgetOrder.add(i)
        }
        applyOrder()
        Log.d(TAG, "Widget order reset to sequential")
    }

    /**
     * Clean up drag state. Resets alpha, isEnabled, isClickable, and isLongClickable on all children.
     */
    private fun endDrag() {
        isDragging = false

        // Reset alpha, isEnabled, isClickable, and isLongClickable for all child views
        for (i in 0 until gridLayout.childCount) {
            val child = gridLayout.getChildAt(i) ?: continue
            child.alpha = 1.0f
            child.isEnabled = true
            child.isClickable = true
            child.isLongClickable = true
        }

        Log.d(TAG, "Drag ended, alpha, isEnabled, isClickable, and isLongClickable reset on all widgets")
    }

    /**
     * Set callback for order changes.
     */
    fun setOnOrderChangedListener(listener: (List<Int>) -> Unit) {
        onOrderChanged = listener
    }

    /**
     * Get the current widget order.
     */
    fun getOrder(): List<Int> = widgetOrder.toList()

    /**
     * Set the widget order from a saved state.
     */
    fun setOrder(order: List<Int>) {
        if (order.size != gridLayout.childCount) {
            Log.w(TAG, "Order size ${order.size} doesn't match child count ${gridLayout.childCount}")
            resetOrder()
            return
        }
        widgetOrder.clear()
        widgetOrder.addAll(order)
        applyOrder()
        Log.d(TAG, "Widget order restored: $widgetOrder")
    }

    /**
     * Clean up resources. Call when the menu is destroyed.
     */
    fun cleanup() {
        val grid = gridLayout ?: run {
            Log.w(TAG, "cleanup: gridLayout is null")
            return
        }
        for (i in 0 until grid.childCount) {
            val child = grid.getChildAt(i)
            child?.setOnLongClickListener(null)
            child?.setOnDragListener(null)
            child?.alpha = 1.0f
            child?.isEnabled = true
            child?.isClickable = true
            child?.isLongClickable = true
        }
        widgetOrder.clear()
        viewListeners.clear()
        dragListeners.clear()
        onOrderChanged = null
        isDragging = false
        Log.d(TAG, "Cleanup complete")
    }
}
