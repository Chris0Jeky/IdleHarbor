#include "idleharbor/core.hpp"

#include <chrono>
#include <iostream>
#include <string>

namespace {

using idleharbor::core::ActiveHours;
using idleharbor::core::DecisionAction;
using idleharbor::core::EngineState;
using idleharbor::core::IntervalSampler;
using idleharbor::core::KeepAwakeMode;
using idleharbor::core::MotionMode;
using idleharbor::core::Point;
using idleharbor::core::PolicyEngine;
using idleharbor::core::PolicyInput;
using idleharbor::core::PolicyReason;
using idleharbor::core::ProfileKind;
using idleharbor::core::Seconds;
using idleharbor::core::Settings;

using namespace std::chrono_literals;

#define CHECK(condition)                                                                                 \
    do {                                                                                                 \
        if (!(condition)) {                                                                              \
            std::cerr << "FAILED: " << __FUNCTION__ << ": " << #condition << "\n";                  \
            return false;                                                                               \
        }                                                                                                \
    } while (false)

bool test_default_settings_validate() {
    const auto result = idleharbor::core::validate(Settings{});
    CHECK(result.valid);
    CHECK(result.errors.empty());
    CHECK(idleharbor::core::motion_mode_name(MotionMode::Circle) == "circle");
    CHECK(idleharbor::core::keep_awake_mode_name(KeepAwakeMode::Hybrid) == "hybrid");
    CHECK(idleharbor::core::profile_kind_name(ProfileKind::LongOperation) == "long-operation");
    CHECK(idleharbor::core::engine_state_name(EngineState::Paused) == "paused");
    return true;
}

bool test_invalid_settings_report_errors() {
    Settings settings{};
    settings.interval = 0s;
    settings.random_minimum = 2s;
    settings.distance = 121;
    settings.user_activity_cooldown = 0s;
    settings.low_battery_threshold = 101;
    settings.active_hours = ActiveHours{true, 1440, 10};
    settings.max_duration = -1s;

    const auto result = idleharbor::core::validate(settings);
    CHECK(!result.valid);
    CHECK(result.errors.size() == 7);
    return true;
}

bool test_motion_plans_are_deterministic_and_return_home() {
    for (const auto mode : {MotionMode::Normal, MotionMode::Linear, MotionMode::Circle, MotionMode::Zen}) {
        const auto first = idleharbor::core::make_motion_plan(mode, 12);
        const auto second = idleharbor::core::make_motion_plan(mode, 12);
        CHECK(first.mode == mode);
        CHECK(first.distance == 12);
        CHECK(first.relative_offsets == second.relative_offsets);
        CHECK(!first.relative_offsets.empty());
        CHECK(first.relative_offsets.back() == (Point{0, 0}));
        for (const auto point : first.relative_offsets) {
            CHECK(point.x <= 12 && point.x >= -12);
            CHECK(point.y <= 12 && point.y >= -12);
        }
        if (mode != MotionMode::Zen) {
            CHECK(first.relative_offsets.size() > 1);
        }
    }

    const auto clamped = idleharbor::core::make_motion_plan(MotionMode::Linear, 1000);
    CHECK(clamped.distance == Settings::kMaximumDistance);
    CHECK(clamped.relative_offsets.front() == (Point{120, 0}));
    return true;
}

bool test_interval_sampler_is_seeded_and_bounded() {
    IntervalSampler first(42, 1s, 10s, true);
    IntervalSampler second(42, 1s, 10s, true);
    for (int i = 0; i < 32; ++i) {
        const auto left = first.next();
        const auto right = second.next();
        CHECK(left == right);
        CHECK(left >= 1s && left <= 10s);
    }

    IntervalSampler fixed(7, 1s, 10s, false);
    CHECK(fixed.next() == 10s);
    IntervalSampler equal(7, 10s, 10s, true);
    CHECK(equal.next() == 10s);
    return true;
}

bool test_active_hours_support_disabled_normal_and_cross_midnight() {
    CHECK(ActiveHours{}.contains(0));
    const ActiveHours normal{true, 9 * 60, 17 * 60};
    CHECK(!normal.contains(8 * 60 + 59));
    CHECK(normal.contains(9 * 60));
    CHECK(normal.contains(16 * 60 + 59));
    CHECK(!normal.contains(17 * 60));

    const ActiveHours overnight{true, 22 * 60, 2 * 60};
    CHECK(overnight.contains(23 * 60));
    CHECK(overnight.contains(1 * 60));
    CHECK(!overnight.contains(12 * 60));
    CHECK(!overnight.contains(1440));
    return true;
}

bool test_policy_starts_and_stops_explicitly() {
    PolicyEngine policy(Settings{});
    auto decision = policy.evaluate(PolicyInput{0s});
    CHECK(decision.action == DecisionAction::Stop);
    CHECK(decision.state == EngineState::Stopped);
    CHECK(decision.reason == PolicyReason::Manual);

    policy.start(10s);
    decision = policy.evaluate(PolicyInput{10s});
    CHECK(decision.action == DecisionAction::Run);
    CHECK(decision.state == EngineState::Running);
    CHECK(policy.reason() == PolicyReason::None);

    policy.stop();
    decision = policy.evaluate(PolicyInput{11s});
    CHECK(decision.action == DecisionAction::Stop);
    CHECK(idleharbor::core::status_text(decision) == "Stopped: manually stopped");
    return true;
}

bool test_user_activity_cooldown_resumes() {
    Settings settings{};
    settings.user_activity_cooldown = 60s;
    PolicyEngine policy(settings);
    policy.start(0s);

    auto decision = policy.evaluate(PolicyInput{10s, true});
    CHECK(decision.state == EngineState::Paused);
    CHECK(decision.reason == PolicyReason::UserActivity);
    CHECK(decision.cooldown_remaining == 60s);
    CHECK(idleharbor::core::status_text(decision) == "Paused: user activity cooldown (60s remaining)");

    decision = policy.evaluate(PolicyInput{39s});
    CHECK(decision.cooldown_remaining == 31s);
    decision = policy.evaluate(PolicyInput{70s});
    CHECK(decision.state == EngineState::Running);
    CHECK(decision.reason == PolicyReason::None);
    return true;
}

bool test_policy_reason_priority_is_explicit() {
    Settings settings{};
    settings.pause_on_battery = true;
    settings.pause_on_low_battery = true;
    settings.pause_when_fullscreen = true;
    settings.active_hours = ActiveHours{true, 9 * 60, 17 * 60};
    PolicyEngine policy(settings);
    policy.start(0s);

    PolicyInput all_conditions{1s, true, true, true, true, 10, true, 2 * 60};
    auto decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::Locked);
    CHECK(idleharbor::core::reason_priority(PolicyReason::Locked) >
          idleharbor::core::reason_priority(PolicyReason::UserActivity));

