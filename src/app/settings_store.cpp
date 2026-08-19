#include "idleharbor/app/settings_store.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace idleharbor::app {
namespace {

using Values = std::map<std::string, std::string, std::less<>>;

std::string Trim(std::string value) {
    const auto is_space = [](const unsigned char character) { return std::isspace(character) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
    return value;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<std::uint64_t> ParseUnsigned(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<bool> ParseBool(std::string value) {
    value = Lower(Trim(std::move(value)));
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<core::MotionMode> ParseMotion(std::string value) {
    value = Lower(Trim(std::move(value)));
    if (value == "off") {
        return core::MotionMode::Off;
    }
    if (value == "normal" || value == "diagonal") {
        return core::MotionMode::Normal;
    }
    if (value == "zen") {
        return core::MotionMode::Zen;
    }
    if (value == "circle") {
        return core::MotionMode::Circle;
    }
    if (value == "linear") {
        return core::MotionMode::Linear;
    }
    return std::nullopt;
}

std::optional<core::PowerMode> ParsePower(std::string value) {
    value = Lower(Trim(std::move(value)));
    if (value == "none") {
        return core::PowerMode::None;
    }
    if (value == "system") {
        return core::PowerMode::System;
    }
    if (value == "display") {
        return core::PowerMode::Display;
    }
    return std::nullopt;
}

std::optional<core::ProfileKind> ParseProfile(std::string value) {
    value = Lower(Trim(std::move(value)));
    if (value == "balanced") {
        return core::ProfileKind::Balanced;
    }
    if (value == "long-task" || value == "long-operation") {
        return core::ProfileKind::LongTask;
    }
    if (value == "presentation") {
        return core::ProfileKind::Presentation;
    }
    if (value == "compatibility") {
        return core::ProfileKind::Compatibility;
    }
    if (value == "visible") {
        return core::ProfileKind::Visible;
    }
    if (value == "battery-saver") {
        return core::ProfileKind::BatterySaver;
    }
    if (value == "custom") {
        return core::ProfileKind::Custom;
    }
    return std::nullopt;
}

void Warn(SettingsLoadResult& result, std::string message) {
    result.warnings.push_back(std::move(message));
}

template <typename Setter>
void ApplyBool(const Values& values, std::string_view key, SettingsLoadResult& result, Setter setter) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return;
    }
    const auto parsed = ParseBool(found->second);
    if (!parsed.has_value()) {
        Warn(result, "Ignoring invalid boolean for '" + std::string(key) + "'.");
        return;
    }
    setter(*parsed);
}

template <typename Setter>
void ApplyNumber(
    const Values& values,
    std::string_view key,
    std::uint64_t minimum,
    std::uint64_t maximum,
    SettingsLoadResult& result,
    Setter setter) {
    const auto found = values.find(key);
    if (found == values.end()) {
        return;
    }
    const auto parsed = ParseUnsigned(Trim(found->second));
    if (!parsed.has_value() || *parsed < minimum || *parsed > maximum) {
        Warn(
            result,
            "Ignoring out-of-range integer for '" + std::string(key) + "' (expected " +
                std::to_string(minimum) + " to " + std::to_string(maximum) + ").");
        return;
    }
    setter(*parsed);
}

std::filesystem::path ExecutablePath() {
    std::wstring buffer(512, L'\0');
    for (;;) {
        const DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            return std::filesystem::current_path() / L"IdleHarbor.exe";
        }
        if (copied < buffer.size() - 1) {
            buffer.resize(copied);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string BoolText(const bool value) {
    return value ? "true" : "false";
}

bool WriteDataOwnershipMarker(const std::filesystem::path& directory, std::string& error) {
    const auto marker = directory / L".idleharbor-data.json";
    std::error_code filesystem_error;
    if (std::filesystem::exists(marker, filesystem_error)) {
        if (filesystem_error) {
            error = "Settings were saved, but the data ownership marker could not be inspected.";
        }
        return !filesystem_error;
    }

    auto temporary = marker;
    temporary += L".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Settings were saved, but the data ownership marker could not be created.";
        return false;
    }
    output << "{\n"
           << "  \"product\": \"IdleHarbor\",\n"
           << "  \"markerVersion\": 1,\n"
           << "  \"settingsFile\": \"settings.ini\"\n"
           << "}\n";
    output.close();
    if (!output || MoveFileExW(temporary.c_str(), marker.c_str(), MOVEFILE_WRITE_THROUGH) == FALSE) {
        error = "Settings were saved, but the data ownership marker could not be created.";
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    return true;
}

}  // namespace

std::filesystem::path ResolveSettingsPath(
    const bool portable,
    const std::optional<std::filesystem::path>& explicit_path) {
    if (explicit_path.has_value()) {
        std::error_code error;
        auto absolute = std::filesystem::absolute(*explicit_path, error);
        return error ? *explicit_path : absolute;
    }

    if (portable) {
        return ExecutablePath().parent_path() / L"IdleHarbor.ini";
    }

    PWSTR local_app_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local_app_data))) {
        const std::filesystem::path path(local_app_data);
        CoTaskMemFree(local_app_data);
        return path / L"IdleHarbor" / L"settings.ini";
    }
    return ExecutablePath().parent_path() / L"IdleHarbor.ini";
}

SettingsLoadResult LoadSettings(const std::filesystem::path& path) {
    SettingsLoadResult result;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return result;
    }
    result.file_found = true;

