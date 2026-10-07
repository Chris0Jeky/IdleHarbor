#include <windows.h>
#include <shellapi.h>
#include "idleharbor/platform/windows/motion_emitter.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
int checks = 0, failures = 0, hook_attempts = 0, emitted_pulses = 0;
ULONGLONG ticks = 100000;
bool release_fails = false;
EXECUTION_STATE current_power = ES_CONTINUOUS;
std::wstring dialog, selected_caption;
HWND dropped_combo = nullptr;
bool early_closeup = false;
std::vector<std::wstring> menu_captions;
void Check(bool value, std::string_view description) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
std::wstring Caption(HMENU menu, int index) {
    std::array<wchar_t,512> text{};
    GetMenuStringW(menu, static_cast<UINT>(index), text.data(), static_cast<int>(text.size()), MF_BYPOSITION);
    std::wstring result(text.data());
    result.erase(std::remove(result.begin(),result.end(),L'&'),result.end());
    return result;
}
UINT FindAction(HMENU menu, bool parent_enabled=true) {
    UINT selected=0;
    for (int i=0;i<GetMenuItemCount(menu);++i) {
        const auto caption=Caption(menu,i); menu_captions.push_back(caption);
        const UINT state=GetMenuState(menu,static_cast<UINT>(i),MF_BYPOSITION);
        const bool enabled=parent_enabled && (state & (MF_DISABLED|MF_GRAYED))==0;
        if (const HMENU child=GetSubMenu(menu,i);child!=nullptr) {
            const auto candidate=FindAction(child,enabled); if(candidate) selected=candidate;
        } else if(caption==selected_caption && enabled) selected=GetMenuItemID(menu,i);
    }
    return selected;
}
}
BOOL WINAPI TestNotifyIcon(DWORD,PNOTIFYICONDATAW) { return TRUE; }
int WINAPI TestMessageBox(HWND,LPCWSTR text,LPCWSTR,UINT) { dialog=text;return IDOK; }
BOOL WINAPI TestShowWindow(HWND,int) { return TRUE; }
BOOL WINAPI TestForeground(HWND) { return TRUE; }
ULONGLONG WINAPI TestTicks() { return ticks; }
UINT_PTR WINAPI TestSetTimer(HWND,UINT_PTR id,UINT,TIMERPROC) { return id; }
BOOL WINAPI TestKillTimer(HWND,UINT_PTR) { return TRUE; }
EXECUTION_STATE WINAPI TestExecutionState(EXECUTION_STATE flags) {
    if (flags==ES_CONTINUOUS && release_fails) return 0;
    const auto previous=current_power; current_power=flags;return previous;
}
HHOOK WINAPI TestInstallHook(int,HOOKPROC,HINSTANCE,DWORD) { ++hook_attempts;return nullptr; }
BOOL WINAPI TestTrackPopupMenu(HMENU menu,UINT,int,int,int,HWND,const RECT*) {
    menu_captions.clear();const auto command=FindAction(menu);
    if(!selected_caption.empty()) Check(command!=0,"requested tray action exists and is enabled");
    return static_cast<BOOL>(command);
}
LRESULT WINAPI TestSendMessage(HWND target,UINT message,WPARAM w,LPARAM l) {
    if(message==CB_GETDROPPEDSTATE && target==dropped_combo) return TRUE;
    if(message==CB_SETCURSEL && target==dropped_combo) Check(false,"refresh must not overwrite a pending dropdown selection");
    if(message==CB_SHOWDROPDOWN && w==FALSE && target==dropped_combo) {
        const HWND combo=dropped_combo;
        if(!early_closeup) dropped_combo=nullptr;
        SendMessageW(GetParent(combo),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(combo),CBN_CLOSEUP),reinterpret_cast<LPARAM>(combo));
        dropped_combo=nullptr;
        return TRUE;
    }
    return SendMessageW(target,message,w,l);
}
namespace idleharbor::platform::windows {
MotionEmissionResult TestZenPulse() noexcept { ++emitted_pulses; return {.succeeded=true}; }
MotionEmissionResult TestMotionPulse(std::span<const POINT>) noexcept { ++emitted_pulses; return {.succeeded=true}; }
}
#define Shell_NotifyIconW TestNotifyIcon
#define MessageBoxW TestMessageBox
#define ShowWindow TestShowWindow
#define SetForegroundWindow TestForeground
#define GetTickCount64 TestTicks
#define SetTimer TestSetTimer
#define KillTimer TestKillTimer
#define TrackPopupMenu TestTrackPopupMenu
#define SendMessageW TestSendMessage
#define EmitZenPulse TestZenPulse
#define EmitMotionPulse TestMotionPulse
#include "application-session-under-test.inc"
#undef EmitMotionPulse
#undef EmitZenPulse
#undef SendMessageW
#undef TrackPopupMenu
#undef KillTimer
#undef SetTimer
#undef GetTickCount64
#undef SetForegroundWindow
#undef ShowWindow
#undef MessageBoxW
#undef Shell_NotifyIconW
// Resolve only the test executable's platform objects through these boundaries.
#define SetThreadExecutionState TestExecutionState
#include "../src/platform/windows/power_request.cpp"
#undef SetThreadExecutionState
#define SetWindowsHookExW TestInstallHook
#include "../src/platform/windows/input_monitor.cpp"
#undef SetWindowsHookExW

