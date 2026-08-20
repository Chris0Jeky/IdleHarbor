#include "idleharbor/app/window_layout.hpp"

#include <algorithm>

namespace idleharbor::app {

namespace {

constexpr int kWheelDeltaPerStep = 120;
constexpr int kDefaultDpi = 96;

int RoundedRatio(const int value, const int numerator, const int denominator) noexcept {
    const int safe_denominator = std::max(denominator, 1);
    const long long scaled = static_cast<long long>(std::max(value, 0)) * numerator;
    return static_cast<int>((scaled + safe_denominator / 2) / safe_denominator);
}

}  // namespace

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

WheelDeltaResult ConsumeWheelDelta(const int remainder, const int delta) noexcept {
    const int total = remainder + delta;
    return {total / kWheelDeltaPerStep, total % kWheelDeltaPerStep};
}

int LogicalPixels(const int physical_pixels, const int dpi) noexcept {
    return RoundedRatio(physical_pixels, kDefaultDpi, dpi);
}

int PhysicalPixels(const int logical_pixels, const int dpi) noexcept {
    return RoundedRatio(logical_pixels, std::max(dpi, 1), kDefaultDpi);
}

ActionLayoutMode DetermineActionLayout(const int client_width, const int dpi) noexcept {
    if (LogicalPixels(client_width, dpi) >= 380) {
        return ActionLayoutMode::Wide;
    }
    if (LogicalPixels(client_width, dpi) >= 245) {
        return ActionLayoutMode::Wrapped;
    }
    return ActionLayoutMode::Stacked;
}

SettingsLayoutMode DetermineSettingsLayout(const int client_width, const int dpi) noexcept {
    return LogicalPixels(client_width, dpi) >= 560 ? SettingsLayoutMode::Columns : SettingsLayoutMode::Stacked;
}

SafetyRegions ComputeSafetyRegions(const int client_width, const int client_height, const int dpi) noexcept {
    const int safe_dpi = std::max(dpi, 1);
    const int width = std::max(client_width, 1);
    const int height = std::max(client_height, 1);
    const int header_height = std::min(PhysicalPixels(58, safe_dpi), height);
    const auto action_layout = DetermineActionLayout(width, safe_dpi);
    const int footer_height = PhysicalPixels(
        action_layout == ActionLayoutMode::Wide ? 52 : action_layout == ActionLayoutMode::Wrapped ? 92 : 132,
        safe_dpi);
    const int footer_top = std::max(header_height, height - footer_height);
    const int horizontal_margin = std::min(PhysicalPixels(20, safe_dpi), std::max((width - 1) / 2, 0));
    return {
        {horizontal_margin,
         std::min(PhysicalPixels(12, safe_dpi), height),
         width - horizontal_margin,
         std::min(PhysicalPixels(42, safe_dpi), header_height)},
        {0, footer_top, width, height},
        {0, header_height, width, footer_top},
    };
}

ActionButtonRects ComputeActionButtonRects(const int client_width, const int client_height, const int dpi) noexcept {
    const int safe_dpi = std::max(dpi, 1);
    const int width = std::max(client_width, 1);
    const int height = std::max(client_height, 1);
    const auto regions = ComputeSafetyRegions(width, height, safe_dpi);
    const auto action_layout = DetermineActionLayout(width, safe_dpi);
    const int margin = PhysicalPixels(20, safe_dpi);
    const int gap = PhysicalPixels(10, safe_dpi);
    const int available = std::max(width - 2 * margin, 1);
    const int action_top = std::clamp(regions.actions.top, 0, height);
    const int action_height = std::max(height - action_top, 0);
    if (action_height == 0) {
        return {};
    }

    const auto make_rect = [&](const int requested_x, const int requested_y, const int requested_width, const int requested_height) {
        const int rect_width = std::clamp(requested_width, 1, width);
        const int rect_height = std::clamp(requested_height, 1, action_height);
        const int x = std::clamp(requested_x, 0, width - rect_width);
        const int y = std::clamp(requested_y, action_top, height - rect_height);
        return PixelRect{x, y, x + rect_width, y + rect_height};
    };

    if (action_layout == ActionLayoutMode::Stacked) {
        const int button_height = std::min(PhysicalPixels(32, safe_dpi), std::max((action_height - 2 * gap) / 3, 1));
        const int actual_gap = std::min(gap, std::max((action_height - 3 * button_height) / 2, 0));
        const int block_height = 3 * button_height + 2 * actual_gap;
        const int first_y = action_top + std::max((action_height - block_height) / 2, 0);
        return {
            make_rect(margin, first_y, available, button_height),
            make_rect(margin, first_y + button_height + actual_gap, available, button_height),
            make_rect(margin, first_y + 2 * (button_height + actual_gap), available, button_height),
        };
    }

    if (action_layout == ActionLayoutMode::Wrapped) {
        const int button_height = std::min(
            PhysicalPixels(32, safe_dpi),
            std::max((action_height - gap) / 2, 1));
        const int actual_gap = std::min(gap, std::max(action_height - 2 * button_height, 0));
        const int block_height = 2 * button_height + actual_gap;
        const int first_y = action_top + std::max((action_height - block_height) / 2, 0);
        const int half_width = std::max((available - gap) / 2, 1);
        return {
            make_rect(margin, first_y, half_width, button_height),
            make_rect(margin + half_width + gap, first_y, half_width, button_height),
            make_rect((width - half_width) / 2, first_y + button_height + actual_gap, half_width, button_height),
        };
    }

    const int button_height = std::min(PhysicalPixels(32, safe_dpi), std::max(action_height, 1));
    const int button_y = action_top + std::max((action_height - button_height) / 2, 0);
    const int button_width = std::min(PhysicalPixels(115, safe_dpi), available);
    return {
        make_rect(margin, button_y, button_width, button_height),
        make_rect(margin + PhysicalPixels(125, safe_dpi), button_y, button_width, button_height),
        make_rect(margin + 2 * PhysicalPixels(125, safe_dpi), button_y, button_width, button_height),
    };
}

bool TabOrderBefore(const TabOrderPosition& left, const TabOrderPosition& right) noexcept {
    const auto left_region = static_cast<int>(left.region);
    const auto right_region = static_cast<int>(right.region);
    if (left_region != right_region) {
        return left_region < right_region;
    }
    if (left.top != right.top) {
        return left.top < right.top;
    }
    if (left.left != right.left) {
        return left.left < right.left;
    }
    return left.sequence < right.sequence;
}

StackedBodyLayout ComputeStackedBodyLayout(const int logical_client_width) noexcept {
    const int width = std::max(logical_client_width, 1);
    const int margin = std::min(20, std::max((width - 1) / 2, 0));
    return {margin, std::max(width - 2 * margin, 1)};
}

bool FocusChanged(const std::uintptr_t previous_focus, const std::uintptr_t current_focus) noexcept {
    return previous_focus != current_focus;
}

}  // namespace idleharbor::app
