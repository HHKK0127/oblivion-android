#include "ui_component.h"
#include "ui_draw_helper.h"
#include "../engine/texture_loader.h"
#include <GLES3/gl3.h>
#include <algorithm>

// Static member initialization
uint32_t UIComponent::nextId = 1;

// === UIComponent Implementation ===

UIComponent::UIComponent(const std::string& name)
    : name(name), id(nextId++), position(0.0f, 0.0f), size(100.0f, 100.0f),
      anchor(UIAnchor::TOP_LEFT), visible(true), initialized(false), enabled(true),
      backgroundColor(0.0f, 0.0f, 0.0f, 0.0f), borderColor(1.0f, 1.0f, 1.0f, 1.0f),
      borderWidth(0.0f), textureId(0), screenWidth(1920), screenHeight(1080) {
}

UIComponent::~UIComponent() {
    cleanup();
}

bool UIComponent::initialize() {
    UIDrawHelper::initialize();
    initialized = true;
    return true;
}

void UIComponent::update(float deltaTime) {
    if (!visible || !initialized) return;
    updateChildren(deltaTime);
}

void UIComponent::render() {
    if (!visible || !initialized) return;

    // Save OpenGL state
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    GLboolean blendEnabled;
    glGetBooleanv(GL_BLEND, &blendEnabled);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderBackground();
    renderTexture();
    renderBorder();
    renderChildren();

    // Restore state
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (!blendEnabled) glDisable(GL_BLEND);
}

void UIComponent::cleanup() {
    children.clear();
    parent.reset();
    initialized = false;
}

bool UIComponent::onEvent(const UIEvent& event) {
    if (!visible || !enabled) return false;

    // Check if event is within bounds
    if (!contains(event.x, event.y)) return false;

    // Try children first (front-to-back order)
    if (dispatchEventToChildren(event)) return true;

    // Handle by self
    switch (event.type) {
        case UIEventType::TOUCH_DOWN:
            return onTouchDown(event.x, event.y, event.pointerId);
        case UIEventType::TOUCH_UP:
            return onTouchUp(event.x, event.y, event.pointerId);
        case UIEventType::TOUCH_MOVE:
            return onTouchMove(event.x, event.y, event.dx, event.dy, event.pointerId);
        default:
            return false;
    }
}

bool UIComponent::onTouchDown(float /*x*/, float /*y*/, int /*pointerId*/) {
    // Default: no-op
    return false;
}

bool UIComponent::onTouchUp(float /*x*/, float /*y*/, int /*pointerId*/) {
    return false;
}

bool UIComponent::onTouchMove(float /*x*/, float /*y*/, float /*dx*/, float /*dy*/, int /*pointerId*/) {
    return false;
}

void UIComponent::setPosition(float x, float y) {
    position = glm::vec2(x, y);
}

void UIComponent::setSize(float width, float height) {
    size = glm::vec2(width, height);
}

void UIComponent::setAnchor(UIAnchor a) {
    anchor = a;
}

glm::vec2 UIComponent::getAbsolutePosition() const {
    glm::vec2 absPos = position;

    // Apply anchor offset
    switch (anchor) {
        case UIAnchor::TOP_CENTER:
            absPos.x -= size.x * 0.5f;
            break;
        case UIAnchor::TOP_RIGHT:
            absPos.x -= size.x;
            break;
        case UIAnchor::CENTER_LEFT:
            absPos.y -= size.y * 0.5f;
            break;
        case UIAnchor::CENTER:
            absPos.x -= size.x * 0.5f;
            absPos.y -= size.y * 0.5f;
            break;
        case UIAnchor::CENTER_RIGHT:
            absPos.x -= size.x;
            absPos.y -= size.y * 0.5f;
            break;
        case UIAnchor::BOTTOM_LEFT:
            absPos.y -= size.y;
            break;
        case UIAnchor::BOTTOM_CENTER:
            absPos.x -= size.x * 0.5f;
            absPos.y -= size.y;
            break;
        case UIAnchor::BOTTOM_RIGHT:
            absPos.x -= size.x;
            absPos.y -= size.y;
            break;
        default:
            break;
    }

    // Add parent offset if available
    auto p = parent.lock();
    if (p) {
        glm::vec2 parentPos = p->getAbsolutePosition();
        absPos.x += parentPos.x;
        absPos.y += parentPos.y;
    }

    return absPos;
}

bool UIComponent::contains(float x, float y) const {
    // Use a larger hit box for small buttons (like spell panel rows)
    const float HIT_PADDING = 8.0f;
    glm::vec2 absPos = getAbsolutePosition();
    return (x >= absPos.x - HIT_PADDING && x <= absPos.x + size.x + HIT_PADDING &&
            y >= absPos.y - HIT_PADDING && y <= absPos.y + size.y + HIT_PADDING);
}

