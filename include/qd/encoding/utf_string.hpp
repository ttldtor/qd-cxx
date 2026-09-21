#pragma once

#include "qd/encoding/compact.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace qd::encoding {

/** Explicit operational budgets, independent of Java wire validity; no unlimited default. */
struct UtfStringLimits {
    std::size_t bodyBytes; ///< Maximum encoded body bytes, excluding the compact prefix.
    std::size_t codeUnits; ///< Maximum decoded/read or input/write UTF-16 units.
};

/** Owning UTFString result; an error never exposes a partial value. */
struct UtfStringResult {
    std::optional<std::u16string> value; ///< Interpret only on OK; disengaged then means Java null.
    std::size_t consumed;                ///< Examined bytes, including errors; not resumable state.
    Status status;                       ///< Wire, input or operational-budget outcome.
};

/** Read Java UTFString, preserving raw UTF-16 units and measured noncanonical forms.
 * May read past the declared body before detecting an overrun, but never past input.
 * No normalization, repair or rollback. Allocation may throw std::bad_alloc/std::length_error;
 * no partial result escapes. Memory is grown only for examined and budgeted units.
 * @param input Caller-owned bytes borrowed only during this call; trailing bytes may remain.
 * @param limits Required finite operational budgets; zero permits null and empty.
 * @return Owning value/null on OK; otherwise no value, with examined byte count and error category.
 */
[[nodiscard]] UtfStringResult readUtfString(std::span<const std::byte> input, UtfStringLimits limits);

/** Write canonical Java UTFString with size/capacity preflight and no allocation.
 * Input units and output storage must not overlap and must remain valid during the call.
 * Isolated surrogates are preserved; pairs use four bytes. Excess output remains untouched.
 * @param value Borrowed UTF-16 view, or disengaged for Java null; never retained.
 * @param output Caller-owned writable span, borrowed only during this call.
 * @param limits Required finite body-byte and input-code-unit budgets.
 * @return Encoded count on OK; otherwise zero written and completely unchanged output.
 */
[[nodiscard]] WriteResult writeUtfString(std::optional<std::u16string_view> value, std::span<std::byte> output,
                                         UtfStringLimits limits) noexcept;

} // namespace qd::encoding
