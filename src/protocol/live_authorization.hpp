// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {

/** Private two-step authorization for a serialized owner with an external admission fence. */
struct LiveAuthorization {
    /** Compose complete owning bytes with ordinary bounded allocation, without changing authentication state. */
    [[nodiscard]] static std::expected<LivePendingOutput, LiveError>
    prepare(LiveSession &session, std::u16string_view basicToken, LiveWork &work);

    /** Commit the successful preparation from this same serialized session without any intervening progress. */
    static void commit(LiveSession &session) noexcept;
};

} // namespace org::ttldtor::qdcxx::protocol::detail
