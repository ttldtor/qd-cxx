// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::connect {

/** Read an explicit UTF-8 path during synchronous application preparation.
 * Internal callers are prepareRecovery's JSON loader and prepareRecoveryTlsInput, before activation.
 * Canonical resolution intentionally accepts symlinks/junctions whose resolved target is regular,
 * matching the existing schema-input policy. The caller controls input-file stability; metadata
 * checks detect observed changes but do not provide an atomic or adversarially race-free snapshot.
 * No path, file content or exception text is returned in a diagnostic. No runtime/provider is called.
 * @param pathBytes Borrowed strict UTF-8 path, 1..4096 bytes, without NUL; never retained.
 * @param maximumBytes Explicit payload ceiling in 1..1048576 bytes, independent of SDK/work scopes.
 * @return Exact owned bytes, possibly empty, without NUL or trimming; otherwise a static refusal.
 */
[[nodiscard]] std::expected<std::string, std::string_view> readRecoveryFile(std::string_view pathBytes,
                                                                            std::size_t maximumBytes);

} // namespace org::ttldtor::qdcxx::connect
