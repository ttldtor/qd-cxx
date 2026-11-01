// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/recovery.hpp"

#include <expected>
#include <vector>

namespace org::ttldtor::qdcxx::transport::detail {

/** Inspect the library's qualified compile profile without allocating or inspecting caller inputs.
 * This does not attest arbitrary consumer translation units or later target-local overrides.
 * @return One for the qualified MSVC profile, two for GCC, or zero for an unsupported compilation.
 */
[[nodiscard]] unsigned recoveryCopyProfile() noexcept;

/** Admit the simultaneous copy and adopted-input backing before any configuration allocation.
 * Selected fresh vector/string paths have a source-qualified capacity envelope. Actual input
 * capacities remain independently charged; sparse over-reserved inputs cannot hide behind size.
 * @param endpoint Immutable borrowed input, unchanged and not retained by this inspection.
 * @param blueprint Actual caller vector to adopt only after all later preparation succeeds.
 * @param limits Explicit independent capacity ceilings.
 * @param work Remaining scan credits, consumed once without refunds.
 * @return Required vector including the runtime-entry three-schema peak, or a static refusal.
 */
[[nodiscard]] std::expected<RecoveryCapacity, RecoveryDiagnostic>
inspectRecoveryCopies(const client::EndpointOptions &endpoint, const std::vector<protocol::OutgoingRecord> &blueprint,
                      const RecoveryLimits &limits, RecoveryWork &work) noexcept;

/** Check the inspectable capacities of an already prepared bounded schema copy.
 * The pre-copy proof controls inaccessible outer vector backing; this postcondition is additional.
 * @param schema Complete prepared copy.
 * @param envelope One-copy tuple previously admitted before construction.
 * @return True when inspectable nested backing fits the qualified envelope.
 */
[[nodiscard]] bool recoverySchemaFits(const core::LocalSchema &schema, RecoverySchemaCapacity envelope) noexcept;

} // namespace org::ttldtor::qdcxx::transport::detail
