#pragma once

#include <string>
#include <map>
#include <glm/glm.hpp>
#include <GLES3/gl3.h>
#include <android/asset_manager.h>

/**
 * @brief Text rendering system using stb_truetype
 * Transforms TTF fonts into bitmaps to create texture atlas
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    /**
     * @brief Initialize text rendering system
     * @param assetMgr Android AssetManager (for font loading)
     * @return True on successful initialization
     */
    bool initialize(AAssetManager* assetMgr);

    /**
     * @brief Draw text on screen
     * @param text Text to draw
     * @param x X coordinate from top-left origin (pixels)
     * @param y Y coordinate from top-left origin (pixels)
     * @param color Text color (r, g, b)
     * @param scale Scale (default 1.0f)
     */
    void renderText(const std::string& text, float x, float y,
                    const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f),
                    float scale = 1.0f);

    /**
     * @brief Draw text on screen (with alpha)
     * @param text Text to draw
     * @param x X coordinate from top-left origin (pixels)
     * @param y Y coordinate from top-left origin (pixels)
     * @param color Text color (r, g, b, a)
     * @param scale Scale (default 1.0f)
     */
    void renderText(const std::string& text, float x, float y,
                    const glm::vec4& color, float scale = 1.0f);

    /**
     * @brief Calculate and return text width
     * @param text Text to measure
     * @param scale Scale
     * @return Text width (pixels)
     */
    float getTextWidth(const std::string& text, float scale = 1.0f);
    float getGlyphBearingX(char ch) const;
    float getGlyphHeight(char ch, float scale = 1.0f) const;
    float getGlyphOffsetY(char ch, float scale = 1.0f) const;

    /**
     * @brief Set screen size (for projection matrix)
     */
    void setScreenSize(int width, int height);

    /**
     * @brief Cleanup
     */
    void cleanup();

    /**
     * @brief Debug: draw simple rectangle (for testing)
     */
    void renderDebugQuad();

private:
    struct Glyph {
        float x0, y0, x1, y1;  // Texture coordinates
        float advanceX;
        float bearingX, bearingY;
    };

    // For font rendering
    struct FontData {
        unsigned char* fontData;
        float fontScale;
        int fontHeight;
        std::map<unsigned int, Glyph> glyphCache;
    };

    GLuint vao;
    GLuint vbo;
    GLuint shaderProgram;
    GLint projectionLoc;
    GLint colorLoc;
    GLuint fontTexture;  // Font texture

    int screenWidth;
    int screenHeight;

    // Font data
    FontData* fontData;

    // Android AssetManager (for font loading)
    AAssetManager* assetManager;

    // Texture atlas size
    static constexpr int ATLAS_WIDTH = 512;
    static constexpr int ATLAS_HEIGHT = 512;
    static constexpr int FONT_SIZE = 32;

    void compileShaders();
    Glyph getGlyph(unsigned int codepoint);
    bool loadFontFromAssets(const std::string& filename);
    bool createFontTextureAtlas();
};
