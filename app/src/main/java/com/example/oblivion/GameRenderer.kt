package com.example.oblivion

import android.content.res.AssetManager
import android.opengl.GLSurfaceView
import android.util.Log
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GameRenderer : GLSurfaceView.Renderer {

    private var nativeEngineHandle: Long = 0
    private var gameSurfaceView: GameSurfaceView? = null
    private var frameCount = 0
    private var lastLogTime: Long = 0
    private var onExitRequested: (() -> Unit)? = null

    companion object {
        private const val TAG = "GameRenderer"
        var dataPath: String = ""

        init {
            try {
                System.loadLibrary("native-lib")
                Log.i(TAG, "Native library loaded successfully")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "FATAL: Failed to load native library - ${e.message}", e)
                throw e
            }
        }

        @JvmStatic
        external fun nativeInitAudioBridge(assetManager: AssetManager, mainActivity: Any)

        @JvmStatic
        external fun nativeSetDataPath(path: String)
    }

    constructor()

    constructor(surfaceView: GameSurfaceView) {
        this.gameSurfaceView = surfaceView
        Log.i(TAG, "GameRenderer created with GameSurfaceView reference")
    }

    fun setOnExitRequestedListener(listener: () -> Unit) {
        onExitRequested = listener
    }

    override fun onSurfaceCreated(gl: GL10, config: EGLConfig) {
        android.util.Log.wtf(TAG, "===== onSurfaceCreated CALLED - THIS SHOULD APPEAR IN LOGS =====")
        Log.i(TAG, "=== onSurfaceCreated called ===")

        try {
            val vendor = gl.glGetString(GL10.GL_VENDOR)
            val rendererName = gl.glGetString(GL10.GL_RENDERER)
            val version = gl.glGetString(GL10.GL_VERSION)

            Log.i(TAG, "OpenGL Info - Vendor: $vendor")
            Log.i(TAG, "OpenGL Info - Renderer: $rendererName")
            Log.i(TAG, "OpenGL Info - Version: $version")
        } catch (e: Exception) {
            Log.e(TAG, "Error getting GL info: ${e.message}", e)
        }

        try {
            Log.i(TAG, "Calling nativeInitEngine()")
            nativeEngineHandle = nativeInitEngine()
            Log.i(TAG, "nativeInitEngine returned: handle=$nativeEngineHandle")

            if (nativeEngineHandle == 0L) {
                            Log.e(TAG, "CRITICAL ERROR: nativeInitEngine returned 0 (native initialization failed)")
                        } else {
                            Log.i(TAG, "SUCCESS: Native engine initialized with valid handle")
                            // Set data path AFTER engine is created (g_renderer must exist in native)
                            if (dataPath.isNotEmpty()) {
                                nativeSetDataPath(dataPath)
                                Log.i(TAG, "BSA data path set on native: $dataPath")
                            }
                        }

            gameSurfaceView?.let {
                it.setRenderThreadInitialized(true)
                Log.i(TAG, "Signaled GameSurfaceView that render thread is initialized")
            }
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "FATAL JNI ERROR: Native library issue - ${e.message}", e)
        } catch (e: Exception) {
            Log.e(TAG, "Exception in onSurfaceCreated: ${e.message}", e)
        }
    }

    override fun onSurfaceChanged(gl: GL10, width: Int, height: Int) {
        Log.i(TAG, "=== onSurfaceChanged: ${width}x${height} ===")

        try {
            gl.glViewport(0, 0, width, height)
            Log.d(TAG, "Viewport set")

            // Notify GameSurfaceView of GL viewport size for touch coordinate scaling
            gameSurfaceView?.setGLViewportSize(width, height)

            if (nativeEngineHandle != 0L) {
                Log.d(TAG, "Calling nativeSetViewport")
                nativeSetViewport(nativeEngineHandle, width, height)
                Log.d(TAG, "nativeSetViewport completed")
            } else {
                Log.w(TAG, "onSurfaceChanged: nativeEngineHandle is 0")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception in onSurfaceChanged: ${e.message}", e)
        }
    }

    override fun onDrawFrame(gl: GL10) {
        try {
            if (nativeEngineHandle != 0L) {
                // Check for exit request before rendering
                if (nativeIsExitRequested()) {
                    Log.i(TAG, "Exit requested by native engine")
                    onExitRequested?.invoke()
                    return
                }

                nativeRenderFrame(nativeEngineHandle)
                frameCount++

                val currentTime = System.currentTimeMillis()
                if (currentTime - lastLogTime >= 1000) {
                    Log.d(TAG, "FPS: $frameCount")
                    frameCount = 0
                    lastLogTime = currentTime
                }
            } else {
                if (frameCount == 0) {
                    Log.e(TAG, "CRITICAL: onDrawFrame called but nativeEngineHandle is 0 - native initialization never completed")
                }
                frameCount++
                if (frameCount > 10) {
                    frameCount = 0
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Exception in onDrawFrame: ${e.message}", e)
        }
    }

    fun onTouchEvent(pointerId: Int, x: Float, y: Float, action: Int) {
        if (nativeEngineHandle != 0L) {
            nativeOnTouchEvent(nativeEngineHandle, pointerId, x, y, action)
        }
    }

    private external fun nativeInitEngine(): Long
    private external fun nativeSetViewport(handle: Long, width: Int, height: Int)
    private external fun nativeRenderFrame(handle: Long)
    private external fun nativeOnTouchEvent(handle: Long, pointerId: Int, x: Float, y: Float, action: Int)

    // Phase 30 Step 13: Integration test
    external fun nativeRunPhase30Test(assetPath: String): String

    // Debug System Toggles
    external fun nativeToggleDebugConsole()
    external fun nativeToggleNpcDebug()
    external fun nativeToggleWorldDebug()
    external fun nativeTogglePerfGraph()
    external fun nativeToggleAllDebug()
    external fun nativeToggleDebugMenu()
    external fun nativeIsDebugMenuVisible(): Boolean

    // Exit request check
    external fun nativeIsExitRequested(): Boolean
}
