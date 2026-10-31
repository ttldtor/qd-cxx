// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/protocol/frame_reader.hpp"

#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::connect_tests {

/** Exact source tuple owned by the deterministic native loopback peer. */
struct Record {
    std::u16string symbol;                ///< Exact non-null selector.
    std::vector<core::FieldValue> fields; ///< Complete typed fields in the selected local order.
};
/** Explicit finite test fault after the initial data prefix. */
enum class PeerMode {
    HOLD,        ///< Keep the accepted socket open after scripted data.
    CLEAN_END,   ///< Close after complete data, before the requested application cutoff.
    PARTIAL_END, ///< Append an incomplete length prefix and close.
    SILENT       ///< Advertise readiness/descriptions and send no data.
};

/** Real nonblocking TCP peer; bootstrap must precede construction and outlive destruction.
 * Only this object's listener, accepted connection and jthread are touched. Destruction requests stop,
 * joins the finite worker and closes owned sockets. Worker faults are retained numerically for tests.
 */
class Peer {
    std::intptr_t listener_ = -1;
    std::jthread worker_;
    std::vector<std::byte> initial_;
    std::vector<std::byte> data_;
    std::vector<std::u16string> expected_;
    std::size_t chunk_;
    PeerMode mode_;
    std::uint16_t port_ = 0;
    std::atomic<unsigned> adds_{0};
    std::atomic<unsigned> removes_{0};
    std::atomic<bool> failed_{false};

    /** Own the accepted socket and perform finite scripted I/O.
     * @param stop Cooperative test-owned retirement token.
     */
    void serve(std::stop_token stop) noexcept;

    public:
    /** Bind numeric IPv4 loopback and start one explicitly owned peer thread.
     * @param schema Validated immutable schema, encoded/copied before the worker starts.
     * @param records Initial exact data tuples, encoded/copied before worker startup.
     * @param expected Complete expected client add/remove symbols, copied before startup.
     * @param chunk Maximum native peer write bytes per turn; one tests byte fragmentation.
     * @param mode Explicit finite peer-end/data fault.
     */
    Peer(const core::LocalSchema &schema, std::span<const Record> records, std::span<const std::u16string> expected,
         std::size_t chunk = 8192, PeerMode mode = PeerMode::HOLD);
    /** Stop/join only this peer's worker and close its listener. */
    ~Peer();
    /** Inspect the assigned loopback port.
     * @return Numeric owned listener port.
     */
    std::uint16_t port() const noexcept;
    /** Inspect actual complete add frames received by the peer.
     * @return Atomic observation count.
     */
    unsigned adds() const noexcept;
    /** Inspect actual complete remove frames received by the peer.
     * @return Atomic observation count.
     */
    unsigned removes() const noexcept;
    /** Inspect worker/command-identity failure without accessing worker-owned containers.
     * @return True after a visible worker failure or unexpected exact subscription tuple.
     */
    bool failed() const noexcept;
};
} // namespace org::ttldtor::qdcxx::connect_tests
