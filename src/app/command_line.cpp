#include "idleharbor/app/command_line.hpp"

#include <algorithm>
#include <array>
#include <cwctype>
#include <limits>
#include <sstream>

namespace idleharbor::app {
namespace {

std::wstring Lower(std::wstring_view value) {
    std::wstring result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    return result;
}

bool IsOneOf(std::wstring_view value, const std::initializer_list<std::wstring_view>& allowed) {
    return std::find(allowed.begin(), allowed.end(), value) != allowed.end();
}

std::optional<std::uint64_t> ParseUnsigned(std::wstring_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::uint64_t result = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(character - L'0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        result = result * 10 + digit;
    }
    return result;
}

std::optional<std::chrono::seconds> ParseDuration(
    std::wstring_view value,
    const bool allow_zero,
    const std::chrono::seconds maximum) {
    if (value.empty()) {
        return std::nullopt;
    }

    std::uint64_t multiplier = 1;
    const wchar_t suffix = static_cast<wchar_t>(std::towlower(value.back()));
    if (suffix == L's' || suffix == L'm' || suffix == L'h') {
        value.remove_suffix(1);
        if (suffix == L'm') {
            multiplier = 60;
        } else if (suffix == L'h') {
            multiplier = 60 * 60;
        }
    }

    const auto amount = ParseUnsigned(value);
    if (!amount.has_value() || (!allow_zero && *amount == 0)) {
        return std::nullopt;
    }
    if (*amount > static_cast<std::uint64_t>(maximum.count()) / multiplier) {
        return std::nullopt;
    }

    return std::chrono::seconds(*amount * multiplier);
}

void AddError(CommandLineParseResult& result, std::wstring message) {
    result.errors.push_back(std::move(message));
}

bool SetCommand(CommandLineParseResult& result, const RequestedCommand command, std::wstring_view option) {
    const auto current = result.options.command;
    if (current != RequestedCommand::Launch && current != command) {
        AddError(
            result,
            L"Conflicting command '" + std::wstring(option) + L"'; '" + std::wstring(CommandName(current)) +
                L"' was already selected.");
        return false;
    }
    result.options.command = command;
    return true;
}

std::optional<std::wstring_view> TakeValue(
    CommandLineParseResult& result,
    const std::vector<std::wstring_view>& arguments,
    std::size_t& index) {
    if (index + 1 >= arguments.size()) {
        AddError(result, L"Option '" + std::wstring(arguments[index]) + L"' requires a value.");
        return std::nullopt;
    }
    ++index;
    return arguments[index];
}

void ParseChoice(
    CommandLineParseResult& result,
    std::wstring_view option,
    std::wstring_view raw_value,
    const std::initializer_list<std::wstring_view>& allowed,
    std::optional<std::wstring>& target) {
    auto value = Lower(raw_value);
    if (value == L"normal") {
        value = L"diagonal";
    }
    if (!IsOneOf(value, allowed)) {
        std::wostringstream message;
        message << L"Invalid value '" << raw_value << L"' for " << option << L". Allowed values: ";
        bool first = true;
        for (const auto candidate : allowed) {
            if (!first) {
                message << L", ";
            }
            message << candidate;
            first = false;
        }
        message << L'.';
        AddError(result, message.str());
        return;
    }
    target = std::move(value);
}

}  // namespace

CommandLineParseResult ParseCommandLine(const std::vector<std::wstring_view>& arguments) {
    CommandLineParseResult result;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto option = arguments[index];

        if (option == L"--help" || option == L"-h" || option == L"-?") {
            result.options.show_help = true;
        } else if (option == L"--version") {
            result.options.show_version = true;
        } else if (option == L"--start" || option == L"--jiggle" || option == L"-j") {
            SetCommand(result, RequestedCommand::Start, option);
        } else if (option == L"--stop") {
            SetCommand(result, RequestedCommand::Stop, option);
        } else if (option == L"--toggle") {
            SetCommand(result, RequestedCommand::Toggle, option);
        } else if (option == L"--status") {
            SetCommand(result, RequestedCommand::Status, option);
        } else if (option == L"--show" || option == L"--settings" || option == L"-g") {
            SetCommand(result, RequestedCommand::Show, option);
        } else if (option == L"--exit") {
            SetCommand(result, RequestedCommand::Exit, option);
        } else if (option == L"--minimized" || option == L"-m") {
            result.options.minimized = true;
        } else if (option == L"--portable") {
            result.options.portable = true;
        } else if (option == L"--random" || option == L"-r") {
            result.options.randomize = true;
        } else if (option == L"--no-random") {
            result.options.randomize = false;
        } else if (option == L"--pause-on-fullscreen") {
            result.options.pause_on_fullscreen = true;
        } else if (option == L"--no-pause-on-fullscreen") {
            result.options.pause_on_fullscreen = false;
        } else if (option == L"--profile") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                ParseChoice(
                    result,
                    option,
                    *value,
                    {L"balanced", L"long-task", L"presentation", L"compatibility", L"visible", L"battery-saver", L"custom"},
                    result.options.profile);
            }
        } else if (option == L"--motion" || option == L"--mode" || option == L"-o") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                ParseChoice(
                    result,
                    option,
                    *value,
                    {L"off", L"zen", L"diagonal", L"linear", L"circle"},
                    result.options.motion_mode);
            }
        } else if (option == L"--power") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                ParseChoice(result, option, *value, {L"none", L"system", L"display"}, result.options.power_mode);
            }
        } else if (option == L"--interval" || option == L"--seconds" || option == L"-s") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                const auto parsed = ParseDuration(*value, false, std::chrono::hours(24));
                if (!parsed.has_value()) {
                    AddError(result, L"Invalid interval '" + std::wstring(*value) + L"'; use 1s to 24h.");
                } else {
                    result.options.interval = parsed;
                }
            }
        } else if (option == L"--pause-on-input") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                const auto parsed = ParseDuration(*value, true, std::chrono::hours(24));
                if (!parsed.has_value()) {
                    AddError(result, L"Invalid input-pause duration '" + std::wstring(*value) + L"'; use 0s to 24h.");
                } else {
                    result.options.pause_on_input = parsed;
                }
            }
        } else if (option == L"--stop-after") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                const auto parsed = ParseDuration(*value, true, std::chrono::hours(24 * 7));
                if (!parsed.has_value()) {
                    AddError(result, L"Invalid stop duration '" + std::wstring(*value) + L"'; use 0s to 168h.");
                } else {
                    result.options.stop_after = parsed;
                }
            }
        } else if (option == L"--distance" || option == L"-d") {
            const auto value = TakeValue(result, arguments, index);
            const auto parsed = value.has_value() ? ParseUnsigned(*value) : std::nullopt;
            if (value.has_value() && (!parsed.has_value() || *parsed < 1 || *parsed > 120)) {
                AddError(result, L"Invalid distance '" + std::wstring(*value) + L"'; use an integer from 1 to 120.");
            } else if (parsed.has_value()) {
                result.options.distance = static_cast<std::uint32_t>(*parsed);
            }
        } else if (option == L"--battery-threshold") {
            const auto value = TakeValue(result, arguments, index);
            const auto parsed = value.has_value() ? ParseUnsigned(*value) : std::nullopt;
            if (value.has_value() && (!parsed.has_value() || *parsed > 100)) {
                AddError(result, L"Invalid battery threshold '" + std::wstring(*value) + L"'; use 0 to 100.");
            } else if (parsed.has_value()) {
                result.options.battery_threshold = static_cast<std::uint32_t>(*parsed);
            }
        } else if (option == L"--config") {
            const auto value = TakeValue(result, arguments, index);
            if (value.has_value()) {
                if (value->empty()) {
                    AddError(result, L"The configuration path must not be empty.");
                } else {
                    result.options.config_path = std::filesystem::path(*value);
                }
            }
        } else {
            AddError(result, L"Unknown option '" + std::wstring(option) + L"'. Use --help for supported options.");
        }
    }

    return result;
}

