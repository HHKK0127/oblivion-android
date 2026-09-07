package com.example.oblivion.debug

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.io.File
import java.io.IOException

/**
 * Data class representing a single crash event in the crash history.
 *
 * @param timestamp Milliseconds since epoch when the crash occurred
 * @param threadName Name of the thread where the crash occurred
 * @param exceptionType Simple name of the exception class (e.g., "NullPointerException")
 * @param message Exception message, or empty string if none
 * @param stackTrace Full stack trace of the exception
 * @param deviceInfo Device identifier: "MANUFACTURER MODEL RELEASE"
 */
data class CrashEntry(
    val timestamp: Long,
    val threadName: String,
    val exceptionType: String,
    val message: String,
    val stackTrace: String,
    val deviceInfo: String
) {
    /**
     * Converts this CrashEntry to a JSON object.
     */
    fun toJson(): JSONObject {
        return JSONObject().apply {
            put("timestamp", timestamp)
            put("threadName", threadName)
            put("exceptionType", exceptionType)
            put("message", message)
            put("stackTrace", stackTrace)
            put("deviceInfo", deviceInfo)
        }
    }

    companion object {
        /**
         * Attempts to deserialize a CrashEntry from a JSONObject.
         * Returns null if deserialization fails.
         */
        fun fromJson(json: JSONObject): CrashEntry? {
            return try {
                CrashEntry(
                    timestamp = json.getLong("timestamp"),
                    threadName = json.getString("threadName"),
                    exceptionType = json.getString("exceptionType"),
                    message = json.getString("message"),
                    stackTrace = json.getString("stackTrace"),
                    deviceInfo = json.getString("deviceInfo")
                )
            } catch (e: Exception) {
                null
            }
        }
    }
}

/**
 * Singleton crash handler for capturing uncaught exceptions and storing them
 * in a persistent line-delimited JSON file.
 *
 * Usage:
 *   In your Application class onCreate():
 *   DebugCrashHandler.init(this)
 *
 *   Later, retrieve crash history:
 *   val crashes = DebugCrashHandler.getCrashHistory()
 *
 * Thread safety: All file operations are synchronized to prevent concurrent writes.
 */
object DebugCrashHandler {
    private var context: Context? = null
    private val lock = Any()
    private var previousHandler: Thread.UncaughtExceptionHandler? = null

    /**
     * Installs the uncaught exception handler.
     * Must be called once during app initialization, typically in Application.onCreate().
     *
     * @param context The application context (converted to applicationContext internally)
     */
    fun init(context: Context) {
        synchronized(lock) {
            this.context = context.applicationContext
            previousHandler = Thread.getDefaultUncaughtExceptionHandler()
            Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
                captureException(thread, throwable)
                // Chain to the previous handler so the app still crashes normally
                previousHandler?.uncaughtException(thread, throwable)
            }
        }
    }

    /**
     * Internal method to capture an exception and write it to the crash history file.
     * Runs synchronously and silently handles any IO errors.
     */
    private fun captureException(thread: Thread, throwable: Throwable) {
        synchronized(lock) {
            context?.let { ctx ->
                try {
                    val entry = CrashEntry(
                        timestamp = System.currentTimeMillis(),
                        threadName = thread.name,
                        exceptionType = throwable::class.java.simpleName,
                        message = throwable.message ?: "",
                        stackTrace = getStackTraceString(throwable),
                        deviceInfo = "${Build.MANUFACTURER} ${Build.MODEL} ${Build.VERSION.RELEASE}"
                    )
                    writeCrashEntry(ctx, entry)
                } catch (e: Exception) {
                    // Silent fail - do not throw or log (we're already in a crash handler)
                }
            }
        }
    }

    /**
     * Converts a Throwable to its full stack trace string.
     */
    private fun getStackTraceString(throwable: Throwable): String {
        return throwable.stackTraceToString()
    }

    /**
     * Writes a single crash entry to the crash history file.
     * File is line-delimited JSON for easy streaming reads.
     */
    private fun writeCrashEntry(context: Context, entry: CrashEntry) {
        try {
            val file = getHistoryFile(context)
            val jsonString = entry.toJson().toString()
            file.appendText(jsonString + "\n")
        } catch (e: IOException) {
            // Silent fail - do not propagate IO errors from the crash handler
        }
    }

    /**
     * Retrieves all crash entries from the history file.
     *
     * @return List of CrashEntry objects, or empty list if file doesn't exist or cannot be read.
     *         Malformed lines are silently skipped.
     */
    fun getCrashHistory(): List<CrashEntry> {
        synchronized(lock) {
            return context?.let { ctx ->
                try {
                    val file = getHistoryFile(ctx)
                    if (!file.exists()) {
                        return@let emptyList()
                    }
                    val lines = file.readLines()
                    lines.mapNotNull { line ->
                        try {
                            CrashEntry.fromJson(JSONObject(line))
                        } catch (e: Exception) {
                            // Skip malformed lines
                            null
                        }
                    }
                } catch (e: IOException) {
                    emptyList()
                }
            } ?: emptyList()
        }
    }

    /**
     * Clears the crash history by deleting the history file.
     * Thread-safe with synchronized block.
     */
    fun clearHistory() {
        synchronized(lock) {
            context?.let { ctx ->
                try {
                    val file = getHistoryFile(ctx)
                    if (file.exists()) {
                        file.delete()
                    }
                } catch (e: Exception) {
                    // Silent fail - do not propagate errors
                }
            }
        }
    }

    /**
     * Returns the File object for the crash history file.
     * Can be used to export or share crash data.
     *
     * @return The crash history File, or null if init() has not been called
     */
    fun getHistoryFile(): File? {
        return context?.let { getHistoryFile(it) }
    }

    /**
     * Internal helper to construct the crash history file path.
     */
    private fun getHistoryFile(context: Context): File {
        return File(context.filesDir, "crash_history.json")
    }
}
