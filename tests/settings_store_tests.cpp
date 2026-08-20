#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include "idleharbor/app/settings_store.hpp"

namespace {

int failures = 0;

void Expect(const bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

bool ContainsWarning(const idleharbor::app::SettingsLoadResult& result, const std::string_view fragment) {
    for (const auto& warning : result.warnings) {
        if (warning.find(fragment) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using idleharbor::app::AppSettings;
    using idleharbor::core::MotionMode;
    using idleharbor::core::PowerMode;
    using idleharbor::core::ProfileKind;
    using idleharbor::core::Seconds;

    const auto test_root = fs::temp_directory_path() /
                           (L"IdleHarbor-settings-test-" + std::to_wstring(GetCurrentProcessId()));
    const auto path = test_root / L"Unicode 港" / L"settings.ini";

    AppSettings settings;
    settings.session = idleharbor::core::settings_for_profile(ProfileKind::Custom);
    settings.session.motion = MotionMode::Circle;
    settings.session.power = PowerMode::Display;
    settings.session.interval = Seconds{75};
    settings.session.random_minimum = Seconds{4};
    settings.session.distance = 12;
    settings.session.randomize = true;
    settings.session.user_activity_cooldown = Seconds{45};
    settings.session.pause_when_fullscreen = true;
    settings.session.active_hours = {true, 8 * 60, 18 * 60};
    settings.session.max_duration = Seconds{3 * 60 * 60};
    settings.start_minimized = true;
    settings.close_to_tray = false;
    settings.show_notifications = false;

    std::string error;
    Expect(idleharbor::app::SaveSettings(path, settings, error), "settings save succeeds");
    Expect(error.empty(), "successful save has no error");
    Expect(
        fs::exists(path.parent_path() / L".idleharbor-data.json"),
        "first save writes the data ownership marker");

    const auto loaded = idleharbor::app::LoadSettings(path);
    Expect(loaded.file_found, "saved file is found");
    Expect(loaded.warnings.empty(), "saved file loads without warnings");
    Expect(loaded.settings.session.profile == ProfileKind::Custom, "profile round-trips");
    Expect(loaded.settings.session.motion == MotionMode::Circle, "motion round-trips");
    Expect(loaded.settings.session.power == PowerMode::Display, "power round-trips");
    Expect(loaded.settings.session.interval == Seconds{75}, "interval round-trips");
    Expect(loaded.settings.session.distance == std::uint32_t{12}, "distance round-trips");
    Expect(loaded.settings.session.pause_when_fullscreen, "fullscreen policy round-trips");
    Expect(loaded.settings.session.active_hours.enabled, "active hours round-trip");
    Expect(loaded.settings.start_minimized, "launch visibility round-trips");
    Expect(!loaded.settings.close_to_tray, "close behavior round-trips");
    Expect(!loaded.settings.show_notifications, "notification behavior round-trips");

    const auto malformed_path = test_root / L"malformed.ini";
    {
        std::ofstream malformed(malformed_path, std::ios::binary);
        malformed << "schema=999\nprofile=balanced\nmotion=zen\npower=system\n"
                     "distance=999\npause_when_locked=perhaps\nnot a setting\n";
    }
    const auto malformed = idleharbor::app::LoadSettings(malformed_path);
    Expect(malformed.file_found, "malformed file is found");
    Expect(malformed.warnings.size() == 4, "malformed values preserve every warning");
    Expect(ContainsWarning(malformed, "schema is not supported"), "unsupported schema warning is preserved");
    Expect(ContainsWarning(malformed, "out-of-range integer for 'distance'"), "range warning is preserved");
    Expect(ContainsWarning(malformed, "invalid boolean for 'pause_when_locked'"), "boolean warning is preserved");
    Expect(ContainsWarning(malformed, "malformed settings line"), "malformed-line warning is preserved");
    Expect(idleharbor::core::validate(malformed.settings.session).valid, "malformed file falls back validly");

    const auto out_of_range_path = test_root / L"out-of-range.ini";
    {
        std::ofstream out_of_range(out_of_range_path, std::ios::binary);
        out_of_range << "profile=balanced\ninterval_seconds=0\ndistance=121\n"
                        "low_battery_threshold=101\nmax_duration_seconds=2592001\n";
    }
    const auto out_of_range = idleharbor::app::LoadSettings(out_of_range_path);
    Expect(out_of_range.warnings.size() == 4, "out-of-range values preserve every warning");
    Expect(ContainsWarning(out_of_range, "interval_seconds"), "interval range warning is preserved");
    Expect(ContainsWarning(out_of_range, "distance"), "distance range warning is preserved");
    Expect(ContainsWarning(out_of_range, "low_battery_threshold"), "battery range warning is preserved");
    Expect(ContainsWarning(out_of_range, "max_duration_seconds"), "duration range warning is preserved");
    Expect(idleharbor::core::validate(out_of_range.settings.session).valid, "out-of-range file returns valid settings");

    const auto cross_field_path = test_root / L"cross-field.ini";
    {
        std::ofstream cross_field(cross_field_path, std::ios::binary);
        cross_field << "profile=balanced\nmotion=off\npower=none\ninterval_seconds=60\n"
                       "random_minimum_seconds=120\n";
    }
    const auto cross_field = idleharbor::app::LoadSettings(cross_field_path);
    Expect(cross_field.warnings.size() == 2, "cross-field validation preserves every warning");
    Expect(
        ContainsWarning(cross_field, "motion Off with power None"),
        "motion/power cross-field warning is preserved");
    Expect(
        ContainsWarning(cross_field, "random minimum must be between"),
        "interval/random-minimum cross-field warning is preserved");
    Expect(cross_field.settings.session.motion == MotionMode::Zen, "cross-field recovery restores profile motion");
    Expect(cross_field.settings.session.power == PowerMode::System, "cross-field recovery restores profile power");
    Expect(cross_field.settings.session.random_minimum == Seconds{1}, "cross-field recovery restores profile timing");

    const auto missing = idleharbor::app::LoadSettings(test_root / L"missing.ini");
    Expect(!missing.file_found, "missing file is not reported as loaded");
    Expect(idleharbor::core::validate(missing.settings.session).valid, "missing file returns valid defaults");

    std::error_code cleanup_error;
    fs::remove_all(test_root, cleanup_error);
    Expect(!cleanup_error, "test files clean up");

    if (failures != 0) {
        std::cerr << failures << " settings-store test(s) failed\n";
        return 1;
    }
    return 0;
}
