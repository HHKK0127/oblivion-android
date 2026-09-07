#include "title_screen.h"
#include "text_renderer.h"
#include "../engine/texture_loader.h"
#include "ui_draw_helper.h"
#include <GLES3/gl3.h>
#include <cmath>

TitleScreen::TitleScreen()
    : state(TitleScreenState::INTRO_MOVIE), displayTimer(0.0f),
      selectedIndex(0), gameStarted(false), settingsRequested(false),
      loadGameRequested(false), creditsRequested(false),
      localizationManager(nullptr), textRenderer(nullptr) {
    LOGD("TitleScreen created (Oblivion Authentic Edition)");
}

TitleScreen::~TitleScreen() {
    TextureLoader::deleteTexture(bgTexture);
    TextureLoader::deleteTexture(logoTexture);
    TextureLoader::deleteTexture(vignetteTexture);
    for (auto& tex : movieFrames) {
        TextureLoader::deleteTexture(tex);
    }
    LOGD("TitleScreen destroyed");
}

void TitleScreen::initialize(LocalizationManager* lm, TextRenderer* tr) {
    localizationManager = lm;
    textRenderer = tr;

    menuItems.clear();
    menuItems.push_back("menu_new");
    menuItems.push_back("menu_load");
    menuItems.push_back("menu_options");
    menuItems.push_back("menu_credits");
    menuItems.push_back("menu_quit");

    displayTimer = 0.0f;
    bgAnimTime = 0.0f;
    logoFadeAlpha = 0.0f;
    introLogoAlpha = 0.0f;
    glowPhase = 0.0f;
    state = TitleScreenState::INTRO_MOVIE;
    selectedIndex = 0;
    gameStarted = false;

    buildGraphicalMenu();
    initParticles();

    if (!texturesLoaded) {
        bgTexture = TextureLoader::loadTextureFromAsset("textures/ui/map_loop_01.png");
        logoTexture = TextureLoader::loadTextureFromAsset("textures/ui/oblivion_logo.png");
        vignetteTexture = TextureLoader::loadTextureFromAsset("textures/ui/vignette.png");

        for (int i = 0; i < 30; ++i) {
            char path[128];
            snprintf(path, sizeof(path), "textures/ui/map_loop_%02d.png", i + 1);
            GLuint tex = TextureLoader::loadTextureFromAsset(path);
            if (tex != 0) movieFrames.push_back(tex);
        }

        texturesLoaded = true;
        LOGI("TitleScreen textures: bg=%u logo=%u frames=%zu",
             bgTexture, logoTexture, movieFrames.size());
    }

    LOGI("TitleScreen initialized (Oblivion Authentic)");
}

void TitleScreen::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
    rebuildMenuLayout();
}

void TitleScreen::initParticles() {
    if (particlesInitialized) return;
    srand(42);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].x = static_cast<float>(rand()) / RAND_MAX;
        particles[i].y = static_cast<float>(rand()) / RAND_MAX;
        particles[i].size = 0.8f + (static_cast<float>(rand()) / RAND_MAX) * 2.0f;
        particles[i].alpha = 0.08f + (static_cast<float>(rand()) / RAND_MAX) * 0.25f;
        particles[i].driftX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.0003f;
        particles[i].driftY = -0.0002f - (static_cast<float>(rand()) / RAND_MAX) * 0.0003f;
        particles[i].phase = static_cast<float>(rand()) / RAND_MAX * 6.283f;
    }
    particlesInitialized = true;
}

