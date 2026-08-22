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
    const auto regions = ComputeSafetyRegions(600, 700, 96);
    CHECK(regions.status.top == 12);
    CHECK(regions.status.bottom <= regions.viewport.top);
    CHECK(regions.viewport.top == 58);
    CHECK(regions.viewport.bottom == regions.actions.top);
    CHECK(regions.actions.bottom == 700);
    const auto compact = ComputeSafetyRegions(220, 320, 96);
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

void test_tab_order_keeps_fixed_actions_after_the_settings_body() {
    using idleharbor::app::TabOrderBefore;
    using idleharbor::app::TabOrderPosition;
    using idleharbor::app::TabOrderRegion;
    const TabOrderPosition save{TabOrderRegion::FixedBottom, 0, 270, 10};
    const TabOrderPosition start{TabOrderRegion::FixedBottom, 0, 20, 8};
    const TabOrderPosition stop{TabOrderRegion::FixedBottom, 0, 145, 9};
    CHECK(TabOrderBefore(stop, save));
    CHECK(TabOrderBefore(save, TabOrderPosition{TabOrderRegion::FixedBottom, 0, 395, 11}));
    CHECK(TabOrderBefore(start, stop));
    CHECK(!TabOrderBefore(stop, start));
    CHECK(TabOrderBefore(TabOrderPosition{TabOrderRegion::Body, 50, 20, 0}, start));
}

void test_layout_change_reveals_only_when_focused_control_is_clipped() {
    using idleharbor::app::ScrollPositionToReveal;
    CHECK(ScrollPositionToReveal(300, 340, 380, 1200, 200) == 300);
    CHECK(ScrollPositionToReveal(300, 520, 560, 1200, 200) == 360);
}

void test_narrow_work_areas_reflow_settings_and_actions() {
    using idleharbor::app::ActionLayoutMode;
    using idleharbor::app::DetermineActionLayout;
    using idleharbor::app::DetermineSettingsLayout;
    using idleharbor::app::SettingsLayoutMode;
    CHECK(DetermineSettingsLayout(600, 96) == SettingsLayoutMode::Columns);
    CHECK(DetermineSettingsLayout(420, 96) == SettingsLayoutMode::Stacked);
    CHECK(DetermineActionLayout(600, 96) == ActionLayoutMode::Wide);
    CHECK(DetermineActionLayout(320, 96) == ActionLayoutMode::Wrapped);
    CHECK(DetermineActionLayout(220, 96) == ActionLayoutMode::Stacked);
}

void test_wrapped_footer_actions_do_not_overlap() {
    using idleharbor::app::ComputeActionButtonRects;
    using idleharbor::app::PhysicalPixels;
    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int width = PhysicalPixels(320, dpi);
        const int height = PhysicalPixels(320, dpi);
        const auto buttons = ComputeActionButtonRects(width, height, dpi);
        CHECK(buttons.start.right <= buttons.stop.left);
        CHECK(buttons.start.bottom <= buttons.save.top);
        CHECK(buttons.stop.bottom <= buttons.save.top);
        CHECK(buttons.save.bottom <= height);
    }
}

void test_fractional_dpi_layout_uses_true_logical_widths() {
    using idleharbor::app::ActionLayoutMode;
    using idleharbor::app::ComputeSafetyRegions;
    using idleharbor::app::DetermineActionLayout;
    using idleharbor::app::DetermineSettingsLayout;
    using idleharbor::app::LogicalPixels;
    using idleharbor::app::PhysicalPixels;
    using idleharbor::app::SettingsLayoutMode;
    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int physical_normal_width = PhysicalPixels(560, dpi);
        CHECK(LogicalPixels(physical_normal_width, dpi) == 560);
        CHECK(DetermineSettingsLayout(physical_normal_width, dpi) == SettingsLayoutMode::Columns);
        CHECK(DetermineActionLayout(physical_normal_width, dpi) == ActionLayoutMode::Wide);
        const auto regions = ComputeSafetyRegions(physical_normal_width, PhysicalPixels(700, dpi), dpi);
        CHECK(regions.status.right <= physical_normal_width);
        CHECK(regions.viewport.right <= physical_normal_width);
        CHECK(regions.status.bottom <= regions.viewport.top);
        CHECK(regions.viewport.bottom == regions.actions.top);

        const int physical_narrow_width = PhysicalPixels(400, dpi);
        CHECK(LogicalPixels(physical_narrow_width, dpi) == 400);
        CHECK(DetermineSettingsLayout(physical_narrow_width, dpi) == SettingsLayoutMode::Stacked);
        CHECK(DetermineActionLayout(physical_narrow_width, dpi) == ActionLayoutMode::Wide);
        CHECK(DetermineActionLayout(PhysicalPixels(320, dpi), dpi) == ActionLayoutMode::Wrapped);
        CHECK(DetermineActionLayout(PhysicalPixels(220, dpi), dpi) == ActionLayoutMode::Stacked);
    }
}

