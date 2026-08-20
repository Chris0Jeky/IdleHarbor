#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "idleharbor/app/command_line.hpp"
#include "idleharbor/app/settings_store.hpp"
#include "idleharbor/app/window_layout.hpp"
#include "idleharbor/core.hpp"
#include "idleharbor/platform/windows/input_monitor.hpp"
#include "idleharbor/platform/windows/motion_emitter.hpp"
#include "idleharbor/platform/windows/power_request.hpp"
#include "idleharbor/platform/windows/system_snapshot.hpp"
#include "idleharbor/version.hpp"

namespace {

using idleharbor::app::CommandLineOptions;
using idleharbor::app::RequestedCommand;
using idleharbor::core::DecisionAction;
using idleharbor::core::EngineState;
using idleharbor::core::MotionMode;
using idleharbor::core::PolicyDecision;
using idleharbor::core::PolicyEngine;
using idleharbor::core::PolicyInput;
using idleharbor::core::PolicyReason;
using idleharbor::core::PowerMode;
using idleharbor::core::ProfileKind;
using idleharbor::core::Seconds;
using idleharbor::core::Settings;

using idleharbor::platform::windows::InputMonitor;
using idleharbor::platform::windows::MotionEmissionResult;
using idleharbor::platform::windows::PowerRequest;
using idleharbor::platform::windows::PowerRequestMode;

constexpr wchar_t kWindowClassName[] = L"IdleHarbor.MainWindow";
constexpr wchar_t kMutexName[] = L"Local\\IdleHarbor.Singleton.v1";
constexpr ULONG_PTR kCopyDataCommand = 0x49444843;  // IDHC
constexpr int kIconResourceId = 101;

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kGenuineInputMessage = WM_APP + 2;
constexpr UINT kDeferredCommandMessage = WM_APP + 3;
constexpr UINT kDeferredFocusMessage = WM_APP + 4;
constexpr UINT kTimerId = 1;
constexpr UINT_PTR kChildSubclassId = 1;
constexpr int kEmergencyHotkeyId = 1;
constexpr ULONGLONG kInputHookRefreshIntervalMs = 10'000;
constexpr std::size_t kMaximumDeferredCommands = 32;
constexpr int kBaseWindowWidth = 600;
constexpr int kBaseWindowHeight = 700;
constexpr int kBaseWindowMargin = 12;
constexpr int kBaseScrollLine = 32;
constexpr int kBaseMinimumClientHeight = 320;
constexpr int kBaseBodyOrigin = 52;
constexpr int kBaseBodyContentHeight = 570;

enum ControlId : int {
    kStatus = 100,
    kProfile = 101,
    kMotion = 102,
    kPower = 103,
    kInterval = 104,
    kDistance = 105,
    kRandomize = 106,
    kPauseInput = 107,
    kLockPause = 108,
    kBattery = 109,
    kFullscreen = 110,
    kMaxDuration = 111,
    kStartMinimized = 112,
    kCloseToTray = 113,
    kDisconnectPause = 114,
    kPauseOnBattery = 115,
    kNotifications = 116,
    kEmergencyHotkey = 117,
    kStart = 120,
    kStop = 121,
    kSave = 122,
};

constexpr std::array<ProfileKind, 7> kProfiles{
    ProfileKind::Balanced,
    ProfileKind::LongTask,
    ProfileKind::Presentation,
    ProfileKind::Compatibility,
    ProfileKind::Visible,
    ProfileKind::BatterySaver,
    ProfileKind::Custom,
};
constexpr std::array<MotionMode, 5> kMotionModes{
    MotionMode::Off,
    MotionMode::Normal,
    MotionMode::Zen,
    MotionMode::Circle,
    MotionMode::Linear,
};
constexpr std::array<PowerMode, 3> kPowerModes{
    PowerMode::None,
    PowerMode::System,
    PowerMode::Display,
};

std::wstring Lower(std::wstring_view value) {
    std::wstring result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](const wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return result;
}

std::wstring Widen(const std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::wstring ControlText(const HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(std::max(length, 0)) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
    text.resize(static_cast<std::size_t>(std::max(length, 0)));
    return text;
}

void SetControlText(const HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

void SetChecked(const HWND control, const bool checked) {
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool IsChecked(const HWND control) {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

std::optional<std::uint64_t> ParseUnsigned(const std::wstring& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t result = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(character - L'0');
        if (result > (UINT64_MAX - digit) / 10) {
            return std::nullopt;
        }
        result = result * 10 + digit;
    }
    return result;
}

std::wstring WindowsErrorText(const DWORD error) {
    std::wostringstream result;
    result << L"Windows error " << error;
    return result.str();
}

bool SettingsEqual(const Settings& left, const Settings& right) noexcept {
    return left.profile == right.profile && left.motion == right.motion && left.power == right.power &&
           left.interval == right.interval && left.random_minimum == right.random_minimum &&
           left.distance == right.distance && left.randomize == right.randomize &&
           left.pause_on_user_activity == right.pause_on_user_activity &&
           left.user_activity_cooldown == right.user_activity_cooldown &&
           left.pause_when_locked == right.pause_when_locked &&
           left.pause_when_disconnected == right.pause_when_disconnected &&
           left.pause_on_battery == right.pause_on_battery && left.pause_on_low_battery == right.pause_on_low_battery &&
           left.low_battery_threshold == right.low_battery_threshold &&
           left.pause_when_fullscreen == right.pause_when_fullscreen &&
           left.active_hours.enabled == right.active_hours.enabled &&
           left.active_hours.start_minute == right.active_hours.start_minute &&
           left.active_hours.end_minute == right.active_hours.end_minute &&
           left.max_duration == right.max_duration;
}

bool AppSettingsEqual(const idleharbor::app::AppSettings& left, const idleharbor::app::AppSettings& right) noexcept {
    return SettingsEqual(left.session, right.session) && left.start_minimized == right.start_minimized &&
           left.close_to_tray == right.close_to_tray && left.show_notifications == right.show_notifications &&
           left.emergency_hotkey == right.emergency_hotkey;
}

std::optional<ProfileKind> ProfileFromText(std::wstring_view raw) {
    const auto value = Lower(raw);
    for (const auto profile : kProfiles) {
        if (value == Widen(idleharbor::core::profile_kind_name(profile))) {
            return profile;
        }
    }
    return std::nullopt;
}

std::optional<MotionMode> MotionFromText(std::wstring_view raw) {
    auto value = Lower(raw);
    if (value == L"diagonal" || value == L"normal") {
        value = L"normal";
    }
    for (const auto mode : kMotionModes) {
        if (value == Widen(idleharbor::core::motion_mode_name(mode))) {
            return mode;
        }
    }
    return std::nullopt;
}

std::optional<PowerMode> PowerFromText(std::wstring_view raw) {
    const auto value = Lower(raw);
    for (const auto mode : kPowerModes) {
        if (value == Widen(idleharbor::core::power_mode_name(mode))) {
            return mode;
        }
    }
    return std::nullopt;
}

int ComboIndex(const HWND combo) {
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
}

Seconds NowSeconds() {
    return Seconds{static_cast<std::int64_t>(GetTickCount64() / 1000)};
}

int ScaleForDpi(const int value, const UINT dpi) noexcept {
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

HFONT CreateUiFont(const UINT dpi) noexcept {
    return CreateFontW(
        -MulDiv(9, static_cast<int>(dpi), 72),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
}

PowerRequestMode ToPlatformPowerMode(const PowerMode mode) {
    switch (mode) {
    case PowerMode::None:
        return PowerRequestMode::None;
    case PowerMode::System:
        return PowerRequestMode::System;
    case PowerMode::Display:
        return PowerRequestMode::Display;
    }
    return PowerRequestMode::None;
}

HICON LoadIdleHarborIcon(const HINSTANCE instance) {
    const HICON icon = LoadIconW(instance, MAKEINTRESOURCEW(kIconResourceId));
    return icon != nullptr ? icon : LoadIconW(nullptr, IDI_APPLICATION);
}

bool HasRuntimeOverrides(const CommandLineOptions& options) noexcept {
    return options.profile.has_value() || options.motion_mode.has_value() || options.power_mode.has_value() ||
           options.interval.has_value() || options.pause_on_input.has_value() || options.stop_after.has_value() ||
           options.distance.has_value() || options.battery_threshold.has_value() || options.randomize.has_value() ||
           options.pause_on_fullscreen.has_value();
}

class Application final {
  public:
    explicit Application(const HINSTANCE instance) : instance_(instance) {}
    ~Application() = default;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize(const CommandLineOptions& options) {
        INITCOMMONCONTROLSEX common_controls{sizeof(common_controls)};
        common_controls.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
        if (InitCommonControlsEx(&common_controls) == FALSE) {
            return false;
        }
        settings_path_ = idleharbor::app::ResolveSettingsPath(options.portable, options.config_path);
        const auto loaded_settings = idleharbor::app::LoadSettings(settings_path_);
        settings_ = loaded_settings.settings;
        settings_load_warnings_ = loaded_settings.warnings;
        ApplyCommandLineOptions(options);
        saved_settings_ = settings_;
        dirty_ = false;
        taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
        dpi_ = GetDpiForSystem();

        WNDCLASSEXW window_class{sizeof(window_class)};
        window_class.hInstance = instance_;
        window_class.lpfnWndProc = WindowProc;
        window_class.lpszClassName = kWindowClassName;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIdleHarborIcon(instance_);
        window_class.hIconSm = window_class.hIcon;
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        window_ = CreateWindowExW(
            0,
            kWindowClassName,
            idleharbor::kProductName.data(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            ScaleForDpi(kBaseWindowWidth, dpi_),
            ScaleForDpi(kBaseWindowHeight, dpi_),
            nullptr,
            nullptr,
            instance_,
            this);
        if (window_ == nullptr) {
            return false;
        }
        ResizeToPreferredWorkArea();

        InitializeTrayIcon();
        session_notifications_available_ =
            WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION) != FALSE;
        if (session_notifications_available_) {
            const auto session = idleharbor::platform::windows::QuerySessionSnapshot();
            session_state_available_ = session.available;
            if (session_state_available_) {
                locked_ = session.locked;
                disconnected_ = session.disconnected;
            }
        }
        ApplyEmergencyHotkeySetting();

        RefreshControls();
        std::wstring initial_status = L"Stopped: ready";
        if (!settings_load_warnings_.empty()) {
            initial_status = L"Stopped: settings recovered; review and save before automatic start";
        } else if (!tray_added_) {
            initial_status = L"Stopped: notification icon unavailable; window kept visible";
        } else if (settings_.emergency_hotkey && !hotkey_registered_) {
            initial_status = L"Stopped: emergency hotkey unavailable";
        } else if (settings_.session.pause_when_locked || settings_.session.pause_when_disconnected) {
            if (!session_notifications_available_) {
                initial_status = L"Stopped: session-change observer unavailable";
            } else if (!session_state_available_) {
                initial_status = L"Stopped: current session state unavailable";
            }
        }
        SetStatus(initial_status);
        if (settings_load_warnings_.empty() && (options.minimized || settings_.start_minimized) && tray_added_) {
            ShowWindow(window_, SW_HIDE);
        } else {
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
            SetFocus(profile_);
        }
        ShowSettingsLoadWarnings();
        return true;
    }

    void HandleCommand(const CommandLineOptions& options) {
        const bool restart_for_overrides = session_active_ && HasRuntimeOverrides(options);
        const bool automatic_start_blocked =
            !settings_load_warnings_.empty() &&
            (options.command == RequestedCommand::Start ||
             (options.command == RequestedCommand::Toggle && !session_active_));
        ApplyCommandLineOptions(options);
        RefreshControls();
        if (automatic_start_blocked) {
            ShowWindow(window_, SW_SHOW);
            SetForegroundWindow(window_);
            if (!session_active_) {
                SetStatus(L"Stopped: settings recovered; review and save before automatic start");
            }
            return;
        }
        switch (options.command) {
        case RequestedCommand::Launch:
            if (options.minimized && tray_added_ && settings_load_warnings_.empty()) {
                ShowWindow(window_, SW_HIDE);
            } else {
                ShowWindow(window_, SW_SHOW);
                SetForegroundWindow(window_);
            }
            break;
        case RequestedCommand::Start:
            ShowWindow(window_, options.minimized && tray_added_ ? SW_HIDE : SW_SHOW);
            if (restart_for_overrides) {
                StopSession();
            }
            StartSession();
            break;
        case RequestedCommand::Stop:
            StopSession();
            break;
        case RequestedCommand::Toggle:
            if (session_active_) {
                StopSession();
            } else {
                StartSession();
            }
            break;
        case RequestedCommand::Status:
            MessageBoxW(window_, status_text_.c_str(), L"IdleHarbor status", MB_OK | MB_ICONINFORMATION);
            break;
        case RequestedCommand::Show:
            ShowWindow(window_, SW_SHOW);
            SetForegroundWindow(window_);
            break;
        case RequestedCommand::Exit:
            RequestExit();
            break;
        }
    }

    [[nodiscard]] HWND window() const noexcept { return window_; }

    void MaintainFocusedControlVisibility() {
        ObserveFocusChange();
    }

    void ObserveFocusChange() {
        const HWND focused = GetFocus();
        const auto previous = reinterpret_cast<std::uintptr_t>(last_focus_);
        const auto current = reinterpret_cast<std::uintptr_t>(focused);
        if (!idleharbor::app::FocusChanged(previous, current)) {
            return;
        }
        last_focus_ = focused;
        EnsureFocusedControlVisible();
    }

    [[nodiscard]] bool HandleTabNavigation(const MSG& message) {
        if (message.message != WM_KEYDOWN || message.wParam != VK_TAB) {
            return false;
        }
        std::vector<HWND> focusable;
        for (const auto& child : child_layouts_) {
            if ((GetWindowLongPtrW(child.window, GWL_STYLE) & WS_TABSTOP) != 0 &&
                IsWindowVisible(child.window) != FALSE && IsWindowEnabled(child.window) != FALSE) {
                focusable.push_back(child.window);
            }
        }
        if (focusable.empty()) {
            return false;
        }
        std::stable_sort(focusable.begin(), focusable.end(), [&](const HWND left, const HWND right) {
            const auto position_for = [&](const HWND window) {
                const auto child = std::find_if(child_layouts_.begin(), child_layouts_.end(), [&](const ChildLayout& item) {
                    return item.window == window;
                });
                if (child == child_layouts_.end()) {
                    return idleharbor::app::TabOrderPosition{};
                }
                const auto region = child->region == LayoutRegion::Body
                                        ? idleharbor::app::TabOrderRegion::Body
                                        : child->region == LayoutRegion::FixedTop
                                              ? idleharbor::app::TabOrderRegion::FixedTop
                                              : idleharbor::app::TabOrderRegion::FixedBottom;
                return idleharbor::app::TabOrderPosition{
                    region,
                    child->region == LayoutRegion::Body ? child->arranged_y : 0,
                    child->region == LayoutRegion::Body ? child->arranged_x : child->x,
                    static_cast<int>(child - child_layouts_.begin()),
                };
            };
            return idleharbor::app::TabOrderBefore(position_for(left), position_for(right));
        });
        const auto current = std::find(focusable.begin(), focusable.end(), GetFocus());
        const bool reverse = (GetKeyState(VK_SHIFT) & static_cast<SHORT>(0x8000)) != 0;
        std::size_t index = current == focusable.end() ? 0 : static_cast<std::size_t>(current - focusable.begin());
        if (reverse) {
            index = index == 0 ? focusable.size() - 1 : index - 1;
        } else {
            index = (index + 1) % focusable.size();
        }
        SetFocus(focusable[index]);
        return true;
    }

    static LRESULT CALLBACK WindowProc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param) noexcept {
        Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            application = static_cast<Application*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
            application->window_ = window;
        }
        if (application != nullptr) {
            return application->HandleMessage(message, w_param, l_param);
        }
        return DefWindowProcW(window, message, w_param, l_param);
    }

    static LRESULT CALLBACK ChildWindowProc(
        const HWND window,
        const UINT message,
        const WPARAM w_param,
        const LPARAM l_param,
        const UINT_PTR subclass_id,
        const DWORD_PTR reference_data) noexcept {
        auto* application = reinterpret_cast<Application*>(reference_data);
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, ChildWindowProc, subclass_id);
        } else if (message == WM_COMMAND && application != nullptr) {
            return application->HandleMessage(message, w_param, l_param);
        } else if (message == WM_SETFOCUS && application != nullptr) {
            application->ObserveFocusChange();
        } else if (message == WM_VSCROLL && application != nullptr && window == application->settings_viewport_) {
            application->HandleVerticalScroll(w_param);
            return 0;
        } else if (message == WM_MOUSEWHEEL && application != nullptr) {
            if (application->IsDroppedComboBox(window)) {
                return DefSubclassProc(window, message, w_param, l_param);
            }
            application->HandleMouseWheel(w_param);
            return 0;
        }
        return DefSubclassProc(window, message, w_param, l_param);
    }

  private:
    enum class ChildWidthMode {
        Fixed,
        Fill,
    };

    enum class LayoutRegion {
        Body,
        FixedTop,
        FixedBottom,
    };

    enum class BodyControlKind {
        Generic,
        Label,
        Heading,
        Field,
        Check,
    };

    struct ChildLayout {
        HWND window = nullptr;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int focus_height = 0;
        ChildWidthMode width_mode = ChildWidthMode::Fixed;
        LayoutRegion region = LayoutRegion::Body;
        BodyControlKind kind = BodyControlKind::Generic;
        int arranged_x = 0;
        int arranged_y = 0;
        int arranged_width = 0;
    };

    [[nodiscard]] int Scale(const int value) const noexcept { return ScaleForDpi(value, dpi_); }

    [[nodiscard]] idleharbor::app::PixelRect WorkAreaFor(const RECT& rectangle) const noexcept {
        MONITORINFO monitor_info{sizeof(monitor_info)};
        const HMONITOR monitor = MonitorFromRect(&rectangle, MONITOR_DEFAULTTONEAREST);
        if (monitor != nullptr && GetMonitorInfoW(monitor, &monitor_info) != FALSE) {
            return {
                monitor_info.rcWork.left,
                monitor_info.rcWork.top,
                monitor_info.rcWork.right,
                monitor_info.rcWork.bottom,
            };
        }
        RECT work_area{};
        if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0) != FALSE) {
            return {work_area.left, work_area.top, work_area.right, work_area.bottom};
        }
        return {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
    }

    [[nodiscard]] RECT ClampToWorkArea(const RECT& desired) const noexcept {
        const auto clamped = idleharbor::app::ClampWindowRect(
            {desired.left, desired.top, desired.right, desired.bottom},
            WorkAreaFor(desired),
            Scale(kBaseWindowMargin));
        return {clamped.left, clamped.top, clamped.right, clamped.bottom};
    }

    void ApplyMinimumTrackingSize(MINMAXINFO* info) const noexcept {
        if (info == nullptr) {
            return;
        }
        RECT current{};
        if (GetWindowRect(window_, &current) == FALSE) {
            return;
        }
        const auto work_area = WorkAreaFor(current);
        const int available_width = std::max(work_area.right - work_area.left - 2 * Scale(kBaseWindowMargin), 1);
        const int available_height = std::max(work_area.bottom - work_area.top - 2 * Scale(kBaseWindowMargin), 1);
        info->ptMinTrackSize.x = std::min(Scale(kBaseWindowWidth), available_width);
        info->ptMinTrackSize.y = std::min(Scale(kBaseMinimumClientHeight), available_height);
    }

    void ResizeToPreferredWorkArea() {
        RECT current{};
        if (GetWindowRect(window_, &current) == FALSE) {
            return;
        }
        RECT desired{
            current.left,
            current.top,
            current.left + Scale(kBaseWindowWidth),
            current.top + Scale(kBaseWindowHeight),
        };
        const RECT target = ClampToWorkArea(desired);
        SetWindowPos(
            window_,
            nullptr,
            target.left,
            target.top,
            target.right - target.left,
            target.bottom - target.top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        UpdateViewport();
    }

    void TrackChild(
        const HWND window,
        const int x,
        const int y,
        const int width,
        const int height,
        const int focus_height,
        const ChildWidthMode width_mode = ChildWidthMode::Fixed,
        const LayoutRegion region = LayoutRegion::Body,
        const BodyControlKind kind = BodyControlKind::Generic) {
        child_layouts_.push_back({window, x, y, width, height, focus_height, width_mode, region, kind});
        SetWindowSubclass(window, ChildWindowProc, kChildSubclassId, reinterpret_cast<DWORD_PTR>(this));
    }

    [[nodiscard]] int ContentHeight() const noexcept { return body_content_height_; }

    [[nodiscard]] int ViewportHeight() const noexcept {
        RECT viewport{};
        return settings_viewport_ != nullptr && GetClientRect(settings_viewport_, &viewport) != FALSE
                   ? std::max(static_cast<int>(viewport.bottom - viewport.top), 0)
                   : 0;
    }

    [[nodiscard]] idleharbor::app::SafetyRegions SafetyRegionsFor(const RECT& client) const noexcept {
        return idleharbor::app::ComputeSafetyRegions(client.right, client.bottom, static_cast<int>(dpi_));
    }

    [[nodiscard]] bool IsSettingsViewportPoint(const LPARAM l_param) const noexcept {
        if (settings_viewport_ == nullptr) {
            return false;
        }
        POINT point{
            static_cast<short>(LOWORD(l_param)),
            static_cast<short>(HIWORD(l_param)),
        };
        if (ScreenToClient(window_, &point) == FALSE) {
            return false;
        }
        RECT viewport{};
        if (GetWindowRect(settings_viewport_, &viewport) == FALSE) {
            return false;
        }
        POINT top_left{viewport.left, viewport.top};
        POINT bottom_right{viewport.right, viewport.bottom};
        if (ScreenToClient(window_, &top_left) == FALSE || ScreenToClient(window_, &bottom_right) == FALSE) {
            return false;
        }
        const RECT client_viewport{top_left.x, top_left.y, bottom_right.x, bottom_right.y};
        return PtInRect(&client_viewport, point) != FALSE;
    }

    void LayoutControls() {
        if (child_layouts_.empty() || window_ == nullptr) {
            return;
        }
        RECT client{};
        if (GetClientRect(window_, &client) == FALSE) {
            return;
        }
        const auto regions = SafetyRegionsFor(client);
        if (settings_viewport_ != nullptr) {
            SetWindowPos(
                settings_viewport_,
                nullptr,
                regions.viewport.left,
                regions.viewport.top,
                std::max(regions.viewport.right - regions.viewport.left, 1),
                std::max(regions.viewport.bottom - regions.viewport.top, 0),
                SWP_NOACTIVATE | SWP_NOZORDER);
        }
        RECT viewport_client{};
        const int viewport_width = settings_viewport_ != nullptr &&
                                            GetClientRect(settings_viewport_, &viewport_client) != FALSE
                                        ? std::max(static_cast<int>(viewport_client.right), 1)
                                        : std::max(regions.viewport.right - regions.viewport.left, 1);
        const auto settings_layout = idleharbor::app::DetermineSettingsLayout(viewport_width, static_cast<int>(dpi_));
        const bool stacked = settings_layout == idleharbor::app::SettingsLayoutMode::Stacked;
        const int logical_client_width = std::max(
            idleharbor::app::LogicalPixels(viewport_width, static_cast<int>(dpi_)),
            1);
        const auto stacked_body = idleharbor::app::ComputeStackedBodyLayout(logical_client_width);
        int narrow_y = 0;
        int body_bottom = 0;
        for (auto& child : child_layouts_) {
            if (child.region != LayoutRegion::Body) {
                continue;
            }
            if (stacked) {
                child.arranged_x = stacked_body.left;
                child.arranged_y = narrow_y;
                child.arranged_width = stacked_body.width;
                if (child.kind == BodyControlKind::Heading) {
                    narrow_y += 36;
                } else if (child.kind == BodyControlKind::Label) {
                    narrow_y += 28;
                } else if (child.kind == BodyControlKind::Field) {
                    narrow_y += 38;
                } else {
                    narrow_y += 34;
                }
            } else {
                child.arranged_x = child.x;
                child.arranged_y = child.y - kBaseBodyOrigin;
                child.arranged_width = child.width_mode == ChildWidthMode::Fill
                                           ? std::max(80, logical_client_width - child.x - 20)
                                           : child.width;
            }
            body_bottom = std::max(body_bottom, child.arranged_y + child.focus_height);
        }
        body_content_height_ = std::max(Scale(kBaseBodyContentHeight), Scale(body_bottom + 16));
        for (const auto& child : child_layouts_) {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = Scale(child.height);
            if (child.region == LayoutRegion::FixedTop) {
                x = regions.status.left;
                y = regions.status.top;
                width = std::max(regions.status.right - regions.status.left, Scale(80));
                height = std::max(regions.status.bottom - regions.status.top, Scale(24));
            } else if (child.region == LayoutRegion::FixedBottom) {
                const auto action_buttons = idleharbor::app::ComputeActionButtonRects(
                    static_cast<int>(client.right),
                    static_cast<int>(client.bottom),
                    static_cast<int>(dpi_));
                const auto& action = child.window == stop_ ? action_buttons.stop
                                  : child.window == save_ ? action_buttons.save
                                                          : action_buttons.start;
                x = action.left;
                y = action.top;
                width = action.right - action.left;
                height = action.bottom - action.top;
            } else {
                x = Scale(child.arranged_x);
                y = Scale(child.arranged_y) - scroll_position_;
                width = Scale(child.arranged_width);
            }
            SetWindowPos(
                child.window,
                nullptr,
                x,
                y,
                width,
                height,
                SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    void RepaintSettingsViewport() const noexcept {
        const HWND target = settings_viewport_ != nullptr ? settings_viewport_ : window_;
        if (target == nullptr) {
            return;
        }
        // Body controls are child windows moved inside a clipped child viewport.
        // Finish each layout/state transaction with one synchronous descendant
        // repaint so exposed client pixels cannot retain their previous contents.
        RedrawWindow(
            target,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }

    void UpdateViewport() {
        if (updating_viewport_) {
            return;
        }
        updating_viewport_ = true;
        constexpr int kMaxLayoutPasses = 4;
        const int requested_scroll_position = scroll_position_;
        if (settings_viewport_ != nullptr) {
            // A visible scrollbar reduces GetClientRect(). Start each resize pass
            // from the scrollbar-free candidate so a height increase can shed an
            // inherited bar and return to the wider layout.
            ShowScrollBar(settings_viewport_, SB_VERT, FALSE);
        }
        const auto publish_scroll_info = [&](const bool include_position) {
            const int viewport_height = ViewportHeight();
            SCROLLINFO scroll_info{sizeof(scroll_info)};
            scroll_info.fMask = SIF_RANGE | SIF_PAGE;
            scroll_info.nMin = 0;
            scroll_info.nMax = std::max(ContentHeight() - 1, 0);
            scroll_info.nPage = static_cast<UINT>(viewport_height);
            if (include_position) {
                scroll_position_ = idleharbor::app::ClampScrollPosition(
                    scroll_position_,
                    ContentHeight(),
                    viewport_height);
                scroll_info.fMask |= SIF_POS;
                scroll_info.nPos = scroll_position_;
            }
            if (settings_viewport_ != nullptr) {
                SetScrollInfo(settings_viewport_, SB_VERT, &scroll_info, TRUE);
                ShowScrollBar(
                    settings_viewport_,
                    SB_VERT,
                    ContentHeight() > viewport_height ? TRUE : FALSE);
            }
            return viewport_height;
        };
        for (int pass = 0; pass < kMaxLayoutPasses; ++pass) {
            scroll_position_ = requested_scroll_position;
            LayoutControls();
            const int content_height_before = ContentHeight();
            const int viewport_height_before = publish_scroll_info(false);

            LayoutControls();
            const bool stable = content_height_before == ContentHeight() &&
                                 viewport_height_before == ViewportHeight();
            if (stable) {
                break;
            }
        }

        // Clamp only after the final scrollbar/layout state is known, preserving a
        // valid near-bottom request while a temporary probe uses a shorter layout.
        scroll_position_ = requested_scroll_position;
        LayoutControls();
        publish_scroll_info(true);
        LayoutControls();
        updating_viewport_ = false;
        RepaintSettingsViewport();
    }

    void ScrollTo(const int position) {
        const int target = idleharbor::app::ClampScrollPosition(position, ContentHeight(), ViewportHeight());
        if (target == scroll_position_) {
            return;
        }
        scroll_position_ = target;
        SCROLLINFO scroll_info{sizeof(scroll_info)};
        scroll_info.fMask = SIF_POS;
        scroll_info.nPos = scroll_position_;
        if (settings_viewport_ != nullptr) {
            SetScrollInfo(settings_viewport_, SB_VERT, &scroll_info, TRUE);
        }
        LayoutControls();
        RepaintSettingsViewport();
    }

    void HandleVerticalScroll(const WPARAM w_param) {
        SCROLLINFO scroll_info{sizeof(scroll_info)};
        scroll_info.fMask = SIF_ALL;
        if (settings_viewport_ == nullptr || GetScrollInfo(settings_viewport_, SB_VERT, &scroll_info) == FALSE) {
            return;
        }
        int target = scroll_position_;
        switch (LOWORD(w_param)) {
        case SB_TOP:
            target = 0;
            break;
        case SB_BOTTOM:
            target = idleharbor::app::MaximumScrollPosition(ContentHeight(), ViewportHeight());
            break;
        case SB_LINEUP:
            target -= Scale(kBaseScrollLine);
            break;
        case SB_LINEDOWN:
            target += Scale(kBaseScrollLine);
            break;
        case SB_PAGEUP:
            target -= static_cast<int>(scroll_info.nPage);
            break;
        case SB_PAGEDOWN:
            target += static_cast<int>(scroll_info.nPage);
            break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:
            target = scroll_info.nTrackPos;
            break;
        default:
            return;
        }
        ScrollTo(target);
    }

    void HandleMouseWheel(const WPARAM w_param) {
        const auto wheel = idleharbor::app::ConsumeWheelDelta(
            wheel_delta_remainder_,
            GET_WHEEL_DELTA_WPARAM(w_param));
        wheel_delta_remainder_ = wheel.remainder;
        if (wheel.steps == 0) {
            return;
        }

        UINT scroll_lines = 3;
        if (SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0) == FALSE) {
            scroll_lines = 3;
        }
        if (scroll_lines == 0) {
            return;
        }
        const long long distance_per_step = scroll_lines == WHEEL_PAGESCROLL
                                                ? ViewportHeight()
                                                : static_cast<long long>(Scale(kBaseScrollLine)) * scroll_lines;
        const long long maximum = idleharbor::app::MaximumScrollPosition(ContentHeight(), ViewportHeight());
        const long long requested = static_cast<long long>(scroll_position_) -
                                    static_cast<long long>(wheel.steps) * distance_per_step;
        ScrollTo(static_cast<int>(std::clamp(requested, 0LL, maximum)));
    }

    [[nodiscard]] bool IsDroppedComboBox(const HWND window) const noexcept {
        const bool is_combo = window == profile_ || window == motion_ || window == power_;
        return is_combo && SendMessageW(window, CB_GETDROPPEDSTATE, 0, 0) != FALSE;
    }

    void EnsureFocusedControlVisible() {
        const HWND focused = GetFocus();
        if (focused == nullptr || (focused != window_ && IsChild(window_, focused) == FALSE)) {
            return;
        }
        const auto layout = std::find_if(child_layouts_.begin(), child_layouts_.end(), [&](const ChildLayout& child) {
            return child.window == focused;
        });
        if (layout == child_layouts_.end()) {
            return;
        }
        if (layout->region != LayoutRegion::Body) {
            return;
        }
        const int top = Scale(layout->arranged_y);
        const int bottom = top + Scale(layout->focus_height);
        ScrollTo(idleharbor::app::ScrollPositionToReveal(
            scroll_position_,
            top,
            bottom,
            ContentHeight(),
            ViewportHeight()));
    }

    void ApplyDpiChange(const UINT new_dpi, const RECT& suggested) {
        if (new_dpi == 0) {
            return;
        }
        const UINT old_dpi = dpi_;
        dpi_ = new_dpi;
        scroll_position_ = old_dpi == 0
                               ? 0
                               : MulDiv(scroll_position_, static_cast<int>(new_dpi), static_cast<int>(old_dpi));

        const RECT target = ClampToWorkArea(suggested);
        SetWindowPos(
            window_,
            nullptr,
            target.left,
            target.top,
            target.right - target.left,
            target.bottom - target.top,
            SWP_NOACTIVATE | SWP_NOZORDER);

        HFONT replacement_font = CreateUiFont(dpi_);
        if (replacement_font != nullptr) {
            HFONT replacement_heading_font = CreateFontW(
                -MulDiv(10, static_cast<int>(dpi_), 72),
                0,
                0,
                0,
                FW_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                L"Segoe UI");
            const HFONT heading_for_children = replacement_heading_font != nullptr ? replacement_heading_font : replacement_font;
            for (const auto& child : child_layouts_) {
                SendMessageW(
                    child.window,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(child.kind == BodyControlKind::Heading ? heading_for_children : replacement_font),
                    TRUE);
            }
            if (ui_font_ != nullptr) {
                DeleteObject(ui_font_);
            }
            ui_font_ = replacement_font;
            if (replacement_heading_font != nullptr) {
                if (heading_font_ != nullptr) {
                    DeleteObject(heading_font_);
                }
                heading_font_ = replacement_heading_font;
            }
        }
        UpdateViewport();
        EnsureFocusedControlVisible();
    }

    void CreateControls() {
        ui_font_ = CreateUiFont(dpi_);
        const HFONT font = ui_font_ != nullptr ? ui_font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        heading_font_ = CreateFontW(
            -MulDiv(10, static_cast<int>(dpi_), 72),
            0,
            0,
            0,
            FW_SEMIBOLD,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
        const HFONT heading = heading_font_ != nullptr ? heading_font_ : font;
        const auto scale = [&](const int value) { return ScaleForDpi(value, dpi_); };
        settings_viewport_ = CreateWindowExW(
            WS_EX_CONTROLPARENT,
            L"STATIC",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
            0,
            0,
            0,
            0,
            window_,
            nullptr,
            instance_,
            nullptr);
        if (settings_viewport_ != nullptr) {
            SetWindowSubclass(settings_viewport_, ChildWindowProc, kChildSubclassId, reinterpret_cast<DWORD_PTR>(this));
        }
        const HWND body_parent = settings_viewport_ != nullptr ? settings_viewport_ : window_;
        const auto add_label = [&](const wchar_t* text, const int y) {
            const HWND control = CreateWindowExW(
                0,
                L"STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                scale(20),
                scale(y),
                scale(270),
                scale(24),
                body_parent,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            TrackChild(control, 20, y, 270, 24, 24, ChildWidthMode::Fixed, LayoutRegion::Body, BodyControlKind::Label);
        };
        const auto add_heading = [&](const wchar_t* text, const int y) {
            const HWND control = CreateWindowExW(
                0,
                L"STATIC",
                text,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                scale(20),
                scale(y),
                scale(525),
                scale(28),
                body_parent,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(heading), TRUE);
            TrackChild(control, 20, y, 525, 28, 28, ChildWidthMode::Fill, LayoutRegion::Body, BodyControlKind::Heading);
        };
        const auto add_combo = [&](HWND& target, const int y, const int id) {
            target = CreateWindowExW(
                0,
                L"COMBOBOX",
                nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                scale(300),
                scale(y - 3),
                scale(245),
                scale(260),
                body_parent,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            TrackChild(
                target,
                300,
                y - 3,
                245,
                260,
                26,
                ChildWidthMode::Fill,
                LayoutRegion::Body,
                BodyControlKind::Field);
        };
        const auto add_edit = [&](HWND& target, const int y, const int id) {
            target = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                scale(300),
                scale(y - 3),
                scale(145),
                scale(26),
                body_parent,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            TrackChild(target, 300, y - 3, 145, 26, 26, ChildWidthMode::Fixed, LayoutRegion::Body, BodyControlKind::Field);
        };
        const auto add_check = [&](HWND& target, const wchar_t* text, const int y, const int id) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                scale(20),
                scale(y),
                scale(525),
                scale(26),
                body_parent,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            TrackChild(
                target,
                20,
                y,
                525,
                26,
                26,
                ChildWidthMode::Fill,
                LayoutRegion::Body,
                BodyControlKind::Check);
        };

        status_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"Stopped: ready",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            scale(20),
            scale(18),
            scale(525),
            scale(28),
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatus)),
            instance_,
            nullptr);
        SendMessageW(status_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        TrackChild(status_, 20, 18, 525, 28, 28, ChildWidthMode::Fill, LayoutRegion::FixedTop);

        add_heading(L"Session", 56);
        add_label(L"Profile", 86);
        add_combo(profile_, 86, kProfile);
        for (const auto profile : kProfiles) {
            const std::wstring text = Widen(idleharbor::core::profile_kind_name(profile));
            SendMessageW(profile_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Motion", 120);
        add_combo(motion_, 120, kMotion);
        const std::array<std::wstring, 5> motions{
            L"Off",
            L"Normal (diagonal)",
            L"Zen",
            L"Circle",
            L"Linear",
        };
        for (const auto& text : motions) {
            SendMessageW(motion_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Power request", 154);
        add_combo(power_, 154, kPower);
        const std::array<std::wstring, 3> powers{L"None", L"System sleep", L"Display and system"};
        for (const auto& text : powers) {
            SendMessageW(power_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_heading(L"Pulse", 190);
        add_label(L"Pulse interval (seconds)", 220);
        add_edit(interval_, 220, kInterval);
        add_label(L"Motion multiplier (1-120)", 254);
        add_edit(distance_, 254, kDistance);
        add_check(randomize_, L"Randomize pulse interval", 286, kRandomize);
        add_heading(L"Safeguards", 322);
        add_label(L"Pause after genuine input (seconds; 0 disables)", 352);
        add_edit(pause_input_, 352, kPauseInput);
        add_check(lock_pause_, L"Pause while the workstation is locked", 384, kLockPause);
        add_check(disconnect_pause_, L"Pause while the session is disconnected", 416, kDisconnectPause);
        add_label(L"Low-battery threshold (0 disables)", 450);
        add_edit(battery_, 450, kBattery);
        add_check(pause_on_battery_, L"Pause whenever the device is on battery", 482, kPauseOnBattery);
        add_check(fullscreen_, L"Pause while a full-screen application is foreground", 514, kFullscreen);
        add_heading(L"Window & notifications", 550);
        add_label(L"Maximum session duration (seconds; 0 disables)", 580);
        add_edit(max_duration_, 580, kMaxDuration);
        add_check(start_minimized_, L"Start minimized to the notification area", 612, kStartMinimized);
        add_check(close_to_tray_, L"Close button hides to the notification area", 644, kCloseToTray);
        add_check(notifications_, L"Show safety state notifications", 676, kNotifications);
        add_check(emergency_hotkey_, L"Enable emergency stop: Ctrl+Alt+Shift+F12", 708, kEmergencyHotkey);

        const auto add_button = [&](HWND& target, const wchar_t* text, const int x, const int id) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                scale(x),
                scale(618),
                scale(115),
                scale(32),
                window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            TrackChild(
                target,
                x,
                618,
                115,
                32,
                32,
                ChildWidthMode::Fixed,
                LayoutRegion::FixedBottom,
                BodyControlKind::Generic);
        };
        add_button(start_, L"Start", 20, kStart);
        add_button(stop_, L"Stop", 145, kStop);
        add_button(save_, L"Save", 270, kSave);
        UpdateViewport();
    }

    void InitializeTrayIcon() {
        tray_icon_ = LoadIdleHarborIcon(instance_);
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        icon.uCallbackMessage = kTrayMessage;
        icon.hIcon = tray_icon_;
        wcsncpy_s(icon.szTip, (L"IdleHarbor - " + status_text_).c_str(), _TRUNCATE);
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
        if (tray_added_) {
            icon.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &icon);
        }
    }

    void MarkTrayUnavailable() {
        ShowWindow(window_, SW_SHOW);
        status_text_ = session_active_ ? L"Running: notification icon unavailable; window kept visible"
                                       : L"Stopped: notification icon unavailable; window kept visible";
        if (status_ != nullptr) {
            SetControlText(status_, status_text_);
        }
    }

    void RecoverTrayIcon() {
        RemoveTrayIcon();
        InitializeTrayIcon();
        if (!tray_added_) {
            MarkTrayUnavailable();
        }
    }

    void RemoveTrayIcon() noexcept {
        if (!tray_added_) {
            return;
        }
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = window_;
        icon.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &icon);
        tray_added_ = false;
    }

    void UpdateTrayTooltip() {
        if (!tray_added_) {
            return;
        }
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_TIP;
        const std::wstring tooltip = L"IdleHarbor - " + DisplayStatusText();
        wcsncpy_s(icon.szTip, tooltip.c_str(), _TRUNCATE);
        if (Shell_NotifyIconW(NIM_MODIFY, &icon) == FALSE) {
            RecoverTrayIcon();
        }
    }

    void ShowSafetyNotification(const wchar_t* title, const std::wstring& message) {
        if (!tray_added_ || !settings_.show_notifications) {
            return;
        }
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_INFO;
        wcsncpy_s(icon.szInfoTitle, title, _TRUNCATE);
        wcsncpy_s(icon.szInfo, message.c_str(), _TRUNCATE);
        icon.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
        if (Shell_NotifyIconW(NIM_MODIFY, &icon) == FALSE) {
            RecoverTrayIcon();
        }
    }

    void SetStatus(const std::wstring& status) {
        status_text_ = status;
        if (status_ != nullptr) {
            InvalidateRect(status_, nullptr, TRUE);
            UpdateWindow(status_);
        }
        UpdateTrayTooltip();
    }

    [[nodiscard]] std::wstring DisplayStatusText() const {
        return dirty_ ? L"Unsaved changes — " + status_text_ : status_text_;
    }

    void UpdateDirtyPresentation() {
        if (save_ != nullptr) {
            SetControlText(save_, dirty_ ? L"Save changes" : L"Save");
        }
        if (status_ != nullptr) {
            InvalidateRect(status_, nullptr, TRUE);
            UpdateWindow(status_);
        }
        UpdateTrayTooltip();
        UpdateButtons();
    }

    void UpdateDirtyStateFromControls() {
        if (suppress_dirty_tracking_ || profile_ == nullptr) {
            return;
        }
        const auto before = settings_;
        std::wstring error;
        if (!ReadControls(error)) {
            settings_ = before;
            dirty_ = true;
        } else {
            dirty_ = !AppSettingsEqual(settings_, saved_settings_);
        }
        UpdateDirtyPresentation();
    }

    void DrawStatusCard(const DRAWITEMSTRUCT* draw_item) const noexcept {
        if (draw_item == nullptr || draw_item->hDC == nullptr) {
            return;
        }
        const HBRUSH background = GetSysColorBrush(COLOR_INFOBK);
        FillRect(draw_item->hDC, &draw_item->rcItem, background);
        FrameRect(draw_item->hDC, &draw_item->rcItem, GetSysColorBrush(COLOR_ACTIVEBORDER));
        const int old_mode = SetBkMode(draw_item->hDC, TRANSPARENT);
        const COLORREF old_color = SetTextColor(draw_item->hDC, GetSysColor(COLOR_INFOTEXT));
        const HFONT old_font = static_cast<HFONT>(SelectObject(
            draw_item->hDC,
            ui_font_ != nullptr ? ui_font_ : GetStockObject(DEFAULT_GUI_FONT)));
        RECT text_rect = draw_item->rcItem;
        InflateRect(&text_rect, -Scale(12), -Scale(2));
        DrawTextW(
            draw_item->hDC,
            DisplayStatusText().c_str(),
            -1,
            &text_rect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(draw_item->hDC, old_font);
        SetTextColor(draw_item->hDC, old_color);
        SetBkMode(draw_item->hDC, old_mode);
    }

    void RefreshControls() {
        if (profile_ == nullptr) {
            return;
        }
        const bool previous_suppression = suppress_dirty_tracking_;
        suppress_dirty_tracking_ = true;
        const auto profile_index = std::find(kProfiles.begin(), kProfiles.end(), settings_.session.profile);
        SendMessageW(
            profile_,
            CB_SETCURSEL,
            static_cast<WPARAM>(profile_index == kProfiles.end() ? 0 : profile_index - kProfiles.begin()),
            0);
        const auto motion_index = std::find(kMotionModes.begin(), kMotionModes.end(), settings_.session.motion);
        SendMessageW(
            motion_,
            CB_SETCURSEL,
            static_cast<WPARAM>(motion_index == kMotionModes.end() ? 0 : motion_index - kMotionModes.begin()),
            0);
        const auto power_index = std::find(kPowerModes.begin(), kPowerModes.end(), settings_.session.power);
        SendMessageW(
            power_,
            CB_SETCURSEL,
            static_cast<WPARAM>(power_index == kPowerModes.end() ? 0 : power_index - kPowerModes.begin()),
            0);
        SetControlText(interval_, std::to_wstring(settings_.session.interval.count()));
        SetControlText(distance_, std::to_wstring(settings_.session.distance));
        SetChecked(randomize_, settings_.session.randomize);
        SetControlText(
            pause_input_,
            std::to_wstring(settings_.session.pause_on_user_activity ? settings_.session.user_activity_cooldown.count() : 0));
        SetChecked(lock_pause_, settings_.session.pause_when_locked);
        SetChecked(disconnect_pause_, settings_.session.pause_when_disconnected);
        SetControlText(battery_, std::to_wstring(settings_.session.pause_on_low_battery
                                                     ? settings_.session.low_battery_threshold
                                                     : 0));
        SetChecked(fullscreen_, settings_.session.pause_when_fullscreen);
        SetChecked(pause_on_battery_, settings_.session.pause_on_battery);
        SetControlText(max_duration_, std::to_wstring(settings_.session.max_duration.count()));
        SetChecked(start_minimized_, settings_.start_minimized);
        SetChecked(close_to_tray_, settings_.close_to_tray);
        SetChecked(notifications_, settings_.show_notifications);
        SetChecked(emergency_hotkey_, settings_.emergency_hotkey);
        suppress_dirty_tracking_ = previous_suppression;
        UpdateButtons();
        UpdateDirtyPresentation();
    }

    void ApplyEmergencyHotkeySetting() {
        if (settings_.emergency_hotkey && !hotkey_registered_) {
            hotkey_registered_ = RegisterHotKey(
                window_,
                kEmergencyHotkeyId,
                MOD_CONTROL | MOD_ALT | MOD_SHIFT,
                VK_F12) != FALSE;
        } else if (!settings_.emergency_hotkey && hotkey_registered_) {
            UnregisterHotKey(window_, kEmergencyHotkeyId);
            hotkey_registered_ = false;
        }
    }

    void UpdateButtons() {
        if (session_active_) {
            if (stop_ != nullptr) {
                EnableWindow(stop_, TRUE);
            }
            if (start_ != nullptr) {
                EnableWindow(start_, FALSE);
            }
        } else {
            if (start_ != nullptr) {
                EnableWindow(start_, TRUE);
            }
            if (stop_ != nullptr) {
                EnableWindow(stop_, FALSE);
            }
        }
        for (const HWND control : {profile_, motion_, power_, interval_, distance_, randomize_, pause_input_,
                                   lock_pause_, disconnect_pause_, battery_, pause_on_battery_, fullscreen_,
                                   max_duration_, start_minimized_, close_to_tray_, notifications_,
                                   emergency_hotkey_}) {
            if (control != nullptr) {
                EnableWindow(control, session_active_ ? FALSE : TRUE);
            }
        }
        if (save_ != nullptr) {
            EnableWindow(save_, session_active_ == false && dirty_ ? TRUE : FALSE);
        }
        RepaintSettingsViewport();
    }

    bool ReadControls(std::wstring& error) {
        error.clear();
        Settings& session = settings_.session;
        const int profile_index = ComboIndex(profile_);
        const int motion_index = ComboIndex(motion_);
        const int power_index = ComboIndex(power_);
        if (profile_index < 0 || profile_index >= static_cast<int>(kProfiles.size()) || motion_index < 0 ||
            motion_index >= static_cast<int>(kMotionModes.size()) || power_index < 0 ||
            power_index >= static_cast<int>(kPowerModes.size())) {
            error = L"Choose a valid profile, motion mode, and power mode.";
            return false;
        }
        session.profile = kProfiles[static_cast<std::size_t>(profile_index)];
        session.motion = kMotionModes[static_cast<std::size_t>(motion_index)];
        session.power = kPowerModes[static_cast<std::size_t>(power_index)];

        const auto read_number = [&](const HWND control, const std::uint64_t maximum, std::uint64_t& target) {
            const auto parsed = ParseUnsigned(ControlText(control));
            if (!parsed.has_value() || *parsed > maximum) {
                return false;
            }
            target = *parsed;
            return true;
        };
        std::uint64_t value = 0;
        if (!read_number(interval_, 24ULL * 60 * 60, value) || value == 0) {
            error = L"Pulse interval must be between 1 and 86400 seconds.";
            return false;
        }
        session.interval = Seconds{static_cast<std::int64_t>(value)};
        if (session.random_minimum > session.interval) {
            session.random_minimum = Seconds{1};
        }
        if (!read_number(distance_, 120, value) || value == 0) {
            error = L"Motion multiplier must be between 1 and 120.";
            return false;
        }
        session.distance = static_cast<std::uint32_t>(value);
        session.randomize = IsChecked(randomize_);
        if (!read_number(pause_input_, 24ULL * 60 * 60, value)) {
            error = L"Input pause must be between 0 and 86400 seconds.";
            return false;
        }
        session.pause_on_user_activity = value != 0;
        if (value != 0) {
            session.user_activity_cooldown = Seconds{static_cast<std::int64_t>(value)};
        }
        session.pause_when_locked = IsChecked(lock_pause_);
        session.pause_when_disconnected = IsChecked(disconnect_pause_);
        if (!read_number(battery_, 100, value)) {
            error = L"Battery threshold must be between 0 and 100.";
            return false;
        }
        session.pause_on_low_battery = value != 0;
        if (value != 0) {
            session.low_battery_threshold = static_cast<std::uint8_t>(value);
        }
        session.pause_when_fullscreen = IsChecked(fullscreen_);
        session.pause_on_battery = IsChecked(pause_on_battery_);
        if (!read_number(max_duration_, 30ULL * 24 * 60 * 60, value)) {
            error = L"Maximum duration must be between 0 and 2592000 seconds.";
            return false;
        }
        session.max_duration = Seconds{static_cast<std::int64_t>(value)};
        settings_.start_minimized = IsChecked(start_minimized_);
        settings_.close_to_tray = IsChecked(close_to_tray_);
        settings_.show_notifications = IsChecked(notifications_);
        settings_.emergency_hotkey = IsChecked(emergency_hotkey_);

        const auto validation = idleharbor::core::validate(session);
        if (!validation.valid) {
            error = L"Invalid settings: " + std::wstring(validation.errors.front().begin(), validation.errors.front().end());
            return false;
        }
        return true;
    }

    void ApplyCommandLineOptions(const CommandLineOptions& options) {
        if (options.profile.has_value()) {
            if (const auto profile = ProfileFromText(*options.profile); profile.has_value()) {
                settings_.session = idleharbor::core::settings_for_profile(*profile);
            }
        }
        if (options.motion_mode.has_value()) {
            if (const auto motion = MotionFromText(*options.motion_mode); motion.has_value()) {
                settings_.session.motion = *motion;
            }
        }
        if (options.power_mode.has_value()) {
            if (const auto power = PowerFromText(*options.power_mode); power.has_value()) {
                settings_.session.power = *power;
            }
        }
        if (options.interval.has_value()) {
            settings_.session.interval = *options.interval;
            if (settings_.session.random_minimum > settings_.session.interval) {
                settings_.session.random_minimum = Seconds{1};
            }
        }
        if (options.distance.has_value()) {
            settings_.session.distance = *options.distance;
        }
        if (options.randomize.has_value()) {
            settings_.session.randomize = *options.randomize;
        }
        if (options.pause_on_input.has_value()) {
            settings_.session.pause_on_user_activity = options.pause_on_input->count() != 0;
            if (options.pause_on_input->count() != 0) {
                settings_.session.user_activity_cooldown = *options.pause_on_input;
            }
        }
        if (options.stop_after.has_value()) {
            settings_.session.max_duration = *options.stop_after;
        }
        if (options.battery_threshold.has_value()) {
            settings_.session.low_battery_threshold = static_cast<std::uint8_t>(*options.battery_threshold);
            settings_.session.pause_on_low_battery = *options.battery_threshold != 0;
        }
        if (options.pause_on_fullscreen.has_value()) {
            settings_.session.pause_when_fullscreen = *options.pause_on_fullscreen;
        }
        if (options.minimized) {
            settings_.start_minimized = true;
        }
        if (options.close_to_tray.has_value()) {
            settings_.close_to_tray = *options.close_to_tray;
        }
    }

    void ApplySelectedProfile() {
        const int index = ComboIndex(profile_);
        if (index < 0 || index >= static_cast<int>(kProfiles.size())) {
            return;
        }
        const auto app_start_minimized = settings_.start_minimized;
        const auto app_close_to_tray = settings_.close_to_tray;
        settings_.session = idleharbor::core::settings_for_profile(kProfiles[static_cast<std::size_t>(index)]);
        settings_.start_minimized = app_start_minimized;
        settings_.close_to_tray = app_close_to_tray;
        RefreshControls();
        SetStatus(L"Stopped: profile defaults loaded; press Save to persist them");
    }

    PolicyInput Snapshot(const bool user_activity) const {
        const auto battery = idleharbor::platform::windows::QueryBatterySnapshot();
        return PolicyInput{
            NowSeconds(),
            user_activity,
            locked_,
            disconnected_,
            battery.on_battery,
            battery.percent,
            runtime_settings_.pause_when_fullscreen && idleharbor::platform::windows::IsForegroundFullscreen(),
            idleharbor::platform::windows::LocalMinuteOfDay(),
        };
    }

    void StartSession() {
        if (session_active_) {
            return;
        }
        std::wstring error;
        if (!ReadControls(error)) {
            MessageBoxW(window_, error.c_str(), L"IdleHarbor settings", MB_OK | MB_ICONWARNING);
            RefreshControls();
            return;
        }
        dirty_ = !AppSettingsEqual(settings_, saved_settings_);
        UpdateDirtyPresentation();

        ApplyEmergencyHotkeySetting();
        runtime_settings_ = settings_.session;
        const bool session_safeguards_requested =
            runtime_settings_.pause_when_locked || runtime_settings_.pause_when_disconnected;
        if (session_safeguards_requested &&
            (!session_notifications_available_ || !session_state_available_)) {
            const bool observer_unavailable = !session_notifications_available_;
            SetStatus(observer_unavailable ? L"Stopped: session-change observer unavailable"
                                           : L"Stopped: current session state unavailable");
            MessageBoxW(
                window_,
                observer_unavailable
                    ? L"Windows session-change notifications are unavailable. Disable the lock/disconnect safeguard "
                      L"explicitly or resolve the Windows error before starting."
                    : L"IdleHarbor could not establish the current lock/disconnect state. Disable the "
                      L"lock/disconnect safeguard explicitly or resolve the Windows error before starting.",
                L"IdleHarbor safeguard",
                MB_OK | MB_ICONWARNING);
            return;
        }
        input_observer_requested_ = runtime_settings_.pause_on_user_activity;
        if (input_observer_requested_) {
            const auto capabilities = input_monitor_.Start(window_, kGenuineInputMessage);
            input_observer_available_ = capabilities.any();
            input_observer_complete_ = capabilities.mouse && capabilities.keyboard;
            if (!input_observer_complete_) {
                input_monitor_.Stop();
                input_observer_requested_ = false;
                input_observer_available_ = false;
                SetStatus(L"Stopped: genuine-input observer unavailable");
                MessageBoxW(
                    window_,
                    L"IdleHarbor could not observe both mouse and keyboard activity. Set the genuine-input "
                    L"pause to 0 explicitly or resolve the Windows hook error before starting.",
                    L"IdleHarbor safeguard",
                    MB_OK | MB_ICONWARNING);
                return;
            }
            next_input_hook_refresh_tick_ = GetTickCount64() + kInputHookRefreshIntervalMs;
        }
        policy_ = std::make_unique<PolicyEngine>(runtime_settings_);
        policy_->start(NowSeconds());
        sampler_ = std::make_unique<idleharbor::core::IntervalSampler>(
            GetTickCount64() ^ static_cast<ULONGLONG>(GetCurrentProcessId()),
            runtime_settings_.random_minimum,
            runtime_settings_.interval,
            runtime_settings_.randomize);
        next_pulse_tick_ = GetTickCount64() + static_cast<ULONGLONG>(sampler_->next().count()) * 1000ULL;
        session_active_ = true;
        if (SetTimer(window_, kTimerId, 1000, nullptr) == 0) {
            FailSession(L"session timer could not start (" + WindowsErrorText(GetLastError()) + L")");
            return;
        }
        Evaluate(false);
        UpdateButtons();
        if (stop_ != nullptr) {
            PostMessageW(window_, kDeferredFocusMessage, reinterpret_cast<WPARAM>(stop_), 0);
        }
    }

    void StopSession() {
        if (policy_ != nullptr) {
            policy_->stop();
        }
        const PolicyDecision decision{DecisionAction::Stop, EngineState::Stopped, PolicyReason::Manual, Seconds{0}};
        EndSession(decision);
    }

    void EndSession(const PolicyDecision& decision) {
        const bool transitioned_to_stopped = session_active_;
        KillTimer(window_, kTimerId);
        input_monitor_.Stop();
        power_request_.Clear();
        sampler_.reset();
        policy_.reset();
        session_active_ = false;
        input_observer_requested_ = false;
        input_observer_available_ = false;
        input_observer_complete_ = false;
        next_input_hook_refresh_tick_ = 0;
        const auto text = idleharbor::core::status_text(decision);
        SetStatus(std::wstring(text.begin(), text.end()));
        if (decision.reason == PolicyReason::MaxDuration) {
            ShowSafetyNotification(L"IdleHarbor session stopped", L"The configured maximum duration was reached.");
        }
        UpdateButtons();
        if (transitioned_to_stopped && start_ != nullptr) {
            PostMessageW(window_, kDeferredFocusMessage, reinterpret_cast<WPARAM>(start_), 0);
        }
    }

    void FailSession(const std::wstring& message) {
        if (policy_ != nullptr) {
            policy_->stop();
        }
        const PolicyDecision decision{DecisionAction::Stop, EngineState::Stopped, PolicyReason::Manual, Seconds{0}};
        EndSession(decision);
        SetStatus(L"Stopped: " + message);
        ShowSafetyNotification(L"IdleHarbor session stopped", message);
    }

    bool ApplyPower() {
        const auto requested = ToPlatformPowerMode(runtime_settings_.power);
        if (power_request_.mode() == requested) {
            return true;
        }
        if (!power_request_.Apply(requested)) {
            FailSession(L"power request failed (" + WindowsErrorText(GetLastError()) + L")");
            return false;
        }
        return true;
    }

    bool EmitPulse() {
        if (runtime_settings_.motion == MotionMode::Off) {
            return true;
        }
        MotionEmissionResult result{};
        if (runtime_settings_.motion == MotionMode::Zen) {
            result = idleharbor::platform::windows::EmitZenPulse();
        } else {
            const auto plan = idleharbor::core::make_motion_plan(runtime_settings_.motion, runtime_settings_.distance);
            std::vector<POINT> offsets;
            offsets.reserve(plan.relative_offsets.size());
            for (const auto point : plan.relative_offsets) {
                offsets.push_back(POINT{point.x, point.y});
            }
            result = idleharbor::platform::windows::EmitMotionPulse(offsets);
        }
        if (!result.succeeded) {
            std::wstring message = L"input emission failed (" + WindowsErrorText(result.error) + L")";
            if (result.cleanup_attempted && !result.cleanup_succeeded) {
                message += L"; pointer restoration failed (" + WindowsErrorText(result.cleanup_error) + L")";
            }
            FailSession(message);
            return false;
        }
        return true;
    }

    void ScheduleNextPulse() {
        if (sampler_ != nullptr) {
            next_pulse_tick_ = GetTickCount64() + static_cast<ULONGLONG>(sampler_->next().count()) * 1000ULL;
        }
    }

    void Evaluate(const bool user_activity) {
        if (!session_active_ || policy_ == nullptr) {
            return;
        }
        const auto was_running = policy_->state() == EngineState::Running;
        const auto decision = policy_->evaluate(Snapshot(user_activity));
        if (decision.action == DecisionAction::Stop) {
            EndSession(decision);
            return;
        }
        if (decision.action == DecisionAction::Pause) {
            power_request_.Clear();
            const auto text = idleharbor::core::status_text(decision);
            const std::wstring status(text.begin(), text.end());
            SetStatus(status);
            if (was_running) {
                ShowSafetyNotification(L"IdleHarbor session paused", status);
            }
            return;
        }
        if ((!was_running || power_request_.mode() != ToPlatformPowerMode(runtime_settings_.power)) && !ApplyPower()) {
            return;
        }
        if (GetTickCount64() >= next_pulse_tick_) {
            if (!EmitPulse()) {
                return;
            }
            ScheduleNextPulse();
        }
        std::wstring status = L"Running";
        if (!was_running) {
            ShowSafetyNotification(L"IdleHarbor session resumed", L"All configured safeguards are clear.");
        }
        if (input_observer_requested_ && !input_observer_available_) {
            status += L" (genuine-input observer unavailable)";
        } else if (input_observer_requested_ && !input_observer_complete_) {
            status += L" (genuine-input observer partially available)";
        } else if ((runtime_settings_.pause_when_locked || runtime_settings_.pause_when_disconnected) &&
                   !session_notifications_available_) {
            status += L" (session-change observer unavailable)";
        } else if (settings_.emergency_hotkey && !hotkey_registered_) {
            status += L" (emergency hotkey unavailable)";
        }
        SetStatus(status);
    }

    void Save() {
        std::wstring error;
        if (!ReadControls(error)) {
            MessageBoxW(window_, error.c_str(), L"IdleHarbor settings", MB_OK | MB_ICONWARNING);
            RefreshControls();
            return;
        }
        ApplyEmergencyHotkeySetting();
        std::string save_error;
        if (!idleharbor::app::SaveSettings(settings_path_, settings_, save_error)) {
            const std::wstring wide(save_error.begin(), save_error.end());
            MessageBoxW(window_, wide.c_str(), L"IdleHarbor settings", MB_OK | MB_ICONERROR);
            return;
        }
        saved_settings_ = settings_;
        dirty_ = false;
        settings_load_warnings_.clear();
        if (settings_.emergency_hotkey && !hotkey_registered_) {
            SetStatus(L"Stopped: settings saved; emergency hotkey unavailable");
        } else {
            SetStatus(session_active_ ? L"Running: settings saved; restart to apply changes" : L"Stopped: settings saved");
        }
        UpdateDirtyPresentation();
    }

    void ShowSettingsLoadWarnings() const {
        if (settings_load_warnings_.empty()) {
            return;
        }

        std::wstring message =
            L"IdleHarbor recovered safe settings from the configuration file. Review the values and "
            L"save a corrected file before relying on automatic start:\r\n\r\n";
        for (const auto& warning : settings_load_warnings_) {
            message += L"- " + Widen(warning) + L"\r\n";
        }
        MessageBoxW(window_, message.c_str(), L"IdleHarbor settings recovered", MB_OK | MB_ICONWARNING);
    }

    void ShowTrayMenu() {
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) {
            return;
        }
        AppendMenuW(menu, MF_STRING, kStatus, L"Show");
        AppendMenuW(menu, MF_STRING, session_active_ ? kStop : kStart, session_active_ ? L"Stop" : L"Start");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        constexpr UINT kExitMenu = 199;
        AppendMenuW(menu, MF_STRING, kExitMenu, L"Exit");
        SetForegroundWindow(window_);
        POINT cursor{};
        GetCursorPos(&cursor);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, 0, window_, nullptr);
        DestroyMenu(menu);
        if (command == kStatus) {
            ShowWindow(window_, SW_SHOW);
            SetForegroundWindow(window_);
        } else if (command == kStart) {
            StartSession();
        } else if (command == kStop) {
            StopSession();
        } else if (command == kExitMenu) {
            RequestExit();
        }
    }

    void RequestExit() {
        exiting_ = true;
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
    }

    void ProcessDeferredCommand() {
        if (deferred_commands_.empty()) {
            return;
        }
        std::wstring command_line = std::move(deferred_commands_.front());
        deferred_commands_.pop_front();

        int argument_count = 0;
        LPWSTR* arguments = CommandLineToArgvW(command_line.c_str(), &argument_count);
        if (arguments == nullptr) {
            MessageBoxW(
                window_,
                L"The forwarded IdleHarbor command could not be parsed.",
                L"IdleHarbor command",
                MB_OK | MB_ICONWARNING);
            return;
        }
        std::vector<std::wstring_view> views;
        for (int index = 1; index < argument_count; ++index) {
            views.emplace_back(arguments[index]);
        }
        const auto parsed = idleharbor::app::ParseCommandLine(views);
        LocalFree(arguments);
        if (!parsed.ok()) {
            MessageBoxW(window_, parsed.errors.front().c_str(), L"IdleHarbor command", MB_OK | MB_ICONWARNING);
            return;
        }
        if (parsed.options.portable || parsed.options.config_path.has_value()) {
            MessageBoxW(
                window_,
                L"--portable and --config select storage only when the first IdleHarbor instance starts. "
                L"Exit the running instance before changing storage.",
                L"IdleHarbor command",
                MB_OK | MB_ICONWARNING);
            return;
        }
        HandleCommand(parsed.options);
    }

    bool HandleCopyData(const COPYDATASTRUCT* data) {
        if (data == nullptr || data->dwData != kCopyDataCommand || data->lpData == nullptr || data->cbData == 0 ||
            data->cbData > 64 * 1024 || data->cbData % sizeof(wchar_t) != 0) {
            return false;
        }
        const auto* characters = static_cast<const wchar_t*>(data->lpData);
        const std::size_t count = data->cbData / sizeof(wchar_t);
        if (characters[count - 1] != L'\0') {
            return false;
        }
        if (deferred_commands_.size() >= kMaximumDeferredCommands) {
            return false;
        }
        try {
            deferred_commands_.emplace_back(characters, count - 1);
        } catch (...) {
            return false;
        }
        if (PostMessageW(window_, kDeferredCommandMessage, 0, 0) == FALSE) {
            deferred_commands_.pop_back();
            return false;
        }
        return true;
    }

    LRESULT HandleMessage(const UINT message, const WPARAM w_param, const LPARAM l_param) noexcept {
        if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
            tray_added_ = false;
            InitializeTrayIcon();
            if (tray_added_) {
                UpdateTrayTooltip();
            } else {
                ShowWindow(window_, SW_SHOW);
                SetStatus(
                    session_active_ ? L"Running: notification icon unavailable; window kept visible"
                                    : L"Stopped: notification icon unavailable; window kept visible");
            }
            return 0;
        }
        switch (message) {
        case WM_CREATE:
            if (const UINT window_dpi = GetDpiForWindow(window_); window_dpi != 0) {
                dpi_ = window_dpi;
            }
            CreateControls();
            return 0;
        case WM_COMMAND:
            if (LOWORD(w_param) == kProfile && HIWORD(w_param) == CBN_SELCHANGE) {
                ApplySelectedProfile();
            } else if (LOWORD(w_param) == kStart && HIWORD(w_param) == BN_CLICKED) {
                StartSession();
            } else if (LOWORD(w_param) == kStop && HIWORD(w_param) == BN_CLICKED) {
                StopSession();
            } else if (LOWORD(w_param) == kSave && HIWORD(w_param) == BN_CLICKED) {
                Save();
            } else if ((LOWORD(w_param) == kMotion || LOWORD(w_param) == kPower) &&
                       HIWORD(w_param) == CBN_SELCHANGE) {
                UpdateDirtyStateFromControls();
            } else if (HIWORD(w_param) == EN_CHANGE || HIWORD(w_param) == BN_CLICKED) {
                UpdateDirtyStateFromControls();
            }
            return 0;
        case WM_DRAWITEM:
            if (w_param == static_cast<WPARAM>(kStatus) &&
                reinterpret_cast<const DRAWITEMSTRUCT*>(l_param) != nullptr &&
                reinterpret_cast<const DRAWITEMSTRUCT*>(l_param)->hwndItem == status_) {
                DrawStatusCard(reinterpret_cast<const DRAWITEMSTRUCT*>(l_param));
                return TRUE;
            }
            break;
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            if (status_ != nullptr) {
                InvalidateRect(status_, nullptr, TRUE);
            }
            return 0;
        case WM_TIMER:
            if (w_param == kTimerId) {
                if (input_observer_requested_ && GetTickCount64() >= next_input_hook_refresh_tick_) {
                    const auto capabilities = input_monitor_.Refresh();
                    if (!capabilities.mouse || !capabilities.keyboard) {
                        FailSession(L"genuine-input observer lost; session stopped for safety");
                        return 0;
                    }
                    next_input_hook_refresh_tick_ = GetTickCount64() + kInputHookRefreshIntervalMs;
                }
                Evaluate(false);
            }
            return 0;
        case kGenuineInputMessage:
            input_monitor_.AcknowledgeNotification();
            Evaluate(true);
            return 0;
        case kDeferredCommandMessage:
            ProcessDeferredCommand();
            return 0;
        case kDeferredFocusMessage: {
            const HWND target = reinterpret_cast<HWND>(w_param);
            if (target != nullptr && IsChild(window_, target) != FALSE && IsWindowEnabled(target) != FALSE) {
                SetFocus(target);
            }
            return 0;
        }
        case WM_HOTKEY:
            if (w_param == kEmergencyHotkeyId) {
                StopSession();
                SetStatus(L"Stopped: emergency hotkey");
                ShowSafetyNotification(L"IdleHarbor emergency stop", L"The active session was stopped immediately.");
            }
            return 0;
        case WM_WTSSESSION_CHANGE:
            if (w_param == WTS_SESSION_LOCK) {
                locked_ = true;
            } else if (w_param == WTS_SESSION_UNLOCK) {
                locked_ = false;
            } else if (w_param == WTS_CONSOLE_DISCONNECT || w_param == WTS_REMOTE_DISCONNECT) {
                disconnected_ = true;
            } else if (w_param == WTS_CONSOLE_CONNECT || w_param == WTS_REMOTE_CONNECT) {
                disconnected_ = false;
            }
            Evaluate(false);
            return 0;
        case WM_COPYDATA:
            return HandleCopyData(reinterpret_cast<const COPYDATASTRUCT*>(l_param)) ? TRUE : FALSE;
        case kTrayMessage:
            if (LOWORD(l_param) == WM_LBUTTONDBLCLK || LOWORD(l_param) == WM_LBUTTONUP) {
                ShowWindow(window_, SW_SHOW);
                SetForegroundWindow(window_);
            } else if (LOWORD(l_param) == WM_CONTEXTMENU || LOWORD(l_param) == WM_RBUTTONUP) {
                ShowTrayMenu();
            }
            return 0;
        case WM_SIZE:
            if (w_param == SIZE_MINIMIZED && settings_.close_to_tray && tray_added_) {
                ShowWindow(window_, SW_HIDE);
            } else if (w_param != SIZE_MINIMIZED) {
                UpdateViewport();
                EnsureFocusedControlVisible();
            }
            return 0;
        case WM_GETMINMAXINFO:
            ApplyMinimumTrackingSize(reinterpret_cast<MINMAXINFO*>(l_param));
            return 0;
        case WM_VSCROLL:
            HandleVerticalScroll(w_param);
            return 0;
        case WM_MOUSEWHEEL:
            if (IsSettingsViewportPoint(l_param)) {
                HandleMouseWheel(w_param);
            }
            return 0;
        case WM_DPICHANGED:
            ApplyDpiChange(
                HIWORD(w_param),
                *reinterpret_cast<const RECT*>(l_param));
            return 0;
        case WM_CLOSE:
            if (settings_.close_to_tray && tray_added_ && !exiting_) {
                ShowWindow(window_, SW_HIDE);
                return 0;
            }
            DestroyWindow(window_);
            return 0;
        case WM_QUERYENDSESSION:
            StopSession();
            return TRUE;
        case WM_DESTROY:
            if (session_active_) {
                StopSession();
            }
            if (hotkey_registered_) {
                UnregisterHotKey(window_, kEmergencyHotkeyId);
                hotkey_registered_ = false;
            }
            if (session_notifications_available_) {
                WTSUnRegisterSessionNotification(window_);
                session_notifications_available_ = false;
            }
            session_state_available_ = false;
            RemoveTrayIcon();
            if (ui_font_ != nullptr) {
                DeleteObject(ui_font_);
                ui_font_ = nullptr;
            }
            if (heading_font_ != nullptr) {
                DeleteObject(heading_font_);
                heading_font_ = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window_, message, w_param, l_param);
    }

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND settings_viewport_ = nullptr;
    HWND status_ = nullptr;
    HWND profile_ = nullptr;
    HWND motion_ = nullptr;
    HWND power_ = nullptr;
    HWND interval_ = nullptr;
    HWND distance_ = nullptr;
    HWND randomize_ = nullptr;
    HWND pause_input_ = nullptr;
    HWND lock_pause_ = nullptr;
    HWND disconnect_pause_ = nullptr;
    HWND battery_ = nullptr;
    HWND pause_on_battery_ = nullptr;
    HWND fullscreen_ = nullptr;
    HWND max_duration_ = nullptr;
    HWND start_minimized_ = nullptr;
    HWND close_to_tray_ = nullptr;
    HWND notifications_ = nullptr;
    HWND emergency_hotkey_ = nullptr;
    HWND start_ = nullptr;
    HWND stop_ = nullptr;
    HWND save_ = nullptr;
    HICON tray_icon_ = nullptr;
    HFONT ui_font_ = nullptr;
    HFONT heading_font_ = nullptr;
    UINT taskbar_created_message_ = 0;
    UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
    bool tray_added_ = false;
    bool hotkey_registered_ = false;
    bool session_notifications_available_ = false;
    bool session_state_available_ = false;
    bool session_active_ = false;
    bool input_observer_requested_ = false;
    bool input_observer_available_ = false;
    bool input_observer_complete_ = false;
    bool locked_ = false;
    bool disconnected_ = false;
    bool exiting_ = false;
    HWND last_focus_ = nullptr;
    int scroll_position_ = 0;
    bool updating_viewport_ = false;
    int body_content_height_ = ScaleForDpi(kBaseBodyContentHeight, USER_DEFAULT_SCREEN_DPI);
    int wheel_delta_remainder_ = 0;
    std::uint64_t next_pulse_tick_ = 0;
    ULONGLONG next_input_hook_refresh_tick_ = 0;
    std::wstring status_text_ = L"Stopped: ready";
    std::filesystem::path settings_path_;
    std::vector<ChildLayout> child_layouts_;
    std::vector<std::string> settings_load_warnings_;
    std::deque<std::wstring> deferred_commands_;
    idleharbor::app::AppSettings settings_{};
    idleharbor::app::AppSettings saved_settings_{};
    Settings runtime_settings_{};
    InputMonitor input_monitor_{};
    PowerRequest power_request_{};
    std::unique_ptr<PolicyEngine> policy_{};
    std::unique_ptr<idleharbor::core::IntervalSampler> sampler_{};
    bool dirty_ = false;
    bool suppress_dirty_tracking_ = false;
};

bool SendCommandToExistingInstance(const std::wstring& raw_command_line) {
    HWND window = nullptr;
    for (int attempt = 0; attempt < 40 && window == nullptr; ++attempt) {
        window = FindWindowW(kWindowClassName, nullptr);
        if (window == nullptr) {
            Sleep(50);
        }
    }
    if (window == nullptr) {
        return false;
    }
    COPYDATASTRUCT data{};
    data.dwData = kCopyDataCommand;
    data.cbData = static_cast<DWORD>((raw_command_line.size() + 1) * sizeof(wchar_t));
    data.lpData = const_cast<wchar_t*>(raw_command_line.c_str());
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(
               window,
               WM_COPYDATA,
               0,
               reinterpret_cast<LPARAM>(&data),
               SMTO_ABORTIFHUNG,
               2000,
               &result) != 0 &&
           result == TRUE;
}

void ShowCommandLineError(const std::vector<std::wstring>& errors) {
    if (!errors.empty()) {
        MessageBoxW(nullptr, errors.front().c_str(), L"IdleHarbor command", MB_OK | MB_ICONWARNING);
    }
}

}  // namespace

int WINAPI wWinMain(const HINSTANCE instance, const HINSTANCE, const PWSTR, const int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const std::wstring raw_command_line = GetCommandLineW();
    int argument_count = 0;
    LPWSTR* arguments = CommandLineToArgvW(raw_command_line.c_str(), &argument_count);
    if (arguments == nullptr) {
        return 2;
    }
    std::vector<std::wstring_view> views;
    for (int index = 1; index < argument_count; ++index) {
        views.emplace_back(arguments[index]);
    }
    const auto parsed = idleharbor::app::ParseCommandLine(views);
    LocalFree(arguments);

    if (!parsed.ok()) {
        ShowCommandLineError(parsed.errors);
        return 2;
    }
    if (parsed.options.show_help) {
        const auto help = idleharbor::app::CommandLineHelp();
        MessageBoxW(nullptr, help.c_str(), idleharbor::kProductName.data(), MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    if (parsed.options.show_version) {
        const std::wstring version = std::wstring(idleharbor::kProductName) + L" " + std::wstring(idleharbor::kVersion);
        MessageBoxW(nullptr, version.c_str(), idleharbor::kProductName.data(), MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex == nullptr) {
        MessageBoxW(nullptr, L"Could not create the single-instance guard.", L"IdleHarbor", MB_OK | MB_ICONERROR);
        return 1;
    }
    const bool already_running = GetLastError() == ERROR_ALREADY_EXISTS;
    if (already_running) {
        CloseHandle(mutex);
        if (parsed.options.portable || parsed.options.config_path.has_value()) {
            MessageBoxW(
                nullptr,
                L"--portable and --config select storage only when the first IdleHarbor instance starts. "
                L"Exit the running instance before changing storage.",
                L"IdleHarbor command",
                MB_OK | MB_ICONWARNING);
            return 2;
        }
        if (!SendCommandToExistingInstance(raw_command_line)) {
            MessageBoxW(nullptr, L"The existing IdleHarbor window could not be contacted.", L"IdleHarbor", MB_OK | MB_ICONWARNING);
            return 1;
        }
        return 0;
    }

    if (parsed.options.command == RequestedCommand::Stop || parsed.options.command == RequestedCommand::Status ||
        parsed.options.command == RequestedCommand::Exit) {
        MessageBoxW(nullptr, L"IdleHarbor is not currently running.", L"IdleHarbor", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex);
        return 0;
    }

    Application application(instance);
    if (!application.Initialize(parsed.options)) {
        MessageBoxW(nullptr, L"IdleHarbor could not create its visible window.", L"IdleHarbor", MB_OK | MB_ICONERROR);
        CloseHandle(mutex);
        return 1;
    }
    if (parsed.options.command != RequestedCommand::Launch) {
        application.HandleCommand(parsed.options);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!application.HandleTabNavigation(message) && IsDialogMessageW(application.window(), &message) == FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        application.MaintainFocusedControlVisibility();
    }
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
