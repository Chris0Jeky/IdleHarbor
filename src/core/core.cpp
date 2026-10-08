#include "idleharbor/core.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace idleharbor::core {
namespace {

void add_error(ValidationResult& result, std::string error) {
    result.valid = false;
    result.errors.push_back(std::move(error));
}

Seconds nonnegative_difference(Seconds later, Seconds earlier) noexcept {
    if (later < earlier) {
        return Seconds{0};
    }
    // Saturate instead of overflowing when a caller supplies extreme epochs.
    if (earlier < Seconds{0} && later > Seconds::max() + earlier) {
        return Seconds::max();
    }
    return later - earlier;
}

PolicyDecision pause_decision(PolicyReason reason, Seconds cooldown = Seconds{0}) noexcept {
    return PolicyDecision{DecisionAction::Pause, EngineState::Paused, reason, cooldown};
}

}  // namespace

bool ActiveHours::contains(std::uint16_t minute_of_day) const noexcept {
    if (!enabled) {
        return true;
    }
    if (minute_of_day >= 24 * 60) {
        return false;
    }
    if (start_minute < end_minute) {
        return minute_of_day >= start_minute && minute_of_day < end_minute;
    }
    if (start_minute > end_minute) {
        return minute_of_day >= start_minute || minute_of_day < end_minute;
    }
    return true;
}

ValidationResult validate(const Settings& settings) {
    ValidationResult result{};

    if (settings.motion == MotionMode::Off && settings.power == PowerMode::None) {
        add_error(result, "motion Off with power None cannot keep the system awake");
    }
    if (settings.interval < Settings::kMinimumInterval || settings.interval > Settings::kMaximumInterval) {
        add_error(result, "interval must be between 1 second and 24 hours");
    }
    if (settings.random_minimum < Settings::kMinimumInterval || settings.random_minimum > settings.interval) {
        add_error(result, "random minimum must be between 1 second and the interval");
    }
    if (settings.distance < Settings::kMinimumDistance || settings.distance > Settings::kMaximumDistance) {
        add_error(result, "motion size must be between 1 and 120");
    }
    if (settings.user_activity_cooldown < Settings::kMinimumInterval ||
        settings.user_activity_cooldown > Settings::kMaximumInterval) {
        add_error(result, "user activity cooldown must be between 1 second and 24 hours");
    }
    if (settings.low_battery_threshold > 100) {
        add_error(result, "low battery threshold must be between 0 and 100 percent");
    }
    if (settings.active_hours.enabled &&
        (settings.active_hours.start_minute >= 24 * 60 || settings.active_hours.end_minute >= 24 * 60)) {
        add_error(result, "active-hours endpoints must be within the day");
    }
    if (settings.max_duration < Seconds{0} || settings.max_duration > Settings::kMaximumDuration) {
        add_error(result, "maximum duration must be between 0 and 30 days");
    }

    return result;
}

Settings settings_for_profile(ProfileKind profile) {
    Settings settings{};
    settings.profile = profile;

    switch (profile) {
    case ProfileKind::Balanced:
        // Zen/System at 60 seconds; user, lock, disconnect, and low-battery safeguards stay on.
        break;
    case ProfileKind::LongTask:
        // A bounded system-only request avoids synthetic input during a long local operation.
        settings.motion = MotionMode::Off;
        settings.interval = Seconds{120};
        settings.max_duration = Seconds{4 * 60 * 60};
        break;
    case ProfileKind::Presentation:
        // Do not move the pointer, but keep the display on for a presentation.
        settings.motion = MotionMode::Off;
        settings.power = PowerMode::Display;
        settings.pause_on_user_activity = false;
        break;
    case ProfileKind::Compatibility:
        // Use visible Normal input for applications that implement their own idle detection.
        settings.motion = MotionMode::Normal;
        settings.power = PowerMode::None;
        settings.interval = Seconds{60};
        break;
    case ProfileKind::Visible:
        // Circle movement makes activity obvious; it does not request display power.
        settings.motion = MotionMode::Circle;
        settings.power = PowerMode::None;
        break;
    case ProfileKind::BatterySaver:
        // Zen input avoids a continuous power request and pauses before the battery becomes critical.
        settings.motion = MotionMode::Zen;
        settings.power = PowerMode::None;
        settings.interval = Seconds{120};
        settings.random_minimum = Seconds{30};
        settings.randomize = true;
        settings.low_battery_threshold = 30;
        break;
    case ProfileKind::Custom:
        // Custom starts from the balanced preset and is intended for caller overrides.
        settings = Settings{};
        settings.profile = ProfileKind::Custom;
        break;
    }

    return settings;
}

