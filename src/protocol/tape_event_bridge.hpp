// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"
#include "qd/protocol/tape_reader.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {

/** Convert whole successful tape results into counted collector event owners, with a private local epoch. */
class TapeEventBridge {
    LiveSnapshot epoch_;
    std::size_t eventBytes_;

    TapeEventBridge(LiveSnapshot epoch, std::size_t eventBytes) noexcept;

    public:
    /** Open a catalog parser sharing the exact fixed local schema used by the collector.
     * @param schema Validated receiving schema, copied with its original provenance.
     * @param limits Finite parser and cumulative allowances.
     * @param dataPerFrame Additional decoding allowances for each individual frame.
     * @param fieldTime Selected FIELD decoding policy.
     * @return The owning reader; wire descriptions can change without replacing local schema identity.
     */
    static TapeReader makeFixedSchemaReader(const core::LocalSchema &schema, TapeLimits limits,
                                            StreamDataLimits dataPerFrame, FieldTimePolicy fieldTime);

    /** Create a local epoch with an empty descriptor, no peer or network policy.
     * @param limits Exact shared event backing and ownership allowances.
     * @param work Remaining preparation credits.
     * @return Complete local epoch or typed refusal.
     */
    static std::expected<TapeEventBridge, LiveError> create(LiveLimits limits, LiveWork &work);

    /** Copy one whole successful source data result into the fixed arena.
     * @param result Complete successful TapeReader data result.
     * @param messageType Selected collector family message ID; RAW_DATA can select any one family.
     * @param work Remaining bounded copy/view credits.
     * @return Complete independent event or a refusal preserving input and older owners.
     */
    std::expected<LiveEvent, LiveError> copy(const TapeFeedResult &result, std::int32_t messageType,
                                             LiveWork &work) const;
};

} // namespace org::ttldtor::qdcxx::protocol::detail
