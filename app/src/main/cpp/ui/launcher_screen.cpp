#include "launcher_screen.h"
#include "text_renderer.h"
#include "../engine/texture_loader.h"
#include "../system/settings_manager.h"
#include "../engine/renderer.h"
#include "ui_draw_helper.h"
#include <GLES3/gl3.h>
#include <cmath>

LauncherScreen::LauncherScreen()
    : state(LauncherState::MAIN),
      localizationManager(nullptr), textRenderer(nullptr), selectedIndex(0) {
    LOGD("LauncherScreen created (Oblivion Authentic Launcher)");
}

LauncherScreen::~LauncherScreen() {
    TextureLoader::deleteTexture(bgTexture);
    TextureLoader::deleteTexture(logoTexture);
    TextureLoader::deleteTexture(buttonBgTex);
    TextureLoader::deleteTexture(buttonHoverTex);
    LOGD("LauncherScreen destroyed");
}

void LauncherScreen::initialize(LocalizationManager* lm, TextRenderer* tr,
                                SettingsManager* sm, Renderer* rend) {
    localizationManager = lm;
    textRenderer = tr;
    settingsManager = sm;
    renderer = rend;

    selectedIndex = 0;
    fadeInAlpha = 0.0f;
    displayTimer = 0.0f;
    state = LauncherState::MAIN;

    buildMainMenu();

    if (!texturesLoaded) {
        // Original: dark stone/metal background
        bgTexture = TextureLoader::loadTextureFromAsset("textures/ui/launcher_bg.png");
        // Right side Oblivion logo (large)
        logoTexture = TextureLoader::loadTextureFromAsset("textures/ui/oblivion_logo_large.png");
        // Button background (stone style)
        buttonBgTex = TextureLoader::loadTextureFromAsset("textures/ui/btn_stone.png");
        // Hover state (bright stone style)
        buttonHoverTex = TextureLoader::loadTextureFromAsset("textures/ui/btn_stone_hover.png");

        texturesLoaded = true;
        LOGI("Launcher textures loaded");
    }

    LOGI("LauncherScreen initialized");
}

void LauncherScreen::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
    rebuildLayout();
}

void LauncherScreen::buildMainMenu() {
    mainPanel = std::make_shared<UIPanel>("LauncherMainPanel");
    mainPanel->initialize();
    mainPanel->setTitle("");
    mainPanel->setTitleBarHeight(0.0f);
    mainPanel->setCloseButtonVisible(false);
    mainPanel->setDraggable(false);
    mainPanel->setBackgroundColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    mainPanel->setBorderColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    mainPanel->setBorderWidth(0.0f);

    struct BtnInfo { int index; std::string labelKey; };
    BtnInfo infos[] = {
        {BTN_PLAY,       "launcher_play"},
        {BTN_OPTIONS,    "launcher_options"},
        {BTN_DATA_FILES, "launcher_data_files"},
        {BTN_SUPPORT,    "launcher_support"},
        {BTN_EXIT,       "launcher_exit"}
    };

    for (const auto& info : infos) {
        auto btn = std::make_shared<UIButton>("LauncherBtn" + std::to_string(info.index));
        btn->initialize();
        std::string label = localizationManager ? localizationManager->getString(info.labelKey) : info.labelKey;
        btn->setLabel(label);
        btn->setTextRenderer(textRenderer);

        // Original: large buttons (TV/console friendly)
        btn->setSize(380.0f, 72.0f);
        btn->setLabelScale(1.6f);

        // Label color: dark gold (unselected)
        btn->setLabelColor(COLOR_GOLD_DIM);

        // Button background: stone texture or dark semi-transparent
        if (buttonBgTex != 0) {
            btn->setNormalTexture(buttonBgTex);
            btn->setHoverTexture(buttonHoverTex);
        } else {
            btn->setNormalColor(glm::vec4(0.12f, 0.10f, 0.08f, 0.85f));
            btn->setHoverColor(glm::vec4(0.20f, 0.17f, 0.12f, 0.90f));
            btn->setPressedColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.95f));
        }

        int idx = info.index;
        btn->setOnClick([this, idx]() {
            selectedIndex = idx;
            handleSelection();
        });

        menuButtons.push_back(btn);
        mainPanel->addChild(btn);
    }

    rebuildLayout();
}

