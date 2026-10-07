#include "idleharbor/platform/windows/input_monitor.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace {
using idleharbor::platform::windows::InputMonitor;
using idleharbor::platform::windows::kIdleHarborInputMarker;
struct Hook { int kind; HOOKPROC callback; };
std::map<HHOOK, Hook> hooks;
std::set<HHOOK> refuse_release;
std::set<int> refuse_install;
std::vector<HHOOK> release_calls;
std::vector<HWND> notifications;
std::uintptr_t sequence = 1;
std::size_t installs = 0;
DWORD last_error = 0;
bool post_succeeds = true;
bool refuse_all_releases = false;
int failures = 0;
int cases = 0;
const auto window = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(101));
const auto other_window = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(102));
constexpr UINT message = 0x8002;
void Check(bool passed, std::string_view label) {
    if (!passed) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
void Reset() {
    hooks.clear(); refuse_release.clear(); refuse_install.clear(); release_calls.clear();
    notifications.clear(); installs = 0; post_succeeds = true; refuse_all_releases = false; last_error = 0; ++cases;
}
HHOOK Find(int kind) {
    for (const auto& [handle, hook] : hooks) if (hook.kind == kind) return handle;
    return nullptr;
}
std::size_t Attempts(HHOOK handle) {
    return static_cast<std::size_t>(std::count(release_calls.begin(), release_calls.end(), handle));
}
void Emit(HOOKPROC callback, int kind, DWORD flags = 0, ULONG_PTR extra = 0, int code = HC_ACTION) {
    const MSLLHOOKSTRUCT mouse{flags, extra};
    const KBDLLHOOKSTRUCT keyboard{flags, extra};
    const auto data = kind == WH_MOUSE_LL ? reinterpret_cast<LPARAM>(&mouse) : reinterpret_cast<LPARAM>(&keyboard);
    Check(callback(code, 0, data) == 73, "hook always forwards the next-hook result");
}
void StopRetry(int kind) {
    Reset();
    InputMonitor monitor;
    Check(monitor.Start(window, message).any(), "observer starts before stop failure");
    const auto retained = Find(kind);
    const auto callback = hooks.at(retained).callback;
    refuse_release.insert(retained);
    monitor.Stop();
    Check(!monitor.capabilities().any(), "stopped cleanup residue is not observer availability");
    Emit(callback, kind);
    Check(notifications.empty(), "stopped hooks cannot notify the window");
    monitor.Stop();
    Check(Attempts(retained) == 2, "Stop retries the same failed hook");
    const auto before = installs;
    Check(!monitor.Start(window, message).any(), "restart refuses unresolved cleanup");
    Check(installs == before, "restart does not accumulate hooks during failed cleanup");
    {
        InputMonitor competitor;
        Check(!competitor.Start(other_window, message).any(), "pending cleanup keeps exclusive ownership");
        Check(installs == before, "competing Start does not install around pending cleanup");
    }
    refuse_release.clear();
    monitor.Stop();
    Check(hooks.empty(), "Stop releases the retained hook after recovery");
    Check(monitor.Start(window, message).any(), "restart succeeds after cleanup recovery");
    monitor.Stop();
    Check(hooks.empty(), "restarted observer releases both hooks");
}
}

HINSTANCE GetModuleHandleW(const wchar_t*) { return nullptr; }
HHOOK SetWindowsHookExW(int kind, HOOKPROC callback, HINSTANCE, DWORD) {
    ++installs;
    if (refuse_install.contains(kind)) return nullptr;
    const auto handle = reinterpret_cast<HHOOK>(++sequence);
    hooks.emplace(handle, Hook{kind, callback});
    return handle;
}
BOOL UnhookWindowsHookEx(HHOOK handle) {
    release_calls.push_back(handle);
    if (refuse_all_releases || refuse_release.contains(handle)) { last_error = 5; return FALSE; }
    if (hooks.erase(handle) == 0) { last_error = ERROR_INVALID_HOOK_HANDLE; return FALSE; }
    return TRUE;
}
DWORD GetLastError() { return last_error; }
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 73; }
BOOL PostMessageW(HWND target, UINT msg, WPARAM, LPARAM) {
    Check(msg == message, "notifications retain their registered message");
    if (!post_succeeds) return FALSE;
    notifications.push_back(target);
    return TRUE;
}

