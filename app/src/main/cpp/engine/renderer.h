#pragma once

#include <memory>
#include <array>
#include <chrono>
#include <unordered_map>
#include <android/log.h>
#include <android/asset_manager.h>
#include <GLES3/gl3.h>
#include "../ui/title_screen.h"
#include "../ui/launcher_screen.h"
#include "../ui/quest_ui.h"
#include "../ui/text_renderer.h"
#include "../ui/debug_hud.h"
#include "../system/debug_system.h"
#include "../ui/settings_ui.h"
#include "../ui/save_load_ui.h"
#include "../ui/game_console.h"
#include "../ui/debug_menu.h"
#include "../ui/npc_debug_visualizer.h"
#include "../ui/world_debug_info.h"
#include "../ui/performance_graph.h"
#include "../game/quest_manager.h"
#include "../game/npc_manager.h"
#include "../system/settings_manager.h"
#include "../game/combat_manager.h"
#include "../game/spell_manager.h"
#include "../game/navmesh_manager.h"
#include "../game/ai_scheduler.h"
#include "../game/player_controller.h"
#include "../game/inventory_manager.h"
#include "../ui/inventory_ui.h"
#include "../ui/ui_inventory_panel.h"
#include "../world/world_manager.h"
#include "../world/world_entity.h"
#include "../localization/localization_manager.h"
#include "../profiling/performance_monitor.h"
#include "../physics/physics_manager.h"
#include "../save_system/save_manager.h"
#include "../assets/asset_manager.h"
#include "graphics/retro_filter.h"
#include "../ui/ui_system.h"
#include "../ui/ui_joystick.h"
#include "../ui/ui_button.h"
#include "../ui/ui_map_panel.h"
#include "../ui/ui_floating_text.h"
#include "../ui/spell_selection_panel.h"
#include "../ui/responsive_ui_manager.h"
#include "../animation/animation_subscriber.h"
#include "../audio/audio_subscriber.h"
#include "../map/map_system.h"
#include "../inventory/inventory_grid.h"
#include "../inventory/equipment_manager.h"

#ifdef AUDIO_SYSTEM_ENABLED
#include "../audio/audio_manager.h"
#endif

#undef LOG_TAG
#undef LOGD
#undef LOGI
#undef LOGW
#undef LOGE

#define LOG_TAG "Renderer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global AssetManager (defined in jni_bridge.cpp)
extern AAssetManager* g_assetManager;

class Renderer {
private:
    // UI Systems
    std::unique_ptr<LauncherScreen> launcherScreen;
    std::unique_ptr<TitleScreen> titleScreen;
    std::unique_ptr<QuestUI> questUI;
    std::unique_ptr<InventoryUI> inventoryUI;
    std::unique_ptr<TextRenderer> textRenderer;
    std::unique_ptr<DebugHUD> debugHUD;
    std::unique_ptr<SettingsUI> settingsUI;
    std::unique_ptr<SaveLoadUI> saveLoadUI;
    std::unique_ptr<GameConsole> gameConsole;
    std::unique_ptr<DebugMenu> debugMenu;
    std::unique_ptr<NpcDebugVisualizer> npcDebugVisualizer;
    std::unique_ptr<WorldDebugInfo> worldDebugInfo;
    std::unique_ptr<PerformanceGraph> performanceGraph;

    // Settings
    std::unique_ptr<SettingsManager> settingsManager;

    // Asset System
    std::unique_ptr<AssetManager> assetManager;

    // Game Systems
    std::unique_ptr<LocalizationManager> localizationManager;
    std::unique_ptr<NpcManager> npcManager;
    std::unique_ptr<WorldManager> worldManager;
    std::unique_ptr<QuestManager> questManager;
    std::unique_ptr<CombatManager> combatManager;
    std::unique_ptr<SpellManager> spellManager;
    std::unique_ptr<oblivion::NavMeshManager> navMeshManager;
    std::unique_ptr<ai::AIScheduler> aiScheduler;
    std::unique_ptr<PlayerController> playerController;
    std::unique_ptr<InventoryManager> inventoryManager;

    // Profiling
    std::unique_ptr<PerformanceMonitor> performanceMonitor;

