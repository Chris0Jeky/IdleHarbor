#pragma once

#include <windows.h>

namespace idleharbor::platform::windows {

enum class PowerRequestMode {
    None,
    System,
    Display,
};

class PowerRequest final {
  public:
    PowerRequest() = default;
    ~PowerRequest();

    PowerRequest(const PowerRequest&) = delete;
    PowerRequest& operator=(const PowerRequest&) = delete;
    PowerRequest(PowerRequest&&) = delete;
    PowerRequest& operator=(PowerRequest&&) = delete;

    [[nodiscard]] bool Apply(PowerRequestMode mode) noexcept;
    void Clear() noexcept;

    [[nodiscard]] PowerRequestMode mode() const noexcept { return mode_; }
    [[nodiscard]] bool active() const noexcept { return mode_ != PowerRequestMode::None; }

  private:
    PowerRequestMode mode_ = PowerRequestMode::None;
};

}  // namespace idleharbor::platform::windows