int main() {
    Reset();
    {
        InputMonitor monitor;
        Check(!monitor.Start(nullptr, message).any(), "null target is rejected");
        Check(!monitor.Start(window, 0).any(), "zero message is rejected");
        Check(installs == 0, "invalid targets install no hooks");
        Check(!monitor.Refresh().any(), "unstarted refresh remains unavailable");
        Check(monitor.Start(window, message).any(), "invalid target does not strand ownership");
        monitor.Stop();
    }

    Reset();
    {
        InputMonitor monitor;
        const auto capabilities = monitor.Start(window, message);
        Check(capabilities.mouse && capabilities.keyboard, "both observers are installed");
        for (const int kind : {WH_MOUSE_LL, WH_KEYBOARD_LL}) {
            const auto callback = hooks.at(Find(kind)).callback;
            Emit(callback, kind, kind == WH_MOUSE_LL ? LLMHF_INJECTED : LLKHF_INJECTED);
            Emit(callback, kind, 0, kIdleHarborInputMarker);
            Emit(callback, kind, 0, 0, -1);
        }
        Check(notifications.empty(), "injected, tagged and non-action events are ignored");
        const auto mouse = hooks.at(Find(WH_MOUSE_LL)).callback;
        const auto keyboard = hooks.at(Find(WH_KEYBOARD_LL)).callback;
        Emit(mouse, WH_MOUSE_LL); Emit(keyboard, WH_KEYBOARD_LL);
        Check(notifications == std::vector<HWND>{window}, "real input notifications are coalesced");
        monitor.AcknowledgeNotification();
        post_succeeds = false; Emit(mouse, WH_MOUSE_LL);
        post_succeeds = true; Emit(keyboard, WH_KEYBOARD_LL);
        Check(notifications.size() == 2, "failed PostMessage rearms notification delivery");
        InputMonitor competitor;
        Check(!competitor.Start(other_window, message).any(), "second monitor cannot steal ownership");
        competitor.Stop();
        Check(monitor.capabilities().any() && hooks.size() == 2, "rejected owner cannot stop the first monitor");
    }
    Check(hooks.empty(), "healthy destruction releases all hooks");

    StopRetry(WH_MOUSE_LL);
    StopRetry(WH_KEYBOARD_LL);

    Reset();
    {
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before refresh");
        const auto old_mouse = Find(WH_MOUSE_LL);
        const auto old_keyboard = Find(WH_KEYBOARD_LL);
        Check(monitor.Refresh().any(), "healthy refresh remains available");
        Check(!hooks.contains(old_mouse) && !hooks.contains(old_keyboard) && hooks.size() == 2,
              "refresh replaces both old hooks without leaking handles");
        monitor.Stop();
        const auto before = installs;
        Check(!monitor.Refresh().any() && installs == before, "stopped refresh installs no hooks");
    }

    for (const int kind : {WH_MOUSE_LL, WH_KEYBOARD_LL}) {
        Reset();
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before refresh retirement failure");
        const auto retained = Find(kind);
        const auto callback = hooks.at(retained).callback;
        refuse_release.insert(retained);
        Check(!monitor.Refresh().any(), "failed retirement makes refresh unavailable");
        Check(!monitor.capabilities().any(), "retirement failure does not claim healthy capabilities");
        const auto before = installs;
        for (int attempt = 0; attempt < 5; ++attempt) {
            Check(!monitor.Refresh().any(), "unresolved retirement remains unavailable");
        }
        Check(installs == before, "repeated refresh failures do not grow installed hooks");
        Emit(callback, kind);
        Check(notifications.empty(), "failed refresh quiesces stale callbacks");
        refuse_release.clear();
        monitor.Stop();
        Check(hooks.empty(), "refresh retirement remains recoverable by Stop");
    }

    for (const int failed_kind : {WH_MOUSE_LL, WH_KEYBOARD_LL}) {
        Reset();
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before partial refresh");
        const auto old_hook = Find(failed_kind);
        refuse_install.insert(failed_kind);
        // Refuse releases of old and newly-created hooks of the other kind by
        // predicting the next unique handle, so both ownership classes are tested.
        refuse_release.insert(old_hook);
        refuse_release.insert(reinterpret_cast<HHOOK>(sequence + 1));
        Check(!monitor.Refresh().any(), "partial replacement never claims a complete observer");
        Check(!monitor.capabilities().any(), "partial failure quiesces old hooks too");
        refuse_install.clear(); refuse_release.clear();
        monitor.Stop();
        Check(hooks.empty(), "old and partial replacement handles survive for retry");
    }

    Reset();
    {
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before OS-side removal");
        hooks.clear();
        Check(monitor.Refresh().any(), "known-invalid old handles do not block watchdog replacement");
        Check(hooks.size() == 2, "watchdog owns only its new hooks");
    }

    Reset();
    {
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before total installation failure");
        refuse_install = {WH_MOUSE_LL, WH_KEYBOARD_LL};
        refuse_all_releases = true;
        Check(!monitor.Refresh().any(), "total installation failure quiesces the observer");
        Check(!monitor.capabilities().any(), "total failure never reports stale capabilities");
        refuse_all_releases = false; refuse_install.clear();
        monitor.Stop();
        Check(hooks.empty(), "both old handles remain retryable after total failure");
    }

    Reset();
    {
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before four-handle cleanup failure");
        refuse_all_releases = true;
        Check(!monitor.Refresh().any(), "failed retirement of both old hooks stops refresh");
        Check(hooks.size() == 4, "cleanup backlog never exceeds one current and replacement pair");
        Check(!monitor.Start(window, message).any(), "same owner cannot allocate around four pending hooks");
        Check(hooks.size() == 4, "repeated start keeps the backlog bounded");
    }
    {
        InputMonitor next;
        Check(!next.Start(window, message).any(), "all four destructor handles remain quarantined");
        Check(hooks.size() == 4, "quarantine cannot grow across owner lifetimes");
        refuse_all_releases = false;
        Check(next.Start(window, message).any(), "all four quarantine handles can be drained");
        Check(hooks.size() == 2, "only the new observer survives quarantine cleanup");
    }
    Check(hooks.empty(), "four-handle quarantine recovery leaves no residue");

    Reset();
    HOOKPROC stale_callback = nullptr;
    {
        InputMonitor monitor;
        Check(monitor.Start(window, message).any(), "observer starts before destruction failure");
        const auto retained = Find(WH_MOUSE_LL);
        stale_callback = hooks.at(retained).callback;
        refuse_release.insert(retained);
    }
    Emit(stale_callback, WH_MOUSE_LL);
    Check(notifications.empty(), "failed destructor cleanup cannot reach a destroyed owner");
    {
        InputMonitor next;
        const auto before = installs;
        Check(!next.Start(other_window, message).any(), "new owner refuses quarantined destructor residue");
        Check(installs == before, "quarantine does not allocate replacement hooks");
        refuse_release.clear();
        Check(next.Start(other_window, message).any(), "new owner retries and clears destructor residue");
        Check(hooks.size() == 2, "recovered owner has exactly two hooks");
        next.Stop();
    }
    Check(hooks.empty(), "all hooks are released after quarantine recovery");
    if (failures) { std::cerr << failures << " assertions failed across " << cases << " scenarios.\n"; return 1; }
    std::cout << "Input monitor lifecycle passed (" << cases << " scenarios).\n";
}
