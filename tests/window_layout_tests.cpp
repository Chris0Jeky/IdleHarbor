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

}  // namespace

int main() {
    test_window_is_clamped_inside_inset_work_area();
    test_oversized_window_uses_available_work_area();
    test_scroll_range_and_clamping();
    test_focus_reveal_scrolls_only_when_needed();
    return failures == 0 ? 0 : 1;
}
