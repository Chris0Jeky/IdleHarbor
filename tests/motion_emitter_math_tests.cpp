#include <windows.h>

#include <array>
#include <iostream>
#include <string_view>

#include "idleharbor/platform/windows/motion_emitter.hpp"

namespace {

int failures = 0;

void Expect(const bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using idleharbor::platform::windows::ChooseSafeAnchor;
    using idleharbor::platform::windows::NormalizeAbsoluteCoordinate;

    Expect(NormalizeAbsoluteCoordinate(-1920, -1920, 3840) == 0, "virtual left maps to zero");
    Expect(NormalizeAbsoluteCoordinate(1919, -1920, 3840) == 65'535, "virtual right maps to maximum");
    Expect(NormalizeAbsoluteCoordinate(-3000, -1920, 3840) == 0, "coordinates clamp below virtual screen");
    Expect(NormalizeAbsoluteCoordinate(3000, -1920, 3840) == 65'535, "coordinates clamp above virtual screen");

    constexpr RECT work_area{0, 0, 1920, 1080};
    constexpr std::array<POINT, 4> circle{{{10, 0}, {0, 10}, {-10, 0}, {0, 0}}};
    const auto centered = ChooseSafeAnchor({500, 500}, work_area, circle);
    Expect(centered.x == 500 && centered.y == 500, "safe origin remains unchanged");

    const auto right_edge = ChooseSafeAnchor({1919, 500}, work_area, circle);
    Expect(right_edge.x == 1909 && right_edge.y == 500, "anchor shifts inward at right edge");

    const auto left_edge = ChooseSafeAnchor({0, 500}, work_area, circle);
    Expect(left_edge.x == 10 && left_edge.y == 500, "anchor shifts inward at left edge");

    const auto corner = ChooseSafeAnchor({1919, 1079}, work_area, circle);
    Expect(corner.x == 1909 && corner.y == 1069, "anchor shifts inward at corner");

    if (failures != 0) {
        std::cerr << failures << " motion-emitter math test(s) failed\n";
        return 1;
    }
    return 0;
}
