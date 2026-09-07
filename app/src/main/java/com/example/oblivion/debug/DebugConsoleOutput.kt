package com.example.oblivion.debug

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.TextView
import java.io.BufferedReader
import java.io.InputStreamReader
import kotlin.concurrent.thread

/**
 * Real-time logcat reader and console output display for debugging.
 * Displays Android system logs with color-coding and filtering.
 */
class DebugConsoleOutput(
    private val context: Context,
    private val consoleOutputProvider: (() -> String?)? = null
) {
    private companion object {
        const val TAG = "DebugConsoleOutput"
        const val MAX_LINES = 1000
    }

    // UI Components
    private lateinit var logTextView: TextView
    private lateinit var levelSpinner: Spinner
    private lateinit var autoScrollCheckBox: CheckBox
    private lateinit var clearButton: Button
    private lateinit var scrollView: ScrollView

    // Logcat reading state
    @Volatile
    private var isReading = false

    @Volatile
    private var isAttached = false

    private var logcatProcess: Process? = null
    private var readerThread: Thread? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    // Log level filter
    private var selectedLogLevel = "ALL"
    private val logLevelMap = mapOf(
        "VERBOSE" to 2,
        "DEBUG" to 3,
        "INFO" to 4,
        "WARN" to 5,
        "ERROR" to 6
    )

    // Color mappings for log levels
    private val levelColorMap = mapOf(
        'V' to 0xFF888888.toInt(),  // gray for VERBOSE
        'D' to Color.WHITE,          // white for DEBUG
        'I' to 0xFF00FF00.toInt(),  // green for INFO
        'W' to 0xFFFFFF00.toInt(),  // yellow for WARN
        'E' to 0xFFFF0000.toInt()   // red for ERROR
    )

    /**
     * Build the debug console UI.
     */
    fun build(): View {
        return LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.BLACK)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )

            // Title
            addView(
                TextView(context).apply {
                    text = "Console Output"
                    setTextColor(0xFFFF8844.toInt())
                    textSize = 16f
                    setTypeface(null, android.graphics.Typeface.BOLD)
                    setPadding(
                        DebugButtonWidget.dpToPx(context, 8),
                        DebugButtonWidget.dpToPx(context, 8),
                        DebugButtonWidget.dpToPx(context, 8),
                        DebugButtonWidget.dpToPx(context, 4)
                    )
                },
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
            )

            // Control bar
            addView(
                LinearLayout(context).apply {
                    orientation = LinearLayout.HORIZONTAL
                    setBackgroundColor(0xFF222222.toInt())
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    )
                    setPadding(
                        DebugButtonWidget.dpToPx(context, 4),
                        DebugButtonWidget.dpToPx(context, 4),
                        DebugButtonWidget.dpToPx(context, 4),
                        DebugButtonWidget.dpToPx(context, 4)
                    )

                    // Log Level Filter Spinner
                    levelSpinner = Spinner(context).apply {
                        val levels = arrayOf("ALL", "DEBUG", "INFO", "WARN", "ERROR")
                        adapter = ArrayAdapter(
                            context,
                            android.R.layout.simple_spinner_item,
                            levels
                        ).apply {
                            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
                        }
                        onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                            override fun onItemSelected(
                                parent: AdapterView<*>?,
                                view: View?,
                                position: Int,
                                id: Long
                            ) {
                                selectedLogLevel = parent?.getItemAtPosition(position).toString()
                            }

                            override fun onNothingSelected(parent: AdapterView<*>?) {}
                        }
                        layoutParams = LinearLayout.LayoutParams(
                            0,
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                            1f
                        )
                    }
                    addView(levelSpinner)

                    // Auto-scroll checkbox
                    autoScrollCheckBox = CheckBox(context).apply {
                        text = "Auto-scroll"
                        setTextColor(Color.WHITE)
                        isChecked = true
                        layoutParams = LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT
                        ).apply {
                            marginStart = DebugButtonWidget.dpToPx(context, 8)
                        }
                    }
                    addView(autoScrollCheckBox)

                    // Clear button
                    clearButton = Button(context).apply {
                        text = "Clear"
                        setTextColor(Color.WHITE)
                        setBackgroundColor(0xFF444444.toInt())
                        layoutParams = LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.WRAP_CONTENT,
                            LinearLayout.LayoutParams.WRAP_CONTENT
                        ).apply {
                            marginStart = DebugButtonWidget.dpToPx(context, 8)
                        }
                        setOnClickListener {
                            logTextView.text = ""
                        }
                    }
                    addView(clearButton)
                }
            )

            // Scrollable log view
            scrollView = ScrollView(context).apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.MATCH_PARENT
                )
                setBackgroundColor(Color.BLACK)

                logTextView = TextView(context).apply {
                    setTextColor(Color.WHITE)
                    textSize = 10f
                    typeface = Typeface.MONOSPACE
                    layoutParams = FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT
                    )
                    isVerticalScrollBarEnabled = true
                    isClickable = true
                }
                addView(logTextView)
            }
            addView(scrollView)

            isAttached = true
        }
    }

    /**
     * Start reading logcat in a background thread.
     */
    fun startReading() {
        if (isReading) return

        isReading = true
        readerThread = thread(isDaemon = true, name = "DebugConsoleOutput-Reader") {
            try {
                val process = Runtime.getRuntime().exec("logcat -v threadtime")
                logcatProcess = process

                val reader = BufferedReader(InputStreamReader(process.inputStream))
                var line: String? = null

                while (isReading && reader.readLine().also { line = it } != null) {
                    line?.let { logLine ->
                        if (shouldDisplayLine(logLine)) {
                            val displayText = formatLogLine(logLine)
                            if (isAttached) {
                                mainHandler.post {
                                    if (isAttached) {
                                        appendLogLine(displayText)
                                    }
                                }
                            }
                        }
                    }
                }

                reader.close()
            } catch (e: Exception) {
                Log.e(TAG, "Error reading logcat: ${e.message}", e)
            }
        }
    }

    /**
     * Stop reading logcat.
     */
    fun stopReading() {
        isReading = false
        readerThread?.interrupt()
    }

    /**
     * Clean up resources.
     */
    fun cleanup() {
        stopReading()
        try {
            logcatProcess?.destroy()
        } catch (e: Exception) {
            Log.e(TAG, "Error destroying logcat process: ${e.message}")
        }
        isAttached = false
    }

    /**
     * Determine if a log line should be displayed based on filter.
     */
    private fun shouldDisplayLine(line: String): Boolean {
        if (selectedLogLevel == "ALL") return true

        // Extract log level from line
        // Format: MM-DD HH:MM:SS.mmm  PID  TID L TAG: message
        val parts = line.split(Regex("\\s+"))
        if (parts.size < 6) return false

        val levelChar = parts[5].firstOrNull() ?: return false
        val levelName = when (levelChar) {
            'V' -> "VERBOSE"
            'D' -> "DEBUG"
            'I' -> "INFO"
            'W' -> "WARN"
            'E' -> "ERROR"
            else -> return false
        }

        val selectedLevelValue = logLevelMap[selectedLogLevel] ?: return true
        val lineLevelValue = logLevelMap[levelName] ?: return false

        return lineLevelValue >= selectedLevelValue
    }

    /**
     * Format and colorize a log line.
     */
    private fun formatLogLine(line: String): Pair<String, Int> {
        // Parse logcat line: MM-DD HH:MM:SS.mmm  PID  TID L TAG: message
        val parts = line.split(Regex("\\s+"), limit = 6)

        if (parts.size < 6) {
            return Pair(line, Color.WHITE)
        }

        val levelChar = parts[5].firstOrNull() ?: 'V'
        val color = levelColorMap[levelChar] ?: Color.WHITE

        return Pair(line, color)
    }

    /**
     * Append a formatted log line to the TextView.
     */
    private fun appendLogLine(logEntry: Pair<String, Int>) {
        val (text, color) = logEntry

        if (!isAttached) return

        val currentText = logTextView.text.toString()
        val lines = if (currentText.isEmpty()) {
            listOf(text)
        } else {
            val existingLines = currentText.split("\n").toMutableList()
            existingLines.add(text)
            if (existingLines.size > MAX_LINES) {
                existingLines.drop(existingLines.size - MAX_LINES)
            } else {
                existingLines
            }
        }

        val newText = lines.joinToString("\n")
        logTextView.text = newText
        logTextView.setTextColor(color)

        // Auto-scroll to bottom if enabled
        if (autoScrollCheckBox.isChecked && scrollView.isAttachedToWindow) {
            scrollView.post {
                scrollView.fullScroll(ScrollView.FOCUS_DOWN)
            }
        }
    }

    /**
     * Refresh console output from native code.
     */
    fun refreshConsoleOutput() {
        val output = consoleOutputProvider?.invoke()
        if (output != null && isAttached) {
            val displayText = Pair("=== Native Console Output ===\n$output\n", 0xFF00FFFF.toInt())
            mainHandler.post {
                if (isAttached) {
                    appendLogLine(displayText)
                }
            }
        }
    }
}
