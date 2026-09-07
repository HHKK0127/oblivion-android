#include "ui_system.h"
#include <algorithm>
#include "text_renderer.h"
#include <android/log.h>

#define LOG_TAG "UISystem"
#define SYS_LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define SYS_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define SYS_LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define SYS_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

UISystem::UISystem()
    : textRenderer(nullptr), screenWidth(1920), screenHeight(1080), initialized(false) {
}

UISystem::~UISystem() {
    cleanup();
}

bool UISystem::initialize(TextRenderer* renderer) {
    textRenderer = renderer;
    initialized = true;
    SYS_LOGI("UISystem initialized");
    return true;
}

void UISystem::cleanup() {
    clearComponents();
    textRenderer = nullptr;
    initialized = false;
    SYS_LOGI("UISystem cleaned up");
}

// === Component Management ===

void UISystem::registerComponent(std::shared_ptr<UIComponent> component, int layer) {
    if (!component) return;

    // Initialize if needed
    if (!component->isInitialized()) {
        component->initialize();
    }

    // Remove from existing layer if already registered
    for (auto& [existingLayer, components] : layers) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            components.erase(it);
            break;
        }
    }

    // Add to new layer
    layers[layer].push_back(component);

    // Register in name map
    nameMap[component->getName()] = component;

    SYS_LOGD("Registered UIComponent: %s (id=%u) on layer %d",
             component->getName().c_str(), component->getId(), layer);
}

void UISystem::unregisterComponent(std::shared_ptr<UIComponent> component) {
    if (!component) return;

    // Remove from layer
    for (auto& [layer, components] : layers) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            components.erase(it);
            break;
        }
    }

    // Remove from name map
    nameMap.erase(component->getName());

    SYS_LOGD("Unregistered UIComponent: %s", component->getName().c_str());
}

std::shared_ptr<UIComponent> UISystem::findComponent(const std::string& name) const {
    auto it = nameMap.find(name);
    if (it != nameMap.end()) {
        return it->second.lock();
    }
    return nullptr;
}

void UISystem::clearComponents() {
    layers.clear();
    nameMap.clear();
    focusedComponent.reset();
    SYS_LOGI("All UI components cleared");
}

// === Layer Management ===

void UISystem::setLayer(std::shared_ptr<UIComponent> component, int newLayer) {
    if (!component) return;

    // Find and remove from current layer
    for (auto& [layer, components] : layers) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            components.erase(it);
            break;
        }
    }

    // Add to new layer
    layers[newLayer].push_back(component);
}

// === Update & Render ===

void UISystem::update(float deltaTime) {
    if (!initialized) return;

    for (auto& [layer, components] : layers) {
        for (auto& comp : components) {
            if (comp && comp->isVisible()) {
                comp->update(deltaTime);
            }
        }
    }
}

void UISystem::render() {
    if (!initialized) return;

    // Save OpenGL state
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    GLboolean blendEnabled;
    glGetBooleanv(GL_BLEND, &blendEnabled);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    renderLayers();

    // Restore state
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (!blendEnabled) glDisable(GL_BLEND);
}

void UISystem::renderLayers() {
    // Render in layer order (smallest first = back, largest last = front)
    for (auto& [layer, components] : layers) {
        for (auto& comp : components) {
            if (comp && comp->isVisible()) {
                comp->render();
            }
        }
    }
}

// === Event Dispatch ===

bool UISystem::onTouchDown(float x, float y, int pointerId) {
    LOGI("UISystem::onTouchDown: x=%.1f, y=%.1f, id=%d", x, y, pointerId);
    UIEvent event(UIEventType::TOUCH_DOWN, x, y, pointerId);
    bool result = dispatchEvent(event);
    LOGI("UISystem::onTouchDown: dispatchEvent returned %d", result ? 1 : 0);
    return result;
}

