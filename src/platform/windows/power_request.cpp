#include "idleharbor/platform/windows/power_request.hpp"

namespace idleharbor::platform::windows {

PowerRequest::~PowerRequest() {
    Clear();
}

bool PowerRequest::Apply(const PowerRequestMode mode) noexcept {
    if (mode == PowerRequestMode::None) {
        Clear();
        return true;
    }

    EXECUTION_STATE flags = ES_CONTINUOUS | ES_SYSTEM_REQUIRED;
    if (mode == PowerRequestMode::Display) {
        flags |= ES_DISPLAY_REQUIRED;
    }

    if (SetThreadExecutionState(flags) == 0) {
        return false;
    }
    mode_ = mode;
    return true;
}

void PowerRequest::Clear() noexcept {
    if (mode_ != PowerRequestMode::None) {
        SetThreadExecutionState(ES_CONTINUOUS);
        mode_ = PowerRequestMode::None;
    }
}

}  // namespace idleharbor::platform::windows
