#include "idleharbor/platform/windows/system_snapshot.hpp"

#include <wtsapi32.h>

#include <algorithm>
#include <array>

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
        // An unavailable query is not evidence of AC power or of a battery-free machine.
        // Feed the same explicit unknown values used by the API so requested battery
        // safeguards fail closed.
        status.ACLineStatus = 255;
        status.BatteryFlag = 255;
        status.BatteryLifePercent = 255;
    }
    return InterpretBatteryStatus(status);
}

SessionSnapshot QuerySessionSnapshot() noexcept {
    SessionSnapshot snapshot;

    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (WTSQuerySessionInformationW(
            WTS_CURRENT_SERVER_HANDLE,
            WTS_CURRENT_SESSION,
            WTSConnectState,
            &buffer,
            &bytes) == FALSE ||
        buffer == nullptr || bytes < sizeof(WTS_CONNECTSTATE_CLASS)) {
        if (buffer != nullptr) {
            WTSFreeMemory(buffer);
        }
        return snapshot;
    }
    const auto connect_state = *reinterpret_cast<const WTS_CONNECTSTATE_CLASS*>(buffer);
    WTSFreeMemory(buffer);

    if (connect_state == WTSActive) {
        snapshot.disconnected = false;
    } else if (connect_state == WTSDisconnected) {
        snapshot.disconnected = true;
    } else {
        // An unrecognised/intermediate state cannot safely establish the connected state.
        return snapshot;
    }

    const HDESK input_desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (input_desktop == nullptr) {
        return snapshot;
    }

    std::array<wchar_t, 64> desktop_name{};
    DWORD returned = 0;
    const bool name_read = GetUserObjectInformationW(
                               input_desktop,
                               UOI_NAME,
                               desktop_name.data(),
                               static_cast<DWORD>(desktop_name.size() * sizeof(wchar_t)),
                               &returned) != FALSE;
    CloseDesktop(input_desktop);
    if (!name_read || returned < sizeof(wchar_t) || desktop_name[0] == L'\0') {
        return snapshot;
    }

    // The interactive desktop is named Default; secure desktops such as Winlogon mean
    // the workstation is locked. Treat any other readable desktop conservatively.
    snapshot.locked = lstrcmpW(desktop_name.data(), L"Default") != 0;
    snapshot.available = true;
    return snapshot;
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
