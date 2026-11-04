// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <stop_token>

namespace org::ttldtor::qdcxx::ipf::detail {

/** Observe creator-only facade cutoff and a thread-safe external stop request without SDK work.
 * The two booleans are sampled on the serialized creator; other threads own only stop_source.
 * @param publicLive Whether the original facade still authorizes this stable operation.
 * @param ownerCancel Creator-thread request-only cutoff.
 * @param stopToken Immutable token whose shared stop state may be requested by another thread.
 * @return True when ordinary admission must stop before the next bounded owner/HTTP/parser turn.
 */
[[nodiscard]] inline bool transferCancelled(bool publicLive, bool ownerCancel,
                                            const std::stop_token &stopToken) noexcept {
    return !publicLive || ownerCancel || stopToken.stop_requested();
}

} // namespace org::ttldtor::qdcxx::ipf::detail
