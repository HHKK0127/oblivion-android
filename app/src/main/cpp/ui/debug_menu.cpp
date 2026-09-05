#include "debug_menu.h"
#include "game_console.h"
#include "text_renderer.h"
#include "ui_draw_helper.h"
#include <algorithm>
#include <cmath>
#include <android/log.h>

#define LOG_TAG_DEBUG "DebugMenu"
#define LOGI_DEBUG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG_DEBUG, __VA_ARGS__)

DebugMenu::DebugMenu()
    : textRenderer(nullptr), console(nullptr),
      visible(false), initialized(false),
      screenWidth(1080), screenHeight(1920),
      safeLeft(0), safeTop(0), safeRight(160), safeBottom(220),
      currentTab(Tab::PLAYER),
      feedbackTimer(0.0f),
      feedbackColor(0.4f, 0.9f, 0.4f) {}

DebugMenu::~DebugMenu() { cleanup(); }

bool DebugMenu::initialize(TextRenderer* tr, GameConsole* c) {
    if (initialized) return true;
    if (!tr || !c) return false;
    textRenderer = tr;
    console = c;
    createTabButtons();
    createAllTabContents();
    initialized = true;
    LOGI_DEBUG("DebugMenu initialized");
    return true;
}

void DebugMenu::cleanup() {
    tabButtons.clear();
    tabContents.clear();
    initialized = false;
}

void DebugMenu::toggle() {
    visible = !visible;
    touchState = {};
    LOGI_DEBUG("DebugMenu %s", visible ? "opened" : "closed");
}

void DebugMenu::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
    safeRight = std::max(160.0f, w * 0.15f);
    safeBottom = std::max(220.0f, h * 0.18f);
}

// ==================== Touch Event Handling ====================

void DebugMenu::onTouchDown(float x, float y) {
    if (!visible) return;
    touchState.isActive = true;
    touchState.startX = touchState.lastX = x;
    touchState.startY = touchState.lastY = y;
    touchState.pressedButton = nullptr;
    touchState.isScrolling = false;

    LOGI_DEBUG("onTouchDown: touch=(%.1f, %.1f) screen=%dx%d scale=%.2f",
               x, y, screenWidth, screenHeight, getScale());

    // Check tab buttons first
    Button* tabBtn = hitTestTab(x, y);
    if (tabBtn) {
        touchState.pressedButton = tabBtn;
        tabBtn->isPressed = true;
        tabBtn->pressTimer = 0.15f;
        LOGI_DEBUG("Hit tab button: %s at (%.0f, %.0f) size (%.0f x %.0f)",
                   tabBtn->label.c_str(), tabBtn->x, tabBtn->y, tabBtn->w, tabBtn->h);
        return;
    }

    // Check content buttons
    Button* contentBtn = hitTestContent(x, y);
    if (contentBtn) {
        touchState.pressedButton = contentBtn;
        contentBtn->isPressed = true;
        contentBtn->pressTimer = 0.15f;
        LOGI_DEBUG("Hit content button: '%s' at (%.0f, %.0f) size (%.0f x %.0f)",
                   contentBtn->label.c_str(), contentBtn->x, contentBtn->y,
                   contentBtn->w, contentBtn->h);
        return;
    }

    // No button hit - start scrolling
    LOGI_DEBUG("No button hit at (%.1f, %.1f) - starting scroll", x, y);
    touchState.isScrolling = true;
}

void DebugMenu::onTouchMove(float x, float y) {
    if (!touchState.isActive) return;

    float dx = x - touchState.startX;
    float dy = y - touchState.startY;
    float dist = std::hypot(dx, dy);

    // If moved beyond threshold, switch to scrolling mode
    if (!touchState.isScrolling && dist > TouchState::SCROLL_THRESHOLD) {
        touchState.isScrolling = true;
        if (touchState.pressedButton) {
            touchState.pressedButton->isPressed = false;
            touchState.pressedButton = nullptr;
        }
    }

    // Handle scrolling
    if (touchState.isScrolling) {
        float moveY = y - touchState.lastY;
        size_t idx = static_cast<size_t>(currentTab);
        if (idx < tabContents.size()) {
            tabContents[idx].scrollOffset -= moveY;
            clampScrollOffsets();
        }
    }

    touchState.lastX = x;
    touchState.lastY = y;
}