std::string_view motion_mode_name(MotionMode mode) noexcept {
    switch (mode) {
    case MotionMode::Off:
        return "off";
    case MotionMode::Normal:
        return "normal";
    case MotionMode::Zen:
        return "zen";
    case MotionMode::Circle:
        return "circle";
    case MotionMode::Linear:
        return "linear";
    }
    return "unknown";
}

std::string_view power_mode_name(PowerMode mode) noexcept {
    switch (mode) {
    case PowerMode::None:
        return "none";
    case PowerMode::System:
        return "system";
    case PowerMode::Display:
        return "display";
    }
    return "unknown";
}

std::string_view profile_kind_name(ProfileKind profile) noexcept {
    switch (profile) {
    case ProfileKind::Balanced:
        return "balanced";
    case ProfileKind::LongTask:
        return "long-task";
    case ProfileKind::Presentation:
        return "presentation";
    case ProfileKind::Compatibility:
        return "compatibility";
    case ProfileKind::Visible:
        return "visible";
    case ProfileKind::BatterySaver:
        return "battery-saver";
    case ProfileKind::Custom:
        return "custom";
    }
    return "unknown";
}

std::string_view engine_state_name(EngineState state) noexcept {
    switch (state) {
    case EngineState::Stopped:
        return "stopped";
    case EngineState::Running:
        return "running";
    case EngineState::Paused:
        return "paused";
    }
    return "unknown";
}

std::string_view policy_reason_name(PolicyReason reason) noexcept {
    switch (reason) {
    case PolicyReason::None:
        return "none";
    case PolicyReason::Manual:
        return "manually stopped";
    case PolicyReason::UserActivity:
        return "user activity cooldown";
    case PolicyReason::Locked:
        return "workstation locked";
    case PolicyReason::Disconnected:
        return "session disconnected";
    case PolicyReason::LowBattery:
        return "low battery";
    case PolicyReason::OnBattery:
        return "on battery power";
    case PolicyReason::Fullscreen:
        return "fullscreen activity";
    case PolicyReason::OutsideActiveHours:
        return "outside active hours";
    case PolicyReason::MaxDuration:
        return "maximum duration reached";
    case PolicyReason::ManualPause:
        return "manual pause";
    }
    return "unknown";
}

int reason_priority(PolicyReason reason) noexcept {
    switch (reason) {
    case PolicyReason::MaxDuration:
        return 100;
    case PolicyReason::ManualPause:
        return 95;
    case PolicyReason::Locked:
        return 90;
    case PolicyReason::Disconnected:
        return 80;
    case PolicyReason::LowBattery:
        return 70;
    case PolicyReason::OnBattery:
        return 60;
    case PolicyReason::Fullscreen:
        return 50;
    case PolicyReason::OutsideActiveHours:
        return 40;
    case PolicyReason::UserActivity:
        return 30;
    case PolicyReason::Manual:
        return 20;
    case PolicyReason::None:
        return 0;
    }
    return 0;
}

