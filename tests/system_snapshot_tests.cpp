#include <windows.h>

#include <iostream>
#include <string_view>

#include "idleharbor/platform/windows/system_snapshot.hpp"

namespace {

int failures = 0;

void Expect(const bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using idleharbor::platform::windows::InterpretBatteryStatus;
    using idleharbor::platform::windows::WindowCoversMonitor;

    constexpr RECT monitor{0, 0, 1920, 1080};
    Expect(WindowCoversMonitor({0, 0, 1920, 1080}, monitor), "exact monitor rectangle is fullscreen");
    Expect(WindowCoversMonitor({1, 1, 1919, 1079}, monitor), "small frame border is tolerated");
    Expect(!WindowCoversMonitor({0, 0, 1280, 720}, monitor), "smaller window is not fullscreen");
    Expect(!WindowCoversMonitor({0, 40, 1920, 1080}, monitor), "window below top edge is not fullscreen");
    Expect(!WindowCoversMonitor({0, 0, 1920, 1040}, monitor), "window above taskbar is not fullscreen");

    const auto minute = idleharbor::platform::windows::LocalMinuteOfDay();
    Expect(minute < 24U * 60U, "local minute is within the day");

    SYSTEM_POWER_STATUS unknown{};
    unknown.ACLineStatus = 255;
    unknown.BatteryFlag = 255;
    unknown.BatteryLifePercent = 255;
    const auto unknown_battery = InterpretBatteryStatus(unknown);
    Expect(unknown_battery.available, "unknown battery availability is treated conservatively");
    Expect(!unknown_battery.power_source_known, "unknown power source stays explicit");
    Expect(unknown_battery.on_battery, "unknown power source fails safe as battery power");
    Expect(unknown_battery.percent == 0, "unknown battery percentage fails safe at zero");

    SYSTEM_POWER_STATUS desktop{};
    desktop.ACLineStatus = 1;
    desktop.BatteryFlag = 128;
    desktop.BatteryLifePercent = 255;
    const auto no_battery = InterpretBatteryStatus(desktop);
    Expect(!no_battery.available, "no-battery flag remains distinct from unknown");
    Expect(!no_battery.on_battery, "a battery-free desktop is not treated as on battery");

    SYSTEM_POWER_STATUS known{};
    known.ACLineStatus = 0;
    known.BatteryFlag = 1;
    known.BatteryLifePercent = 55;
    const auto known_battery = InterpretBatteryStatus(known);
    Expect(known_battery.on_battery, "known battery power is detected");
    Expect(known_battery.percent == 55, "known battery percentage is preserved");

    if (failures != 0) {
        std::cerr << failures << " system-snapshot test(s) failed\n";
        return 1;
    }
    return 0;
}
