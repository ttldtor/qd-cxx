// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/describe_records.hpp"

#include <cassert>

namespace org::ttldtor::qdcxx::protocol::detail {
/** Call-local metadata admission; cumulative work belongs to the reader, not this temporary. */
class SessionBudget {
    std::size_t owned_;
    std::size_t maximum_;
    std::size_t &work_;
    std::size_t workMaximum_;

    public:
    /** Borrow the reader's monotonic counter and account its already published generation.
     * @param owned Published metadata units, already admitted.
     * @param maximum Simultaneous metadata limit.
     * @param work Reader-owned cumulative construction counter.
     * @param workMaximum Finite cumulative construction limit.
     */
    SessionBudget(std::size_t owned, std::size_t maximum, std::size_t &work, std::size_t workMaximum) noexcept
        : owned_(owned), maximum_(maximum), work_(work), workMaximum_(workMaximum) {
        assert(owned <= maximum && work <= workMaximum);
    }

    /** Admit an owning copy before allocation; rejected work is not charged.
     * @param units Logical owning elements or UTF-16 units to construct.
     * @return False without changing either counter when either limit would be exceeded.
     */
    [[nodiscard]] bool admit(std::size_t units) noexcept {
        if (units > maximum_ - owned_ || units > workMaximum_ - work_) {
            return false;
        }

        owned_ += units;
        work_ += units;

        return true;
    }

    /** Charge parsing work without inventing retained metadata.
     * @param units Admitted source bytes to examine.
     * @return False without charging when cumulative work does not fit.
     */
    [[nodiscard]] bool work(std::size_t units) noexcept {
        if (units > workMaximum_ - work_) {
            return false;
        }

        work_ += units;

        return true;
    }

    /** Release destroyed staging owners without refunding their construction work.
     * @param units Previously admitted logical ownership.
     */
    void release(std::size_t units) noexcept {
        assert(units <= owned_);
        owned_ -= units;
    }
};

/** Parse with shared old/staging metadata admission; ordinary public parsing remains unchanged.
 * @param payload Borrowed complete description payload.
 * @param limits Existing finite wire/name bounds.
 * @param budget Call-local peak ledger and reader-owned monotonic work counter.
 * @return Owned completed prefix; the caller publishes none on failure.
 */
[[nodiscard]] DescriptionResult parseSessionDescriptions(std::span<const std::byte> payload, DescriptionLimits limits,
                                                         SessionBudget &budget);
} // namespace org::ttldtor::qdcxx::protocol::detail
