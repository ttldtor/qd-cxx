// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_configuration.hpp"

#include <csignal>
#include <iosfwd>

namespace org::ttldtor::qdcxx::connect {
struct PreparedRecovery;

/** Drive one explicitly prepared recovery invocation on its creating application thread.
 * Preparation precedes signal/bootstrap acquisition. Streams and the signal flag outlive all
 * callbacks and actual local retirement. Synchronous output cannot be preempted by a deadline.
 * @param configuration Complete owned arguments, unchanged through the run.
 * @param prepared Exact unactivated schema/profile; its blueprint transfers once into Recovery.
 * @param interrupted Signal-safe application request; null means no external signal source.
 * @param output Borrowed data sink, checked separately from local/network completion.
 * @param diagnostics Borrowed control sink; a failed publication never authorizes a reset ACK.
 * @return 0 actual successful intentional drain, 1 failure, 2 unmet readiness/target/control, or130 actual interrupted
 * abort.
 */
[[nodiscard]] int runRecoveryApplication(const RecoveryConfiguration &configuration, PreparedRecovery &prepared,
                                         const volatile std::sig_atomic_t *interrupted, std::ostream &output,
                                         std::ostream &diagnostics);
} // namespace org::ttldtor::qdcxx::connect
