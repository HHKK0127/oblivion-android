package com.example.oblivion.debug

import android.content.Context
import android.content.Intent
import android.util.Log
import android.widget.Toast
import androidx.core.content.FileProvider
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Utility for exporting debug artifacts (logs, crash history, memory snapshots)
 * to user-accessible files and sharing them via Android's chooser intent.
 */
object DebugExportShare {

    private const val TAG = "DebugExportShare"
    private const val AUTHORITY = "com.example.oblivion.debug.fileprovider"

    private val filenameFmt = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US)

    /**
     * Export logcat to a file in the app's external files directory.
     * Returns the file on success, null on failure.
     */
    fun exportLogcat(context: Context, lines: List<String>): File? {
        return try {
            val dir = getOrCreateExportsDir(context)
            val file = File(dir, "logcat_${filenameFmt.format(Date())}.txt")
            file.bufferedWriter(Charsets.UTF_8).use { writer ->
                for (line in lines) {
                    writer.write(line)
                    writer.newLine()
                }
            }
            file
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export logcat: ${e.message}")
            null
        }
    }

    /**
     * Copy the crash history file to an exportable location.
     */
    fun exportCrashHistory(context: Context): File? {
        return try {
            val source = DebugCrashHandler.getHistoryFile() ?: return null
            if (!source.exists() || source.length() == 0L) return null
            val dir = getOrCreateExportsDir(context)
            val target = File(dir, "crash_history_${filenameFmt.format(Date())}.json")
            source.copyTo(target, overwrite = true)
            target
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export crash history: ${e.message}")
            null
        }
    }

    /**
     * Write a memory snapshot to a file.
     */
    fun exportMemorySnapshot(context: Context, summary: String, history: String): File? {
        return try {
            val dir = getOrCreateExportsDir(context)
            val file = File(dir, "memory_${filenameFmt.format(Date())}.txt")
            file.bufferedWriter(Charsets.UTF_8).use { writer ->
                writer.write("=== Memory Snapshot at ${Date()} ===\n\n")
                writer.write("--- Summary ---\n")
                writer.write(summary)
                writer.write("\n\n--- History ---\n")
                writer.write(history)
            }
            file
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export memory snapshot: ${e.message}")
            null
        }
    }

    /**
     * Write system info to a file.
     */
    fun exportSystemInfo(context: Context, content: String): File? {
        return try {
            val dir = getOrCreateExportsDir(context)
            val file = File(dir, "system_${filenameFmt.format(Date())}.txt")
            file.bufferedWriter(Charsets.UTF_8).use { writer ->
                writer.write("=== System Info at ${Date()} ===\n\n")
                writer.write(content)
            }
            file
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export system info: ${e.message}")
            null
        }
    }

    /**
     * Share a debug file via Android chooser intent.
     * Returns true on success, false on failure.
     */
    fun shareFile(context: Context, file: File, mimeType: String = "text/plain"): Boolean {
        return try {
            val uri = FileProvider.getUriForFile(context, AUTHORITY, file)
            val sendIntent = Intent(Intent.ACTION_SEND).apply {
                type = mimeType
                putExtra(Intent.EXTRA_STREAM, uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            val chooser = Intent.createChooser(sendIntent, "Share debug artifact")
                .apply { addFlags(Intent.FLAG_ACTIVITY_NEW_TASK) }
            context.startActivity(chooser)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to share file: ${e.message}")
            Toast.makeText(context, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
            false
        }
    }

    /**
     * List all previously exported debug files.
     */
    fun listExports(context: Context): List<File> {
        val dir = getOrCreateExportsDir(context)
        return dir.listFiles()?.toList().orEmpty()
    }

    /**
     * Clear all export files.
     */
    fun clearExports(context: Context): Boolean {
        return try {
            val dir = getOrCreateExportsDir(context)
            val files = dir.listFiles()
            if (files != null) {
                for (file in files) {
                    file.delete()
                }
            }
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to clear exports: ${e.message}")
            false
        }
    }

    private fun getOrCreateExportsDir(context: Context): File {
        val base = context.getExternalFilesDir(null) ?: context.filesDir
        val dir = File(base, "debug_exports")
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }
}