    Values values;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line_number == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        line = Trim(std::move(line));
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            Warn(result, "Ignoring malformed settings line " + std::to_string(line_number) + ".");
            continue;
        }
        auto key = Lower(Trim(line.substr(0, separator)));
        auto value = Trim(line.substr(separator + 1));
        if (key.empty()) {
            Warn(result, "Ignoring settings line " + std::to_string(line_number) + " with an empty key.");
            continue;
        }
        values[std::move(key)] = std::move(value);
    }
    if (input.bad()) {
        Warn(result, "The settings file could not be read completely; safe defaults were retained where needed.");
    }

    if (const auto found = values.find("schema"); found != values.end()) {
        const auto schema = ParseUnsigned(Trim(found->second));
        if (!schema.has_value() || *schema != kSettingsSchemaVersion) {
            Warn(result, "Settings schema is not supported; known values will be imported conservatively.");
        }
    }

    if (const auto found = values.find("profile"); found != values.end()) {
        const auto profile = ParseProfile(found->second);
        if (profile.has_value()) {
            result.settings.session = core::settings_for_profile(*profile);
        } else {
            Warn(result, "Ignoring unknown profile '" + found->second + "'.");
        }
    }

    if (const auto found = values.find("motion"); found != values.end()) {
        const auto motion = ParseMotion(found->second);
        if (motion.has_value()) {
            result.settings.session.motion = *motion;
        } else {
            Warn(result, "Ignoring unknown motion mode '" + found->second + "'.");
        }
    }
    if (const auto found = values.find("power"); found != values.end()) {
        const auto power = ParsePower(found->second);
        if (power.has_value()) {
            result.settings.session.power = *power;
        } else {
            Warn(result, "Ignoring unknown power mode '" + found->second + "'.");
        }
    }

    ApplyNumber(values, "interval_seconds", 1, 24 * 60 * 60, result, [&](const std::uint64_t value) {
        result.settings.session.interval = core::Seconds(value);
    });
    ApplyNumber(values, "random_minimum_seconds", 1, 24 * 60 * 60, result, [&](const std::uint64_t value) {
        result.settings.session.random_minimum = core::Seconds(value);
    });
    ApplyNumber(values, "distance", 1, 120, result, [&](const std::uint64_t value) {
        result.settings.session.distance = static_cast<std::uint32_t>(value);
    });
    ApplyNumber(values, "user_activity_cooldown_seconds", 1, 24 * 60 * 60, result, [&](const std::uint64_t value) {
        result.settings.session.user_activity_cooldown = core::Seconds(value);
    });
    ApplyNumber(values, "low_battery_threshold", 0, 100, result, [&](const std::uint64_t value) {
        result.settings.session.low_battery_threshold = static_cast<std::uint8_t>(value);
    });
    ApplyNumber(values, "active_hours_start_minute", 0, 24 * 60 - 1, result, [&](const std::uint64_t value) {
        result.settings.session.active_hours.start_minute = static_cast<std::uint16_t>(value);
    });
    ApplyNumber(values, "active_hours_end_minute", 0, 24 * 60 - 1, result, [&](const std::uint64_t value) {
        result.settings.session.active_hours.end_minute = static_cast<std::uint16_t>(value);
    });
    ApplyNumber(values, "max_duration_seconds", 0, 30ULL * 24 * 60 * 60, result, [&](const std::uint64_t value) {
        result.settings.session.max_duration = core::Seconds(value);
    });

    ApplyBool(values, "randomize", result, [&](const bool value) { result.settings.session.randomize = value; });
    ApplyBool(values, "pause_on_user_activity", result, [&](const bool value) {
        result.settings.session.pause_on_user_activity = value;
    });
    ApplyBool(values, "pause_when_locked", result, [&](const bool value) {
        result.settings.session.pause_when_locked = value;
    });
    ApplyBool(values, "pause_when_disconnected", result, [&](const bool value) {
        result.settings.session.pause_when_disconnected = value;
    });
    ApplyBool(values, "pause_on_battery", result, [&](const bool value) {
        result.settings.session.pause_on_battery = value;
    });
    ApplyBool(values, "pause_on_low_battery", result, [&](const bool value) {
        result.settings.session.pause_on_low_battery = value;
    });
    ApplyBool(values, "pause_when_fullscreen", result, [&](const bool value) {
        result.settings.session.pause_when_fullscreen = value;
    });
    ApplyBool(values, "active_hours_enabled", result, [&](const bool value) {
        result.settings.session.active_hours.enabled = value;
    });
    ApplyBool(values, "start_minimized", result, [&](const bool value) { result.settings.start_minimized = value; });
    ApplyBool(values, "close_to_tray", result, [&](const bool value) { result.settings.close_to_tray = value; });
    ApplyBool(values, "show_notifications", result, [&](const bool value) {
        result.settings.show_notifications = value;
    });
    ApplyBool(values, "emergency_hotkey", result, [&](const bool value) {
        result.settings.emergency_hotkey = value;
    });

    const auto validation = core::validate(result.settings.session);
    if (!validation.valid) {
        for (const auto& error : validation.errors) {
            Warn(result, "Settings validation: " + error + ". Reverting to the selected profile defaults.");
        }
        const auto profile = result.settings.session.profile;
        result.settings.session = core::settings_for_profile(profile);
    }
    return result;
}

