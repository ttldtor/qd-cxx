// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "event_time_fixture.hpp"
#include "qd/events/summary.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <set>
#include <tuple>

namespace summary_fixture {
namespace core = org::ttldtor::qdcxx::core;
using event_time_fixture::cells;
using event_time_fixture::Scenario;
using event_time_fixture::units;

/** Read complete production observations with bounded file, line, record and sample counts.
 * @param path Raw oracle TSV; opened read-only without normalizing any bytes.
 * @return Owned scenario declarations and complete successful cursor samples; errors throw.
 */
inline std::map<std::string, Scenario> loadRaw(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024) {
        throw std::runtime_error("Summary fixture exceeds eight MiB");
    }

    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Cannot open Summary fixture");
    }

    std::map<std::string, Scenario> result;
    std::string line;
    std::size_t lines = 0;

    while (std::getline(input, line)) {
        if (++lines > 100000 || line.size() > 8192) {
            throw std::runtime_error("Summary fixture input bound exceeded");
        }

        const auto row = cells(line);

        if (row[0] == "factory-reject") {
            if ((row.at(3) != "1" && row.at(3) != "3") || row.at(4) != "true") {
                throw std::runtime_error("Production factory boundary changed");
            }

            continue;
        }

        if (row[0] == "interest") {
            if (row.size() != 8) {
                throw std::runtime_error("Interest evidence shape changed");
            }

            continue;
        }

        auto &scenario = result[row.at(1)];
        const auto recordId = std::stoull(row.at(2));

        if (recordId >= 2) {
            throw std::runtime_error("Summary record count exceeded");
        }

        if (row[0] == "record") {
            if (recordId != scenario.records.size()) {
                throw std::runtime_error("Summary record order differs");
            }

            scenario.records.push_back({static_cast<std::int32_t>(recordId),
                                        std::u16string(row.at(3).begin(), row.at(3).end()),
                                        false,
                                        {},
                                        {}});
        } else if (row[0] == "field") {
            auto &record = scenario.records.at(recordId);
            const auto fieldId = std::stoull(row.at(3));

            if (fieldId != record.fields.size() || fieldId >= 128) {
                throw std::runtime_error("Summary field order differs");
            }

            record.fields.push_back({std::stoi(row.at(4))});
            record.lookups.push_back({std::u16string(row.at(5).begin(), row.at(5).end()), fieldId});

            if (row.at(5) != row.at(6)) {
                record.lookups.push_back({std::u16string(row.at(6).begin(), row.at(6).end()), fieldId});
            }
        } else if (row[0] == "text-read") {
            if (row.at(5) != "null" && row.at(5) != "java.lang.String") {
                throw std::runtime_error("Production text reader storage changed");
            }
        } else if (row[0] == "reject" || row[0] == "conversion-reject" || row[0] == "native-reject") {
            scenario.rejected.push_back(recordId);
        } else {
            const auto sampleId = std::stoull(row.at(3));

            if (sampleId >= 2048) {
                throw std::runtime_error("Summary sample count exceeded");
            }

            auto &sample = scenario.samples[{recordId, sampleId}];

            if (sample.symbol.empty()) {
                sample.symbol = u"AAPL";
            }

            if (row[0] == "symbol") {
                sample.symbol = units(row.at(4));
                sample.rawEventTime = std::stoll(row.at(5));
            } else if (row[0] == "value") {
                const auto fieldId = std::stoull(row.at(4));

                if (fieldId != sample.values.size()) {
                    throw std::runtime_error("Summary value order differs");
                }

                core::FieldValue value{
                    static_cast<core::SerialFieldType>(scenario.records.at(recordId).fields.at(fieldId).serialTypeId),
                    std::stoll(row.at(5))};

                if (row.at(6) != "null") {
                    value.units = units(row.at(6));
                }

                sample.values.push_back(std::move(value));
            } else if (row[0] == "summary") {
                sample.expected.assign(row.begin() + 4, row.end());
            } else {
                throw std::runtime_error("Unknown Summary fixture row");
            }
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("Cannot finish reading Summary fixture");
    }

    return result;
}

/** Read immutable delegate observations and require a separately observed long-accessor reference for every sample.
 * @param path Original raw delegate corpus; companion files must be in the same directory.
 * @return Complete expected cells with only explicitly classified OI loss corrected per ADR-0010; errors throw.
 */
