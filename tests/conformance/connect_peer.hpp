// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/core/subscription.hpp"
#include "qd/protocol/frame_reader.hpp"

#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::connect_tests {

/** Exact source tuple owned by the deterministic native loopback peer. */
struct Record {
    std::u16string symbol;                   ///< Exact non-null selector.
    std::vector<core::FieldValue> fields;    ///< Complete typed fields in the selected local order.
    std::int32_t flags = 0;                  ///< Exact optional binary event flags; zero preserves old STREAM bytes.
    std::optional<std::int64_t> removalTime; ///< Required only for an actual REMOVE_EVENT record.
};
/** Explicit finite test fault after the initial data prefix. */
enum class PeerMode {
    HOLD,        ///< Keep the accepted socket open after scripted data.
    CLEAN_END,   ///< Close after complete data, before the requested application cutoff.
    PARTIAL_END, ///< Append an incomplete length prefix and close.
    SILENT,      ///< Advertise readiness/descriptions and send no data.
    NO_READINESS ///< Keep the connection open without advertising protocol readiness or sending data.
};

/** Optional explicit receiving script; defaults preserve the original STREAM peer and replay-after-remove. */
struct PeerScript {
    core::SubscriptionFamily family = core::SubscriptionFamily::STREAM; ///< Exact advertised/delivered family.
    std::optional<std::int64_t> historyBound;       ///< Expected signed raw bound in every actual HISTORY ADD tuple.
    std::optional<std::vector<Record>> afterRemove; ///< Separate late data, or the old initial-data replay by default.
    bool waitForDataRelease = false; ///< Test-controlled barrier after full ADD parsing, without a timer race.
    bool canonicalSymbols = false;   ///< Explicit canonical DATA encoding; false preserves legacy FD/cipher-zero bytes.
};

/** Encode the existing independent peer readiness/complete-description fixture without opening a socket.
 * @param schema Complete ordinary source schema, borrowed only during encoding.
 * @param family Explicit wire family; STREAM preserves the original peer bytes.
 * @return Exact owned protocol and source-description frames, using sender 17.
 */
[[nodiscard]] std::vector<std::byte>
fixturePeerPrefix(const core::LocalSchema &schema, core::SubscriptionFamily family = core::SubscriptionFamily::STREAM);
/** Encode the existing independent peer DATA fixture without opening a socket.
 * @param records Exact source tuples, borrowed only during encoding.
 * @param family Explicit wire family, STREAM by default.
 * @param canonical Explicit canonical symbol opt-in; false preserves the original FD-symbol bytes.
 * @return Exact owned complete DATA frame, using sender 17.
 */
[[nodiscard]] std::vector<std::byte> fixturePeerData(std::span<const Record> records,
                                                     core::SubscriptionFamily family = core::SubscriptionFamily::STREAM,
                                                     bool canonical = false);

/** Real nonblocking TCP peer; bootstrap must precede construction and outlive destruction.
 * Only this object's listener, accepted connection and jthread are touched. Destruction requests stop,
 * joins the finite worker and closes owned sockets. Worker faults are retained numerically for tests.
 */
class Peer {
    std::intptr_t listener_ = -1;
    std::jthread worker_;
    std::vector<std::byte> initial_;
    std::vector<std::byte> data_;
    std::vector<std::byte> afterRemove_;
    std::vector<std::u16string> expected_;
    std::size_t chunk_;
    PeerMode mode_;
    PeerScript script_;
    std::uint16_t port_ = 0;
    std::atomic<unsigned> adds_{0};
    std::atomic<unsigned> removes_{0};
    std::atomic<bool> failed_{false};
    std::atomic<bool> dataReleased_{true};
    std::atomic<bool> initialDataSent_{false};
    std::atomic<bool> afterRemoveSent_{false};

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
     * @param script Optional family/bound/late-data/release selection; omitted values preserve legacy behavior.
     */
    Peer(const core::LocalSchema &schema, std::span<const Record> records, std::span<const std::u16string> expected,
         std::size_t chunk = 8192, PeerMode mode = PeerMode::HOLD, PeerScript script = {});
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
    /** Release initial scripted data after a test observes/changes its owned child's sink.
     * The worker still waits for an actual complete valid ADD before sending DATA.
     */
    void releaseData() noexcept;
    /** Inspect complete native write of the initial whole DATA frame, without claiming peer receipt.
     * @return True only after all initial frame bytes were accepted by the owned socket send calls.
     */
    bool initialDataSent() const noexcept;
    /** Inspect complete native write of the late post-REMOVE whole frame.
     * @return True only after actual REMOVE parsing and complete native acceptance of that frame.
     */
    bool afterRemoveSent() const noexcept;
    /** Inspect worker/command-identity failure without accessing worker-owned containers.
     * @return True after a visible worker failure or unexpected exact subscription tuple.
     */
    bool failed() const noexcept;
};
} // namespace org::ttldtor::qdcxx::connect_tests