std::wstring CommandLineHelp() {
    return LR"HELP(IdleHarbor - transparent, native idle prevention for Windows

Usage:
  IdleHarbor.exe [command] [options]

Commands (choose at most one):
  --start, -j                 Start an IdleHarbor session
  --stop                      Stop the current session
  --toggle                    Toggle running/stopped
  --status                    Print the current state
  --show, --settings, -g      Open the settings window
  --exit                      Stop and exit the running instance

Session options:
  --profile NAME              balanced, long-task, presentation, compatibility,
                              visible, battery-saver, or custom
  --motion MODE, --mode, -o   off, zen, diagonal (normal), linear, or circle
  --power MODE                none, system, or display
  --interval DURATION, -s     Pulse interval, from 1s to 24h
  --distance N, -d            Movement distance, from 1 to 120
  --random, -r                Randomize pulses from 1s to the interval
  --no-random                 Use the exact interval
  --pause-on-input DURATION   Resume after this much genuine-input quiet time;
                              0 disables the safeguard
  --stop-after DURATION       Stop automatically after up to 168h; 0 disables
  --battery-threshold N       Pause at or below N percent; 0 disables
  --pause-on-fullscreen       Pause while a full-screen app is foreground
  --no-pause-on-fullscreen    Disable that safeguard

Launch and storage:
  --minimized, -m             Start in the notification area
  --portable                  Store settings beside the executable
  --config PATH               Use an explicit settings file
  --version                   Print the version
  --help, -h, -?              Show this help

Durations accept s, m, or h suffixes; an omitted suffix means seconds.
IdleHarbor is visible and user-controlled. It does not hide from monitoring or
bypass device policy, and injected input may be blocked or detected.
)HELP";
}

std::wstring_view CommandName(const RequestedCommand command) noexcept {
    switch (command) {
        case RequestedCommand::Launch:
            return L"launch";
        case RequestedCommand::Start:
            return L"start";
        case RequestedCommand::Stop:
            return L"stop";
        case RequestedCommand::Toggle:
            return L"toggle";
        case RequestedCommand::Status:
            return L"status";
        case RequestedCommand::Show:
            return L"show";
        case RequestedCommand::Exit:
            return L"exit";
    }
    return L"unknown";
}

}  // namespace idleharbor::app
