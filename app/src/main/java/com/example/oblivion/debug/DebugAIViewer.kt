package com.example.oblivion.debug

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/**
 * AI state viewer for NPCs.
 * Displays NPC AI states, positions, and behaviors.
 */
class DebugAIViewer(
    private val context: Context,
    private val npcStateProvider: (() -> List<NPCState>)? = null
) {
    data class NPCState(
        val name: String,
        val state: String = "IDLE",
        val position: String = "(0, 0, 0)",
        val cell: String = "Unknown",
        val health: Float = 1.0f,
        val hostility: Int = 0,
        val packageType: String = "NONE"
    )

    private var container: LinearLayout? = null
    private var npcListView: LinearLayout? = null
    private var canvasView: AIStateCanvas? = null
    private val handler = Handler(Looper.getMainLooper())
    private var refreshRunnable: Runnable? = null
    private var isUpdating = false
    private var selectedNPC: NPCState? = null

    fun build(): ScrollView {
        val scroll = ScrollView(context)

        container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
        }

        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)

        // Title
        val title = TextView(context).apply {
            text = "NPC AI Viewer"
            setTextColor(Color.parseColor("#FF8844"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            setTypeface(null, android.graphics.Typeface.BOLD)
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(title)

        // State distribution canvas
        canvasView = AIStateCanvas(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                DebugButtonWidget.dpToPx(context, 100)
            )
        }
        container?.addView(canvasView)

        // NPC list
        npcListView = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(npcListView)

        scroll.addView(container)
        refresh()
        return scroll
    }

    private fun refresh() {
        val states = npcStateProvider?.invoke() ?: emptyList()

        // Update canvas
        canvasView?.updateStates(states)

        // Update list
        npcListView?.removeAllViews()
        val dp2 = DebugButtonWidget.dpToPx(context, 2)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)

        states.forEach { npc ->
            val npcRow = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                setPadding(dp4, dp2, dp4, dp2)
            }

            // State indicator
            val stateColor = when (npc.state.uppercase()) {
                "COMBAT" -> Color.RED
                "ALERT" -> Color.YELLOW
                "FLEEING" -> Color.MAGENTA
                "IDLE" -> Color.GREEN
                else -> Color.GRAY
            }

            val indicator = View(context).apply {
                setBackgroundColor(stateColor)
                layoutParams = LinearLayout.LayoutParams(
                    DebugButtonWidget.dpToPx(context, 8),
                    DebugButtonWidget.dpToPx(context, 8)
                ).apply {
                    marginEnd = dp4
                    gravity = android.view.Gravity.CENTER_VERTICAL
                }
            }
            npcRow.addView(indicator)

            // NPC name and state
            val info = TextView(context).apply {
                text = "${npc.name} [${npc.state}] - ${npc.packageType}"
                setTextColor(Color.WHITE)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            }
            npcRow.addView(info)

            npcListView?.addView(npcRow)
        }
    }

    fun startUpdating(intervalMs: Long = 2000) {
        if (isUpdating) return

        // Properly remove existing runnable before setting flag
        refreshRunnable?.let { handler.removeCallbacks(it) }
        refreshRunnable = null
        isUpdating = true

        refreshRunnable = object : Runnable {
            override fun run() {
                if (!isUpdating) return
                refresh()
                handler.postDelayed(this, intervalMs)
            }
        }
        handler.postDelayed(refreshRunnable!!, intervalMs)
    }

    fun stopUpdating() {
        // Set flag first to prevent rescheduling from within a pending callback
        isUpdating = false
        refreshRunnable?.let { handler.removeCallbacks(it) }
        refreshRunnable = null
        // Clear entire message queue to prevent stale callbacks
        handler.removeCallbacksAndMessages(null)
    }

    /**
     * Custom canvas for drawing AI state distribution.
     */
    private class AIStateCanvas(context: Context) : View(context) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 24f
        }
        private var states: List<NPCState> = emptyList()

        fun updateStates(newStates: List<NPCState>) {
            states = newStates
            invalidate()
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)
            canvas.drawColor(Color.parseColor("#222222"))

            if (states.isEmpty()) {
                textPaint.textSize = 28f
                canvas.drawText("No NPC data", 20f, 50f, textPaint)
                return
            }

            // State distribution bar
            val stateCounts = states.groupingBy { it.state }.eachCount()
            val total = states.size.toFloat()
            var x = 0f
            val barHeight = 30f
            val y = 10f

            val colors = mapOf(
                "IDLE" to Color.GREEN,
                "COMBAT" to Color.RED,
                "ALERT" to Color.YELLOW,
                "FLEEING" to Color.MAGENTA,
                "SCRIPTED" to Color.CYAN
            )

            stateCounts.forEach { (state, count) ->
                val width = (count / total) * width
                paint.color = colors[state.uppercase()] ?: Color.GRAY
                canvas.drawRect(x, y, x + width, y + barHeight, paint)

                // Label
                if (width > 50) {
                    textPaint.textSize = 18f
                    canvas.drawText("$state($count)", x + 4, y + barHeight - 6, textPaint)
                }
                x += width
            }

            // Total count
            textPaint.textSize = 22f
            canvas.drawText("Total NPCs: ${states.size}", 20f, y + barHeight + 30f, textPaint)
        }
    }
}
