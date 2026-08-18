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

    LASTINPUTINFO last_input{sizeof(last_input), 0};
    if (GetLastInputInfo(&last_input) != FALSE) {
        last_genuine_input_tick_.store(last_input.dwTime, std::memory_order_relaxed);
    } else {
        last_genuine_input_tick_.store(GetTickCount64(), std::memory_order_relaxed);
    }

    const HINSTANCE module = GetModuleHandleW(nullptr);
    mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, module, 0);
    keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, module, 0);

    if (mouse_hook_ == nullptr && keyboard_hook_ == nullptr) {
        Stop();
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
}

InputMonitorCapabilities InputMonitor::capabilities() const noexcept {
    return {.mouse = mouse_hook_ != nullptr, .keyboard = keyboard_hook_ != nullptr};
}

std::uint64_t InputMonitor::last_genuine_input_tick() const noexcept {
    return last_genuine_input_tick_.load(std::memory_order_relaxed);
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
    last_genuine_input_tick_.store(GetTickCount64(), std::memory_order_relaxed);
    if (notification_window_ != nullptr && notification_message_ != 0) {
        PostMessageW(notification_window_, notification_message_, 0, 0);
    }
}

}  // namespace idleharbor::platform::windows