bool SaveSettings(
    const std::filesystem::path& path,
    const AppSettings& settings,
    std::string& error) {
    error.clear();
    const auto validation = core::validate(settings.session);
    if (!validation.valid) {
        error = "Refusing to save invalid settings: " + validation.errors.front();
        return false;
    }

    std::error_code filesystem_error;
    const bool parent_existed =
        path.parent_path().empty() || std::filesystem::exists(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not inspect the settings directory: " + filesystem_error.message();
        return false;
    }
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), filesystem_error);
        if (filesystem_error) {
            error = "Could not create the settings directory: " + filesystem_error.message();
            return false;
        }
    }

    auto temporary = path;
    temporary += L".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not open a temporary settings file for writing.";
        return false;
    }

    const auto& session = settings.session;
    output << "# IdleHarbor settings. Unknown keys are ignored; invalid values fall back safely.\r\n"
           << "schema=" << kSettingsSchemaVersion << "\r\n"
           << "profile=" << core::profile_kind_name(session.profile) << "\r\n"
           << "motion=" << core::motion_mode_name(session.motion) << "\r\n"
           << "power=" << core::power_mode_name(session.power) << "\r\n"
           << "interval_seconds=" << session.interval.count() << "\r\n"
           << "random_minimum_seconds=" << session.random_minimum.count() << "\r\n"
           << "distance=" << session.distance << "\r\n"
           << "randomize=" << BoolText(session.randomize) << "\r\n"
           << "pause_on_user_activity=" << BoolText(session.pause_on_user_activity) << "\r\n"
           << "user_activity_cooldown_seconds=" << session.user_activity_cooldown.count() << "\r\n"
           << "pause_when_locked=" << BoolText(session.pause_when_locked) << "\r\n"
           << "pause_when_disconnected=" << BoolText(session.pause_when_disconnected) << "\r\n"
           << "pause_on_battery=" << BoolText(session.pause_on_battery) << "\r\n"
           << "pause_on_low_battery=" << BoolText(session.pause_on_low_battery) << "\r\n"
           << "low_battery_threshold=" << static_cast<unsigned int>(session.low_battery_threshold) << "\r\n"
           << "pause_when_fullscreen=" << BoolText(session.pause_when_fullscreen) << "\r\n"
           << "active_hours_enabled=" << BoolText(session.active_hours.enabled) << "\r\n"
           << "active_hours_start_minute=" << session.active_hours.start_minute << "\r\n"
           << "active_hours_end_minute=" << session.active_hours.end_minute << "\r\n"
           << "max_duration_seconds=" << session.max_duration.count() << "\r\n"
           << "start_minimized=" << BoolText(settings.start_minimized) << "\r\n"
           << "close_to_tray=" << BoolText(settings.close_to_tray) << "\r\n"
           << "show_notifications=" << BoolText(settings.show_notifications) << "\r\n"
           << "emergency_hotkey=" << BoolText(settings.emergency_hotkey) << "\r\n";
    output.close();
    if (!output) {
        error = "Could not finish writing the temporary settings file.";
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }

    if (MoveFileExW(
            temporary.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
        error = "Could not replace the settings file (Windows error " + std::to_string(GetLastError()) + ").";
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    if (!parent_existed && !WriteDataOwnershipMarker(path.parent_path(), error)) {
        return false;
    }
    return true;
}

}  // namespace idleharbor::app