    // Save System
    std::unique_ptr<SaveManager> saveManager;

    // Phase 9: UI Framework System
    std::unique_ptr<UISystem> uiSystem;
    std::shared_ptr<UIJoystick> joystick;
    
    // Responsive UI Manager
    std::unique_ptr<ResponsiveUIManager> responsiveUIManager;

    // Combat UI buttons (right side of screen)
    std::shared_ptr<UIButton> attackButton;
    std::shared_ptr<UIButton> blockButton;
    std::shared_ptr<UIButton> castSpellButton;

    // Floating combat text
    std::unique_ptr<UIFloatingText> floatingText;

    // Animation subscriber for NPC animations
    std::unique_ptr<animation::AnimationSubscriber> animSubscriber;

    // Spell selection panel
    std::shared_ptr<SpellSelectionPanel> spellSelectionPanel;
    std::shared_ptr<Spell> selectedSpell;

    // Quick-slot spell buttons (4 slots at bottom-left)
    static constexpr int QUICK_SLOT_COUNT = 4;
    std::array<std::shared_ptr<UIButton>, QUICK_SLOT_COUNT> quickSlotButtons = {};
    int pendingAssignSlot = -1; // >=0 when next spell selection assigns to this slot

    // Audio subscriber for combat sound effects
    std::unique_ptr<audio::AudioSubscriber> audioSubscriber;

    // Phase 9.1: Map System
    std::unique_ptr<map::MapSystem> mapSystem;
    ui::MapUI* mapUI = nullptr;

    // Phase 9.2: Inventory System
    std::unique_ptr<inventory::InventoryGrid> inventoryGrid;
    std::unique_ptr<inventory::EquipmentManager> equipmentManager;
    ui::UIInventoryPanel* uiInventoryPanel = nullptr;

    // Phase 31: World Entity System
    std::unique_ptr<WorldLoader> worldLoader;
    std::vector<WorldEntity> worldEntities;

    // Imperial Weave: thin integration layer
    bool imperialWeaveInitialized = false;

    // Audio System
#ifdef AUDIO_SYSTEM_ENABLED
    std::unique_ptr<AudioManager> audioManager;
#endif

    // Retro Filter (Post-Processing)
    std::unique_ptr<RetroFilter> retroFilter;
    RetroFilter::Settings retroSettings;

    // State
    bool showLauncher;
    bool showTitleScreen;
    bool shouldExit;
    bool initialized = false;  // Track if initialization succeeded
    unsigned int screenWidth;
    unsigned int screenHeight;
    float uiScale = 1.0f;  // UI scale factor based on aspect ratio

    // Frame Rate Control
    int targetFPS;
    float frameTimeThreshold;  // Milliseconds per frame (1000/fps)
    std::chrono::high_resolution_clock::time_point lastFrameTime;

public:
    Renderer();
    ~Renderer();

    bool init(unsigned int width, unsigned int height);
    void render(float deltaTime);
    void cleanup();
    void resize(unsigned int width, unsigned int height);

    // Getters
    AssetManager* getAssetManager() { return assetManager.get(); }
    LocalizationManager* getLocalizationManager() { return localizationManager.get(); }
    NpcManager* getNpcManager() { return npcManager.get(); }
    QuestManager* getQuestManager() { return questManager.get(); }
    CombatManager* getCombatManager() { return combatManager.get(); }
    SpellManager* getSpellManager() { return spellManager.get(); }
    oblivion::NavMeshManager* getNavMeshManager() { return navMeshManager.get(); }
    ai::AIScheduler* getAIScheduler() { return aiScheduler.get(); }
    WorldManager* getWorldManager() { return worldManager.get(); }
    LauncherScreen* getLauncherScreen() { return launcherScreen.get(); }
    TitleScreen* getTitleScreen() { return titleScreen.get(); }
    QuestUI* getQuestUI() { return questUI.get(); }
    TextRenderer* getTextRenderer() { return textRenderer.get(); }
    DebugHUD* getDebugHUD() { return debugHUD.get(); }
    SettingsUI* getSettingsUI() { return settingsUI.get(); }
    GameConsole* getGameConsole() { return gameConsole.get(); }
    DebugMenu* getDebugMenu() { return debugMenu.get(); }
    NpcDebugVisualizer* getNpcDebugVisualizer() { return npcDebugVisualizer.get(); }
    WorldDebugInfo* getWorldDebugInfo() { return worldDebugInfo.get(); }
    PerformanceGraph* getPerformanceGraph() { return performanceGraph.get(); }
    SaveLoadUI* getSaveLoadUI() { return saveLoadUI.get(); }
    SettingsManager* getSettingsManager() { return settingsManager.get(); }
    PerformanceMonitor* getPerformanceMonitor() { return performanceMonitor.get(); }
    PlayerController* getPlayerController() { return playerController.get(); }
    InventoryManager* getInventoryManager() { return inventoryManager.get(); }
    InventoryUI* getInventoryUI() { return inventoryUI.get(); }
#ifdef AUDIO_SYSTEM_ENABLED
    AudioManager* getAudioManager() { return audioManager.get(); }
#endif

