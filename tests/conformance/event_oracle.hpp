// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "evidence_cells.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/protocol/tape_reader.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>

namespace event_oracle {
using org::ttldtor::qdcxx::test_support::hexUnits;
using org::ttldtor::qdcxx::test_support::split;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace p = org::ttldtor::qdcxx::protocol;

/** Fail a strict evidence comparison.
 * @param condition Required invariant.
 * @param message Diagnostic without input secrets.
 */
inline void ensure(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

/** Parse one complete integer cell without requiring canonical decimal spelling.
 * @tparam T Integral destination.
 * @param text Complete cell.
 * @param base Radix.
 * @return Parsed integer, or a fixture error.
 */
template <typename T> T number(std::string_view text, int base = 10) {
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    ensure(result.ec == std::errc{} && result.ptr == text.data() + text.size(), "invalid integer cell");

    return value;
}

/** Decode an exact oracle UTF-16 cell.
 * @param text Lowercase hex, or the empty sentinel.
 * @return Owned units including unpaired surrogates.
 */
inline std::u16string units(std::string_view text) {
    return org::ttldtor::qdcxx::test_support::decodeUnits(text, "invalid UTF-16 cell");
}

/** Encode nullable oracle text.
 * @param value Owned optional string borrowed for comparison.
 * @return Null, empty sentinel or exact units.
 */
inline std::string nullable(const std::optional<std::u16string> &value) {
    return value ? hexUnits(*value) : "null";
}

/** Encode exact canonical binary64 bits.
 * @param value Projected numeric value.
 * @return Sixteen lowercase digits.
 */
inline std::string bits(double value) {
    const auto raw = std::bit_cast<std::uint64_t>(value);
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (int shift = 60; shift >= 0; shift -= 4) {
        result += DIGITS[(raw >> shift) & 15];
    }

    return result;
}

/** One complete production or explicitly labelled direct observation. */
struct Observation {
    std::string phase;                 ///< Source phase distinguishes production replay and direct probes.
    std::string name;                  ///< Quote or Profile.
    std::size_t index{};               ///< Index within its phase.
    std::int64_t timeSequence{};       ///< Exact Java cursor metadata.
    std::vector<c::FieldValue> values; ///< Logical raw values, excluding physical VOID slots.
    std::vector<std::string> event;    ///< Original event cells from symbol onward.
};

/** One captured schema projection and its event observations. */
struct Scenario {
    std::string name;                                             ///< Exact case identifier.
    std::map<std::string, std::array<c::LocalRecord, 2>> schemas; ///< Phase to Profile/Quote projection.
    std::vector<Observation> observations;                        ///< Ordered cursor and delivered event pairs.
};

/** Read a complete, bounded and structurally strict oracle file.
 * @param path Captured TSV.
 * @return Schema phases and paired value/event observations.
 */
inline Scenario read(const std::filesystem::path &path) {
    const std::map<std::string, std::size_t> arity{{"case", 2},    {"startup", 3}, {"record", 7},   {"schema", 8},
                                                   {"lookup", 5},  {"quote", 17},  {"profile", 23}, {"cursor", 6},
                                                   {"value", 7},   {"replay", 5},  {"reject", 4},   {"unsupported", 3},
                                                   {"complete", 2}};
    std::ifstream file(path);
    ensure(file.good(), "missing oracle TSV");
    Scenario result;
    std::optional<Observation> pending;
    std::map<std::string, std::size_t> phaseCounts;
    std::map<std::string, std::size_t> slotCounts;
    std::map<std::string, std::size_t> slotLimits;
    bool completed = false;
    std::string line;
    std::size_t rows = 0;

    while (std::getline(file, line)) {
        ensure(++rows <= 16384 && line.size() <= 262144, "oracle file budget");

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto cells = split(line);
        const auto shape = arity.find(cells[0]);
        ensure(!completed && shape != arity.end() && cells.size() == shape->second, "unknown/trailing/malformed row");

        if (cells[0] == "case") {
            ensure(rows == 1 && cells[1] == path.parent_path().filename().string(), "invalid case header");
            result.name = cells[1];
        } else {
            ensure(!result.name.empty(), "missing case header");
        }

        if (cells[0] == "record") {
            ensure(cells[2] == "Profile" || cells[2] == "Quote", "unknown record");
            ensure(cells[4] == "false", "indexed oracle record");
            auto &record = result.schemas[cells[1]][cells[2] == "Profile" ? 0 : 1];
            ensure(record.name.empty(), "duplicate record declaration");
            record = {cells[2] == "Profile" ? 0 : 1, cells[2] == "Profile" ? u"Profile" : u"Quote", false, {}, {}};

            for (std::size_t i = 0; i < 2; ++i) {
                slotLimits[cells[1] + cells[2] + (i == 0 ? "int" : "obj")] = number<std::size_t>(cells[5 + i]);
            }

            static_cast<void>(number<std::int32_t>(cells[3]));
        } else if (cells[0] == "schema") {
            ensure(cells[3] == "int" || cells[3] == "obj", "invalid slot category");
            const auto key = cells[1] + cells[2] + cells[3];
            ensure(slotLimits.contains(key) && number<std::size_t>(cells[4]) == slotCounts[key]++, "slot order");
            ensure(slotCounts[key] <= slotLimits[key], "excess slots");
            const auto type = number<std::int32_t>(cells[5]);
            const auto local = units(cells[6]);
            const auto property = units(cells[7]);

            if (type == 0) {
                continue;
            }

            auto &record = result.schemas.at(cells[1])[cells[2] == "Profile" ? 0 : 1];
            const auto id = record.fields.size();
            record.fields.push_back({type});
            record.lookups.push_back({local, id});

            if (property != local) {
                record.lookups.push_back({property, id});
            }
        } else if (cells[0] == "cursor") {
            ensure(!pending && (cells[3] == "Quote" || cells[3] == "Profile"), "unpaired cursor");
            ensure(cells[4] == "0", "nonzero event flags");
            ensure(number<std::size_t>(cells[2]) == phaseCounts[cells[1]]++, "cursor phase index");
            pending =
                Observation{cells[1], cells[3], number<std::size_t>(cells[2]), number<std::int64_t>(cells[5]), {}, {}};
        } else if (cells[0] == "value") {
            ensure(pending && pending->phase == cells[1] && pending->index == number<std::size_t>(cells[2]),
                   "orphan value");
            const auto schemaPhase = result.schemas.contains("variant") ? "variant" : "initial";
            const auto &declaration = result.schemas.at(schemaPhase)[pending->name == "Profile" ? 0 : 1];
            const auto position = pending->values.size();
            ensure(position < declaration.fields.size(), "excess logical value");
            const auto lookup =
                std::find_if(declaration.lookups.begin(), declaration.lookups.end(), [&](const auto &key) {
                    return key.fieldId == position;
                });
            ensure(lookup != declaration.lookups.end() && lookup->name == units(cells[3]), "value field identity");
            c::FieldValue value{static_cast<c::SerialFieldType>(number<std::int32_t>(cells[4])),
                                number<std::int64_t>(cells[5])};
            ensure(std::to_underlying(value.type) == declaration.fields[position].serialTypeId, "value full type");

            if (value.type == c::SerialFieldType::UTF_CHAR_ARRAY && cells[6] != "null") {
                value.units = units(cells[6]);
            } else {
                ensure(cells[6] == "null", "invalid scalar payload");
            }

            pending->values.push_back(std::move(value));
        } else if (cells[0] == "quote" || cells[0] == "profile") {
            static_cast<void>(units(cells[3]));

            if (cells[1].ends_with("-input")) {
                continue;
            }

            ensure(pending && pending->phase == cells[1] && pending->index == number<std::size_t>(cells[2]),
                   "orphan event");
            ensure((cells[0] == "quote") == (pending->name == "Quote"), "wrong event kind");
            const auto schemaPhase = result.schemas.contains("variant") ? "variant" : "initial";
            ensure(pending->values.size() ==
                       result.schemas.at(schemaPhase)[pending->name == "Profile" ? 0 : 1].fields.size(),
                   "missing logical value");
            pending->event.assign(cells.begin() + 3, cells.end());
            result.observations.push_back(std::move(*pending));
            pending.reset();
        } else if (cells[0] == "replay") {
            ensure(!pending && number<std::size_t>(cells[2]) == phaseCounts[cells[1] + "-read"] && cells[3] == "0",
                   "replay count/error");
            ensure(number<std::size_t>(cells[4]) ==
                       number<std::size_t>(cells[2]) * (cells[1].ends_with("-raw") ? 3 : 1),
                   "callback count");
        } else if (cells[0] == "complete") {
            ensure(!pending && cells[1] == result.name, "invalid completion");
            completed = true;
        }
    }

    ensure(completed && !file.bad(), "missing completion/read failure");

    for (const auto &[key, count] : slotLimits) {
        ensure(slotCounts[key] == count, "missing physical slot");
    }

    return result;
}

/** Construct equivalent selected local records without implying full dxlink support.
 * @param scenario Captured source projection.
 * @return Owned two-record schema, with deliberately independent C++ local IDs.
 */
inline std::expected<c::LocalSchema, c::SchemaError> schema(const Scenario &scenario) {
    const auto phase = scenario.schemas.contains("variant") ? "variant" : "initial";

    return c::LocalSchema::create(scenario.schemas.at(phase), {2, 128, 256, 256, 16384},
                                  c::LocalProfile::EXTENDED_FIELDS_V1);
}

/** Finite preparation bounds independent of the observed field counts.
 * @return Explicit bounded mapping configuration.
 */
inline e::MappingLimits mappingLimits() {
    return {128, 256, 32, 128, 128, 256, 16384, 1000000};
}

/** Finite whole-file decoding allowances.
 * @return Explicit tape and string bounds.
 */
inline p::TapeLimits tapeLimits() {
    return {{65536, 65545},
            1048576,
            512,
            {32, 16, {4096, 1024}, 8192},
            {{2, 128, 256, 256, 16384}, 128, {2048, 256, 32768}},
            1000000,
            {65536, 1024, 8192, 1024, 8192, 16384, {65536, 1024, 1024}, {4096, 1024}, 65536, 16384, 16384},
            512};
}

/** Read a bounded original binary capture.
 * @param path Original tape.
 * @return Exact owned bytes.
 */
inline std::vector<std::byte> bytes(const std::filesystem::path &path) {
    ensure(std::filesystem::file_size(path) <= 1048576, "fixture file budget");
    std::ifstream input(path, std::ios::binary);
    ensure(input.good(), "missing tape");
    std::vector<char> raw((std::istreambuf_iterator<char>(input)), {});
    ensure(!input.bad(), "tape read error");
    const auto view = std::as_bytes(std::span(raw));

    return {view.begin(), view.end()};
}

/** Serialize every Quote property in independent oracle order.
 * @param value Owned mapped event.
 * @return Cells from symbol through packed sequence.
 */
inline std::vector<std::string> properties(const e::Quote &value) {
    const auto millis = value.timeMs - std::max(value.bidTimeMs, value.askTimeMs);
    const auto packed = (static_cast<std::uint32_t>(millis) << 22) | static_cast<std::uint32_t>(value.sequence);

    return {hexUnits(value.symbol),
            std::to_string(value.bidTimeMs),
            std::to_string(value.bidExchangeCode),
            bits(value.bidPrice.toDouble()),
            bits(value.bidSize.toDouble()),
            std::to_string(value.askTimeMs),
            std::to_string(value.askExchangeCode),
            bits(value.askPrice.toDouble()),
            bits(value.askSize.toDouble()),
            std::to_string(value.timeMs),
            std::to_string(value.sequence),
            std::to_string(value.timeNanoPart),
            std::to_string(value.eventTime),
            std::to_string(std::bit_cast<std::int32_t>(packed))};
}

/** Serialize every Profile property in independent oracle order.
 * @param value Owned mapped event.
 * @return Cells from symbol through eventTime.
 */
inline std::vector<std::string> properties(const e::Profile &value) {
    return {hexUnits(value.symbol),
            bits(value.beta.toDouble()),
            bits(value.earningsPerShare.toDouble()),
            bits(value.dividendFrequency.toDouble()),
            bits(value.exDividendAmount.toDouble()),
            std::to_string(value.exDividendDayId),
            bits(value.high52WeekPrice.toDouble()),
            bits(value.low52WeekPrice.toDouble()),
            bits(value.shares.toDouble()),
            bits(value.freeFloat.toDouble()),
            bits(value.highLimitPrice.toDouble()),
            bits(value.lowLimitPrice.toDouble()),
            std::to_string(value.haltStartTimeMs),
            std::to_string(value.haltEndTimeMs),
            std::to_string(value.flags),
            nullable(value.description),
            nullable(value.statusReason),
            std::to_string(static_cast<int>(value.tradingStatus())),
            std::to_string(static_cast<int>(value.shortSaleRestriction())),
            std::to_string(value.eventTime)};
}
} // namespace event_oracle
