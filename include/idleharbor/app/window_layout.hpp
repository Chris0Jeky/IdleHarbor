#pragma once

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

}  // namespace idleharbor::app
