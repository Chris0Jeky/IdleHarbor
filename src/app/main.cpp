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
    std::wstring text(static_cast<std::size_t>(std::max(length, 0)), L'\0');
    if (length > 0) {
        GetWindowTextW(control, text.data(), length + 1);
    }
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
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            600,
            665,
            nullptr,
            nullptr,
            instance_,
            this);
        if (window_ == nullptr) {
            return false;
        }

        InitializeTrayIcon();
        WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION);
        if (settings_.emergency_hotkey) {
            hotkey_registered_ = RegisterHotKey(
                window_,
                kEmergencyHotkeyId,
                MOD_CONTROL | MOD_ALT | MOD_SHIFT,
                VK_F12) != FALSE;
        }

        RefreshControls();
        SetStatus(L"Stopped: ready");
        if (options.minimized || settings_.start_minimized) {
            ShowWindow(window_, SW_HIDE);
        } else {
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
        }
        return true;
    }

    void HandleCommand(const CommandLineOptions& options) {
        ApplyCommandLineOptions(options);
        RefreshControls();
        switch (options.command) {
        case RequestedCommand::Launch:
            if (options.minimized) {
                ShowWindow(window_, SW_HIDE);
            } else {
                ShowWindow(window_, SW_SHOW);
                SetForegroundWindow(window_);
            }
            break;
        case RequestedCommand::Start:
            ShowWindow(window_, options.minimized ? SW_HIDE : SW_SHOW);
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
    void CreateControls() {
        const HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        const auto add_label = [&](const wchar_t* text, const int y) {
            const HWND control = CreateWindowExW(
                0,
                L"STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                20,
                y,
                270,
                24,
                window_,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        const auto add_combo = [&](HWND& target, const int y) {
            target = CreateWindowExW(
                0,
                L"COMBOBOX",
                nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                300,
                y - 3,
                245,
                260,
                window_,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        const auto add_edit = [&](HWND& target, const int y) {
            target = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                nullptr,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
                300,
                y - 3,
                145,
                26,
                window_,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        const auto add_check = [&](HWND& target, const wchar_t* text, const int y) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                20,
                y,
                525,
                26,
                window_,
                nullptr,
                instance_,
                nullptr);
            SendMessageW(target, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };

        status_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"Stopped: ready",
            WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
            20,
            18,
            525,
            28,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatus)),
            instance_,
            nullptr);
        SendMessageW(status_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        add_label(L"Profile", 68);
        add_combo(profile_, 68);
        for (const auto profile : kProfiles) {
            const std::wstring text = Widen(idleharbor::core::profile_kind_name(profile));
            SendMessageW(profile_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Motion", 108);
        add_combo(motion_, 108);
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

        add_label(L"Power request", 148);
        add_combo(power_, 148);
        const std::array<std::wstring, 3> powers{L"None", L"System sleep", L"Display and system"};
        for (const auto& text : powers) {
            SendMessageW(power_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }

        add_label(L"Pulse interval (seconds)", 188);
        add_edit(interval_, 188);
        add_label(L"Motion distance (1-120)", 228);
        add_edit(distance_, 228);
        add_check(randomize_, L"Randomize pulse interval", 266);
        add_label(L"Pause after genuine input (seconds; 0 disables)", 306);
        add_edit(pause_input_, 306);
        add_check(lock_pause_, L"Pause while the workstation is locked", 346);
        add_label(L"Low-battery threshold (0 disables)", 386);
        add_edit(battery_, 386);
        add_check(fullscreen_, L"Pause while a full-screen application is foreground", 426);
        add_label(L"Maximum session duration (seconds; 0 disables)", 466);
        add_edit(max_duration_, 466);
        add_check(start_minimized_, L"Start minimized to the notification area", 506);
        add_check(close_to_tray_, L"Close button hides to the notification area", 536);

        const auto add_button = [&](HWND& target, const wchar_t* text, const int x, const int id) {
            target = CreateWindowExW(
                0,
                L"BUTTON",
                text,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                x,
                578,
                115,
                32,
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
        wcscpy_s(icon.szTip, L"IdleHarbor");
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
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
        Shell_NotifyIconW(NIM_MODIFY, &icon);
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
        SetControlText(battery_, std::to_wstring(settings_.session.pause_on_low_battery
                                                     ? settings_.session.low_battery_threshold
                                                     : 0));
        SetChecked(fullscreen_, settings_.session.pause_when_fullscreen);
        SetControlText(max_duration_, std::to_wstring(settings_.session.max_duration.count()));
        SetChecked(start_minimized_, settings_.start_minimized);
        SetChecked(close_to_tray_, settings_.close_to_tray);
        UpdateButtons();
    }

    void UpdateButtons() {
        if (start_ != nullptr) {
            EnableWindow(start_, session_active_ ? FALSE : TRUE);
        }
        if (stop_ != nullptr) {
            EnableWindow(stop_, session_active_ ? TRUE : FALSE);
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
        if (!read_number(battery_, 100, value)) {
            error = L"Battery threshold must be between 0 and 100.";
            return false;
        }
        session.pause_on_low_battery = value != 0;
        if (value != 0) {
            session.low_battery_threshold = static_cast<std::uint8_t>(value);
        }
        session.pause_when_fullscreen = IsChecked(fullscreen_);
        if (!read_number(max_duration_, 30ULL * 24 * 60 * 60, value)) {
            error = L"Maximum duration must be between 0 and 2592000 seconds.";
            return false;
        }
        session.max_duration = Seconds{static_cast<std::int64_t>(value)};
        settings_.start_minimized = IsChecked(start_minimized_);
        settings_.close_to_tray = IsChecked(close_to_tray_);

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
        const auto validation = idleharbor::core::validate(settings_.session);
        if (!validation.valid) {
            settings_.session = idleharbor::core::settings_for_profile(settings_.session.profile);
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
            settings_.session.pause_when_fullscreen && idleharbor::platform::windows::IsForegroundFullscreen(),
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

        policy_ = std::make_unique<PolicyEngine>(settings_.session);
        policy_->start(NowSeconds());
        sampler_ = std::make_unique<idleharbor::core::IntervalSampler>(
            GetTickCount64() ^ static_cast<ULONGLONG>(GetCurrentProcessId()),
            settings_.session.random_minimum,
            settings_.session.interval,
            settings_.session.randomize);
        next_pulse_tick_ = GetTickCount64() + static_cast<ULONGLONG>(sampler_->next().count()) * 1000ULL;
        const auto capabilities = input_monitor_.Start(window_, kGenuineInputMessage);
        input_observer_available_ = capabilities.any();
        session_active_ = true;
        SetTimer(window_, kTimerId, 1000, nullptr);
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
        session_active_ = false;
        input_observer_available_ = false;
        const auto text = idleharbor::core::status_text(decision);
        SetStatus(std::wstring(text.begin(), text.end()));
        UpdateButtons();
    }

    void FailSession(const std::wstring& message) {
        if (policy_ != nullptr) {
            policy_->stop();
        }
        const PolicyDecision decision{DecisionAction::Stop, EngineState::Stopped, PolicyReason::Manual, Seconds{0}};
        EndSession(decision);
        SetStatus(L"Stopped: " + message);
    }

    bool ApplyPower() {
        const auto requested = ToPlatformPowerMode(settings_.session.power);
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
        if (settings_.session.motion == MotionMode::Off) {
            return true;
        }
        MotionEmissionResult result{};
        if (settings_.session.motion == MotionMode::Zen) {
            result = idleharbor::platform::windows::EmitZenPulse();
        } else {
            const auto plan = idleharbor::core::make_motion_plan(settings_.session.motion, settings_.session.distance);
            std::vector<POINT> offsets;
            offsets.reserve(plan.relative_offsets.size());
            for (const auto point : plan.relative_offsets) {
                offsets.push_back(POINT{point.x, point.y});
            }
            result = idleharbor::platform::windows::EmitMotionPulse(offsets);
        }
        if (!result.succeeded) {
            FailSession(L"input emission failed (" + WindowsErrorText(result.error) + L")");
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
            SetStatus(std::wstring(text.begin(), text.end()));
            return;
        }
        if ((!was_running || power_request_.mode() != ToPlatformPowerMode(settings_.session.power)) && !ApplyPower()) {
            return;
        }
        if (GetTickCount64() >= next_pulse_tick_) {
            if (!EmitPulse()) {
                return;
            }
            ScheduleNextPulse();
        }
        std::wstring status = L"Running";
        if (!input_observer_available_) {
            status += L" (genuine-input observer unavailable)";
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
        std::string save_error;
        if (!idleharbor::app::SaveSettings(settings_path_, settings_, save_error)) {
            const std::wstring wide(save_error.begin(), save_error.end());
            MessageBoxW(window_, wide.c_str(), L"IdleHarbor settings", MB_OK | MB_ICONERROR);
            return;
        }
        SetStatus(session_active_ ? L"Running: settings saved; restart to apply changes" : L"Stopped: settings saved");
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
        HandleCommand(parsed.options);
        LocalFree(arguments);
        return true;
    }

    LRESULT HandleMessage(const UINT message, const WPARAM w_param, const LPARAM l_param) noexcept {
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
                Evaluate(false);
            }
            return 0;
        case kGenuineInputMessage:
            Evaluate(true);
            return 0;
        case WM_HOTKEY:
            if (w_param == kEmergencyHotkeyId) {
                StopSession();
                SetStatus(L"Stopped: emergency hotkey");
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
            if (l_param == WM_LBUTTONDBLCLK || l_param == WM_LBUTTONUP) {
                ShowWindow(window_, SW_SHOW);
                SetForegroundWindow(window_);
            } else if (l_param == WM_RBUTTONUP) {
                ShowTrayMenu();
            }
            return 0;
        case WM_SIZE:
            if (w_param == SIZE_MINIMIZED && settings_.close_to_tray) {
                ShowWindow(window_, SW_HIDE);
            }
            return 0;
        case WM_CLOSE:
            if (settings_.close_to_tray && !exiting_) {
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
            WTSUnRegisterSessionNotification(window_);
            RemoveTrayIcon();
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
    HWND battery_ = nullptr;
    HWND fullscreen_ = nullptr;
    HWND max_duration_ = nullptr;
    HWND start_minimized_ = nullptr;
    HWND close_to_tray_ = nullptr;
    HWND start_ = nullptr;
    HWND stop_ = nullptr;
    HWND save_ = nullptr;
    HICON tray_icon_ = nullptr;
    bool tray_added_ = false;
    bool hotkey_registered_ = false;
    bool session_active_ = false;
    bool input_observer_available_ = false;
    bool locked_ = false;
    bool disconnected_ = false;
    bool exiting_ = false;
    std::uint64_t next_pulse_tick_ = 0;
    std::wstring status_text_ = L"Stopped: ready";
    std::filesystem::path settings_path_;
    idleharbor::app::AppSettings settings_{};
    InputMonitor input_monitor_{};
    PowerRequest power_request_{};
    std::unique_ptr<PolicyEngine> policy_{};
    std::unique_ptr<idleharbor::core::IntervalSampler> sampler_{};
};

bool SendCommandToExistingInstance(const std::wstring& raw_command_line) {
    const HWND window = FindWindowW(kWindowClassName, nullptr);
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
               &result) != 0;
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
    application.HandleCommand(parsed.options);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
