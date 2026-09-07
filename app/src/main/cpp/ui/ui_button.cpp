#include "ui_button.h"
#include "text_renderer.h"
#include "ui_system.h"
#include "../audio/audio_manager.h"
#include <cmath>
#include <android/log.h>

#define LOG_TAG "UIButton"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

UIButton::UIButton(const std::string& name)
    : UIComponent(name),
      label("Button"),
      labelColor(1.0f, 1.0f, 1.0f),
      labelScale(1.0f),
      textRenderer(nullptr),
      enabled(true),
      pressed(false),
      hovered(false),
      normalColor(0.25f, 0.25f, 0.3f, 0.9f),
      pressedColor(0.15f, 0.15f, 0.18f, 1.0f),
      hoverColor(0.35f, 0.35f, 0.4f, 0.95f),
      disabledColor(0.15f, 0.15f, 0.15f, 0.5f),
      pressAnimTimer(0.0f),
      hoverScaleTimer(0.0f) {
}

UIButton::~UIButton() {
    cleanup();
}

bool UIButton::initialize() {
    if (!UIComponent::initialize()) return false;
    updateVisualState();
    return true;
}

void UIButton::update(float deltaTime) {
    UIComponent::update(deltaTime);

    // Press animation decay
    if (pressAnimTimer > 0.0f) {
        pressAnimTimer -= deltaTime;
        if (pressAnimTimer < 0.0f) {
            pressAnimTimer = 0.0f;
            pressed = false;
            updateVisualState();
        }
    }

    // Hover scale animation
    if (hovered && hoverScaleTimer < HOVER_SCALE_DURATION) {
        hoverScaleTimer += deltaTime;
        if (hoverScaleTimer > HOVER_SCALE_DURATION) hoverScaleTimer = HOVER_SCALE_DURATION;
    } else if (!hovered && hoverScaleTimer > 0.0f) {
        hoverScaleTimer -= deltaTime;
        if (hoverScaleTimer < 0.0f) hoverScaleTimer = 0.0f;
    }
}

void UIButton::render() {
    if (!isVisible()) return;

    // Update texture based on state before rendering
    GLuint stateTexture = 0;
    if (!enabled && disabledTexture != 0) {
        stateTexture = disabledTexture;
    } else if (pressed && pressedTexture != 0) {
        stateTexture = pressedTexture;
    } else if (hovered && hoverTexture != 0) {
        stateTexture = hoverTexture;
    } else if (normalTexture != 0) {
        stateTexture = normalTexture;
    }
    if (stateTexture != 0) {
        setTexture(stateTexture);
    }

    // Save OpenGL state
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Apply hover scale: expand around center
    glm::vec2 origPos = getPosition();
    glm::vec2 origSize = getSize();
    if (hoverScaleTimer > 0.0f && !pressed) {
        float t = hoverScaleTimer / HOVER_SCALE_DURATION;
        float scale = 1.0f + (HOVER_SCALE_MAX - 1.0f) * t;
        float dw = origSize.x * (scale - 1.0f) * 0.5f;
        float dh = origSize.y * (scale - 1.0f) * 0.5f;
        setPosition(origPos.x - dw, origPos.y - dh);
        setSize(origSize.x * scale, origSize.y * scale);
    }

    // Render button background using updated visual state (includes children)
    UIComponent::render();

    // Render label text
    renderLabel();

    // Restore position/size if scaled
    if (hoverScaleTimer > 0.0f && !pressed) {
        setPosition(origPos.x, origPos.y);
        setSize(origSize.x, origSize.y);
    }

    // Restore state
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
}

void UIButton::cleanup() {
    UIComponent::cleanup();
}

