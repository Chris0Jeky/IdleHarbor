#include "idleharbor/platform/windows/input_monitor.hpp"

namespace idleharbor::platform::windows {

std::atomic<InputMonitor*> InputMonitor::active_monitor_{nullptr};

InputMonitor::~InputMonitor() {
    Stop();
}

InputMonitorCapabilities InputMonitor::Start(
    const HWND notification_window,
    const UINT notification_message) noexcept {
    Stop();

    InputMonitor* expected = nullptr;
    if (!active_monitor_.compare_exchange_strong(expected, this)) {
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
    if (active_monitor_.load(std::memory_order_acquire) != this) {
        return {};
    }

    const HINSTANCE module = GetModuleHandleW(nullptr);
    const HHOOK replacement_mouse = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, module, 0);
    const HHOOK replacement_keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, module, 0);
    if (replacement_mouse == nullptr || replacement_keyboard == nullptr) {
        if (replacement_mouse != nullptr) {
            UnhookWindowsHookEx(replacement_mouse);
        }
        if (replacement_keyboard != nullptr) {
            UnhookWindowsHookEx(replacement_keyboard);
        }
        Stop();
        return {};
    }

    const HHOOK old_mouse = mouse_hook_;
    const HHOOK old_keyboard = keyboard_hook_;
    mouse_hook_ = replacement_mouse;
    keyboard_hook_ = replacement_keyboard;
    if (old_mouse != nullptr) {
        UnhookWindowsHookEx(old_mouse);
    }
    if (old_keyboard != nullptr) {
        UnhookWindowsHookEx(old_keyboard);
    }
    return capabilities();
}

void InputMonitor::Stop() noexcept {
    if (mouse_hook_ != nullptr) {
        UnhookWindowsHookEx(mouse_hook_);
        mouse_hook_ = nullptr;
    }
    if (keyboard_hook_ != nullptr) {
        UnhookWindowsHookEx(keyboard_hook_);
        keyboard_hook_ = nullptr;
    }

    InputMonitor* expected = this;
    active_monitor_.compare_exchange_strong(expected, nullptr);
    notification_window_ = nullptr;
    notification_message_ = 0;
    notification_pending_.store(false, std::memory_order_relaxed);
}

void InputMonitor::AcknowledgeNotification() noexcept {
    notification_pending_.store(false, std::memory_order_release);
}

InputMonitorCapabilities InputMonitor::capabilities() const noexcept {
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
