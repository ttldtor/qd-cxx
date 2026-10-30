// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "tape_sequence.hpp"
#include <array>

namespace org::ttldtor::qdcxx::tape_inspect::detail {
/** Narrow internal physical-close/operation observation seam; never a public source provider. */
struct SequenceFileTest {
    std::optional<std::size_t> failDataOpen;  ///< Refuse this occurrence's data open before file IO.
    std::optional<std::size_t> failTimeOpen;  ///< Refuse this occurrence's companion open.
    std::optional<std::size_t> failDataClose; ///< Report failure after the actual data file close attempt.
    std::optional<std::size_t> failTimeClose; ///< Report failure after the actual companion close attempt.
    std::size_t dataOpens = 0;                ///< Owning data-open attempts, including injected refusals.
    std::size_t timeOpens = 0;                ///< Owning companion-open attempts.
    std::size_t dataCloses = 0;               ///< Actual explicitly opened data files closed once.
    std::size_t timeCloses = 0;               ///< Actual explicitly opened companion files closed once.
    std::size_t reads = 0;                    ///< Concrete adapter pull attempts, not individual internal OS reads.
    std::size_t dataReads = 0;                ///< Data adapter pulls.
    std::size_t timeReads = 0;                ///< Companion adapter pulls.
    std::size_t afterPrimaryReads = 0;        ///< Forbidden pull attempts after an already-owned first cause.
    std::optional<std::size_t> timeReadsAtSourceCause; ///< Companion pull checkpoint at first container error.
    std::optional<std::size_t> dataReadsAtSourceCause; ///< Data pull checkpoint at first container error.
    std::size_t parserFeeds = 0;                       ///< Concrete parser feed calls.
    std::size_t scannerFeeds = 0;                      ///< Concrete scanner feed/finalization calls.
    std::size_t liveAdapters = 0; ///< Observed lexical adapter lifetimes, zero before physical close.
    std::array<unsigned, 4>
        cleanupOrder{};           ///< First four releases: time adapter1, data adapter2, time close3, data close4.
    std::size_t cleanupCount = 0; ///< Total observed cleanup checkpoints; array storage never grows.
};

/** Execute the real owning path with only concrete open/close faults and scalar observations.
 * @param segments Explicit borrowed specifications, never retained after this call.
 * @param output Borrowed data destination.
 * @param diagnostic Borrowed terminal diagnostic destination.
 * @param options Exact resolved and admitted capacities.
 * @param test Caller-owned fault settings/counters, valid through the synchronous operation.
 * @param stop Explicit cancellation state, with no registered callback.
 * @return Same owning result as production; the public production entry always uses no test state.
 */
[[nodiscard]] TapeSequenceResult inspectSequenceForTest(std::span<const TapeSegmentSpec> segments, std::ostream &output,
                                                        std::ostream &diagnostic, const TapeSequenceOptions &options,
                                                        SequenceFileTest &test, std::stop_token stop = {});
} // namespace org::ttldtor::qdcxx::tape_inspect::detail
