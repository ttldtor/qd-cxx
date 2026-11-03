// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/indexed_event_source.hpp"

namespace org::ttldtor::qdcxx::events {
/** Owned order-source identity with strict built-in/name resolution.
 * No capabilities, record packing, subscription routing or interning is implied.
 */
class OrderSource final {
    IndexedEventSource source_;
    /** @param id Valid resolved ID.
     * @param name Exact resolved name, copied into owned storage.
     */
    OrderSource(std::int32_t id, std::u16string_view name);

    public:
    /** Resolve built-ins first, otherwise decode one to four ASCII alphanumeric bytes.
     * @param id Signed source ID; unknown malformed IDs are rejected.
     * @return Independently owned resolved identity.
     * @throws std::invalid_argument ID cannot be decoded.
     * @throws std::bad_alloc Allocation fails.
     */
    [[nodiscard]] static OrderSource valueOf(std::int32_t id);
    /** Resolve built-ins first, otherwise encode one to four ASCII alphanumeric units.
     * @param name Exact case-sensitive UTF-16 name borrowed during this call; nullopt is invalid.
     * @return Independently owned resolved identity.
     * @throws std::invalid_argument Null or malformed name (Java null maps to this exception).
     * @throws std::bad_alloc Allocation fails.
     */
    [[nodiscard]] static OrderSource valueOf(std::optional<std::u16string_view> name);
    /** @return Signed resolved source ID. */
    [[nodiscard]] std::int32_t id() const noexcept;
    /** @return Non-null borrowed name, valid until assignment, move-from or destruction. */
    [[nodiscard]] std::u16string_view name() const noexcept;
    /** @return Signed Java-compatible hash, exactly id(). */
    [[nodiscard]] std::int32_t hashCode() const noexcept;
    /** @return Borrowed base identity with the lifetime of this value. */
    [[nodiscard]] const IndexedEventSource &indexedSource() const noexcept;
    /** @param other Order source to compare.
     * @return True when IDs match, ignoring names.
     */
    [[nodiscard]] bool operator==(const OrderSource &other) const noexcept;
    /** @param other Base source to compare.
     * @return True when IDs match, ignoring source family and names.
     */
    [[nodiscard]] bool operator==(const IndexedEventSource &other) const noexcept;
    /** Built-in identity named COMPOSITE_BID. */
    static const OrderSource COMPOSITE_BID;
    /** Built-in identity named COMPOSITE_ASK. */
    static const OrderSource COMPOSITE_ASK;
    /** Built-in identity named REGIONAL_BID. */
    static const OrderSource REGIONAL_BID;
    /** Built-in identity named REGIONAL_ASK. */
    static const OrderSource REGIONAL_ASK;
    /** Built-in identity named AGGREGATE_BID. */
    static const OrderSource AGGREGATE_BID;
    /** Built-in identity named AGGREGATE_ASK. */
    static const OrderSource AGGREGATE_ASK;
    /** Built-in identity named COMPOSITE. */
    static const OrderSource COMPOSITE;
    /** Built-in identity named REGIONAL. */
    static const OrderSource REGIONAL;
    /** Built-in identity named AGGREGATE. */
    static const OrderSource AGGREGATE;
    /** Built-in identity named DEFAULT. */
    static const OrderSource DEFAULT;
    /** Built-in identity named NTV. */
    static const OrderSource NTV;
    /** Built-in identity named ntv. */
    static const OrderSource NTV_LOWER;
    /** Built-in identity named NFX. */
    static const OrderSource NFX;
    /** Built-in identity named ESPD. */
    static const OrderSource ESPD;
    /** Built-in identity named XNFI. */
    static const OrderSource XNFI;
    /** Built-in identity named ICE. */
    static const OrderSource ICE;
    /** Built-in identity named ISE. */
    static const OrderSource ISE;
    /** Built-in identity named DEA. */
    static const OrderSource DEA;
    /** Built-in identity named DEX. */
    static const OrderSource DEX;
    /** Built-in identity named dex. */
    static const OrderSource DEX_LOWER;
    /** Built-in identity named BYX. */
    static const OrderSource BYX;
    /** Built-in identity named BZX. */
    static const OrderSource BZX;
    /** Built-in identity named bzx. */
    static const OrderSource BZX_LOWER;
    /** Built-in identity named BATE. */
    static const OrderSource BATE;
    /** Built-in identity named CHIX. */
    static const OrderSource CHIX;
    /** Built-in identity named CEUX. */
    static const OrderSource CEUX;
    /** Built-in identity named BXTR. */
    static const OrderSource BXTR;
    /** Built-in identity named IST. */
    static const OrderSource IST;
    /** Built-in identity named BI20. */
    static const OrderSource BI20;
    /** Built-in identity named ABE. */
    static const OrderSource ABE;
    /** Built-in identity named FAIR. */
    static const OrderSource FAIR;
    /** Built-in identity named GLBX. */
    static const OrderSource GLBX;
    /** Built-in identity named glbx. */
    static const OrderSource GLBX_LOWER;
    /** Built-in identity named ERIS. */
    static const OrderSource ERIS;
    /** Built-in identity named XEUR. */
    static const OrderSource XEUR;
    /** Built-in identity named xeur. */
    static const OrderSource XEUR_LOWER;
    /** Built-in identity named CFE. */
    static const OrderSource CFE;
    /** Built-in identity named C2OX. */
    static const OrderSource C2OX;
    /** Built-in identity named SMFE. */
    static const OrderSource SMFE;
    /** Built-in identity named smfe. */
    static const OrderSource SMFE_LOWER;
    /** Built-in identity named iex. */
    static const OrderSource IEX_LOWER;
    /** Built-in identity named MEMX. */
    static const OrderSource MEMX;
    /** Built-in identity named memx. */
    static const OrderSource MEMX_LOWER;
    /** Built-in identity named OCEA. */
    static const OrderSource OCEA;
    /** Built-in identity named ocea. */
    static const OrderSource OCEA_LOWER;
    /** Built-in identity named pink. */
    static const OrderSource PINK_LOWER;
    /** Built-in identity named ARCA. */
    static const OrderSource ARCA;
    /** Built-in identity named arca. */
    static const OrderSource ARCA_LOWER;
    /** Built-in identity named CEDX. */
    static const OrderSource CEDX;
    /** Built-in identity named cedx. */
    static const OrderSource CEDX_LOWER;
    /** Built-in identity named IGC. */
    static const OrderSource IGC;
    /** Built-in identity named igc. */
    static const OrderSource IGC_LOWER;
    /** Built-in identity named EDX. */
    static const OrderSource EDX;
    /** Built-in identity named edx. */
    static const OrderSource EDX_LOWER;
    /** Built-in identity named NUAM. */
    static const OrderSource NUAM;
    /** Built-in identity named nuam. */
    static const OrderSource NUAM_LOWER;
    /** Built-in identity named MOON. */
    static const OrderSource MOON;
    /** Built-in identity named moon. */
    static const OrderSource MOON_LOWER;
    /** Built-in identity named OTCM. */
    static const OrderSource OTCM;
    /** Built-in identity named otcm. */
    static const OrderSource OTCM_LOWER;
    /** Built-in identity named BOSS. */
    static const OrderSource BOSS;
    /** Built-in identity named boss. */
    static const OrderSource BOSS_LOWER;
    /** Built-in identity named NEO. */
    static const OrderSource NEO;
    /** Built-in identity named neo. */
    static const OrderSource NEO_LOWER;
};
} // namespace org::ttldtor::qdcxx::events

namespace std {
/** Standard hash adapter consistent with ID-only equality. */
template <> struct hash<org::ttldtor::qdcxx::events::OrderSource> {
    /** @param source Order identity to hash.
     * @return ID converted to size_t.
     */
    size_t operator()(const org::ttldtor::qdcxx::events::OrderSource &source) const noexcept {
        return static_cast<size_t>(source.hashCode());
    }
};
} // namespace std
