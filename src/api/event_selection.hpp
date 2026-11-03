// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

namespace org::ttldtor::qdcxx::api::detail {
/** Immutable supported type set; it selects routes independently of their local record IDs. */
enum class EventSelection {
    QUOTE,   ///< Every registered Quote route.
    PROFILE, ///< Every registered Profile route.
    BOTH     ///< Every registered route of either supported type.
};

/** Exact owning value shape expected by the logical subscription's listeners. */
enum class EventDelivery {
    QUOTE,     ///< Existing single-type Quote values.
    PROFILE,   ///< Existing single-type Profile values.
    REGISTERED ///< Owning public RegisteredEvent variants, restricted by EventSelection.
};
} // namespace org::ttldtor::qdcxx::api::detail
