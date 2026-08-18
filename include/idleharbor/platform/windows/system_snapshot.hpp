#pragma once

#include <windows.h>

#include <cstdint>

namespace idleharbor::platform::windows {

struct BatterySnapshot {
    bool available = false;
    bool power_source_known = false;
    bool on_battery = false;
    std::uint8_t percent = 100;
};

[[nodiscard]] BatterySnapshot QueryBatterySnapshot() noexcept;
[[nodiscard]] bool WindowCoversMonitor(const RECT& window, const RECT& monitor, LONG tolerance = 2) noexcept;
[[nodiscard]] bool IsForegroundFullscreen() noexcept;
[[nodiscard]] std::uint16_t LocalMinuteOfDay() noexcept;

}  // namespace idleharbor::platform::windows
