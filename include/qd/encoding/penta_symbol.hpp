// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace org::ttldtor::qdcxx::encoding {

/** Independent canonical Penta symbol, without flags or repeat state. */
struct PentaSymbolEncoding {
    std::int32_t cipher;              ///< Exact Java signed32 key identity, never zero.
    std::array<std::byte, 5> bytes{}; ///< Complete symbol bytes in the first size positions.
    std::size_t size;                 ///< Symbol bytes, between one and five inclusive.
};

/** Encode the locked PentaCodec symbol domain without allocating or retaining input.
 * At most seven UTF-16 units are examined. Empty text encodes the canonical FE symbol.
 * Non-ASCII, unsupported ASCII and unrepresentable 35-bit forms return no encoding;
 * callers may then use their explicitly selected raw-string profile. This does not
 * grant wildcard permission or interpret a DATA symbol as a subscription selector.
 * Distinct values and concurrent calls are independent; no mutable state is accessed.
 * @param symbol Borrowed exact UTF-16 units, valid for this call only.
 * @return Canonical cipher/bytes, or no encoding for the raw-string domain; never throws.
 */
[[nodiscard]] std::optional<PentaSymbolEncoding> encodePentaSymbol(std::u16string_view symbol) noexcept;

/** Check the immutable canonical key domain without allocating or retaining source input.
 * At most seven units are examined. Encodable text requires its exact Penta cipher;
 * unencodable text requires the raw string cipher zero. DATA star is ordinary data here.
 * @param symbol Actual decoded source UTF-16, borrowed only for the call.
 * @param cipher Actual signed32 postdecoder Java cipher.
 * @return Whether both actual values have canonical key identity; thread-safe and noexcept.
 */
[[nodiscard]] bool isCanonicalPentaSymbol(std::u16string_view symbol, std::int32_t cipher) noexcept;

} // namespace org::ttldtor::qdcxx::encoding
