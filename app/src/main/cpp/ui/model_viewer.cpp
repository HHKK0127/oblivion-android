#include "model_viewer.h"
#include "../assets/asset_manager.h"
#include "ui_draw_helper.h"
#include <android/log.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#undef LOG_TAG
#define LOG_TAG "ModelViewer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

ModelViewer::ModelViewer()
    : textRenderer(nullptr), assetManager(nullptr),
      visible(false), initialized(false),
      screenWidth(1080), screenHeight(1920),
      selectedIndex(-1), scrollOffset(0.0f) {
}

ModelViewer::~ModelViewer() {
    cleanup();
}

bool ModelViewer::initialize(TextRenderer* textRend, AssetManager* assetMgr) {
    if (initialized) return true;
    if (!textRend || !assetMgr) return false;
    textRenderer = textRend;
    assetManager = assetMgr;
    initialized = true;
    LOGD("ModelViewer initialized");
    return true;
}

void ModelViewer::cleanup() {
    models.clear();
    selectedIndex = -1;
    initialized = false;
}

void ModelViewer::toggle() {
    visible = !visible;
    LOGD("ModelViewer %s", visible ? "opened" : "closed");
}

void ModelViewer::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
}

void ModelViewer::addModelEntry(const std::string& path, int vertexCount, int faceCount,
                                   int boneCount, const std::string& name) {
    ModelEntry entry;
    entry.path = path;
    entry.name = name.empty() ? path : name;
    entry.vertexCount = vertexCount;
    entry.faceCount = faceCount;
    entry.boneCount = boneCount;
    models.push_back(entry);
}

void ModelViewer::update(float deltaTime) {
    if (!visible) return;
    (void)deltaTime;
}

void ModelViewer::render() {
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
    textRenderer->renderText("MODEL VIEWER", xPos, yPos, headerColor, 0.6f * s);
    yPos += 30.0f * s;

    // Render model count
    std::stringstream countSs;
    countSs << "Loaded: " << models.size() << " models";
    textRenderer->renderText(countSs.str(), xPos, yPos, glm::vec3(0.7f, 0.7f, 0.7f), 0.4f * s);
    yPos += 30.0f * s;

    // Render model list
    renderModelList();

    // Render selected model info
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(models.size())) {
        renderSelectedModel();
    }
}

void ModelViewer::calculatePositions() {
    // Layout handled in render()
}

void ModelViewer::renderModelList() {
    float s = getScale();
    float entryH = ENTRY_HEIGHT * s;
    float listX = 20.0f * s;
    float listY = 120.0f * s;
    float listW = screenWidth * 0.55f;
    float listH = screenHeight - 200.0f * s;

    // Clip background
    UIDrawHelper::drawColoredQuad(listX, listY, listW, listH,
                                   glm::vec4(0.1f, 0.1f, 0.15f, 0.6f),
                                   screenWidth, screenHeight);

    // Header row
    glm::vec3 colColor(0.5f, 0.5f, 0.6f);
    textRenderer->renderText("Name", listX + 10.0f * s, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Vertices", listX + listW * 0.35f, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Faces", listX + listW * 0.55f, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Bones", listX + listW * 0.75f, listY + 12.0f * s, colColor, 0.35f * s);

    float contentY = listY + 30.0f * s - scrollOffset;
    float maxScroll = std::max(0.0f, static_cast<float>(models.size()) * entryH - listH + 30.0f * s);
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScroll);

    for (size_t i = 0; i < models.size(); ++i) {
        const auto& model = models[i];
        float btnY = contentY + static_cast<int>(i) * entryH;

        // Skip if outside visible area
        if (btnY + entryH < listY || btnY > listY + listH) continue;

        // Background for selected item
        if (static_cast<int>(i) == selectedIndex) {
            UIDrawHelper::drawColoredQuad(listX + 2.0f * s, btnY, listW - 4.0f * s, entryH - 2.0f * s,
                                           glm::vec4(0.3f, 0.5f, 0.8f, 0.6f),
                                           screenWidth, screenHeight);
        }

        // Model name
        std::string name = model.name.length() > 25 ? model.name.substr(0, 22) + "..." : model.name;
        textRenderer->renderText(name.c_str(), listX + 10.0f * s, btnY + 12.0f * s,
                                    glm::vec3(1.0f, 1.0f, 1.0f), 0.38f * s);

        // Vertices
        std::stringstream ss;
        ss << formatNumber(model.vertexCount);
        textRenderer->renderText(ss.str().c_str(), listX + listW * 0.35f, btnY + 12.0f * s,
                                    glm::vec3(0.7f, 0.7f, 0.7f), 0.35f * s);

        // Faces
        ss.str(""); ss << formatNumber(model.faceCount);
        textRenderer->renderText(ss.str().c_str(), listX + listW * 0.55f, btnY + 12.0f * s,
                                    glm::vec3(0.7f, 0.7f, 0.7f), 0.35f * s);

        // Bones
        ss.str(""); ss << model.boneCount;
        textRenderer->renderText(ss.str().c_str(), listX + listW * 0.75f, btnY + 12.0f * s,
                                    glm::vec3(0.5f, 0.8f, 0.5f), 0.35f * s);
    }
}

void ModelViewer::renderSelectedModel() {
    float s = getScale();
    const auto& model = models[selectedIndex];

    float infoX = screenWidth * 0.6f;
    float infoY = 120.0f * s;
    float infoW = screenWidth * 0.38f;
    float infoH = screenHeight - 200.0f * s;

    // Info panel background
    UIDrawHelper::drawColoredQuad(infoX, infoY, infoW, infoH,
                                   glm::vec4(0.15f, 0.15f, 0.2f, 0.6f),
                                   screenWidth, screenHeight);

    float yPos = infoY + 20.0f * s;
    glm::vec3 labelColor(0.5f, 0.7f, 0.9f);
    glm::vec3 valueColor(0.9f, 0.9f, 0.9f);

    // Header
    textRenderer->renderText("MODEL DETAIL", infoX + 10.0f * s, yPos, labelColor, 0.5f * s);
    yPos += 30.0f * s;

    // Name
    textRenderer->renderText("Name:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    yPos += 25.0f * s;

    // Path
    textRenderer->renderText("Path:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    yPos += 25.0f * s;

    // Vertices
    textRenderer->renderText("Vertices:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    std::stringstream ss;
    ss << formatNumber(model.vertexCount);
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Faces
    textRenderer->renderText("Faces:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << formatNumber(model.faceCount);
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Bones
    textRenderer->renderText("Bones:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << model.boneCount;
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
}

std::string ModelViewer::formatNumber(int num) const {
    if (num >= 1000000) {
        return std::to_string(num / 1000000) + "M";
    } else if (num >= 1000) {
        return std::to_string(num / 1000) + "K";
    }
    return std::to_string(num);
}

float ModelViewer::getScale() const {
    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    return std::clamp(scale, 0.5f, 2.0f);
}
