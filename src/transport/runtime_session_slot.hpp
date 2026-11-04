// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

#include <optional>
#include <utility>
#include <variant>

namespace org::ttldtor::qdcxx::transport::detail {

class RawByteState;

/** Exclusive QTP session or stable raw-state borrow using the existing session-sized storage.
 * The raw-state owner must outlive core/engine/native retirement. Boolean inspection deliberately
 * recognizes only QTP, so existing QTP-only branches never dereference a raw alternative.
 */
class RuntimeSessionSlot {
    std::variant<std::monostate, protocol::LiveSession, RawByteState *> value_; ///< Exclusive protocol selection.

    public:
    /** Inspect whether an actual QTP session was installed.
     * @return True only for the QTP alternative, never for a raw borrow.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return std::holds_alternative<protocol::LiveSession>(value_);
    }

    /** Borrow the installed QTP session; QTP selection is a private caller precondition.
     * @return Stable session address while this slot lives.
     */
    [[nodiscard]] protocol::LiveSession *operator->() noexcept {
        return std::get_if<protocol::LiveSession>(&value_);
    }

    /** Inspect the installed immutable QTP session.
     * @return Stable session address, or null for an empty/raw slot.
     */
    [[nodiscard]] const protocol::LiveSession *operator->() const noexcept {
        return std::get_if<protocol::LiveSession>(&value_);
    }

    /** Install an already prepared QTP session without allocating.
     * @param session Complete session, transferred into this empty slot.
     */
    void emplace(protocol::LiveSession &&session) noexcept {
        value_.emplace<protocol::LiveSession>(std::move(session));
    }

    /** Select raw input/output without constructing any QTP configuration.
     * @param state Stable separately owned byte state, retained through core destruction.
     */
    void selectRaw(RawByteState &state) noexcept {
        value_.emplace<RawByteState *>(&state);
    }

    /** Inspect the explicit raw selection without treating it as a QTP session.
     * @return Stable raw state, or null for empty/QTP selection.
     */
    [[nodiscard]] RawByteState *raw() const noexcept {
        const auto selected = std::get_if<RawByteState *>(&value_);

        return selected ? *selected : nullptr;
    }
};

static_assert(sizeof(RuntimeSessionSlot) == sizeof(std::optional<protocol::LiveSession>));
static_assert(alignof(RuntimeSessionSlot) == alignof(std::optional<protocol::LiveSession>));

} // namespace org::ttldtor::qdcxx::transport::detail
