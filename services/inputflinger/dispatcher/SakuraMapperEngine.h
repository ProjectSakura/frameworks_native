#pragma once

#include <NotifyArgs.h>
#include <input/Input.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace android {

enum class SakuraMappingType {
    TAP,
    JOYSTICK_WASD,
    SWIPE,
};

struct SakuraKeyMapping {
    int32_t keyCode{0};
    SakuraMappingType type{SakuraMappingType::TAP};
    float normX{0.0f};
    float normY{0.0f};
    float radius{0.08f};
    int32_t upKey{0};
    int32_t downKey{0};
    int32_t leftKey{0};
    int32_t rightKey{0};
};

struct SakuraActiveTouch {
    int32_t pointerId{-1};
    float currentX{0.0f};
    float currentY{0.0f};
    int32_t originatingKeyCode{0};
    bool isJoystick{false};
    nsecs_t downTime{0};
};

class SakuraMapperEngine {
public:
    SakuraMapperEngine();
    ~SakuraMapperEngine();

    void setProfile(const std::string& packageName, const std::string& profileJson,
                    int32_t displayWidth, int32_t displayHeight);
    void setActive(bool active);
    void setOverlayShowing(bool showing);

    bool isMapperActive() const;
    bool isOverlayShowing() const;

    bool processKey(const NotifyKeyArgs& keyArgs,
                    std::vector<NotifyMotionArgs>& outSyntheticMotions);

    void setDisplayDimensions(int32_t width, int32_t height);

private:
    mutable Mutex mLock;
    bool mEnabled{false};
    bool mOverlayShowing{false};
    std::string mActivePackage;
    int32_t mDisplayWidth{1080};
    int32_t mDisplayHeight{2400};

    std::unordered_map<int32_t, SakuraKeyMapping> mMappingsByKeyCode;
    std::vector<SakuraKeyMapping> mJoystickMappings;

    std::map<int32_t, SakuraActiveTouch> mActiveTouches;
    std::set<int32_t> mPressedKeys;
    nsecs_t mLastDownTime{0};

    int32_t allocatePointerIdLocked();
    void releasePointerIdLocked(int32_t pointerId);

    void buildMotionArgsLocked(int32_t action, int32_t actionPointerIndex,
                               const NotifyKeyArgs& keyArgs,
                               std::vector<NotifyMotionArgs>& outMotions);

    void parseProfileJsonLocked(const std::string& json);
};

} // namespace android
