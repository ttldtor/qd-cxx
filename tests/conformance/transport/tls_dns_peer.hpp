// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <memory>

namespace org::ttldtor::qdcxx::test {

/** Single-owner finite loopback UDP DNS fixture, driven explicitly alongside a TLS test.
 * Construct after RuntimeBootstrap, which outlives this fixture on Windows. No worker or callback
 * is created, and no ambient resolver is consulted. step() performs at most one nonblocking
 * receive and one bounded response; destruction closes only this fixture's descriptor.
 */
class TlsDnsPeer {
    struct State;
    std::unique_ptr<State> state_; ///< Sole stable descriptor, port, counter and deadline owner.

    public:
    /** Bind an ephemeral IPv4 loopback port, or throw a static test-only setup failure. */
    TlsDnsPeer();
    /** Release exactly the owned UDP descriptor; no thread or asynchronous borrower exists. */
    ~TlsDnsPeer();
    /** Inspect the explicit numeric DNS port.
     * @return The bound ephemeral port, in1..65535.
     */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** Respond to one eligible A/AAAA query within the fixture's five-second absolute lifetime.
     * The original route receives CNAME→alias.route.invalid→127.0.0.1; AAAA receives no address.
     */
    void step();
    /** Inspect actual bounded queries received from the runtime.
     * @return Number of syntactically eligible questions processed, not a resolver outcome.
     */
    [[nodiscard]] unsigned queries() const noexcept;
};

} // namespace org::ttldtor::qdcxx::test
