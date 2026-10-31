// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "configuration.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"

#include <iosfwd>

namespace org::ttldtor::qdcxx::connect {

/** App-owned synchronous sink; its stream must outlive all callbacks and final flush.
 * Output/flush failure throws a static exception, producing dispatcher callback failure without retry.
 * Blocking stream implementations are application-owned and cannot be preempted by a run deadline.
 */
class OutputConsumer final : public client::Consumer {
    std::ostream &output_;
    core::LocalSchema schema_;
    OutputProfile profile_;
    std::optional<events::PreparedQuote> quote_;
    std::optional<events::PreparedProfile> profileMapping_;
    std::size_t ordinal_ = 0;

    public:
    /** Prepare provenance and optional standard mapping before any source work.
     * @param output Borrowed output stream, preserved through callbacks and flush.
     * @param schema Immutable schema copied with identity preserved.
     * @param profile Exact generic output or explicit prepared summary matching the schema.
     * @throws std::runtime_error Static preparation failure; allocation exceptions propagate before activation.
     */
    OutputConsumer(std::ostream &output, const core::LocalSchema &schema, OutputProfile profile);
    /** Write explicit version/schema metadata before activating the TCP runtime.
     * @throws std::runtime_error Static sink failure; no partial line is advertised as a completed record.
     */
    void begin();
    /** Validate provenance and emit complete typed generic values or prepared summaries.
     * @param batch Borrowed immutable callback values, never retained after return.
     * @throws std::runtime_error Static provenance/mapping/sink failure; side effects are never retried.
     */
    void onBatch(const client::BorrowedBatch &batch) override;
    /** Check final sink publication separately from local/transport terminal state.
     * @throws std::runtime_error Static sink failure.
     */
    void flush();
};

/** Classify application completion without concealing pending or uncertain native output.
 * @param result Separate application/local/transport evidence.
 * @param targetRequested Whether the user required a record target.
 * @return 0 intentional cutoff/target, 1 failure, 2 unmet target/readiness, or 130 interrupt.
 */
[[nodiscard]] int exitCode(const examples::StreamResult &result, bool targetRequested) noexcept;

/** Emit separate numeric completion scopes and native output uncertainty.
 * @param output Borrowed diagnostic sink.
 * @param result Immutable application completion evidence.
 * @throws std::runtime_error Static sink failure.
 */
void writeResult(std::ostream &output, const examples::StreamResult &result);

} // namespace org::ttldtor::qdcxx::connect