bool UISystem::onTouchUp(float x, float y, int pointerId) {
    LOGI("UISystem::onTouchUp: x=%.1f, y=%.1f, id=%d", x, y, pointerId);
    UIEvent event(UIEventType::TOUCH_UP, x, y, pointerId);
    bool result = dispatchEvent(event);
    LOGI("UISystem::onTouchUp: dispatchEvent returned %d", result ? 1 : 0);
    return result;
}

bool UISystem::onTouchMove(float x, float y, float dx, float dy, int pointerId) {
    UIEvent event(UIEventType::TOUCH_MOVE, x, y, pointerId);
    event.dx = dx;
    event.dy = dy;
    return dispatchEvent(event);
}

bool UISystem::dispatchEvent(const UIEvent& event) {
    if (!initialized) {
        LOGI("UISystem::dispatchEvent: not initialized");
        return false;
    }

    int totalComponents = 0;
    for (const auto& [layer, components] : layers) {
        totalComponents += components.size();
    }
    LOGI("UISystem::dispatchEvent: type=%d, pos=(%.1f, %.1f), layers=%d, totalComponents=%d",
         static_cast<int>(event.type), event.x, event.y,
         static_cast<int>(layers.size()), totalComponents);

    // Bug #74: Copy layers to prevent iterator invalidation if handlers modify layers
    std::map<int, std::vector<std::shared_ptr<UIComponent>>> layersCopy;
    for (const auto& [layer, components] : layers) {
        layersCopy[layer] = components;
    }

    // Dispatch to front-most layers first
    for (auto it = layersCopy.rbegin(); it != layersCopy.rend(); ++it) {
        auto& components = it->second;
        // Iterate in reverse for front-to-back within same layer
        for (auto compIt = components.rbegin(); compIt != components.rend(); ++compIt) {
            auto& comp = *compIt;
            if (!comp) continue;
            glm::vec2 absPos = comp->getAbsolutePosition();
            glm::vec2 sz = comp->getSize();
            bool contains = comp->isVisible() && comp->isEnabled() &&
                            event.x >= absPos.x && event.x <= absPos.x + sz.x &&
                            event.y >= absPos.y && event.y <= absPos.y + sz.y;
            LOGI("  - '%s' at (%.0f, %.0f) size (%.0fx%.0f) vis=%d en=%d hit=%d",
                 comp->getName().c_str(), absPos.x, absPos.y, sz.x, sz.y,
                 comp->isVisible() ? 1 : 0, comp->isEnabled() ? 1 : 0, contains ? 1 : 0);
            if (comp && comp->isVisible() && comp->isEnabled()) {
                if (comp->onEvent(event)) {
                    LOGI("UISystem::dispatchEvent: event consumed by '%s'", comp->getName().c_str());
                    return true;  // Event consumed
                }
            }
        }
    }
    LOGI("UISystem::dispatchEvent: event NOT consumed");
    return false;
}

// === Screen Size ===

void UISystem::setScreenSize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    SYS_LOGI("UISystem screen size set to %dx%d", width, height);

    for (auto& [layer, components] : layers) {
        for (auto& comp : components) {
            if (comp) {
                comp->onScreenResize(width, height);
            }
        }
    }
}

// === Focus Management ===

void UISystem::setFocusedComponent(std::shared_ptr<UIComponent> component) {
    focusedComponent = component;
}

// === Utility ===

void UISystem::setAllVisible(bool visible) {
    for (auto& [layer, components] : layers) {
        for (auto& comp : components) {
            if (comp) comp->setVisible(visible);
        }
    }
}

void UISystem::showOnlyLayer(int targetLayer) {
    for (auto& [layer, components] : layers) {
        bool show = (layer == targetLayer);
        for (auto& comp : components) {
            if (comp) comp->setVisible(show);
        }
    }
}

size_t UISystem::getComponentCount() const {
    size_t count = 0;
    for (const auto& [layer, components] : layers) {
        count += components.size();
    }
    return count;
}
