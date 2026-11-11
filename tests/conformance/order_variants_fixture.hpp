// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "event_time_fixture.hpp"
#include "qd/events/analytic_order.hpp"
#include "qd/events/spread_order.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <set>

namespace order_variants_fixture {
namespace q = org::ttldtor::qdcxx;
using event_time_fixture::cells;
using event_time_fixture::units;

/** A complete actual parser cursor and named delegate observations. */
struct Sample {
    std::u16string symbol;
    std::vector<q::core::FieldValue> values;
    std::map<std::string, std::string> getters;
    std::map<std::string, std::string> wide;
    std::int64_t fieldTime = 0;
    std::int64_t collectorTime = 0;
};

/** Exact local schema plus all samples and actual factory refusals. */
struct Scenario {
    std::vector<q::core::LocalRecord> records;
    std::map<std::pair<std::size_t, std::size_t>, Sample> samples;
    std::set<std::size_t> rejected;
    std::map<std::size_t, std::size_t> parsed;
};

/** All observations, including capability and scheme metadata outside individual samples. */
struct Corpus {
    std::map<std::string, Scenario> scenarios;
    std::vector<std::vector<std::string>> metadata;
    std::size_t rows = 0;
};

/** Resolve an existing canonical local key; malformed fixture references are terminal. */
inline std::size_t position(const q::core::LocalRecord &record, const std::string &key) {
    const std::u16string name(key.begin(), key.end());

    for (const auto &lookup : record.lookups) {
        if (lookup.name == name) {
            return lookup.fieldId;
        }
    }

    throw std::runtime_error("Variant fixture key absent");
}

/** Load every original observation with finite bytes, lines, fields, records, samples and row budgets.
 * @param path Read-only original TSV; terminal CR framing is accepted without trimming field contents.
 * @return Complete owning corpus, or a terminal malformed/resource/I/O exception.
 */
inline Corpus load(const std::filesystem::path &path) {
    constexpr std::size_t MAX_BYTES = 16 * 1024 * 1024, MAX_ROWS = 250000, MAX_LINE_BYTES = 32768;

    if (std::filesystem::file_size(path) > MAX_BYTES) {
        throw std::runtime_error("Variant fixture byte bound exceeded");
    }

    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Cannot open variant fixture");
    }

    Corpus result;
    std::string line;

