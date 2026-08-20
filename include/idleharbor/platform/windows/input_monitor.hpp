#pragma once

#include <windows.h>

#include <atomic>

namespace idleharbor::platform::windows {

inline constexpr ULONG_PTR kIdleHarborInputMarker =
    static_cast<ULONG_PTR>(0x49444842504C5345ULL);  // "IDHBPLSE"

struct InputMonitorCapabilities {
    bool mouse = false;
    bool keyboard = false;

    [[nodiscard]] bool any() const noexcept { return mouse || keyboard; }
};

class InputMonitor final {
  public:
    InputMonitor() = default;
    ~InputMonitor();

    InputMonitor(const InputMonitor&) = delete;
    InputMonitor& operator=(const InputMonitor&) = delete;
    InputMonitor(InputMonitor&&) = delete;
    InputMonitor& operator=(InputMonitor&&) = delete;

    [[nodiscard]] InputMonitorCapabilities Start(HWND notification_window, UINT notification_message) noexcept;
    // Reinstall both low-level hooks so silent OS removal is detected within one
    // application timer interval. A failed refresh is a capability loss.
    [[nodiscard]] InputMonitorCapabilities Refresh() noexcept;
    void Stop() noexcept;
    void AcknowledgeNotification() noexcept;

    [[nodiscard]] InputMonitorCapabilities capabilities() const noexcept;

  private:
    static LRESULT CALLBACK MouseHook(int code, WPARAM event, LPARAM data) noexcept;
    static LRESULT CALLBACK KeyboardHook(int code, WPARAM event, LPARAM data) noexcept;
    void RecordGenuineInput() noexcept;

    static std::atomic<InputMonitor*> active_monitor_;

    HHOOK mouse_hook_ = nullptr;
    HHOOK keyboard_hook_ = nullptr;
    HWND notification_window_ = nullptr;
    UINT notification_message_ = 0;
    std::atomic<bool> notification_pending_{false};
};

}  // namespace idleharbor::platform::windows