    unsigned int getScreenWidth() const { return screenWidth; }
    unsigned int getScreenHeight() const { return screenHeight; }

    bool isLauncherActive() const { return showLauncher; }
    bool isTitleScreenActive() const { return showTitleScreen; }
    bool isExitRequested() const { return shouldExit; }
    UISystem* getUISystem() { return uiSystem.get(); }

    // Phase 9.1: Map System
    map::MapSystem* getMapSystem() { return mapSystem.get(); }
    void toggleMap();
    bool isMapVisible() const { return mapUI && mapUI->isVisible(); }

    // Phase 9.2: Inventory System
    inventory::InventoryGrid* getInventoryGrid() { return inventoryGrid.get(); }
    inventory::EquipmentManager* getEquipmentManager() { return equipmentManager.get(); }
    void toggleInventory();
    bool isInventoryVisible() const { return uiInventoryPanel && uiInventoryPanel->isVisible(); }

    // Debug System Toggles
    void toggleGameConsole();
    void toggleDebugMenu();
    bool isDebugMenuVisible() const;
    void toggleNpcDebugVisualizer();
    void toggleWorldDebugInfo();
    void togglePerformanceGraph();
    void toggleAllDebugSystems();

    // Phase 31: World Entity System
    WorldLoader* getWorldLoader() { return worldLoader.get(); }
    std::vector<WorldEntity>& getWorldEntities() { return worldEntities; }

    // FPS Control
    void setTargetFPS(int fps);
    int getTargetFPS() const { return targetFPS; }

    // Input Handling
    void onTouchEvent(int pointerId, float x, float y, int action);

    // Touch state tracking
    struct TouchState {
        float lastX = 0.0f;
        float lastY = 0.0f;
        bool active = false;
    };
    std::unordered_map<int, TouchState> touchStates;

    // Save/Load
    bool saveGameState(const std::string& slotName);
    bool loadGameState(const std::string& slotName);
    SaveManager* getSaveManager() { return saveManager.get(); }

    // Retro Filter
    RetroFilter::Settings* getRetroSettings() { return &retroSettings; }
    RetroFilter::Settings& getRetroSettingsRef() { return retroSettings; }
    RetroFilter* getRetroFilter() { return retroFilter.get(); }

    // Debug Rendering Modes
    void setWireframeMode(bool enabled) { wireframeMode = enabled; }
    bool isWireframeMode() const { return wireframeMode; }
    void setAabbVisualization(bool enabled) { aabbVisualization = enabled; }
    bool isAabbVisualization() const { return aabbVisualization; }
    void setNpcOverlay(bool enabled) { npcOverlay = enabled; }
    bool isNpcOverlay() const { return npcOverlay; }
    void setTouchTrail(bool enabled) { touchTrail = enabled; }
    bool isTouchTrail() const { return touchTrail; }

    // Debug HUD page navigation
    void debugHudNextPage();
    void debugHudPrevPage();

private:
    bool wireframeMode = false;
    bool aabbVisualization = false;
    bool npcOverlay = false;
    bool touchTrail = false;

    void initLocalization();
    bool initGameSystems();
    void createTestScenario();
};