    all_conditions.locked = false;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::Disconnected);
    all_conditions.disconnected = false;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::LowBattery);
    all_conditions.battery_percent = 50;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::OnBattery);
    all_conditions.on_battery = false;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::Fullscreen);
    all_conditions.fullscreen = false;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::OutsideActiveHours);
    all_conditions.minute_of_day = 10 * 60;
    decision = policy.evaluate(all_conditions);
    CHECK(decision.reason == PolicyReason::UserActivity);

    CHECK(idleharbor::core::reason_priority(PolicyReason::MaxDuration) >
          idleharbor::core::reason_priority(PolicyReason::Locked));
    return true;
}

bool test_optional_safeguards_are_opt_in() {
    Settings settings{};
    settings.pause_on_user_activity = false;
    settings.pause_when_locked = false;
    settings.pause_when_disconnected = false;
    settings.pause_on_battery = false;
    settings.pause_on_low_battery = false;
    settings.pause_when_fullscreen = false;
    PolicyEngine policy(settings);
    policy.start(0s);
    const auto decision = policy.evaluate(PolicyInput{1s, true, true, true, true, 1, true, 0});
    CHECK(decision.state == EngineState::Running);
    CHECK(decision.reason == PolicyReason::None);
    return true;
}

bool test_max_duration_stops_and_remains_stopped() {
    Settings settings{};
    settings.max_duration = 30s;
    PolicyEngine policy(settings);
    policy.start(100s);

    auto decision = policy.evaluate(PolicyInput{129s});
    CHECK(decision.state == EngineState::Running);
    decision = policy.evaluate(PolicyInput{130s});
    CHECK(decision.action == DecisionAction::Stop);
    CHECK(decision.reason == PolicyReason::MaxDuration);
    CHECK(idleharbor::core::status_text(decision) == "Stopped: maximum duration reached");
    decision = policy.evaluate(PolicyInput{1000s});
    CHECK(decision.state == EngineState::Stopped);
    CHECK(decision.reason == PolicyReason::MaxDuration);
    return true;
}

bool test_policy_can_resume_after_transient_safeguard() {
    Settings settings{};
    settings.pause_when_locked = true;
    PolicyEngine policy(settings);
    policy.start(0s);

    auto decision = policy.evaluate(PolicyInput{1s, false, true});
    CHECK(decision.reason == PolicyReason::Locked);
    decision = policy.evaluate(PolicyInput{2s});
    CHECK(decision.state == EngineState::Running);
    CHECK(decision.reason == PolicyReason::None);
    return true;
}

}  // namespace

int main() {
    const bool results[] = {
        test_default_settings_validate(),
        test_invalid_settings_report_errors(),
        test_motion_plans_are_deterministic_and_return_home(),
        test_interval_sampler_is_seeded_and_bounded(),
        test_active_hours_support_disabled_normal_and_cross_midnight(),
        test_policy_starts_and_stops_explicitly(),
        test_user_activity_cooldown_resumes(),
        test_policy_reason_priority_is_explicit(),
        test_optional_safeguards_are_opt_in(),
        test_max_duration_stops_and_remains_stopped(),
        test_policy_can_resume_after_transient_safeguard(),
    };

    for (const bool passed : results) {
        if (!passed) {
            return 1;
        }
    }
    std::cout << "All IdleHarbor core tests passed\n";
    return 0;
}
