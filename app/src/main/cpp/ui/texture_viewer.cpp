#include "texture_viewer.h"
#include "../assets/asset_manager.h"
#include "ui_draw_helper.h"
#include <android/log.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

#undef LOG_TAG
#define LOG_TAG "TextureViewer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

TextureViewer::TextureViewer()
    : textRenderer(nullptr), assetManager(nullptr),
      visible(false), initialized(false),
      screenWidth(1080), screenHeight(1920),
      selectedIndex(-1), scrollOffset(0.0f) {
}

TextureViewer::~TextureViewer() {
    cleanup();
}

bool TextureViewer::initialize(TextRenderer* textRend, AssetManager* assetMgr) {
    if (initialized) return true;
    if (!textRend || !assetMgr) return false;
    textRenderer = textRend;
    assetManager = assetMgr;
    initialized = true;
    LOGD("TextureViewer initialized");
    return true;
}

void TextureViewer::cleanup() {
    textures.clear();
    selectedIndex = -1;
    initialized = false;
}

void TextureViewer::toggle() {
    visible = !visible;
    LOGD("TextureViewer %s", visible ? "opened" : "closed");
}

void TextureViewer::setScreenSize(int w, int h) {
    screenWidth = w;
    screenHeight = h;
}

void TextureViewer::addTextureEntry(const std::string& path, uint32_t width, uint32_t height,
                                       const std::string& format, size_t memoryBytes, GLuint glTextureId) {
    TextureEntry entry;
    entry.path = path;
    entry.width = width;
    entry.height = height;
    entry.format = format;
    entry.memoryBytes = memoryBytes;
    entry.glTextureId = glTextureId;
    textures.push_back(entry);
}

void TextureViewer::update(float deltaTime) {
    if (!visible) return;
    (void)deltaTime;

    // Update press timers and scroll
    // Touch handling is done externally via onTouchDown/Move/Up
}

void TextureViewer::render() {
    if (!visible || !textRenderer) return;

    float s = getScale();
    float margin = MARGIN * s;
    float entryH = ENTRY_HEIGHT * s;

    // Calculate positions
    calculatePositions();

    // Render background
    UIDrawHelper::drawColoredQuad(0, 0, screenWidth, screenHeight,
                                   glm::vec4(0.0f, 0.0f, 0.0f, 0.75f),
                                   screenWidth, screenHeight);

    // Render header
    float xPos = 20.0f * s;
    float yPos = 60.0f * s;
    glm::vec3 headerColor(0.3f, 0.7f, 0.9f);
    textRenderer->renderText("TEXTURE VIEWER", xPos, yPos, headerColor, 0.6f * s);
    yPos += 30.0f * s;

    // Render texture count
    std::stringstream countSs;
    countSs << "Loaded: " << textures.size() << " textures";
    textRenderer->renderText(countSs.str(), xPos, yPos, glm::vec3(0.7f, 0.7f, 0.7f), 0.4f * s);
    yPos += 30.0f * s;

    // Render texture list
    renderTextureList();

    // Render selected texture info
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(textures.size())) {
        renderSelectedTexture();
    }
}

void TextureViewer::calculatePositions() {
    // Layout is handled in render() based on screen dimensions
}

