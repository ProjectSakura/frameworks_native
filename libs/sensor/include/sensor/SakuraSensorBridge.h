/*
 * Copyright (C) 2026 Project Sakura Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
