// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <string_view>
#include <variant>

namespace org::ttldtor::qdcxx::feed_tests {

/** @param value Owning registered event borrowed for the duration of the result.
 * @return View of the event's exact logical symbol.
 */
inline std::u16string_view registeredEventSymbol(const api::RegisteredEvent &value) {
    return std::visit(
        [](const auto &event) -> std::u16string_view {
            if constexpr (requires { event.symbol; }) {
                return event.symbol;
            } else {
                return event.order.symbol;
            }
        },
        value);
}

} // namespace org::ttldtor::qdcxx::feed_tests
