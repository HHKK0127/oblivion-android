package com.example.oblivion.debug

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.TypedValue
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

/**
 * 3D model viewer for NIF files.
 * Displays loaded NIF models with stats and selection.
 */
class DebugModelViewer(
    private val context: Context,
    private val modelProvider: (() -> List<ModelInfo>)? = null
) {
    data class ModelInfo(
        val path: String,
        val name: String,
        val vertexCount: Int = 0,
        val triangleCount: Int = 0,
        val textureCount: Int = 0,
        val materialCount: Int = 0,
        val boneCount: Int = 0,
        val fileSizeKB: Long = 0,
        val isVisible: Boolean = false,
        val distance: Float = 0f
    )

    private var container: LinearLayout? = null
    private var modelListView: LinearLayout? = null
    private var selectedModel: ModelInfo? = null
    private var modelList: List<ModelInfo> = emptyList()

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
            text = "3D Model Viewer"
            setTextColor(Color.parseColor("#FF8844"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            setTypeface(null, android.graphics.Typeface.BOLD)
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(title)

        // Stats summary
        val statsLabel = TextView(context).apply {
            text = "Models: 0 | Vertices: 0 | Triangles: 0"
            setTextColor(Color.parseColor("#AAAAAA"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            setPadding(dp8, dp4, dp8, dp4)
            tag = "statsLabel"
        }
        container?.addView(statsLabel)

        // Model list
        modelListView = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
        }
        container?.addView(modelListView)

        // Refresh button
        val refreshBtn = Button(context).apply {
            text = "Refresh Models"
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#4D6680"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(dp8, dp4, dp8, dp4)
            isAllCaps = false
            setOnClickListener { refresh() }
        }
        container?.addView(refreshBtn)

        scroll.addView(container)
        refresh()
        return scroll
    }

    private fun refresh() {
        modelList = modelProvider?.invoke() ?: emptyList()
        val dp4 = DebugButtonWidget.dpToPx(context, 4)
        val dp2 = DebugButtonWidget.dpToPx(context, 2)

        // Update stats
        val totalVerts = modelList.sumOf { it.vertexCount }
        val totalTris = modelList.sumOf { it.triangleCount }
        val statsLabel = container?.findViewWithTag<TextView>("statsLabel")
        statsLabel?.text = "Models: ${modelList.size} | Vertices: $totalVerts | Triangles: $totalTris"

        // Rebuild list
        modelListView?.removeAllViews()

        modelList.forEach { model ->
            val row = LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                setBackgroundColor(Color.parseColor("#2A2A2A"))
                setPadding(dp4, dp2, dp4, dp2)
                val params = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = dp2
                }
                layoutParams = params
            }

            // Model name
            val nameText = TextView(context).apply {
                text = model.name
                setTextColor(if (model.isVisible) Color.parseColor("#4DFF4D") else Color.WHITE)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
                setTypeface(null, android.graphics.Typeface.BOLD)
            }
            row.addView(nameText)

            // Stats chips
            val chipsRow = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
            }
            val chips = listOf(
                "V:${model.vertexCount}",
                "T:${model.triangleCount}",
                "Tex:${model.textureCount}",
                "Mat:${model.materialCount}",
                if (model.boneCount > 0) "Bones:${model.boneCount}" else null,
                "${model.fileSizeKB}KB"
            ).filterNotNull()

            chips.forEach { chip ->
                val chipView = TextView(context).apply {
                    text = chip
                    setTextColor(Color.WHITE)
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
                    setBackgroundColor(Color.parseColor("#444444"))
                    setPadding(dp4, dp2, dp4, dp2)
                    val params = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    ).apply {
                        marginEnd = dp2
                    }
                    layoutParams = params
                }
                chipsRow.addView(chipView)
            }
            row.addView(chipsRow)

            // Path
            val pathText = TextView(context).apply {
                text = model.path
                setTextColor(Color.GRAY)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 9f)
            }
            row.addView(pathText)

            // Distance if visible
            if (model.isVisible && model.distance > 0) {
                val distText = TextView(context).apply {
                    text = String.format("Distance: %.1f", model.distance)
                    setTextColor(Color.parseColor("#88AAFF"))
                    setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
                }
                row.addView(distText)
            }

            modelListView?.addView(row)
        }
    }
}
