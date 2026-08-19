#include "idleharbor/platform/windows/motion_emitter.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "idleharbor/platform/windows/input_monitor.hpp"

namespace idleharbor::platform::windows {
namespace {

INPUT AbsoluteMove(const POINT point, const RECT& virtual_screen) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = NormalizeAbsoluteCoordinate(point.x, virtual_screen.left, virtual_screen.right - virtual_screen.left);
    input.mi.dy = NormalizeAbsoluteCoordinate(point.y, virtual_screen.top, virtual_screen.bottom - virtual_screen.top);
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK |
                       MOUSEEVENTF_MOVE_NOCOALESCE;
    input.mi.dwExtraInfo = kIdleHarborInputMarker;
    return input;
}

MotionEmissionResult Send(std::vector<INPUT>& inputs) noexcept {
    MotionEmissionResult result;
    if (inputs.empty() || inputs.size() > static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }

    result.requested_events = static_cast<UINT>(inputs.size());
    SetLastError(ERROR_SUCCESS);
    result.emitted_events = SendInput(result.requested_events, inputs.data(), sizeof(INPUT));
    result.succeeded = result.emitted_events == result.requested_events;
    if (!result.succeeded) {
        result.error = GetLastError();
        if (result.error == ERROR_SUCCESS) {
            result.error = ERROR_ACCESS_DENIED;
        }
    }
    return result;
}

}  // namespace

LONG NormalizeAbsoluteCoordinate(
    const int coordinate,
    const int virtual_origin,
    const int virtual_length) noexcept {
    if (virtual_length <= 1) {
        return 0;
    }

    const auto maximum_offset = static_cast<std::int64_t>(virtual_length - 1);
    const auto offset = std::clamp(
        static_cast<std::int64_t>(coordinate) - static_cast<std::int64_t>(virtual_origin),
        std::int64_t{0},
        maximum_offset);
    return static_cast<LONG>((offset * 65'535 + maximum_offset / 2) / maximum_offset);
}

POINT ChooseSafeAnchor(
    const POINT origin,
    const RECT& work_area,
    const std::span<const POINT> offsets) noexcept {
    if (offsets.empty() || work_area.right <= work_area.left || work_area.bottom <= work_area.top) {
        return origin;
    }

    LONG minimum_x = 0;
    LONG maximum_x = 0;
    LONG minimum_y = 0;
    LONG maximum_y = 0;
    for (const POINT offset : offsets) {
        minimum_x = std::min(minimum_x, offset.x);
        maximum_x = std::max(maximum_x, offset.x);
        minimum_y = std::min(minimum_y, offset.y);
        maximum_y = std::max(maximum_y, offset.y);
    }

    const LONG minimum_anchor_x = work_area.left - minimum_x;
    const LONG maximum_anchor_x = (work_area.right - 1) - maximum_x;
    const LONG minimum_anchor_y = work_area.top - minimum_y;
    const LONG maximum_anchor_y = (work_area.bottom - 1) - maximum_y;

    POINT anchor = origin;
    if (minimum_anchor_x <= maximum_anchor_x) {
        anchor.x = std::clamp(anchor.x, minimum_anchor_x, maximum_anchor_x);
    }
    if (minimum_anchor_y <= maximum_anchor_y) {
        anchor.y = std::clamp(anchor.y, minimum_anchor_y, maximum_anchor_y);
    }
    return anchor;
}

MotionEmissionResult EmitMotionPulse(const std::span<const POINT> offsets) noexcept {
    if (offsets.empty()) {
        return {.succeeded = false, .error = ERROR_INVALID_PARAMETER};
    }

    POINT origin{};
    if (GetCursorPos(&origin) == FALSE) {
        return {.succeeded = false, .error = GetLastError()};
    }

    MONITORINFO monitor{sizeof(monitor)};
    const HMONITOR monitor_handle = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    if (monitor_handle == nullptr || GetMonitorInfoW(monitor_handle, &monitor) == FALSE) {
        return {.succeeded = false, .error = GetLastError()};
    }

    const RECT virtual_screen{
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN),
    };
    if (virtual_screen.right <= virtual_screen.left || virtual_screen.bottom <= virtual_screen.top) {
        return {.succeeded = false, .error = ERROR_INVALID_DATA};
    }

    const POINT anchor = ChooseSafeAnchor(origin, monitor.rcWork, offsets);
    std::vector<INPUT> inputs;
    inputs.reserve(offsets.size() + 2);
    if (anchor.x != origin.x || anchor.y != origin.y) {
        inputs.push_back(AbsoluteMove(anchor, virtual_screen));
    }

    for (const POINT offset : offsets) {
        const POINT target{
            std::clamp(anchor.x + offset.x, monitor.rcWork.left, monitor.rcWork.right - 1),
            std::clamp(anchor.y + offset.y, monitor.rcWork.top, monitor.rcWork.bottom - 1),
        };
        inputs.push_back(AbsoluteMove(target, virtual_screen));
    }

    if (anchor.x + offsets.back().x != origin.x || anchor.y + offsets.back().y != origin.y) {
        inputs.push_back(AbsoluteMove(origin, virtual_screen));
    }

    POINT current{};
    if (GetCursorPos(&current) == FALSE) {
        return {.succeeded = false, .error = GetLastError()};
    }
    if (current.x != origin.x || current.y != origin.y) {
        // Genuine movement won the race while the pulse was being prepared. Skip the
        // pulse rather than returning the pointer to a stale captured position.
        return {.succeeded = true};
    }
    auto result = Send(inputs);
    if (!result.succeeded && result.emitted_events > 0) {
        // SendInput can theoretically accept only a prefix. Make one bounded cleanup
        // attempt so an accepted anchor/path event does not leave the pointer displaced.
        std::vector<INPUT> restoration{AbsoluteMove(origin, virtual_screen)};
        result.cleanup_attempted = true;
        const auto cleanup = Send(restoration);
        result.cleanup_succeeded = cleanup.succeeded;
        if (!cleanup.succeeded) {
            result.cleanup_error = cleanup.error;
        }
    }
    return result;
}

MotionEmissionResult EmitZenPulse() noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;
    input.mi.dwExtraInfo = kIdleHarborInputMarker;

    std::vector<INPUT> inputs{input};
    return Send(inputs);
}

}  // namespace idleharbor::platform::windows