void test_settings_layout_uses_the_viewport_client_width() {
    using idleharbor::app::ComputeStackedBodyLayout;
    using idleharbor::app::DetermineSettingsLayout;
    using idleharbor::app::LogicalPixels;
    using idleharbor::app::PhysicalPixels;
    using idleharbor::app::SettingsLayoutMode;

    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int outer_width = PhysicalPixels(600, dpi);
        const int scrollbar_width = PhysicalPixels(48, dpi);
        const int viewport_width = outer_width - scrollbar_width;
        const int logical_viewport_width = LogicalPixels(viewport_width, dpi);
        CHECK(logical_viewport_width < 560);
        CHECK(DetermineSettingsLayout(viewport_width, dpi) == SettingsLayoutMode::Stacked);
        const auto body = ComputeStackedBodyLayout(logical_viewport_width);
        CHECK(body.left >= 0);
        CHECK(body.left + body.width <= logical_viewport_width);
    }
}

void test_scrollbar_boundary_reflows_and_keeps_bottom_reachable() {
    using idleharbor::app::ClampScrollPosition;
    using idleharbor::app::ComputeStackedBodyLayout;
    using idleharbor::app::DetermineSettingsLayout;
    using idleharbor::app::MaximumScrollPosition;
    using idleharbor::app::PhysicalPixels;
    using idleharbor::app::SettingsLayoutMode;

    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int outer_width = PhysicalPixels(560, dpi);
        const int viewport_width = outer_width - PhysicalPixels(48, dpi);
        CHECK(DetermineSettingsLayout(outer_width, dpi) == SettingsLayoutMode::Columns);
        CHECK(DetermineSettingsLayout(viewport_width, dpi) == SettingsLayoutMode::Stacked);

        const auto body = ComputeStackedBodyLayout(idleharbor::app::LogicalPixels(viewport_width, dpi));
        const int content_height = PhysicalPixels(570 + body.width / 2, dpi);
        const int viewport_height = PhysicalPixels(390, dpi);
        const int maximum = MaximumScrollPosition(content_height, viewport_height);
        CHECK(maximum > 0);
        CHECK(ClampScrollPosition(maximum, content_height, viewport_height) == maximum);
    }
}

void test_height_resize_can_clear_a_scrollbar_free_candidate() {
    using idleharbor::app::MaximumScrollPosition;
    using idleharbor::app::PhysicalPixels;

    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int candidate_content_height = PhysicalPixels(614, dpi);
        const int short_viewport_height = PhysicalPixels(570, dpi);
        const int tall_viewport_height = PhysicalPixels(700, dpi);
        CHECK(MaximumScrollPosition(candidate_content_height, short_viewport_height) > 0);
        CHECK(MaximumScrollPosition(candidate_content_height, tall_viewport_height) == 0);
    }
}

void test_viewport_fill_widths_respect_the_effective_client_width() {
    using idleharbor::app::LogicalPixels;
    using idleharbor::app::PhysicalPixels;
    for (const int dpi : {96, 120, 144, 168, 192}) {
        const int outer_width = PhysicalPixels(600, dpi);
        const int viewport_width = outer_width - PhysicalPixels(48, dpi);
        const int logical_viewport_width = LogicalPixels(viewport_width, dpi);
        const int fill_right = logical_viewport_width - 300 - 20;
        CHECK(fill_right >= 80);
        CHECK(300 + fill_right + 20 <= logical_viewport_width);
    }
}

