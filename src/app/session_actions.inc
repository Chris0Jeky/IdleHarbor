// Included inside Application's private section. Session state stays in the
// existing policy engine; these native actions never save preference defaults.

    [[nodiscard]] static std::wstring FormatCountdown(Seconds duration) {
        const auto total = std::max(duration.count(), Seconds::rep{0});
        const auto hours = total / 3600;
        const auto minutes = (total / 60) % 60;
        const auto seconds = total % 60;
        const auto two_digits = [](Seconds::rep value) {
            return (value < 10 ? L"0" : L"") + std::to_wstring(value);
        };
        return hours > 0 ? std::to_wstring(hours) + L":" + two_digits(minutes) + L":" + two_digits(seconds)
                         : std::to_wstring(minutes) + L":" + two_digits(seconds);
    }

    bool ReleasePowerRequest() noexcept {
        power_request_.Clear();
        power_cleanup_pending_ = power_request_.active();
        return !power_cleanup_pending_;
    }

    void StartTimedSession(Seconds duration) {
        if (session_active_) {
            MessageBoxW(window_, L"A session is already active. Extend it or stop it before starting a new timer.",
                        L"IdleHarbor session", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (!settings_load_warnings_.empty()) {
            ShowWindow(window_, SW_SHOW);
            SetStatus(L"Stopped: settings recovered; review and save before automatic start");
            return;
        }
        StartSession(duration);
    }

    void ControlSession(RequestedCommand command, Seconds duration = Seconds{0}) {
        if (!session_active_ || policy_ == nullptr) {
            MessageBoxW(window_, L"No session is active. Start a session before using this control.",
                        L"IdleHarbor session", MB_OK | MB_ICONINFORMATION);
            return;
        }
        bool accepted = false;
        switch (command) {
        case RequestedCommand::Pause:
            accepted = policy_->pause(NowSeconds());
            break;
        case RequestedCommand::Resume:
            accepted = policy_->resume();
            break;
        case RequestedCommand::Snooze:
            accepted = duration > Seconds{0} && policy_->pause(NowSeconds(), duration);
            break;
        case RequestedCommand::Extend:
            accepted = policy_->extend_duration(NowSeconds(), duration);
            if (accepted) {
                runtime_settings_.max_duration += duration;
            }
            break;
        default:
            return;
        }
        if (accepted || policy_->state() == EngineState::Stopped) {
            Evaluate(false);
        }
        if (!accepted) {
            const wchar_t* explanation = command == RequestedCommand::Resume
                ? L"The session is not manually paused. Automatic safeguards clear only when their conditions clear."
                : command == RequestedCommand::Extend
                ? L"Only an unexpired timed session can be extended, to at most 30 days total. No time was added."
                : L"This session could not be paused. It may have expired; snooze must be from 1 second to 24 hours.";
            MessageBoxW(window_, explanation, L"IdleHarbor session", MB_OK | MB_ICONINFORMATION);
        }
    }

    void ShowTrayMenu() {
        const auto menu_session_generation = session_generation_;
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) {
            ShowWindow(window_, SW_SHOW);
            return;
        }
        constexpr UINT kExitMenu = 199;
        constexpr UINT kQuickStart = 200;
        constexpr UINT kPauseMenu = 210;
        constexpr UINT kResumeMenu = 211;
        constexpr UINT kSnoozeMenu = 212;
        constexpr UINT kExtendMenu = 220;
        constexpr std::array<Seconds, 4> quick_times{Seconds{900}, Seconds{1800}, Seconds{3600}, Seconds{7200}};
        constexpr std::array<const wchar_t*, 4> quick_labels{L"15 minutes", L"30 minutes", L"1 hour", L"2 hours"};
        constexpr std::array<Seconds, 3> snooze_times{Seconds{300}, Seconds{900}, Seconds{1800}};
        constexpr std::array<const wchar_t*, 3> snooze_labels{L"5 minutes", L"15 minutes", L"30 minutes"};
        bool menu_ok = true;
        const auto add = [&](HMENU parent, UINT flags, UINT_PTR id, const wchar_t* label) {
            if (AppendMenuW(parent, flags, id, label) == FALSE) {
                menu_ok = false;
            }
        };
        const auto add_submenu = [&](UINT first_id, const auto& labels, const wchar_t* title) {
            HMENU child = CreatePopupMenu();
            if (child == nullptr) {
                menu_ok = false;
                return;
            }
            for (std::size_t index = 0; index < labels.size(); ++index) {
                add(child, MF_STRING, first_id + static_cast<UINT>(index), labels[index]);
            }
            if (AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(child), title) == FALSE) {
                DestroyMenu(child);
                menu_ok = false;
            }
        };
        add(menu, MF_STRING, kStatus, L"Show");
        if (session_active_) {
            add(menu, MF_STRING, kStop, L"Stop");
            const bool held = policy_ != nullptr && policy_->manually_paused();
            add(menu, MF_STRING, held ? kResumeMenu : kPauseMenu, held ? L"Resume" : L"Pause");
            add_submenu(kSnoozeMenu, snooze_labels, L"Snooze for");
            if (policy_ != nullptr && policy_->remaining_duration(NowSeconds()).has_value()) {
                add(menu, MF_STRING, kExtendMenu, L"Add 15 minutes");
                add(menu, MF_STRING, kExtendMenu + 1, L"Add 1 hour");
            }
        } else if (power_cleanup_pending_) {
            add(menu, MF_STRING, kStop, L"Stop");
        } else {
            add(menu, MF_STRING, kStart, L"Start");
            add_submenu(kQuickStart, quick_labels, L"Start for");
        }
        add(menu, MF_SEPARATOR, 0, nullptr);
        add(menu, MF_STRING, kExitMenu, L"Exit");
        if (!menu_ok) {
            DestroyMenu(menu);
            ShowWindow(window_, SW_SHOW);
            return;
        }
        SetForegroundWindow(window_);
        POINT cursor{};
        GetCursorPos(&cursor);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, 0, window_, nullptr);
        DestroyMenu(menu);
        // Timer/forwarded messages in the nested loop may replace the session,
        // even at the same clock tick. Show and Exit remain global actions.
        if (command != 0 && command != kStatus && command != kExitMenu &&
            menu_session_generation != session_generation_) {
            MessageBoxW(window_, L"The session changed while the menu was open. Open the menu again to choose an action.",
                        L"IdleHarbor session", MB_OK | MB_ICONINFORMATION);
            return;
        }
        if (command == kStatus) {
            ShowWindow(window_, SW_SHOW);
            SetForegroundWindow(window_);
        } else if (command == kStart) {
            StartSession();
        } else if (command == kStop) {
            StopSession();
        } else if (command == kExitMenu) {
            RequestExit();
        } else if (command >= kQuickStart && command < kQuickStart + quick_times.size()) {
            StartTimedSession(quick_times[command - kQuickStart]);
        } else if (command == kPauseMenu || command == kResumeMenu) {
            ControlSession(command == kPauseMenu ? RequestedCommand::Pause : RequestedCommand::Resume);
        } else if (command >= kSnoozeMenu && command < kSnoozeMenu + snooze_times.size()) {
            ControlSession(RequestedCommand::Snooze, snooze_times[command - kSnoozeMenu]);
        } else if (command == kExtendMenu || command == kExtendMenu + 1) {
            ControlSession(RequestedCommand::Extend, command == kExtendMenu ? Seconds{900} : Seconds{3600});
        }
    }
