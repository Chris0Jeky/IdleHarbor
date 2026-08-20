#pragma once

#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace idleharbor::core {

using Seconds = std::chrono::seconds;

enum class MotionMode { Off, Normal, Zen, Circle, Linear };
enum class PowerMode { None, System, Display };
enum class ProfileKind {
    Balanced,
    LongTask,
    Presentation,
    Compatibility,
    Visible,
    BatterySaver,
    Custom,
};
enum class EngineState { Stopped, Running, Paused };
enum class DecisionAction { Stop, Run, Pause };
enum class PolicyReason {
    None,
    Manual,
    UserActivity,
    Locked,
    Disconnected,
    LowBattery,
    OnBattery,
    Fullscreen,
    OutsideActiveHours,
    MaxDuration,
};

struct Point {
    int x{};
    int y{};

    friend constexpr bool operator==(const Point&, const Point&) = default;
};

struct MotionPlan {
    MotionMode mode{MotionMode::Normal};
    std::uint32_t distance{};
    std::vector<Point> relative_offsets{};
};

struct ActiveHours {
    bool enabled{false};
    std::uint16_t start_minute{0};
    std::uint16_t end_minute{0};

    [[nodiscard]] bool contains(std::uint16_t minute_of_day) const noexcept;
};

struct Settings {
    static constexpr std::uint32_t kMinimumDistance = 1;
    static constexpr std::uint32_t kMaximumDistance = 120;
    static constexpr Seconds kMinimumInterval{1};
    static constexpr Seconds kMaximumInterval{24 * 60 * 60};

    ProfileKind profile{ProfileKind::Balanced};
    MotionMode motion{MotionMode::Zen};
    PowerMode power{PowerMode::System};
    Seconds interval{60};
    Seconds random_minimum{1};
    std::uint32_t distance{1};
    bool randomize{false};

    bool pause_on_user_activity{true};
    Seconds user_activity_cooldown{60};
    bool pause_when_locked{true};
    bool pause_when_disconnected{true};
    bool pause_on_battery{false};
    bool pause_on_low_battery{true};
    std::uint8_t low_battery_threshold{20};
    bool pause_when_fullscreen{false};
    ActiveHours active_hours{};
    Seconds max_duration{0};
};

struct ValidationResult {
    bool valid{true};
    std::vector<std::string> errors{};
};

[[nodiscard]] ValidationResult validate(const Settings& settings);

// Returns a complete, conservative preset. Profiles only select defaults; callers may
// override individual fields and validate the resulting settings before starting.
[[nodiscard]] Settings settings_for_profile(ProfileKind profile);

[[nodiscard]] std::string_view motion_mode_name(MotionMode mode) noexcept;
[[nodiscard]] std::string_view power_mode_name(PowerMode mode) noexcept;
[[nodiscard]] std::string_view profile_kind_name(ProfileKind profile) noexcept;
[[nodiscard]] std::string_view engine_state_name(EngineState state) noexcept;
[[nodiscard]] std::string_view policy_reason_name(PolicyReason reason) noexcept;
[[nodiscard]] int reason_priority(PolicyReason reason) noexcept;

[[nodiscard]] MotionPlan make_motion_plan(MotionMode mode, std::uint32_t distance);

class IntervalSampler {
public:
    IntervalSampler(std::uint64_t seed, Seconds minimum, Seconds maximum, bool randomized);

    [[nodiscard]] Seconds next();

private:
    Seconds minimum_{};
    Seconds maximum_{};
    bool randomized_{};
    std::mt19937_64 generator_{};
};

struct PolicyInput {
    Seconds now{};
    bool user_activity{false};
    bool locked{false};
    bool disconnected{false};
    bool on_battery{false};
    std::uint8_t battery_percent{100};
    bool fullscreen{false};
    std::uint16_t minute_of_day{0};
};

struct PolicyDecision {
    DecisionAction action{DecisionAction::Stop};
    EngineState state{EngineState::Stopped};
    PolicyReason reason{PolicyReason::Manual};
    Seconds cooldown_remaining{};
};

class PolicyEngine {
public:
    explicit PolicyEngine(Settings settings);

    void start(Seconds now) noexcept;
    void stop() noexcept;

    [[nodiscard]] PolicyDecision evaluate(const PolicyInput& input) noexcept;
    [[nodiscard]] EngineState state() const noexcept;
    [[nodiscard]] PolicyReason reason() const noexcept;

private:
    [[nodiscard]] PolicyDecision stopped_decision() const noexcept;

    Settings settings_{};
    EngineState state_{EngineState::Stopped};
    PolicyReason reason_{PolicyReason::Manual};
    Seconds started_at_{};
    Seconds last_activity_at_{};
    bool has_activity_{false};
};

[[nodiscard]] std::string status_text(const PolicyDecision& decision);

}  // namespace idleharbor::core
