// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/schedule/schedule.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
/** @param value The fixed example's owning ASCII venue metadata.
 * @return Console bytes, with an explicit refusal if the fixed example stops being ASCII.
 */
std::string asciiMetadata(std::u16string_view value) {
    std::string result;

    for (const auto unit : value) {
        if (unit > 0x7F) {
            throw std::runtime_error("The fixed console example expects ASCII metadata");
        }

        result.push_back(static_cast<char>(unit));
    }

    return result;
}

/** Print input-free help. */
void help() {
    std::cout
        << "Parse a named trading schedule's rules.\n"
           "Usage: qdcxx-schedule-example --run | --help\n"
           "No arguments and --help show help without working inputs.\n"
           "--run: creates an explicit context with embedded defaults and standard ScheduleLimits, reads XNYS().\n"
           "Output: name and timezone ID; side effects: owned in-memory objects only.\n"
           "Help creates no context and starts no work. No file or network access.\n"
           "Exit: 0 success/help, 1 data/resource/output failure, 2 invalid arguments.\n";
}

/** @return Zero after the complete finite library example; nonzero on typed refusal. */
int runExample() {
    // schedule-usage-begin
    using namespace org::ttldtor::qdcxx::schedule;
    auto context = ScheduleDataContext::create();

    if (!context) {
        std::cerr << "Calendar context refused: " << static_cast<int>(context.error()) << '\n';

        return 1;
    }

    auto schedule = Schedule::getInstance(*context, u"XNYS()");

    if (!schedule) {
        std::cerr << "Schedule refused: " << static_cast<int>(schedule.error()) << '\n';

        return 1;
    }

    const auto name = schedule->getName();
    const auto zoneId = schedule->getTimeZoneId();
    // schedule-usage-end
    std::cout << asciiMetadata(name) << '\t' << asciiMetadata(zoneId) << '\n';

    return std::cout.good() ? 0 : 1;
}
} // namespace

/** @param argc Argument count.
 * @param argv Standalone help selector or explicit --run.
 * @return Documented success, operational-failure or usage code.
 */
int main(int argc, char **argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            help();

            return std::cout.good() ? 0 : 1;
        }

        if (argc == 2 && std::string_view(argv[1]) == "--run") {
            return runExample();
        }

        std::cerr << "Use --help\n";

        return 2;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
