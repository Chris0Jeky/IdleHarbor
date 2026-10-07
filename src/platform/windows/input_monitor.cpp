#include "idleharbor/platform/windows/input_monitor.hpp"

namespace idleharbor::platform::windows {

std::atomic<InputMonitor*> InputMonitor::active_monitor_{nullptr};
std::array<HHOOK, 4> InputMonitor::abandoned_hooks_{};

InputMonitor::~InputMonitor() {
    Stop();
    if (HasHooks()) {
        // Stop retains exclusive ownership while handles remain. Preserve those
        // handles without leaving callbacks a pointer to this destroyed object.
        abandoned_hooks_ = {mouse_hook_, keyboard_hook_, retired_mouse_, retired_keyboard_};
        InputMonitor* expected = this;
        active_monitor_.compare_exchange_strong(expected, nullptr);
    }
}

void InputMonitor::ReleaseHook(HHOOK& hook) noexcept {
    if (hook != nullptr &&
        (UnhookWindowsHookEx(hook) != FALSE || GetLastError() == ERROR_INVALID_HOOK_HANDLE)) {
        // Windows may already have removed a timed-out low-level hook.
        hook = nullptr;
    }
}

bool InputMonitor::HasHooks() const noexcept {
    return mouse_hook_ != nullptr || keyboard_hook_ != nullptr ||
           retired_mouse_ != nullptr || retired_keyboard_ != nullptr;
}

InputMonitorCapabilities InputMonitor::Start(
    const HWND notification_window,
    const UINT notification_message) noexcept {
    Stop();
    if (HasHooks()) {
        return {};
    }

    InputMonitor* expected = nullptr;
    if (!active_monitor_.compare_exchange_strong(expected, this)) {
        return {};
    }

    if (notification_window == nullptr || notification_message == 0) {
        Stop();
        return {};
    }
    bool cleanup_pending = false;
    for (HHOOK& hook : abandoned_hooks_) {
        ReleaseHook(hook);
        cleanup_pending = cleanup_pending || hook != nullptr;
    }
    if (cleanup_pending) {
        Stop();
        return {};
    }
    notification_window_ = notification_window;
    notification_message_ = notification_message;
    notification_pending_.store(false, std::memory_order_relaxed);

    if (!Refresh().any()) {
        Stop();
    }
    return capabilities();
}

InputMonitorCapabilities InputMonitor::Refresh() noexcept {
    if (active_monitor_.load(std::memory_order_acquire) != this || notification_window_ == nullptr) {
        return {};
    }

    // These slots are empty in a healthy observer. Record replacements before
    // any failure path so a partially installed hook cannot lose its owner.
    const HINSTANCE module = GetModuleHandleW(nullptr);
    retired_mouse_ = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, module, 0);
    retired_keyboard_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, module, 0);
    if (retired_mouse_ == nullptr || retired_keyboard_ == nullptr) {
        Stop();
        return {};
    }

    const HHOOK old_mouse = mouse_hook_;
    const HHOOK old_keyboard = keyboard_hook_;
    mouse_hook_ = retired_mouse_;
    keyboard_hook_ = retired_keyboard_;
    retired_mouse_ = old_mouse;
    retired_keyboard_ = old_keyboard;
    ReleaseHook(retired_mouse_);
    ReleaseHook(retired_keyboard_);
    if (retired_mouse_ != nullptr || retired_keyboard_ != nullptr) {
        // Do not accumulate replacements or claim an observer is healthy after
        // failed retirement. Stop also disables callbacks before cleanup retries.
        Stop();
        return {};
    }
    return capabilities();
}

void InputMonitor::Stop() noexcept {
    notification_window_ = nullptr;
    notification_message_ = 0;
    notification_pending_.store(false, std::memory_order_relaxed);
    ReleaseHook(mouse_hook_);
    ReleaseHook(keyboard_hook_);
    ReleaseHook(retired_mouse_);
    ReleaseHook(retired_keyboard_);
    if (!HasHooks()) {
        InputMonitor* expected = this;
        active_monitor_.compare_exchange_strong(expected, nullptr);
    }
}

void InputMonitor::AcknowledgeNotification() noexcept {
    notification_pending_.store(false, std::memory_order_release);
}

InputMonitorCapabilities InputMonitor::capabilities() const noexcept {
    if (notification_window_ == nullptr || retired_mouse_ != nullptr || retired_keyboard_ != nullptr) {
        return {};
    }
    return {.mouse = mouse_hook_ != nullptr, .keyboard = keyboard_hook_ != nullptr};
}

LRESULT CALLBACK InputMonitor::MouseHook(const int code, const WPARAM event, const LPARAM data) noexcept {
    if (code == HC_ACTION) {
        const auto* details = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        const bool injected =
            (details->flags & LLMHF_INJECTED) != 0 || details->dwExtraInfo == kIdleHarborInputMarker;
        if (!injected) {
            if (InputMonitor* monitor = active_monitor_.load(std::memory_order_relaxed); monitor != nullptr) {
                monitor->RecordGenuineInput();
            }
        }
    }
    return CallNextHookEx(nullptr, code, event, data);
}

LRESULT CALLBACK InputMonitor::KeyboardHook(const int code, const WPARAM event, const LPARAM data) noexcept {
    if (code == HC_ACTION) {
        const auto* details = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        const bool injected =
            (details->flags & LLKHF_INJECTED) != 0 || details->dwExtraInfo == kIdleHarborInputMarker;
        if (!injected) {
            if (InputMonitor* monitor = active_monitor_.load(std::memory_order_relaxed); monitor != nullptr) {
                monitor->RecordGenuineInput();
            }
        }
    }
    return CallNextHookEx(nullptr, code, event, data);
}

void InputMonitor::RecordGenuineInput() noexcept {
    if (notification_window_ != nullptr && notification_message_ != 0 &&
        !notification_pending_.exchange(true, std::memory_order_acq_rel)) {
        if (PostMessageW(notification_window_, notification_message_, 0, 0) == FALSE) {
            notification_pending_.store(false, std::memory_order_release);
        }
    }
}

}  // namespace idleharbor::platform::windows
