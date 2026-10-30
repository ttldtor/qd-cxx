// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/protocol/tape_reader.hpp"

#include <array>
#include <doctest/doctest.h>

namespace indexed_test {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;

/** Append one canonical primitive used to assemble designed session mutations.
 * @param target Owned output.
 * @param value Exact signed64 value.
 */
inline void compact(std::vector<std::byte> &target, std::int64_t value) {
    std::array<std::byte, 9> storage{};
    const auto encoded = e::writeCompactLong(value, storage);
    REQUIRE(encoded.status == e::Status::OK);
    target.insert(target.end(), storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Append a finite exact UTF-16 descriptor key.
 * @param target Owned output.
 * @param value Borrowed units.
 */
inline void text(std::vector<std::byte> &target, std::u16string_view value) {
    std::array<std::byte, 4096> storage{};
    const auto encoded = e::writeUtfString(value, storage, {4096, 1024});
    REQUIRE(encoded.status == e::Status::OK);
    target.insert(target.end(), storage.begin(), storage.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Construct an owning frame without reimplementing data-field decoding.
 * @param type Exact message type.
 * @param payload Borrowed complete body.
 * @return Owning envelope.
 */
inline std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    compact(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    compact(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Assemble the unchanged selected strict descriptor grammar.
 * @return BINARY/NONE/EMPTY_SET/STREAM_DATA initial header.
 */
inline std::vector<std::byte> header() {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    compact(body, 3);
    text(body, u"type");
    text(body, u"tape");
    text(body, u"opt");
    text(body, u"");
    text(body, u"time");
    text(body, u"NONE");
    compact(body, 1);
    compact(body, 15);
    text(body, u"STREAM_DATA");
    compact(body, 0);
    compact(body, 0);

    return frame(1, body);
}

/** Encode raw descriptions, preserving same-sender duplicates in one payload.
 * @param descriptions Complete designed entries in wire order.
 * @return Owning type2 envelope.
 */
inline std::vector<std::byte> descriptions(std::span<const p::WireRecordDescription> descriptions) {
    std::vector<std::byte> body;

    for (const auto &record : descriptions) {
        compact(body, record.senderId);
        text(body, record.name);
        compact(body, static_cast<std::int64_t>(record.fields.size()));

        for (const auto &field : record.fields) {
            text(body, field.name);
            compact(body, field.serialTypeId);
        }
    }

    return frame(2, body);
}

/** Provide finite generous bounds so each individual boundary can be narrowed by a test. */
inline p::TapeLimits limits() {
    return {{65536, 65545},
            1048576,
            1024,
            {32, 8, {4096, 1024}, 8192},
            {{32, 1024, 4096, 1024, 65536}, 128, {4096, 1024, 65536}},
            10000000,
            {65536, 10000, 100000, 10000, 100000, 100000, {65536, 1024, 1024}},
            0,
            1000000};
}

/** Construct deliberately reordered logical fields, independent of source slots/wire positions. */
inline c::LocalRecord pairRecord() {
    return {0,
            u"Pair",
            true,
            {{72}, {56}, {8}},
            {{u"Low", 0}, {u"High", 1}, {u"Sentinel", 2}},
            c::IndexedTimePairInt32{1, 0}};
}

/** Validate an explicit new-profile schema for custom session tests. */
inline c::LocalSchema schema() {
    const std::array records{pairRecord()};
    auto result = c::LocalSchema::create(records, limits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Supply a source-ordered declaration which resolves to reordered local positions. */
inline p::WireRecordDescription description(std::int32_t sender = 0) {
    return {sender, u"Pair", {{u"High", 56}, {u"Low", 72}, {u"Sentinel", 8}}};
}

/** Assemble one data payload with an explicit UTF-empty symbol and compact scalar words.
 * @param sender Sparse source identity.
 * @param high Signed high32.
 * @param low Signed low32.
 * @return Complete source-ordered STREAM_DATA frame.
 */
inline std::vector<std::byte> data(std::int32_t sender, std::int32_t high, std::int32_t low) {
    std::vector<std::byte> body{std::byte{0xfe}};
    compact(body, sender);
    compact(body, high);
    compact(body, low);
    compact(body, 913);

    return frame(15, body);
}

/** Submit a complete envelope and require one completed nonfailure event.
 * @param reader Single-thread owner.
 * @param bytes Borrowed complete envelope.
 * @return Owning event, including any complete data records.
 */
inline p::TapeFeedResult accept(p::TapeReader &reader, const std::vector<std::byte> &bytes) {
    auto result = reader.feed(bytes);
    REQUIRE(result.accepted == bytes.size());
    REQUIRE(result.status == p::TapeStatus::EVENT);

    return result;
}
} // namespace indexed_test