void TitleScreen::buildGraphicalMenu() {
    menuPanel = std::make_shared<UIPanel>("TitleMenuPanel");
    menuPanel->initialize();
    menuPanel->setTitle("");
    menuPanel->setTitleBarHeight(0.0f);
    menuPanel->setCloseButtonVisible(false);
    menuPanel->setDraggable(false);
    menuPanel->setBackgroundColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    menuPanel->setBorderColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    menuPanel->setBorderWidth(0.0f);

    struct BtnInfo { int index; std::string labelKey; };
    BtnInfo infos[] = {
        {MENU_NEW,     "menu_new"},
        {MENU_LOAD,    "menu_load"},
        {MENU_OPTIONS, "menu_options"},
        {MENU_CREDITS, "menu_credits"},
        {MENU_QUIT,    "menu_quit"}
    };

    for (const auto& info : infos) {
        auto btn = std::make_shared<UIButton>("MenuBtn" + std::to_string(info.index));
        btn->initialize();
        std::string label = localizationManager ? localizationManager->getString(info.labelKey) : info.labelKey;
        btn->setLabel(label);
        btn->setTextRenderer(textRenderer);
        btn->setSize(340.0f, 48.0f);
        btn->setLabelScale(1.4f);
        btn->setLabelColor(COLOR_PARCHMENT);
        btn->setNormalColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        btn->setHoverColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
        btn->setPressedColor(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        int idx = info.index;
        btn->setOnClick([this, idx]() {
            selectedIndex = idx;
            handleMenuSelection();
        });

        menuButtons.push_back(btn);
        menuPanel->addChild(btn);
    }

    rebuildMenuLayout();
}

void TitleScreen::rebuildMenuLayout() {
    if (!menuPanel) return;
    menuPanel->setScreenSize(screenWidth, screenHeight);

    float panelW = 420.0f;
    float panelH = 400.0f;
    float px = screenWidth * 0.04f;
    float py = screenHeight * 0.38f;
    menuPanel->setPosition(px, py);
    menuPanel->setSize(panelW, panelH);

    float btnW = 400.0f;
    float btnH = 48.0f;
    float startY = 20.0f;
    float gap = 12.0f;
    for (size_t i = 0; i < menuButtons.size(); ++i) {
        float bx = 15.0f;
        float by = startY + static_cast<float>(i) * (btnH + gap);
        menuButtons[i]->setPosition(bx, by);
        menuButtons[i]->setSize(btnW, btnH);
        menuButtons[i]->setScreenSize(screenWidth, screenHeight);
    }
}

void TitleScreen::update(float deltaTime) {
    bgAnimTime += deltaTime;
    glowPhase += deltaTime * 2.0f;
    movieFrameTime += deltaTime;

    if (!movieFrames.empty() && movieFrameTime > 1.0f / MOVIE_FPS) {
        currentMovieFrame = (currentMovieFrame + 1) % movieFrames.size();
        movieFrameTime = 0.0f;
    }

    switch (state) {
        case TitleScreenState::INTRO_MOVIE: {
            displayTimer += deltaTime;
            float t = displayTimer / INTRO_DURATION;
            if (t > 1.0f) t = 1.0f;
            introLogoAlpha = easeOutQuad(t);
            if (displayTimer >= INTRO_DURATION) {
                transitionToLogo();
            }
            break;
        }
        case TitleScreenState::LOGO_DISPLAY: {
            displayTimer += deltaTime;
            float t = displayTimer / LOGO_FADE_DURATION;
            if (t > 1.0f) t = 1.0f;
            logoFadeAlpha = easeInQuad(t);
            break;
        }
        case TitleScreenState::MENU: {
            updateMenu(deltaTime);
            break;
        }
        default:
            break;
    }
}

void TitleScreen::render() {
    switch (state) {
        case TitleScreenState::INTRO_MOVIE:
            renderIntroMovie();
            break;
        case TitleScreenState::LOGO_DISPLAY:
            renderLogoDisplay();
            break;
        case TitleScreenState::MENU:
            renderMenu();
            break;
        case TitleScreenState::TRANSITIONING:
            renderFadeOut();
            break;
        default:
            break;
    }
}

void TitleScreen::renderIntroMovie() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (logoTexture != 0) {
        float scale = (screenWidth > screenHeight) ? 0.55f : 0.9f;
        float logoW = screenWidth * scale;
        float logoH = logoW * 0.22f;
        float logoX = (screenWidth - logoW) * 0.5f;
        float logoY = (screenHeight - logoH) * 0.45f;

        UIDrawHelper::drawTexturedQuad(
            logoX, logoY, logoW, logoH,
            logoTexture, glm::vec4(1.0f, 1.0f, 1.0f, introLogoAlpha),
            screenWidth, screenHeight);
    }
}

void TitleScreen::renderLogoDisplay() {
    renderBackground(logoFadeAlpha, false);
    renderSepiaOverlay();
    renderVignette();
    renderParticles();
    renderOblivionLogo(logoFadeAlpha, true);

    if (displayTimer > LOGO_FADE_DURATION + 0.3f) {
        float msgT = displayTimer - (LOGO_FADE_DURATION + 0.3f);
        float msgAlpha = std::min(msgT * 2.0f, 1.0f);
        msgAlpha *= 0.5f + 0.5f * sin(displayTimer * 2.5f);
        renderPressAnyKey(msgAlpha);
    }
}