MotionPlan make_motion_plan(MotionMode mode, std::uint32_t distance) {
    const auto bounded_distance = std::min(distance, Settings::kMaximumDistance);
    const auto d = static_cast<int>(bounded_distance);
    MotionPlan plan{mode, bounded_distance, {}};

    // IdleHarbor-owned cumulative paths. Every visible path is bounded and ends at
    // the safe anchor so the emitter can restore the user's captured cursor position.
    switch (mode) {
    case MotionMode::Off:
        plan.relative_offsets = {};
        break;
    case MotionMode::Normal:
        plan.relative_offsets = {{3 * d, 2 * d}, {1 * d, 4 * d}, {-2 * d, 1 * d}, {0, 0}};
        break;
    case MotionMode::Linear:
        plan.relative_offsets = {{3 * d, 0}, {-3 * d, 0}, {0, 0}};
        break;
    case MotionMode::Circle: {
        plan.relative_offsets = {
            {2 * d, -1 * d}, {4 * d, 1 * d}, {4 * d, 4 * d}, {2 * d, 6 * d},
            {-1 * d, 6 * d}, {-4 * d, 4 * d}, {-4 * d, 1 * d}, {-2 * d, -1 * d}, {0, 0},
        };
        break;
    }
    case MotionMode::Zen:
        plan.relative_offsets = {{0, 0}};
        break;
    }

    return plan;
}

IntervalSampler::IntervalSampler(
    std::uint64_t seed,
    Seconds minimum,
    Seconds maximum,
    bool randomized)
    : minimum_(minimum), maximum_(maximum), randomized_(randomized), generator_(seed) {
    if (minimum <= Seconds{0} || minimum > maximum) {
        throw std::invalid_argument("interval bounds must be positive and ordered");
    }
}

Seconds IntervalSampler::next() {
    if (!randomized_ || minimum_ >= maximum_) {
        return maximum_;
    }
    const auto minimum = minimum_.count();
    const auto maximum = maximum_.count();
    std::uniform_int_distribution<std::int64_t> distribution(minimum, maximum);
    return Seconds{distribution(generator_)};
}

PolicyEngine::PolicyEngine(Settings settings) : settings_(std::move(settings)) {}

void PolicyEngine::start(Seconds now) noexcept {
    state_ = EngineState::Running;
    reason_ = PolicyReason::None;
    started_at_ = now;
    last_activity_at_ = Seconds{0};
    has_activity_ = false;
    manually_paused_ = false;
    paused_at_ = Seconds{0};
    pause_duration_ = Seconds{0};
}

void PolicyEngine::stop() noexcept {
    state_ = EngineState::Stopped;
    reason_ = PolicyReason::Manual;
    manually_paused_ = false;
}

bool PolicyEngine::expire_if_due(Seconds now) noexcept {
    if (state_ != EngineState::Stopped && settings_.max_duration > Seconds{0} &&
        nonnegative_difference(now, started_at_) >= settings_.max_duration) {
        state_ = EngineState::Stopped;
        reason_ = PolicyReason::MaxDuration;
        manually_paused_ = false;
        return true;
    }
    return false;
}

bool PolicyEngine::pause(Seconds now, Seconds duration) noexcept {
    if (state_ == EngineState::Stopped || duration < Seconds{0} || duration > Settings::kMaximumInterval ||
        expire_if_due(now)) {
        return false;
    }
    manually_paused_ = true;
    paused_at_ = now;
    pause_duration_ = duration;
    state_ = EngineState::Paused;
    reason_ = PolicyReason::ManualPause;
    return true;
}

bool PolicyEngine::resume() noexcept {
    if (!manually_paused()) {
        return false;
    }
    manually_paused_ = false;
    // Deliberately leave state paused until evaluate rechecks all safeguards.
    return true;
}

bool PolicyEngine::extend_duration(Seconds now, Seconds extra) noexcept {
    if (state_ == EngineState::Stopped || extra <= Seconds{0} || settings_.max_duration <= Seconds{0} ||
        expire_if_due(now)) {
        return false;
    }
    if (settings_.max_duration > Settings::kMaximumDuration ||
        extra > Settings::kMaximumDuration - settings_.max_duration) {
        return false;
    }
    settings_.max_duration += extra;
    return true;
}