void DebugMenu::onTouchUp(float x, float y) {
    LOGI_DEBUG("onTouchUp: touch=(%.1f, %.1f) isActive=%d pressedBtn=%p isScrolling=%d",
               x, y, touchState.isActive ? 1 : 0, (void*)touchState.pressedButton,
               touchState.isScrolling ? 1 : 0);
    if (!touchState.isActive) return;

    float dx = x - touchState.startX;
    float dy = y - touchState.startY;
    float dist = std::hypot(dx, dy);

    LOGI_DEBUG("onTouchUp: dist=%.2f threshold=%.2f", dist, TouchState::TAP_THRESHOLD);

    // Only execute command if it was a tap (not a scroll)
    if (!touchState.isScrolling && dist < TouchState::TAP_THRESHOLD) {
        if (touchState.pressedButton) {
            // Check if this is a tab button
            int tabIdx = findTabIndex(touchState.pressedButton);
            if (tabIdx >= 0) {
                // Switch to the selected tab
                currentTab = static_cast<Tab>(tabIdx);
                LOGI_DEBUG("Switched to tab: %s", getTabName(currentTab).c_str());
            } else {
                // Execute content button command
                executeButtonCommand(*touchState.pressedButton);
            }
        }
    }

    // Reset pressed state
    if (touchState.pressedButton) {
        touchState.pressedButton->isPressed = false;
    }
    touchState = {};
}

void DebugMenu::onTouchCancel() {
    if (touchState.pressedButton) {
        touchState.pressedButton->isPressed = false;
    }
    touchState = {};
}

// ==================== Command Execution ====================

void DebugMenu::executeButtonCommand(Button& btn) {
    if (btn.command.empty()) {
        LOGI_DEBUG("Button '%s' has no command", btn.label.c_str());
        return;
    }
    if (!console) {
        LOGI_DEBUG("Console not available");
        return;
    }
    console->executeCommand(btn.command);
    btn.pressTimer = 0.2f; // Visual feedback

    // Show feedback in DebugMenu
    feedbackText = "Executed: " + btn.command;
    feedbackTimer = 3.0f;  // Show for 3 seconds
    feedbackColor = glm::vec3(0.4f, 0.9f, 0.4f);  // Green

    LOGI_DEBUG("Executed: %s", btn.command.c_str());
}

