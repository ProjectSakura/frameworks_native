#include <sensor/SakuraSensorBridge.h>

namespace android {

std::atomic<float> SakuraSensorBridge::sGyroRoll{0.0f};
std::atomic<float> SakuraSensorBridge::sGyroPitch{0.0f};
std::atomic<float> SakuraSensorBridge::sGyroYaw{0.0f};
std::atomic<float> SakuraSensorBridge::sAccelX{0.0f};
std::atomic<float> SakuraSensorBridge::sAccelY{0.0f};
std::atomic<bool> SakuraSensorBridge::sActive{false};

void SakuraSensorBridge::setGyroTilt(float roll, float pitch, float yaw, float accelX, float accelY) {
    sGyroRoll.store(roll, std::memory_order_relaxed);
    sGyroPitch.store(pitch, std::memory_order_relaxed);
    sGyroYaw.store(yaw, std::memory_order_relaxed);
    sAccelX.store(accelX, std::memory_order_relaxed);
    sAccelY.store(accelY, std::memory_order_relaxed);
    sActive.store(true, std::memory_order_relaxed);
}

void SakuraSensorBridge::reset() {
    sGyroRoll.store(0.0f, std::memory_order_relaxed);
    sGyroPitch.store(0.0f, std::memory_order_relaxed);
    sGyroYaw.store(0.0f, std::memory_order_relaxed);
    sAccelX.store(0.0f, std::memory_order_relaxed);
    sAccelY.store(0.0f, std::memory_order_relaxed);
    sActive.store(false, std::memory_order_relaxed);
}

} // namespace android
