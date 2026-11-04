// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/ipf/instrument_profile_collector.hpp"

namespace org::ttldtor::qdcxx::ipf::collector_detail {
/** Independent collector-local connection namespace; public integer tags remain in domain zero. */
struct ConnectionCollectorAccess {
    /** Reserve a non-reused namespace before a connection port is published.
     * @param collector Explicit stable repository owner.
     * @return Nonzero identity or closed/work/exhaustion failure, without allocation/callbacks.
     */
    static std::expected<std::uint64_t, ProfileCollectorError> reserve(InstrumentProfileCollector &collector);
    /** Publish under one previously reserved independent namespace.
     * @param collector Same repository used for reservation.
     * @param profiles Immutable span through synchronous dispatch.
     * @param domain Nonzero admitted namespace.
     * @param generation Attempt-local nonzero generation.
     * @return Existing atomic mutation result, without changing public domain-zero tags.
     */
    static std::expected<ProfileCollectorChange, ProfileCollectorError>
    update(InstrumentProfileCollector &collector, std::span<const InstrumentProfile> profiles, std::uint64_t domain,
           std::uint64_t generation);
    /** Remove only matching tags in this connection's namespace.
     * @param collector Same repository used for reservation.
     * @param generations Immutable attempt tags.
     * @param domain Nonzero admitted namespace.
     * @return Existing atomic removal result; other/manual namespaces remain unchanged.
     */
    static std::expected<ProfileCollectorChange, ProfileCollectorError>
    remove(InstrumentProfileCollector &collector, std::span<const std::uint64_t> generations, std::uint64_t domain);
};
} // namespace org::ttldtor::qdcxx::ipf::collector_detail
