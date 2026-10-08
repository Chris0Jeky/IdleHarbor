#include "idleharbor/core.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>

namespace {
using namespace idleharbor::core;
using namespace std::chrono_literals;
int assertions = 0;
int failures = 0;
void Check(bool condition, std::string_view message) {
    ++assertions;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

template<class Engine>
void Controls() {
    // Keep the missing-feature baseline executable, not a compiler-error test.
    if constexpr (!requires(Engine& e) {
        e.pause(0s, 1s); e.resume(); e.extend_duration(0s, 1s);
        e.remaining_duration(0s); e.manually_paused();
    }) {
        Check(false, "PolicyEngine exposes session-only pause, snooze, extension and countdown");
    } else {
        Settings settings;
        settings.max_duration = 1h;
        Engine e(settings);
        Check(!e.pause(0s, 1s), "cannot pause a stopped session");
        Check(!e.resume(), "cannot resume a stopped session");
        Check(!e.extend_duration(0s, 1s), "cannot extend a stopped session");
        Check(e.remaining_duration(0s) == 0s, "stopped timed session has no remaining time");
        e.start(100s);
        Check(e.remaining_duration(100s) == 1h, "countdown begins at the selected duration");
        Check(e.remaining_duration(99s) == 1h, "backward time does not increase the budget");
        Check(e.remaining_duration(101s) == 3599s, "countdown reflects elapsed time");
        Check(e.pause(110s, 0s), "indefinite manual pause is accepted");
        Check(e.manually_paused(), "manual pause is observable");
        auto result = e.evaluate(PolicyInput{.now=115s});
        Check(result.action == DecisionAction::Pause, "manual pause prevents Run");
        Check(policy_reason_name(result.reason) == "manual pause", "manual pause is explained");
        Check(result.cooldown_remaining == 0s, "indefinite pause has no fake deadline");
        Check(status_text(result) == "Paused: manual pause", "manual pause has explicit status");
        Check(e.remaining_duration(115s) == 3585s, "session expiry continues during a manual pause");
        Check(e.resume(), "manual pause can be cleared");
        Check(!e.manually_paused(), "resume clears only the manual hold");
        Check(e.evaluate(PolicyInput{.now=116s,.locked=true}).reason == PolicyReason::Locked,
              "resume cannot bypass the lock safeguard");
        Check(e.evaluate(PolicyInput{.now=117s,.disconnected=true}).reason == PolicyReason::Disconnected,
              "resume cannot bypass the disconnect safeguard");
        Check(e.evaluate(PolicyInput{.now=118s,.on_battery=true,.battery_percent=10}).reason == PolicyReason::LowBattery,
              "resume cannot bypass low-battery protection");
        Check(e.evaluate(PolicyInput{.now=119s}).action == DecisionAction::Run, "clear safeguards allow resumed work");
        Check(!e.resume(), "redundant resume does not create a new session");

        Check(e.pause(120s, 5min), "five-minute snooze is accepted");
        Check(e.evaluate(PolicyInput{.now=419s}).cooldown_remaining == 1s, "snooze counts down to one second");
        Check(e.evaluate(PolicyInput{.now=420s}).action == DecisionAction::Run, "snooze expires at the exact boundary");
        Check(!e.manually_paused(), "expired snooze clears the manual hold");
        Check(e.pause(500s, 5min), "snooze can be started again");
        (void)e.evaluate(PolicyInput{.now=790s,.user_activity=true});
        Check(e.evaluate(PolicyInput{.now=800s}).reason == PolicyReason::UserActivity,
              "genuine activity during snooze still delays automatic resume");
        Check(e.evaluate(PolicyInput{.now=850s}).action == DecisionAction::Run, "activity cooldown is preserved exactly");

        Check(e.pause(900s, 5min), "snooze can be replaced");
        Check(e.pause(910s, 10s), "explicit resnooze resets only the snooze deadline");
        Check(e.evaluate(PolicyInput{.now=919s}).cooldown_remaining == 1s, "replacement snooze uses its new deadline");
        Check(e.evaluate(PolicyInput{.now=920s}).action == DecisionAction::Run, "replacement snooze expires");
        Check(!e.pause(921s, -1s), "negative snooze is rejected");
        Check(!e.pause(921s, 25h), "snooze longer than one day is rejected");
        Check(!e.manually_paused(), "invalid pause arguments do not change the session");
        Check(e.pause(925s, 0s), "pause before extension");
        const auto before = e.remaining_duration(930s);
        Check(e.extend_duration(930s, 15min), "timed session accepts extension");
        Check(e.remaining_duration(930s) == *before + 15min, "extension adds exactly the requested time");
        Check(e.manually_paused(), "extension cannot clear a manual pause");
        Check(!e.extend_duration(930s, 0s), "zero extension is rejected");
        Check(!e.extend_duration(930s, -1s), "negative extension is rejected");
        Check(!e.extend_duration(930s, 720h), "extension cannot exceed the thirty-day total");
        Check(e.remaining_duration(930s) == *before + 15min, "invalid extensions preserve the deadline");
        e.stop();
        Check(!e.manually_paused(), "Stop clears manual pause state");
        Check(e.remaining_duration(940s) == 0s, "Stop clears remaining time");
        Check(!e.resume(), "Resume never restarts a stopped session");
        e.start(1000s);
        Check(!e.manually_paused(), "Start clears previous manual pause");
        // The object is a single session configuration; the application makes a
        // fresh one for each new start to avoid persisting runtime extensions.
        Check(e.remaining_duration(1000s) == 75min, "engine restart uses its current session configuration");

        Settings brief; brief.max_duration = 10s;
        Engine expires(brief); expires.start(0s);
        Check(expires.pause(0s, 20s), "snooze may be longer than the session");
        result = expires.evaluate(PolicyInput{.now=10s,.user_activity=true,.locked=true});
        Check(result.action == DecisionAction::Stop && result.reason == PolicyReason::MaxDuration,
              "maximum duration outranks manual pause and every safeguard");
        Check(!expires.manually_paused(), "expiry removes the manual hold");
        Check(!expires.extend_duration(11s, 1min), "extension cannot resurrect an expired session");
        Engine late(brief); late.start(0s);
        Check(!late.extend_duration(10s, 1min), "extension checks expiry even before the next timer tick");
        Check(late.reason() == PolicyReason::MaxDuration, "late extension records expiry");
        Engine late_pause(brief); late_pause.start(0s);
        Check(!late_pause.pause(10s, 1s), "pause checks expiry before the next timer tick");
        Check(late_pause.reason() == PolicyReason::MaxDuration, "late pause records expiry");

        Engine unlimited(Settings{}); unlimited.start(0s);
        Check(!unlimited.remaining_duration(10s).has_value(), "unlimited session is not shown as zero seconds");
        Check(!unlimited.extend_duration(10s, 1min), "unlimited session cannot be accidentally given a deadline");
        Check(unlimited.pause(10s, 1s), "unlimited session supports snooze");
        Check(unlimited.evaluate(PolicyInput{.now=11s}).action == DecisionAction::Run, "unlimited snooze resumes normally");

        Settings limit; limit.max_duration = 720h - 1s;
        Engine bounded(limit); bounded.start(0s);
        Check(bounded.extend_duration(0s, 1s), "exact thirty-day total is accepted");
        Check(!bounded.extend_duration(0s, 1s), "thirty-day bound cannot be crossed");
        Check(!bounded.extend_duration(0s, Seconds::max()), "huge extension is rejected without overflow");

        Engine huge(brief); huge.start(Seconds::min());
        Check(huge.remaining_duration(Seconds::max()) == 0s, "extreme elapsed values saturate safely");
        Check(huge.evaluate(PolicyInput{.now=Seconds::max()}).reason == PolicyReason::MaxDuration,
              "extreme elapsed values still expire a bounded session");
    }
}
}
int main() {
    Controls<PolicyEngine>();
    if (failures) { std::cerr << failures << " of " << assertions << " session-control assertions failed.\n"; return 1; }
    std::cout << "Session controls passed (" << assertions << " assertions).\n";
}