void LauncherScreen::rebuildLayout() {
    if (!mainPanel) return;
    mainPanel->setScreenSize(screenWidth, screenHeight);

    // Original: panel on left, logo on right
    float panelW = 450.0f;
    float panelH = 520.0f;
    float px = screenWidth * 0.06f;   // Left-aligned
    float py = screenHeight * 0.18f;  // Slightly upper
    mainPanel->setPosition(px, py);
    mainPanel->setSize(panelW, panelH);

    float btnW = 400.0f;
    float btnH = 72.0f;
    float startY = 20.0f;
    float gap = 18.0f;

    for (size_t i = 0; i < menuButtons.size(); ++i) {
        float bx = 25.0f;
        float by = startY + static_cast<float>(i) * (btnH + gap);
        menuButtons[i]->setPosition(bx, by);
        menuButtons[i]->setSize(btnW, btnH);
        menuButtons[i]->setScreenSize(screenWidth, screenHeight);
    }
}

void LauncherScreen::update(float deltaTime) {
    displayTimer += deltaTime;
    glowPhase += deltaTime * 2.2f;

    // Fade in
    if (fadeInAlpha < 1.0f) {
        fadeInAlpha += deltaTime * 1.5f;
        if (fadeInAlpha > 1.0f) fadeInAlpha = 1.0f;
    }

    // Selection highlight animation
    for (size_t i = 0; i < menuButtons.size(); ++i) {
        bool isSelected = (static_cast<int>(i) == selectedIndex);
        if (isSelected) {
            float glow = 0.5f + 0.5f * sin(glowPhase);
            glm::vec3 c(COLOR_GOLD_DIM.x + (COLOR_GOLD_BRIGHT.x - COLOR_GOLD_DIM.x) * glow,
                        COLOR_GOLD_DIM.y + (COLOR_GOLD_BRIGHT.y - COLOR_GOLD_DIM.y) * glow,
                        COLOR_GOLD_DIM.z + (COLOR_GOLD_BRIGHT.z - COLOR_GOLD_DIM.z) * glow);
            menuButtons[i]->setLabelColor(c);
        } else {
            glm::vec3 dimmed(COLOR_GOLD_DIM.x * 0.6f, COLOR_GOLD_DIM.y * 0.6f, COLOR_GOLD_DIM.z * 0.6f);
            menuButtons[i]->setLabelColor(dimmed);
        }
    }
}

void LauncherScreen::render() {
    switch (state) {
        case LauncherState::MAIN:
            renderMain();
            break;
        case LauncherState::OPTIONS:
            renderOptions();
            break;
        case LauncherState::DATA_FILES:
            renderDataFiles();
            break;
        case LauncherState::SUPPORT:
            renderSupport();
            break;
        case LauncherState::TRANSITIONING:
            renderFadeToGame();
            break;
    }
}

// ============================================================================
// Main launcher screen (original: left buttons / right logo)
// ============================================================================
void LauncherScreen::renderMain() {
    glClearColor(COLOR_DARK_BG.x, COLOR_DARK_BG.y, COLOR_DARK_BG.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderBackground();
    renderLogo();

    if (mainPanel) {
        mainPanel->render();
    }

    // Fade in overlay
    if (fadeInAlpha < 1.0f) {
        UIDrawHelper::drawColoredQuad(
            0.0f, 0.0f,
            static_cast<float>(screenWidth), static_cast<float>(screenHeight),
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f - fadeInAlpha),
            screenWidth, screenHeight);
    }
}

