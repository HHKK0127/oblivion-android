#include "world_viewer.h"
#include "../world/world_manager.h"
#include "cell.h"
#include "text_renderer.h"
#include "ui_draw_helper.h"
#include <android/log.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "WorldViewer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

WorldViewer::WorldViewer()
    : textRenderer(nullptr), worldManager(nullptr),
      visible(false), initialized(false),
      screenWidth(1080), screenHeight(1920),
      selectedCellX(0), selectedCellY(0), scrollOffset(0.0f),
      updateInterval(0.5f), timeSinceLastUpdate(0.0f) {
    cachedInfo = {};
}

WorldViewer::~WorldViewer() {
    cleanup();
}

bool WorldViewer::initialize(TextRenderer* textRend, WorldManager* wm) {
    if (initialized) return true;
    if (!textRend || !wm) return false;
    textRenderer = textRend;
    worldManager = wm;
    initialized = true;
    LOGD("WorldViewer initialized");
    return true;
}

void WorldViewer::cleanup() {
    initialized = false;
}

void WorldViewer::toggle() {
    visible = !visible;
    LOGD("WorldViewer %s", visible ? "opened" : "closed");
}

void WorldViewer::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
}

void WorldViewer::update(float deltaTime) {
    if (!visible) return;

    timeSinceLastUpdate += deltaTime;
    if (timeSinceLastUpdate >= updateInterval) {
        timeSinceLastUpdate = 0.0f;
        updateCachedInfo();
    }
}

void WorldViewer::updateCachedInfo() {
    if (!worldManager) return;

    cachedInfo.playerPos = worldManager->getPlayerPosition();
    cachedInfo.currentCellX = static_cast<int32_t>(floor(cachedInfo.playerPos.x / 4096.0f));
    cachedInfo.currentCellY = static_cast<int32_t>(floor(cachedInfo.playerPos.z / 4096.0f));
    cachedInfo.loadedCells = static_cast<int>(worldManager->getActiveCells().size());
    cachedInfo.timeOfDay = worldManager->getTimeOfDay();
    cachedInfo.dayCount = worldManager->getDayCount();
}

void WorldViewer::render() {
    if (!visible || !textRenderer) return;

    float s = getScale();

    // Render background
    UIDrawHelper::drawColoredQuad(0, 0, screenWidth, screenHeight,
                                   glm::vec4(0.0f, 0.0f, 0.0f, 0.75f),
                                   screenWidth, screenHeight);

    // Render header
    float xPos = 20.0f * s;
    float yPos = 60.0f * s;
    glm::vec3 headerColor(0.3f, 0.7f, 0.9f);
    textRenderer->renderText("WORLD VIEWER", xPos, yPos, headerColor, 0.6f * s);
    yPos += 30.0f * s;

    // Render world stats
    renderWorldStats();

    // Render cell list
    renderCellList();

    // Render cell info
    renderCellInfo();
}

