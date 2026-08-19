#include <windows.h>

#include <shellapi.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "idleharbor/app/command_line.hpp"
#include "idleharbor/app/settings_store.hpp"
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
constexpr UINT kDeferredStatusMessage = WM_APP + 3;
constexpr UINT kTimerId = 1;
constexpr int kEmergencyHotkeyId = 1;

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
        settings_path_ = idleharbor::app::ResolveSettingsPath(options.portable, options.config_path);
        settings_ = idleharbor::app::LoadSettings(settings_path_).settings;
        ApplyCommandLineOptions(options);
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
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            ScaleForDpi(600, dpi_),
            ScaleForDpi(700, dpi_),
            nullptr,
            nullptr,
            instance_,
            this);
        if (window_ == nullptr) {
            return false;
        }

        InitializeTrayIcon();
        session_notifications_available_ =
            WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION) != FALSE;
        ApplyEmergencyHotkeySetting();

        RefreshControls();
        std::wstring initial_status = L"Stopped: ready";
        if (!tray_added_) {
            initial_status = L"Stopped: notification icon unavailable; window kept visible";
        } else if (settings_.emergency_hotkey && !hotkey_registered_) {
            initial_status = L"Stopped: emergency hotkey unavailable";
        } else if ((settings_.session.pause_when_locked || settings_.session.pause_when_disconnected) &&
                   !session_notifications_available_) {
            initial_status = L"Stopped: session-change observer unavailable";
        }
        SetStatus(initial_status);
        if ((options.minimized || settings_.start_minimized) && tray_added_) {
            ShowWindow(window_, SW_HIDE);
        } else {
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
        }
        return true;
    }

    void HandleCommand(const CommandLineOptions& options, const bool forwarded = false) {
        const bool restart_for_overrides = session_active_ && HasRuntimeOverrides(options);
        ApplyCommandLineOptions(options);
        RefreshControls();
        switch (options.command) {
        case RequestedCommand::Launch:
            if (options.minimized && tray_added_) {
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
            if (forwarded) {
                // WM_COPYDATA is synchronous. Defer the modal local status dialog so a
                // second-instance --status request cannot hold the sender past its timeout.
                PostMessageW(window_, kDeferredStatusMessage, 0, 0);
            } else {
                MessageBoxW(window_, status_text_.c_str(), L"IdleHarbor status", MB_OK | MB_ICONINFORMATION);
            }
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

  private:
    void ApplyDpiChange(const UINT new_dpi, const RECT& suggested) {
        if (new_dpi == 0 || new_dpi == dpi_) {
            return;
        }

        struct ChildPlacement {
            HWND window{};
            RECT rectangle{};
        };
        std::vector<ChildPlacement> children;
        for (HWND child = GetWindow(window_, GW_CHILD); child != nullptr; child = GetWindow(child, GW_HWNDNEXT)) {
            RECT rectangle{};
            if (GetWindowRect(child, &rectangle) != FALSE) {
                MapWindowPoints(HWND_DESKTOP, window_, reinterpret_cast<POINT*>(&rectangle), 2);
                children.push_back({child, rectangle});
            }
        }

        const UINT old_dpi = dpi_;
        SetWindowPos(
            window_,
            nullptr,
            suggested.left,
            suggested.top,
            suggested.right - suggested.left,
            suggested.bottom - suggested.top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        dpi_ = new_dpi;

        HFONT replacement_font = CreateUiFont(dpi_);
        for (const auto& child : children) {
            const int x = MulDiv(child.rectangle.left, static_cast<int>(new_dpi), static_cast<int>(old_dpi));
            const int y = MulDiv(child.rectangle.top, static_cast<int>(new_dpi), static_cast<int>(old_dpi));
            const int width = MulDiv(
                child.rectangle.right - child.rectangle.left,
                static_cast<int>(new_dpi),
                static_cast<int>(old_dpi));
            const int height = MulDiv(
                child.rectangle.bottom - child.rectangle.top,
                static_cast<int>(new_dpi),
                static_cast<int>(old_dpi));
            SetWindowPos(child.window, nullptr, x, y, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
            if (replacement_font != nullptr) {
                SendMessageW(child.window, WM_SETFONT, reinterpret_cast<WPARAM>(replacement_font), TRUE);
            }
        }
        if (replacement_font != nullptr) {
            if (ui_font_ != nullptr) {
                DeleteObject(ui_font_);
            }
            ui_font_ = replacement_font;
        }
        InvalidateRect(window_, nullptr, TRUE);
    }

    void CreateControls() {
        ui_font_ = CreateUiFont(dpi_);
        const HFONT font = ui_font_ != nullptr ? ui_font_ : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        const auto scale = [&](const int value) { return ScaleForDpi(value, dpi_); };
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
                window_,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
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
                window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
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
                window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
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
                window_,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };

        status_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"Stopped: ready",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            scale(20),
            scale(18),
            scale(525),
            scale(28),
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatus)),
            instance_,
            nullptr);
        SendMessageW(status_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        add_label(L"Profile", 56);
        add_combo(profile_, 56, kProfile);
        for (const auto profile : kProfiles) {
            const std::wstring text = Widen(idleharbor::core::profile_kind_name(profile));
            SendMessageW(profile_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Motion", 90);
        add_combo(motion_, 90, kMotion);
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

        add_label(L"Power request", 124);
        add_combo(power_, 124, kPower);
        const std::array<std::wstring, 3> powers{L"None", L"System sleep", L"Display and system"};
        for (const auto& text : powers) {
            SendMessageW(power_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Pulse interval (seconds)", 158);
        add_edit(interval_, 158, kInterval);
        add_label(L"Motion distance (1-120)", 192);
        add_edit(distance_, 192, kDistance);
        add_check(randomize_, L"Randomize pulse interval", 224, kRandomize);
        add_label(L"Pause after genuine input (seconds; 0 disables)", 258);
        add_edit(pause_input_, 258, kPauseInput);
        add_check(lock_pause_, L"Pause while the workstation is locked", 290, kLockPause);
        add_check(disconnect_pause_, L"Pause while the session is disconnected", 322, kDisconnectPause);
        add_label(L"Low-battery threshold (0 disables)", 354);
        add_edit(battery_, 354, kBattery);
        add_check(pause_on_battery_, L"Pause whenever the device is on battery", 386, kPauseOnBattery);
        add_check(fullscreen_, L"Pause while a full-screen application is foreground", 418, kFullscreen);
        add_label(L"Maximum session duration (seconds; 0 disables)", 450);
        add_edit(max_duration_, 450, kMaxDuration);
        add_check(start_minimized_, L"Start minimized to the notification area", 484, kStartMinimized);
        add_check(close_to_tray_, L"Close button hides to the notification area", 516, kCloseToTray);
        add_check(notifications_, L"Show safety state notifications", 548, kNotifications);
        add_check(emergency_hotkey_, L"Enable emergency stop: Ctrl+Alt+Shift+F12", 580, kEmergencyHotkey);

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
        };
        add_button(start_, L"Start", 20, kStart);
        add_button(stop_, L"Stop", 145, kStop);
        add_button(save_, L"Save", 270, kSave);
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
        const std::wstring tooltip = L"IdleHarbor - " + status_text_;
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
            SetControlText(status_, status_text_);
        }
        UpdateTrayTooltip();
    }

    void RefreshControls() {
        if (profile_ == nullptr) {
            return;
        }
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
        UpdateButtons();
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
        if (start_ != nullptr) {
            EnableWindow(start_, session_active_ ? FALSE : TRUE);
        }
        if (stop_ != nullptr) {
            EnableWindow(stop_, session_active_ ? TRUE : FALSE);
        }
        for (const HWND control : {profile_, motion_, power_, interval_, distance_, randomize_, pause_input_,
                                   lock_pause_, disconnect_pause_, battery_, pause_on_battery_, fullscreen_,
                                   max_duration_, start_minimized_, close_to_tray_, notifications_,
                                   emergency_hotkey_, save_}) {
            if (control != nullptr) {
                EnableWindow(control, session_active_ ? FALSE : TRUE);
            }
        }
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
            error = L"Motion distance must be between 1 and 120.";
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

        ApplyEmergencyHotkeySetting();
        runtime_settings_ = settings_.session;
        if ((runtime_settings_.pause_when_locked || runtime_settings_.pause_when_disconnected) &&
            !session_notifications_available_) {
            SetStatus(L"Stopped: session-change observer unavailable");
            MessageBoxW(
                window_,
                L"Windows session-change notifications are unavailable. Disable the lock/disconnect safeguard "
                L"explicitly or resolve the Windows error before starting.",
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
    }

    void StopSession() {
        if (policy_ != nullptr) {
            policy_->stop();
        }
        const PolicyDecision decision{DecisionAction::Stop, EngineState::Stopped, PolicyReason::Manual, Seconds{0}};
        EndSession(decision);
    }

    void EndSession(const PolicyDecision& decision) {
        KillTimer(window_, kTimerId);
        input_monitor_.Stop();
        power_request_.Clear();
        sampler_.reset();
        policy_.reset();
        session_active_ = false;
        input_observer_requested_ = false;
        input_observer_available_ = false;
        input_observer_complete_ = false;
        const auto text = idleharbor::core::status_text(decision);
        SetStatus(std::wstring(text.begin(), text.end()));
        if (decision.reason == PolicyReason::MaxDuration) {
            ShowSafetyNotification(L"IdleHarbor session stopped", L"The configured maximum duration was reached.");
        }
        UpdateButtons();
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
        if (settings_.emergency_hotkey && !hotkey_registered_) {
            SetStatus(L"Stopped: settings saved; emergency hotkey unavailable");
        } else {
            SetStatus(session_active_ ? L"Running: settings saved; restart to apply changes" : L"Stopped: settings saved");
        }
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
        std::wstring command_line(characters, count - 1);
        int argument_count = 0;
        LPWSTR* arguments = CommandLineToArgvW(command_line.c_str(), &argument_count);
        if (arguments == nullptr) {
            return false;
        }
        std::vector<std::wstring_view> views;
        for (int index = 1; index < argument_count; ++index) {
            views.emplace_back(arguments[index]);
        }
        const auto parsed = idleharbor::app::ParseCommandLine(views);
        if (!parsed.ok()) {
            MessageBoxW(window_, parsed.errors.front().c_str(), L"IdleHarbor command", MB_OK | MB_ICONWARNING);
            LocalFree(arguments);
            return true;
        }
        HandleCommand(parsed.options, true);
        LocalFree(arguments);
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
            }
            return 0;
        case WM_TIMER:
            if (w_param == kTimerId) {
                if (input_observer_requested_) {
                    const auto capabilities = input_monitor_.Refresh();
                    if (!capabilities.mouse || !capabilities.keyboard) {
                        FailSession(L"genuine-input observer lost; session stopped for safety");
                        return 0;
                    }
                }
                Evaluate(false);
            }
            return 0;
        case kGenuineInputMessage:
            input_monitor_.AcknowledgeNotification();
            Evaluate(true);
            return 0;
        case kDeferredStatusMessage:
            MessageBoxW(window_, status_text_.c_str(), L"IdleHarbor status", MB_OK | MB_ICONINFORMATION);
            return 0;
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
            RemoveTrayIcon();
            if (ui_font_ != nullptr) {
                DeleteObject(ui_font_);
                ui_font_ = nullptr;
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
    UINT taskbar_created_message_ = 0;
    UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
    bool tray_added_ = false;
    bool hotkey_registered_ = false;
    bool session_notifications_available_ = false;
    bool session_active_ = false;
    bool input_observer_requested_ = false;
    bool input_observer_available_ = false;
    bool input_observer_complete_ = false;
    bool locked_ = false;
    bool disconnected_ = false;
    bool exiting_ = false;
    std::uint64_t next_pulse_tick_ = 0;
    std::wstring status_text_ = L"Stopped: ready";
    std::filesystem::path settings_path_;
    idleharbor::app::AppSettings settings_{};
    Settings runtime_settings_{};
    InputMonitor input_monitor_{};
    PowerRequest power_request_{};
    std::unique_ptr<PolicyEngine> policy_{};
    std::unique_ptr<idleharbor::core::IntervalSampler> sampler_{};
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
        if (IsDialogMessageW(application.window(), &message) == FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