// ============================================================================
// Background (dark stone/metal)
// ============================================================================
void LauncherScreen::renderBackground() {
    if (bgTexture != 0) {
        UIDrawHelper::drawTexturedQuad(
            0.0f, 0.0f,
            static_cast<float>(screenWidth), static_cast<float>(screenHeight),
            bgTexture, glm::vec4(1.0f, 1.0f, 1.0f, 0.4f),
            screenWidth, screenHeight);
    } else {
        // Fallback: dark gradient
        UIDrawHelper::drawColoredQuad(
            0.0f, 0.0f,
            static_cast<float>(screenWidth), static_cast<float>(screenHeight),
            glm::vec4(0.06f, 0.05f, 0.04f, 1.0f),
            screenWidth, screenHeight);
    }

    // Original: thin panel background on left side (button area)
    float panelX = screenWidth * 0.04f;
    float panelY = screenHeight * 0.12f;
    float panelW = 480.0f;
    float panelH = 560.0f;
    UIDrawHelper::drawColoredQuad(
        panelX, panelY, panelW, panelH,
        glm::vec4(0.04f, 0.03f, 0.02f, 0.65f),
        screenWidth, screenHeight);

    // Border (thin gold)
    UIDrawHelper::drawBorder(
        panelX, panelY, panelW, panelH, 2.0f,
        glm::vec4(COLOR_GOLD_DIM.x, COLOR_GOLD_DIM.y, COLOR_GOLD_DIM.z, 0.3f),
        screenWidth, screenHeight);
}

// ============================================================================
// Right side logo (large)
// ============================================================================
void LauncherScreen::renderLogo() {
    if (logoTexture == 0) return;

    // Right side center placement
    float scaleFactor = (screenWidth > screenHeight) ? 0.45f : 0.75f;
    float logoW = static_cast<float>(screenWidth) * scaleFactor;
    float logoH = logoW * 0.22f;
    float logoX = static_cast<float>(screenWidth) * 0.58f;
    float logoY = (static_cast<float>(screenHeight) - logoH) * 0.45f;

    UIDrawHelper::drawTexturedQuad(
        logoX, logoY, logoW, logoH,
        logoTexture, glm::vec4(1.0f, 1.0f, 1.0f, fadeInAlpha),
        screenWidth, screenHeight);
}