    while (std::getline(input, line)) {
        if (++result.rows > MAX_ROWS || line.size() > MAX_LINE_BYTES) {
            throw std::runtime_error("Variant fixture work bound exceeded");
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto row = cells(line);
        const auto &kind = row.at(0);

        if (kind == "capability" || kind == "standardAvailable" || kind == "icebergSetter") {
            result.metadata.push_back(row);
            continue;
        }

        auto &scenario = result.scenarios[row.at(1)];

        if (result.scenarios.size() > 512) {
            throw std::runtime_error("Variant scenario bound exceeded");
        }

        const auto id = std::stoull(row.at(2));

        if (id >= 4) {
            throw std::runtime_error("Variant record bound exceeded");
        }

        if (kind == "record") {
            if (id != scenario.records.size()) {
                throw std::runtime_error("Variant record order changed");
            }

            scenario.records.push_back(
                {static_cast<std::int32_t>(id), std::u16string(row.at(3).begin(), row.at(3).end()), false, {}, {}});
            continue;
        }

        auto &record = scenario.records.at(id);

        if (kind == "physical" || kind == "objfield") {
            const auto type = std::stoi(row.at(4));

            if (type == 0) {
                continue;
            }

            if (record.fields.size() >= 128) {
                throw std::runtime_error("Variant field bound exceeded");
            }

            record.lookups.push_back({std::u16string(row.at(5).begin(), row.at(5).end()), record.fields.size()});

            if (row.at(5) == "MMID") {
                record.lookups.push_back({u"MarketMaker", record.fields.size()});
            }

            if (row.at(5) == "Index") {
                record.hasTime = true;
                record.indexedTimeLayout = q::core::IndexedTimeLowInt32{record.fields.size()};
            }

            record.fields.push_back({type});
        } else if (kind == "reject") {
            scenario.rejected.insert(id);
        } else if (kind == "parsed") {
            if (row.at(4) != "0") {
                throw std::runtime_error("Actual Java variant parser failed");
            }

            scenario.parsed[id] = std::stoull(row.at(3));
        } else {
            const auto sampleId = std::stoull(row.at(3));

            if (sampleId >= 64) {
                throw std::runtime_error("Variant sample bound exceeded");
            }

            auto &sample = scenario.samples[{id, sampleId}];

            if (kind == "symbol") {
                sample.symbol = units(row.at(4));
                sample.fieldTime = std::stoll(row.at(5));

                for (const auto &f : record.fields) {
                    sample.values.push_back({static_cast<q::core::SerialFieldType>(f.serialTypeId), 0});
                }
            } else if (kind == "value") {
                auto &v = sample.values.at(position(record, row.at(4)));

                if (std::to_underlying(v.type) != std::stoi(row.at(5))) {
                    throw std::runtime_error("Variant field type changed");
                }

                v.raw = std::stoll(row.at(6));
            } else if (kind == "object") {
                auto &v = sample.values.at(position(record, row.at(4)));

                if (row.at(5) != "null") {
                    v.units = units(row.at(5));
                }
            } else if (kind == "event" || kind == "wide") {
                auto &target = kind == "event" ? sample.getters : sample.wide;

                if (!target.emplace(row.at(4), row.at(5)).second) {
                    throw std::runtime_error("Duplicate variant observation");
                }
            } else if (kind == "collectorTime") {
                sample.collectorTime = std::stoll(row.at(4));
            } else {
                throw std::runtime_error("Unknown variant fixture row");
            }
        }
    }

    if (!input.eof()) {
        throw std::runtime_error("Cannot finish variant fixture");
    }

    return result;
}

/** Compare exact binary64 bits, allowing only NaN payload normalization. */
inline bool sameDouble(double value, const std::string &expected) {
    if (!expected.starts_with("bits:")) {
        throw std::runtime_error("Malformed variant double observation");
    }

    const auto bits = std::stoll(expected.substr(5));

    return (std::isnan(value) && std::isnan(std::bit_cast<double>(bits))) || std::bit_cast<std::int64_t>(value) == bits;
}

/** Compare every common OrderBase public accessor with the production delegate. */
template <class Event> bool matchesBase(const Event &v, const Sample &s) {
    auto remaining = s.getters;
    const auto n = [&](const std::string &name, std::int64_t actual) {
        const auto expected = std::stoll(remaining.at(name));
        remaining.erase(name);

        return expected == actual;
    };
    const auto d = [&](const std::string &name, double actual) {
        const auto expected = remaining.at(name);
        remaining.erase(name);

        return sameDouble(actual, expected);
    };
    const auto text = [&](const std::string &name, const std::optional<std::u16string> &actual) {
        const auto e = remaining.at(name);
        remaining.erase(name);

        return e == "null" ? !actual : (actual && *actual == units(e));
    };
    const auto symbol = remaining.at("getEventSymbol");
    remaining.erase("getEventSymbol");
    const auto hasSize = remaining.at("hasSize");
    remaining.erase("hasSize");
    bool result = v.symbol == units(symbol) && (v.hasSize() == (hasSize == "true"));
    result &= n("getEventTime", v.eventTime) & n("getIndex", v.index) & n("getSource", v.getSourceId()) &
              n("getEventFlags", v.eventFlags) & n("getTimeSequence", v.timeSequence) & n("getTime", v.getTime()) &
              n("getTimeNanos", v.getTimeNanos()) & n("getSequence", v.getSequence()) &
              n("getTimeNanoPart", v.timeNanoPart) & n("getActionTime", v.actionTime) & n("getOrderId", v.orderId) &
              n("getAuxOrderId", v.auxOrderId) & n("getCount", v.count) & n("getTradeId", v.tradeId) &
              n("getExchangeCode", v.getExchangeCode()) & n("getOrderSide", std::to_underlying(v.getOrderSide())) &
              n("getScope", std::to_underlying(v.getScope())) & n("getAction", std::to_underlying(v.getAction())) &
              n("getSize", v.getSize()) & n("rawFlags", v.flags);
    result &= d("getPrice", v.getPrice()) & d("getSizeAsDouble", v.getSizeAsDouble()) &
              d("getExecutedSize", v.getExecutedSize()) & d("getTradePrice", v.getTradePrice()) &
              d("getTradeSize", v.getTradeSize());
    auto wide = s.wide;
    const auto w = [&](const std::string &name, q::core::WideDecimal value) {
        const auto expected = std::stoll(wide.at(name));
        wide.erase(name);

        return value.raw == expected;
    };
    result &= w("Price", v.price) & w("Size", v.size) & w("ExecutedSize", v.executedSize) &
              w("TradePrice", v.tradePrice) & w("TradeSize", v.tradeSize);

    if constexpr (std::is_same_v<Event, q::events::SpreadOrder>) {
        result &= text("getSpreadSymbol", v.spreadSymbol);
    } else {
        result &= text("getMarketMaker", v.marketMaker) & d("getIcebergPeakSize", v.getIcebergPeakSize()) &
                  d("getIcebergHiddenSize", v.getIcebergHiddenSize()) &
                  d("getIcebergExecutedSize", v.getIcebergExecutedSize()) & n("rawIcebergFlags", v.icebergFlags) &
                  n("getIcebergType", std::to_underlying(v.getIcebergType()));
        result &= w("IcebergPeakSize", v.icebergPeakSize) & w("IcebergHiddenSize", v.icebergHiddenSize) &
                  w("IcebergExecutedSize", v.icebergExecutedSize);
    }

    return result && remaining.empty() && wide.empty();
}
} // namespace order_variants_fixture

#include "qd/protocol/describe_records.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"
#include <iterator>

