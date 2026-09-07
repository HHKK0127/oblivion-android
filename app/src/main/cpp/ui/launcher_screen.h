#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <android/log.h>
#include "../localization/localization_manager.h"
#include "ui_panel.h"
#include "ui_button.h"

#define LOG_TAG "LauncherScreen"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class TextRenderer;
class SettingsManager;
class Renderer;

enum class LauncherState {
    MAIN,           // Main launcher
    OPTIONS,        // Options sub-screen
    DATA_FILES,     // Data Files (plugin management)
    SUPPORT,        // Support info display
    TRANSITIONING   // Transitioning to game
};

class LauncherScreen {
public:
    using OnPlayCallback = std::function<void()>;
    using OnExitCallback = std::function<void()>;

private:
    LauncherState state;
    int screenWidth = 1920;
    int screenHeight = 1080;
    LocalizationManager* localizationManager = nullptr;
    TextRenderer* textRenderer = nullptr;
    SettingsManager* settingsManager = nullptr;
    Renderer* renderer = nullptr;

    // UI
    std::shared_ptr<UIPanel> mainPanel;
    std::vector<std::shared_ptr<UIButton>> menuButtons;
    std::shared_ptr<UIPanel> optionsPanel;
    std::shared_ptr<UIPanel> dataFilesPanel;
    std::shared_ptr<UIButton> supportBackBtn;

    // Textures
    GLuint bgTexture = 0;
    GLuint logoTexture = 0;
    GLuint buttonBgTex = 0;
    GLuint buttonHoverTex = 0;
    bool texturesLoaded = false;

    // Plugin data for DataFiles screen
    struct PluginInfo {
        std::string name;
        bool enabled;
    };
    std::vector<PluginInfo> plugins;
    bool pluginsInitialized = false;

    // Callbacks
    OnPlayCallback onPlayCallback;
    OnExitCallback onExitCallback;

    // Selection
    int selectedIndex = 0;
    float glowPhase = 0.0f;

    // Animation
    float fadeInAlpha = 0.0f;
    float displayTimer = 0.0f;

    // Menu items (authentic original)
    static constexpr int BTN_PLAY       = 0;
    static constexpr int BTN_OPTIONS    = 1;
    static constexpr int BTN_DATA_FILES = 2;
    static constexpr int BTN_SUPPORT    = 3;
    static constexpr int BTN_EXIT       = 4;

    // Original colors (dark stone/metal)
    const glm::vec3 COLOR_STONE       = glm::vec3(0.55f, 0.50f, 0.42f);
    const glm::vec3 COLOR_GOLD_DIM    = glm::vec3(0.65f, 0.55f, 0.30f);
    const glm::vec3 COLOR_GOLD_BRIGHT = glm::vec3(0.90f, 0.78f, 0.45f);
    const glm::vec3 COLOR_DARK_BG     = glm::vec3(0.06f, 0.05f, 0.04f);

public:
    LauncherScreen();
    ~LauncherScreen();

    void initialize(LocalizationManager* lm, TextRenderer* tr,
                    SettingsManager* sm = nullptr, Renderer* rend = nullptr);
    void update(float deltaTime);
    void render();
    void onTouchEvent(float x, float y, int action);
    void onKeyPress(int key);

    void setScreenSize(int w, int h);
    void setOnPlayCallback(OnPlayCallback cb) { onPlayCallback = std::move(cb); }
    void setOnExitCallback(OnExitCallback cb) { onExitCallback = std::move(cb); }

    bool isTransitioning() const { return state == LauncherState::TRANSITIONING; }
    float getDisplayTimer() const { return displayTimer; }

private:
    void buildMainMenu();
    void rebuildLayout();
    void handleSelection();

    void renderMain();
    void renderOptions();
    void renderDataFiles();
    void renderSupport();
    void renderFadeToGame();

    void renderBackground();
    void renderLogo();
};