// ============================================================================
// Options screen (quality settings etc.)
// ============================================================================
void LauncherScreen::renderOptions() {
    glClearColor(COLOR_DARK_BG.x, COLOR_DARK_BG.y, COLOR_DARK_BG.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderBackground();

    if (!optionsPanel) {
        optionsPanel = std::make_shared<UIPanel>("OptionsPanel");
        optionsPanel->initialize();
        optionsPanel->setTitle(localizationManager ? localizationManager->getString("launcher_options") : "Options");
        optionsPanel->setScreenSize(screenWidth, screenHeight);
        optionsPanel->setPosition(screenWidth * 0.15f, screenHeight * 0.1f);
        optionsPanel->setSize(screenWidth * 0.7f, screenHeight * 0.8f);
        optionsPanel->setBackgroundColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.92f));
        optionsPanel->setBorderColor(glm::vec4(0.65f, 0.55f, 0.30f, 0.5f));
        optionsPanel->setBorderWidth(2.0f);

        // Option items
        struct OptionItem { std::string label; std::string key; };
        std::vector<OptionItem> options = {
            {"Graphics Quality", "graphics"},
            {"Resolution", "resolution"},
            {"Audio Volume", "audio"},
            {"Language", "language"},
            {"Controls", "controls"},
        };

        float btnY = 60.0f;
        float btnH = 50.0f;
        float btnGap = 10.0f;
        float panelW = optionsPanel->getSize().x;

        for (size_t i = 0; i < options.size(); i++) {
            auto optBtn = std::make_shared<UIButton>("OptBtn_" + options[i].key);
            optBtn->initialize();
            optBtn->setLabel(options[i].label);
            optBtn->setTextRenderer(textRenderer);
            optBtn->setSize(panelW - 40.0f, btnH);
            optBtn->setLabelScale(1.0f);
            optBtn->setLabelColor(COLOR_GOLD_DIM);
            optBtn->setNormalColor(glm::vec4(0.15f, 0.13f, 0.10f, 0.9f));
            optBtn->setHoverColor(glm::vec4(0.25f, 0.22f, 0.15f, 0.95f));
            optBtn->setPressedColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.95f));
            optBtn->setPosition(20.0f, btnY);

            std::string key = options[i].key;
            optBtn->setOnClick([this, key]() {
                LOGI("Option selected: %s", key.c_str());

                if (key == "graphics") {
                    // Toggle graphics quality: Low -> Medium -> High
                    if (renderer) {
                        auto& retro = renderer->getRetroSettingsRef();
                        if (!retro.pixelation_enabled && !retro.scanlines_enabled) {
                            // Low -> Medium: enable pixelation
                            retro.pixelation_enabled = true;
                            LOGI("Graphics: Medium (pixelation ON)");
                        } else if (retro.pixelation_enabled && !retro.scanlines_enabled) {
                            // Medium -> High: enable scanlines too
                            retro.scanlines_enabled = true;
                            LOGI("Graphics: High (pixelation+scanlines ON)");
                        } else {
                            // High -> Low: disable all
                            retro.pixelation_enabled = false;
                            retro.scanlines_enabled = false;
                            retro.color_reduction_enabled = false;
                            retro.crt_distortion_enabled = false;
                            retro.grain_enabled = false;
                            LOGI("Graphics: Low (all filters OFF)");
                        }
                    }
                } else if (key == "resolution") {
                    // Show current resolution info
                    LOGI("Resolution: %dx%d", screenWidth, screenHeight);
                } else if (key == "audio") {
                    // Toggle audio on/off
                    if (renderer) {
                        // Audio toggle not directly available, log info
                        LOGI("Audio settings: Use in-game settings for volume control");
                    }
                } else if (key == "language") {
                    // Toggle language
                    if (settingsManager) {
                        std::string current = settingsManager->getLanguage();
                        std::string newLang = (current == "ja") ? "en" : "ja";
                        settingsManager->setLanguage(newLang);
                        LOGI("Language changed to: %s", newLang.c_str());
                        // Rebuild menu to update labels
                        optionsPanel.reset();
                    }
                } else if (key == "controls") {
                    LOGI("Controls: Joystick=Move, ATK=Attack, BLK=Block, MAG=Magic, F1-F4=QuickSlots");
                }
            });

            optionsPanel->addChild(optBtn);
            btnY += btnH + btnGap;
        }

        // Back button
        auto backBtn = std::make_shared<UIButton>("OptionsBackBtn");
        backBtn->initialize();
        backBtn->setLabel("Back");
        backBtn->setTextRenderer(textRenderer);
        backBtn->setSize(160.0f, 50.0f);
        backBtn->setLabelScale(1.2f);
        backBtn->setLabelColor(COLOR_GOLD_DIM);
        backBtn->setNormalColor(glm::vec4(0.15f, 0.13f, 0.10f, 0.9f));
        backBtn->setHoverColor(glm::vec4(0.25f, 0.22f, 0.15f, 0.95f));
        backBtn->setPosition(optionsPanel->getSize().x - 180.0f, optionsPanel->getSize().y - 70.0f);
        backBtn->setOnClick([this]() {
            state = LauncherState::MAIN;
        });
        optionsPanel->addChild(backBtn);
    }

    if (optionsPanel) optionsPanel->render();
}

