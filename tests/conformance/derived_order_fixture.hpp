// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "event_time_fixture.hpp"
#include "qd/events/derived_order.hpp"
#include "qd/events/market_maker.hpp"
#include <array>
#include <map>
#include <tuple>

namespace derived_fixture {
namespace q = org::ttldtor::qdcxx;
using event_time_fixture::cells;
using event_time_fixture::units;

/** One original or production-parser cursor with all actual delegate outcomes. */
struct Sample {
    std::u16string symbol;
    std::int32_t flags = 0;
    std::optional<std::int64_t> rawTime;
    std::vector<q::core::FieldValue> values;
    std::map<q::events::DerivedOrderRole, std::vector<std::string>> orders;
    std::vector<std::string> maker;
    std::vector<q::events::DerivedOrderRole> refused;
};

/** Complete original declarations and cursor/delegate observations for three physical records. */
struct Scenario {
    std::vector<q::core::LocalRecord> records;
    std::array<std::vector<std::size_t>, 3> physical;
    std::array<bool, 3> prepareRefused{};
    std::map<std::tuple<std::size_t, std::string, std::size_t>, Sample> samples;
};

/** @param name Actual production delegate name. @return Exact generated bid/ask/unitary role. */
inline q::events::DerivedOrderRole role(const std::string &name) {
    const bool ask = name.find("Ask") != std::string::npos;
    const bool unitary = name.find("Unitary") != std::string::npos;

    return unitary ? (ask ? q::events::DerivedOrderRole::UNITARY_ASK : q::events::DerivedOrderRole::UNITARY_BID)
                   : (ask ? q::events::DerivedOrderRole::ASK : q::events::DerivedOrderRole::BID);
}

/** Read bounded immutable production observations. No schema/type identities are guessed from values.
 * @param path Raw checked-in TSV. @return Complete owning observations; malformed/excessive input throws.
 */
inline std::map<std::string, Scenario> load(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024) {
        throw std::runtime_error("Derived fixture exceeds finite byte bound");
    }

    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Derived fixture unavailable");
    }

    std::map<std::string, Scenario> result;
    std::string line;
    std::size_t lines = 0;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == char{13}) {
            line.pop_back();
        }

        if (++lines > 100000 || line.size() > 8192) {
            throw std::runtime_error("Derived fixture element bound exceeded");
        }

        auto row = cells(line);
        auto &scenario = result[row.at(1)];

        if (row[0] == "RECORD") {
            const auto id = std::stoull(row.at(2));

            if (id != scenario.records.size() || id >= 3) {
                throw std::runtime_error("Derived record identity changed");
            }

            scenario.records.push_back({static_cast<std::int32_t>(id), units(row.at(3)), row.at(4) == "true", {}, {}});
        } else if (row[0] == "FIELD") {
            const auto id = std::stoull(row.at(2)), field = std::stoull(row.at(3));
            auto &record = scenario.records.at(id);

            if (field != record.fields.size() || field >= 128) {
                throw std::runtime_error("Derived logical field order changed");
            }

            record.fields.push_back({std::stoi(row.at(5))});
            scenario.physical.at(id).push_back(std::stoull(row.at(4)));
            const auto local = units(row.at(6)), property = units(row.at(7));
            record.lookups.push_back({local, field});

            if (local != property) {
                record.lookups.push_back({property, field});
            }
        } else if (row[0] == "PREPARE_REFUSAL") {
            scenario.prepareRefused.at(std::stoull(row.at(2))) = true;
        } else if (row[0] == "DELEGATE") {
            continue;
        } else {
            const auto id = std::stoull(row.at(3)), sampleId = std::stoull(row.at(4));

            if (id >= 3 || sampleId >= 64) {
                throw std::runtime_error("Derived sample identity exceeds bound");
            }

            auto &sample = scenario.samples[{id, row.at(2), sampleId}];

            if (row[0] == "INPUT") {
                sample.symbol = units(row.at(5));
                sample.flags = std::stoi(row.at(6));

                if (row.at(7) != "none") {
                    sample.rawTime = std::stoll(row.at(7));
                }
            } else if (row[0] == "VALUE") {
                const auto field = std::stoull(row.at(5));

                if (field != sample.values.size()) {
                    throw std::runtime_error("Derived value order changed");
                }

                sample.values.push_back(
                    {static_cast<q::core::SerialFieldType>(scenario.records.at(id).fields.at(field).serialTypeId),
                     std::stoll(row.at(6))});
            } else if (row[0] == "ORDER") {
                sample.orders.emplace(role(row.at(5)), std::move(row));
            } else if (row[0] == "MM") {
                sample.maker = std::move(row);
            } else if (row[0] == "REFUSAL") {
                sample.refused.push_back(role(row.at(5)));
            } else {
                throw std::runtime_error("Unexpected production fixture row");
            }
        }
    }

    // Indexed time comes only from the captured original integer-slot positions and full serial type.
    for (auto &[name, scenario] : result) {
        for (std::size_t id = 0; id < scenario.records.size(); ++id) {
            auto &record = scenario.records[id];

            if (!record.hasTime) {
                continue;
            }

            const auto &positions = scenario.physical[id];

            if (positions.empty() || positions[0] != 0) {
                throw std::runtime_error("Missing original indexed first slot");
            }

            const auto type = static_cast<q::core::SerialFieldType>(record.fields[0].serialTypeId);
            const bool wide = type == q::core::SerialFieldType::LONG ||
                              type == q::core::SerialFieldType::WIDE_DECIMAL ||
                              type == q::core::SerialFieldType::TIME_MILLIS;

            if (wide) {
                record.indexedTimeLayout = q::core::IndexedTimeSingleInt64{0};
            } else {
                if (positions.size() < 2 || positions[1] != 1) {
                    throw std::runtime_error("Missing original indexed second slot");
                }

                record.indexedTimeLayout = q::core::IndexedTimePairInt32{0, 1};
            }
        }
    }

    return result;
}
} // namespace derived_fixture
