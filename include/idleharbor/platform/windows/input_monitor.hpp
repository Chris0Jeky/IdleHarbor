#pragma once

#include <windows.h>

#include <array>
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

    // Lifecycle methods and destruction run on the installing thread.
    // Start requires a non-null notification window and a nonzero notification
    // message. Null/zero arguments install no hooks and return no capabilities.
    [[nodiscard]] InputMonitorCapabilities Start(HWND notification_window, UINT notification_message) noexcept;
    // Reinstall both low-level hooks so silent OS removal is detected within one
    // application timer interval. A failed refresh is a capability loss.
    [[nodiscard]] InputMonitorCapabilities Refresh() noexcept;
    // Stop quiesces notifications immediately. Failed unhooks remain owned for
    // retry and block Start; capabilities() never treats cleanup residue as active.
    void Stop() noexcept;
    void AcknowledgeNotification() noexcept;

    [[nodiscard]] InputMonitorCapabilities capabilities() const noexcept;

  private:
    static LRESULT CALLBACK MouseHook(int code, WPARAM event, LPARAM data) noexcept;
    static LRESULT CALLBACK KeyboardHook(int code, WPARAM event, LPARAM data) noexcept;
    void RecordGenuineInput() noexcept;
    [[nodiscard]] bool HasHooks() const noexcept;
    static void ReleaseHook(HHOOK& hook) noexcept;

    static std::atomic<InputMonitor*> active_monitor_;
    // Only the exclusive owner accesses quarantine. A destructor can leave at
    // most two current and two retirement handles; Start drains them first.
    static std::array<HHOOK, 4> abandoned_hooks_;

    HHOOK mouse_hook_ = nullptr;
    HHOOK keyboard_hook_ = nullptr;
    HHOOK retired_mouse_ = nullptr;
    HHOOK retired_keyboard_ = nullptr;
    HWND notification_window_ = nullptr;
    UINT notification_message_ = 0;
    std::atomic<bool> notification_pending_{false};
};

}  // namespace idleharbor::platform::windows
