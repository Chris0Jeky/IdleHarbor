#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "idleharbor/core.hpp"

namespace idleharbor::app {

inline constexpr int kSettingsSchemaVersion = 1;

struct AppSettings {
    core::Settings session = core::settings_for_profile(core::ProfileKind::Balanced);
    bool start_minimized = false;
    bool close_to_tray = true;
    bool show_notifications = true;
    bool emergency_hotkey = true;
};

struct SettingsLoadResult {
    AppSettings settings;
    bool file_found = false;
    std::vector<std::string> warnings;
};

[[nodiscard]] std::filesystem::path ResolveSettingsPath(
    bool portable,
    const std::optional<std::filesystem::path>& explicit_path = std::nullopt);
[[nodiscard]] SettingsLoadResult LoadSettings(const std::filesystem::path& path);
[[nodiscard]] bool SaveSettings(
    const std::filesystem::path& path,
    const AppSettings& settings,
    std::string& error);

}  // namespace idleharbor::app