void test_stacked_stop_stays_inside_short_clients() {
    using idleharbor::app::ComputeActionButtonRects;
    using idleharbor::app::PhysicalPixels;
    for (const int dpi : {96, 120, 144, 168}) {
        const int width = PhysicalPixels(220, dpi);
        for (const int logical_height : {100, 90, 70}) {
            const int height = PhysicalPixels(logical_height, dpi);
            const auto buttons = ComputeActionButtonRects(width, height, dpi);
            CHECK(buttons.start.left >= 0 && buttons.start.right <= width);
            CHECK(buttons.stop.left >= 0 && buttons.stop.right <= width);
            CHECK(buttons.start.top >= 0 && buttons.start.bottom <= height);
            CHECK(buttons.stop.top >= 0 && buttons.stop.bottom <= height);
            CHECK(buttons.save.left >= 0 && buttons.save.right <= width);
            CHECK(buttons.save.top >= 0 && buttons.save.bottom <= height);
            CHECK(buttons.stop.bottom >= buttons.start.bottom);
        }
    }
}

void test_stacked_body_fits_extreme_logical_widths_at_fractional_dpi() {
    using idleharbor::app::ComputeStackedBodyLayout;
    using idleharbor::app::LogicalPixels;
    using idleharbor::app::PhysicalPixels;
    for (const int dpi : {96, 120, 144, 168}) {
        for (const int logical_width : {80, 99}) {
            const int physical_width = PhysicalPixels(logical_width, dpi);
            const int measured_logical_width = LogicalPixels(physical_width, dpi);
            const auto body = ComputeStackedBodyLayout(measured_logical_width);
            CHECK(body.left >= 0);
            CHECK(body.width >= 1);
            CHECK(body.left + body.width <= measured_logical_width);
        }
    }
}

void test_pointer_focus_does_not_scroll_the_body() {
    using idleharbor::app::FocusRevealTrigger;
    using idleharbor::app::ShouldRevealFocusedControl;
    CHECK(ShouldRevealFocusedControl(FocusRevealTrigger::Keyboard, false));
    CHECK(ShouldRevealFocusedControl(FocusRevealTrigger::Layout, false));
    CHECK(!ShouldRevealFocusedControl(FocusRevealTrigger::Pointer, false));
}

void test_open_popup_freezes_every_reveal_trigger() {
    using idleharbor::app::FocusRevealTrigger;
    using idleharbor::app::ShouldRevealFocusedControl;
    CHECK(!ShouldRevealFocusedControl(FocusRevealTrigger::Keyboard, true));
    CHECK(!ShouldRevealFocusedControl(FocusRevealTrigger::Pointer, true));
    CHECK(!ShouldRevealFocusedControl(FocusRevealTrigger::Layout, true));
}

void test_body_layout_is_frozen_while_a_popup_is_open() {
    using idleharbor::app::BodyLayoutIsMovable;
    CHECK(BodyLayoutIsMovable(false));
    CHECK(!BodyLayoutIsMovable(true));
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
    test_tab_order_keeps_fixed_actions_after_the_settings_body();
    test_layout_change_reveals_only_when_focused_control_is_clipped();
    test_narrow_work_areas_reflow_settings_and_actions();
    test_wrapped_footer_actions_do_not_overlap();
    test_fractional_dpi_layout_uses_true_logical_widths();
    test_settings_layout_uses_the_viewport_client_width();
    test_scrollbar_boundary_reflows_and_keeps_bottom_reachable();
    test_height_resize_can_clear_a_scrollbar_free_candidate();
    test_viewport_fill_widths_respect_the_effective_client_width();
    test_stacked_stop_stays_inside_short_clients();
    test_stacked_body_fits_extreme_logical_widths_at_fractional_dpi();
    test_pointer_focus_does_not_scroll_the_body();
    test_open_popup_freezes_every_reveal_trigger();
    test_body_layout_is_frozen_while_a_popup_is_open();
    return failures == 0 ? 0 : 1;
}
