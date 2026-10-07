#include "idleharbor/core.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
int failures = 0;
void Expect(bool condition, std::string_view label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
}
}
int main() {
    using namespace idleharbor::core;
    using namespace std::chrono_literals;
    const std::array<std::pair<Seconds, Seconds>, 6> invalid{{
        {2s, 1s}, {0s, 1s}, {-1s, 5s}, {1s, 0s}, {-2s, -1s},
        {Seconds{std::numeric_limits<Seconds::rep>::min()}, 1s},
    }};
    for (const bool random : {false, true}) {
        for (const auto& [minimum, maximum] : invalid) {
            bool rejected = false;
            try { IntervalSampler sampler(123, minimum, maximum, random); (void)sampler.next(); }
            catch (const std::invalid_argument&) { rejected = true; }
            Expect(rejected, "nonpositive or reversed interval bounds are rejected");
        }
        IntervalSampler equal(42, 1s, 1s, random);
        Expect(equal.next() == 1s, "equal positive bounds remain valid");
    }
    IntervalSampler first(42, 1s, 10s, true), second(42, 1s, 10s, true);
    IntervalSampler fixed(42, 1s, 10s, false);
    for (int i = 0; i < 100; ++i) {
        const auto sample = first.next();
        Expect(sample >= 1s && sample <= 10s, "random samples stay within bounds");
        Expect(sample == second.next(), "same seed remains deterministic");
        Expect(fixed.next() == 10s, "fixed sampling still selects maximum");
    }

    const std::array<std::pair<PolicyReason, std::string_view>, 7> paused{{
        {PolicyReason::UserActivity, "user activity cooldown"},
        {PolicyReason::Locked, "workstation locked"},
        {PolicyReason::Disconnected, "session disconnected"},
        {PolicyReason::LowBattery, "low battery"},
        {PolicyReason::OnBattery, "on battery power"},
        {PolicyReason::Fullscreen, "fullscreen activity"},
        {PolicyReason::OutsideActiveHours, "outside active hours"},
    }};
    for (const auto& [reason, text] : paused) {
        const std::string expected = "Paused: " + std::string(text);
        Expect(status_text({DecisionAction::Pause, EngineState::Paused, reason, 0s}) == expected,
               "each safety-pause reason has its own status");
        const auto timed = status_text({DecisionAction::Pause, EngineState::Paused, reason, 5s});
        Expect(timed == expected + (reason == PolicyReason::UserActivity ? " (5s remaining)" : ""),
               "only user-activity status carries a cooldown");
    }
    Expect(status_text({DecisionAction::Pause, EngineState::Paused, PolicyReason::UserActivity, -1s}) ==
           "Paused: user activity cooldown", "negative cooldown is not displayed");
    Expect(status_text({DecisionAction::Run, EngineState::Running, PolicyReason::None, 0s}) == "Running", "running status");
    Expect(status_text({DecisionAction::Stop, EngineState::Stopped, PolicyReason::None, 0s}) == "Stopped", "plain stopped status");
    Expect(status_text({DecisionAction::Stop, EngineState::Stopped, PolicyReason::Manual, 0s}) == "Stopped: manually stopped", "manual stopped status");
    Expect(status_text({DecisionAction::Stop, EngineState::Stopped, PolicyReason::MaxDuration, 0s}) == "Stopped: maximum duration reached", "duration stopped status");
    if (failures) return 1;
    std::cout << "Interval bounds and all safety-pause status contracts passed.\n";
}
