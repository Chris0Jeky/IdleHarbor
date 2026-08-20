#pragma once

#include <cstdint>

namespace idleharbor::app {

struct PixelRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct WheelDeltaResult {
    int steps = 0;
    int remainder = 0;
};

enum class ActionLayoutMode {
    Wide,
    Wrapped,
    Stacked,
};

enum class SettingsLayoutMode {
    Columns,
    Stacked,
};

struct SafetyRegions {
    PixelRect status;
    PixelRect actions;
    PixelRect viewport;
};

[[nodiscard]] PixelRect ClampWindowRect(PixelRect desired, PixelRect work_area, int margin) noexcept;
[[nodiscard]] int MaximumScrollPosition(int content_height, int viewport_height) noexcept;
[[nodiscard]] int ClampScrollPosition(int position, int content_height, int viewport_height) noexcept;
[[nodiscard]] int ScrollPositionToReveal(
    int current_position,
    int control_top,
    int control_bottom,
    int content_height,
    int viewport_height) noexcept;
[[nodiscard]] WheelDeltaResult ConsumeWheelDelta(int remainder, int delta) noexcept;
[[nodiscard]] int LogicalPixels(int physical_pixels, int dpi) noexcept;
[[nodiscard]] int PhysicalPixels(int logical_pixels, int dpi) noexcept;
[[nodiscard]] ActionLayoutMode DetermineActionLayout(int client_width, int dpi) noexcept;
[[nodiscard]] SettingsLayoutMode DetermineSettingsLayout(int client_width, int dpi) noexcept;
[[nodiscard]] SafetyRegions ComputeSafetyRegions(int client_width, int client_height, int dpi) noexcept;
[[nodiscard]] bool FocusChanged(std::uintptr_t previous_focus, std::uintptr_t current_focus) noexcept;

}  // namespace idleharbor::app
