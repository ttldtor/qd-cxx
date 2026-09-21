// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/encoding/utf_string.hpp"

#include <vector>

namespace qd::protocol {

/** Largest structurally valid full DESCRIBE_RECORDS type ID.
 * Includes representation bits; this bound neither masks them nor guarantees type support.
 * See spec/protocol/DESCRIBE-RECORDS.md and BinaryQTPParser.parseDescribeRecords.
 */
inline constexpr std::int32_t MAX_WIRE_SERIAL_TYPE_ID = 0xff;

/** Required logical-element budgets; zero is valid, with no unlimited sentinel. */
struct DescriptionLimits {
    std::size_t payloadBytes;         ///< Maximum complete payload bytes, excluding frame envelope.
    std::size_t descriptions;         ///< Maximum completed records, counting repeated sender IDs.
    std::size_t fieldsPerDescription; ///< Maximum declared field count for one record.
    std::size_t totalFields;          ///< Maximum sum of declared fields across accepted headers.
    encoding::UtfStringLimits name;   ///< Per-name encoded body bytes and decoded UTF-16 units.
    std::size_t totalNameUnits;       ///< Aggregate record/field name units, including duplicates.
};

/** One raw wire field; structural acceptance does not imply serialization or local factory support. */
struct WireFieldDescription {
    std::u16string name;       ///< Owned exact nonempty UTF-16 units, without normalization.
    std::int32_t serialTypeId; ///< Full wire ID in 0..255, including representation bits.
};

/** One structurally completed description; no local-schema identity or binding. */
struct WireRecordDescription {
    std::int32_t senderId;                    ///< Nonnegative sender identity; never used as an allocation index.
    std::u16string name;                      ///< Owned exact nonempty UTF-16 units.
    std::vector<WireFieldDescription> fields; ///< Wire order, preserving duplicate names and VOID.
};

/** Complete-payload outcomes; none requests additional transport input. */
enum class DescriptionStatus {
    OK,                ///< Entire payload consumed at a description boundary.
    TRUNCATED_PAYLOAD, ///< A primitive cannot finish inside this complete payload.
    INVALID_RECORD,    ///< Negative ID/count or null/empty record name after header read.
    INVALID_FIELD,     ///< Null/empty field name or type outside 0..255 after pair read.
    INVALID_STRING,    ///< UTFString encoding or length violates its wire contract.
    RESOURCE_LIMIT,    ///< Explicit caller budget exceeded, independently of wire validity.
    ALLOCATION_FAILURE ///< Allocation failed; earlier completed descriptions remain owned.
};

/** Primitive or checkpoint responsible for the result and its position. */
enum class DescriptionStage {
    PAYLOAD,       ///< Initial payload-byte budget check.
    DESCRIPTION,   ///< Description-count check or completed-record publication.
    SENDER_ID,     ///< Reading the compact sender ID.
    RECORD_NAME,   ///< Reading the record UTFString.
    FIELD_COUNT,   ///< Reading the compact field count.
    RECORD_HEADER, ///< Header validity and declared-field budgets.
    FIELD_NAME,    ///< Reading a field UTFString.
    FIELD_TYPE,    ///< Reading the full compact serial type ID.
    FIELD_HEADER,  ///< Field-pair validity or publication into the pending record.
    END            ///< Successful exact payload end.
};

/** Owning completed prefix plus first failure, with no partial current record. */
struct DescriptionResult {
    std::vector<WireRecordDescription> descriptions; ///< Completed records in original wire order.
    DescriptionStatus status;                        ///< Success, structural, truncation or operational outcome.
    DescriptionStage stage;                          ///< Failing primitive/checkpoint, or END on success.
    std::size_t consumed;             ///< Payload-relative committed examination count; never exceeds input.
    std::size_t errorOffset;          ///< Primitive start or checkpoint position; payload size on OK.
    encoding::Status primitiveStatus; ///< Codec cause; OK for success or nonprimitive failures.
};

/** Decode one complete type-2 payload, without inspecting or changing any frame/session state.
 * Preserves raw in-range types without implementing their serialization or local construction.
 * Allocation exceptions become ALLOCATION_FAILURE with the completed prefix. A throwing string
 * read leaves consumed at its start, as no primitive consumption result was returned.
 * @param payload Caller-owned payload only, borrowed during the call, possibly empty.
 * @param limits Mandatory finite operational budgets; no unchecked count-sized reservation occurs.
 * @return Owned completed descriptions and exact positions/status; never a resumable partial record.
 */
[[nodiscard]] DescriptionResult parseDescribeRecords(std::span<const std::byte> payload, DescriptionLimits limits);

} // namespace qd::protocol
