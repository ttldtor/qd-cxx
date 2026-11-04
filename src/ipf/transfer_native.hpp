// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../transport/raw_byte_owner.hpp"

namespace org::ttldtor::qdcxx::ipf::detail {

/** Inspect the plain owner's already completed/core prefix after a native first cause.
 * The dedicated RawByteOwner drain copies this prefix without ordinary polling. Codec owners are
 * deliberately excluded: ordinary codec progress may submit output or call SSL_write.
 * @param core Stable creator-thread native owner.
 * @return True only for an eligible plain completed/core prefix; never a new read/output grant.
 */
[[nodiscard]] inline bool transferHasOwnedInput(const transport::detail::RuntimeCore &core) noexcept {
    return !core.engine && !core.closeRequested && core.state() != transport::TcpState::CLOSED &&
           ((core.readReady && core.readCount != 0) || core.inputBegin != core.inputEnd);
}

/** Drive the selected plain-prefix path after the caller recorded its native first cause.
 * @param owner Stable native byte owner.
 * @param work Remaining cumulative native credits.
 * @param bytes Remaining cumulative copy credits.
 * @return Cached-prefix admission/yield only; no ordinary poll, output, SDK or user callback.
 */
[[nodiscard]] inline std::expected<transport::TcpPollResult, transport::TcpDiagnostic>
drainTransferPlainInput(transport::detail::RawByteOwner &owner, transport::TcpWork &work, protocol::LiveWork &bytes) {
    return owner.drainOwnedInput(work, bytes);
}

/** Borrow only plaintext already verified and cached before a native first cause.
 * This path performs no codec advance, SSL call, native submission or application-output acceptance.
 * The caller checks public/stop/deadline cutoff and charges each copy before consuming this prefix.
 * @param core Stable owner whose codec remains alive through matching consumePlaintext/drain.
 * @return Immutable verified suffix, or empty; unverified ciphertext fails closed.
 */
[[nodiscard]] inline std::span<const std::byte>
transferVerifiedInput(const transport::detail::RuntimeCore &core) noexcept {
    return core.engine && core.engine->verified() ? core.engine->plaintext() : std::span<const std::byte>{};
}

} // namespace org::ttldtor::qdcxx::ipf::detail
