#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
bool add_succeeds = true;
bool modify_succeeds = true;
std::vector<DWORD> tray_calls;
std::wstring last_tooltip;
std::wstring last_dialog;
int visibility_requests = 0;
int failures = 0;
int assertions = 0;
void Expect(bool condition, std::string_view label) {
    ++assertions;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

BOOL WINAPI TestNotifyIcon(DWORD action, PNOTIFYICONDATAW icon) {
    tray_calls.push_back(action);
    if ((icon->uFlags & NIF_TIP) != 0) last_tooltip = icon->szTip;
    if (action == NIM_ADD) return add_succeeds ? TRUE : FALSE;
    if (action == NIM_MODIFY) return modify_succeeds ? TRUE : FALSE;
    return TRUE;
}
int WINAPI TestMessageBox(HWND, LPCWSTR text, LPCWSTR, UINT) {
    last_dialog = text;
    return IDOK;
}
BOOL WINAPI TestShowWindow(HWND, int command) {
    if (command == SW_SHOW) ++visibility_requests;
    return TRUE;
}

// Only the generated test copy has a friend access declaration. Its methods are
// otherwise byte-identical to main.cpp. The application target uses main.cpp
// directly and has no private-access or API interception macros.
#define Shell_NotifyIconW TestNotifyIcon
#define MessageBoxW TestMessageBox
#define ShowWindow TestShowWindow
#include "application-under-test.inc"
#undef ShowWindow
#undef MessageBoxW
#undef Shell_NotifyIconW

namespace {
struct ApplicationStatusTestAccess {
    static void Prepare(Application& app, HWND window, HWND status, bool dirty, bool running) {
        app.window_ = window;
        app.status_ = status;
        app.taskbar_created_message_ = WM_APP + 100;
        app.session_active_ = running;
        app.settings_ = {};
        app.saved_settings_ = app.settings_;
        if (dirty) app.settings_.close_to_tray = !app.saved_settings_.close_to_tray;
        app.dirty_ = dirty;
        app.InitializeTrayIcon();
    }
    static void SetStatus(Application& app, const std::wstring& value) { app.SetStatus(value); }
    static std::wstring Display(const Application& app) { return app.DisplayStatusText(); }
    static std::wstring Base(const Application& app) { return app.status_text_; }
    static void Lose(Application& app) { app.MarkTrayUnavailable(); }
    static void Recreate(Application& app) { app.HandleMessage(app.taskbar_created_message_, 0, 0); }
    static void Modify(Application& app) { app.UpdateTrayTooltip(); }
    static void Recover(Application& app) { app.RecoverTrayIcon(); }
    static bool Running(const Application& app) { return app.session_active_; }
    static bool Tray(const Application& app) { return app.tray_added_; }
    static void Stop(Application& app) { app.StopSession(); }
};

std::wstring NativeText(HWND window) {
    std::array<wchar_t, 1024> text{};
    GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
    return text.data();
}
std::wstring Expected(std::wstring_view base, bool dirty, bool unavailable) {
    std::wstring text = dirty ? L"Unsaved changes \u2014 " : L"";
    text += base;
    if (unavailable) text += L"; notification icon unavailable; window kept visible";
    return text;
}
std::wstring Tooltip(const std::wstring& text) { return (L"IdleHarbor - " + text).substr(0, 127); }

void HelperContracts() {
    Expect(ParseUnsigned(L"0") == 0, "unsigned zero parses");
    Expect(ParseUnsigned(L"00042") == 42, "leading zeros preserve value");
    Expect(ParseUnsigned(L"18446744073709551615") == std::numeric_limits<std::uint64_t>::max(), "UINT64_MAX parses exactly");
    for (const auto* value : {L"", L"-1", L"+1", L" 1", L"1 ", L"1.0", L"1e2", L"18446744073709551616", L"999999999999999999999999", L"\uff11"}) {
        Expect(!ParseUnsigned(value).has_value(), "invalid or overflowing unsigned input is rejected");
    }
    for (const auto& [text, value] : std::array<std::pair<std::wstring_view, ProfileKind>, 7>{{
        {L"BALANCED", ProfileKind::Balanced}, {L"LONG-TASK", ProfileKind::LongTask},
        {L"PRESENTATION", ProfileKind::Presentation}, {L"COMPATIBILITY", ProfileKind::Compatibility},
        {L"VISIBLE", ProfileKind::Visible}, {L"BATTERY-SAVER", ProfileKind::BatterySaver}, {L"CUSTOM", ProfileKind::Custom},
    }}) Expect(ProfileFromText(text) == value, "profile text maps to its exact enum");
    for (const auto& [text, value] : std::array<std::pair<std::wstring_view, MotionMode>, 6>{{
        {L"OFF", MotionMode::Off}, {L"NORMAL", MotionMode::Normal}, {L"DIAGONAL", MotionMode::Normal},
        {L"ZEN", MotionMode::Zen}, {L"CIRCLE", MotionMode::Circle}, {L"LINEAR", MotionMode::Linear},
    }}) Expect(MotionFromText(text) == value, "motion text and aliases map to their exact enum");
    for (const auto& [text, value] : std::array<std::pair<std::wstring_view, PowerMode>, 3>{{
        {L"NONE", PowerMode::None}, {L"SYSTEM", PowerMode::System}, {L"DISPLAY", PowerMode::Display},
    }}) Expect(PowerFromText(text) == value, "power text maps to its exact enum");
    for (const auto* text : {L"", L"unknown", L" system", L"off "}) {
        Expect(!ProfileFromText(text).has_value(), "unknown profile text is rejected");
        Expect(!MotionFromText(text).has_value(), "unknown motion text is rejected");
        Expect(!PowerFromText(text).has_value(), "unknown power text is rejected");
    }
}
}

int main() {
    HelperContracts();
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const HWND window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 400, 100, nullptr, nullptr, instance, nullptr);
    const HWND status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 400, 100, window, nullptr, instance, nullptr);
    if (window == nullptr || status == nullptr) {
        if (window != nullptr) DestroyWindow(window);
        std::cerr << "Cannot create the hidden native status fixture.\n";
        return 1;
    }
    int scenarios = 0;
    for (const auto& [base, active] : std::array<std::pair<std::wstring_view, bool>, 8>{{
        {L"Stopped: ready", false}, {L"Stopped: current session state unavailable", false},
        {L"Stopped: settings recovered; review and save before automatic start", false},
        {L"Running", true}, {L"Paused: workstation locked", true},
        {L"Paused: low battery", true}, {L"Paused: user activity cooldown (5s remaining)", true},
        {L"Paused: fullscreen activity", true},
    }}) {
        for (const bool dirty : {false, true}) {
            ++scenarios;
            tray_calls.clear(); visibility_requests = 0; last_dialog.clear();
            add_succeeds = true; modify_succeeds = true;
            Application app(instance);
            ApplicationStatusTestAccess::Prepare(app, window, status, dirty, active);
            ApplicationStatusTestAccess::SetStatus(app, std::wstring(base));
            const auto healthy = Expected(base, dirty, false);
            Expect(NativeText(status) == healthy, "normal native status exposes the composed text");
            CommandLineOptions command;
            command.command = RequestedCommand::Status;
            app.HandleCommand(command);
            Expect(last_dialog == healthy, "status command agrees with the composed status card");

            ApplicationStatusTestAccess::Lose(app);
            const auto unavailable = Expected(base, dirty, true);
            Expect(ApplicationStatusTestAccess::Base(app) == base, "tray loss preserves the underlying status reason");
            Expect(ApplicationStatusTestAccess::Display(app) == unavailable, "tray loss is an overlay on the actual session state");
            Expect(NativeText(status) == ApplicationStatusTestAccess::Display(app), "tray failure keeps accessible and painted text identical");
            Expect(visibility_requests > 0, "tray failure requests a visible recovery window");

            add_succeeds = false;
            ApplicationStatusTestAccess::Recreate(app);
            Expect(NativeText(status) == unavailable, "failed taskbar recovery retains the composed state and warning");
            Expect(ApplicationStatusTestAccess::Base(app) == base, "failed taskbar recovery does not overwrite the base status");
            add_succeeds = true;
            ApplicationStatusTestAccess::Recreate(app);
            Expect(ApplicationStatusTestAccess::Tray(app), "successful taskbar recovery restores tray availability");
            Expect(NativeText(status) == healthy, "successful taskbar recovery removes only the transient warning");
            Expect(last_tooltip == Tooltip(healthy), "recovered tooltip uses the same current composed status");
            app.HandleCommand(command);
            Expect(last_dialog == healthy, "recovered status command retracts the transient warning");

            modify_succeeds = false; add_succeeds = false;
            const auto before = tray_calls.size();
            ApplicationStatusTestAccess::Modify(app);
            Expect(tray_calls.size() - before <= 3, "failed tooltip recovery is bounded and nonrecursive");
            Expect(NativeText(status) == unavailable, "failed tooltip recovery keeps the actual reason accessible");
            add_succeeds = true;
            ApplicationStatusTestAccess::Recover(app);
            Expect(NativeText(status) == healthy, "direct recovery refreshes accessible text immediately");
            Expect(last_tooltip == Tooltip(healthy), "newly added icon never receives the stale unavailable tooltip");

            ApplicationStatusTestAccess::Lose(app);
            add_succeeds = false;
            ApplicationStatusTestAccess::Stop(app);
            Expect(!ApplicationStatusTestAccess::Running(app), "Stop remains immediate after tray failure");
            Expect(NativeText(status) == Expected(L"Stopped: manually stopped", dirty, true), "Stop updates the base state without hiding a tray failure");
            Expect(IsWindowVisible(window) == FALSE && IsWindowVisible(status) == FALSE, "fixture never shows an actual desktop window");
        }
    }
    DestroyWindow(window);
    if (failures) { std::cerr << failures << " of " << assertions << " assertions failed across " << scenarios << " status scenarios.\n"; return 1; }
    std::cout << "Application status and helper contracts passed (" << assertions << " assertions, " << scenarios << " status scenarios).\n";
}
