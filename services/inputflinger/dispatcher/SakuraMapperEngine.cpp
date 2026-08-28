#define LOG_TAG "SakuraMapperEngine"
#include "SakuraMapperEngine.h"

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <cutils/properties.h>
#include <log/log.h>

#include <cmath>
#include <sstream>

namespace android {

SakuraMapperEngine::SakuraMapperEngine() {
    ALOGI("SakuraMapperEngine initialized");
}

SakuraMapperEngine::~SakuraMapperEngine() {
    SakuraSensorBridge::reset();
}

void SakuraMapperEngine::setProfile(const std::string& packageName, const std::string& profileJson,
                                    int32_t displayWidth, int32_t displayHeight) {
    AutoMutex _l(mLock);
    mActivePackage = packageName;
    if (displayWidth > 0) mDisplayWidth = displayWidth;
    if (displayHeight > 0) mDisplayHeight = displayHeight;

    mMappingsByKeyCode.clear();
    mJoystickMappings.clear();
    mGyroMappings.clear();
    mActiveTouches.clear();
    mPressedKeys.clear();
    mLastDownTime = 0;
    SakuraSensorBridge::reset();

    if (!profileJson.empty()) {
        parseProfileJsonLocked(profileJson);
        mEnabled = !mMappingsByKeyCode.empty() || !mJoystickMappings.empty() || !mGyroMappings.empty();
    } else {
        mEnabled = false;
    }

    ALOGI("SakuraMapperEngine profile updated for %s, enabled=%d, mappings=%zu, display=%dx%d",
          mActivePackage.c_str(), mEnabled, mMappingsByKeyCode.size(),
          mDisplayWidth, mDisplayHeight);
}

void SakuraMapperEngine::setActive(bool active) {
    AutoMutex _l(mLock);
    mEnabled = active;
    if (!mEnabled) {
        mActiveTouches.clear();
        mPressedKeys.clear();
        mLastDownTime = 0;
        SakuraSensorBridge::reset();
    }
}

void SakuraMapperEngine::setOverlayShowing(bool showing) {
    AutoMutex _l(mLock);
    mOverlayShowing = showing;
    if (mOverlayShowing) {
        mActiveTouches.clear();
        mPressedKeys.clear();
        mLastDownTime = 0;
        SakuraSensorBridge::reset();
    }
}

bool SakuraMapperEngine::isMapperActive() const {
    AutoMutex _l(mLock);
    return mEnabled && !mOverlayShowing;
}

bool SakuraMapperEngine::isOverlayShowing() const {
    AutoMutex _l(mLock);
    return mOverlayShowing;
}

void SakuraMapperEngine::setDisplayDimensions(int32_t width, int32_t height) {
    AutoMutex _l(mLock);
    if (width > 0) mDisplayWidth = width;
    if (height > 0) mDisplayHeight = height;
}

int32_t SakuraMapperEngine::allocatePointerIdLocked() {
    for (int32_t id = 0; id < 10; ++id) {
        if (mActiveTouches.find(id) == mActiveTouches.end()) {
            return id;
        }
    }
    return -1;
}

void SakuraMapperEngine::releasePointerIdLocked(int32_t pointerId) {
    mActiveTouches.erase(pointerId);
    if (mActiveTouches.empty()) {
        mLastDownTime = 0;
    }
}

void SakuraMapperEngine::parseProfileJsonLocked(const std::string& json) {
    auto findValue = [](const std::string& block, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        size_t pos = block.find(search);
        if (pos == std::string::npos) {
            search = "\"" + key + "\" :";
            pos = block.find(search);
        }
        if (pos == std::string::npos) return "";

        size_t start = pos + search.length();
        while (start < block.length() && (block[start] == ' ' || block[start] == '"')) {
            start++;
        }
        size_t end = start;
        while (end < block.length() && block[end] != ',' && block[end] != '}' && block[end] != '"' && block[end] != '\n') {
            end++;
        }
        return block.substr(start, end - start);
    };

    size_t mapPos = json.find("\"mappings\"");
    if (mapPos == std::string::npos) return;

    size_t arrayStart = json.find('[', mapPos);
    if (arrayStart == std::string::npos) return;

    size_t current = arrayStart + 1;
    while (current < json.length()) {
        size_t objStart = json.find('{', current);
        if (objStart == std::string::npos) break;
        size_t objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos) break;

        std::string objStr = json.substr(objStart, objEnd - objStart + 1);
        std::string typeStr = findValue(objStr, "type");
        std::string xStr = findValue(objStr, "x");
        std::string yStr = findValue(objStr, "y");
        std::string keyStr = findValue(objStr, "key");

        float x = xStr.empty() ? 0.5f : std::stof(xStr);
        float y = yStr.empty() ? 0.5f : std::stof(yStr);

        if (typeStr == "tap" || typeStr.empty()) {
            if (!keyStr.empty()) {
                int32_t k = std::stoi(keyStr);
                SakuraKeyMapping mapping;
                mapping.keyCode = k;
                mapping.type = SakuraMappingType::TAP;
                mapping.normX = x;
                mapping.normY = y;
                mMappingsByKeyCode[k] = mapping;
            }
        } else if (typeStr == "gyro_left") {
            if (!keyStr.empty()) {
                int32_t k = std::stoi(keyStr);
                SakuraKeyMapping mapping;
                mapping.keyCode = k;
                mapping.type = SakuraMappingType::GYRO_LEFT;
                mapping.normX = x;
                mapping.normY = y;
                std::string sensStr = findValue(objStr, "sensitivity");
                mapping.sensitivity = sensStr.empty() ? 1.0f : std::stof(sensStr);
                mMappingsByKeyCode[k] = mapping;
            }
        } else if (typeStr == "gyro_right") {
            if (!keyStr.empty()) {
                int32_t k = std::stoi(keyStr);
                SakuraKeyMapping mapping;
                mapping.keyCode = k;
                mapping.type = SakuraMappingType::GYRO_RIGHT;
                mapping.normX = x;
                mapping.normY = y;
                std::string sensStr = findValue(objStr, "sensitivity");
                mapping.sensitivity = sensStr.empty() ? 1.0f : std::stof(sensStr);
                mMappingsByKeyCode[k] = mapping;
            }
        } else if (typeStr == "joystick_wasd") {
            SakuraKeyMapping mapping;
            mapping.type = SakuraMappingType::JOYSTICK_WASD;
            mapping.normX = x;
            mapping.normY = y;

            std::string rStr = findValue(objStr, "radius");
            mapping.radius = rStr.empty() ? 0.08f : std::stof(rStr);

            std::string upStr = findValue(objStr, "up");
            std::string downStr = findValue(objStr, "down");
            std::string leftStr = findValue(objStr, "left");
            std::string rightStr = findValue(objStr, "right");

            mapping.upKey = upStr.empty() ? 51 : std::stoi(upStr);
            mapping.downKey = downStr.empty() ? 47 : std::stoi(downStr);
            mapping.leftKey = leftStr.empty() ? 29 : std::stoi(leftStr);
            mapping.rightKey = rightStr.empty() ? 32 : std::stoi(rightStr);

            mJoystickMappings.push_back(mapping);
            mMappingsByKeyCode[mapping.upKey] = mapping;
            mMappingsByKeyCode[mapping.downKey] = mapping;
            mMappingsByKeyCode[mapping.leftKey] = mapping;
            mMappingsByKeyCode[mapping.rightKey] = mapping;
        } else if (typeStr == "gyro") {
            SakuraKeyMapping mapping;
            mapping.type = SakuraMappingType::GYRO;
            mapping.normX = x;
            mapping.normY = y;

            std::string sensStr = findValue(objStr, "sensitivity");
            mapping.sensitivity = sensStr.empty() ? 1.0f : std::stof(sensStr);

            std::string upStr = findValue(objStr, "up");
            std::string downStr = findValue(objStr, "down");
            std::string leftStr = findValue(objStr, "left");
            std::string rightStr = findValue(objStr, "right");

            mapping.upKey = upStr.empty() ? 51 : std::stoi(upStr);
            mapping.downKey = downStr.empty() ? 47 : std::stoi(downStr);
            mapping.leftKey = leftStr.empty() ? 35 : std::stoi(leftStr);
            mapping.rightKey = rightStr.empty() ? 36 : std::stoi(rightStr);

            mGyroMappings.push_back(mapping);
            mMappingsByKeyCode[mapping.upKey] = mapping;
            mMappingsByKeyCode[mapping.downKey] = mapping;
            mMappingsByKeyCode[mapping.leftKey] = mapping;
            mMappingsByKeyCode[mapping.rightKey] = mapping;
        }

        current = objEnd + 1;
    }
}

