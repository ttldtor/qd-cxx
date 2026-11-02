// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS

#    include "recovery_configuration.hpp"

#    include "qd/transport/tls_resources.hpp"

#    include <expected>
#    include <string_view>

namespace org::ttldtor::qdcxx::connect {

/** Pure owned TLS inputs, without an SDK host qualification, runtime or native bootstrap.
 * Trust/reference/token storage is application-owned; it is outside TLS SDK backing/work scopes.
 * The copied provider is immutable and retains no file path, config reference or runtime owner.
 */
struct PreparedTlsInput {
    client::EndpointTls endpoint;        ///< Exact trust/reference/deadlines and optional shared bounded token owner.
    transport::TlsResourceLimits limits; ///< Actual trust bytes/marker count/reference bytes and fixed peer caps.
};

/** Prepare explicit TLS input files without SDK/TLS/network calls or provider invocation.
 * Internal caller: prepareRecovery, before EndpointOptions::create and every runtime/bootstrap.
 * input must be an unmodified successful parseRecoveryArguments result: that existing parser owns
 * numeric IP/A-label identity grammar; EndpointOptions::create remains the final shape validator.
 * PEM marker counting bounds input; the existing TLS adapter still owns framing/ASN.1/crypto validation.
 * All failures are static and contain no path, identity, credential text or credential length.
 * @param input Borrowed parsed TLS configuration; no reference escapes this call.
 * @return Complete owned application input, or a static refusal without partial publication.
 */
[[nodiscard]] std::expected<PreparedTlsInput, std::string_view>
prepareRecoveryTlsInput(const RecoveryConfiguration &input);

} // namespace org::ttldtor::qdcxx::connect

#endif
