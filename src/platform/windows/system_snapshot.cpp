#include "idleharbor/platform/windows/system_snapshot.hpp"

#include <algorithm>

namespace idleharbor::platform::windows {

BatterySnapshot InterpretBatteryStatus(const SYSTEM_POWER_STATUS& status) noexcept {
    BatterySnapshot snapshot;
    snapshot.available = status.BatteryFlag != 128U;
    snapshot.power_source_known = status.ACLineStatus != 255;
    snapshot.on_battery = snapshot.available && (status.ACLineStatus == 0 || !snapshot.power_source_known);
    if (snapshot.available && status.BatteryLifePercent != 255) {
        snapshot.percent = std::min<std::uint8_t>(status.BatteryLifePercent, 100);
    }
    return snapshot;
}

BatterySnapshot QueryBatterySnapshot() noexcept {
    SYSTEM_POWER_STATUS status{};
    if (GetSystemPowerStatus(&status) == FALSE) {
        return {};
    }
    return InterpretBatteryStatus(status);
}

bool WindowCoversMonitor(const RECT& window, const RECT& monitor, const LONG tolerance) noexcept {
    const LONG safe_tolerance = std::max<LONG>(tolerance, 0);
    return window.left <= monitor.left + safe_tolerance && window.top <= monitor.top + safe_tolerance &&
           window.right >= monitor.right - safe_tolerance && window.bottom >= monitor.bottom - safe_tolerance;
}

bool IsForegroundFullscreen() noexcept {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr || IsIconic(foreground) != FALSE || IsWindowVisible(foreground) == FALSE) {
        return false;
    }

    wchar_t class_name[64]{};
    const int class_length = GetClassNameW(foreground, class_name, static_cast<int>(std::size(class_name)));
    if (class_length > 0 &&
        (lstrcmpW(class_name, L"Progman") == 0 || lstrcmpW(class_name, L"WorkerW") == 0 ||
         lstrcmpW(class_name, L"Shell_TrayWnd") == 0)) {
        return false;
    }

    RECT window{};
    if (GetWindowRect(foreground, &window) == FALSE) {
        return false;
    }

    const HMONITOR monitor_handle = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
    if (monitor_handle == nullptr) {
        return false;
    }
    MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(monitor_handle, &monitor) == FALSE) {
        return false;
    }
    return WindowCoversMonitor(window, monitor.rcMonitor);
}

std::uint16_t LocalMinuteOfDay() noexcept {
    SYSTEMTIME local_time{};
    GetLocalTime(&local_time);
    return static_cast<std::uint16_t>(local_time.wHour * 60U + local_time.wMinute);
}

}  // namespace idleharbor::platform::windows
