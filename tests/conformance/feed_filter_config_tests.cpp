// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal runner: exact production property/period captures; no bootstrap, native work or files beyond fixture input.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../detail/qd_time_period.hpp"
#include "feed_filter_ipf_config.hpp"
#include "qd/api/feed.hpp"

#include <doctest/doctest.h>

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;

std::string periodInput(std::string text) {
    constexpr std::pair<std::string_view, std::string_view> UNITS[]{{"U0130", "\xc4\xb0"},
                                                                    {"U0131", "\xc4\xb1"},
                                                                    {"U017f", "\xc5\xbf"},
                                                                    {"U0661", "\xd9\xa1"},
                                                                    {"Uff11", "\xef\xbc\x91"}};

    for (const auto &[label, bytes] : UNITS) {
        if (const auto pos = text.find(label); pos != std::string::npos) {
            text.replace(pos, label.size(), bytes);
        }
    }

    return text;
}
} // namespace

TEST_CASE("Private shared period and IPF properties match exact production captures") {
    constexpr std::u16string_view EXPRESSIONS[]{u"ipf[fixture]",
                                                u"ipf[fixture,update=1d]",
                                                u"ipf[fixture][update=PT0.001S]",
                                                u"ipf(fixture)(update=1s)",
                                                u"ipf[fi\\,xture,update=1s]",
                                                u"ipf[fixture,update=1s,schedule=]",
                                                u"ipf[fixture,schedule=]",
                                                u"ipf[fixture,update=0s,schedule=]",
                                                u"ipf[fixture,update=1s,update=2s]",
                                                u"ipf[fixture,unknown=x]",
                                                u"ipf[fixture,update=-1]",
                                                u"ipf[fixture,update=NaN]",
                                                u" ipf [ fixture , update = 1s ] ",
                                                u"ipf[fixture,Update=1s]",
                                                u"ipf[fixture,update]",
                                                u"ipf[fixture,update=1s,schedule=X(a=b)]",
                                                u"ipf[fixture,update=1s,schedule=[x,y]]",
                                                u"ipf[fixture,update=1s,schedule]",
                                                u"ipf[fixture,update=1s,schedule=X(a=\\\\,b)]",
                                                u"ipf[fixture,update=1s,u\\u017fer=,password=]"};
    std::ifstream input(ORG_TTLDTOR_QDCXX_FILTER_CONFIG_FIXTURE);
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream stream(line);
        std::vector<std::string> cells;
        std::string cell;

        while (std::getline(stream, cell, '\t')) {
            cells.push_back(cell);
        }

        if (!line.empty() && line.back() == '\t') {
            cells.emplace_back();
        }

        REQUIRE(cells.size() >= 3);
        INFO(line);

        if (cells[0] == "period" || cells[0] == "period-invalid") {
            const auto parsed = q::detail::parseQdTimePeriod(periodInput(cells[1]));

            if (cells[0] == "period") {
                REQUIRE(parsed);
                CHECK(parsed->count() == std::stoll(cells[2]));
            } else {
                CHECK_FALSE(parsed);
            }
        } else {
            const auto index = static_cast<std::size_t>(std::stoul(cells[1]));
            REQUIRE(index < std::size(EXPRESSIONS));

            if (cells[0] == "config-invalid") {
                CHECK_THROWS_AS(static_cast<void>(q::api::detail::parseFilterIpfConfig(EXPRESSIONS[index], 4096)),
                                q::api::Error);
            } else {
                REQUIRE(cells.size() == 6);
                const auto config = q::api::detail::parseFilterIpfConfig(EXPRESSIONS[index], 4096);
                CHECK(config.source == cells[3]);
                CHECK(config.update.count() == std::stoll(cells[4]));

                if (cells[5] == "<null>") {
                    CHECK_FALSE(config.schedule);
                } else {
                    REQUIRE(config.schedule);
                    CHECK(*config.schedule == std::u16string(cells[5].begin(), cells[5].end()));
                }
            }
        }

        ++rows;
    }

    CHECK(rows == 54);
}
