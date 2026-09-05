#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "text_renderer.h"

class WorldManager;

/**
 * @brief World Viewer - Display world cell information and navigation
 *
 * Features:
 * - Display world cell information
 * - Show active cells, loaded objects, terrain info
 * - Allow navigating between cells
 */
class WorldViewer {
public:
    WorldViewer();
    ~WorldViewer();

    bool initialize(TextRenderer* textRenderer, WorldManager* worldMgr);
    void cleanup();

    void toggle();
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    void update(float deltaTime);
    void render();

    void setScreenSize(int w, int h);

private:
    TextRenderer* textRenderer;
    WorldManager* worldManager;
    bool visible;
    bool initialized;

    int screenWidth;
    int screenHeight;

    // Cell navigation
    int selectedCellX;
    int selectedCellY;
    float scrollOffset;

    // Cached info
    struct CachedInfo {
        glm::vec3 playerPos;
        int32_t currentCellX;
        int32_t currentCellY;
        int loadedCells;
        int activeCells;
        float timeOfDay;
        std::string currentWeather;
        int dayCount;
    };
    CachedInfo cachedInfo;
    float updateInterval;
    float timeSinceLastUpdate;

    // UI constants
    static constexpr float ENTRY_HEIGHT = 44.0f;
    static constexpr float MARGIN = 8.0f;

    // Helper methods
    void updateCachedInfo();
    void renderCellList();
    void renderCellInfo();
    void renderWorldStats();
    std::string formatVector(const glm::vec3& v) const;
    float getScale() const;
};
