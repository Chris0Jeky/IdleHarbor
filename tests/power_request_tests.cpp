#include "idleharbor/platform/windows/power_request.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace {
using idleharbor::platform::windows::PowerRequest;
using idleharbor::platform::windows::PowerRequestMode;
std::vector<EXECUTION_STATE> calls;
EXECUTION_STATE reply = ES_CONTINUOUS;
int failures = 0;
void Expect(bool condition, std::string_view label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
void Reset() { calls.clear(); reply = ES_CONTINUOUS; }
}

// Only this test executable resolves the production call to this deterministic
// boundary double. The real application still includes/links the Windows API.
EXECUTION_STATE SetThreadExecutionState(EXECUTION_STATE flags) {
    calls.push_back(flags);
    return reply;
}

int main() {
    Reset();
    {
        PowerRequest request;
        Expect(!request.active(), "initial request is inactive");
        Expect(request.Apply(PowerRequestMode::None), "clearing inactive request succeeds");
        request.Clear();
        Expect(calls.empty(), "inactive clear never calls the OS");
    }
    Expect(calls.empty(), "inactive destruction never calls the OS");

    for (const auto mode : {PowerRequestMode::System, PowerRequestMode::Display}) {
        Reset();
        {
            PowerRequest request;
            Expect(request.Apply(mode), "apply succeeds");
            const auto expected = ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                (mode == PowerRequestMode::Display ? ES_DISPLAY_REQUIRED : 0u);
            Expect(calls == std::vector<EXECUTION_STATE>{expected}, "apply flags match the selected mode");
            Expect(request.mode() == mode && request.active(), "successful apply records its state");

            reply = 0;
            request.Clear();
            Expect(request.mode() == mode && request.active(), "failed clear preserves the outstanding request");
            Expect(!request.Apply(PowerRequestMode::None), "Apply(None) propagates failed clear");
            Expect(request.mode() == mode && request.active(), "repeated clear failure remains retryable");
            Expect(calls.size() == 3 && calls[1] == ES_CONTINUOUS && calls[2] == ES_CONTINUOUS,
                   "each failed clear retries only release flags");

            reply = ES_CONTINUOUS;
            Expect(request.Apply(PowerRequestMode::None), "release can recover after failure");
            Expect(!request.active() && request.mode() == PowerRequestMode::None, "successful retry clears tracked state");
            Expect(calls.size() == 4 && calls.back() == ES_CONTINUOUS, "successful retry reaches the OS");
            request.Clear();
            Expect(calls.size() == 4, "successful clear becomes idempotent");
        }
        Expect(calls.size() == 4, "released destruction does not issue extra calls");
    }

    Reset();
    {
        PowerRequest request;
        reply = 0;
        Expect(!request.Apply(PowerRequestMode::Display), "failed initial apply is reported");
        Expect(!request.active(), "failed initial apply does not claim activity");
        reply = ES_CONTINUOUS;
        Expect(request.Apply(PowerRequestMode::System), "apply recovers");
        reply = 0;
        Expect(!request.Apply(PowerRequestMode::Display), "failed mode transition is reported");
        Expect(request.mode() == PowerRequestMode::System, "failed transition preserves earlier request");
        request.Clear();
        reply = ES_CONTINUOUS;
    }
    Expect(calls.size() == 5 && calls.back() == ES_CONTINUOUS, "destructor retries outstanding failed cleanup");
    if (failures) return 1;
    std::cout << "Power-request failure, recovery and cleanup checks passed.\n";
}
