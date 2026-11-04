// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <cstddef>

namespace org::ttldtor::qdcxx::schedule::allocation_test {
/** Arm a test-only ordinary global-new failure for one numbered allocation.
 * @param ordinal One-based failure index; zero observes allocation count without failing.
 */
void arm(std::size_t ordinal) noexcept;
/** Disable injection before assertions or other test-framework work.
 * @return Number of observed ordinary allocation calls; aligned/C/runtime allocations are outside this probe.
 */
std::size_t disarm() noexcept;
} // namespace org::ttldtor::qdcxx::schedule::allocation_test
