#pragma once

#include <atomic>
#include <cstdint>

namespace android {

class __attribute__((visibility("default"))) SakuraSensorBridge {
public:
    static std::atomic<float> sGyroRoll;
    static std::atomic<float> sGyroPitch;
    static std::atomic<float> sGyroYaw;
    static std::atomic<float> sAccelX;
    static std::atomic<float> sAccelY;
    static std::atomic<bool> sActive;

    static void setGyroTilt(float roll, float pitch, float yaw, float accelX, float accelY);
    static void reset();
};

} // namespace android
