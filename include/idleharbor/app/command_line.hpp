#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace idleharbor::app {

enum class RequestedCommand {
    Launch,
    Start,
    Stop,
    Toggle,
    Status,
    Show,
    Exit,
};

struct CommandLineOptions {
    RequestedCommand command = RequestedCommand::Launch;
    std::optional<std::wstring> profile;
    std::optional<std::wstring> motion_mode;
    std::optional<std::wstring> power_mode;
    std::optional<std::chrono::seconds> interval;
    std::optional<std::chrono::seconds> pause_on_input;
    std::optional<std::chrono::seconds> stop_after;
    std::optional<std::uint32_t> distance;
    std::optional<std::uint32_t> battery_threshold;
    std::optional<bool> randomize;
    std::optional<bool> pause_on_fullscreen;
    bool minimized = false;
    bool portable = false;
    bool show_help = false;
    bool show_version = false;
    std::optional<std::filesystem::path> config_path;
};

struct CommandLineParseResult {
    CommandLineOptions options;
    std::vector<std::wstring> errors;

    [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
};

[[nodiscard]] CommandLineParseResult ParseCommandLine(const std::vector<std::wstring_view>& arguments);
[[nodiscard]] std::wstring CommandLineHelp();
[[nodiscard]] std::wstring_view CommandName(RequestedCommand command) noexcept;

}  // namespace idleharbor::app
