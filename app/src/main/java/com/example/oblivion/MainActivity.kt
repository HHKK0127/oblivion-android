package com.example.oblivion

import android.app.Activity
import android.media.AudioManager
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import java.io.IOException
import java.io.File
import com.example.oblivion.debug.DebugMenuPanel

class MainActivity : Activity() {

    private var gameSurfaceView: GameSurfaceView? = null
    private var gameRenderer: GameRenderer? = null
    private var mediaPlayer: MediaPlayer? = null
    private var soundPool: SoundPool? = null
    private val loadedSounds = mutableMapOf<String, Int>() // filename -> soundId
    private var spLoadListener: SoundPool.OnLoadCompleteListener? = null
    private var debugButtonPanel: LinearLayout? = null
    private var isDebugPanelVisible = false
    private var debugMenuPanel: DebugMenuPanel? = null

    companion object {
        private const val TAG = "MainActivity"
        @Volatile
        private var instance: MainActivity? = null

        fun getInstance(): MainActivity? = instance
    }

    fun playBGM(filename: String) {
        runOnUiThread { playBGMInternal(filename) }
    }

    fun stopBGM() {
        runOnUiThread {
            mediaPlayer?.let {
                if (it.isPlaying) {
                    it.stop()
                    Log.i(TAG, "BGM stopped")
                }
            }
        }
    }