// ============================================================================
// Data Files screen (plugin management)
// ============================================================================
void LauncherScreen::renderDataFiles() {
    glClearColor(COLOR_DARK_BG.x, COLOR_DARK_BG.y, COLOR_DARK_BG.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderBackground();

    // Initialize plugin list once
    if (!pluginsInitialized) {
        plugins = {
            {"Oblivion.esm", true},
            {"Oblivion - Meshes.bsa", true},
            {"Oblivion - Textures.bsa", true},
            {"Oblivion - Sounds.bsa", true},
            {"Oblivion - Voices.bsa", true},
            {"Oblivion - Misc.bsa", true},
        };
        pluginsInitialized = true;
    }

    if (!dataFilesPanel) {
        dataFilesPanel = std::make_shared<UIPanel>("DataFilesPanel");
        dataFilesPanel->initialize();
        dataFilesPanel->setTitle(localizationManager ? localizationManager->getString("launcher_data_files") : "Data Files");
        dataFilesPanel->setScreenSize(screenWidth, screenHeight);
        dataFilesPanel->setPosition(screenWidth * 0.1f, screenHeight * 0.1f);
        dataFilesPanel->setSize(screenWidth * 0.8f, screenHeight * 0.8f);
        dataFilesPanel->setBackgroundColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.92f));
        dataFilesPanel->setBorderColor(glm::vec4(0.65f, 0.55f, 0.30f, 0.5f));
        dataFilesPanel->setBorderWidth(2.0f);

        float panelW = dataFilesPanel->getSize().x;
        float panelH = dataFilesPanel->getSize().y;

        // Title: "Loaded Archives"
        float titleY = 50.0f;

        float listY = titleY + 40.0f;
        float itemH = 45.0f;
        float itemGap = 5.0f;

        for (size_t i = 0; i < plugins.size(); i++) {
            auto pluginBtn = std::make_shared<UIButton>("PluginBtn_" + std::to_string(i));
            pluginBtn->initialize();

            // Checkbox style: [X] or [ ] prefix
            std::string prefix = plugins[i].enabled ? "[X] " : "[ ] ";
            pluginBtn->setLabel(prefix + plugins[i].name);
            pluginBtn->setTextRenderer(textRenderer);
            pluginBtn->setSize(panelW - 40.0f, itemH);
            pluginBtn->setLabelScale(0.9f);
            pluginBtn->setLabelColor(plugins[i].enabled ? COLOR_GOLD_BRIGHT : COLOR_GOLD_DIM);
            pluginBtn->setNormalColor(glm::vec4(0.12f, 0.10f, 0.08f, 0.85f));
            pluginBtn->setHoverColor(glm::vec4(0.20f, 0.17f, 0.12f, 0.90f));
            pluginBtn->setPressedColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.95f));
            pluginBtn->setPosition(20.0f, listY);

            // Toggle plugin enabled state using member variable
            size_t idx = i;
            pluginBtn->setOnClick([this, idx]() {
                if (idx < plugins.size()) {
                    plugins[idx].enabled = !plugins[idx].enabled;
                    LOGI("Plugin %s: %s", plugins[idx].name.c_str(),
                         plugins[idx].enabled ? "enabled" : "disabled");
                    // Rebuild panel to update labels
                    dataFilesPanel.reset();
                }
            });

            dataFilesPanel->addChild(pluginBtn);
            listY += itemH + itemGap;
        }

        // Info text at bottom
        // (Using button as text container since we don't have standalone text in panels)
        auto infoBtn = std::make_shared<UIButton>("DataFilesInfo");
        infoBtn->initialize();
        infoBtn->setLabel("Toggle plugins to enable/disable. Changes apply on next launch.");
        infoBtn->setTextRenderer(textRenderer);
        infoBtn->setSize(panelW - 40.0f, 35.0f);
        infoBtn->setLabelScale(0.7f);
        infoBtn->setLabelColor(glm::vec3(0.5f, 0.5f, 0.5f));
        infoBtn->setNormalColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent
        infoBtn->setHoverColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        infoBtn->setPressedColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        infoBtn->setPosition(20.0f, panelH - 120.0f);
        dataFilesPanel->addChild(infoBtn);

        // Back button
        auto backBtn = std::make_shared<UIButton>("DataFilesBackBtn");
        backBtn->initialize();
        backBtn->setLabel("Back");
        backBtn->setTextRenderer(textRenderer);
        backBtn->setSize(160.0f, 50.0f);
        backBtn->setLabelScale(1.2f);
        backBtn->setLabelColor(COLOR_GOLD_DIM);
        backBtn->setNormalColor(glm::vec4(0.15f, 0.13f, 0.10f, 0.9f));
        backBtn->setHoverColor(glm::vec4(0.25f, 0.22f, 0.15f, 0.95f));
        backBtn->setPosition(panelW - 180.0f, panelH - 70.0f);
        backBtn->setOnClick([this]() {
            state = LauncherState::MAIN;
        });
        dataFilesPanel->addChild(backBtn);
    }

    if (dataFilesPanel) dataFilesPanel->render();
}

