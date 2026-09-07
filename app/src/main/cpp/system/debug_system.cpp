#include "debug_system.h"
#include "game_console.h"
#include <cmath>
#include <android/log.h>

#define LOG_TAG "DebugSystem"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

DebugSystem& DebugSystem::getInstance() {
    static DebugSystem instance;
    return instance;
}

DebugSystem::DebugSystem()
    : gameConsole(nullptr),
      tapCount(0), lastTapTime(0), lastTapX(0), lastTapY(0),
      twoFingerDown(false) {
}

DebugSystem::~DebugSystem() {
    cleanup();
}

bool DebugSystem::initialize(GameConsole* console) {
    gameConsole = console;
    LOGI("DebugSystem initialized (Console=%p)",
         (void*)console);
    return true;
}

void DebugSystem::cleanup() {
    gameConsole = nullptr;
    LOGI("DebugSystem cleaned up");
}

void DebugSystem::onTouch(int action, int pointerCount, float x, float y, int64_t eventTime) {
    // ACTION_DOWN=0, ACTION_UP=1, ACTION_MOVE=2,
    // ACTION_POINTER_DOWN=5, ACTION_POINTER_UP=6

    // --- 2-finger double-tap detection -> toggle GameConsole ---
    if (pointerCount >= MIN_POINTER_COUNT_FOR_2FINGER) {
        if (action == 5 || action == 0) {
            twoFingerDown = true;
        }
    }

    if (twoFingerDown && (action == 1 || action == 6)) {
        twoFingerDown = false;

        // Check if this is a continuation of a double-tap
        if (eventTime - lastTapTime <= DOUBLE_TAP_MAX_INTERVAL_MS) {
            float dx = x - lastTapX;
            float dy = y - lastTapY;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= TAP_MAX_DISTANCE_PX * 2.0f) {
                // 2-finger double-tap confirmed -> toggle GameConsole
                if (gameConsole) {
                    bool wasVisible = gameConsole->isVisible();
                    gameConsole->setVisible(!wasVisible);
                    LOGI("2-finger double-tap: GameConsole %s",
                         gameConsole->isVisible() ? "shown" : "hidden");
                }
                tapCount = 0;
                lastTapTime = 0;
                return;
            }
        }

        // First tap of potential double-tap
        tapCount = 1;
        lastTapTime = eventTime;
        lastTapX = x;
        lastTapY = y;
    }

    // Reset tap count if too much time passed
    if (tapCount > 0 && (eventTime - lastTapTime) > DOUBLE_TAP_MAX_INTERVAL_MS) {
        tapCount = 0;
    }
}