namespace order_variants_fixture {
/** Complete owned actual binary observations and optional schema derived from exact described fields. */
struct Wire {
    std::optional<q::core::LocalSchema> schema;
    std::vector<q::protocol::StreamDataRecord> records;
};

/** Read the actual production Java binary through existing native framing/binding/decoding.
 * @param path Explicit original binary, at most 64KiB.
 * @param supplied Existing exact local schema, or null to derive ordinary fields from the descriptions.
 * @return Owned complete decoded records; malformed input is a terminal test exception.
 */
inline Wire readWire(const std::filesystem::path &path, const q::core::LocalSchema *supplied = nullptr) {
    if (std::filesystem::file_size(path) > 65536) {
        throw std::runtime_error("Variant wire byte bound exceeded");
    }

    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Cannot open variant binary");
    }

    const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});

    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Cannot finish variant binary");
    }

    const auto bytes = std::as_bytes(std::span(chars));
    q::protocol::FrameReader reader({65536, 65545});
    std::vector<q::protocol::WireRecordDescription> descriptions;
    std::optional<q::protocol::StreamDataContext> context;
    constexpr q::core::SchemaLimits schemaLimits{8, 128, 256, 128, 32768};
    Wire result;
    std::size_t offset = 0, frames = 0;

    while (offset < bytes.size()) {
        auto frame = reader.feed(bytes.subspan(offset));
        offset += frame.accepted;

        if (frame.status == q::protocol::FrameStatus::FAILED) {
            throw std::runtime_error("Variant native framing failed");
        }

        if (!frame.frame) {
            continue;
        }

        if (++frames > 16) {
            throw std::runtime_error("Variant binary frame bound exceeded");
        }

        if (frame.frame->type == 2) {
            auto parsed =
                q::protocol::parseDescribeRecords(frame.frame->payload, {65536, 16, 128, 128, {65536, 8192}, 65536});

            if (parsed.status != q::protocol::DescriptionStatus::OK) {
                throw std::runtime_error("Variant native description failed");
            }

            descriptions.insert(descriptions.end(), parsed.descriptions.begin(), parsed.descriptions.end());

            if (!supplied) {
                std::vector<q::core::LocalRecord> records;

                for (const auto &description : descriptions) {
                    q::core::LocalRecord record{
                        static_cast<std::int32_t>(records.size()), description.name, false, {}, {}};

                    for (const auto &field : description.fields) {
                        if (field.serialTypeId == 0 || field.name == u"EventTime" || field.name == u"EventSequence") {
                            continue;
                        }

                        record.lookups.push_back({field.name, record.fields.size()});

                        if (field.name == u"Index") {
                            record.hasTime = true;
                            record.indexedTimeLayout = q::core::IndexedTimeLowInt32{record.fields.size()};
                        }

                        record.fields.push_back({field.serialTypeId});
                    }

                    records.push_back(std::move(record));
                }

                auto made =
                    q::core::LocalSchema::create(records, schemaLimits, q::core::LocalProfile::BINARY_FIELDS_V1);

                if (!made) {
                    throw std::runtime_error("Variant described local schema failed");
                }

                result.schema.emplace(std::move(*made));
            }

            auto bound = q::protocol::StreamDataContext::createWithFieldTimePolicy(
                supplied ? *supplied : *result.schema, descriptions, {schemaLimits, 16, {256, 128, 65536}},
                q::protocol::FieldTimePolicy::READ);

            if (!bound) {
                throw std::runtime_error("Variant native binding failed");
            }

            context.emplace(std::move(*bound));
        } else if (frame.frame->type == 20) {
            // Complete the derived immutable schema before decoding any source context.
            if (!supplied) {
                continue;
            }

            if (!context) {
                throw std::runtime_error("Variant data before schema");
            }

            q::protocol::StreamDataLimits limits{65536,         128,   8192, 128,  8192,  8192,  {65536, 128, 128},
                                                 {65536, 8192}, 65536, 8192, 8192, 65536, 65536, 65536,
                                                 65536,         65536, 128};
            auto decoded = q::protocol::decodeStreamDataWithEventPolicy(
                frame.frame->payload, *context, limits, q::protocol::DataEventPolicy::BINARY_CATALOG_V1);

            if (decoded.status != q::protocol::StreamDataStatus::OK) {
                throw std::runtime_error("Variant native data decoding failed");
            }

            for (auto &record : decoded.records) {
                result.records.push_back(std::move(record));
            }

            if (result.records.size() > 128) {
                throw std::runtime_error("Variant decoded record bound exceeded");
            }
        } else {
            throw std::runtime_error("Unexpected variant Java binary frame");
        }
    }

    if (reader.finish() != q::protocol::FrameStatus::FINISHED) {
        throw std::runtime_error("Incomplete variant binary");
    }

    if (!supplied) {
        if (!result.schema) {
            throw std::runtime_error("Variant binary has no schema");
        }

        auto decoded = readWire(path, &*result.schema);
        result.records = std::move(decoded.records);
    }

    return result;
}
} // namespace order_variants_fixture