void WorldViewer::renderWorldStats() {
    float s = getScale();
    float xPos = 20.0f * s;
    float yPos = 120.0f * s;
    glm::vec3 labelColor(0.7f, 0.7f, 0.8f);
    glm::vec3 valueColor(0.9f, 0.9f, 0.9f);

    // Player Position
    textRenderer->renderText("Player:", xPos, yPos, labelColor, 0.4f * s);
    textRenderer->renderText(formatVector(cachedInfo.playerPos).c_str(), xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Current Cell
    char cellBuf[64];
    snprintf(cellBuf, sizeof(cellBuf), "(%d, %d)", cachedInfo.currentCellX, cachedInfo.currentCellY);
    textRenderer->renderText("Cell:", xPos, yPos, labelColor, 0.4f * s);
    textRenderer->renderText(cellBuf, xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Loaded Cells
    textRenderer->renderText("Loaded:", xPos, yPos, labelColor, 0.4f * s);
    std::stringstream ss;
    ss << cachedInfo.loadedCells;
    textRenderer->renderText(ss.str().c_str(), xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Time of Day
    textRenderer->renderText("Time:", xPos, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << std::fixed << std::setprecision(1) << cachedInfo.timeOfDay;
    textRenderer->renderText(ss.str().c_str(), xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Weather
    textRenderer->renderText("Weather:", xPos, yPos, labelColor, 0.4f * s);
    textRenderer->renderText(cachedInfo.currentWeather.c_str(), xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Day Count
    textRenderer->renderText("Day:", xPos, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << cachedInfo.dayCount;
    textRenderer->renderText(ss.str().c_str(), xPos + 80.0f * s, yPos, valueColor, 0.4f * s);
}

void WorldViewer::renderCellList() {
    float s = getScale();
    float entryH = ENTRY_HEIGHT * s;
    float listX = 20.0f * s;
    float listY = 300.0f * s;
    float listW = screenWidth * 0.55f;
    float listH = screenHeight - 380.0f * s;

    // Clip background
    UIDrawHelper::drawColoredQuad(listX, listY, listW, listH,
                                   glm::vec4(0.1f, 0.1f, 0.15f, 0.6f),
                                   screenWidth, screenHeight);

    // Header
    glm::vec3 colColor(0.5f, 0.5f, 0.6f);
    textRenderer->renderText("Cell X", listX + 10.0f * s, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Cell Y", listX + listW * 0.4f, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Status", listX + listW * 0.7f, listY + 12.0f * s, colColor, 0.35f * s);

    float contentY = listY + 30.0f * s - scrollOffset;
    int range = 5; // Show cells in a range around player
    int totalCells = (2 * range + 1) * (2 * range + 1);
    float maxScroll = std::max(0.0f, static_cast<float>(totalCells) * entryH - listH + 30.0f * s);
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScroll);

    int idx = 0;
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            int cx = cachedInfo.currentCellX + dx;
            int cy = cachedInfo.currentCellY + dy;
            float btnY = contentY + idx * entryH;

            // Skip if outside visible area
            if (btnY + entryH < listY || btnY > listY + listH) continue;

            // Background for selected cell
            if (cx == selectedCellX && cy == selectedCellY) {
                UIDrawHelper::drawColoredQuad(listX + 2.0f * s, btnY, listW - 4.0f * s, entryH - 2.0f * s,
                                               glm::vec4(0.3f, 0.5f, 0.8f, 0.6f),
                                               screenWidth, screenHeight);
            }

            // Cell coordinates
            std::stringstream ss;
            ss << cx;
            textRenderer->renderText(ss.str().c_str(), listX + 10.0f * s, btnY + 12.0f * s,
                                        glm::vec3(1.0f, 1.0f, 1.0f), 0.38f * s);
            ss.str(""); ss << cy;
            textRenderer->renderText(ss.str().c_str(), listX + listW * 0.4f, btnY + 12.0f * s,
                                        glm::vec3(1.0f, 1.0f, 1.0f), 0.38f * s);

            // Status (simplified - based on distance)
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            std::string status = dist <= 1.0f ? "Active" : (dist <= 2.0f ? "Loaded" : "Unloaded");
            glm::vec3 statusColor = dist <= 1.0f ? glm::vec3(0.3f, 1.0f, 0.3f) :
                                     dist <= 2.0f ? glm::vec3(0.9f, 0.9f, 0.3f) :
                                                    glm::vec3(0.7f, 0.7f, 0.7f);
            textRenderer->renderText(status.c_str(), listX + listW * 0.7f, btnY + 12.0f * s,
                                        statusColor, 0.35f * s);

            ++idx;
        }
    }
}

void WorldViewer::renderCellInfo() {
    float s = getScale();
    float infoX = screenWidth * 0.6f;
    float infoY = 300.0f * s;
    float infoW = screenWidth * 0.38f;
    float infoH = screenHeight - 380.0f * s;

    // Info panel background
    UIDrawHelper::drawColoredQuad(infoX, infoY, infoW, infoH,
                                   glm::vec4(0.15f, 0.15f, 0.2f, 0.6f),
                                   screenWidth, screenHeight);

    float yPos = infoY + 20.0f * s;
    glm::vec3 labelColor(0.5f, 0.7f, 0.9f);
    glm::vec3 valueColor(0.9f, 0.9f, 0.9f);

    // Header
    textRenderer->renderText("CELL INFO", infoX + 10.0f * s, yPos, labelColor, 0.5f * s);
    yPos += 30.0f * s;

    // Cell Coordinates
    textRenderer->renderText("Cell:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    std::stringstream ss;
    ss << "(" << selectedCellX << ", " << selectedCellY << ")";
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Player Distance
    textRenderer->renderText("Player:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    glm::vec3 playerPos = cachedInfo.playerPos;
    float dist = std::sqrt(std::pow(playerPos.x - selectedCellX * 4096.0f, 2) +
                           std::pow(playerPos.z - selectedCellY * 4096.0f, 2));
    ss.str(""); ss << std::fixed << std::setprecision(0) << dist << " units";
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Cell Size
    textRenderer->renderText("Size:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    textRenderer->renderText("4096x4096", infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Cell Type
    textRenderer->renderText("Type:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    textRenderer->renderText("Exterior", infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Instructions
    textRenderer->renderText("Tap cells to navigate", infoX + 10.0f * s, yPos + 20.0f * s,
                                glm::vec3(0.5f, 0.5f, 0.5f), 0.3f * s);
}

std::string WorldViewer::formatVector(const glm::vec3& v) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "(%.1f, %.1f, %.1f)", v.x, v.y, v.z);
    return std::string(buf);
}

float WorldViewer::getScale() const {
    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    return std::clamp(scale, 0.5f, 2.0f);
}
