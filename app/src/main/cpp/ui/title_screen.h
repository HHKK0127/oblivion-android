#pragma once

#include <vector>
#include <string>
#include <memory>
#include <array>
#include <android/log.h>
#include "../localization/localization_manager.h"
#include "settings_ui.h"
#include "ui_panel.h"
#include "ui_button.h"

#define LOG_TAG "TitleScreen"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

class TextRenderer;

enum class TitleScreenState {
    INTRO_MOVIE,    // Oblivion logo fade-in
    LOGO_DISPLAY,   // "Press any key to continue" wait
    MENU,           // Main menu
    OPTIONS,        // Options sub-menu (reserved)
    CREDITS,        // Credits display (reserved)
    TRANSITIONING   // Game start fade-out
};

struct TitleParticle {
    float x, y;
    float size;
    float alpha;
    float driftX;
    float driftY;
    float phase;
};

class TitleScreen {
private:
    TitleScreenState state;
    float displayTimer;
    float bgAnimTime = 0.0f;
    std::vector<std::string> menuItems;
    int selectedIndex;
    bool gameStarted;
    bool settingsRequested;
    bool loadGameRequested;
    bool creditsRequested;
    bool quitRequested = false;
    LocalizationManager* localizationManager;
    std::unique_ptr<SettingsUI> settingsUI;
    TextRenderer* textRenderer = nullptr;

    std::shared_ptr<UIPanel> menuPanel;
    std::vector<std::shared_ptr<UIButton>> menuButtons;
    int screenWidth = 1920;
    int screenHeight = 1080;

    GLuint bgTexture = 0;
    GLuint logoTexture = 0;
    GLuint vignetteTexture = 0;
    bool texturesLoaded = false;

    std::vector<GLuint> movieFrames;
    int currentMovieFrame = 0;
    float movieFrameTime = 0.0f;
    static constexpr float MOVIE_FPS = 30.0f;

    static constexpr int MAX_PARTICLES = 48;
    std::array<TitleParticle, MAX_PARTICLES> particles{};
    bool particlesInitialized = false;

    float glowPhase = 0.0f;
    float logoFadeAlpha = 0.0f;
    float introLogoAlpha = 0.0f;
    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;

    static constexpr float INTRO_DURATION = 4.0f;
    static constexpr float LOGO_FADE_DURATION = 2.0f;

    static constexpr int MENU_NEW     = 0;
    static constexpr int MENU_LOAD    = 1;
    static constexpr int MENU_OPTIONS = 2;
    static constexpr int MENU_CREDITS = 3;
    static constexpr int MENU_QUIT    = 4;

    const glm::vec3 COLOR_PARCHMENT = glm::vec3(0.72f, 0.64f, 0.49f);
    const glm::vec3 COLOR_GOLD      = glm::vec3(0.85f, 0.72f, 0.35f);
    const glm::vec3 COLOR_WHITE     = glm::vec3(1.0f, 1.0f, 1.0f);

public:
    TitleScreen();
    ~TitleScreen();

    void initialize(LocalizationManager* lm, TextRenderer* tr);
    void update(float deltaTime);
    void render();
    void onTouchEvent(float x, float y, int action);
    void debugStartNewGame();
    void onKeyPress(int key);

    bool isGameStarted() const { return gameStarted; }
    bool isSettingsRequested() const { return settingsRequested; }
    void resetSettingsRequest() { settingsRequested = false; }
    bool isLoadGameRequested() const { return loadGameRequested; }
    void resetLoadGameRequest() { loadGameRequested = false; }
    bool isCreditsRequested() const { return creditsRequested; }
    void resetCreditsRequest() { creditsRequested = false; }
    bool isQuitRequested() const { return quitRequested; }
    void resetQuitRequest() { quitRequested = false; }
    TitleScreenState getState() const { return state; }

    void setScreenSize(int w, int h);

private:
    void transitionToLogo();
    void transitionToMenu();
    void updateMenu(float deltaTime);
    void handleMenuSelection();
    void buildGraphicalMenu();
    void rebuildMenuLayout();
    void initParticles();

    void renderIntroMovie();
    void renderLogoDisplay();
    void renderMenu();
    void renderFadeOut();
    void renderBackground(float alpha, bool menuMode);
    void renderSepiaOverlay();
    void renderVignette();
    void renderOblivionLogo(float alpha, bool large);
    void renderPressAnyKey(float alpha);
    void renderVersionText();
    void renderParticles();

    static float easeInQuad(float t) { return t * t; }
    static float easeOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
};
