#include "idleharbor/app/window_layout.hpp"

#include <algorithm>

namespace idleharbor::app {

PixelRect ClampWindowRect(PixelRect desired, const PixelRect work_area, const int margin) noexcept {
    const int work_width = std::max(work_area.right - work_area.left, 1);
    const int work_height = std::max(work_area.bottom - work_area.top, 1);
    const int safe_margin = std::max(margin, 0);
    const int horizontal_margin = std::min(safe_margin, std::max((work_width - 1) / 2, 0));
    const int vertical_margin = std::min(safe_margin, std::max((work_height - 1) / 2, 0));

    const int usable_left = work_area.left + horizontal_margin;
    const int usable_top = work_area.top + vertical_margin;
    const int usable_right = work_area.right - horizontal_margin;
    const int usable_bottom = work_area.bottom - vertical_margin;
    const int usable_width = std::max(usable_right - usable_left, 1);
    const int usable_height = std::max(usable_bottom - usable_top, 1);

    const int desired_width = std::max(desired.right - desired.left, 1);
    const int desired_height = std::max(desired.bottom - desired.top, 1);
    const int width = std::min(desired_width, usable_width);
    const int height = std::min(desired_height, usable_height);
    const int left = std::clamp(desired.left, usable_left, usable_right - width);
    const int top = std::clamp(desired.top, usable_top, usable_bottom - height);
    return {left, top, left + width, top + height};
}

int MaximumScrollPosition(const int content_height, const int viewport_height) noexcept {
    return std::max(content_height - std::max(viewport_height, 0), 0);
}

int ClampScrollPosition(const int position, const int content_height, const int viewport_height) noexcept {
    return std::clamp(position, 0, MaximumScrollPosition(content_height, viewport_height));
}

int ScrollPositionToReveal(
    const int current_position,
    const int control_top,
    const int control_bottom,
    const int content_height,
    const int viewport_height) noexcept {
    int target = ClampScrollPosition(current_position, content_height, viewport_height);
    if (control_top < target) {
        target = control_top;
    } else if (control_bottom > target + viewport_height) {
        target = control_bottom - viewport_height;
    }
    return ClampScrollPosition(target, content_height, viewport_height);
}

}  // namespace idleharbor::app