void SakuraMapperEngine::updateVirtualGyroLocked() {
    bool isLandscape = (mDisplayWidth > mDisplayHeight);

    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float accelX = 0.0f;
    float accelY = 0.0f;

    for (const auto& gyro : mGyroMappings) {
        float sens = gyro.sensitivity > 0.0f ? gyro.sensitivity : 1.0f;
        float steer = 0.0f;
        float pitchSteer = 0.0f;

        if (mPressedKeys.count(gyro.leftKey)) steer -= 1.0f;
        if (mPressedKeys.count(gyro.rightKey)) steer += 1.0f;
        if (mPressedKeys.count(gyro.upKey)) pitchSteer += 1.0f;
        if (mPressedKeys.count(gyro.downKey)) pitchSteer -= 1.0f;

        if (isLandscape) {
            accelY += steer * 8.0f * sens;
            accelX += pitchSteer * 7.0f * sens;
            roll += steer * 6.0f * sens;
            yaw += steer * 5.0f * sens;
            pitch += pitchSteer * 5.0f * sens;
        } else {
            accelX += steer * 8.0f * sens;
            accelY += pitchSteer * 7.0f * sens;
            roll += steer * 6.0f * sens;
            pitch += pitchSteer * 5.0f * sens;
        }
    }

    for (const auto& [keyCode, mapping] : mMappingsByKeyCode) {
        if (mPressedKeys.count(keyCode)) {
            float sens = mapping.sensitivity > 0.0f ? mapping.sensitivity : 1.0f;
            if (mapping.type == SakuraMappingType::GYRO_LEFT) {
                if (isLandscape) {
                    accelY -= 8.0f * sens;
                    roll -= 6.0f * sens;
                    yaw -= 5.0f * sens;
                } else {
                    accelX -= 8.0f * sens;
                    roll -= 6.0f * sens;
                }
            } else if (mapping.type == SakuraMappingType::GYRO_RIGHT) {
                if (isLandscape) {
                    accelY += 8.0f * sens;
                    roll += 6.0f * sens;
                    yaw += 5.0f * sens;
                } else {
                    accelX += 8.0f * sens;
                    roll += 6.0f * sens;
                }
            }
        }
    }

    ALOGI("SakuraMapperEngine: Gyro tilt -> isLandscape=%d, accelX=%.2f, accelY=%.2f, roll=%.2f",
          isLandscape, accelX, accelY, roll);

    if (std::abs(roll) > 0.01f || std::abs(pitch) > 0.01f || std::abs(accelX) > 0.01f || std::abs(accelY) > 0.01f) {
        SakuraSensorBridge::setGyroTilt(roll, pitch, yaw, accelX, accelY);
    } else {
        SakuraSensorBridge::reset();
    }
}

