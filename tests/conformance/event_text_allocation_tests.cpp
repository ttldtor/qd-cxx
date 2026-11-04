// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest caller: no arguments. Inject ordinary creator-thread allocations one by one;
// each public operation must return ALLOCATION atomically, then succeed after the complete sweep.
#include "qd/events/event_text.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"

#include <cstdlib>
#include <limits>
#include <new>
#include <string>

namespace {
thread_local bool armed = false;        ///< Only the selected synchronous operation participates.
thread_local std::size_t remaining = 0; ///< Successful ordinary allocations before the injected failure.
namespace e = org::ttldtor::qdcxx::events;
constexpr e::EventTextLimits LIMITS{65536, 65536, 262144, 1000000}; ///< Finite logical operation bounds.

/** Inject each ordinary allocation position, then require an unchanged successful result.
 * @tparam Action Synchronous local public-API operation.
 * @param action Borrowed callable, never retained after this sweep.
 * @return True when all injected failures are typed and the final complete result matches baseline.
 */
template <typename Action> bool sweep(Action action) {
    const auto baseline = action();

    if (!baseline) {
        return false;
    }

    constexpr std::size_t MAX_INJECTION_POSITIONS = 1024;

    for (std::size_t index = 0; index < MAX_INJECTION_POSITIONS; ++index) {
        remaining = index;
        armed = true;
        const auto result = action();
        armed = false;

        if (result) {
            return *result == *baseline;
        }

        if (result.error() != e::EventTextError::ALLOCATION) {
            return false;
        }
    }

    return false;
}
} // namespace

/** @param bytes Requested ordinary bytes; zero receives one byte.
 * @return Owned malloc-compatible storage, or bad_alloc for the injected/real failure.
 */
void *operator new(std::size_t bytes) {
    if (armed) {
        if (remaining == 0) {
            throw std::bad_alloc();
        }

        --remaining;
    }

    if (auto *result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc();
}

/** @param bytes Requested array bytes.
 * @return Owned ordinary storage, or bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param pointer Matching ordinary allocation or null. */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** @param pointer Matching array allocation or null. */
void operator delete[](void *pointer) noexcept {
    std::free(pointer);
}

/** @param pointer Matching ordinary allocation or null.
 * @param size Unused compiler-provided size.
 */
void operator delete(void *pointer, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(pointer);
}

/** @param pointer Matching array allocation or null.
 * @param size Unused compiler-provided size.
 */
void operator delete[](void *pointer, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(pointer);
}

/** @return Zero only after every ordinary allocation failure returns an atomic typed error. */
int main() {
    const auto context = e::EventTextContext::utc();
    e::Quote quote{};
    quote.symbol = std::u16string(64, u'Q');
    e::Profile profile{};
    profile.symbol = std::u16string(64, u'P');
    profile.description = std::u16string(256, u'D');
    profile.statusReason = std::u16string(256, u'R');
    const auto symbol = quote.symbol;
    const auto description = profile.description;

    const auto ok = sweep([&]() -> std::expected<std::u16string, e::EventTextError> {
                        const auto bound = e::EventTextContext::timeZone(nullptr, u"GMT+05:30", 0);

                        if (!bound) {
                            return std::unexpected(bound.error());
                        }

                        return e::formatEventTime(-1, true, *bound, LIMITS);
                    }) &&
                    sweep([&] {
                        return e::formatEventDouble(std::numeric_limits<double>::max(), LIMITS);
                    }) &&
                    sweep([&] {
                        return e::toString(quote, context, LIMITS);
                    }) &&
                    sweep([&] {
                        return e::toString(profile, context, LIMITS);
                    }) &&
                    sweep([&] {
                        return e::toStringUtf8(profile, context, LIMITS, e::MalformedUtf16::REJECT);
                    }) &&
                    sweep([&] {
                        return e::toUtf8(*profile.description, LIMITS, e::MalformedUtf16::REJECT);
                    });

    return ok && quote.symbol == symbol && profile.description == description ? 0 : 1;
}