void TextureViewer::renderTextureList() {
    float s = getScale();
    float margin = MARGIN * s;
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
    textRenderer->renderText("Size", listX + listW * 0.4f, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Format", listX + listW * 0.65f, listY + 12.0f * s, colColor, 0.35f * s);
    textRenderer->renderText("Memory", listX + listW * 0.85f, listY + 12.0f * s, colColor, 0.35f * s);

    float contentY = listY + 30.0f * s - scrollOffset;
    float maxScroll = std::max(0.0f, static_cast<float>(textures.size()) * entryH - listH + 30.0f * s);
    scrollOffset = std::clamp(scrollOffset, 0.0f, maxScroll);

    for (size_t i = 0; i < textures.size(); ++i) {
        const auto& tex = textures[i];
        float btnY = contentY + i * entryH;

        // Skip if outside visible area
        if (btnY + entryH < listY || btnY > listY + listH) continue;

        // Background for selected item
        if (static_cast<int>(i) == selectedIndex) {
            UIDrawHelper::drawColoredQuad(listX + 2.0f * s, btnY, listW - 4.0f * s, entryH - 2.0f * s,
                                           glm::vec4(0.3f, 0.5f, 0.8f, 0.6f),
                                           screenWidth, screenHeight);
        }

        // Texture name
        std::string name = tex.path.length() > 30 ? tex.path.substr(0, 27) + "..." : tex.path;
        textRenderer->renderText(name.c_str(), listX + 10.0f * s, btnY + 12.0f * s,
                                    glm::vec3(1.0f, 1.0f, 1.0f), 0.38f * s);

        // Dimensions
        std::stringstream dimSs;
        dimSs << tex.width << "x" << tex.height;
        textRenderer->renderText(dimSs.str(), listX + listW * 0.4f, btnY + 12.0f * s,
                                    glm::vec3(0.7f, 0.7f, 0.7f), 0.35f * s);

        // Format
        textRenderer->renderText(tex.format.c_str(), listX + listW * 0.65f, btnY + 12.0f * s,
                                    glm::vec3(0.5f, 0.8f, 0.5f), 0.35f * s);

        // Memory
        textRenderer->renderText(formatMemorySize(tex.memoryBytes).c_str(),
                                    listX + listW * 0.85f, btnY + 12.0f * s,
                                    glm::vec3(0.9f, 0.9f, 0.3f), 0.35f * s);
    }
}

void TextureViewer::renderSelectedTexture() {
    float s = getScale();
    const auto& tex = textures[selectedIndex];

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
    textRenderer->renderText("TEXTURE DETAIL", infoX + 10.0f * s, yPos, labelColor, 0.5f * s);
    yPos += 30.0f * s;

    // Path
    textRenderer->renderText("Path:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    yPos += 25.0f * s;
    std::string displayPath = tex.path.length() > 40 ? tex.path.substr(0, 37) + "..." : tex.path;
    textRenderer->renderText(displayPath.c_str(), infoX + 10.0f * s, yPos, valueColor, 0.38f * s);
    yPos += 30.0f * s;

    // Width
    textRenderer->renderText("Width:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    std::stringstream ss;
    ss << tex.width;
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Height
    textRenderer->renderText("Height:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << tex.height;
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Format
    textRenderer->renderText("Format:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    textRenderer->renderText(tex.format.c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // Memory
    textRenderer->renderText("Memory:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    textRenderer->renderText(formatMemorySize(tex.memoryBytes).c_str(),
                                infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
    yPos += 25.0f * s;

    // GL Texture ID
    textRenderer->renderText("GL ID:", infoX + 10.0f * s, yPos, labelColor, 0.4f * s);
    ss.str(""); ss << tex.glTextureId;
    textRenderer->renderText(ss.str().c_str(), infoX + infoW * 0.5f, yPos, valueColor, 0.4f * s);
}

std::string TextureViewer::formatMemorySize(size_t bytes) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (bytes < 1024) {
        ss << bytes << " B";
    } else if (bytes < 1024 * 1024) {
        ss << bytes / 1024.0f << " KB";
    } else {
        ss << bytes / (1024.0f * 1024.0f) << " MB";
    }
    return ss.str();
}

std::string TextureViewer::getFormatName(TextureFormat fmt) const {
    switch (fmt) {
        case TextureFormat::RGBA8: return "RGBA8";
        case TextureFormat::RGB8: return "RGB8";
        case TextureFormat::DXT1: return "DXT1";
        case TextureFormat::DXT3: return "DXT3";
        case TextureFormat::DXT5: return "DXT5";
        default: return "Unknown";
    }
}

float TextureViewer::getScale() const {
    float minDim = static_cast<float>(std::min(screenWidth, screenHeight));
    float scale = minDim / 1080.0f;
    return std::clamp(scale, 0.5f, 2.0f);
}
