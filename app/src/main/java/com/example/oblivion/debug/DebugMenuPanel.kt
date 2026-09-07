package com.example.oblivion.debug

import android.app.Activity
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.*


/**
 * Main debug menu panel replacing the C++ debug_menu.
 * 13 tabs matching C++ version with buttons that execute console commands via JNI.
 * Uses Android Views with semi-transparent overlay, tab bar, and scrollable content.
 */
class DebugMenuPanel(
    private val activity: Activity,
    private val commandExecutor: (String) -> String?
) {
    companion object {
        private const val TAG = "DebugMenuPanel"
        private const val TAB_COUNT = 14
        private const val BUTTON_HEIGHT_DP = 40
        private const val TAB_HEIGHT_DP = 36
        private const val PADDING_DP = 4
        private const val MARGIN_DP = 2
        private const val OVERLAY_ALPHA = 191 // 75% opaque
    }

    // Tab definitions matching C++ version
    enum class Tab(val label: String) {
        PLAYER("Player"),
        COMBAT("Combat"),
        ITEMS("Items"),
        MAGIC("Magic"),
        QUEST("Quest"),
        NPC("NPC"),
        TALK("Talk"),
        WORLD("World"),
        SAVE("Save"),
        SYSTEM("System"),
        SOUND("Sound"),
        ASSETS("Assets"),
        TOOLS("Tools"),
        LOGS("Logs")
    }

    // Command entry: label -> command string
    data class CommandEntry(val label: String, val command: String)

    // Tab contents
    private val tabContents = mutableMapOf<Tab, List<CommandEntry>>()

    // UI components
    private var overlay: FrameLayout? = null
    private var tabContainer: LinearLayout? = null
    private var contentContainer: ScrollView? = null
    private var buttonGrid: LinearLayout? = null
    private var statusText: TextView? = null
    private var currentTab = Tab.PLAYER

    // Sub-systems
    private val immediateMode = DebugImmediateMode()
    private val storage = DebugStorage(activity)

    // Phase 1 debug tools (lazy, created on first use)
    private val logcatReader by lazy { DebugLogcatReader(activity) }
    private val memoryProfiler by lazy { DebugMemoryProfiler(activity) }
    private val systemInfo by lazy { DebugSystemInfo(activity) }
    private val networkInterceptor by lazy { DebugNetworkInterceptor(activity) }
    private val crashViewer by lazy { DebugCrashViewer(activity) }

    // Active tool overlay (only one tool open at a time)
    private var toolOverlay: ViewGroup? = null
    private var activeTool: ActiveTool? = null

    private enum class ActiveTool {
        LOGCAT, MEMORY, SYSTEM_INFO, NETWORK, CRASHES
    }

    // State
    private var isVisible = false
    private var lastCommandResult: String? = null

    /**
     * Initialize the debug menu panel. Call once from Activity.onCreate().
     */
    fun initialize() {
        Log.i(TAG, "Initializing DebugMenuPanel")
        initializeTabContents()
        currentTab = Tab.entries[storage.getLastTab().coerceIn(0, Tab.entries.size - 1)]
        Log.i(TAG, "Initialized with ${tabContents.size} tabs")
    }

    /**
     * Initialize all tab contents with command mappings.
     */
    private fun initializeTabContents() {
        // Player tab
        tabContents[Tab.PLAYER] = listOf(
            CommandEntry("Heal", "heal"),
            CommandEntry("God Mode", "god"),
            CommandEntry("Set HP 100", "sethealth 100"),
            CommandEntry("Set MP 100", "setmana 100"),
            CommandEntry("Set Stamina 100", "setstamina 100"),
            CommandEntry("Set Level 50", "setlevel 50"),
            CommandEntry("Add XP 1000", "addxp 1000"),
            CommandEntry("Max Skills", "maxskills"),
            CommandEntry("Reset Stats", "resetstats"),
            CommandEntry("Set Blade 100", "setskill Blade 100"),
            CommandEntry("Set Dest 100", "setskill Destruction 100"),
            CommandEntry("Set Speed 100", "setattr Speed 100"),
            CommandEntry("Show Stats", "stats")
        )

        // Combat tab
        tabContents[Tab.COMBAT] = listOf(
            CommandEntry("Attack", "attack"),
            CommandEntry("Block", "block"),
            CommandEntry("Dodge", "dodge"),
            CommandEntry("Kill Nearest", "kill"),
            CommandEntry("Kill All", "killall"),
            CommandEntry("Resurrect", "resurrect"),
            CommandEntry("Damage 10", "damage 0 10"),
            CommandEntry("Damage 100", "damage 0 100"),
            CommandEntry("Combat Debug", "combatdebug"),
            CommandEntry("Combat Stats", "combatstats"),
            CommandEntry("Active Combats", "activecombats"),
            CommandEntry("Attack Nearest", "attacknearest"),
            CommandEntry("Set Damage x2", "setdamagemultiplier 2.0"),
            CommandEntry("Set Damage x5", "setdamagemultiplier 5.0"),
            CommandEntry("Invincible On", "invincible on"),
            CommandEntry("Invincible Off", "invincible off")
        )

        // Items tab
        tabContents[Tab.ITEMS] = listOf(
            CommandEntry("Add Gold x100", "additem 0 100"),
            CommandEntry("Add Apple x5", "additem 1 5"),
            CommandEntry("Add Health Pot", "additem 10 5"),
            CommandEntry("Add Magicka Pot", "additem 11 5"),
            CommandEntry("Remove Apple", "removeitem 1 1"),
            CommandEntry("Clear Inv", "clearinv"),
            CommandEntry("List Items", "listitems"),
            CommandEntry("Max Weight", "setweight 9999"),
            CommandEntry("Inventory Info", "inventoryinfo"),
            CommandEntry("Item Info", "iteminfo 0"),
            CommandEntry("Add Sword", "additem 100 1"),
            CommandEntry("Add Shield", "additem 200 1"),
            CommandEntry("Add Armor", "additem 300 1"),
            CommandEntry("Carry Weight", "carryweight")
        )

        // Magic tab
        tabContents[Tab.MAGIC] = listOf(
            CommandEntry("Learn Fire", "learnspell 1"),
            CommandEntry("Learn Heal", "learnspell 2"),
            CommandEntry("Learn Light", "learnspell 3"),
            CommandEntry("Equip Fire", "equipspell 1"),
            CommandEntry("Cast Fire", "castspell 1 0"),
            CommandEntry("Cast Heal", "castspell 2 0"),
            CommandEntry("List Spells", "listspells"),
            CommandEntry("Set MP 100", "setmana 100"),
            CommandEntry("Spell Info", "spellinfo 1"),
            CommandEntry("Player Spells", "playerspells"),
            CommandEntry("Cast at Enemy", "castspellatenemy 1"),
            CommandEntry("Infinite Mana On", "infinitmana on"),
            CommandEntry("Infinite Mana Off", "infinitmana off"),
            CommandEntry("Spell Damage x2", "setspelldamage 2.0")
        )

        // Quest tab
        tabContents[Tab.QUEST] = listOf(
            CommandEntry("List Quests", "listquests"),
            CommandEntry("Active Quests", "activequests"),
            CommandEntry("Quest Details", "questdetails 1"),
            CommandEntry("Accept Main", "acceptquest 1"),
            CommandEntry("Accept Side", "acceptquest 2"),
            CommandEntry("Complete Q1", "completequest 1"),
            CommandEntry("Fail Q1", "failquest 1"),
            CommandEntry("Update Obj", "updateobj 1 1 5"),
            CommandEntry("Reset Quest", "resetquest 1"),
            CommandEntry("Quest Reward", "questreward 1"),
            CommandEntry("Complete Obj", "completeobjectives 1")
        )

        // NPC tab
        tabContents[Tab.NPC] = listOf(
            CommandEntry("List NPCs", "listnpcs"),
            CommandEntry("NPC Count", "npccount"),
            CommandEntry("NPC Info", "npcinfo 0"),
            CommandEntry("Nearby", "nearby"),
            CommandEntry("Spawn Guard", "spawnat Guard 0 0 0"),
            CommandEntry("Spawn Mage", "spawnat Mage 5 0 5"),
            CommandEntry("Spawn Bandit", "spawnat Bandit -5 0 5"),
            CommandEntry("Spawn at Player", "spawnplayer Guard"),
            CommandEntry("Kill All NPCs", "killallnpcs"),
            CommandEntry("Aggro NPC", "aggro 0"),
            CommandEntry("Calm NPC", "calm 0"),
            CommandEntry("Set AI Combat", "setai 0 combat"),
            CommandEntry("Set AI Idle", "setai 0 idle"),
            CommandEntry("Resurrect", "resurrectnpc 0"),
            CommandEntry("Set Speed", "setnpcspeed 100")
        )

        // Dialogue tab
        tabContents[Tab.TALK] = listOf(
            CommandEntry("Talk NPC", "talk 0"),
            CommandEntry("Dialogue State", "dialoguestate"),
            CommandEntry("Topics", "dialoguetopics"),
            CommandEntry("Choices", "dialoguechoices"),
            CommandEntry("History", "dialoguehistory"),
            CommandEntry("Topic 0", "selecttopic 0"),
            CommandEntry("Topic 1", "selecttopic 1"),
            CommandEntry("Topic 2", "selecttopic 2"),
            CommandEntry("Choice 0", "selectchoice 0"),
            CommandEntry("Choice 1", "selectchoice 1"),
            CommandEntry("End Talk", "endtalk"),
            CommandEntry("Reset Dialogue", "resetdialogue")
        )

        // World tab
        tabContents[Tab.WORLD] = listOf(
            CommandEntry("Weather Clear", "setweather clear"),
            CommandEntry("Weather Rain", "setweather rain"),
            CommandEntry("Weather Snow", "setweather snow"),
            CommandEntry("Weather Fog", "setweather fog"),
            CommandEntry("Weather Storm", "setweather storm"),
            CommandEntry("Time Dawn (6)", "settime 6"),
            CommandEntry("Time Noon (12)", "settime 12"),
            CommandEntry("Time Dusk (18)", "settime 18"),
            CommandEntry("Time Midnight", "settime 0"),
            CommandEntry("Time x30", "settimescale 30"),
            CommandEntry("Time x1", "settimescale 1"),
            CommandEntry("Time x0 (Pause)", "settimescale 0"),
            CommandEntry("Load Cell 0,0", "loadcell 0 0"),
            CommandEntry("World Info", "worldinfo"),
            CommandEntry("Player Position", "playerpos"),
            CommandEntry("Nearby Cells", "nearbycells"),
            CommandEntry("Active Cells", "activecells"),
            CommandEntry("Teleport 0,0", "teleportcell 0 0"),
            CommandEntry("Teleport 1,0", "teleportcell 1 0"),
            CommandEntry("Teleport 0,1", "teleportcell 0 1"),
            CommandEntry("Teleport 1,1", "teleportcell 1 1"),
            CommandEntry("Move North", "moverel 0 -512"),
            CommandEntry("Move South", "moverel 0 512"),
            CommandEntry("Move East", "moverel 512 0"),
            CommandEntry("Move West", "moverel -512 0")
        )

        // Save tab
        tabContents[Tab.SAVE] = listOf(
            CommandEntry("Quick Save", "quicksave"),
            CommandEntry("Quick Load", "quickload"),
            CommandEntry("Save Slot 0", "save 0"),
            CommandEntry("Save Slot 1", "save 1"),
            CommandEntry("Save Slot 2", "save 2"),
            CommandEntry("Load Slot 0", "load 0"),
            CommandEntry("Load Slot 1", "load 1"),
            CommandEntry("Load Slot 2", "load 2"),
            CommandEntry("List Saves", "listsaves")
        )

        // System tab
        tabContents[Tab.SYSTEM] = listOf(
            CommandEntry("Toggle Wireframe", "wireframe"),
            CommandEntry("Toggle AABB", "aabb"),
            CommandEntry("NPC Overlay", "npcoverlay"),
            CommandEntry("Touch Trail", "touchtrail"),
            CommandEntry("Debug HUD+", "debughudnext"),
            CommandEntry("Debug HUD-", "debughudprev"),
            CommandEntry("Debug Log", "debuglog"),
            CommandEntry("FPS Stats", "fpsstats"),
            CommandEntry("Memory Stats", "memorystats"),
            CommandEntry("Performance", "performance"),
            CommandEntry("Reset Stats", "resetstats")
        )

        // Sound tab
        tabContents[Tab.SOUND] = listOf(
            CommandEntry("Play BGM", "playbgm"),
            CommandEntry("Stop BGM", "stopbgm"),
            CommandEntry("Play SE", "playse"),
            CommandEntry("Stop All SE", "stopallse"),
            CommandEntry("Set Vol 50%", "setvolume 0.5"),
            CommandEntry("Set Vol 100%", "setvolume 1.0"),
            CommandEntry("Mute All", "mute"),
            CommandEntry("Unmute All", "unmute"),
            CommandEntry("List Audio", "listaudio"),
            CommandEntry("Audio Stats", "audiostats"),
            CommandEntry("List BGM Tracks", "listbgm"),
            CommandEntry("BGM Info", "bgminfo"),
            CommandEntry("BGM Vol 25%", "bgmvolume 0.25"),
            CommandEntry("BGM Vol 50%", "bgmvolume 0.5"),
            CommandEntry("BGM Vol 75%", "bgmvolume 0.75"),
            CommandEntry("BGM Vol 100%", "bgmvolume 1.0")
        )

        // Assets tab
        tabContents[Tab.ASSETS] = listOf(
            CommandEntry("List Textures", "listtextures"),
            CommandEntry("List Models", "listmodels"),
            CommandEntry("List Audio", "listaudio"),
            CommandEntry("Texture Info", "textureinfo"),
            CommandEntry("Model Info", "modelinfo"),
            CommandEntry("Cache Stats", "cachestats"),
            CommandEntry("Clear Cache", "clearcache"),
            CommandEntry("Reload Assets", "reloadassets"),
            CommandEntry("Memory Usage", "memoryusage"),
            CommandEntry("Asset Stats", "assetstats"),
            CommandEntry("Textures Detail", "texturesdetail")
        )

        // Logs tab
        tabContents[Tab.LOGS] = listOf(
            CommandEntry("Show All Logs", "loglevel all"),
            CommandEntry("Show Debug", "loglevel debug"),
            CommandEntry("Show Info", "loglevel info"),
            CommandEntry("Show Warning", "loglevel warn"),
            CommandEntry("Show Error", "loglevel error"),
            CommandEntry("Clear Logs", "clearlogs"),
            CommandEntry("Export Logs", "exportlogs"),
            CommandEntry("Log Stats", "logstats"),
            CommandEntry("Search Logs", "searchlog"),
            CommandEntry("Toggle Auto-scroll", "logautoscroll")
        )

        // Tools tab - debug tools launched as separate overlays
        tabContents[Tab.TOOLS] = listOf(
            CommandEntry("Open Logcat Reader", "__open_logcat"),
            CommandEntry("Open Memory Profiler", "__open_memory"),
            CommandEntry("Open System Info", "__open_system_info"),
            CommandEntry("Open Network Inspector", "__open_network"),
            CommandEntry("Show Crash History", "__show_crashes"),
            CommandEntry("Clear Crash History", "__clear_crashes"),
            CommandEntry("Export Logcat", "__export_logcat"),
            CommandEntry("Export Crash History", "__export_crashes"),
            CommandEntry("Export All Debug Artifacts", "__export_all"),
            CommandEntry("Share Latest Export", "__share_latest"),
            CommandEntry("Clear Exports", "__clear_exports")
        )
    }

    /**
     * Show the debug menu overlay.
     */
    fun show() {
        if (isVisible) {
            // Already visible - bring to front
            overlay?.bringToFront()
            return
        }

        activity.runOnUiThread {
            val rootLayout = activity.findViewById<FrameLayout>(android.R.id.content) ?: return@runOnUiThread
            val density = activity.resources.displayMetrics.density

            // Try-catch around overlay removal
            try {
                overlay?.let { existing ->
                    (existing.parent as? ViewGroup)?.removeView(existing)
                }
            } catch (e: Exception) {
                Log.w(TAG, "Error removing existing overlay: ${e.message}")
            }
            overlay = null

            // Create overlay container
            val newOverlay = FrameLayout(activity).apply {
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                )
                setBackgroundColor(Color.argb(OVERLAY_ALPHA, 0, 0, 0))
            }

            // Main vertical layout
            val mainLayout = LinearLayout(activity).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                )
                setPadding(
                    dpToPx(8f, density),
                    dpToPx(40f, density), // Below status bar
                    dpToPx(8f, density),
                    dpToPx(8f, density)
                )
            }

            // Status text at top
            statusText = TextView(activity).apply {
                text = "Debug Menu - ${currentTab.label}"
                setTextColor(Color.WHITE)
                textSize = 14f
                setPadding(dpToPx(8f, density), 0, 0, dpToPx(4f, density))
            }
            mainLayout.addView(statusText)

            // Tab bar - HorizontalScrollView for 13 tabs
            val tabScroll = HorizontalScrollView(activity).apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    dpToPx(TAB_HEIGHT_DP.toFloat(), density)
                )
                isHorizontalScrollBarEnabled = false
            }

            tabContainer = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
            }

            // Create tab buttons
            Tab.entries.forEach { tab ->
                val tabButton = Button(activity).apply {
                    text = tab.label
                    textSize = 10f
                    isAllCaps = false
                    setPadding(dpToPx(6f, density), 0, dpToPx(6f, density), 0)
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.MATCH_PARENT
                    ).apply {
                        setMargins(dpToPx(MARGIN_DP.toFloat(), density), 0,
                            dpToPx(MARGIN_DP.toFloat(), density), 0)
                    }

                    val isActive = tab == currentTab
                    setBackgroundColor(if (isActive) {
                        Color.argb(230, 102, 179, 102)
                    } else {
                        Color.argb(204, 51, 51, 77)
                    })
                    setTextColor(if (isActive) Color.WHITE else Color.argb(204, 179, 179, 179))

                    setOnClickListener {
                        switchTab(tab)
                    }
                }
                tabContainer?.addView(tabButton) ?: Log.e(TAG, "tabContainer null")
            }

            tabScroll.addView(tabContainer)
            mainLayout.addView(tabScroll)

            // Content area - ScrollView with button grid
            contentContainer = ScrollView(activity).apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    0,
                    1f // Fill remaining space
                )
                setBackgroundColor(Color.argb(153, 25, 25, 38))
            }

            buttonGrid = LinearLayout(activity).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
                setPadding(
                    dpToPx(4f, density),
                    dpToPx(4f, density),
                    dpToPx(4f, density),
                    dpToPx(4f, density)
                )
            }

            // Populate buttons for current tab
            populateButtons(currentTab)

            contentContainer?.addView(buttonGrid) ?: Log.e(TAG, "contentContainer null")
            mainLayout.addView(contentContainer)

            // Close button at bottom
            val closeButton = Button(activity).apply {
                text = "Close Debug Menu"
                textSize = 12f
                setBackgroundColor(Color.argb(204, 179, 77, 77))
                setTextColor(Color.WHITE)
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    dpToPx(36f, density)
                ).apply {
                    topMargin = dpToPx(4f, density)
                }
                setOnClickListener { hide() }
            }
            mainLayout.addView(closeButton)

            newOverlay.addView(mainLayout)
            rootLayout.addView(newOverlay)

            overlay = newOverlay
            isVisible = true
            storage.savePanelVisible(true)
            Log.i(TAG, "Debug menu shown")
        }
    }

    /**
     * Hide the debug menu overlay and clean up.
     */
    fun hide() {
        if (!isVisible) return

        activity.runOnUiThread {
            // Save current state before hiding
            storage.saveLastTab(currentTab.ordinal)
            contentContainer?.let { scroll ->
                storage.saveScrollOffset(currentTab.ordinal, scroll.scrollY.toFloat())
            }

            // Close any tool overlays before tearing down the menu
            closeActiveTool()

            // Clean up overlay
            overlay?.let { existing ->
                val parent = existing.parent as? ViewGroup
                parent?.removeView(existing)
            }
            overlay = null
            tabContainer = null
            contentContainer = null
            buttonGrid = null
            statusText = null

            // Reset immediate mode state
            immediateMode.clearState()

            isVisible = false
            storage.savePanelVisible(false)
            Log.i(TAG, "Debug menu hidden")
        }
    }

    /**
     * Toggle visibility.
     */
    fun toggle() {
        if (isVisible) hide() else show()
    }

    /**
     * Switch to a different tab.
     */
    private fun switchTab(tab: Tab) {
        if (tab == currentTab) return

        currentTab = tab
        storage.saveLastTab(tab.ordinal)

        activity.runOnUiThread {
            // Update tab button styles
            val tabScroll = tabContainer?.parent as? HorizontalScrollView
            val density = activity.resources.displayMetrics.density

            tabContainer?.let { container ->
                for (i in 0 until container.childCount) {
                    val btn = container.getChildAt(i) as? Button ?: continue
                    val isActive = (i == tab.ordinal)
                    btn.setBackgroundColor(if (isActive) {
                        Color.argb(230, 102, 179, 102)
                    } else {
                        Color.argb(204, 51, 51, 77)
                    })
                    btn.setTextColor(if (isActive) Color.WHITE else Color.argb(204, 179, 179, 179))
                }
            }

            // Update content buttons
            populateButtons(tab)

            // Restore scroll position
            val savedOffset = storage.getScrollOffset(tab.ordinal)
            contentContainer?.post {
                contentContainer?.scrollTo(0, savedOffset.toInt())
            }

            // Update status text
            statusText?.text = "Debug Menu - ${tab.label}"
        }
    }

    /**
     * Populate buttons for the given tab.
     */
    private fun populateButtons(tab: Tab) {
            val commands = tabContents[tab] ?: emptyList()
        val density = activity.resources.displayMetrics.density

        buttonGrid?.let { grid ->
            grid.removeAllViews()

                if (commands.isEmpty()) {
                    // Fallback for null/empty commands
                    val emptyLabel = TextView(activity).apply {
                        text = "No commands for ${tab.label}"
                        setTextColor(Color.GRAY)
                        setPadding(dpToPx(8f, density), dpToPx(8f, density), 0, 0)
                    }
                    grid.addView(emptyLabel)
                    return
                }

                // Use 2-column grid layout
                // Declare currentRow outside loop so it persists between iterations
                var currentRow: LinearLayout? = null

                for ((index, entry) in commands.withIndex()) {
                    if (index % 2 == 0) {
                        currentRow = LinearLayout(activity).apply {
                            orientation = LinearLayout.HORIZONTAL
                            layoutParams = LinearLayout.LayoutParams(
                                LinearLayout.LayoutParams.MATCH_PARENT,
                                LinearLayout.LayoutParams.WRAP_CONTENT
                            ).apply {
                                bottomMargin = dpToPx(MARGIN_DP.toFloat(), density)
                            }
                        }
                        grid.addView(currentRow)
                    }

                    val button = Button(activity).apply {
                        text = entry.label
                        textSize = 11f
                        isAllCaps = false
                        setPadding(dpToPx(4f, density), 0, dpToPx(4f, density), 0)
                        setBackgroundColor(DebugThemedStyles.withAlpha(
                            DebugThemedStyles.TabAccents.forTabIndex(tab.ordinal), 180))
                        setTextColor(Color.WHITE)
                        layoutParams = LinearLayout.LayoutParams(
                            0,
                            dpToPx(BUTTON_HEIGHT_DP.toFloat(), density),
                            1f
                        ).apply {
                            setMargins(
                                dpToPx(MARGIN_DP.toFloat(), density), 0,
                                dpToPx(MARGIN_DP.toFloat(), density), 0
                            )
                        }

                        setOnClickListener {
                            executeCommand(entry)
                        }
                    }

                    currentRow?.addView(button)
                }
            }
        }

    /**
     * Execute a command entry.
     */
    private fun executeCommand(entry: CommandEntry) {
        Log.d(TAG, "Executing command: ${entry.command}")
        // Guard against executing commands while the activity is finishing or destroyed
        if (activity.isFinishing || activity.isDestroyed) {
            Log.w(TAG, "Skipping command '${entry.command}' - activity is finishing or destroyed")
            return
        }

        // Handle internal tool commands (prefixed with "__")
        if (entry.command.startsWith("__")) {
            handleInternalCommand(entry.command)
            return
        }

        try {
            val result = commandExecutor(entry.command)
            lastCommandResult = result
            Log.i(TAG, "Command '${entry.command}' result: ${result ?: "null"}")

            // Show brief toast feedback - check Looper and activity state
            if (android.os.Looper.myLooper() != null &&
                !activity.isFinishing && !activity.isDestroyed) {
                activity.runOnUiThread {
                    if (!activity.isFinishing && !activity.isDestroyed) {
                        Toast.makeText(activity, "${entry.label}: ${result ?: "OK"}", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error executing command '${entry.command}': ${e.message}", e)
            lastCommandResult = "Error: ${e.message}"
        }
    }

    /**
     * Handle internal panel commands that open or close debug tools.
     */
    private fun handleInternalCommand(command: String) {
        try {
            when (command) {
                "__open_logcat" -> openToolOverlay(ActiveTool.LOGCAT, logcatReader.build(), logcatReader::start, logcatReader::stop)
                "__open_memory" -> openToolOverlay(ActiveTool.MEMORY, memoryProfiler.build(), memoryProfiler::start, memoryProfiler::stop)
                "__open_system_info" -> openToolOverlay(ActiveTool.SYSTEM_INFO, systemInfo.build(), systemInfo::start, systemInfo::stop)
                "__open_network" -> openToolOverlay(ActiveTool.NETWORK, networkInterceptor.build(), networkInterceptor::start, networkInterceptor::stop)
                "__show_crashes" -> openToolOverlay(ActiveTool.CRASHES, crashViewer.build(), crashViewer::start, crashViewer::stop)
                "__clear_crashes" -> {
                    DebugCrashHandler.clearHistory()
                    Toast.makeText(activity, "Crash history cleared", Toast.LENGTH_SHORT).show()
                }
                "__export_logcat" -> {
                    val file = DebugExportShare.exportLogcat(activity, getRecentLogcatLines())
                    val msg = if (file != null) "Logcat exported: ${file.name}" else "Export failed"
                    Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show()
                }
                "__export_crashes" -> {
                    val file = DebugExportShare.exportCrashHistory(activity)
                    val msg = if (file != null) "Crash history exported: ${file.name}" else "No crashes to export"
                    Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show()
                }
                "__export_all" -> {
                    exportAllDebugArtifacts()
                }
                "__share_latest" -> {
                    val exports = DebugExportShare.listExports(activity)
                    val latest = exports.maxByOrNull { it.lastModified() }
                    if (latest != null) {
                        DebugExportShare.shareFile(activity, latest)
                    } else {
                        Toast.makeText(activity, "No exports to share", Toast.LENGTH_SHORT).show()
                    }
                }
                "__clear_exports" -> {
                    val success = DebugExportShare.clearExports(activity)
                    val msg = if (success) "Exports cleared" else "Clear failed"
                    Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show()
                }
                else -> Log.w(TAG, "Unknown internal command: $command")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to handle internal command '$command': ${e.message}", e)
        }
    }

    /**
     * Open a tool view as a full-screen overlay on top of the debug menu.
     */
    private fun openToolOverlay(tool: ActiveTool, content: View, onStart: () -> Unit, onStop: () -> Unit) {
        if (!isVisible) return
        closeActiveTool()

        val overlay = FrameLayout(activity).apply {
            setBackgroundColor(0xCC000000.toInt())
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        }

        val container = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        }

        val closeButton = Button(activity).apply {
            text = "Close"
            setOnClickListener { closeActiveTool() }
        }
        container.addView(closeButton, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        ))

        container.addView(content, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            0,
            1f
        ))

        overlay.addView(container)
        (toolOverlay?.parent as? ViewGroup)?.removeView(toolOverlay)
        toolOverlay = overlay
        activeTool = tool
        attachOverlay(overlay)
        onStart()
    }

    /**
     * Attach an overlay to the activity's content view.
     */
    private fun attachOverlay(overlay: View) {
        val root = activity.window.decorView.findViewById<ViewGroup>(android.R.id.content)
        (root as? ViewGroup)?.addView(
            overlay,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
        )
    }

    /**
     * Close the currently active tool overlay, if any.
     */
    private fun closeActiveTool() {
        val current = toolOverlay
        if (current != null) {
            when (activeTool) {
                ActiveTool.LOGCAT -> logcatReader.cleanup()
                ActiveTool.MEMORY -> memoryProfiler.cleanup()
                ActiveTool.SYSTEM_INFO -> systemInfo.cleanup()
                ActiveTool.NETWORK -> networkInterceptor.cleanup()
                ActiveTool.CRASHES -> crashViewer.cleanup()
                null -> {}
            }
            (current.parent as? ViewGroup)?.removeView(current)
        }
        toolOverlay = null
        activeTool = null
    }

    /**
     * Get recent logcat lines from the native console output provider.
     * Falls back to an empty list if no provider is available.
     */
    private fun getRecentLogcatLines(): List<String> {
        return try {
            val provider = commandExecutor("getrecentlog")
            provider?.split('\n')?.filter { it.isNotBlank() }.orEmpty()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to fetch recent logcat: ${e.message}")
            emptyList()
        }
    }

    /**
     * Export all available debug artifacts (logcat, crash history).
     * Reports the result via Toast.
     */
    private fun exportAllDebugArtifacts() {
        try {
            val crashFile = DebugExportShare.exportCrashHistory(activity)
            val logFile = DebugExportShare.exportLogcat(activity, getRecentLogcatLines())
            val count = listOfNotNull(crashFile, logFile).size
            val msg = when {
                count == 2 -> "Exported 2 artifacts"
                count == 1 -> "Exported 1 artifact"
                else -> "Nothing to export"
            }
            Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Log.e(TAG, "Export all failed: ${e.message}")
            Toast.makeText(activity, "Export failed: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    /**
     * Check if the menu is currently visible.
     */
    fun isVisible(): Boolean = isVisible

    /**
     * Get the last command result.
     */
    fun getLastResult(): String? = lastCommandResult

    /**
     * Get the current active tab.
     */
    fun getCurrentTab(): Tab = currentTab

    /**
     * Get storage reference for external access.
     */
    fun getStorage(): DebugStorage = storage

    /**
     * Get immediate mode reference for external access.
     */
    fun getImmediateMode(): DebugImmediateMode = immediateMode

    /**
     * Cleanup all resources. Call from Activity.onDestroy().
     */
    fun cleanup() {
        hide()
        immediateMode.clearState()
        Log.i(TAG, "Cleanup complete")
    }

    /**
     * Convert dp to pixels.
     */
    private fun dpToPx(dp: Float, density: Float): Int = (dp * density + 0.5f).toInt()
}