bool UIButton::onEvent(const UIEvent& event) {
    if (!isVisible() || !enabled) {
        LOGD("UIButton::onEvent: not visible or enabled (visible=%d, enabled=%d)", isVisible() ? 1 : 0, enabled ? 1 : 0);
        return false;
    }

    glm::vec2 absPos = getAbsolutePosition();
    LOGI("UIButton::onEvent: touch (%.1f, %.1f) button '%s' at abs(%.1f, %.1f) size(%.1f, %.1f) contains=%d",
         event.x, event.y, label.c_str(), absPos.x, absPos.y, size.x, size.y, contains(event.x, event.y) ? 1 : 0);
    if (!contains(event.x, event.y)) {
        LOGI("UIButton::onEvent: touch (%.1f, %.1f) missed button '%s' at abs(%.1f, %.1f) size(%.1f, %.1f)",
             event.x, event.y, label.c_str(), absPos.x, absPos.y, size.x, size.y);
        if (event.type == UIEventType::TOUCH_UP) {
            pressed = false;
            updateVisualState();
        }
        return false;
    }

    switch (event.type) {
        case UIEventType::TOUCH_DOWN:
            pressed = true;
            pressAnimTimer = PRESS_ANIM_DURATION;
            updateVisualState();
            // Bug #100: Click callback moved to TOUCH_UP for proper behavior
            return true;

        case UIEventType::TOUCH_UP:
            if (pressed) {
                pressed = false;
                updateVisualState();
                // Fire click callback on release (standard button behavior)
                if (onClickCallback) {
                    onClickCallback();
                }
                // Play notification sound
                if (g_audioManager) {
                    g_audioManager->playSound("ui/notification");
                }
            }
            return true;

        case UIEventType::TOUCH_MOVE:
            // Keep pressed state while inside
            return true;

        default:
            return true;
    }
}

bool UIButton::onTouchDown(float x, float y, int pointerId) {
    UIEvent event(UIEventType::TOUCH_DOWN, x, y, pointerId);
    return onEvent(event);
}

bool UIButton::onTouchUp(float x, float y, int pointerId) {
    UIEvent event(UIEventType::TOUCH_UP, x, y, pointerId);
    return onEvent(event);
}

void UIButton::setLabel(const std::string& text) {
    label = text;
}

void UIButton::setEnabled(bool e) {
    enabled = e;
    if (!enabled) {
        pressed = false;
    }
    updateVisualState();
}

void UIButton::updateVisualState() {
    if (!enabled) {
        setBackgroundColor(disabledColor);
    } else if (pressed) {
        setBackgroundColor(pressedColor);
    } else if (hovered) {
        setBackgroundColor(hoverColor);
    } else {
        setBackgroundColor(normalColor);
    }
}

void UIButton::renderLabel() const {
    if (!textRenderer || label.empty()) return;

    glm::vec2 absPos = getAbsolutePosition();

    // Calculate accurate text dimensions
    // getTextWidth returns sum of advanceX; renderText adds bearingX to the first glyph,
    // so factor that in to keep the glyph bounds visually centered inside the button.
    float textWidth = textRenderer->getTextWidth(label, labelScale);
    float firstBearingX = textRenderer->getGlyphBearingX(label[0]) * labelScale;
    float visibleWidth = textWidth + firstBearingX;

    // Calculate vertical metrics from first character's glyph data.
    // renderText places the glyph bitmap at y + (FONT_SIZE + bearingY) * scale.
    // The glyph bitmap height is (y1 - y0) * ATLAS_HEIGHT * scale.
    // So the glyph top relative to the y coordinate is: offsetY
    // And the glyph bottom is: offsetY + height
    // The center of the glyph vertically is: offsetY + height/2
    // To center the glyph in the button, we want:
    //   y + offsetY + height/2 = absPos.y + btnSize.y/2
    // => y = absPos.y + btnSize.y/2 - offsetY - height/2
    float glyphHeight = textRenderer->getGlyphHeight(label[0], labelScale);
    float glyphOffsetY = textRenderer->getGlyphOffsetY(label[0], labelScale);

    // Center the glyph bounds within the button
    glm::vec2 btnSize = getSize();
    float textX = absPos.x + (btnSize.x - visibleWidth) * 0.5f;
    float textY = absPos.y + (btnSize.y * 0.5f) - glyphOffsetY - (glyphHeight * 0.5f);

    // Clamp to integer positions for sharper text
    textX = std::round(textX);
    textY = std::round(textY);

    // If button has transparent background, highlight the label itself on press/hover (classic Oblivion style)
    glm::vec3 drawColor = labelColor;
    if (normalColor.w == 0.0f) {
        if (pressed || hovered) {
            drawColor = glm::vec3(0.95f, 0.88f, 0.65f); // Bright gold highlight
        } else {
            drawColor = glm::vec3(0.65f, 0.58f, 0.44f); // Muted bronze/parchment
        }
    } else {
        if (!enabled) {
            drawColor = labelColor * 0.5f;
        } else if (pressed) {
            drawColor = glm::vec3(1.0f, 1.0f, 1.0f);
        }
    }

    textRenderer->renderText(label, textX, textY, drawColor, labelScale);
}

glm::vec4 UIButton::getCurrentColor() const {
    if (!enabled) return disabledColor;
    if (pressed) return pressedColor;
    if (hovered) return hoverColor;
    return normalColor;
}
