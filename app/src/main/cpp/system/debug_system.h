#pragma once

#include <cstdint>
#include <android/log.h>

class GameConsole;

/**
 * @brief Unified debug system coordinator
 * Provides gesture-based toggling of GameConsole.
 * Wraps existing debug subsystems without replacing them.
 */
class DebugSystem {
public:
    static DebugSystem& getInstance();

    bool initialize(GameConsole* console);
    void cleanup();

    /**
     * @brief Feed touch events for gesture recognition
     * @param action MotionEvent action (0=DOWN, 1=UP, 2=MOVE, 5=POINTER_DOWN, 6=POINTER_UP)
     * @param pointerCount Number of active pointers
     * @param x X coordinate of primary pointer
     * @param y Y coordinate of primary pointer
     * @param eventTime Timestamp in milliseconds
     */
    void onTouch(int action, int pointerCount, float x, float y, int64_t eventTime);

    GameConsole* getConsole() const { return gameConsole; }

private:
    DebugSystem();
    ~DebugSystem();
    DebugSystem(const DebugSystem&) = delete;
    DebugSystem& operator=(const DebugSystem&) = delete;

    GameConsole* gameConsole;

    // Gesture state: 2-finger double-tap detection
    int tapCount;
    int64_t lastTapTime;
    float lastTapX;
    float lastTapY;
    bool twoFingerDown;

    static constexpr int64_t TAP_MAX_DURATION_MS = 300;
    static constexpr float TAP_MAX_DISTANCE_PX = 50.0f;
    static constexpr int64_t DOUBLE_TAP_MAX_INTERVAL_MS = 400;
    static constexpr int MIN_POINTER_COUNT_FOR_2FINGER = 2;
};
