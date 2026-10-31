// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/stream_dispatcher.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::examples {

/** Explicit settings for one controlled credential-free STREAM application run. */
struct StreamOptions {
    std::string_view host;                    ///< Borrowed numeric IPv4/IPv6 endpoint, preserved through runStream.
    std::uint16_t port;                       ///< Numeric port in 1..65535.
    std::int32_t recordId;                    ///< Exact ordinary caller-schema record identity.
    std::span<const std::u16string> symbols;  ///< Borrowed 1..16 exact selectors, at most 128 units each.
    std::chrono::milliseconds duration{5000}; ///< Admission interval in 1..600000 milliseconds.
    std::optional<std::chrono::milliseconds> removeAfter; ///< Optional all-selector removal before duration.
    std::optional<std::size_t> maximumRecords; ///< Positive completed-record target; admitted prefix can exceed it.
    const volatile std::sig_atomic_t *interrupted = nullptr; ///< Optional signal flag, borrowed until return.
    std::size_t ownerOperationsPerTurn = 10000; ///< Local progress attempts shared across one owner turn, in 1..10000.
};

/** Application failure, separate from local callback and native transport evidence. */
enum class StreamFailure {
    NONE,          ///< No application-level failure observed.
    CONFIGURATION, ///< Invalid explicit settings, schema shape or occupied output handle slot.
    PREPARATION,   ///< Bounded owner or command preparation failed.
    DISPATCH,      ///< Local command, data admission or progress failed.
    OUTPUT         ///< Explicit protocol/description/subscription composition failed.
};

/** Completion preserves local delivery and native close as distinct evidence. */
struct StreamResult {
    StreamFailure failure = StreamFailure::NONE; ///< First application-level failure.
    bool addApplied = false;                     ///< Initial desired-state mutation committed; never wire emission.
    bool addComposed = false;                    ///< STREAM_ADD entered local output; never peer acknowledgement.
    bool removeApplied = false;                  ///< Optional local removal committed before output composition.
    bool removeComposed = false;                 ///< STREAM_REMOVE entered the runtime output slot.
    bool interrupted = false;        ///< Caller signal flag was observed by the serialized application owner.
    bool intervalExpired = false;    ///< Intentional admission interval cutoff was observed.
    bool targetReached = false;      ///< Requested record target reached the callback completion barrier.
    std::size_t eventWorkYields = 0; ///< Saturating actual post-admission local work-yield observations.
    std::optional<client::DispatchTerminal> local;      ///< Local quiescence, independent of native operations.
    std::optional<transport::TcpTerminal> transport;    ///< Native handler retirement and write evidence.
    std::optional<transport::TcpDiagnostic> diagnostic; ///< Static/numeric preparation/composition rejection.
};

/** Drive one real TCP runtime and local STREAM dispatcher in the calling thread.
 * Create bootstrap on the application main thread before other application threads and keep it alive
 * through this call. The complete validated schema is copied with provenance preserved. Fixed example
 * bounds admit at most 16 records/128 fields/256 lookups and 128-unit names; overflow is visible.
 * No callback runs concurrently or remains retained by the dispatcher after local retirement.
 * Consumer exceptions fail locally without retry. Intentional cutoff stops source admission and aborts
 * native output before retiring the admitted local prefix; this is not whole-run network drain.
 * Native completion and arbitrary blocking consumers have no hard elapsed-time guarantee.
 * @param bootstrap Explicit process owner, borrowed through native retirement.
 * @param schema Immutable caller schema, borrowed while preparing independent bounded copies.
 * @param options Selection/settings borrowed unchanged until return.
 * @param consumer Shared callable guarded through invocation and local retirement.
 * @param publishedHandle Initially empty caller-owned slot; populated before any callback, preserved on early
 * rejection. Keep the slot object alive until return and synchronize any cross-thread publication yourself.
 * Use requestStop from callbacks; clearing this slot does not release the driver's separate public handle.
 * After return it retains only the detached command port/terminal, never the dispatcher/socket/callable.
 * @return Separate local/native completion or explicit preparation failure, without a peer ACK claim.
 */
[[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &bootstrap, const core::LocalSchema &schema,
                                     StreamOptions options, const std::shared_ptr<client::Consumer> &consumer,
                                     std::optional<client::ClientHandle> &publishedHandle);

} // namespace org::ttldtor::qdcxx::examples
