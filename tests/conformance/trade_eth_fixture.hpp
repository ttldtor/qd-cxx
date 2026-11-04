// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "event_time_fixture.hpp"
#include "qd/events/trade_eth.hpp"
#include <array>
#include <bit>
#include <cmath>

namespace trade_eth_fixture {
namespace core = org::ttldtor::qdcxx::core;
using event_time_fixture::cells;
using event_time_fixture::Scenario;
using event_time_fixture::units;

/** Read complete production observations with bounded file, line, record and sample counts.
 * @param path Raw oracle TSV; opened read-only without normalizing any bytes.
 * @return Owned scenario declarations and complete successful cursor samples; errors throw.
 */
inline std::map<std::string, Scenario> load(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024) {
        throw std::runtime_error("TradeETH fixture exceeds eight MiB");
    }

    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Cannot open TradeETH fixture");
    }

    std::map<std::string, Scenario> result;
    std::string line;
    std::size_t lines = 0;

    while (std::getline(input, line)) {
        if (++lines > 100000 || line.size() > 8192) {
            throw std::runtime_error("TradeETH fixture input bound exceeded");
        }

        const auto row = cells(line);

        if (row[0] == "factory-reject") {
            if ((row.at(3) != "1" && row.at(3) != "3") || row.at(4) != "true") {
                throw std::runtime_error("Production factory boundary changed");
            }

            continue;
        }

        auto &scenario = result[row.at(1)];
        const auto recordId = std::stoull(row.at(2));

        if (recordId >= 2) {
            throw std::runtime_error("TradeETH record count exceeded");
        }

        if (row[0] == "record") {
            if (recordId != scenario.records.size()) {
                throw std::runtime_error("TradeETH record order differs");
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
                throw std::runtime_error("TradeETH field order differs");
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
                throw std::runtime_error("TradeETH sample count exceeded");
            }

            auto &sample = scenario.samples[{recordId, sampleId}];

            if (sample.symbol.empty())
                sample.symbol = u"AAPL";

            if (row[0] == "symbol") {
                sample.symbol = units(row.at(4));
                sample.rawEventTime = std::stoll(row.at(5));
            } else if (row[0] == "value") {
                const auto fieldId = std::stoull(row.at(4));

                if (fieldId != sample.values.size()) {
                    throw std::runtime_error("TradeETH value order differs");
                }

                core::FieldValue value{
                    static_cast<core::SerialFieldType>(scenario.records.at(recordId).fields.at(fieldId).serialTypeId),
                    std::stoll(row.at(5))};

                if (row.at(6) != "null") {
                    value.units = units(row.at(6));
                }

                sample.values.push_back(std::move(value));
            } else if (row[0] == "trade") {
                sample.expected.assign(row.begin() + 4, row.end());
            } else {
                throw std::runtime_error("Unknown TradeETH fixture row");
            }
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("Cannot finish reading TradeETH fixture");
    }

    return result;
}

/** Compare complete native TradeETH output with original production delegate cells.
 * @param value Independent native output, including its owning business snapshot.
 * @param expected Twenty-five actual Java mapping/delegate result cells.
 * @return True for exact raw/scalar/non-NaN double bits and matching NaN classes.
 */
inline bool matches(const org::ttldtor::qdcxx::events::TradeETH &value, const std::vector<std::string> &expected) {
    if (expected.size() != 25 || value.symbol != units(expected[0]))
        return false;

    const std::array<std::int64_t, 12> scalars{value.eventTime,
                                               value.timeSequence,
                                               value.getTime(),
                                               value.getTimeNanos(),
                                               value.getSequence(),
                                               value.timeNanoPart,
                                               value.exchangeCode,
                                               value.tradeId,
                                               value.dayId,
                                               value.flags,
                                               static_cast<std::int64_t>(value.getTickDirection()),
                                               value.isExtendedTradingHours() ? 1 : 0};

    for (std::size_t i = 0; i < scalars.size(); ++i)
        if (scalars[i] != std::stoll(expected[1 + i]))
            return false;

    const std::array wide{value.price.raw, value.size.raw, value.change.raw, value.dayVolume.raw,
                          value.dayTurnover.raw};
    const std::array business{value.getPrice(), value.getSizeAsDouble(), value.getChange(),
                              value.getDayVolumeAsDouble(), value.getDayTurnover()};

    for (std::size_t i = 0; i < wide.size(); ++i) {
        if (wide[i] != std::stoll(expected[13 + i]))
            return false;

        const auto bits = static_cast<std::uint64_t>(std::stoll(expected[18 + i]));

        if (std::isnan(std::bit_cast<double>(bits)) ? !std::isnan(business[i])
                                                    : std::bit_cast<std::uint64_t>(business[i]) != bits)
            return false;
    }

    return value.getSize() == std::stoll(expected[23]) && value.getDayVolume() == std::stoll(expected[24]);
}
} // namespace trade_eth_fixture
