// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>

namespace org::ttldtor::qdcxx::transport::detail {

/** Optional Feed-owned admission fence for a single native TLS attempt. */
struct TlsCredentialAttempt {
    using Action = void (*)(void *) noexcept;
    using Enter = bool (*)(void *, std::uint64_t, void *, Action) noexcept;

    void *owner = nullptr;      ///< Borrowed stable Feed owner, outliving the private TLS attempt.
    Enter enter = nullptr;      ///< Run an allocation-free action only while the logical attempt remains current.
    std::uint64_t revision = 0; ///< Captured at actual native factory entry, never reread for this attempt.

    /** Check admission or perform the final bounded commit. The standalone TLS API has no producer fence. */
    bool admit(void *context = nullptr, Action action = nullptr) const noexcept {
        if (enter)
            return enter(owner, revision, context, action);

        if (action)
            action(context);

        return true;
    }
};

/** Stable Feed control observed only by the recovery worker at each actual factory entry. */
struct TlsCredentialGate {
    void *owner = nullptr;
    TlsCredentialAttempt::Enter enter = nullptr;
    const std::uint64_t *appliedRevision = nullptr; ///< Worker-local revision, outliving all native attempts.

    [[nodiscard]] TlsCredentialAttempt forAttempt() const noexcept {
        return enter ? TlsCredentialAttempt{owner, enter, *appliedRevision} : TlsCredentialAttempt{};
    }
};

} // namespace org::ttldtor::qdcxx::transport::detail
