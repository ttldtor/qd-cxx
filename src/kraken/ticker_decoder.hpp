// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/quote.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::kraken {

/** Type of a decoded public ticker message. */
enum class MessageKind {
    ACKNOWLEDGEMENT, ///< A successful subscription acknowledgement.
    REJECTION,       ///< A rejected subscription request.
    HEARTBEAT,       ///< Connection heartbeat, without fresh quote data.
    STATUS,          ///< Exchange system status, without fresh quote data.
    SNAPSHOT,        ///< Initial ticker values for the current connection.
    UPDATE           ///< Ticker changes for the current connection.
};

/** Validation failure while decoding a ticker message. */
enum class DecodeError {
    RESOURCE_LIMIT,    ///< Input, nesting, row or retained-string limit exceeded.
    MALFORMED_JSON,    ///< Input is not a single complete JSON object.
    INVALID_MESSAGE,   ///< Message shape, field type or symbol is unsupported.
    INVALID_DECIMAL,   ///< A price or size is negative or not exactly representable.
    ALLOCATION_FAILURE ///< Owned result or parser allocation failed.
};

/** One complete owned quote observation. */
struct Observation {
    events::Quote quote{}; ///< Composite BTC/USD quote, with exact wide-decimal raw values.
    std::string timestamp; ///< Provider timestamp as received; not a side-specific quote time.
};

/** A bounded message whose content remains valid independently of the input. */
struct Message {
    MessageKind kind;                       ///< Message classification.
    std::vector<Observation> entries;       ///< Data rows for a snapshot or update, otherwise empty.
    std::string detail;                     ///< Owned status or rejection text, otherwise empty.
    std::optional<std::uint64_t> requestId; ///< Optional subscription acknowledgement identifier.
};

/** Decode a complete public WebSocket text message.
 * @param json Borrowed UTF-8 JSON bytes, unchanged during the call.
 * @return Owned message or an error; no partial rows escape on failure.
 */
[[nodiscard]] std::expected<Message, DecodeError> decodeTicker(std::string_view json);

} // namespace org::ttldtor::qdcxx::kraken