int DebugMenu::findTabIndex(const Button* btn) const {
    for (size_t i = 0; i < tabButtons.size(); ++i) {
        if (&tabButtons[i] == btn) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ==================== Hit Testing ====================

DebugMenu::Button* DebugMenu::hitTestTab(float x, float y) {
    float s = getScale();
    float tabBarY = safeTop + 8.0f * s;
    float tabH = TAB_HEIGHT * s;

    if (y < tabBarY || y > tabBarY + tabH) return nullptr;

    for (auto& btn : tabButtons) {
        if (x >= btn.x && x <= btn.x + btn.w) {
            return &btn;
        }
    }
    return nullptr;
}

DebugMenu::Button* DebugMenu::hitTestContent(float x, float y) {
    float s = getScale();
    float tabBarY = safeTop + 8.0f * s;
    float tabH = TAB_HEIGHT * s;
    float contentY = tabBarY + tabH + 20.0f * s;
    float contentH = screenHeight - safeBottom - contentY;
    if (y < contentY || y > contentY + contentH) {
        LOGI_DEBUG("hitTestContent: y=%.1f outside content range [%.1f, %.1f] (screen=%d, safeBottom=%.0f)",
                   y, contentY, contentY + contentH, screenHeight, safeBottom);
        return nullptr;
    }

    size_t idx = static_cast<size_t>(currentTab);
    if (idx >= tabContents.size()) return nullptr;

    int btnCount = 0;
    for (auto& btn : tabContents[idx].buttons) {
        if (btn.y + btn.h < contentY || btn.y > contentY + contentH) continue;
        if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
            LOGI_DEBUG("hitTestContent: HIT button '%s' at (%.0f, %.0f)-(%.0f, %.0f) touch=(%.1f, %.1f)",
                       btn.label.c_str(), btn.x, btn.y, btn.x + btn.w, btn.y + btn.h, x, y);
            return &btn;
        }
        btnCount++;
    }
    LOGI_DEBUG("hitTestContent: no hit for touch=(%.1f, %.1f) checked %d buttons, first btn at (%.0f, %.0f)",
               x, y, btnCount,
               tabContents[idx].buttons.empty() ? 0.0f : tabContents[idx].buttons[0].x,
               tabContents[idx].buttons.empty() ? 0.0f : tabContents[idx].buttons[0].y);
    return nullptr;
}

// ==================== Update and Render ====================

void DebugMenu::update(float deltaTime) {
    if (!visible) return;

    // Update press timers for visual feedback
    for (auto& btn : tabButtons) {
        if (btn.pressTimer > 0) {
            btn.pressTimer -= deltaTime;
            if (btn.pressTimer <= 0) btn.isPressed = false;
        }
    }
    for (auto& content : tabContents) {
        for (auto& btn : content.buttons) {
            if (btn.pressTimer > 0) {
                btn.pressTimer -= deltaTime;
                if (btn.pressTimer <= 0) btn.isPressed = false;
            }
        }
    }

    // Update feedback timer
    if (feedbackTimer > 0.0f) {
        feedbackTimer -= deltaTime;
    }

    calculateButtonPositions();
}

void DebugMenu::calculateButtonPositions() {
    float s = getScale();
    float margin = BUTTON_MARGIN * s;
    float x = safeLeft + margin;

    // Tab buttons - use smaller width to fit 13 tabs
    float tabY = safeTop + 8.0f * s;
    float tabH = TAB_HEIGHT * s;
    float tabW = std::min(80.0f * s, (screenWidth - safeLeft - safeRight - margin * (tabButtons.size() + 1)) / tabButtons.size());
    for (auto& btn : tabButtons) {
        btn.x = x;
        btn.y = tabY;
        btn.w = tabW;
        btn.h = tabH;
        x += btn.w + margin;
    }

    // Content buttons
    size_t idx = static_cast<size_t>(currentTab);
    if (idx >= tabContents.size()) return;

    auto& content = tabContents[idx];
    float contentY = tabY + tabH + 20.0f * s;
    float btnH = BUTTON_HEIGHT * s;
    float btnW = (screenWidth - safeLeft - safeRight - margin * 3.0f) * 0.5f;

    for (size_t i = 0; i < content.buttons.size(); ++i) {
        int col = i % 2;
        int row = i / 2;
        content.buttons[i].x = safeLeft + margin + (btnW + margin) * col;
        content.buttons[i].y = contentY + (btnH + margin) * row - content.scrollOffset;
        content.buttons[i].w = btnW;
        content.buttons[i].h = btnH;
    }
}

void DebugMenu::render() {
    if (!visible || !textRenderer) return;
    renderBackground();
    renderTabBar();
    renderContent();
}

void DebugMenu::renderBackground() {
    // Semi-transparent dark overlay (full screen)
    UIDrawHelper::drawColoredQuad(0, 0, screenWidth, screenHeight,
                                   glm::vec4(0.0f, 0.0f, 0.0f, 0.75f),
                                   screenWidth, screenHeight);
}

void DebugMenu::renderTabBar() {
    float s = getScale();

    for (size_t i = 0; i < tabButtons.size(); ++i) {
        auto& btn = tabButtons[i];
        bool isActive = (i == static_cast<size_t>(currentTab));

        // Tab background color
        glm::vec4 bgColor = isActive ?
            glm::vec4(0.4f, 0.7f, 0.4f, 0.9f) :
            glm::vec4(0.2f, 0.2f, 0.3f, 0.8f);

        if (btn.isPressed) {
            bgColor = glm::vec4(0.5f, 0.8f, 0.5f, 1.0f);
        }

        UIDrawHelper::drawColoredQuad(btn.x, btn.y, btn.w, btn.h, bgColor,
                                       screenWidth, screenHeight);

        // Tab label - use smaller scale for many tabs
        std::string label = isActive ? "[" + btn.label + "]" : btn.label;
        glm::vec3 textColor = isActive ? glm::vec3(1.0f, 1.0f, 1.0f) : glm::vec3(0.7f, 0.7f, 0.7f);
        float labelScale = 0.35f * s; // Smaller to fit 13 tabs
        textRenderer->renderText(label.c_str(),
                                  btn.x + 3.0f * s,
                                  btn.y + btn.h * 0.5f,
                                  textColor, labelScale);
    }
}

void DebugMenu::renderContent() {
    float s = getScale();
    float tabY = safeTop + 8.0f * s;
    float tabH = TAB_HEIGHT * s;
    float contentY = tabY + tabH + 20.0f * s;
    float contentH = screenHeight - safeBottom - contentY;

    size_t tabIdx = static_cast<size_t>(currentTab);
    if (tabIdx >= tabContents.size()) return;
    TabContent& content = tabContents[tabIdx];

    // Clip content area (draw background)
    UIDrawHelper::drawColoredQuad(safeLeft, contentY,
                                   screenWidth - safeLeft - safeRight, contentH,
                                   glm::vec4(0.1f, 0.1f, 0.15f, 0.6f),
                                   screenWidth, screenHeight);

    for (auto& btn : content.buttons) {
        // Skip if outside visible area
        if (btn.y + btn.h < contentY || btn.y > contentY + contentH) continue;

        renderButton(btn, s);
    }

    // Render command feedback at bottom of content area
    if (feedbackTimer > 0.0f && textRenderer) {
        float feedbackY = contentY + contentH - 30.0f * s;
        float feedbackFontSize = 0.4f * s;
        float textW = textRenderer->getTextWidth(feedbackText.c_str(), feedbackFontSize);
        float feedbackX = (screenWidth - textW) * 0.5f;

        // Background for feedback
        UIDrawHelper::drawColoredQuad(feedbackX - 10.0f * s, feedbackY - 5.0f * s,
                                       textW + 20.0f * s, 25.0f * s,
                                       glm::vec4(0.0f, 0.0f, 0.0f, 0.8f),
                                       screenWidth, screenHeight);

        // Feedback text
        float alpha = std::min(1.0f, feedbackTimer);
        textRenderer->renderText(feedbackText.c_str(), feedbackX, feedbackY + 12.0f * s,
                                  glm::vec4(feedbackColor.x, feedbackColor.y, feedbackColor.z, alpha), feedbackFontSize);
    }
}

void DebugMenu::renderButton(Button& btn, float s) {
    glm::vec4 bgColor = btn.isPressed ?
        glm::vec4(0.5f, 0.7f, 1.0f, 0.9f) :
        glm::vec4(0.3f, 0.3f, 0.35f, 0.8f);

    UIDrawHelper::drawColoredQuad(btn.x, btn.y, btn.w, btn.h, bgColor,
                                   screenWidth, screenHeight);

    textRenderer->renderText(btn.label.c_str(),
                              btn.x + 10.0f * s,
                              btn.y + btn.h * 0.5f,
                              glm::vec3(1.0f, 1.0f, 1.0f), 0.4f * s);
}

// ==================== Utility ====================

float DebugMenu::getScale() const {
    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    return std::clamp(scale, 0.5f, 2.0f);
}

std::string DebugMenu::getTabName(Tab tab) const {
    switch (tab) {
        case Tab::PLAYER: return "Player";
        case Tab::COMBAT: return "Combat";
        case Tab::INVENTORY: return "Items";
        case Tab::MAGIC: return "Magic";
        case Tab::QUEST: return "Quest";
        case Tab::NPC: return "NPC";
        case Tab::DIALOGUE: return "Talk";
        case Tab::WORLD: return "World";
        case Tab::SAVE: return "Save";
        case Tab::SYSTEM: return "System";
        case Tab::SOUND: return "Sound";
        case Tab::ASSETS: return "Assets";
        case Tab::LOGS: return "Logs";
        default: return "?";
    }
}

void DebugMenu::clampScrollOffsets() {
    size_t idx = static_cast<size_t>(currentTab);
    if (idx >= tabContents.size()) return;

    auto& content = tabContents[idx];
    float s = getScale();
    float tabY = safeTop + 8.0f * s;
    float tabH = TAB_HEIGHT * s;
    float contentY = tabY + tabH + 20.0f * s;
    float contentH = screenHeight - safeBottom - contentY;
    float btnH = BUTTON_HEIGHT * s;
    float margin = BUTTON_MARGIN * s;

    // 2-column layout: calculate row count
    int rows = (static_cast<int>(content.buttons.size()) + 1) / 2;
    float totalHeight = rows * (btnH + margin);

    float maxScroll = std::max(0.0f, totalHeight - contentH);
    content.scrollOffset = std::clamp(content.scrollOffset, 0.0f, maxScroll);
}

// ==================== Tab Content Creation ====================

void DebugMenu::createTabButtons() {
    for (int i = 0; i < static_cast<int>(Tab::COUNT); ++i) {
        Button btn;
        btn.label = getTabName(static_cast<Tab>(i));
        btn.baseColor = glm::vec3(0.4f, 0.5f, 0.6f);
        tabButtons.push_back(btn);
    }
}

void DebugMenu::createAllTabContents() {
    // Player tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Heal", "heal"},
            {"God Mode", "god"},
            {"Set HP 100", "sethealth 100"},
            {"Set MP 100", "setmana 100"},
            {"Set Stamina 100", "setstamina 100"},
            {"Set Level 50", "setlevel 50"},
            {"Add XP 1000", "addxp 1000"},
            {"Max Skills", "maxskills"},
            {"Reset Stats", "resetstats"},
            {"Set Blade 100", "setskill Blade 100"},
            {"Set Dest 100", "setskill Destruction 100"},
            {"Set Speed 100", "setattr Speed 100"},
            {"Show Stats", "stats"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.3f, 0.5f, 0.7f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Combat tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Attack", "attack"},
            {"Block", "block"},
            {"Dodge", "dodge"},
            {"Kill Nearest", "kill"},
            {"Kill All", "killall"},
            {"Resurrect", "resurrect"},
            {"Damage 10", "damage 0 10"},
            {"Damage 100", "damage 0 100"},
            {"Combat Debug", "combatdebug"},
            {"Combat Stats", "combatstats"},
            {"Active Combats", "activecombats"},
            {"Attack Nearest", "attacknearest"},
            {"Set Damage x2", "setdamagemultiplier 2.0"},
            {"Set Damage x5", "setdamagemultiplier 5.0"},
            {"Invincible On", "invincible on"},
            {"Invincible Off", "invincible off"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.7f, 0.3f, 0.3f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Inventory tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Add Gold x100", "additem 0 100"},
            {"Add Apple x5", "additem 1 5"},
            {"Add Health Pot", "additem 10 5"},
            {"Add Magicka Pot", "additem 11 5"},
            {"Remove Apple", "removeitem 1 1"},
            {"Clear Inv", "clearinv"},
            {"List Items", "listitems"},
            {"Max Weight", "setweight 9999"},
            {"Inventory Info", "inventoryinfo"},
            {"Item Info", "iteminfo 0"},
            {"Add Sword", "additem 100 1"},
            {"Add Shield", "additem 200 1"},
            {"Add Armor", "additem 300 1"},
            {"Carry Weight", "carryweight"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.6f, 0.5f, 0.2f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Magic tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Learn Fire", "learnspell 1"},
            {"Learn Heal", "learnspell 2"},
            {"Learn Light", "learnspell 3"},
            {"Equip Fire", "equipspell 1"},
            {"Cast Fire", "castspell 1 0"},
            {"Cast Heal", "castspell 2 0"},
            {"List Spells", "listspells"},
            {"Set MP 100", "setmana 100"},
            {"Spell Info", "spellinfo 1"},
            {"Player Spells", "playerspells"},
            {"Cast at Enemy", "castspellatenemy 1"},
            {"Infinite Mana On", "infinitmana on"},
            {"Infinite Mana Off", "infinitmana off"},
            {"Spell Damage x2", "setspelldamage 2.0"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.5f, 0.2f, 0.7f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Quest tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"List Quests", "listquests"},
            {"Active Quests", "activequests"},
            {"Quest Details", "questdetails 1"},
            {"Accept Main", "acceptquest 1"},
            {"Accept Side", "acceptquest 2"},
            {"Complete Q1", "completequest 1"},
            {"Fail Q1", "failquest 1"},
            {"Update Obj", "updateobj 1 1 5"},
            {"Reset Quest", "resetquest 1"},
            {"Quest Reward", "questreward 1"},
            {"Complete Obj", "completeobjectives 1"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.7f, 0.5f, 0.2f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // NPC tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"List NPCs", "listnpcs"},
            {"NPC Count", "npccount"},
            {"NPC Info", "npcinfo 0"},
            {"Nearby", "nearby"},
            {"Spawn Guard", "spawnat Guard 0 0 0"},
            {"Spawn Mage", "spawnat Mage 5 0 5"},
            {"Spawn Bandit", "spawnat Bandit -5 0 5"},
            {"Spawn at Player", "spawnplayer Guard"},
            {"Kill All NPCs", "killallnpcs"},
            {"Aggro NPC", "aggro 0"},
            {"Calm NPC", "calm 0"},
            {"Set AI Combat", "setai 0 combat"},
            {"Set AI Idle", "setai 0 idle"},
            {"Resurrect", "resurrectnpc 0"},
            {"Set Speed", "setnpcspeed 100"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.4f, 0.5f, 0.3f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Dialogue tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Talk NPC", "talk 0"},
            {"Dialogue State", "dialoguestate"},
            {"Topics", "dialoguetopics"},
            {"Choices", "dialoguechoices"},
            {"History", "dialoguehistory"},
            {"Topic 0", "selecttopic 0"},
            {"Topic 1", "selecttopic 1"},
            {"Topic 2", "selecttopic 2"},
            {"Choice 0", "selectchoice 0"},
            {"Choice 1", "selectchoice 1"},
            {"End Talk", "endtalk"},
            {"Reset Dialogue", "resetdialogue"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.6f, 0.4f, 0.6f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // World tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Weather Clear", "setweather clear"},
            {"Weather Rain", "setweather rain"},
            {"Weather Snow", "setweather snow"},
            {"Weather Fog", "setweather fog"},
            {"Weather Storm", "setweather storm"},
            {"Time Dawn (6)", "settime 6"},
            {"Time Noon (12)", "settime 12"},
            {"Time Dusk (18)", "settime 18"},
            {"Time Midnight", "settime 0"},
            {"Time x30", "settimescale 30"},
            {"Time x1", "settimescale 1"},
            {"Time x0 (Pause)", "settimescale 0"},
            {"Load Cell 0,0", "loadcell 0 0"},
            {"World Info", "worldinfo"},
            {"World Detail", "worldinfodetail"},
            // Phase 66: Map debug
            {"Player Position", "playerpos"},
            {"Nearby Cells", "nearbycells"},
            {"Active Cells", "activecells"},
            {"Cell Details", "celldetails 0 0"},
            {"World Items", "worlditems"},
            {"Door Info", "doorinfo"},
            {"Teleport 0,0", "teleportcell 0 0"},
            {"Teleport 1,0", "teleportcell 1 0"},
            {"Teleport 0,1", "teleportcell 0 1"},
            {"Teleport 1,1", "teleportcell 1 1"},
            {"Move North +512", "moverel 0 -512"},
            {"Move South +512", "moverel 0 512"},
            {"Move East +512", "moverel 512 0"},
            {"Move West +512", "moverel -512 0"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.3f, 0.6f, 0.6f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Save tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Quick Save", "quicksave"},
            {"Quick Load", "quickload"},
            {"Save Slot 0", "save 0"},
            {"Save Slot 1", "save 1"},
            {"Save Slot 2", "save 2"},
            {"Load Slot 0", "load 0"},
            {"Load Slot 1", "load 1"},
            {"Load Slot 2", "load 2"},
            {"List Saves", "listsaves"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.5f, 0.5f, 0.3f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // System tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Toggle Wireframe", "wireframe"},
            {"Toggle AABB", "aabb"},
            {"NPC Overlay", "npcoverlay"},
            {"Touch Trail", "touchtrail"},
            {"Debug HUD+", "debughudnext"},
            {"Debug HUD-", "debughudprev"},
            {"Debug Log", "debuglog"},
            {"FPS Stats", "fpsstats"},
            {"Memory Stats", "memorystats"},
            {"Performance", "performance"},
            {"Reset Stats", "resetstats"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.4f, 0.4f, 0.5f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Sound tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Play BGM", "playbgm"},
            {"Stop BGM", "stopbgm"},
            {"Play SE", "playse"},
            {"Stop All SE", "stopallse"},
            {"Set Volume 50%", "setvolume 0.5"},
            {"Set Volume 100%", "setvolume 1.0"},
            {"Mute All", "mute"},
            {"Unmute All", "unmute"},
            {"List Audio", "listaudio"},
            {"Audio Stats", "audiostats"},
            // Phase 66: BGM browsing
            {"List BGM Tracks", "listbgm"},
            {"BGM Info", "bgminfo"},
            {"BGM Vol 25%", "bgmvolume 0.25"},
            {"BGM Vol 50%", "bgmvolume 0.5"},
            {"BGM Vol 75%", "bgmvolume 0.75"},
            {"BGM Vol 100%", "bgmvolume 1.0"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.6f, 0.3f, 0.6f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Assets tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"List Textures", "listtextures"},
            {"List Models", "listmodels"},
            {"List Audio", "listaudio"},
            {"Texture Info", "textureinfo"},
            {"Model Info", "modelinfo"},
            {"Cache Stats", "cachestats"},
            {"Clear Cache", "clearcache"},
            {"Reload Assets", "reloadassets"},
            {"Memory Usage", "memoryusage"},
            {"Asset Stats", "assetstats"},
            // Phase 66: Texture browsing
            {"Textures Detail", "texturesdetail"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.3f, 0.6f, 0.5f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }

    // Logs tab
    {
        TabContent content;
        std::vector<std::pair<std::string, std::string>> items = {
            {"Show All Logs", "loglevel all"},
            {"Show Debug", "loglevel debug"},
            {"Show Info", "loglevel info"},
            {"Show Warning", "loglevel warn"},
            {"Show Error", "loglevel error"},
            {"Clear Logs", "clearlogs"},
            {"Export Logs", "exportlogs"},
            {"Log Stats", "logstats"},
            {"Search Logs", "searchlog"},
            {"Toggle Auto-scroll", "logautoscroll"},
        };
        for (const auto& item : items) {
            Button btn;
            btn.label = item.first;
            btn.command = item.second;
            btn.baseColor = glm::vec3(0.5f, 0.4f, 0.3f);
            content.buttons.push_back(btn);
        }
        tabContents.push_back(content);
    }
}