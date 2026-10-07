#include "idleharbor/app/command_line.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
using namespace idleharbor::app;
using namespace std::chrono_literals;
int checks = 0;
int failures = 0;
void Check(bool condition, std::string_view description) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
template<class Options>
void Duration(const Options& options, std::chrono::seconds expected) {
    if constexpr (requires { options.command_duration; }) {
        Check(options.command_duration == expected, "session-only command duration is exact");
    } else { Check(false, "session-only duration is distinct from saved settings"); }
}
void Accepted(const std::vector<std::wstring_view>& args, std::wstring_view name,
              std::optional<std::chrono::seconds> duration = std::nullopt) {
    const auto result = ParseCommandLine(args);
    Check(result.ok(), "session command parses without errors");
    Check(CommandName(result.options.command) == name, "session command has the expected action");
    Check(!result.options.stop_after.has_value(), "one-shot command does not change the default duration");
    if (duration) Duration(result.options, *duration);
}
}
int main() {
    Accepted({L"--pause"}, L"pause");
    Accepted({L"--resume"}, L"resume");
    for (const auto* flag : {L"--start-for", L"--snooze", L"--extend"}) {
        const auto name = std::wstring_view(flag).substr(2);
        Accepted({flag, L"1"}, name, 1s);
        Accepted({flag, L"15m"}, name, 15min);
        Accepted({flag, L"2H"}, name, 2h);
        for (const auto* bad : {L"", L"0", L"-1", L"1.5h", L"1h30m", L"m", L"18446744073709551615h", L"721h"}) {
            Check(!ParseCommandLine({flag,bad}).ok(), "invalid session duration is rejected");
        }
        const auto missing = ParseCommandLine({flag,L"--stop"});
        Check(!missing.ok(), "missing duration is rejected");
        Check(missing.options.command == RequestedCommand::Stop, "missing duration does not consume the following Stop");
        Check(!ParseCommandLine({flag}).ok(), "trailing duration command requires a value");
        Check(!ParseCommandLine({flag,L"1m",flag,L"2m"}).ok(), "ambiguous duplicate durations are rejected");
    }
    Accepted({L"--start-for",L"720h"}, L"start-for", 720h);
    Accepted({L"--snooze",L"24h"}, L"snooze", 24h);
    Accepted({L"--extend",L"720h"}, L"extend", 720h);
    Check(!ParseCommandLine({L"--snooze",L"25h"}).ok(), "snooze is bounded independently from session duration");
    Accepted({L"--start-for",L"30m",L"--motion",L"off",L"--power",L"system"},L"start-for",30min);
    Check(!ParseCommandLine({L"--start-for",L"30m",L"--stop-after",L"2h"}).ok(), "one-shot and saved duration options cannot conflict");
    for (const auto* flag : {L"--pause",L"--resume"}) {
        Check(!ParseCommandLine({flag,L"--start"}).ok(), "live control conflicts with Start");
        Check(!ParseCommandLine({flag,L"--profile",L"presentation"}).ok(), "live control cannot edit session defaults");
        Check(!ParseCommandLine({flag,L"--minimized"}).ok(), "live control cannot modify launch defaults");
        Check(!ParseCommandLine({flag,L"--config",L"settings.ini"}).ok(), "live control cannot change storage");
    }
    Check(!ParseCommandLine({L"--snooze",L"5m",L"--interval",L"1s"}).ok(), "snooze cannot edit pulse settings");
    Check(!ParseCommandLine({L"--extend",L"5m",L"--portable"}).ok(), "extension cannot change storage");
    Check(!ParseCommandLine({L"--pause",L"--resume"}).ok(), "pause and resume conflict");
    const auto config = ParseCommandLine({L"--config",L"-session.ini",L"--start-for",L"10m"});
    Check(config.ok() && config.options.config_path->wstring() == L"-session.ini", "single-dash configuration compatibility is preserved");
    for (const auto* flag : {L"--start-for",L"--pause",L"--resume",L"--snooze",L"--extend"}) {
        Check(CommandLineHelp().find(flag) != std::wstring::npos, "new command is discoverable in built-in help");
    }
    if (failures) { std::cerr << failures << " of " << checks << " session-command checks failed.\n"; return 1; }
    std::cout << "Session command contracts passed (" << checks << " checks).\n";
}
