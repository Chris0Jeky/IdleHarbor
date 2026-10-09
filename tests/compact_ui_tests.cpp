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
    static COLORREF PaintPixel(HWND control, int x, int y) {
        RECT bounds{}; GetClientRect(control, &bounds);
        HDC source = GetDC(control), memory = CreateCompatibleDC(source);
        HBITMAP bitmap = CreateCompatibleBitmap(source, bounds.right, bounds.bottom);
        const auto previous = SelectObject(memory, bitmap);
        SendMessageW(control, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
        const COLORREF color = GetPixel(memory, x, y);
        SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(control, source);
        return color;
    }

    static void Run(bool capture) {
        Application app{GetModuleHandleW(nullptr)};
        app.settings_.session = idleharbor::core::settings_for_profile(ProfileKind::Balanced);
        app.settings_.emergency_hotkey = false;
        app.settings_.dark_appearance = false;
        app.saved_settings_ = app.settings_;
        const UINT dpi = GetDpiForSystem();
        const std::wstring title = L"IdleHarbor v" + std::wstring(idleharbor::kVersion);
        const HWND window = CreateWindowExW(0, L"IdleHarbor.CompactFixture", title.c_str(),
            WS_OVERLAPPEDWINDOW, 24, 24, ScaleForDpi(600, dpi), ScaleForDpi(480, dpi),
            nullptr, nullptr, GetModuleHandleW(nullptr), &app);
        Check(window != nullptr, "real native window is created");
        if (window == nullptr) return;
        const auto capture_window = [&](const wchar_t* path) {
            // PrintWindow cannot composite DWM material; preview the solid fallback.
            const bool previous_backdrop = app.backdrop_enabled_;
            app.backdrop_enabled_ = false;
            const bool result = Capture(window, path);
            app.backdrop_enabled_ = previous_backdrop;
            return result;
        };
        app.RefreshControls(); app.UpdateButtons();
        if (capture) { ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window); }
        Check(Shown(app.profile_) && Shown(app.power_) && Shown(app.duration_preset_), "routine choices are visible");
        Check(!Shown(app.motion_) && !Shown(app.battery_) && !Shown(app.start_minimized_), "advanced settings begin collapsed");
        Check(!Shown(app.max_duration_), "preset duration hides custom seconds");
        Check(app.ContentHeight() <= app.ViewportHeight(), "default view fits without scrolling");
        Check(ControlText(window).find(idleharbor::kVersion) != std::wstring::npos, "window displays the canonical version");
        if (!app.high_contrast_) {
            // Sample clear interior pixels, away from text, borders and chevrons.
            const COLORREF primary = PaintPixel(app.start_, app.Scale(10), app.Scale(10));
            Check(primary == RGB(0, 91, 211) || primary == RGB(0, 78, 186), "native Start renders the primary accent or hover state");
            const COLORREF field = PaintPixel(app.duration_preset_, app.Scale(10), app.Scale(10));
            Check(field == RGB(255, 255, 255) || field == RGB(240, 242, 246), "native duration renders the light field or hover surface");
            const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (int paint = 0; paint < 100; ++paint) PaintPixel(app.start_, app.Scale(10), app.Scale(10));
            Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before + 1, "repeated control painting releases GDI resources");
            EnableWindow(app.start_, FALSE);
            Check(PaintPixel(app.start_, app.Scale(10), app.Scale(10)) == RGB(237, 239, 243), "disabled primary action is visibly muted");
            EnableWindow(app.start_, TRUE);
            const auto before_theme = app.settings_.session;
            SetChecked(app.dark_appearance_, true);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
            Check(app.settings_.dark_appearance && app.dirty_, "dark appearance updates live and enables Save");
            const COLORREF dark_field = PaintPixel(app.duration_preset_, app.Scale(10), app.Scale(10));
            Check(dark_field == RGB(37, 41, 50) || dark_field == RGB(45, 50, 61), "native duration renders the dark field or hover surface");
            Check(SettingsEqual(before_theme, app.settings_.session), "theme changes preserve session settings");
            if (capture) {
                Check(capture_window(L"out/dark.bmp"), "dark compact appearance is captured");
                SendMessageW(app.duration_preset_, CB_SHOWDROPDOWN, TRUE, 0);
                COMBOBOXINFO info{sizeof(info)}; GetComboBoxInfo(app.duration_preset_, &info);
                Check(PaintPixel(info.hwndList, app.Scale(10), app.Scale(10)) == RGB(137, 186, 255),
                      "native dropdown routes selected-item painting through the theme");
                Check(Capture(info.hwndList, L"out/dark-dropdown.bmp"), "native dark dropdown is captured");
                SendMessageW(app.duration_preset_, CB_SHOWDROPDOWN, FALSE, 0);
            }
            SetChecked(app.soft_backdrop_, false);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kSoftBackdrop, BN_CLICKED), reinterpret_cast<LPARAM>(app.soft_backdrop_));
            Check(!app.settings_.soft_backdrop && !app.backdrop_enabled_, "solid preference disables native backdrop");
            SetChecked(app.dark_appearance_, false);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
            Check(!app.settings_.dark_appearance, "light appearance can be restored live");
            SetControlText(app.interval_, L"");
            SetChecked(app.dark_appearance_, true);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
            Check(app.settings_.dark_appearance && ControlText(app.interval_).empty(), "appearance changes preserve unfinished numeric edits");
            SetControlText(app.interval_, std::to_wstring(before_theme.interval.count()));
            SetChecked(app.dark_appearance_, false);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
            app.saved_settings_ = app.settings_; app.dirty_ = false; app.UpdateDirtyPresentation();
            const DWORD theme_resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (int toggle = 0; toggle < 20; ++toggle) {
                app.settings_.dark_appearance = !app.settings_.dark_appearance;
                app.RefreshAppearance();
            }
            Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= theme_resources + 2, "theme switching releases replaced brushes");
        }
        if (capture) {
            SetFocus(app.profile_);
            MSG tab{}; tab.message = WM_KEYDOWN; tab.wParam = VK_TAB;
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.power_, "Tab reaches Keep awake");
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.duration_preset_, "Tab reaches session duration");
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.section_buttons_[0], "Tab skips collapsed custom and advanced controls");
            Check(capture_window(L"out/compact.bmp"), "compact render is captured");
            app.high_contrast_ = true;
            RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            Check(capture_window(L"out/system-colors.bmp"), "system-color native fallback is captured");
            app.RefreshAppearance();
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
        RECT numeric_client{}; GetClientRect(app.max_duration_, &numeric_client);
        Check(numeric_client.bottom >= app.Scale(18), "padded native numeric field retains usable text and caret space");
        SetFocus(app.max_duration_);
        SendMessageW(app.max_duration_, EM_SETSEL, 0, -1);
        SendMessageW(app.max_duration_, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L"124"));
        Check(ControlText(app.max_duration_) == L"124", "styled native numeric field retains selection and editing");
        SendMessageW(app.max_duration_, WM_UNDO, 0, 0);
        Check(ControlText(app.max_duration_) == L"123", "styled native numeric field retains Undo");
        SetControlText(app.max_duration_, L"2592001");
        std::wstring error;
        Check(!app.ReadControls(error), "custom duration preserves thirty-day validation");
        SetControlText(app.max_duration_, L"123"); app.UpdateDirtyStateFromControls();
        if (capture) Check(capture_window(L"out/custom.bmp"), "custom render is captured");

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
        if (capture) Check(capture_window(L"out/expanded.bmp"), "expanded render is captured");
        if (capture) {
            SetChecked(app.dark_appearance_, true);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
            Check(capture_window(L"out/dark-expanded.bmp"), "dark expanded appearance is captured");
            SetChecked(app.dark_appearance_, false);
            app.HandleMessage(WM_COMMAND, MAKEWPARAM(kDarkAppearance, BN_CLICKED), reinterpret_cast<LPARAM>(app.dark_appearance_));
        }

        if (!app.high_contrast_) {
            const HWND previous_focus = GetFocus();
            SetChecked(app.lock_pause_, true);
            SetFocus(app.lock_pause_);
            Check(GetFocus() == app.lock_pause_, "checked native checkbox accepts keyboard focus");
            Check(PaintPixel(app.lock_pause_, app.Scale(40), app.Scale(3)) == RGB(0, 91, 211),
                  "checked checkbox has a distinct blue keyboard focus outline");
            SetFocus(app.duration_preset_);
            Check(PaintPixel(app.lock_pause_, app.Scale(40), app.Scale(3)) != RGB(0, 91, 211),
                  "checked checkbox outline disappears when focus leaves");
            SetFocus(previous_focus);
        }

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
        Check(IsWindowEnabled(app.dark_appearance_) && IsWindowEnabled(app.soft_backdrop_), "appearance remains available while running");
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
            Check(capture_window(L"out/narrow.bmp"), "narrow render is captured");
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
