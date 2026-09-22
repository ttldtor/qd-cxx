// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/protocol/tape_reader.hpp"
#include <string_view>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Inspect a stable static reason name.
 * @param reason Bounded reason enum.
 * @return Uppercase ASCII name, or NONE for an invalid enum; no borrowed input.
 */
[[nodiscard]] std::string_view tapeRejectionName(protocol::TapeRejection reason) noexcept;
/** Inspect a safe fixed human explanation.
 * @param reason Bounded reason enum.
 * @return Static ASCII text with no untrusted input interpolation.
 */
[[nodiscard]] std::string_view tapeRejectionExplanation(protocol::TapeRejection reason) noexcept;

} // namespace org::ttldtor::qdcxx::tape_inspect
