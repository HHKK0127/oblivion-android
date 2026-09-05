package com.example.oblivion

import android.content.Context
import android.util.Log
import java.io.*

/**
 * Asset extraction system for Oblivion Android.
 * Moves large game assets from APK to app-specific external storage (no permission needed).
 */
class AssetExtractor(private val context: Context) {
    
    companion object {
        private const val TAG = "AssetExtractor"
        private const val ASSET_DIR = "oblivion_assets"
        private const val VERSION_FILE = "version.txt"
        private const val CURRENT_VERSION = 1
    }
    
    private val externalDir: File
        get() = File(
            context.getExternalFilesDir(null),
            ASSET_DIR
        )
    
    /**
     * Check if assets need extraction.
     */
    fun needsExtraction(): Boolean {
        val versionFile = File(externalDir, VERSION_FILE)
        if (!versionFile.exists()) return true
        
        return try {
            val version = versionFile.readText().trim().toInt()
            version < CURRENT_VERSION
        } catch (e: Exception) {
            true
        }
    }
    
    /**
     * Extract assets to external storage.
     * @param progressCallback Called with (current, total) progress
     */
    fun extractAssets(progressCallback: (Int, Int) -> Unit): Boolean {
        return try {
            // Create directory
            externalDir.mkdirs()
            
            // Get list of assets to extract
            val assets = listAssets("")
            val total = assets.size
            
            Log.i(TAG, "Extracting $total assets to ${externalDir.absolutePath}")
            
            // Extract each asset
            assets.forEachIndexed { index, assetPath ->
                extractAsset(assetPath)
                progressCallback(index + 1, total)
            }
            
            // Write version file
            File(externalDir, VERSION_FILE).writeText(CURRENT_VERSION.toString())
            
            Log.i(TAG, "Asset extraction complete")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Asset extraction failed", e)
            false
        }
    }
    
    /**
     * Get the path to an asset, preferring external storage.
     */
    fun getAssetPath(assetPath: String): String {
        val externalFile = File(externalDir, assetPath)
        return if (externalFile.exists()) {
            externalFile.absolutePath
        } else {
            // Fall back to APK assets
            "assets/$assetPath"
        }
    }
    
    /**
     * Check if an asset exists in external storage.
     */
    fun hasExternalAsset(assetPath: String): Boolean {
        return File(externalDir, assetPath).exists()
    }
    
    /**
     * List all assets in the APK.
     */
    private fun listAssets(path: String): List<String> {
        val result = mutableListOf<String>()
        val assets = context.assets.list(path) ?: return emptyList()
        
        for (asset in assets) {
            val fullPath = if (path.isEmpty()) asset else "$path/$asset"
            val subAssets = context.assets.list(fullPath)
            
            if (subAssets == null || subAssets.isEmpty()) {
                // It's a file
                result.add(fullPath)
            } else {
                // It's a directory
                result.addAll(listAssets(fullPath))
            }
        }
        
        return result
    }
    
    /**
     * Extract a single asset to external storage.
     */
    private fun extractAsset(assetPath: String) {
        val outputFile = File(externalDir, assetPath)
        
        // Create parent directories
        outputFile.parentFile?.mkdirs()
        
        // Copy asset
        context.assets.open(assetPath).use { input ->
            FileOutputStream(outputFile).use { output ->
                input.copyTo(output)
            }
        }
    }
    
    /**
     * Get total size of assets to extract.
     */
    fun getAssetSize(): Long {
        var totalSize = 0L
        
        listAssets("").forEach { assetPath ->
            try {
                context.assets.open(assetPath).use { input ->
                    totalSize += input.available()
                }
            } catch (e: Exception) {
                // Skip errors
            }
        }
        
        return totalSize
    }
    
    /**
     * Clean up extracted assets.
     */
    fun cleanup() {
        externalDir.deleteRecursively()
        Log.i(TAG, "Cleaned up extracted assets")
    }
}
