#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "idleharbor/platform/windows/system_snapshot.hpp"

namespace {
bool add_succeeds = true;
bool modify_succeeds = true;
std::vector<DWORD> tray_calls;
std::wstring last_tooltip;
std::wstring last_dialog;
int visibility_requests = 0;
int hide_requests = 0;
int failures = 0;
int assertions = 0;
idleharbor::platform::windows::SessionSnapshot session_snapshot{};
int session_queries = 0;
bool session_registration_succeeds = false;
int session_registrations = 0;
void Expect(bool condition, std::string_view label) {
    ++assertions;
    if (!condition) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
}

namespace idleharbor::platform::windows {
SessionSnapshot TestQuerySessionSnapshot() noexcept {
    ++session_queries;
    return session_snapshot;
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
    if (command == SW_HIDE) ++hide_requests;
    return TRUE;
}
BOOL WINAPI TestRegisterSession(HWND window, DWORD flags) {
    ++session_registrations;
    Expect(window != nullptr && flags == NOTIFY_FOR_THIS_SESSION, "session retry registers only the current session and a valid window");
    return session_registration_succeeds ? TRUE : FALSE;
}

// Only the generated test copy has a friend access declaration. Its methods are
// otherwise byte-identical to main.cpp. The application target uses main.cpp
// directly and has no private-access or API interception macros.
#define Shell_NotifyIconW TestNotifyIcon
#define MessageBoxW TestMessageBox
#define ShowWindow TestShowWindow
#define QuerySessionSnapshot TestQuerySessionSnapshot
#define WTSRegisterSessionNotification TestRegisterSession
#include "application-under-test.inc"
#undef WTSRegisterSessionNotification
#undef QuerySessionSnapshot
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
    static void PrepareSession(Application& app, HWND window, HWND status, bool notifications = true) {
        Prepare(app, window, status, false, false);
        app.session_notifications_available_ = notifications;
        app.session_state_available_ = false;
        app.locked_ = true;
        app.disconnected_ = true;
        app.SetStatus(kSessionStateUnavailableStatus);
    }
    static bool Establish(Application& app) { return app.EstablishSessionState(); }
    static bool Available(const Application& app) { return app.session_state_available_; }
    static bool Locked(const Application& app) { return app.locked_; }
    static bool Disconnected(const Application& app) { return app.disconnected_; }
    static void Notify(Application& app, WPARAM event) { app.HandleMessage(WM_WTSSESSION_CHANGE, event, 0); }
    static void PrepareStartControls(Application& app) {
        // Never install hooks, emit input or acquire a power request in this fixture.
        app.settings_.session.pause_on_user_activity = false;
        app.settings_.session.pause_on_low_battery = false;
        app.settings_.session.motion = MotionMode::Zen;
        app.settings_.session.power = PowerMode::None;
        app.settings_.session.interval = Seconds{86400};
        app.settings_.emergency_hotkey = false;
        app.settings_.show_notifications = false;
        app.saved_settings_ = app.settings_;
        app.CreateControls();
        app.RefreshControls();
    }
    static void Start(Application& app) { app.StartSession(); }
    static void PendingCleanup(Application& app, bool pending) { app.power_cleanup_pending_ = pending; }
    static void EmergencyStop(Application& app) { app.HandleMessage(WM_HOTKEY, kEmergencyHotkeyId, 0); }
    static void Close(Application& app, bool tray_available) {
        app.settings_.close_to_tray = true;
        app.tray_added_ = tray_available;
        app.HandleMessage(WM_CLOSE, 0, 0);
    }
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

void SessionRecoveryContracts(HINSTANCE instance) {
    const HWND window = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 640, 800, nullptr, nullptr, instance, nullptr);
    const HWND status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 400, 100, window, nullptr, instance, nullptr);
    Expect(window != nullptr && status != nullptr, "hidden session recovery fixture creates real native controls");
    if (window == nullptr || status == nullptr) { if (window != nullptr) DestroyWindow(window); return; }
    {
        Application app(instance);
        ApplicationStatusTestAccess::PrepareSession(app, window, status);
        session_snapshot = {};
        session_queries = 0;
        Expect(!ApplicationStatusTestAccess::Establish(app), "locked-startup unreadable snapshot remains unavailable");
        Expect(!ApplicationStatusTestAccess::Available(app), "failed query is not cached as established state");
        session_snapshot = {true, false, false};
        ApplicationStatusTestAccess::Notify(app, WTS_SESSION_UNLOCK);
        Expect(session_queries == 2, "unlock retries the unavailable startup snapshot");
        Expect(ApplicationStatusTestAccess::Available(app), "unlock establishes newly readable session state");
        Expect(!ApplicationStatusTestAccess::Locked(app) && !ApplicationStatusTestAccess::Disconnected(app), "recovered snapshot clears stale lock and disconnect state");
        Expect(NativeText(status) == L"Stopped: ready", "unlock retracts the stale unavailable status without relaunch");
        session_snapshot = {true, false, false};
        ApplicationStatusTestAccess::Notify(app, WTS_SESSION_LOCK);
        Expect(ApplicationStatusTestAccess::Locked(app), "fresh lock notification overrides an established snapshot");
        ApplicationStatusTestAccess::Notify(app, WTS_SESSION_UNLOCK);
        ApplicationStatusTestAccess::Notify(app, WTS_REMOTE_DISCONNECT);
        Expect(!ApplicationStatusTestAccess::Locked(app) && ApplicationStatusTestAccess::Disconnected(app), "unlock and remote disconnect preserve their own dimensions");
        ApplicationStatusTestAccess::Notify(app, WTS_REMOTE_CONNECT);
        Expect(!ApplicationStatusTestAccess::Disconnected(app) && session_queries == 2, "established state follows notifications without overwriting them by requery");
    }
    {
        Application app(instance);
        ApplicationStatusTestAccess::PrepareSession(app, window, status);
        ApplicationStatusTestAccess::SetStatus(app, L"Stopped: settings recovered; review and save before automatic start");
        session_snapshot = {true, false, false};
        ApplicationStatusTestAccess::Notify(app, WTS_SESSION_UNLOCK);
        Expect(NativeText(status) == L"Stopped: settings recovered; review and save before automatic start", "session recovery preserves unrelated stopped warnings");
    }
    {
        Application app(instance);
        ApplicationStatusTestAccess::PrepareSession(app, window, status, false);
        session_queries = 0;
        session_registrations = 0;
        session_registration_succeeds = false;
        Expect(!ApplicationStatusTestAccess::Establish(app) && session_queries == 0, "missing notification registration never claims healthy session state");
        Expect(session_registrations == 1, "unavailable registration is retried once without blocking");
        session_registration_succeeds = true;
        session_snapshot = {true, false, false};
        Expect(ApplicationStatusTestAccess::Establish(app), "transient startup registration failure recovers without process restart");
        Expect(session_queries == 1 && session_registrations == 2, "successful registration retry queries the current state");
        Expect(!ApplicationStatusTestAccess::Establish(app) && session_registrations == 2, "healthy observer is never registered twice");
    }
    {
        Application app(instance);
        ApplicationStatusTestAccess::PrepareSession(app, window, status);
        ApplicationStatusTestAccess::PrepareStartControls(app);
        session_snapshot = {};
        session_queries = 0;
        ApplicationStatusTestAccess::Start(app);
        Expect(!ApplicationStatusTestAccess::Running(app), "Start fails safely while the current session is unreadable");
        Expect(NativeText(GetDlgItem(window, kStatus)) == kSessionStateUnavailableStatus, "failed Start exposes the actual unavailable-state reason");
        session_snapshot = {true, false, false};
        ApplicationStatusTestAccess::PendingCleanup(app, true);
        ApplicationStatusTestAccess::Start(app);
        Expect(!ApplicationStatusTestAccess::Running(app) &&
                   last_dialog == L"The previous power request could not be released. Press Stop to retry cleanup first.",
               "pending power cleanup blocks Start and explains the immediate Stop recovery path");
        ApplicationStatusTestAccess::PendingCleanup(app, false);
        ApplicationStatusTestAccess::Start(app);
        Expect(session_queries == 2 && ApplicationStatusTestAccess::Running(app), "Start retries after unlock even without a notification or process restart");
        const auto recovered_status = NativeText(GetDlgItem(window, kStatus));
        Expect(recovered_status.starts_with(L"Running; no time limit") &&
                   recovered_status == ApplicationStatusTestAccess::Display(app),
               "recovered Start clears the old error and publishes the active session status");
        ApplicationStatusTestAccess::EmergencyStop(app);
        Expect(!ApplicationStatusTestAccess::Running(app) &&
                   NativeText(GetDlgItem(window, kStatus)) == L"Stopped: emergency hotkey",
               "the emergency-hotkey message stops the session and publishes its reason");
        Expect(IsWindowVisible(window) == FALSE, "recovery fixture never displays a desktop window");
    }
    {
        Application app(instance);
        ApplicationStatusTestAccess::Prepare(app, window, status, false, false);
        const int before_hide = hide_requests;
        ApplicationStatusTestAccess::Close(app, true);
        Expect(IsWindow(window) != FALSE && hide_requests == before_hide + 1,
               "Close requests tray hiding while a working tray keeps the window reachable");
        ApplicationStatusTestAccess::Close(app, false);
        Expect(IsWindow(window) == FALSE, "Close destroys the window when no tray icon is available");
    }
    DestroyWindow(window);
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
    SessionRecoveryContracts(instance);
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
    return 0;
}
