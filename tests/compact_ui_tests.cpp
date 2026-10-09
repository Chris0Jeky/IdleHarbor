#include <fstream>
#include <iostream>
#include <windows.h>
#include <dwmapi.h>
#include <cstring>

namespace {
bool nonzero_glass_margins = false;
int frame_extension_calls = 0;
int settings_dialogs = 0;
bool reverse_tab = false;
HWND measured_hover = nullptr;
int hover_invalidations = 0;
HWND measured_fixed = nullptr;
int fixed_positions = 0;
BOOL WINAPI RecordPosition(HWND window, HWND after, int x, int y, int width, int height, UINT flags) {
    if (window == measured_fixed) ++fixed_positions;
    return ::SetWindowPos(window, after, x, y, width, height, flags);
}
BOOL WINAPI RecordInvalidation(HWND window, const RECT* area, BOOL erase) {
    if (window == measured_hover) ++hover_invalidations;
    return ::InvalidateRect(window, area, erase);
}
SHORT WINAPI FixtureKeyState(int key) {
    return key == VK_SHIFT ? (reverse_tab ? static_cast<SHORT>(0x8000) : 0) : ::GetKeyState(key);
}
int WINAPI RecordSettingsDialog(HWND, LPCWSTR, LPCWSTR, UINT) {
    ++settings_dialogs;
    return IDOK;
}
HRESULT (WINAPI* real_extend_frame)(HWND, const MARGINS*) = nullptr;
HRESULT WINAPI RecordFrameExtension(HWND window, const MARGINS* margins) {
    ++frame_extension_calls;
    nonzero_glass_margins |= margins->cxLeftWidth != 0 || margins->cxRightWidth != 0 ||
                            margins->cyTopHeight != 0 || margins->cyBottomHeight != 0;
    return real_extend_frame(window, margins);
}
FARPROC WINAPI RecordMaterialProc(HMODULE module, LPCSTR name) {
    const auto proc = ::GetProcAddress(module, name);
    if (proc != nullptr && std::strcmp(name, "DwmExtendFrameIntoClientArea") == 0) {
        real_extend_frame = reinterpret_cast<decltype(real_extend_frame)>(proc);
        return reinterpret_cast<FARPROC>(RecordFrameExtension);
    }
    return proc;
}
}
#define GetProcAddress RecordMaterialProc
#define MessageBoxW RecordSettingsDialog
#define GetKeyState FixtureKeyState
#define InvalidateRect RecordInvalidation
#define SetWindowPos RecordPosition
#include "application-under-test.inc"
#undef SetWindowPos
#undef InvalidateRect
#undef GetKeyState
#undef MessageBoxW
#undef GetProcAddress

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
    static COLORREF PaintPixel(HWND control, int x, int y, bool erase = false) {
        RECT bounds{}; GetClientRect(control, &bounds);
        HDC source = GetDC(control), memory = CreateCompatibleDC(source);
        HBITMAP bitmap = CreateCompatibleBitmap(source, bounds.right, bounds.bottom);
        const auto previous = SelectObject(memory, bitmap);
        SendMessageW(control, erase ? WM_ERASEBKGND : WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory),
                     erase ? 0 : PRF_CLIENT);
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
            WS_OVERLAPPEDWINDOW, 24, 24, ScaleForDpi(600, dpi), ScaleForDpi(kBaseWindowHeight, dpi),
            nullptr, nullptr, GetModuleHandleW(nullptr), &app);
        Check(window != nullptr, "real native window is created");
        if (window == nullptr) return;
        const auto capture_window = [&](const wchar_t* path) { return Capture(window, path); };
        app.RefreshControls(); app.UpdateButtons();
        if (capture) { ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window); }
        Check(Shown(app.profile_) && Shown(app.power_) && Shown(app.duration_preset_), "routine choices are visible");
        Check(!Shown(app.motion_) && !Shown(app.battery_) && !Shown(app.start_minimized_), "advanced settings begin collapsed");
        Check(!Shown(app.max_duration_), "preset duration hides custom seconds");
        Check(app.ContentHeight() <= app.ViewportHeight(), "default view fits without scrolling");
        Check(ControlText(window).find(idleharbor::kVersion) != std::wstring::npos, "window displays the canonical version");
        RECT duration_rect{}, profile_rect{};
        GetWindowRect(app.duration_preset_, &duration_rect); GetWindowRect(app.profile_, &profile_rect);
        Check(duration_rect.top < profile_rect.top, "session duration is the first routine choice");
        const auto original_settings = app.settings_;
        for (const bool start : {false, true}) {
            for (const auto control : {app.interval_, app.distance_, app.pause_input_, app.battery_, app.max_duration_}) {
                app.settings_ = original_settings; app.RefreshControls();
                for (int section = 0; section < 3; ++section) {
                    if (app.expanded_sections_[section]) app.ToggleSection(section);
                }
                SendMessageW(app.duration_preset_, CB_SETCURSEL, 6, 0);
                app.QueueComboBoxSelection(kDurationPreset, CBN_SELCHANGE);
                SetControlText(app.max_duration_, L"123");
                SetControlText(control, L""); app.UpdateDirtyStateFromControls();
                const auto before = app.settings_;
                const auto before_sections = app.expanded_sections_;
                const int dialogs_before = settings_dialogs;
                if (start) app.StartSession(); else app.Save();
                Check(settings_dialogs == dialogs_before + 1 && !app.session_active_,
                      "invalid Start and Save report the problem without starting a session");
                Check(ControlText(control).empty() && app.dirty_, "validation preserves the unfinished edit");
                Check(AppSettingsEqual(before, app.settings_), "rejected settings leave the last valid configuration intact");
                Check(Shown(control) && GetFocus() == control, "validation reveals and focuses the field to correct");
                if (control == app.max_duration_) Check(app.expanded_sections_ == before_sections && app.custom_duration_,
                      "custom-duration errors preserve all three disclosure states and their separate visibility group");
                if (control != app.max_duration_) Check(ControlText(app.max_duration_) == L"123",
                      "validation preserves other unfinished form edits");
            }
        }
        app.settings_ = original_settings; app.RefreshControls();
        for (int section = 0; section < 3; ++section) {
            if (app.expanded_sections_[section]) app.ToggleSection(section);
        }
        app.suppress_dirty_tracking_ = true;
        SetControlText(app.interval_, L"45"); SetControlText(app.distance_, L"");
        app.suppress_dirty_tracking_ = false;
        std::wstring rejected_error;
        Check(!app.ReadControls(rejected_error) && AppSettingsEqual(original_settings, app.settings_),
              "a late validation failure cannot commit earlier valid fields");
        app.RefreshControls();
        ShowWindow(window, SW_SHOWNOACTIVATE);
        MSG entry_tab{}; entry_tab.hwnd = window; entry_tab.message = WM_KEYDOWN; entry_tab.wParam = VK_TAB;
        SetFocus(window);
        Check(app.HandleTabNavigation(entry_tab) && GetFocus() == app.duration_preset_,
              "Tab from outside the tab order reaches session duration first");
        SetFocus(window); reverse_tab = true;
        Check(app.HandleTabNavigation(entry_tab) && GetFocus() == (IsWindowEnabled(app.save_) ? app.save_ : app.start_),
              "Shift Tab from outside the tab order reaches the last enabled action");
        reverse_tab = false;
        SendMessageW(app.duration_preset_, CB_SETCURSEL, 6, 0);
        app.QueueComboBoxSelection(kDurationPreset, CBN_SELCHANGE);
        SetControlText(app.max_duration_, L"3600"); SetFocus(app.max_duration_);
        app.SyncDurationPreset();
        Check(!Shown(app.max_duration_) && GetFocus() == app.duration_preset_,
              "hiding a preset-matching custom duration restores focus to the duration selector");
        app.settings_ = original_settings; app.RefreshControls();
        if (!capture) ShowWindow(window, SW_HIDE);
        if (capture) {
            SetControlText(app.distance_, L""); app.UpdateDirtyStateFromControls();
            Check(capture_window(L"out/validation.bmp"), "collapsed unfinished-edit feedback is captured");
            app.RefreshControls();
        }
        if (!app.high_contrast_) {
            SendMessageW(app.start_, WM_MOUSELEAVE, 0, 0);
            measured_hover = app.start_; hover_invalidations = 0;
            for (int move = 0; move < 100; ++move) SendMessageW(app.start_, WM_MOUSEMOVE, 0, MAKELPARAM(10, 10));
            std::cout << "Hover invalidations per 100 moves: " << hover_invalidations << '\n';
            Check(hover_invalidations == 1, "pointer motion within one control repaints only its hover transition");
            SendMessageW(app.start_, WM_MOUSELEAVE, 0, 0);
            SendMessageW(app.start_, WM_MOUSEMOVE, 0, MAKELPARAM(10, 10));
            Check(hover_invalidations == 3, "leaving and reentering a control each repaint the changed hover state");
            SendMessageW(app.start_, WM_ENABLE, TRUE, 0);
            Check(hover_invalidations == 4, "enable changes retain their independent repaint");
            measured_hover = nullptr;
            measured_hover = app.start_; hover_invalidations = 0;
            SendMessageW(app.start_, BM_SETSTATE, TRUE, 0);
            Check(hover_invalidations > 0, "native button press invalidates after its state changes");
            Check(PaintPixel(app.start_, app.Scale(10), app.Scale(10)) == app.Colors().accent_pressed,
                  "pressed action displays its pressed palette");
            SendMessageW(app.start_, BM_SETSTATE, FALSE, 0);
            measured_hover = nullptr;
        }
        SendMessageW(app.motion_, CB_SETCURSEL, 2, 0);
        SetControlText(app.interval_, L"120"); SetChecked(app.randomize_, false);
        app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[0]).find(L"Zen · Every 120 s") != std::wstring::npos,
              "collapsed motion summary exposes the edited interval through the native name");
        SetChecked(app.randomize_, true); app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[0]).find(L"Up to 120 s") != std::wstring::npos,
              "randomized motion does not claim an exact interval");
        SetControlText(app.interval_, L""); app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[0]).find(L"Review pulse interval") != std::wstring::npos &&
              ControlText(app.interval_).empty(), "summary preserves and identifies incomplete edits");
        for (const int mode : {0, 2, 3}) {
            SendMessageW(app.motion_, CB_SETCURSEL, mode, 0);
            for (const auto invalid : {L"", L"0", L"86401"}) {
                SetControlText(app.interval_, invalid); app.UpdateDirtyStateFromControls();
                Check(ControlText(app.section_buttons_[0]).find(L"Review pulse interval") != std::wstring::npos &&
                      ControlText(app.interval_) == invalid,
                      "all motion modes expose invalid interval edits without replacing them");
            }
            SetControlText(app.interval_, L"120");
            for (const auto invalid : {L"", L"0", L"121"}) {
                SetControlText(app.distance_, invalid); app.UpdateDirtyStateFromControls();
                Check(ControlText(app.section_buttons_[0]).find(L"Review motion size") != std::wstring::npos &&
                      ControlText(app.distance_) == invalid,
                      "all motion modes expose invalid size edits without replacing them");
            }
            SetControlText(app.distance_, L"1");
        }
        SendMessageW(app.motion_, CB_SETCURSEL, 0, 0); app.QueueComboBoxSelection(kMotion, CBN_SELCHANGE);
        Check(ControlText(app.section_buttons_[0]).find(L"Motion off") != std::wstring::npos,
              "Off motion summary does not imply pulses");
        SetControlText(app.interval_, L"120"); SetControlText(app.pause_input_, L"0"); SetControlText(app.battery_, L"0");
        for (const auto control : {app.lock_pause_, app.disconnect_pause_, app.fullscreen_, app.pause_on_battery_}) SetChecked(control, false);
        app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[1]).find(L"All safety pauses off") != std::wstring::npos,
              "summary calls out disabled safeguards");
        SetChecked(app.lock_pause_, true); app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[1]).find(L"1 safeguard enabled") != std::wstring::npos,
              "one enabled safety pause is counted accurately");
        SetControlText(app.pause_input_, L"120"); SetControlText(app.battery_, L"20");
        for (const auto control : {app.disconnect_pause_, app.fullscreen_, app.pause_on_battery_}) SetChecked(control, true);
        app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[1]).find(L"6 safeguards enabled · Input pause 120 s") != std::wstring::npos,
              "summary includes every safety pause including battery power");
        SetControlText(app.battery_, L"101"); app.UpdateDirtyStateFromControls();
        Check(ControlText(app.section_buttons_[1]).find(L"Review unfinished") != std::wstring::npos,
              "invalid safety edits are not presented as valid safeguards");
        app.settings_ = original_settings; app.RefreshControls();
        Check(ControlText(app.section_buttons_[2]).find(L"Light · Soft title bar") != std::wstring::npos,
              "appearance preference is visible while collapsed");
        if (!app.high_contrast_) {
            const bool previous_backdrop = app.backdrop_enabled_;
            app.backdrop_enabled_ = true;
            Check(PaintPixel(window, app.Scale(10), app.Scale(10), true) == RGB(247,248,250),
                  "light client background stays opaque with material enabled");
            app.backdrop_enabled_ = previous_backdrop;
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
            Check(ControlText(app.section_buttons_[2]).find(L"Dark · Soft title bar") != std::wstring::npos,
                  "appearance summary updates with the live preference");
            const COLORREF dark_field = PaintPixel(app.duration_preset_, app.Scale(10), app.Scale(10));
            Check(dark_field == RGB(37, 41, 50) || dark_field == RGB(45, 50, 61), "native duration renders the dark field or hover surface");
            Check(SettingsEqual(before_theme, app.settings_.session), "theme changes preserve session settings");
            const bool dark_backdrop = app.backdrop_enabled_;
            app.backdrop_enabled_ = true;
            Check(PaintPixel(window, app.Scale(10), app.Scale(10), true) == RGB(23,25,31),
                  "dark client background stays opaque with material enabled");
            app.backdrop_enabled_ = dark_backdrop;
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
            SetFocus(app.duration_preset_);
            MSG tab{}; tab.message = WM_KEYDOWN; tab.wParam = VK_TAB;
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.profile_, "Tab reaches Profile after duration");
            Check(app.HandleTabNavigation(tab) && GetFocus() == app.power_, "Tab reaches Keep awake");
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
        Check(ControlText(app.section_buttons_[0]).find(L'\n') != std::wstring::npos,
              "expanded native names retain the settings summary");
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

        app.ScrollTo(0); measured_fixed = app.stop_; fixed_positions = 0;
        for (int step = 1; step <= 10; ++step) app.ScrollTo(step * 5);
        std::cout << "Fixed footer positions per 10 scroll updates: " << fixed_positions << '\n';
        Check(fixed_positions == 0, "scrolling only moves the already-arranged body");
        measured_fixed = nullptr;
        app.ScrollTo(100);
        app.HandleMouseWheel(MAKEWPARAM(0, 40));
        Check(app.scroll_position_ < 100, "high-resolution wheel input moves before a complete detent");
        {
            const bool was_visible = IsWindowVisible(window) != FALSE;
            const bool motion_enabled = app.scroll_motion_enabled_;
            ShowWindow(window, SW_SHOWNOACTIVATE);
            app.scroll_motion_enabled_ = true; app.ScrollTo(0);
            app.HandleMouseWheel(MAKEWPARAM(0, -120));
            Check(app.scroll_animating_ && app.scroll_position_ == 0 && app.scroll_target_ > 0,
                  "detent starts a bounded transition without jumping the form");
            const int first_target = app.scroll_target_;
            app.HandleMouseWheel(MAKEWPARAM(0, -120));
            Check(app.scroll_target_ > first_target, "rapid wheel input accumulates its destination");
            app.scroll_started_ = GetTickCount64() - 80; app.AdvanceSmoothScroll();
            Check(app.scroll_position_ > 0 && app.scroll_position_ < app.scroll_target_, "animation advances between endpoints");
            const int destination = app.scroll_target_;
            app.RequestSmoothScroll(destination);
            Check(GetTickCount64() - app.scroll_started_ >= 80, "repeated input at the same boundary does not prolong settling");
            app.scroll_started_ = GetTickCount64() - 200; app.AdvanceSmoothScroll();
            Check(app.scroll_position_ == destination && !app.scroll_animating_, "animation settles exactly and removes its timer");
            app.ScrollTo(200); app.HandleMouseWheel(MAKEWPARAM(0, -120));
            app.scroll_started_ = GetTickCount64() - 80; app.AdvanceSmoothScroll();
            const int reversing_position = app.scroll_position_;
            app.HandleMouseWheel(MAKEWPARAM(0, 120));
            Check(app.scroll_target_ < reversing_position && app.scroll_position_ == reversing_position,
                  "wheel reversal changes direction from the visible position without a jump");
            app.RequestSmoothScroll(0); app.ScrollTo(app.scroll_position_);
            Check(!app.scroll_animating_, "immediate focus reveal cancels motion even at the current position");
            app.RequestSmoothScroll(0); app.UpdateViewport();
            Check(!app.scroll_animating_, "layout cancels a stale animated destination");
            app.scroll_motion_enabled_ = false; app.RequestSmoothScroll(0);
            Check(app.scroll_position_ == 0 && !app.scroll_animating_, "reduced motion moves directly to the requested position");
            app.scroll_motion_enabled_ = true; app.RequestSmoothScroll(200);
            SendMessageW(app.profile_, CB_SHOWDROPDOWN, TRUE, 0);
            const int held = app.scroll_position_;
            app.HandleMouseWheel(MAKEWPARAM(0, -120));
            Check(!app.scroll_animating_ && app.scroll_position_ == held, "dropdown opening cancels motion and holds its anchor");
            SendMessageW(app.profile_, CB_SHOWDROPDOWN, FALSE, 0);
            app.scroll_motion_enabled_ = false;
            SetFocus(app.section_buttons_[0]);
            MSG page{}; page.message = WM_KEYDOWN; page.wParam = VK_END;
            Check(app.HandlePageNavigation(page) && app.scroll_position_ ==
                idleharbor::app::MaximumScrollPosition(app.ContentHeight(), app.ViewportHeight()), "End reaches the settings bottom from a button");
            SetFocus(app.interval_); page.wParam = VK_HOME;
            Check(!app.HandlePageNavigation(page), "Home retains native text-editing semantics");
            SetFocus(app.profile_); page.wParam = VK_NEXT;
            Check(!app.HandlePageNavigation(page), "Page Down retains native combo semantics");
            app.scroll_motion_enabled_ = motion_enabled;
            if (!was_visible) ShowWindow(window, SW_HIDE);
            SCROLLINFO info{sizeof(info)}; info.fMask = SIF_ALL;
            Check(GetScrollInfo(app.scrollbar_, SB_CTL, &info) && info.nPos == app.scroll_position_,
                  "native scrollbar exposes the current range and position");
            SCROLLBARINFO geometry{sizeof(geometry)};
            Check(GetScrollBarInfo(app.scrollbar_, OBJID_CLIENT, &geometry) && geometry.xyThumbBottom > geometry.xyThumbTop,
                  "native accessibility geometry retains a usable thumb");
            const DWORD resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (int paint = 0; paint < 100; ++paint) {
                PaintPixel(app.start_, app.Scale(10), app.Scale(10));
                PaintPixel(app.scrollbar_, app.Scale(8), geometry.xyThumbTop + 1);
            }
            Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == resources, "buffered button and scrollbar paints release GDI objects");
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
        {
            app.ToggleSection(0); app.ToggleSection(1); app.ToggleSection(2);
            RECT narrow{24, 24, 24 + ScaleForDpi(420, 144), 24 + ScaleForDpi(600, 144)};
            app.ApplyDpiChange(144, narrow);
            // Force the small-work-area width; the normal resize path keeps the preferred width.
            SetWindowPos(window, nullptr, 24, 24, ScaleForDpi(420, 144), ScaleForDpi(600, 144), SWP_NOZORDER | SWP_NOACTIVATE);
            app.UpdateViewport(); app.ScrollTo(0);
            RECT duration{}, profile{}; GetWindowRect(app.duration_preset_, &duration); GetWindowRect(app.profile_, &profile);
            Check(duration.top < profile.top, "stacked layout retains duration-first order");
            RECT section{}; GetWindowRect(app.section_buttons_[0], &section);
            Check(section.bottom - section.top == app.Scale(44), "stacked disclosure reserves both summary lines");
            RECT viewport_client{}; GetClientRect(app.settings_viewport_, &viewport_client);
            Check(idleharbor::app::DetermineSettingsLayout(viewport_client.right, 144) ==
                  idleharbor::app::SettingsLayoutMode::Stacked, "fixture exercises the actual stacked layout");
            if (capture) Check(capture_window(L"out/narrow.bmp"), "narrow render is captured");
        }
        Check(frame_extension_calls > 0 && !nonzero_glass_margins,
              "real DWM calls never extend glass into native control areas");
        // This fixture never starts a session, registers hooks, or emits input.
        DestroyWindow(window);
    }
};
}

int main(int argc, char**) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW cls{};
    cls.lpfnWndProc = [](HWND window, UINT message, WPARAM w_param, LPARAM l_param) -> LRESULT {
        // Emulate a small work area without changing the user's display or app minimum.
        if (message == WM_GETMINMAXINFO) return DefWindowProcW(window, message, w_param, l_param);
        return Application::WindowProc(window, message, w_param, l_param);
    };
    cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"IdleHarbor.CompactFixture";
    cls.hbrBackground = GetSysColorBrush(COLOR_WINDOW); cls.hIcon = LoadIdleHarborIcon(cls.hInstance);
    if (!RegisterClassW(&cls)) return 1;
    ApplicationStatusTestAccess::Run(argc > 1);
    std::cout << "Compact UI: " << checks << " checks, " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}