    fun playSE(filename: String) {
        runOnUiThread { playSEInternal(filename) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(TAG, "=== onCreate called ===")

        // Dismiss keyguard and turn screen on
        @Suppress("DEPRECATION")
        window.addFlags(
            android.view.WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
            android.view.WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON or
            android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON or
            android.view.WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD
        )

        instance = this

        try {
            // Initialize game immediately - asset extraction is optional
            Log.i(TAG, "Initializing game (asset extraction deferred)")
            initializeGame()

            // Extract assets in background if needed (non-blocking)
            Thread {
                try {
                    val assetExtractor = AssetExtractor(this@MainActivity)
                    if (assetExtractor.needsExtraction()) {
                        Log.i(TAG, "Background: Extracting assets to external storage")
                        assetExtractor.extractAssets { current, total ->
                            Log.d(TAG, "Extracting: $current/$total")
                        }
                        Log.i(TAG, "Background: Asset extraction complete")
                    } else {
                        Log.i(TAG, "Background: Assets already extracted")
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Background asset extraction failed (non-fatal): ${e.message}")
                }
            }.start()
        } catch (e: Exception) {
            Log.e(TAG, "Exception in onCreate: ${e.message}", e)
        }
    }

    private fun initializeGame() {
        try {
            Log.i(TAG, "Initializing audio system")
            initializeAudio()

            Log.i(TAG, "Setting content view with debug overlay")
            setContentView(R.layout.activity_main)

            // Get the GLSurfaceView from layout and setup renderer
            val glSurfaceView = findViewById<android.opengl.GLSurfaceView>(R.id.gl_surface_view)
            if (glSurfaceView != null) {
                // Create GameRenderer with default constructor
                gameRenderer = GameRenderer()
                gameRenderer!!.setOnExitRequestedListener {
                    Log.i(TAG, "Exit requested - finishing activity")
                    runOnUiThread { finish() }
                }
                glSurfaceView.setEGLContextClientVersion(3)
                glSurfaceView.setRenderer(gameRenderer!!)
                glSurfaceView.renderMode = android.opengl.GLSurfaceView.RENDERMODE_CONTINUOUSLY

                // Setup touch event forwarding
                glSurfaceView.setOnTouchListener { _, event ->
                    val actionMasked = event.actionMasked
                    val actionIndex = event.actionIndex
                    val pointerId: Int
                    val x: Float
                    val y: Float
                    val action: Int

                    when (actionMasked) {
                        android.view.MotionEvent.ACTION_DOWN -> {
                            pointerId = event.getPointerId(0)
                            x = event.getX(0)
                            y = event.getY(0)
                            action = 0
                        }
                        android.view.MotionEvent.ACTION_UP -> {
                            pointerId = event.getPointerId(0)
                            x = event.getX(0)
                            y = event.getY(0)
                            action = 1
                        }
                        android.view.MotionEvent.ACTION_MOVE -> {
                            pointerId = event.getPointerId(0)
                            x = event.getX(0)
                            y = event.getY(0)
                            action = 2
                        }
                        android.view.MotionEvent.ACTION_POINTER_DOWN -> {
                            pointerId = event.getPointerId(actionIndex)
                            x = event.getX(actionIndex)
                            y = event.getY(actionIndex)
                            action = 5
                        }
                        android.view.MotionEvent.ACTION_POINTER_UP -> {
                            pointerId = event.getPointerId(actionIndex)
                            x = event.getX(actionIndex)
                            y = event.getY(actionIndex)
                            action = 6
                        }
                        else -> {
                            pointerId = event.getPointerId(0)
                            x = event.getX(0)
                            y = event.getY(0)
                            action = 3
                        }
                    }
                    gameRenderer?.onTouchEvent(pointerId, x, y, action)
                    true
                }

                Log.i(TAG, "GLSurfaceView setup complete")
            } else {
                Log.e(TAG, "GLSurfaceView not found in layout")
                // Fallback to creating GameSurfaceView directly
                gameSurfaceView = GameSurfaceView(this)
                setContentView(gameSurfaceView)
            }

            // Setup debug buttons
            setupDebugButtons()

            // Setup Kotlin debug menu panel
            setupDebugMenuPanel()

            Log.i(TAG, "ContentView set successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Exception in initializeGame: ${e.message}", e)
        }
    }

    private fun setupDebugButtons() {
        try {
            debugButtonPanel = findViewById(R.id.debug_button_panel)
            val debugToggleBtn = findViewById<Button>(R.id.btn_debug_toggle)

            // Toggle debug panel visibility
            debugToggleBtn.setOnClickListener {
                isDebugPanelVisible = !isDebugPanelVisible
                debugButtonPanel?.visibility = if (isDebugPanelVisible) View.VISIBLE else View.GONE
                Log.d(TAG, "Debug panel ${if (isDebugPanelVisible) "shown" else "hidden"}")
            }

            // Debug Console toggle
            findViewById<Button>(R.id.btn_debug_console)?.setOnClickListener {
                gameRenderer?.nativeToggleDebugConsole()
                Log.d(TAG, "Toggled debug console")
            }

            // NPC Debug toggle
            findViewById<Button>(R.id.btn_debug_npc)?.setOnClickListener {
                gameRenderer?.nativeToggleNpcDebug()
                Log.d(TAG, "Toggled NPC debug")
            }

            // World Debug toggle
            findViewById<Button>(R.id.btn_debug_world)?.setOnClickListener {
                gameRenderer?.nativeToggleWorldDebug()
                Log.d(TAG, "Toggled world debug")
            }

            // Performance Graph toggle
            findViewById<Button>(R.id.btn_debug_perf)?.setOnClickListener {
                gameRenderer?.nativeTogglePerfGraph()
                Log.d(TAG, "Toggled performance graph")
            }

            // All Debug toggle
            findViewById<Button>(R.id.btn_debug_all)?.setOnClickListener {
                gameRenderer?.nativeToggleAllDebug()
                Log.d(TAG, "Toggled all debug systems")
            }

            // Debug Menu toggle
            findViewById<Button>(R.id.btn_debug_menu)?.setOnClickListener {
                gameRenderer?.nativeToggleDebugMenu()
                Log.d(TAG, "Toggled debug menu")
            }

            Log.i(TAG, "Debug buttons setup complete")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to setup debug buttons: ${e.message}", e)
        }
    }

    private fun setupDebugMenuPanel() {
        try {
            // Initialize the Kotlin debug menu panel with JNI command executor
            debugMenuPanel = DebugMenuPanel(this) { command ->
                try {
                    val result = gameRenderer?.nativeExecuteCommand(command) ?: "No renderer"
                    Log.d(TAG, "Debug command '$command' -> $result")
                    result
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to execute debug command '$command': ${e.message}", e)
                    "Error: ${e.message}"
                }
            }
            debugMenuPanel?.initialize()

            // Setup floating debug FAB button
            val debugFab = findViewById<Button>(R.id.btn_debug_fab)
            debugFab?.setOnClickListener {
                debugMenuPanel?.toggle()
                Log.d(TAG, "Debug menu toggled via FAB")
            }

            Log.i(TAG, "Kotlin debug menu panel setup complete")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to setup debug menu panel: ${e.message}", e)
        }
    }

    private fun initializeAudio() {
            try {
                mediaPlayer = MediaPlayer()
                volumeControlStream = AudioManager.STREAM_MUSIC
                Log.i(TAG, "MediaPlayer initialized for BGM")

                soundPool = SoundPool.Builder().setMaxStreams(5).build()
                Log.i(TAG, "SoundPool initialized for SE (max 5 sounds)")

                try {
                    GameRenderer.nativeInitAudioBridge(assets, this)
                    Log.i(TAG, "Audio bridge initialized with AssetManager and MainActivity")
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to initialize audio bridge: ${e.message}")
                }

                // Set data path for BSA/ESM file lookup
                // Note: nativeSetDataPath will be called after nativeInitEngine creates the renderer
                // Store for later use in onSurfaceCreated callback
                try {
                    val dataPath = filesDir.absolutePath + File.separator + "data"
                    val dataDir = java.io.File(dataPath)
                    if (!dataDir.exists()) {
                        dataDir.mkdirs()
                        Log.i(TAG, "Created data directory: $dataPath")
                    }
                    // Store path for use after engine initialization
                    GameRenderer.dataPath = dataPath
                    Log.i(TAG, "BSA data path stored: $dataPath (will be set on native after engine init)")
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to set BSA data path: ${e.message}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to initialize audio", e)
            }
        }

    override fun onPause() {
        super.onPause()
        Log.i(TAG, "onPause")
        // Do NOT call gameSurfaceView.onPause() - keep GL thread running
        // GLSurfaceView.onPause() kills the render thread, which prevents onSurfaceCreated
        mediaPlayer?.let {
            if (it.isPlaying) {
                it.pause()
                Log.i(TAG, "BGM paused")
            }
        }
    }

    override fun onResume() {
        super.onResume()
        Log.i(TAG, "onResume")
        gameSurfaceView?.onResume()
        // Note: startRenderer() is called inside GameSurfaceView.onResume() if needed
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "onDestroy - cleaning up audio and debug")
        cleanupAudio()
        debugMenuPanel?.cleanup()
        debugMenuPanel = null
        if (instance === this) {
            instance = null
        }
    }

    private fun playBGMInternal(filename: String) {
        try {
            val mp = mediaPlayer
            if (mp == null) {
                Log.e(TAG, "MediaPlayer not initialized")
                return
            }

            if (mp.isPlaying) {
                mp.stop()
            }
            mp.reset()

            val assetPath = "audio/music/$filename"
            Log.i(TAG, "Loading BGM: $assetPath")

            val afd = assets.openFd(assetPath)
            mp.setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
            afd.close()
            mp.prepare()
            mp.isLooping = true
            mp.start()

            Log.i(TAG, "BGM playing: $filename")
        } catch (e: IOException) {
            Log.e(TAG, "Failed to play BGM: $filename", e)
        }
    }

    private fun playSEInternal(filename: String) {
        try {
            val sp = soundPool
            if (sp == null) {
                Log.e(TAG, "SoundPool not initialized")
                return
            }

            // Cache hit → play immediately
            loadedSounds[filename]?.let { cachedId ->
                sp.play(cachedId, 1.0f, 1.0f, 0, 0, 1.0f)
                Log.i(TAG, "SE playing from cache: $filename")
                return
            }

            val assetPath = "audio/sounds/$filename"
            Log.i(TAG, "Loading SE: $assetPath")

            val afd = assets.openFd(assetPath)
            val soundId = sp.load(afd, 1)
            afd.close()
            loadedSounds[filename] = soundId

            // Set listener only once
            if (spLoadListener == null) {
                spLoadListener = SoundPool.OnLoadCompleteListener { pool, sampleId, status ->
                    if (status == 0) {
                        pool.play(sampleId, 1.0f, 1.0f, 0, 0, 1.0f)
                    } else {
                        Log.e(TAG, "Failed to load SE, status=$status")
                    }
                }
                sp.setOnLoadCompleteListener(spLoadListener)
            }

            Log.i(TAG, "SE playing queued: $filename")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to play SE: $filename", e)
        }
    }

    private fun cleanupAudio() {
        try {
            mediaPlayer?.let {
                if (it.isPlaying) {
                    it.stop()
                }
                it.release()
                Log.i(TAG, "MediaPlayer released")
            }
            mediaPlayer = null

            soundPool?.let {
                it.release()
                Log.i(TAG, "SoundPool released")
            }
            loadedSounds.clear()
            spLoadListener = null
            soundPool = null
        } catch (e: Exception) {
            Log.e(TAG, "Error during audio cleanup", e)
        }
    }
}