void SakuraMapperEngine::buildMotionArgsLocked(int32_t action, int32_t actionPointerIndex,
                                               const NotifyKeyArgs& keyArgs,
                                               std::vector<NotifyMotionArgs>& outMotions) {
    if (mActiveTouches.empty()) return;

    size_t pointerCount = mActiveTouches.size();
    std::vector<PointerProperties> pointerProperties(pointerCount);
    std::vector<PointerCoords> pointerCoords(pointerCount);

    size_t idx = 0;
    for (const auto& [pId, touch] : mActiveTouches) {
        pointerProperties[idx].clear();
        pointerProperties[idx].id = pId;
        pointerProperties[idx].toolType = ToolType::FINGER;

        pointerCoords[idx].clear();
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_X, touch.currentX);
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_Y, touch.currentY);
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, 1.0f);
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_SIZE, 0.05f);
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, 10.0f);
        pointerCoords[idx].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, 10.0f);
        idx++;
    }

    int32_t finalAction = action;
    if (pointerCount > 1 && (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_UP)) {
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            finalAction = (actionPointerIndex << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT) |
                          AMOTION_EVENT_ACTION_POINTER_DOWN;
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            finalAction = (actionPointerIndex << AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT) |
                          AMOTION_EVENT_ACTION_POINTER_UP;
        }
    }

    NotifyMotionArgs motionArgs(
            keyArgs.id,
            keyArgs.eventTime,
            keyArgs.readTime,
            0,
            AINPUT_SOURCE_TOUCHSCREEN,
            ui::LogicalDisplayId::DEFAULT,
            POLICY_FLAG_TRUSTED | POLICY_FLAG_PASS_TO_USER,
            finalAction,
            0,
            0,
            0,
            0,
            MotionClassification::NONE,
            pointerCount,
            pointerProperties.data(),
            pointerCoords.data(),
            1.0f,
            1.0f,
            0.0f,
            0.0f,
            mLastDownTime > 0 ? mLastDownTime : keyArgs.downTime,
            {});

    outMotions.push_back(motionArgs);
}

