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
    using idleharbor::platform::windows::WindowCoversMonitor;

    constexpr RECT monitor{0, 0, 1920, 1080};
    Expect(WindowCoversMonitor({0, 0, 1920, 1080}, monitor), "exact monitor rectangle is fullscreen");
    Expect(WindowCoversMonitor({1, 1, 1919, 1079}, monitor), "small frame border is tolerated");
    Expect(!WindowCoversMonitor({0, 0, 1280, 720}, monitor), "smaller window is not fullscreen");
    Expect(!WindowCoversMonitor({0, 40, 1920, 1080}, monitor), "window below top edge is not fullscreen");
    Expect(!WindowCoversMonitor({0, 0, 1920, 1040}, monitor), "window above taskbar is not fullscreen");

    const auto minute = idleharbor::platform::windows::LocalMinuteOfDay();
    Expect(minute < 24U * 60U, "local minute is within the day");

    if (failures != 0) {
        std::cerr << failures << " system-snapshot test(s) failed\n";
        return 1;
    }
    return 0;
}
