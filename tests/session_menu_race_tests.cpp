#include <windows.h>
#include <functional>
#include <utility>

namespace {
std::function<void()> menu_interruption;
}

UINT WINAPI InterleavedMenuItemId(HMENU menu, int position) {
    const UINT command = GetMenuItemID(menu, position);
    if (menu_interruption) {
        auto interrupt = std::move(menu_interruption);
        menu_interruption = {};
        interrupt();
    }
    return command;
}

// Reuse the proven actual-Application fixture without copying its implementation.
// GetMenuItemID is reached by its popup double after selecting an old menu item,
// before that ID returns to production ShowTrayMenu. Only this target interleaves.
#define GetMenuItemID InterleavedMenuItemId
#define main OriginalSessionToolkitMain
#include "session_toolkit_tests.cpp"
#undef main
#undef GetMenuItemID

namespace {
void StaleTrayActions() {
    for (const auto* action : {L"Stop", L"Pause", L"5 minutes", L"Add 15 minutes", L"Resume"}) {
        Fixture f;
        if (!f.window) return;
        auto& app = f.app;
        Invoke(app, {L"--start-for", L"1s"});
        const bool resume_action = std::wstring_view(action) == L"Resume";
        if (resume_action) Invoke(app, {L"--pause"});
        selected_caption = action;
        menu_interruption = [&] {
            ticks += 1000;
            ApplicationSessionTestAccess::Tick(app);
            Check(!ApplicationSessionTestAccess::Active(app), "old menu's session expires inside the nested loop");
            Invoke(app, {L"--start-for", L"20m"});
            if (resume_action) Invoke(app, {L"--pause"});
        };
        ApplicationSessionTestAccess::Menu(app);
        Check(ApplicationSessionTestAccess::Active(app), "stale tray action cannot stop the replacement session");
        Check(ApplicationSessionTestAccess::Remaining(app) == Seconds{1200}, "stale tray action cannot extend the replacement deadline");
        Check(ApplicationSessionTestAccess::Manual(app) == resume_action, "stale tray action cannot pause or resume the replacement session");
        Check(dialog.find(L"session changed") != std::wstring::npos, "stale session action explains why it was discarded");
    }
    {
        Fixture f;
        if (!f.window) return;
        auto& app = f.app;
        Invoke(app, {L"--start-for", L"20m"});
        selected_caption = L"Add 15 minutes";
        menu_interruption = [&] {
            // Exact same settings and clock value, including likely allocator
            // address reuse: pointer or timestamp comparison is not identity.
            Invoke(app, {L"--stop"});
            Invoke(app, {L"--start-for", L"20m"});
        };
        ApplicationSessionTestAccess::Menu(app);
        Check(ApplicationSessionTestAccess::Remaining(app) == Seconds{1200}, "same-tick replacement is still a distinct session");
    }
    {
        Fixture f;
        if (!f.window) return;
        auto& app = f.app;
        selected_caption = L"30 minutes";
        menu_interruption = [&] {
            Invoke(app, {L"--start-for", L"1s"});
            ticks += 1000;
            ApplicationSessionTestAccess::Tick(app);
        };
        ApplicationSessionTestAccess::Menu(app);
        Check(!ApplicationSessionTestAccess::Active(app), "old stopped-state menu cannot start after an intervening session");
        Check(dialog.find(L"session changed") != std::wstring::npos, "stale quick start requests a fresh menu selection");
    }
    {
        Fixture f;
        if (!f.window) return;
        auto& app = f.app;
        Invoke(app, {L"--start-for", L"1s"});
        selected_caption = L"Show";
        menu_interruption = [&] {
            ticks += 1000;
            ApplicationSessionTestAccess::Tick(app);
            Invoke(app, {L"--start-for", L"20m"});
        };
        ApplicationSessionTestAccess::Menu(app);
        Check(ApplicationSessionTestAccess::Active(app) && dialog.empty(), "Show remains available across a session replacement");
    }
}
}

int main() {
    WNDCLASSW cls{};
    cls.lpfnWndProc = Application::WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"IdleHarbor.SessionFixture";
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;
    StaleTrayActions();
    Check(hook_attempts == 0 && emitted_pulses == 0, "interleavings do not request live input");
    Check(current_power == ES_CONTINUOUS, "interleavings leave no test power request");
    if (failures) {
        std::cerr << failures << " of " << checks << " menu-session interleaving checks failed.\n";
        return 1;
    }
    std::cout << "Menu-session interleavings passed (" << checks << " checks).\n";
}
