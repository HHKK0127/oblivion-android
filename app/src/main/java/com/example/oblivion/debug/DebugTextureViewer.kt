package com.example.oblivion.debug

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.util.TypedValue
import android.view.View
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import kotlin.math.min

/**
 * Texture preview viewer.
 * Displays loaded textures in a grid with filtering support.
 */
class DebugTextureViewer(
    private val context: Context,
    private val textureProvider: (() -> List<TextureInfo>)? = null
) {
    data class TextureInfo(
        val id: String,
        val name: String,
        val width: Int,
        val height: Int,
        val format: String = "RGBA8",
        val preview: Bitmap? = null
    )

    private var container: LinearLayout? = null
    private var canvasView: TextureGridCanvas? = null
    private var filterText: String = ""
    private var textureList: List<TextureInfo> = emptyList()
    private val handler = Handler(Looper.getMainLooper())

    fun build(): LinearLayout {
        container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#FF1A1A1A"))
        }

        val dp8 = DebugButtonWidget.dpToPx(context, 8)
        val dp4 = DebugButtonWidget.dpToPx(context, 4)

        // Title
        val title = TextView(context).apply {
            text = "Texture Viewer"
            setTextColor(Color.parseColor("#FF8844"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            setTypeface(null, android.graphics.Typeface.BOLD)
            setPadding(dp8, dp4, dp8, dp4)
        }
        container?.addView(title)

        // Filter input
        val filterInput = EditText(context).apply {
            hint = "Filter textures..."
            setTextColor(Color.WHITE)
            setHintTextColor(Color.GRAY)
            setBackgroundColor(Color.parseColor("#333333"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(dp8, dp4, dp8, dp4)

            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    filterText = s?.toString() ?: ""
                    canvasView?.setFilter(filterText)
                }
            })
        }
        container?.addView(filterInput)

        // Texture count
        val countLabel = TextView(context).apply {
            text = "Loaded: 0 textures"
            setTextColor(Color.parseColor("#AAAAAA"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 11f)
            setPadding(dp8, dp4, dp8, dp4)
            tag = "countLabel"
        }
        container?.addView(countLabel)

        // Texture grid canvas
        canvasView = TextureGridCanvas(context).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                DebugButtonWidget.dpToPx(context, 300)
            )
        }
        container?.addView(canvasView)

        // Refresh on tap
        val refreshBtn = android.widget.Button(context).apply {
            text = "Refresh Textures"
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#4D6680"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setPadding(dp8, dp4, dp8, dp4)
            isAllCaps = false
            setOnClickListener { refresh() }
        }
        container?.addView(refreshBtn)

        refresh()
        return container!!
    }

    fun refresh() {
        textureList = textureProvider?.invoke() ?: emptyList()
        canvasView?.setTextures(textureList)

        // Update count label
        val countLabel = container?.findViewWithTag<TextView>("countLabel")
        countLabel?.text = "Loaded: ${textureList.size} textures (showing ${canvasView?.getFilteredCount() ?: 0})"
    }

    /**
     * Canvas that draws texture previews in a grid.
     */
    private class TextureGridCanvas(context: Context) : View(context) {
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.WHITE
            textSize = 20f
        }
        private var textures: List<TextureInfo> = emptyList()
        private var filteredTextures: List<TextureInfo> = emptyList()
        private var filter: String = ""
        private val cellSize = 80
        private val padding = 4

        fun setTextures(newTextures: List<TextureInfo>) {
                    // Move oldTextures reference before assignment to recycle properly
                    val oldTextures = textures
                    textures = newTextures
                    // Recycle old bitmaps after the new reference is in place
                    oldTextures.forEach { tex ->
                        if (tex.preview != null && !tex.preview.isRecycled) {
                            tex.preview.recycle()
                        }
                    }
                    applyFilter()
                    invalidate()
                }

        fun setFilter(newFilter: String) {
            filter = newFilter
            applyFilter()
            invalidate()
        }

        fun getFilteredCount(): Int = filteredTextures.size

        /**
         * Cleanup method to recycle Bitmap resources.
         */
        fun cleanup() {
            textures.forEach { tex ->
                if (tex.preview != null && !tex.preview.isRecycled) {
                    tex.preview.recycle()
                }
            }
            textures = emptyList()
            filteredTextures = emptyList()
        }

        private fun applyFilter() {
            filteredTextures = if (filter.isEmpty()) {
                textures
            } else {
                textures.filter {
                    it.name.contains(filter, ignoreCase = true) ||
                    it.id.contains(filter, ignoreCase = true)
                }
            }
        }

        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)
            canvas.drawColor(Color.parseColor("#222222"))

            if (filteredTextures.isEmpty()) {
                textPaint.textSize = 24f
                canvas.drawText("No textures loaded", 20f, 40f, textPaint)
                return
            }

            val cols = min(8, (width - padding) / (cellSize + padding))
            var x = padding
            var y = padding

            filteredTextures.forEachIndexed { index, tex ->
                if (index > 0 && index % cols == 0) {
                    x = padding
                    y += cellSize + padding
                }

                // Draw texture preview
                val rect = RectF(x.toFloat(), y.toFloat(), (x + cellSize).toFloat(), (y + cellSize).toFloat())

                if (tex.preview != null) {
                    canvas.drawBitmap(tex.preview, null, rect, paint)
                } else {
                    paint.color = Color.parseColor("#444444")
                    canvas.drawRect(rect, paint)

                    // Draw size text
                    textPaint.textSize = 14f
                    canvas.drawText("${tex.width}x${tex.height}", x + 4f, y + 40f, textPaint)
                    textPaint.textSize = 10f
                    canvas.drawText(tex.format, x + 4f, y + 55f, textPaint)
                }

                // Border
                paint.color = Color.parseColor("#666666")
                paint.style = Paint.Style.STROKE
                canvas.drawRect(rect, paint)
                paint.style = Paint.Style.FILL

                // Name below
                textPaint.textSize = 10f
                val shortName = if (tex.name.length > 10) tex.name.substring(0, 10) + ".." else tex.name
                canvas.drawText(shortName, x.toFloat(), (y + cellSize + 12).toFloat(), textPaint)

                x += cellSize + padding
            }
        }
    }
}
