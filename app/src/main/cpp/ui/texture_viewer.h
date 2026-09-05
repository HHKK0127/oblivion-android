#pragma once

#include <vector>
#include <string>
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "text_renderer.h"
#include "../assets/texture_manager.h"

class AssetManager;

/**
 * @brief Texture Viewer - Display loaded textures with details
 *
 * Features:
 * - List all loaded textures with dimensions, format, memory usage
 * - Allow selecting a texture to view it
 * - Show texture info in a scrollable list
 */
class TextureViewer {
public:
    TextureViewer();
    ~TextureViewer();

    bool initialize(TextRenderer* textRenderer, AssetManager* assetMgr);
    void cleanup();

    void toggle();
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    void update(float deltaTime);
    void render();

    void setScreenSize(int w, int h);

    // Add a texture to the list (called from AssetManager)
    void addTextureEntry(const std::string& path, uint32_t width, uint32_t height,
                         const std::string& format, size_t memoryBytes, GLuint glTextureId = 0);

private:
    TextRenderer* textRenderer;
    AssetManager* assetManager;
    bool visible;
    bool initialized;

    int screenWidth;
    int screenHeight;

    // Texture entry info
    struct TextureEntry {
        std::string path;
        uint32_t width;
        uint32_t height;
        std::string format;
        size_t memoryBytes;
        GLuint glTextureId;
    };

    std::vector<TextureEntry> textures;
    int selectedIndex;
    float scrollOffset;

    // Touch state
    struct TouchState {
        bool isActive = false;
        float startX = 0.0f, startY = 0.0f;
        float lastY = 0.0f;
        bool isScrolling = false;
        static constexpr float TAP_THRESHOLD = 15.0f;
        static constexpr float SCROLL_THRESHOLD = 10.0f;
    } touchState;

    // UI constants
    static constexpr float ENTRY_HEIGHT = 44.0f;
    static constexpr float MARGIN = 8.0f;

    // Helper methods
    void calculatePositions();
    void renderTextureList();
    void renderSelectedTexture();
    std::string formatMemorySize(size_t bytes) const;
    std::string getFormatName(TextureFormat fmt) const;
    float getScale() const;
};
