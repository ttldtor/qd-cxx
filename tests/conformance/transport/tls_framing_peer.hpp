// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace org::ttldtor::qdcxx::test {

/** Fixed ordinary TLS record/header boundary fixture; it is deliberately not a successful TLS server.
 * Construct after RuntimeBootstrap, which outlives this owner on Windows. The caller pumps step();
 * there is no worker, asynchronous callback, DER/SPKI body, fuzzing or SDK hook. Both native
 * descriptors remain owned until destruction, including exceptions and test failure unwinding.
 */
class TlsFramingPeer {
    struct State;
    std::unique_ptr<State> state_; ///< Sole listener/connection and immutable nine-byte script owner.

    public:
    /** Bind a single loopback listener, or throw a static test-only setup failure.
     * @param overLimit True declares65608, false declares65607; neither script supplies a body.
     */
    explicit TlsFramingPeer(bool overLimit);
    /** Close only this fixture's accepted/listening descriptors; no callback can borrow them. */
    ~TlsFramingPeer();
    /** Inspect the numeric ephemeral loopback port.
     * @return Listener port in1..65535.
     */
    [[nodiscard]] std::uint16_t port() const noexcept;
    /** Attempt one accept/read/write progression without blocking, within a fixed five-second lifetime. */
    void step();
    /** Inspect actual locally accepted script bytes.
     * @return Prefix length in0..9; no peer receipt is implied.
     */
    [[nodiscard]] std::size_t sentBytes() const noexcept;
    /** Inspect an actual native ClientHello prefix reception.
     * @return Positive after the client emitted data; no payload bytes are retained or logged.
     */
    [[nodiscard]] std::size_t receivedBytes() const noexcept;
};

} // namespace org::ttldtor::qdcxx::test