void TitleScreen::renderMenu() {
    renderBackground(1.0f, true);
    renderSepiaOverlay();
    renderVignette();
    renderParticles();
    renderOblivionLogo(1.0f, false);

    for (size_t i = 0; i < menuButtons.size(); ++i) {
        bool isSelected = (static_cast<int>(i) == selectedIndex);
        if (isSelected) {
            float glow = 0.6f + 0.4f * sin(glowPhase);
            glm::vec3 c(COLOR_PARCHMENT.x + (COLOR_WHITE.x - COLOR_PARCHMENT.x) * glow * 0.6f,
                        COLOR_PARCHMENT.y + (COLOR_WHITE.y - COLOR_PARCHMENT.y) * glow * 0.6f,
                        COLOR_PARCHMENT.z + (COLOR_WHITE.z - COLOR_PARCHMENT.z) * glow * 0.6f);
            menuButtons[i]->setLabelColor(c);
        } else {
            glm::vec3 dimmed(COLOR_PARCHMENT.x * 0.65f, COLOR_PARCHMENT.y * 0.65f, COLOR_PARCHMENT.z * 0.65f);
            menuButtons[i]->setLabelColor(dimmed);
        }
    }

    if (menuPanel) menuPanel->render();
    renderVersionText();
}

void TitleScreen::renderBackground(float alpha, bool menuMode) {
    glClearColor(0.02f, 0.01f, 0.005f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint tex = bgTexture;
    if (!movieFrames.empty() && currentMovieFrame < movieFrames.size()) {
        tex = movieFrames[currentMovieFrame];
    }

    if (tex != 0) {
        float t = bgAnimTime * 0.008f;
        float uMin = 0.03f + 0.02f * sin(t);
        float vMin = 0.03f + 0.02f * cos(t * 0.6f);
        float uMax = uMin + 0.94f;
        float vMax = vMin + 0.94f;

        float bgAlpha = menuMode ? 0.75f : 0.6f;
        UIDrawHelper::drawTexturedQuad(
            0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight),
            tex, glm::vec4(1.0f, 1.0f, 1.0f, alpha * bgAlpha),
            screenWidth, screenHeight,
            uMin, vMin, uMax, vMax);
    }
}

void TitleScreen::renderSepiaOverlay() {
    glm::vec4 sepia(0.44f, 0.26f, 0.08f, 0.15f);
    UIDrawHelper::drawColoredQuad(
        0.0f, 0.0f,
        static_cast<float>(screenWidth), static_cast<float>(screenHeight),
        sepia, screenWidth, screenHeight);
}

void TitleScreen::renderVignette() {
    if (vignetteTexture != 0) {
        UIDrawHelper::drawTexturedQuad(
            0.0f, 0.0f,
            static_cast<float>(screenWidth), static_cast<float>(screenHeight),
            vignetteTexture, glm::vec4(1.0f, 1.0f, 1.0f, 0.6f),
            screenWidth, screenHeight);
    }
}

void TitleScreen::renderOblivionLogo(float alpha, bool large) {
    if (logoTexture == 0) return;

    float scaleFactor = large
        ? ((screenWidth > screenHeight) ? 0.55f : 0.9f)
        : ((screenWidth > screenHeight) ? 0.42f : 0.7f);

    float logoW = static_cast<float>(screenWidth) * scaleFactor;
    float logoH = logoW * 0.20f;
    float logoX = (static_cast<float>(screenWidth) - logoW) * 0.5f;
    float logoY = large
        ? static_cast<float>(screenHeight) * 0.15f
        : static_cast<float>(screenHeight) * 0.08f;

    UIDrawHelper::drawTexturedQuad(
        logoX, logoY, logoW, logoH,
        logoTexture, glm::vec4(1.0f, 1.0f, 1.0f, alpha),
        screenWidth, screenHeight);
}

void TitleScreen::renderPressAnyKey(float alpha) {
    if (!textRenderer) return;

    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 2.0f) scale = 2.0f;

    const char* hint = "Press any key to continue";
    float fontScale = 0.7f * scale;
    float textWidth = textRenderer->getTextWidth(hint, fontScale);
    float textX = (static_cast<float>(screenWidth) - textWidth) * 0.5f;
    float textY = static_cast<float>(screenHeight) * 0.32f;

    glm::vec3 hintColor(COLOR_PARCHMENT.x, COLOR_PARCHMENT.y, COLOR_PARCHMENT.z);
    textRenderer->renderText(hint, textX, textY, hintColor, fontScale);
}

