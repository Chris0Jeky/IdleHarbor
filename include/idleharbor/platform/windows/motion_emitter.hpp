#pragma once

#include <windows.h>

#include <span>

namespace idleharbor::platform::windows {

struct MotionEmissionResult {
    bool succeeded = false;
    bool cleanup_attempted = false;
    bool cleanup_succeeded = true;
    DWORD error = ERROR_SUCCESS;
    DWORD cleanup_error = ERROR_SUCCESS;
    UINT requested_events = 0;
    UINT emitted_events = 0;
};

[[nodiscard]] LONG NormalizeAbsoluteCoordinate(int coordinate, int virtual_origin, int virtual_length) noexcept;
[[nodiscard]] POINT ChooseSafeAnchor(POINT origin, const RECT& work_area, std::span<const POINT> offsets) noexcept;

// Offsets are cumulative path points relative to a safe anchor and should finish at {0, 0}.
// IdleHarbor moves to the safe anchor, emits the path, and returns exactly to the captured origin.
// A pulse is skipped if genuine movement changes that origin while the input batch is prepared.
[[nodiscard]] MotionEmissionResult EmitMotionPulse(std::span<const POINT> offsets) noexcept;
[[nodiscard]] MotionEmissionResult EmitZenPulse() noexcept;

}  // namespace idleharbor::platform::windows
