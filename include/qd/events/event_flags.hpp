// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>

namespace org::ttldtor::qdcxx::events {
/** Indexed-event transaction and snapshot masks, separate from event-specific business flags. */
namespace indexed_event_flags {
inline constexpr std::int32_t TX_PENDING = 0x01;     ///< Transaction continues in a later event.
inline constexpr std::int32_t REMOVE_EVENT = 0x02;   ///< Remove the indexed entry.
inline constexpr std::int32_t SNAPSHOT_BEGIN = 0x04; ///< Start snapshot synchronization.
inline constexpr std::int32_t SNAPSHOT_END = 0x08;   ///< Complete snapshot synchronization.
inline constexpr std::int32_t SNAPSHOT_SNIP = 0x10;  ///< Snapshot ended at a provider limit.
inline constexpr std::int32_t SNAPSHOT_MODE = 0x40;  ///< Publishing request to activate snapshot mode.
} // namespace indexed_event_flags

/** Extract a signed32 bit field with Java arithmetic-right-shift semantics.
 * @param flags Complete raw word; no bits are modified.
 * @param mask Unshifted mask, including arbitrary signed patterns.
 * @param shift Java shift distance, using its low five bits even for negative inputs.
 * @return Arithmetic-right-shifted word AND mask; no enum normalization occurs.
 */
[[nodiscard]] std::int32_t getBits(std::int32_t flags, std::int32_t mask, std::int32_t shift) noexcept;
/** Extract a signed64 bit field with Java arithmetic-right-shift semantics.
 * @param flags Complete raw word; no bits are modified.
 * @param mask Unshifted mask, including arbitrary signed patterns.
 * @param shift Java shift distance, using its low six bits even for negative inputs.
 * @return Arithmetic-right-shifted word AND mask; no enum normalization occurs.
 */
[[nodiscard]] std::int64_t getBits(std::int64_t flags, std::int64_t mask, std::int32_t shift) noexcept;
/** Replace only the selected bits of a signed32 raw word.
 * @param flags Original raw word, including unknown bits.
 * @param mask Unshifted field mask; overflowed shifted bits are discarded.
 * @param shift Java shift distance, using its low five bits.
 * @param bits New field bits; bits outside mask are ignored.
 * @return New raw word with every bit outside the shifted mask preserved.
 */
[[nodiscard]] std::int32_t setBits(std::int32_t flags, std::int32_t mask, std::int32_t shift,
                                   std::int32_t bits) noexcept;
/** Replace only the selected bits of a signed64 raw word.
 * @param flags Original raw word, including unknown bits.
 * @param mask Unshifted field mask; overflowed shifted bits are discarded.
 * @param shift Java shift distance, using its low six bits.
 * @param bits New field bits; bits outside mask are ignored.
 * @return New raw word with every bit outside the shifted mask preserved.
 */
[[nodiscard]] std::int64_t setBits(std::int64_t flags, std::int64_t mask, std::int32_t shift,
                                   std::int64_t bits) noexcept;
} // namespace org::ttldtor::qdcxx::events