void TitleScreen::renderVersionText() {
    if (!textRenderer) return;

    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 2.0f) scale = 2.0f;

    const char* version = "v1.2.0416";
    float fontScale = 0.5f * scale;
    float textWidth = textRenderer->getTextWidth(version, fontScale);
    float textX = static_cast<float>(screenWidth) - textWidth - 16.0f;
    float textY = static_cast<float>(screenHeight) - 16.0f;

    glm::vec3 verColor(COLOR_PARCHMENT.x * 0.5f, COLOR_PARCHMENT.y * 0.5f, COLOR_PARCHMENT.z * 0.5f);
    textRenderer->renderText(version, textX, textY, verColor, fontScale);
}

void TitleScreen::renderParticles() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        auto& p = particles[i];
        p.x += p.driftX;
        p.y += p.driftY;
        if (p.x < 0) p.x += 1.0f;
        if (p.x > 1) p.x -= 1.0f;
        if (p.y < 0) p.y += 1.0f;
        if (p.y > 1) p.y -= 1.0f;

        float flicker = 0.4f + 0.6f * sin(bgAnimTime * 0.5f + p.phase);
        float a = p.alpha * flicker * 0.25f;
        float px = p.x * screenWidth;
        float py = p.y * screenHeight;

        // Render as small colored quads since drawPoint is not available
        float sz = p.size;
        UIDrawHelper::drawColoredQuad(
            px - sz * 0.5f, py - sz * 0.5f, sz, sz,
            glm::vec4(0.9f, 0.8f, 0.6f, a),
            screenWidth, screenHeight);
    }
}

void TitleScreen::renderFadeOut() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void TitleScreen::onTouchEvent(float x, float y, int action) {
    if (state == TitleScreenState::INTRO_MOVIE) {
        if (action == 0) {
            transitionToLogo();
        }
    } else if (state == TitleScreenState::LOGO_DISPLAY) {
        if (action == 0) {
            transitionToMenu();
        }
    } else if (state == TitleScreenState::MENU) {
        if (menuPanel) {
            if (action == 0) {
                lastTouchX = x;
                lastTouchY = y;
                menuPanel->onTouchDown(x, y, 0);
            } else if (action == 1) {
                menuPanel->onTouchUp(x, y, 0);
            } else if (action == 2) {
                float dx = x - lastTouchX;
                float dy = y - lastTouchY;
                lastTouchX = x;
                lastTouchY = y;
                menuPanel->onTouchMove(x, y, dx, dy, 0);
            }
        }
    }
}

// Debug helper: directly start a new game bypassing the menu UI hit-test.
void TitleScreen::debugStartNewGame() {
    if (state == TitleScreenState::MENU) {
        selectedIndex = MENU_NEW;
        handleMenuSelection();
    }
}

void TitleScreen::onKeyPress(int key) {
    if (state == TitleScreenState::INTRO_MOVIE) {
        transitionToLogo();
    } else if (state == TitleScreenState::LOGO_DISPLAY) {
        transitionToMenu();
    } else if (state == TitleScreenState::MENU) {
        if (key == 19) {
            selectedIndex = (selectedIndex - 1 + static_cast<int>(menuButtons.size())) % static_cast<int>(menuButtons.size());
        } else if (key == 20) {
            selectedIndex = (selectedIndex + 1) % static_cast<int>(menuButtons.size());
        } else if (key == 23 || key == 66) {
            handleMenuSelection();
        }
    }
}

void TitleScreen::transitionToLogo() {
    state = TitleScreenState::LOGO_DISPLAY;
    displayTimer = 0.0f;
    logoFadeAlpha = 0.0f;
    LOGI("Transitioned to logo display");
}

void TitleScreen::transitionToMenu() {
    state = TitleScreenState::MENU;
    displayTimer = 0.0f;
    selectedIndex = 0;
    LOGI("Transitioned to menu");
}

void TitleScreen::updateMenu(float deltaTime) {
    (void)deltaTime;
}

void TitleScreen::handleMenuSelection() {
    if (selectedIndex >= static_cast<int>(menuItems.size())) return;

    const std::string& selected = menuItems[selectedIndex];

    if (selected == "menu_new") {
        state = TitleScreenState::TRANSITIONING;
        gameStarted = true;
        LOGI("Menu selection: New Game");
    } else if (selected == "menu_load") {
        loadGameRequested = true;
        LOGI("Menu selection: Load Game");
    } else if (selected == "menu_options") {
        settingsRequested = true;
        LOGI("Menu selection: Options");
    } else if (selected == "menu_credits") {
        creditsRequested = true;
        LOGI("Menu selection: Credits");
    } else if (selected == "menu_quit") {
        quitRequested = true;
        LOGI("Menu selection: Quit");
    }
}