inline std::map<std::string, Scenario> load(const std::filesystem::path &path) {
    auto result = loadRaw(path);
    auto boundary = loadRaw(path.parent_path() / "open-interest-boundary-v1.tsv");

    for (auto &[name, scenario] : boundary) {
        if (!result.emplace(name, std::move(scenario)).second) {
            throw std::runtime_error("Duplicate Summary boundary scenario");
        }
    }

    const auto reference = path.parent_path() / "open-interest-long-v1.tsv";

    if (std::filesystem::file_size(reference) > 1024 * 1024) {
        throw std::runtime_error("Summary long reference exceeds one MiB");
    }

    std::ifstream input(reference);

    if (!input) {
        throw std::runtime_error("Cannot open Summary long reference");
    }

    std::set<std::tuple<std::string, std::size_t, std::size_t>> seen;
    std::string line;
    std::size_t lines = 0;

    while (std::getline(input, line)) {
        if (++lines > 20000 || line.size() > 1024) {
            throw std::runtime_error("Summary long reference bound exceeded");
        }

        const auto row = cells(line);

        if (row.size() != 8 || row[0] != "reference") {
            throw std::runtime_error("Summary long reference shape differs");
        }

        const auto record = std::stoull(row[2]);
        const auto sample = std::stoull(row[3]);

        if (!seen.emplace(row[1], record, sample).second) {
            throw std::runtime_error("Duplicate Summary long reference");
        }

        auto &expected = result.at(row[1]).samples.at({record, sample}).expected;
        const bool identical = row[4] == row[5];

        if (expected.size() != 20 || expected[4] != row[4] || row[6] != row[5] ||
            row[7] != (identical ? "IDENTICAL" : "CORRECTED_INT_ACCESSOR_LOSS")) {
            throw std::runtime_error("Unclassified Summary OpenInterest difference");
        }

        expected[4] = row[6];
    }

    if (!input.eof()) {
        throw std::runtime_error("Cannot finish Summary long reference");
    }

    for (const auto &[name, scenario] : result) {
        for (const auto &[identity, sample] : scenario.samples) {
            if (sample.expected.size() != 20 || !seen.contains({name, identity.first, identity.second})) {
                throw std::runtime_error("Missing complete Summary long reference");
            }
        }
    }

    return result;
}

/** Compare complete Summary output to actual generated mapping and event business cells.
 * @param value Independent native owning event.
 * @param expected Twenty exact result cells; OI follows the classified actual long reference.
 * @return Exact symbol/scalars/wide bits and business binary64 bits, with NaN classification.
 */
inline bool matches(const org::ttldtor::qdcxx::events::Summary &value, const std::vector<std::string> &expected) {
    if (expected.size() != 20 || value.symbol != units(expected[0])) {
        return false;
    }

    const std::array<std::int64_t, 7> scalars{value.eventTime,
                                              value.getDayId(),
                                              value.getPrevDayId(),
                                              value.getOpenInterest(),
                                              value.flags,
                                              static_cast<std::int64_t>(value.getDayClosePriceType()),
                                              static_cast<std::int64_t>(value.getPrevDayClosePriceType())};

    for (std::size_t index = 0; index < scalars.size(); ++index) {
        if (scalars[index] != std::stoll(expected[1 + index])) {
            return false;
        }
    }

    const std::array wide{value.dayOpenPrice.raw,  value.dayHighPrice.raw,      value.dayLowPrice.raw,
                          value.dayClosePrice.raw, value.prevDayClosePrice.raw, value.prevDayVolume.raw};
    const std::array business{value.getDayOpenPrice(),  value.getDayHighPrice(),      value.getDayLowPrice(),
                              value.getDayClosePrice(), value.getPrevDayClosePrice(), value.getPrevDayVolume()};

    for (std::size_t index = 0; index < wide.size(); ++index) {
        if (wide[index] != std::stoll(expected[8 + index])) {
            return false;
        }

        const auto bits = static_cast<std::uint64_t>(std::stoll(expected[14 + index]));

        if (std::isnan(std::bit_cast<double>(bits)) ? !std::isnan(business[index])
                                                    : std::bit_cast<std::uint64_t>(business[index]) != bits) {
            return false;
        }
    }

    return true;
}
} // namespace summary_fixture