bool SakuraMapperEngine::processKey(const NotifyKeyArgs& keyArgs,
                                    std::vector<NotifyMotionArgs>& outSyntheticMotions) {
    AutoMutex _l(mLock);
    if (!mEnabled) {
        return false;
    }

    auto it = mMappingsByKeyCode.find(keyArgs.keyCode);
    if (it == mMappingsByKeyCode.end()) {
        return false;
    }

    const SakuraKeyMapping& mapping = it->second;
    bool isDown = (keyArgs.action == AKEY_EVENT_ACTION_DOWN);

    ALOGI("SakuraMapperEngine: key=%d, action=%d, isDown=%d, type=%d",
          keyArgs.keyCode, keyArgs.action, isDown, (int)mapping.type);

    if (isDown) {
        mPressedKeys.insert(keyArgs.keyCode);
    } else {
        mPressedKeys.erase(keyArgs.keyCode);
    }

    if (mapping.type == SakuraMappingType::GYRO ||
        mapping.type == SakuraMappingType::GYRO_LEFT ||
        mapping.type == SakuraMappingType::GYRO_RIGHT) {
        updateVirtualGyroLocked();
        return true;
    } else if (mapping.type == SakuraMappingType::TAP) {
        float posX = mapping.normX * mDisplayWidth;
        float posY = mapping.normY * mDisplayHeight;

        if (isDown) {
            int32_t pointerId = allocatePointerIdLocked();
            if (pointerId < 0) return true;

            if (mActiveTouches.empty()) {
                mLastDownTime = keyArgs.eventTime;
            }

            SakuraActiveTouch touch;
            touch.pointerId = pointerId;
            touch.currentX = posX;
            touch.currentY = posY;
            touch.originatingKeyCode = keyArgs.keyCode;
            touch.isJoystick = false;
            touch.downTime = keyArgs.eventTime;
            mActiveTouches[pointerId] = touch;

            size_t pointerIndex = 0;
            for (const auto& [pId, _] : mActiveTouches) {
                if (pId == pointerId) break;
                pointerIndex++;
            }

            buildMotionArgsLocked(AMOTION_EVENT_ACTION_DOWN, static_cast<int32_t>(pointerIndex),
                                  keyArgs, outSyntheticMotions);
        } else {
            int32_t targetPointerId = -1;
            size_t pointerIndex = 0;
            for (const auto& [pId, touch] : mActiveTouches) {
                if (touch.originatingKeyCode == keyArgs.keyCode) {
                    targetPointerId = pId;
                    break;
                }
                pointerIndex++;
            }

            if (targetPointerId >= 0) {
                buildMotionArgsLocked(AMOTION_EVENT_ACTION_UP, static_cast<int32_t>(pointerIndex),
                                      keyArgs, outSyntheticMotions);
                releasePointerIdLocked(targetPointerId);
            }
        }
        return true;
    } else if (mapping.type == SakuraMappingType::JOYSTICK_WASD) {
        int32_t jsPointerId = -1;
        for (const auto& [pId, touch] : mActiveTouches) {
            if (touch.isJoystick) {
                jsPointerId = pId;
                break;
            }
        }

        float dx = 0.0f;
        float dy = 0.0f;
        if (mPressedKeys.count(mapping.upKey)) dy -= 1.0f;
        if (mPressedKeys.count(mapping.downKey)) dy += 1.0f;
        if (mPressedKeys.count(mapping.leftKey)) dx -= 1.0f;
        if (mPressedKeys.count(mapping.rightKey)) dx += 1.0f;

        float length = std::hypot(dx, dy);
        if (length > 0.0001f) {
            dx /= length;
            dy /= length;
        }

        float centerX = mapping.normX * mDisplayWidth;
        float centerY = mapping.normY * mDisplayHeight;
        float maxRadius = mapping.radius * mDisplayWidth;

        float targetX = centerX + (dx * maxRadius);
        float targetY = centerY + (dy * maxRadius);

        if (length > 0.0001f) {
            if (jsPointerId < 0) {
                jsPointerId = allocatePointerIdLocked();
                if (jsPointerId < 0) return true;

                if (mActiveTouches.empty()) {
                    mLastDownTime = keyArgs.eventTime;
                }

                SakuraActiveTouch touch;
                touch.pointerId = jsPointerId;
                touch.currentX = targetX;
                touch.currentY = targetY;
                touch.originatingKeyCode = keyArgs.keyCode;
                touch.isJoystick = true;
                touch.downTime = keyArgs.eventTime;
                mActiveTouches[jsPointerId] = touch;

                size_t pointerIndex = 0;
                for (const auto& [pId, _] : mActiveTouches) {
                    if (pId == jsPointerId) break;
                    pointerIndex++;
                }

                buildMotionArgsLocked(AMOTION_EVENT_ACTION_DOWN, static_cast<int32_t>(pointerIndex),
                                      keyArgs, outSyntheticMotions);
            } else {
                mActiveTouches[jsPointerId].currentX = targetX;
                mActiveTouches[jsPointerId].currentY = targetY;

                buildMotionArgsLocked(AMOTION_EVENT_ACTION_MOVE, 0, keyArgs, outSyntheticMotions);
            }
        } else {
            if (jsPointerId >= 0) {
                size_t pointerIndex = 0;
                for (const auto& [pId, _] : mActiveTouches) {
                    if (pId == jsPointerId) break;
                    pointerIndex++;
                }

                buildMotionArgsLocked(AMOTION_EVENT_ACTION_UP, static_cast<int32_t>(pointerIndex),
                                      keyArgs, outSyntheticMotions);
                releasePointerIdLocked(jsPointerId);
            }
        }
        return true;
    }

    return false;
}

} // namespace android
