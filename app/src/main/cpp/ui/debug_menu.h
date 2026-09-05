#pragma once

#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>

class TextRenderer;
class GameConsole;

/**
 * @brief Debug Menu - Touch-based GUI for game system testing
 *
 * Provides categorized buttons for all game operations:
 * - Player: stats, skills, attributes, level
 * - Combat: attack, block, dodge, damage
 * - Inventory: add/remove/equip items
 * - Magic: spells, mana
 * - Quest: accept/complete/fail
 * - NPC: spawn, AI, aggression
 * - Dialogue: start, select topics
 * - World: weather, time, cells
 * - Save/Load: save, load, quicksave
 */
class DebugMenu {
public:
    DebugMenu();
    ~DebugMenu();

    bool initialize(TextRenderer* textRenderer, GameConsole* console);
    void cleanup();

    void toggle();
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    // Touch event handling (separate DOWN/MOVE/UP for proper tap vs scroll detection)
    void onTouchDown(float x, float y);
    void onTouchMove(float x, float y);
    void onTouchUp(float x, float y);
    void onTouchCancel();

    void update(float deltaTime);
    void render();

    void setScreenSize(int w, int h);

private:
    TextRenderer* textRenderer;
    GameConsole* console;
    bool visible;
    bool initialized;

    int screenWidth;
    int screenHeight;

    // Safe area insets
    float safeLeft, safeTop, safeRight, safeBottom;

    // Tab system
    enum class Tab {
        PLAYER,
        COMBAT,
        INVENTORY,
        MAGIC,
        QUEST,
        NPC,
        DIALOGUE,
        WORLD,
        SAVE,
        SYSTEM,
        SOUND,
        ASSETS,
        LOGS,
        COUNT
    };

    Tab currentTab;

    // Button definition
    struct Button {
        float x, y, w, h;
        std::string label;
        std::string command;  // Console command to execute
        glm::vec3 baseColor;
        bool isPressed;
        float pressTimer;
        Button() : x(0), y(0), w(0), h(0), isPressed(false), pressTimer(0.0f) {}
    };

    // Tab buttons
    std::vector<Button> tabButtons;

    // Content buttons per tab
    struct TabContent {
        std::vector<Button> buttons;
        float scrollOffset = 0.0f;
    };
    std::vector<TabContent> tabContents;

    // Touch state for proper tap vs scroll detection
    struct TouchState {
        bool isActive = false;
        float startX = 0.0f, startY = 0.0f;
        float lastX = 0.0f, lastY = 0.0f;
        bool isScrolling = false;
        static constexpr float TAP_THRESHOLD = 15.0f;
        static constexpr float SCROLL_THRESHOLD = 10.0f;
        Button* pressedButton = nullptr;
    } touchState;

    // Command feedback
    std::string feedbackText;
    float feedbackTimer;
    glm::vec3 feedbackColor;

    // UI constants
    static constexpr float TAB_HEIGHT = 50.0f;
    static constexpr float BUTTON_HEIGHT = 48.0f;
    static constexpr float BUTTON_MARGIN = 8.0f;

    // Helper methods
    void createTabButtons();
    void createAllTabContents();

    void calculateButtonPositions();
    Button* hitTestTab(float x, float y);
    Button* hitTestContent(float x, float y);
    void executeButtonCommand(Button& btn);
    int findTabIndex(const Button* btn) const;

    void renderBackground();
    void renderTabBar();
    void renderContent();
    void renderButton(Button& btn, float scale);

    float getScale() const;
    std::string getTabName(Tab tab) const;
    void clampScrollOffsets();
};
