#include <iostream>
#include <string_view>
#include <vector>

#include "idleharbor/app/command_line.hpp"

namespace {
int failures = 0;
void Expect(bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}
}

int main() {
    using idleharbor::app::ParseCommandLine;
    using idleharbor::app::RequestedCommand;
    for (const std::wstring_view path : {L"-odd.ini", L"-o.ini", L"-j-settings.ini", L"-", L"-z", L"./--start"}) {
        const auto result = ParseCommandLine({L"--config", path, L"--stop"});
        Expect(result.ok(), "dash-prefixed config filename is accepted");
        Expect(result.options.config_path.has_value(), "config filename is stored");
        if (result.options.config_path.has_value()) {
            Expect(result.options.config_path->wstring() == path, "config filename is unchanged");
        }
        Expect(result.options.command == RequestedCommand::Stop, "following Stop is preserved");
    }
    for (const std::wstring_view option : {L"--config", L"--profile", L"--motion", L"--power", L"--interval",
                                           L"--pause-on-input", L"--stop-after", L"--distance", L"--battery-threshold"}) {
        for (const std::wstring_view flag : {L"-j", L"-g", L"-m", L"-r", L"-h", L"-?", L"-o", L"-s", L"-d", L"--stop", L"--unknown"}) {
            const auto result = ParseCommandLine({option, flag});
            Expect(!result.ok(), "a following option is not consumed as a missing value");
            Expect(!result.errors.empty() && result.errors.front().find(L"requires a value") != std::wstring::npos,
                   "missing value diagnostic remains first");
            Expect(!result.options.config_path.has_value(), "an option is not stored as a config path");
            if (flag == L"-j") Expect(result.options.command == RequestedCommand::Start, "-j is processed");
            if (flag == L"-g") Expect(result.options.command == RequestedCommand::Show, "-g is processed");
            if (flag == L"-m") Expect(result.options.minimized, "-m is processed");
            if (flag == L"-r") Expect(result.options.randomize == true, "-r is processed");
            if (flag == L"-h" || flag == L"-?") Expect(result.options.show_help, "help is processed");
            if (flag == L"--stop") Expect(result.options.command == RequestedCommand::Stop, "Stop is processed");
        }
    }
    Expect(!ParseCommandLine({L"--config", L""}).ok(), "empty config path is rejected");
    Expect(!ParseCommandLine({L"--interval", L"-1"}).ok(), "negative numeric value is rejected");
    Expect(!ParseCommandLine({L"--profile", L"-odd.ini"}).ok(), "invalid enum value remains rejected");
    Expect(!ParseCommandLine({L"-z"}).ok(), "unknown short option is rejected outside a value position");
    if (failures != 0) return 1;
    std::cout << "CLI value boundaries passed (109 parse cases).\n";
    return 0;
}