// ============================================================================
// Support screen
// ============================================================================
void LauncherScreen::renderSupport() {
    glClearColor(COLOR_DARK_BG.x, COLOR_DARK_BG.y, COLOR_DARK_BG.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderBackground();

    // Support info panel
    float panelX = screenWidth * 0.15f;
    float panelY = screenHeight * 0.1f;
    float panelW = screenWidth * 0.7f;
    float panelH = screenHeight * 0.8f;

    // Panel background
    UIDrawHelper::drawColoredQuad(
        panelX, panelY, panelW, panelH,
        glm::vec4(0.08f, 0.07f, 0.05f, 0.92f),
        screenWidth, screenHeight);

    // Border
    UIDrawHelper::drawBorder(
        panelX, panelY, panelW, panelH, 2.0f,
        glm::vec4(0.65f, 0.55f, 0.30f, 0.5f),
        screenWidth, screenHeight);

    // Title
    float textX = panelX + 30.0f;
    float textY = panelY + 50.0f;
    float lineH = 35.0f;

    if (textRenderer) {
        textRenderer->renderText("Oblivion Android", textX, textY, COLOR_GOLD_BRIGHT, 1.5f);
        textY += lineH * 1.5f;

        textRenderer->renderText("Version 0.9.10 (Phase 36)", textX, textY, COLOR_GOLD_DIM, 1.0f);
        textY += lineH;

        textRenderer->renderText("The Elder Scrolls IV: Oblivion", textX, textY, COLOR_GOLD_DIM, 1.0f);
        textY += lineH;

        textRenderer->renderText("Native Android Port", textX, textY, COLOR_GOLD_DIM, 1.0f);
        textY += lineH * 2.0f;

        textRenderer->renderText("Technical Details:", textX, textY, COLOR_GOLD_BRIGHT, 1.2f);
        textY += lineH;

        textRenderer->renderText("- C++17 / OpenGL ES 3.0", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- Android NDK r26.1", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- JNI Bridge Architecture", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- OpenAL-Soft Audio", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH * 2.0f;

        textRenderer->renderText("Controls:", textX, textY, COLOR_GOLD_BRIGHT, 1.2f);
        textY += lineH;

        textRenderer->renderText("- Joystick: Move character", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- ATK/BLK/MAG: Combat actions", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- F1-F4: Quick spell slots", textX, textY, COLOR_GOLD_DIM, 0.9f);
        textY += lineH;

        textRenderer->renderText("- Debug Menu: Toggle with FAB", textX, textY, COLOR_GOLD_DIM, 0.9f);
    }

    // Back button (using UIButton)
    if (!supportBackBtn) {
        supportBackBtn = std::make_shared<UIButton>("SupportBackBtn");
        supportBackBtn->initialize();
        supportBackBtn->setLabel("Back");
        supportBackBtn->setTextRenderer(textRenderer);
        supportBackBtn->setSize(160.0f, 50.0f);
        supportBackBtn->setLabelScale(1.2f);
        supportBackBtn->setLabelColor(COLOR_GOLD_DIM);
        supportBackBtn->setNormalColor(glm::vec4(0.15f, 0.13f, 0.10f, 0.9f));
        supportBackBtn->setHoverColor(glm::vec4(0.25f, 0.22f, 0.15f, 0.95f));
        supportBackBtn->setPressedColor(glm::vec4(0.08f, 0.07f, 0.05f, 0.95f));
        supportBackBtn->setOnClick([this]() {
            state = LauncherState::MAIN;
        });
    }
    supportBackBtn->setPosition(panelX + panelW - 180.0f, panelY + panelH - 70.0f);
    supportBackBtn->setScreenSize(screenWidth, screenHeight);
    supportBackBtn->render();
}

// ============================================================================
// Game launch fade
// ============================================================================
void LauncherScreen::renderFadeToGame() {
    float fade = displayTimer * 1.5f;
    if (fade > 1.0f) fade = 1.0f;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// ============================================================================
// Input handling
// ============================================================================
void LauncherScreen::onTouchEvent(float x, float y, int action) {
    LOGI("LauncherScreen::onTouchEvent: action=%d, pos=(%.1f, %.1f), state=%d",
         action, x, y, static_cast<int>(state));
    if (state == LauncherState::MAIN) {
        if (action == 0 || action == 5) { // TOUCH_DOWN
            if (mainPanel && mainPanel->onTouchDown(x, y, 0)) return;

            // Fallback: direct hit-test on DOWN for immediate response
            float menuTop = screenHeight * 0.18f;
            float menuLeft = screenWidth * 0.06f;
            float menuWidth = 450.0f;
            float itemH = 90.0f;
            int numButtons = static_cast<int>(menuButtons.size());
            for (int i = 0; i < numButtons; ++i) {
                float itemY = menuTop + static_cast<float>(i) * itemH;
                if (x >= menuLeft && x <= menuLeft + menuWidth &&
                    y >= itemY && y < itemY + itemH) {
                    selectedIndex = i;
                    handleSelection();
                    return;
                }
            }
        } else if (action == 1 || action == 6) { // TOUCH_UP
            if (mainPanel && mainPanel->onTouchUp(x, y, 0)) return;
        } else if (action == 2) { // TOUCH_MOVE
            if (mainPanel && mainPanel->onTouchMove(x, y, 0.0f, 0.0f, 0)) return;
        } else if (action == 3) { // TOUCH_CANCEL
            if (mainPanel && mainPanel->onTouchUp(x, y, 0)) return;
        }
    } else if (state == LauncherState::OPTIONS) {
        if (action == 0 || action == 5) {
            if (optionsPanel && optionsPanel->onTouchDown(x, y, 0)) return;
        } else if (action == 1 || action == 6) {
            if (optionsPanel && optionsPanel->onTouchUp(x, y, 0)) return;
        } else if (action == 2) {
            if (optionsPanel && optionsPanel->onTouchMove(x, y, 0.0f, 0.0f, 0)) return;
        } else if (action == 3) { // TOUCH_CANCEL
            if (optionsPanel && optionsPanel->onTouchUp(x, y, 0)) return;
        }
    } else if (state == LauncherState::DATA_FILES) {
        if (action == 0 || action == 5) {
            if (dataFilesPanel && dataFilesPanel->onTouchDown(x, y, 0)) return;
        } else if (action == 1 || action == 6) {
            if (dataFilesPanel && dataFilesPanel->onTouchUp(x, y, 0)) return;
        } else if (action == 2) {
            if (dataFilesPanel && dataFilesPanel->onTouchMove(x, y, 0.0f, 0.0f, 0)) return;
        } else if (action == 3) { // TOUCH_CANCEL
            if (dataFilesPanel && dataFilesPanel->onTouchUp(x, y, 0)) return;
        }
    }
}

void LauncherScreen::onKeyPress(int key) {
    if (state == LauncherState::MAIN) {
        int numButtons = static_cast<int>(menuButtons.size());
        if (numButtons == 0) numButtons = 5; // Safety fallback

        switch (key) {
            case 19: // UP
                selectedIndex = (selectedIndex - 1 + numButtons) % numButtons;
                break;
            case 20: // DOWN
                selectedIndex = (selectedIndex + 1) % numButtons;
                break;
            case 23: // ENTER
            case 66: // DPAD_CENTER
                handleSelection();
                break;
        }
    } else {
        // Sub-screens: ESC/Back to return
        if (key == 4) { // BACK
            state = LauncherState::MAIN;
        }
    }
}

void LauncherScreen::handleSelection() {
    switch (selectedIndex) {
        case BTN_PLAY: {
            state = LauncherState::TRANSITIONING;
            displayTimer = 0.0f;
            if (onPlayCallback) onPlayCallback();
            LOGI("Launcher: Play selected");
            break;
        }
        case BTN_OPTIONS: {
            state = LauncherState::OPTIONS;
            LOGI("Launcher: Options selected");
            break;
        }
        case BTN_DATA_FILES: {
            state = LauncherState::DATA_FILES;
            LOGI("Launcher: Data Files selected");
            break;
        }
        case BTN_SUPPORT: {
            state = LauncherState::SUPPORT;
            LOGI("Launcher: Support selected");
            break;
        }
        case BTN_EXIT: {
            if (onExitCallback) onExitCallback();
            LOGI("Launcher: Exit selected");
            break;
        }
    }
}
