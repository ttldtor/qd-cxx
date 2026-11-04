// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/event_text.hpp"
#include "qd/events/profile.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Internal CTest caller: no inputs; the source macro names the actual production Java fixture.
// It saves/restores only this test process thread's environment and compares exact Java numeric/share output.
/** @return Zero only when all four supported rounding modes preserve the production Java results. */
int main() {
    std::fenv_t saved{};

    if (std::fegetenv(&saved) != 0) {
        return 2;
    }

    const auto context = org::ttldtor::qdcxx::events::EventTextContext::utc();
    const org::ttldtor::qdcxx::events::EventTextLimits limits{65536, 65536, 262144, 1000000};
    unsigned totalFailed = 0;

    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) != 0) {
            std::fesetenv(&saved);

            return 2;
        }

        std::ifstream input(ORG_TTLDTOR_QDCXX_EVENT_TEXT_FIXTURE);

        if (!input.good()) {
            std::fesetenv(&saved);

            return 2;
        }

        std::string line;
        unsigned checked = 0, failed = 0, doubleFailed = 0;

        while (std::getline(input, line)) {
            std::array<std::string, 4> cell;
            std::istringstream fields(line);

            for (auto &part : cell) {
                std::getline(fields, part, '\t');
            }

            if (cell[1] != "wide" && cell[1] != "double") {
                continue;
            }

            const auto value = std::bit_cast<double>(std::stoull(cell[2], nullptr, 16));
            std::u16string actual;

            if (cell[1] == "double") {
                const auto result = org::ttldtor::qdcxx::events::formatEventDouble(value, limits);

                if (!result) {
                    std::fesetenv(&saved);

                    return 2;
                }

                actual = *result;
            } else {
                org::ttldtor::qdcxx::events::Profile profile{};
                profile.businessNumericSnapshot.emplace();
                (*profile.businessNumericSnapshot)[6].directValue = value;
                const auto result = org::ttldtor::qdcxx::events::toString(profile, context, limits);

                if (!result) {
                    std::fesetenv(&saved);

                    return 2;
                }

                const auto start = result->find(u", shares=") + 9;
                actual = result->substr(start, result->find(u", freeFloat=", start) - start);
            }

            ++checked;
            const std::string text(actual.begin(), actual.end());

            if (text != cell[3]) {
                if (cell[1] == "double") {
                    ++doubleFailed;
                }

                if (failed < 8) {
                    std::cout << mode << '\t' << cell[1] << '\t' << cell[2] << '\t' << cell[3] << '\t' << text << '\n';
                }

                ++failed;
            }
        }

        if (checked < 100000 || !input.eof()) {
            std::fesetenv(&saved);

            return 2;
        }

        totalFailed += failed;
        std::cout << "mode=" << mode << " checked=" << checked << " failed=" << failed
                  << " doubleFailed=" << doubleFailed << '\n';
    }

    return std::fesetenv(&saved) == 0 ? (totalFailed == 0 ? 0 : 1) : 2;
}
