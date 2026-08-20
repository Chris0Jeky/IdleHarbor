#include <iostream>

#include "idleharbor/app/window_layout.hpp"

namespace {

int failures = 0;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << "CHECK failed at line " << __LINE__ << ": " #condition "\n";                             \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (false)

void test_window_is_clamped_inside_inset_work_area() {
    using idleharbor::app::ClampWindowRect;
    const auto result = ClampWindowRect({100, 50, 1300, 1450}, {0, 0, 1920, 1040}, 16);
    CHECK(result.left == 100);
    CHECK(result.top == 16);
    CHECK(result.right == 1300);
    CHECK(result.bottom == 1024);
}

void test_oversized_window_uses_available_work_area() {
    using idleharbor::app::ClampWindowRect;
    const auto result = ClampWindowRect({-400, -300, 2400, 1700}, {0, 0, 1280, 720}, 12);
    CHECK(result.left == 12);
    CHECK(result.top == 12);
    CHECK(result.right == 1268);
    CHECK(result.bottom == 708);
}

void test_scroll_range_and_clamping() {
    using idleharbor::app::ClampScrollPosition;
    using idleharbor::app::MaximumScrollPosition;
    CHECK(MaximumScrollPosition(1330, 1000) == 330);
    CHECK(MaximumScrollPosition(700, 1000) == 0);
    CHECK(ClampScrollPosition(-10, 1330, 1000) == 0);
    CHECK(ClampScrollPosition(400, 1330, 1000) == 330);
}

void test_focus_reveal_scrolls_only_when_needed() {
    using idleharbor::app::ScrollPositionToReveal;
    CHECK(ScrollPositionToReveal(100, 150, 190, 1330, 500) == 100);
    CHECK(ScrollPositionToReveal(100, 40, 80, 1330, 500) == 40);
    CHECK(ScrollPositionToReveal(100, 850, 900, 1330, 500) == 400);
    CHECK(ScrollPositionToReveal(300, 1300, 1340, 1330, 500) == 830);
}

void test_wheel_delta_accumulates_high_resolution_input() {
    using idleharbor::app::ConsumeWheelDelta;
    auto result = ConsumeWheelDelta(0, 40);
    CHECK(result.steps == 0);
    CHECK(result.remainder == 40);
    result = ConsumeWheelDelta(result.remainder, 40);
    CHECK(result.steps == 0);
    CHECK(result.remainder == 80);
    result = ConsumeWheelDelta(result.remainder, 40);
    CHECK(result.steps == 1);
    CHECK(result.remainder == 0);
}

void test_wheel_delta_preserves_direction_and_remainder() {
    using idleharbor::app::ConsumeWheelDelta;
    auto result = ConsumeWheelDelta(0, -200);
    CHECK(result.steps == -1);
    CHECK(result.remainder == -80);
    result = ConsumeWheelDelta(result.remainder, 40);
    CHECK(result.steps == 0);
    CHECK(result.remainder == -40);
    result = ConsumeWheelDelta(result.remainder, 160);
    CHECK(result.steps == 1);
    CHECK(result.remainder == 0);
}

void test_fixed_safety_regions_never_scroll_with_the_body() {
    using idleharbor::app::ComputeSafetyRegions;
    const auto regions = ComputeSafetyRegions(600, 700, 1);
    CHECK(regions.status.top == 12);
    CHECK(regions.status.bottom <= regions.viewport.top);
    CHECK(regions.viewport.top == 58);
    CHECK(regions.viewport.bottom == regions.actions.top);
    CHECK(regions.actions.bottom == 700);
    const auto compact = ComputeSafetyRegions(220, 320, 1);
    CHECK(compact.status.left >= 0 && compact.status.right <= 220);
    CHECK(compact.status.bottom <= compact.viewport.top);
    CHECK(compact.viewport.bottom == compact.actions.top);
}

void test_focus_reveal_is_gated_by_actual_focus_change() {
    using idleharbor::app::FocusChanged;
    CHECK(!FocusChanged(0x10, 0x10));
    CHECK(FocusChanged(0x10, 0x20));
    CHECK(FocusChanged(0, 0x20));
}

void test_narrow_work_areas_reflow_settings_and_actions() {
    using idleharbor::app::ActionLayoutMode;
    using idleharbor::app::DetermineActionLayout;
    using idleharbor::app::DetermineSettingsLayout;
    using idleharbor::app::SettingsLayoutMode;
    CHECK(DetermineSettingsLayout(600, 1) == SettingsLayoutMode::Columns);
    CHECK(DetermineSettingsLayout(420, 1) == SettingsLayoutMode::Stacked);
    CHECK(DetermineActionLayout(600, 1) == ActionLayoutMode::Wide);
    CHECK(DetermineActionLayout(320, 1) == ActionLayoutMode::Wrapped);
    CHECK(DetermineActionLayout(220, 1) == ActionLayoutMode::Stacked);
}

}  // namespace

int main() {
    test_window_is_clamped_inside_inset_work_area();
    test_oversized_window_uses_available_work_area();
    test_scroll_range_and_clamping();
    test_focus_reveal_scrolls_only_when_needed();
    test_wheel_delta_accumulates_high_resolution_input();
    test_wheel_delta_preserves_direction_and_remainder();
    test_fixed_safety_regions_never_scroll_with_the_body();
    test_focus_reveal_is_gated_by_actual_focus_change();
    test_narrow_work_areas_reflow_settings_and_actions();
    return failures == 0 ? 0 : 1;
}
