#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "text_renderer.h"

class AssetManager;
class Mesh;

/**
 * @brief 3D Model Viewer - Display loaded NIF models
 *
 * Features:
 * - List all loaded 3D models (NIF files)
 * - Show model info (vertices, faces, bones)
 * - Allow selecting a model to view it in the scene
 */
class ModelViewer {
public:
    ModelViewer();
    ~ModelViewer();

    bool initialize(TextRenderer* textRenderer, AssetManager* assetMgr);
    void cleanup();

    void toggle();
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    void update(float deltaTime);
    void render();

    void setScreenSize(int w, int h);

    // Add a model to the list
    void addModelEntry(const std::string& path, int vertexCount, int faceCount,
                         int boneCount, const std::string& name = "");

private:
    TextRenderer* textRenderer;
    AssetManager* assetManager;
    bool visible;
    bool initialized;

    int screenWidth;
    int screenHeight;

    // Model entry info
    struct ModelEntry {
        std::string path;
        std::string name;
        int vertexCount;
        int faceCount;
        int boneCount;
    };

    std::vector<ModelEntry> models;
    int selectedIndex;
    float scrollOffset;

    // UI constants
    static constexpr float ENTRY_HEIGHT = 44.0f;
    static constexpr float MARGIN = 8.0f;

    // Helper methods
    void calculatePositions();
    void renderModelList();
    void renderSelectedModel();
    std::string formatNumber(int num) const;
    float getScale() const;
};
