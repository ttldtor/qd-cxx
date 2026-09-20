#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace qd::encoding {

/** Bounded-storage outcomes; Java EOF maps to INSUFFICIENT_INPUT. */
enum class Status {
    OK,                 ///< The complete value was read or written.
    INSUFFICIENT_INPUT, ///< All supplied bytes were consumed; no value is available.
    INSUFFICIENT_OUTPUT ///< No bytes were written and output is unchanged.
};

/** Result owning a decoded value, without retaining the input span.
 * @tparam T Signed fixed-width integer type returned by the reader.
 */
template <typename T> struct ReadResult {
    T value;              ///< Signed decoded value on OK; zero otherwise.
    std::size_t consumed; ///< Bytes consumed, including on incomplete input.
    Status status;        ///< Completion or insufficient-input outcome.
};

/** Result of an atomic bounded write. */
struct WriteResult {
    std::size_t written; ///< Encoded size on OK; zero on insufficient output.
    Status status;       ///< Completion or insufficient-output outcome.
};

/** Read one Java compact int, accepting overlong forms and taking signed low 32 bits.
 * Input is caller-owned and borrowed only during this call. Trailing bytes are untouched.
 * Empty/incomplete input returns INSUFFICIENT_INPUT and consumes the supplied prefix.
 * Retain original bytes to retry from the start; consumed is not a resumable decoder state.
 * @param input Caller-owned encoded bytes, valid for the duration of this call; may be empty.
 * @return On OK, the signed low 32 bits and encoded length. On INSUFFICIENT_INPUT,
 *         zero value and input.size() consumed bytes; no decoded value is available.
 */
[[nodiscard]] ReadResult<std::int32_t> readCompactInt(std::span<const std::byte> input) noexcept;

/** Read one Java compact long, accepting overlong forms and leaving trailing bytes untouched.
 * @param input Caller-owned encoded bytes, borrowed only during this call; may be empty.
 * @return On OK, the signed 64-bit value and encoded length. On INSUFFICIENT_INPUT,
 *         zero value and input.size() consumed bytes. Retain original bytes to retry from the start;
 *         the consumed position is not resumable decoder state.
 */
[[nodiscard]] ReadResult<std::int64_t> readCompactLong(std::span<const std::byte> input) noexcept;

/** Write canonical Java compact int bytes to caller-owned storage borrowed for this call.
 * Requires at most five bytes. Insufficient capacity (including empty output) changes nothing.
 * On success only the first written bytes change; excess capacity is untouched.
 * @param value Signed 32-bit integer to encode using the shortest Java-compatible form.
 * @param output Caller-owned writable storage, valid during this call; may be empty.
 * @return On OK, the encoded length in written. On INSUFFICIENT_OUTPUT, zero written
 *         and unchanged output, including when capacity is zero.
 */
[[nodiscard]] WriteResult writeCompactInt(std::int32_t value, std::span<std::byte> output) noexcept;

/** Write canonical Java compact long bytes, at most nine; excess capacity stays untouched.
 * @param value Signed 64-bit integer to encode using the shortest Java-compatible form.
 * @param output Caller-owned writable storage, borrowed only during this call; may be empty.
 * @return On OK, the encoded length in written. On INSUFFICIENT_OUTPUT, zero written
 *         and unchanged output, including when capacity is zero.
 */
[[nodiscard]] WriteResult writeCompactLong(std::int64_t value, std::span<std::byte> output) noexcept;

} // namespace qd::encoding
