#include <chrono>
#include <iostream>
#include <string_view>
#include <vector>

#include "idleharbor/app/command_line.hpp"

namespace {

int failures = 0;

void Expect(const bool condition, std::string_view description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

idleharbor::app::CommandLineParseResult Parse(
    const std::initializer_list<std::wstring_view>& arguments) {
    return idleharbor::app::ParseCommandLine(std::vector<std::wstring_view>(arguments));
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    using idleharbor::app::RequestedCommand;

    const auto empty = Parse({});
    Expect(empty.ok(), "no arguments are valid");
    Expect(empty.options.command == RequestedCommand::Launch, "no arguments launch normally");

    const auto upstream = Parse({L"-j", L"-m", L"-o", L"Normal", L"-r", L"-s", L"60", L"-d", L"2"});
    Expect(upstream.ok(), "upstream-compatible flags parse");
    Expect(upstream.options.command == RequestedCommand::Start, "-j starts");
    Expect(upstream.options.minimized, "-m minimizes");
    Expect(upstream.options.motion_mode == L"diagonal", "Normal aliases diagonal");
    Expect(upstream.options.randomize == true, "-r randomizes");
    Expect(upstream.options.interval == 60s, "-s parses seconds");
    Expect(upstream.options.distance == std::uint32_t{2}, "-d parses distance multiplier");

    const auto comprehensive = Parse(
        {L"--start",
         L"--profile",
         L"battery-saver",
         L"--motion",
         L"circle",
         L"--power",
         L"system",
         L"--interval",
         L"5m",
         L"--pause-on-input",
         L"30s",
         L"--stop-after",
         L"2h",
         L"--battery-threshold",
         L"25",
         L"--pause-on-fullscreen",
         L"--no-close-to-tray",
         L"--portable"});
    Expect(comprehensive.ok(), "comprehensive options parse");
    Expect(comprehensive.options.interval == 5min, "minutes parse");
    Expect(comprehensive.options.pause_on_input == 30s, "pause duration parses");
    Expect(comprehensive.options.stop_after == 2h, "hours parse");
    Expect(comprehensive.options.battery_threshold == std::uint32_t{25}, "battery threshold parses");
    Expect(comprehensive.options.pause_on_fullscreen == true, "fullscreen pause parses");
    Expect(comprehensive.options.close_to_tray == false, "close-to-tray option parses");
    Expect(comprehensive.options.portable, "portable mode parses");

    const auto balanced = Parse({L"--profile", L"balanced"});
    Expect(balanced.ok(), "balanced profile parses");
    Expect(balanced.options.profile == L"balanced", "balanced profile is preserved");

    const auto disabled = Parse(
        {L"--no-random", L"--pause-on-input", L"0", L"--stop-after", L"0s", L"--battery-threshold", L"0"});
    Expect(disabled.ok(), "zero disables optional safeguards");
    Expect(disabled.options.randomize == false, "randomization disables");
    Expect(disabled.options.pause_on_input == 0s, "input pause disables");
    Expect(disabled.options.stop_after == 0s, "maximum runtime disables");
    Expect(disabled.options.battery_threshold == std::uint32_t{0}, "battery safeguard disables");

    Expect(!Parse({L"--start", L"--stop"}).ok(), "conflicting commands fail");
    Expect(!Parse({L"--profile", L"stealth"}).ok(), "unknown profile fails");
    Expect(Parse({L"--profile", L"balanced"}).ok(), "balanced profile parses");
    Expect(!Parse({L"--motion", L"random-walk"}).ok(), "unknown motion fails");
    Expect(!Parse({L"--power", L"away"}).ok(), "unsupported away mode fails");
    Expect(!Parse({L"--interval", L"0"}).ok(), "zero interval fails");
    Expect(!Parse({L"--interval", L"25h"}).ok(), "oversized interval fails");
    Expect(Parse({L"--distance", L"1"}).ok(), "minimum distance multiplier is accepted");
    Expect(Parse({L"--distance", L"120"}).ok(), "maximum distance multiplier is accepted");
    Expect(!Parse({L"--distance", L"121"}).ok(), "oversized distance multiplier fails");
    Expect(!Parse({L"--battery-threshold", L"101"}).ok(), "oversized threshold fails");
    Expect(!Parse({L"--stop-after", L"169h"}).ok(), "oversized stop duration fails");
    Expect(!Parse({L"--config"}).ok(), "missing value fails");
    Expect(!Parse({L"--wat"}).ok(), "unknown option fails");
    const auto help = idleharbor::app::CommandLineHelp();
    Expect(!help.empty(), "help text is available");
    Expect(
        help.find(L"--status                    Show the current state") != std::wstring::npos,
        "status help describes the visible state dialog");
    Expect(
        help.find(L"--version                   Show product and version information") != std::wstring::npos,
        "version help describes the visible product-information dialog");
    Expect(
        help.find(L"--close-to-tray             Hide the close button") != std::wstring::npos,
        "close-to-tray help columns align with neighboring options");

    if (failures != 0) {
        std::cerr << failures << " command-line test(s) failed\n";
        return 1;
    }
    return 0;
}