std::optional<Seconds> PolicyEngine::remaining_duration(Seconds now) const noexcept {
    if (settings_.max_duration <= Seconds{0}) {
        return std::nullopt;
    }
    if (state_ == EngineState::Stopped) {
        return Seconds{0};
    }
    const auto elapsed = nonnegative_difference(now, started_at_);
    return settings_.max_duration - std::min(elapsed, settings_.max_duration);
}

bool PolicyEngine::manually_paused() const noexcept {
    return state_ != EngineState::Stopped && manually_paused_;
}

PolicyDecision PolicyEngine::stopped_decision() const noexcept {
    return PolicyDecision{DecisionAction::Stop, EngineState::Stopped, reason_, Seconds{0}};
}

PolicyDecision PolicyEngine::evaluate(const PolicyInput& input) noexcept {
    if (state_ == EngineState::Stopped) {
        return stopped_decision();
    }

    if (input.user_activity && settings_.pause_on_user_activity) {
        last_activity_at_ = input.now;
        has_activity_ = true;
    }

    if (expire_if_due(input.now)) {
        return stopped_decision();
    }
    if (manually_paused_) {
        const auto elapsed = nonnegative_difference(input.now, paused_at_);
        if (pause_duration_ == Seconds{0} || elapsed < pause_duration_) {
            state_ = EngineState::Paused;
            reason_ = PolicyReason::ManualPause;
            return pause_decision(reason_, pause_duration_ == Seconds{0} ? Seconds{0} : pause_duration_ - elapsed);
        }
        manually_paused_ = false;
    }

    if (settings_.pause_when_locked && input.locked) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::Locked;
        return pause_decision(reason_);
    }
    if (settings_.pause_when_disconnected && input.disconnected) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::Disconnected;
        return pause_decision(reason_);
    }
    if (settings_.pause_on_low_battery && input.on_battery &&
        input.battery_percent <= settings_.low_battery_threshold) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::LowBattery;
        return pause_decision(reason_);
    }
    if (settings_.pause_on_battery && input.on_battery) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::OnBattery;
        return pause_decision(reason_);
    }
    if (settings_.pause_when_fullscreen && input.fullscreen) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::Fullscreen;
        return pause_decision(reason_);
    }
    if (settings_.active_hours.enabled && !settings_.active_hours.contains(input.minute_of_day)) {
        state_ = EngineState::Paused;
        reason_ = PolicyReason::OutsideActiveHours;
        return pause_decision(reason_);
    }
    if (settings_.pause_on_user_activity && has_activity_) {
        const auto elapsed = nonnegative_difference(input.now, last_activity_at_);
        if (elapsed < settings_.user_activity_cooldown) {
            const auto remaining = settings_.user_activity_cooldown - elapsed;
            state_ = EngineState::Paused;
            reason_ = PolicyReason::UserActivity;
            return pause_decision(reason_, remaining);
        }
    }

    state_ = EngineState::Running;
    reason_ = PolicyReason::None;
    return PolicyDecision{DecisionAction::Run, EngineState::Running, PolicyReason::None, Seconds{0}};
}

EngineState PolicyEngine::state() const noexcept {
    return state_;
}

PolicyReason PolicyEngine::reason() const noexcept {
    return reason_;
}

std::string status_text(const PolicyDecision& decision) {
    if (decision.state == EngineState::Running) {
        return "Running";
    }
    if (decision.state == EngineState::Stopped) {
        if (decision.reason == PolicyReason::None) {
            return "Stopped";
        }
        return "Stopped: " + std::string(policy_reason_name(decision.reason));
    }

    std::string result = "Paused: ";
    result += policy_reason_name(decision.reason);
    if ((decision.reason == PolicyReason::UserActivity || decision.reason == PolicyReason::ManualPause) &&
        decision.cooldown_remaining > Seconds{0}) {
        result += " (" + std::to_string(decision.cooldown_remaining.count()) + "s remaining)";
    }
    return result;
}

}  // namespace idleharbor::core
