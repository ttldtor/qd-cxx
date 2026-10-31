// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "runtime_bootstrap.hpp"

#ifdef _WIN32
#    include <windows.h>
#    include <winsock2.h>

#    include <ntsecapi.h>
#else
#    include <cstdlib>
#endif
#include <ares.h>

#include <array>
#include <exception>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport {
namespace {

/** Complete one native readiness sample without inspecting byte quality or acquiring SDK references.
 * @param argument Unused call-local context.
 * @return The Windows BOOLEAN result; true after the Linux status-free API returns normally.
 */
bool nativeReadiness([[maybe_unused]] void *argument) noexcept {
    constexpr std::size_t ENTROPY_SAMPLE_BYTES = 32;
    std::array<unsigned char, ENTROPY_SAMPLE_BYTES> sample{};
#ifdef _WIN32

    return RtlGenRandom(sample.data(), static_cast<ULONG>(sample.size())) != FALSE;
#else
    ::arc4random_buf(sample.data(), sample.size());

    return true;
#endif
}

} // namespace

RuntimeBootstrap::State::~State() {
    if (borrowers.load(std::memory_order_relaxed) != 0 || owner != std::this_thread::get_id()) {
        std::terminate();
    }

    if (dnsInitialized) {
        ares_library_cleanup();
    }

#ifdef _WIN32

    if (socketsInitialized) {
        WSACleanup();
    }

#endif
}

RuntimeBootstrap::RuntimeBootstrap(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

RuntimeBootstrap::RuntimeBootstrap(RuntimeBootstrap &&other) noexcept = default;
RuntimeBootstrap &RuntimeBootstrap::operator=(RuntimeBootstrap &&other) noexcept = default;
RuntimeBootstrap::~RuntimeBootstrap() = default;

std::expected<RuntimeBootstrap, TcpDiagnostic> RuntimeBootstrap::create() {
    return detail::BootstrapFactory::create(nativeReadiness, nullptr);
}

std::expected<RuntimeBootstrap, TcpDiagnostic> detail::BootstrapFactory::create(Readiness readiness, void *argument) {
    int runtimeVersion = 0;
    const auto *runtimeText = ares_version(&runtimeVersion);

    if (!runtimeText || runtimeVersion != ARES_VERSION || std::string_view(runtimeText) != ARES_VERSION_STR ||
        ares_threadsafety()) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_CONFIGURATION, "bootstrap-sdk-profile"});
    }

    if (!readiness || !readiness(argument)) {
        // Neither native API promises a usable GetLastError/errno result for this Boolean rejection.
        return std::unexpected(TcpDiagnostic{TcpError::BOOTSTRAP_FAILURE, "bootstrap-entropy"});
    }

    try {
        auto state = std::make_unique<RuntimeBootstrap::State>();

#ifdef _WIN32
        WSADATA data{};
        const int sockets = WSAStartup(MAKEWORD(2, 2), &data);

        if (sockets != 0) {
            return std::unexpected(TcpDiagnostic{TcpError::BOOTSTRAP_FAILURE, "bootstrap-sockets", sockets});
        }

        state->socketsInitialized = true;
#endif
        const int dns = ares_library_init(ARES_LIB_INIT_ALL);

        if (dns != ARES_SUCCESS) {
            return std::unexpected(TcpDiagnostic{TcpError::BOOTSTRAP_FAILURE, "bootstrap-dns", dns});
        }

        state->dnsInitialized = true;

        return RuntimeBootstrap(std::move(state));
    } catch (const std::bad_alloc &) {
        return std::unexpected(TcpDiagnostic{TcpError::ALLOCATION_FAILURE, "bootstrap-storage"});
    }
}

} // namespace org::ttldtor::qdcxx::transport