void UIComponent::onScreenResize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    for (auto& child : children) {
        if (child) child->onScreenResize(width, height);
    }
}

void UIComponent::setParent(std::shared_ptr<UIComponent> p) {
    parent = p;
}

void UIComponent::addChild(std::shared_ptr<UIComponent> child) {
    if (!child) return;
    child->setParent(shared_from_this());
    children.push_back(child);
}

void UIComponent::removeChild(std::shared_ptr<UIComponent> child) {
    if (!child) return;
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        (*it)->setParent(nullptr);
        children.erase(it);
    }
}

// === Protected rendering helpers ===

void UIComponent::renderBackground() const {
    if (backgroundColor.w <= 0.01f) return;
    glm::vec2 absPos = getAbsolutePosition();
    UIDrawHelper::drawColoredQuad(absPos.x, absPos.y, size.x, size.y,
                                  backgroundColor, screenWidth, screenHeight);
}

void UIComponent::renderBorder() const {
    if (borderWidth <= 0.0f || borderColor.w <= 0.01f) return;

    glm::vec2 absPos = getAbsolutePosition();
    float x = absPos.x;
    float y = absPos.y;
    float w = size.x;
    float h = size.y;
    float bw = borderWidth;

    // Top
    UIDrawHelper::drawColoredQuad(x, y, w, bw, borderColor, screenWidth, screenHeight);
    // Bottom
    UIDrawHelper::drawColoredQuad(x, y + h - bw, w, bw, borderColor, screenWidth, screenHeight);
    // Left
    UIDrawHelper::drawColoredQuad(x, y, bw, h, borderColor, screenWidth, screenHeight);
    // Right
    UIDrawHelper::drawColoredQuad(x + w - bw, y, bw, h, borderColor, screenWidth, screenHeight);
}

void UIComponent::renderTexture() const {
    if (textureId == 0) return;
    glm::vec2 absPos = getAbsolutePosition();

    if (textureScaleMode == TextureScaleMode::STRETCH) {
        UIDrawHelper::drawTexturedQuad(absPos.x, absPos.y, size.x, size.y,
                                       textureId, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), screenWidth, screenHeight);
        return;
    }

    int texW = 0, texH = 0;
    if (!TextureLoader::getTextureSize(textureId, texW, texH)) {
        // Fallback to stretch
        UIDrawHelper::drawTexturedQuad(absPos.x, absPos.y, size.x, size.y,
                                       textureId, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), screenWidth, screenHeight);
        return;
    }

    float quadAspect = size.x / size.y;
    float texAspect = static_cast<float>(texW) / static_cast<float>(texH);

    if (textureScaleMode == TextureScaleMode::PRESERVE_ASPECT_FIT) {
        // Fit entire texture inside quad, letterbox/pillarbox
        float drawW = size.x;
        float drawH = size.y;
        float drawX = absPos.x;
        float drawY = absPos.y;

        if (texAspect > quadAspect) {
            // Texture is wider: pillarbox
            drawH = size.x / texAspect;
            drawY = absPos.y + (size.y - drawH) * 0.5f;
        } else {
            // Texture is taller: letterbox
            drawW = size.y * texAspect;
            drawX = absPos.x + (size.x - drawW) * 0.5f;
        }

        UIDrawHelper::drawTexturedQuad(drawX, drawY, drawW, drawH,
                                       textureId, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), screenWidth, screenHeight);
    } else if (textureScaleMode == TextureScaleMode::PRESERVE_ASPECT_CROP) {
        // Fill quad, crop texture to center
        float uMin = 0.0f, vMin = 0.0f, uMax = 1.0f, vMax = 1.0f;

        if (texAspect > quadAspect) {
            // Texture is wider: crop sides
            float scale = quadAspect / texAspect;
            uMin = 0.5f - scale * 0.5f;
            uMax = 0.5f + scale * 0.5f;
        } else {
            // Texture is taller: crop top/bottom
            float scale = texAspect / quadAspect;
            vMin = 0.5f - scale * 0.5f;
            vMax = 0.5f + scale * 0.5f;
        }

        UIDrawHelper::drawTexturedQuad(absPos.x, absPos.y, size.x, size.y,
                                       textureId, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), screenWidth, screenHeight,
                                       uMin, vMin, uMax, vMax);
    }
}

void UIComponent::renderChildren() {
    for (auto& child : children) {
        if (child && child->isVisible()) {
            child->render();
        }
    }
}

void UIComponent::updateChildren(float deltaTime) {
    for (auto& child : children) {
        if (child && child->isVisible()) {
            child->update(deltaTime);
        }
    }
}

bool UIComponent::dispatchEventToChildren(const UIEvent& event) {
    // Iterate in reverse for front-to-back hit testing
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto& child = *it;
        if (child && child->isVisible() && child->isEnabled()) {
            if (child->onEvent(event)) return true;
        }
    }
    return false;
}
