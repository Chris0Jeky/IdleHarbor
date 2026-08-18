#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>

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
    void Stop() noexcept;

    [[nodiscard]] InputMonitorCapabilities capabilities() const noexcept;
    [[nodiscard]] std::uint64_t last_genuine_input_tick() const noexcept;

  private:
    static LRESULT CALLBACK MouseHook(int code, WPARAM event, LPARAM data) noexcept;
    static LRESULT CALLBACK KeyboardHook(int code, WPARAM event, LPARAM data) noexcept;
    void RecordGenuineInput() noexcept;

    static std::atomic<InputMonitor*> active_monitor_;

    HHOOK mouse_hook_ = nullptr;
    HHOOK keyboard_hook_ = nullptr;
    HWND notification_window_ = nullptr;
    UINT notification_message_ = 0;
    std::atomic<std::uint64_t> last_genuine_input_tick_{0};
};

}  // namespace idleharbor::platform::windows