namespace {
struct ApplicationSessionTestAccess {
    static void Prepare(Application& app) {
        app.settings_.session.motion=MotionMode::Off;
        app.settings_.session.power=PowerMode::System;
        app.settings_.session.pause_on_user_activity=false;
        app.settings_.session.pause_when_locked=true;
        app.settings_.session.pause_when_disconnected=true;
        app.settings_.session.pause_on_low_battery=false;
        app.settings_.session.max_duration=Seconds{180};
        app.settings_.emergency_hotkey=false;
        app.settings_.show_notifications=false;
        app.saved_settings_=app.settings_;
        app.session_notifications_available_=true;
        app.session_state_available_=true;
    }
    static void Ready(Application& app) { app.RefreshControls();app.InitializeTrayIcon(); }
    static HWND Status(const Application& app) { return app.status_; }
    static HWND Interval(const Application& app) { return app.interval_; }
    static HWND Profile(const Application& app) { return app.profile_; }
    static bool Active(const Application& app) { return app.session_active_; }
    static bool Dirty(const Application& app) { return app.dirty_; }
    static bool Manual(const Application& app) { return app.policy_ && app.policy_->manually_paused(); }
    static std::optional<Seconds> Remaining(const Application& app) { return app.policy_ ? app.policy_->remaining_duration(NowSeconds()) : std::optional<Seconds>{Seconds{0}}; }
    static Seconds DefaultDuration(const Application& app) { return app.settings_.session.max_duration; }
    static bool DefaultsUnchanged(const Application& app) { return AppSettingsEqual(app.settings_,app.saved_settings_); }
    static void Locked(Application& app,bool value) { app.locked_=value; }
    static void Tick(Application& app) { app.HandleMessage(WM_TIMER,kTimerId,0); }
    static void Menu(Application& app) { app.ShowTrayMenu(); }
    static void QueueProfile(Application& app) { app.QueueComboBoxSelection(kProfile,CBN_SELCHANGE); }
    static ProfileKind ProfileValue(const Application& app) { return app.settings_.session.profile; }
    static HWND StopButton(const Application& app) { return app.stop_; }
    static HWND StartButton(const Application& app) { return app.start_; }
};
std::wstring Text(HWND window) { std::array<wchar_t,2048> t{};GetWindowTextW(window,t.data(),static_cast<int>(t.size()));return t.data(); }
bool Invoke(Application& app,std::vector<std::wstring_view> arguments) {
    const auto parsed=idleharbor::app::ParseCommandLine(arguments);
    Check(parsed.ok(),"session action is available through the real CLI parser");
    if(!parsed.ok()) return false;
    dialog.clear();app.HandleCommand(parsed.options);return true;
}
struct Fixture {
    Application app{GetModuleHandleW(nullptr)};
    HWND window=nullptr;
    Fixture() {
        dropped_combo=nullptr;release_fails=false;ticks=100000;selected_caption.clear();dialog.clear();
        ApplicationSessionTestAccess::Prepare(app);
        window=CreateWindowExW(0,L"IdleHarbor.SessionFixture",L"",WS_OVERLAPPEDWINDOW,0,0,600,700,nullptr,nullptr,GetModuleHandleW(nullptr),&app);
        Check(window!=nullptr,"hidden real application fixture is created");
        if(window) ApplicationSessionTestAccess::Ready(app);
    }
    ~Fixture() {
        dropped_combo=nullptr;release_fails=false;
        if(window) { Check(IsWindowVisible(window)==FALSE,"fixture never exposes a desktop window");DestroyWindow(window); }
    }
};
void TimerAndPause() {
    Fixture f;if(!f.window) return;auto& app=f.app;
    if(!Invoke(app,{L"--start-for",L"30m"}) || !ApplicationSessionTestAccess::Active(app)) return;
    Check(ApplicationSessionTestAccess::Remaining(app)==Seconds{1800},"quick start uses a session-only duration");
    Check(ApplicationSessionTestAccess::DefaultDuration(app)==Seconds{180},"quick start leaves the default duration untouched");
    Check(ApplicationSessionTestAccess::DefaultsUnchanged(app) && !ApplicationSessionTestAccess::Dirty(app),"quick start does not create unsaved preference edits");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"30:00 remaining")!=std::wstring::npos,"status exposes the actual countdown");
    ticks+=61000;ApplicationSessionTestAccess::Tick(app);
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"28:59 remaining")!=std::wstring::npos,"timer tick updates the countdown");
    Invoke(app,{L"--pause"});
    Check(ApplicationSessionTestAccess::Manual(app),"pause keeps a resumable session");
    Check(current_power==ES_CONTINUOUS,"pause releases the power request");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"manual pause")!=std::wstring::npos,"manual pause is visible");
    ApplicationSessionTestAccess::Locked(app,true);Invoke(app,{L"--resume"});
    Check(!ApplicationSessionTestAccess::Manual(app),"Resume clears the manual hold");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"workstation locked")!=std::wstring::npos,"Resume cannot override a safeguard");
    Check(current_power==ES_CONTINUOUS,"safeguard pause keeps power released");
    ApplicationSessionTestAccess::Locked(app,false);ApplicationSessionTestAccess::Tick(app);
    Check(current_power!=(ES_CONTINUOUS),"cleared safeguard restores the configured request");
    Invoke(app,{L"--snooze",L"5m"});const auto before=ApplicationSessionTestAccess::Remaining(app);
    ticks+=299000;ApplicationSessionTestAccess::Tick(app);
    Check(ApplicationSessionTestAccess::Manual(app),"snooze lasts until its boundary");
    ticks+=1000;ApplicationSessionTestAccess::Tick(app);
    Check(!ApplicationSessionTestAccess::Manual(app),"snooze expires through the ordinary timer");
    Check(ApplicationSessionTestAccess::Remaining(app)==*before-Seconds{300},"snooze does not freeze session expiry");
    const auto budget=ApplicationSessionTestAccess::Remaining(app);
    Invoke(app,{L"--extend",L"15m"});
    Check(ApplicationSessionTestAccess::Remaining(app)==*budget+Seconds{900},"extension adds time without restarting the session");
    Check(ApplicationSessionTestAccess::DefaultsUnchanged(app),"extension does not change defaults");
    Invoke(app,{L"--start-for",L"1h"});
    Check(!dialog.empty(),"new quick start cannot silently replace an active session");
    Check(ApplicationSessionTestAccess::Remaining(app)==*budget+Seconds{900},"refused quick start leaves current deadline intact");
    Invoke(app,{L"--stop"});Check(!ApplicationSessionTestAccess::Active(app),"Stop is immediate");
    Invoke(app,{L"--resume"});Check(!ApplicationSessionTestAccess::Active(app),"Resume cannot restart Stop");
    Invoke(app,{L"--start"});Check(ApplicationSessionTestAccess::Remaining(app)==Seconds{180},"ordinary Start returns to the saved default duration");
}
void ExpiryAndCleanup() {
    Fixture f;if(!f.window) return;auto& app=f.app;
    if(!Invoke(app,{L"--start-for",L"10s"}) || !ApplicationSessionTestAccess::Active(app)) return;
    Invoke(app,{L"--snooze",L"5m"});ticks+=10000;ApplicationSessionTestAccess::Tick(app);
    Check(!ApplicationSessionTestAccess::Active(app),"session expiry wins over snooze");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"maximum duration reached")!=std::wstring::npos,"expiry is explained");
    Invoke(app,{L"--extend",L"1h"});Check(!ApplicationSessionTestAccess::Active(app),"extension cannot revive expiry");
    Invoke(app,{L"--start-for",L"1m"});release_fails=true;
    Invoke(app,{L"--pause"});
    Check(!ApplicationSessionTestAccess::Active(app),"failed pause cleanup stops automatic activity");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"power request release failed")!=std::wstring::npos,"failed cleanup is visible, not claimed successful");
    Check(IsWindowEnabled(ApplicationSessionTestAccess::StopButton(app))!=FALSE,"Stop remains enabled to retry pending cleanup");
    Check(IsWindowEnabled(ApplicationSessionTestAccess::StartButton(app))==FALSE,"new session cannot obscure pending cleanup");
    Invoke(app,{L"--start"});Check(!ApplicationSessionTestAccess::Active(app),"Start also refuses pending cleanup programmatically");
    release_fails=false;Invoke(app,{L"--stop"});
    Check(current_power==ES_CONTINUOUS,"explicit Stop retries and clears the real power wrapper state");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"power request release failed")==std::wstring::npos,"successful retry retracts cleanup warning");
    Check(IsWindowEnabled(ApplicationSessionTestAccess::StartButton(app))!=FALSE,"successful cleanup permits another Start");
}
void CleanupOnStop() {
    Fixture f;if(!f.window) return;auto& app=f.app;
    Invoke(app,{L"--start"});release_fails=true;Invoke(app,{L"--stop"});
    Check(!ApplicationSessionTestAccess::Active(app),"failed release never keeps a session running");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"power request release failed")!=std::wstring::npos,"Stop reports failed power cleanup");
    Check(IsWindowEnabled(ApplicationSessionTestAccess::StopButton(app))!=FALSE,"failed Stop leaves a retry action");
    release_fails=false;Invoke(app,{L"--stop"});
    Check(current_power==ES_CONTINUOUS,"Stop retries outstanding release");
    Check(Text(ApplicationSessionTestAccess::Status(app)).find(L"power request release failed")==std::wstring::npos,"retry success clears the pending warning");
}
void PreserveEdits() {
    for(bool early : {false,true}) {
        Fixture f;if(!f.window) return;auto& app=f.app;
        const HWND profile=ApplicationSessionTestAccess::Profile(app);
        SendMessageW(profile,CB_SETCURSEL,1,0);dropped_combo=profile;early_closeup=early;
        ApplicationSessionTestAccess::QueueProfile(app);
        Invoke(app,{L"--status"});
        Check(ApplicationSessionTestAccess::ProfileValue(app)==ProfileKind::LongTask,"informational command commits rather than discards a pending profile selection");
        Check(dropped_combo==nullptr,"pending dropdown is closed before command dispatch");
        Check(ApplicationSessionTestAccess::Dirty(app),"committed profile stays visibly unsaved");
    }
    Fixture f;if(!f.window) return;auto& app=f.app;
    SetWindowTextW(ApplicationSessionTestAccess::Interval(app),L"bad");
    Check(ApplicationSessionTestAccess::Dirty(app),"invalid edit is visibly unsaved");
    Invoke(app,{L"--status"});
    Check(Text(ApplicationSessionTestAccess::Interval(app))==L"bad","status command preserves an invalid in-progress edit");
    Check(ApplicationSessionTestAccess::Dirty(app),"status command preserves invalid dirty state");
}
void TrayChoices() {
    Fixture f;if(!f.window) return;auto& app=f.app;
    selected_caption=L"30 minutes";ApplicationSessionTestAccess::Menu(app);
    Check(ApplicationSessionTestAccess::Active(app),"native quick-start menu reaches the real start handler");
    Check(ApplicationSessionTestAccess::Remaining(app)==Seconds{1800},"native quick-start selection uses its own duration");
    if(!ApplicationSessionTestAccess::Active(app)) return;
    selected_caption=L"Pause";ApplicationSessionTestAccess::Menu(app);
    Check(ApplicationSessionTestAccess::Manual(app),"native Pause menu reaches policy control");
    selected_caption=L"Resume";ApplicationSessionTestAccess::Menu(app);
    Check(!ApplicationSessionTestAccess::Manual(app),"native Resume menu reaches policy control");
    selected_caption=L"5 minutes";ApplicationSessionTestAccess::Menu(app);
    Check(ApplicationSessionTestAccess::Manual(app),"native snooze menu reaches policy control");
    const auto remaining=ApplicationSessionTestAccess::Remaining(app);
    selected_caption=L"Add 15 minutes";ApplicationSessionTestAccess::Menu(app);
    Check(ApplicationSessionTestAccess::Remaining(app)==*remaining+Seconds{900},"native extension menu keeps the elapsed clock");
    Check(ApplicationSessionTestAccess::Manual(app),"native extension does not resume a paused session");
    selected_caption=L"Stop";ApplicationSessionTestAccess::Menu(app);
    Check(!ApplicationSessionTestAccess::Active(app),"native Stop remains reachable");
    Check(ApplicationSessionTestAccess::DefaultsUnchanged(app),"all native session actions preserve preferences");
}
}
int main() {
    WNDCLASSW cls{};cls.lpfnWndProc=Application::WindowProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"IdleHarbor.SessionFixture";
    if(!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 1;
    TimerAndPause();ExpiryAndCleanup();CleanupOnStop();PreserveEdits();TrayChoices();
    Check(hook_attempts==0,"test never requests a live input observer");
    Check(emitted_pulses==0,"test never requests pointer emission");
    Check(current_power==ES_CONTINUOUS,"all test power requests are released");
    if(failures) { std::cerr<<failures<<" of "<<checks<<" native session-toolkit checks failed.\n";return 1; }
    std::cout<<"Native session toolkit passed ("<<checks<<" checks; no live input, power, tray or visible window).\n";
}
