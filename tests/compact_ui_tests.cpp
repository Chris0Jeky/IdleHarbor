#include <fstream>
#include <iostream>
#include "application-under-test.inc"

namespace {
int failures = 0;
int checks = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
bool Shown(HWND control) { return (GetWindowLongPtrW(control, GWL_STYLE) & WS_VISIBLE) != 0; }

// Capture this synthetic window only, never the user's desktop or running app.
bool Capture(HWND window, const std::filesystem::path& path) {
    RECT r{}; GetWindowRect(window, &r);
    const int width = r.right - r.left, height = r.bottom - r.top;
    HDC source = GetDC(window), memory = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, width, height);
    HGDIOBJ previous = SelectObject(memory, bitmap);
    const bool printed = PrintWindow(window, memory, 0) != FALSE;
    SelectObject(memory, previous);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<char> pixels(static_cast<std::size_t>(width) * height * 4);
    const bool read = GetDIBits(memory, bitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS) != 0;
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42; file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
    DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window, source);
    return printed && read && output.good();
}

struct ApplicationStatusTestAccess {
    static void Run(bool capture) {
        Application app{GetModuleHandleW(nullptr)};
        app.settings_.session = idleharbor::core::settings_for_profile(ProfileKind::Balanced);
        app.settings_.emergency_hotkey = false;
        app.saved_settings_ = app.settings_;
        const UINT dpi = GetDpiForSystem();
        const std::wstring title = L"IdleHarbor v" + std::wstring(idleharbor::kVersion);
        const HWND window = CreateWindowExW(0, L"IdleHarbor.CompactFixture", title.c_str(),
            WS_OVERLAPPEDWINDOW, 24, 24, ScaleForDpi(600, dpi), ScaleForDpi(480, dpi),
            nullptr, nullptr, GetModuleHandleW(nullptr), &app);
        Check(window != nullptr, "real native window is created");
        if (window == nullptr) return;
        app.RefreshControls(); app.UpdateButtons();
        if (capture) { ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window); }
        Check(Shown(app.profile_) && Shown(app.power_) && Shown(app.duration_preset_), "routine choices are visible");
        Check(!Shown(app.motion_) && !Shown(app.battery_) && !Shown(app.start_minimized_), "advanced settings begin collapsed");
        Check(!Shown(app.max_duration_), "preset duration hides custom seconds");
        Check(app.ContentHeight() <= app.ViewportHeight(), "default view fits without scrolling");
        Check(ControlText(window).find(idleharbor::kVersion) != std::wstring::npos, "window displays the canonical version");
        if (capture) {
            SetFocus(app.profile_);
            MSG tab{}; tab.message = WM_KEYDOWN; tab.wParam = VK_TAB;
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.power_, "Tab reaches Keep awake");
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.duration_preset_, "Tab reaches session duration");
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.section_buttons_[0], "Tab skips collapsed custom and advanced controls");
            Check(Capture(window, L"out/compact.bmp"), "compact render is captured");
        }

        for (int index = 0; index < static_cast<int>(kDurationSeconds.size()); ++index) {
            SendMessageW(app.duration_preset_, CB_SETCURSEL, index, 0);
            app.QueueComboBoxSelection(kDurationPreset, CBN_SELCHANGE);
            Check(ControlText(app.max_duration_) == std::to_wstring(kDurationSeconds[index]), "preset updates exact seconds");
            Check(app.settings_.session.max_duration.count() == static_cast<std::int64_t>(kDurationSeconds[index]), "preset reaches actual settings");
            Check(!Shown(app.max_duration_), "preset keeps custom field collapsed");
            if (index == 1) Check(app.settings_.session.max_duration == Seconds{900}, "15 minutes converts to 900 seconds");
        }
        Check(app.settings_.session.max_duration == Seconds{14400}, "four hours converts to 14400 seconds");
        Check(app.dirty_ && IsWindowEnabled(app.save_), "duration changes enable Save");
        SendMessageW(app.duration_preset_, CB_SETCURSEL, 6, 0);
        app.QueueComboBoxSelection(kDurationPreset, CBN_SELCHANGE);
        Check(Shown(app.max_duration_), "Custom exposes exact seconds");
        SetControlText(app.max_duration_, L"123");
        app.UpdateDirtyStateFromControls();
        app.RefreshControls();
        Check(ComboIndex(app.duration_preset_) == 6 && ControlText(app.max_duration_) == L"123", "non-preset values survive refresh exactly");
        SetControlText(app.max_duration_, L"2592001");
        std::wstring error;
        Check(!app.ReadControls(error), "custom duration preserves thirty-day validation");
        SetControlText(app.max_duration_, L"123"); app.UpdateDirtyStateFromControls();
        if (capture) Check(Capture(window, L"out/custom.bmp"), "custom render is captured");

        const auto settings = app.settings_;
        for (int index = 0; index < 3; ++index) {
            SendMessageW(app.section_buttons_[index], BM_CLICK, 0, 0);
            Check(app.expanded_sections_[index] && IsChecked(app.section_buttons_[index]), "native toggle reports expanded state");
            Check(ControlText(app.section_buttons_[index]).find(L"Hide ") == 0, "expanded toggle names its action");
        }
        Check(Shown(app.motion_) && Shown(app.battery_) && Shown(app.emergency_hotkey_), "all settings remain reachable");
        Check(AppSettingsEqual(settings, app.settings_), "disclosure does not alter preferences");
        if (capture) {
            SetFocus(app.interval_);
            app.ToggleSection(0);
            Check(GetFocus() == app.section_buttons_[0], "collapse returns focus to the disclosure button");
            app.ToggleSection(0);
        }
        if (capture) Check(Capture(window, L"out/expanded.bmp"), "expanded render is captured");

        for (UINT test_dpi : {96u, 120u, 144u, 168u, 192u}) {
            RECT suggested{24, 24, 24 + ScaleForDpi(600, test_dpi), 24 + ScaleForDpi(480, test_dpi)};
            app.ApplyDpiChange(test_dpi, suggested);
            for (const auto& child : app.child_layouts_) {
                if (child.region != Application::LayoutRegion::Body || !Shown(child.window)) continue;
                Check(child.arranged_x >= 0 && child.arranged_width > 0, "visible body controls have usable bounds");
                Check(child.arranged_y + child.focus_height <= app.ContentHeight() * 96 / static_cast<int>(test_dpi) + 1,
                      "scroll extent includes every expanded control");
            }
            app.ScrollTo(app.ContentHeight());
            const int expanded_scroll = app.scroll_position_;
            RECT stop{}, status{}, viewport{};
            GetWindowRect(app.stop_, &stop); GetWindowRect(app.status_, &status); GetWindowRect(app.settings_viewport_, &viewport);
            Check(status.bottom <= viewport.top && stop.top >= viewport.bottom, "status and Stop stay outside scrolling body");
            app.ToggleSection(0); app.ToggleSection(1); app.ToggleSection(2);
            Check(app.scroll_position_ < expanded_scroll && app.scroll_position_ <=
                      idleharbor::app::MaximumScrollPosition(app.ContentHeight(), app.ViewportHeight()),
                  "collapse clamps an obsolete scroll position");
            app.ToggleSection(0); app.ToggleSection(1); app.ToggleSection(2);
        }
        app.RefreshControls();
        app.session_active_ = true; app.UpdateButtons();
        Check(IsWindowEnabled(app.stop_) && !IsWindowEnabled(app.duration_preset_), "running session keeps Stop available and locks duration");
        app.session_active_ = false; app.UpdateButtons();
        Check(AppSettingsEqual(settings, app.settings_), "layout and disclosure preserve all values");
        const auto settings_path = std::filesystem::temp_directory_path() /
            (L"IdleHarbor-compact-" + std::to_wstring(GetCurrentProcessId()) + L".ini");
        app.settings_path_ = settings_path;
        app.Save();
        Check(!app.dirty_, "Save clears dirty state after a duration edit");
        const auto saved = idleharbor::app::LoadSettings(settings_path);
        Check(saved.settings.session.max_duration == Seconds{123}, "custom seconds persist through actual save and reload");
        Check(AppSettingsEqual(settings, saved.settings), "disclosure and Save preserve every other setting");
        std::filesystem::remove(settings_path);
        if (capture) {
            app.ToggleSection(0); app.ToggleSection(1); app.ToggleSection(2);
            RECT narrow{24, 24, 24 + ScaleForDpi(420, 144), 24 + ScaleForDpi(600, 144)};
            app.ApplyDpiChange(144, narrow);
            app.ScrollTo(0);
            Check(Capture(window, L"out/narrow.bmp"), "narrow render is captured");
        }
        // This fixture never starts a session, registers hooks, or emits input.
        DestroyWindow(window);
    }
};
}

int main(int argc, char**) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW cls{}; cls.lpfnWndProc = Application::WindowProc;
    cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"IdleHarbor.CompactFixture";
    cls.hbrBackground = GetSysColorBrush(COLOR_WINDOW); cls.hIcon = LoadIdleHarborIcon(cls.hInstance);
    if (!RegisterClassW(&cls)) return 1;
    ApplicationStatusTestAccess::Run(argc > 1);
    std::cout << "Compact UI: " << checks << " checks, " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}
