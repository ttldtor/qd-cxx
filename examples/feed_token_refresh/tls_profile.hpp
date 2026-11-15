// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"
#include "qd/transport/tls_resources.hpp"

#include <chrono>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace org::ttldtor::qdcxx::examples {
/** Use the controlled TLS profile for this standalone process.
 * @param identity DNS or IP identity required in the server certificate.
 * @param caPath File containing the explicit PEM trust anchors.
 * @param credentials Shared provider used by both verified connection attempts.
 * @return Verified TLS options with finite resource declarations.
 */
inline api::VerifiedTlsOptions tlsOptions(const char *identity, const char *caPath,
                                          std::shared_ptr<client::CredentialProvider> credentials) {
    using namespace std::chrono_literals;

    constexpr transport::TlsResourceLimits LIMITS{.trustBytes = 512 * 1024,
                                                  .anchors = 256,
                                                  .certificateBytes = 65536,
                                                  .certificateRequestBytes = 65536,
                                                  .identityBytes = 255};
    // This executable is the sole SDK user and runs one endpoint worker.
    constexpr transport::TlsHostLimits HOST{.namemapGroups = 6297,
                                            .queryCount = 16,
                                            .queryBytes = 64,
                                            .propertyNames = 64,
                                            .propertyValues = 128,
                                            .propertyStringBytes = 32768,
                                            .propertyStringMaxBytes = 1000,
                                            .participatingThreads = 8,
                                            .globalThreadHandlers = 128,
                                            .ownerThreadHandlers = 64,
                                            .ambientThreadPairs = 8,
                                            .ambientGenerators = 16,
                                            .concurrentEntries = 4,
                                            .errorThreads = 8,
                                            .errorCapacityBytes = 65536,
                                            .errorSlotCapacityBytes = 4096,
                                            .errorDebugBytes = 65536,
                                            .failedPrivatePrefixes = 64,
                                            .failedAmbientPrefixes = 8,
                                            .errorTableEntries = 2048};
    constexpr std::size_t NATIVE_ALLOWANCE = 160 * 1024 * 1024;
    const auto requirements = transport::tlsRequirements(LIMITS, HOST);
    std::ifstream ca(caPath, std::ios::binary);
    std::string trust(LIMITS.trustBytes + 1, '\0');
    ca.read(trust.data(), static_cast<std::streamsize>(trust.size()));
    trust.resize(static_cast<std::size_t>(ca.gcount()));

    if (!requirements || ca.bad() || trust.empty() || trust.size() > LIMITS.trustBytes) {
        throw std::runtime_error("TLS profile or trust file is invalid.");
    }

    return {identity,
            std::move(trust),
            5s,
            1s,
            LIMITS,
            HOST,
            requirements->sdk.instanceBytes,
            requirements->sdk.sharedBytes,
            requirements->sdk.totalBytes + NATIVE_ALLOWANCE,
            api::BasicTlsCredentials{std::move(credentials)}};
}
} // namespace org::ttldtor::qdcxx::examples
