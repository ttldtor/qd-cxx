// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "event_time_fixture.hpp"
#include "qd/events/order.hpp"
#include <array>
#include <bit>
#include <cmath>

namespace order_fixture {
using event_time_fixture::cells;
using event_time_fixture::Scenario;
using event_time_fixture::units;
namespace q = org::ttldtor::qdcxx;

/** Read finite original schema/value/parser/delegate observations without modifying fixture bytes.
 * @param path Explicit read-only oracle TSV, bounded to eight MiB.
 * @return Complete owning scenarios; malformed or excessive observations throw.
 */
inline std::map<std::string, Scenario> load(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024) {
        throw std::runtime_error("Order fixture exceeds eight MiB");
    }

    std::ifstream input(path);

    if (!input) {
        throw std::runtime_error("Cannot open Order fixture");
    }

    std::map<std::string, Scenario> result;
    std::string line;
    std::size_t count = 0;

    while (std::getline(input, line)) {
        if (++count > 100000 || line.size() > 8192) {
            throw std::runtime_error("Order fixture bound exceeded");
        }

        const auto row = cells(line);
        auto &scenario = result[row.at(1)];
        const auto id = std::stoull(row.at(2));

        if (id >= 3) {
            throw std::runtime_error("Order record count exceeded");
        }

        if (row[0] == "record") {
            if (id != scenario.records.size()) {
                throw std::runtime_error("Order record order changed");
            }

            scenario.records.push_back({static_cast<std::int32_t>(id),
                                        std::u16string(row.at(3).begin(), row.at(3).end()),
                                        true,
                                        {},
                                        {},
                                        q::core::IndexedTimeLowInt32{0}});
        } else if (row[0] == "field") {
            auto &record = scenario.records.at(id);
            const auto field = std::stoull(row.at(3));

            if (field != record.fields.size() || field >= 128) {
                throw std::runtime_error("Order field order changed");
            }

            record.fields.push_back({std::stoi(row.at(4))});
            record.lookups.push_back({std::u16string(row.at(5).begin(), row.at(5).end()), field});

            if (row.at(5) != row.at(6)) {
                record.lookups.push_back({std::u16string(row.at(6).begin(), row.at(6).end()), field});
            }
        } else if (row[0] == "physical") {
            if (std::stoull(row.at(3)) == 0 && (row.at(4) != "0" || row.at(5) != "Void")) {
                throw std::runtime_error("Original physical Order VOID slot changed");
            }

            scenario.routes.push_back(row);
        } else if (row[0] == "reject") {
            scenario.rejected.push_back(id);
        } else if (row[0] == "parsed") {
            if (row.at(4) != "0") {
                throw std::runtime_error("Actual Order parser failed");
            }
        } else {
            const auto sample = std::stoull(row.at(3));

            if (sample >= 64) {
                throw std::runtime_error("Order sample count exceeded");
            }

            auto &value = scenario.samples[{id, sample}];

            if (row[0] == "symbol") {
                value.symbol = units(row.at(4));
                value.rawEventTime = std::stoll(row.at(5));
            } else if (row[0] == "value") {
                const auto field = std::stoull(row.at(4));

                if (field != value.values.size()) {
                    throw std::runtime_error("Order value order changed");
                }

                value.values.push_back(
                    {static_cast<q::core::SerialFieldType>(scenario.records.at(id).fields.at(field).serialTypeId),
                     std::stoll(row.at(5))});

                if (row.at(6) != "null") {
                    throw std::runtime_error("Unexpected Order object field");
                }
            } else if (row[0] == "order") {
                if (row.size() != 38) {
                    throw std::runtime_error("Order delegate observation shape changed");
                }

                value.expected.assign(row.begin() + 4, row.end());
            } else {
                throw std::runtime_error("Unknown Order fixture row");
            }
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("Cannot finish Order fixture");
    }

    return result;
}

/** Compare a business binary64 with the actual production getter, allowing canonical NaN payloads.
 * @param value Native owning projection.
 * @param expected Signed raw Java double bits.
 * @return True for equal exact bits or two NaN values.
 */
inline bool sameDouble(double value, const std::string &expected) {
    const auto bits = std::stoll(expected);
    const auto reference = std::bit_cast<double>(bits);

    return (std::isnan(value) && std::isnan(reference)) || std::bit_cast<std::int64_t>(value) == bits;
}

/** Check all public ordinary Order projections against real production parser/delegate output.
 * @param value Complete independently owned native event.
 * @param sample Complete actual production observation.
 * @return True only when every public business value matches its independent reference.
 */
inline bool matches(const q::events::Order &value, const event_time_fixture::Sample &sample) {
    const auto &e = sample.expected;

    if (e.size() != 34) {
        throw std::runtime_error("Incomplete Order expected values");
    }

    const auto number = [&](std::size_t i) {
        return std::stoll(e[i]);
    };
    const auto maker = e[16] == "null" ? std::optional<std::u16string>{} : std::optional{units(e[16])};
    const std::array wide{value.price.raw, value.size.raw, value.executedSize.raw, value.tradePrice.raw,
                          value.tradeSize.raw};
    const std::array business{value.getPrice(), value.getSizeAsDouble(), value.getExecutedSize(), value.getTradePrice(),
                              value.getTradeSize()};

    for (std::size_t i = 0; i < wide.size(); ++i) {
        if (wide[i] != number(23 + i) || !sameDouble(business[i], e[28 + i])) {
            return false;
        }
    }

    return value.symbol == units(e[0]) && value.eventTime == number(1) && value.index == number(2) &&
           value.getSourceId() == number(3) && value.eventFlags == number(4) && value.timeSequence == number(5) &&
           value.getTime() == number(6) && value.getTimeNanos() == number(7) && value.getSequence() == number(8) &&
           value.timeNanoPart == number(9) && value.actionTime == number(10) && value.orderId == number(11) &&
           value.auxOrderId == number(12) && value.count == number(13) && value.flags == number(14) &&
           value.tradeId == number(15) && value.marketMaker == maker && value.getExchangeCode() == number(17) &&
           std::to_underlying(value.getOrderSide()) == number(18) &&
           std::to_underlying(value.getScope()) == number(19) && std::to_underlying(value.getAction()) == number(20) &&
           value.getSize() == number(21) && value.hasSize() == (e[22] == "true");
}
} // namespace order_fixture
