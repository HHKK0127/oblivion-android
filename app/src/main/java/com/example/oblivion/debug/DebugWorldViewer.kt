package com.example.oblivion.debug

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/**
 * World map viewer with minimap and cell grid navigation.
 * Displays loaded cells, coordinates, and allows cell navigation.
 */
class DebugWorldViewer(
    private val context: Context,
    private val cellProvider: (() -> List<CellInfo>)? = null,
    private val onCellSelected: ((CellInfo) -> Unit)? = null
) {
    data class CellInfo(
        val x: Int,
        val y: Int,
        val name: String = "",
        val isLoaded: Boolean = false,
        val isActive: Boolean = false,
        val hasLOD: Boolean = false,
        val objectCount: Int = 0,
        val type: String = "exterior" // "interior" or "exterior"
    )

    private var container: LinearLayout? = null
    private var minimapView: MinimapCanvas? = null
    private var cellListView: LinearLayout? = null
    private var currentCells: List<CellInfo> = emptyList()
    private var playerCellX: Int = 0
    private var playerCellY: Int = 0
    private val handler = Handler(Looper.getMainLooper())

    fun build(): ScrollView {
        val scroll = ScrollView(context)

        container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#FF1A1A1A"))
        }

        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)

        // Title
        val title = TextView(context).apply {
            text = "World Viewer"
            setTextColor(Color.parseColor("#FF8844"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            setTypeface(null, android.graphics.Typeface.BOLD)
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(title)

        // Player position
        val posLabel = TextView(context).apply {
            text = "Player Cell: (0, 0)"
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(dp8, dp4, dp8, dp4)
            tag = "posLabel"
        }
        container?.addView(posLabel)

        // Minimap canvas
        minimapView = MinimapCanvas(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                DebugButtonWidget.dpToPx(context, 200)
            )
            setBackgroundColor(Color.parseColor("#111111"))
        }
        container?.addView(minimapView)

        // Cell stats
        val statsLabel = TextView(context).apply {
            text = "Loaded: 0 cells | Objects: 0"
            setTextColor(Color.parseColor("#AAAAAA"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            setPadding(dp8, dp4, dp8, dp4)
            tag = "statsLabel"
        }
        container?.addView(statsLabel)

        // Cell list
        cellListView = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(cellListView)

        scroll.addView(container)
        refresh()
        return scroll
    }

    fun updatePlayerPosition(cellX: Int, cellY: Int) {
        playerCellX = cellX
        playerCellY = cellY
        val posLabel = container?.findViewWithTag<TextView>("posLabel")
        posLabel?.text = "Player Cell: ($cellX, $cellY)"
        minimapView?.setPlayerPosition(cellX, cellY)
    }

    fun refresh() {
        currentCells = cellProvider?.invoke() ?: emptyList()
            // Always call updateUI (even for empty list)
            updateUI()
        }

        /**
         * Update the UI based on currentCells. Called by refresh().
         */
        private fun updateUI() {
            val dp4 = DebugButtonWidget.dpToPx(context, 4)
            val dp2 = DebugButtonWidget.dpToPx(context, 2)

            // Update stats
            val totalObjects = currentCells.sumOf { it.objectCount }
            val statsLabel = container?.findViewWithTag<TextView>("statsLabel")
            statsLabel?.text = "Loaded: ${currentCells.size} cells | Objects: $totalObjects"

            // Update minimap
            minimapView?.setCells(currentCells)

            // Update list - handle empty list case
            cellListView?.removeAllViews()
            if (currentCells.isEmpty()) {
                val emptyLabel = TextView(context).apply {
                    text = "No cells loaded"
                    setTextColor(Color.GRAY)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
                    setPadding(dp4, dp2, dp4, dp2)
                }
                cellListView?.addView(emptyLabel)
                return
            }

            currentCells.sortedByDescending { it.isActive }.forEach { cell ->
                val row = LinearLayout(context).apply {
                    orientation = LinearLayout.HORIZONTAL
                    setBackgroundColor(if (cell.isActive) Color.parseColor("#333355") else Color.parseColor("#2A2A2A"))
                    setPadding(dp4, dp2, dp4, dp2)
                    val params = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    ).apply {
                        bottomMargin = dp2
                    }
                    layoutParams = params
                }

                val color = when {
                    cell.isActive -> Color.parseColor("#4DFF4D")
                    cell.isLoaded -> Color.WHITE
                    cell.hasLOD -> Color.GRAY
                    else -> Color.DKGRAY
                }

                val info = TextView(context).apply {
                    text = "(${cell.x}, ${cell.y}) ${cell.name.ifEmpty { cell.type }} [${cell.type}] ${cell.objectCount}obj"
                    setTextColor(color)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
                }
                row.addView(info)

                cellListView?.addView(row)
            }
        }

    /**
     * Minimap canvas that draws cell grid.
     */
    private class MinimapCanvas(context: Context) : View(context) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 16f
        }
        private var cells: List<CellInfo> = emptyList()
        private var playerX = 0
        private var playerY = 0
        private val cellPixels = 20f

        fun setCells(newCells: List<CellInfo>) {
            cells = newCells
            invalidate()
        }

        fun setPlayerPosition(x: Int, y: Int) {
            playerX = x
            playerY = y
            invalidate()
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)
            canvas.drawColor(Color.parseColor("#111111"))

            val centerX = width / 2f
            val centerY = height / 2f

            // Draw grid lines
            paint.color = Color.parseColor("#222222")
            paint.style = Paint.Style.STROKE
            val gridRange = 15
                        // Cast playerX/playerY to Float properly for arithmetic
                        val playerXF: Float = playerX.toFloat()
                        val playerYF: Float = playerY.toFloat()
                        for (i in -gridRange..gridRange) {
                            val iF: Float = i.toFloat()
                            val x = centerX + (iF - playerXF) * cellPixels
                            val y = centerY + (iF - playerYF) * cellPixels
                            canvas.drawLine(x, 0f, x, height.toFloat(), paint)
                            canvas.drawLine(0f, y, width.toFloat(), y, paint)
                        }

                        // Draw cells
                        cells.forEach { cell ->
                            val screenX = centerX + (cell.x.toFloat() - playerXF) * cellPixels
                            val screenY = centerY + (cell.y.toFloat() - playerYF) * cellPixels

                            // Skip cells outside screen bounds
                            if (screenX < -cellPixels || screenX > width + cellPixels ||
                                screenY < -cellPixels || screenY > height + cellPixels) return@forEach

                val rect = RectF(
                    screenX, screenY,
                    screenX + cellPixels, screenY + cellPixels
                )

                paint.style = Paint.Style.FILL
                paint.color = when {
                    cell.isActive -> Color.parseColor("#88FF88")
                    cell.isLoaded -> Color.parseColor("#4488FF")
                    cell.hasLOD -> Color.parseColor("#444444")
                    else -> Color.parseColor("#222222")
                }
                canvas.drawRect(rect, paint)

                paint.color = Color.parseColor("#666666")
                paint.style = Paint.Style.STROKE
                canvas.drawRect(rect, paint)
            }

            // Draw player marker (center)
            paint.style = Paint.Style.FILL
            paint.color = Color.RED
            canvas.drawCircle(centerX + cellPixels / 2, centerY + cellPixels / 2, 5f, paint)

            // Crosshair
            paint.color = Color.parseColor("#66FF0000")
            paint.strokeWidth = 1f
            canvas.drawLine(centerX + cellPixels / 2, 0f, centerX + cellPixels / 2, height.toFloat(), paint)
            canvas.drawLine(0f, centerY + cellPixels / 2, width.toFloat(), centerY + cellPixels / 2, paint)

            // Coordinate label
            textPaint.textSize = 14f
            canvas.drawText("($playerX, $playerY)", 8f, 18f, textPaint)
        }
    }
}
