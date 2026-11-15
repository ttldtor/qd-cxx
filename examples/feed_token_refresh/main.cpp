// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_profile.hpp"

#include "qd/api/feed.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace api = org::ttldtor::qdcxx::api;
namespace client = org::ttldtor::qdcxx::client;
namespace examples = org::ttldtor::qdcxx::examples;
using namespace std::chrono_literals;

/** Owns the current complete Basic authorization value for the Feed worker. */
class Credentials final : public client::CredentialProvider {
    mutable std::mutex mutex_;
    std::u16string token_;
    bool provided_ = false;

    public:
    /** Publish credentials for future authentication challenges.
     * @param token Complete Basic authorization value, moved into this provider.
     */
    void update(std::u16string token) {
        const std::lock_guard lock(mutex_);
        token_ = std::move(token);
        provided_ = false;
    }

    /** Test whether Feed requested and received the current provider value.
     * @return True once a challenge supplied the value set by the last update.
     */
    bool wasProvided() const {
        const std::lock_guard lock(mutex_);

        return provided_;
    }

    /** Copy the current value into an SDK-owned buffer during a challenge.
     * @param destination Buffer borrowed for this invocation.
     * @return Written code units or a static error when the value cannot be supplied.
     */
    std::expected<std::size_t, client::CredentialError> basicToken(std::span<char16_t> destination) override {
        const std::lock_guard lock(mutex_);

        if (token_.empty() || token_.size() > destination.size()) {
            return std::unexpected(client::CredentialError::UNAVAILABLE);
        }

        std::ranges::copy(token_, destination.begin());
        provided_ = true;

        return token_.size();
    }
};

/** Read one bounded Basic authorization value without printing it.
 * @return Complete UTF-16 token, or no value on bad or missing input.
 */
std::optional<std::u16string> readToken() {
    constexpr std::string_view PREFIX = "Basic ";
    std::array<char, api::BasicTlsCredentials::MAXIMUM_TOKEN_UNITS + 2> bytes{};

    if (!std::cin.getline(bytes.data(), bytes.size())) {
        return std::nullopt;
    }

    const auto read = static_cast<std::size_t>(std::cin.gcount());
    std::string_view value(bytes.data(), read - (std::cin.eof() ? 0 : 1));

    if (value.ends_with('\r')) {
        value.remove_suffix(1);
    }

    if (!value.starts_with(PREFIX) || value.size() <= PREFIX.size() ||
        value.size() > api::BasicTlsCredentials::MAXIMUM_TOKEN_UNITS ||
        !std::ranges::all_of(value.substr(PREFIX.size()), [](unsigned char c) {
            return c >= '!' && c <= '~';
        })) {
        return std::nullopt;
    }

    return std::u16string(value.begin(), value.end());
}

/** Wait for QTP authorization and read the verified source epoch.
 * @param endpoint Active SDK endpoint to observe.
 * @param credentials Provider for the current authorization value.
 * @return Current connected epoch, or no value if the wait fails.
 */
std::optional<std::uint64_t> awaitEpoch(const api::Endpoint &endpoint, const Credentials &credentials) {
    const auto wait = endpoint.awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 15s);

    if (wait.status != api::WaitStatus::REACHED || !credentials.wasProvided()) {
        std::cerr << "Authentication did not complete.\n";

        return std::nullopt;
    }

    return endpoint.getStatistics().connection.sourceEpoch;
}
} // namespace

/** Rotate QTP Basic credentials across two verified TLS connections.
 * @param argc Argument count.
 * @param argv Program name followed by address, expected server identity and PEM trust path.
 * @return Zero on success, one on credential or connection failure, two on invalid arguments.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout
            << "Usage: qdcxx-feed-token-refresh HOST:PORT SERVER_IDENTITY CA.pem\n"
               "Read two complete QTP Basic values from stdin (Basic <base64>, up to 1024 characters each).\n"
               "Connect over verified TLS, update the provider, then reconnect. Print both authenticated epochs.\n"
               "Help opens no connection. Exit 0: success, 1: credential or connection failure, 2: usage error.\n";
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    if (argc != 4 || !std::ranges::all_of(std::span(argv + 1, 3), [](const char *value) {
            return *value && *value != '-';
        })) {
        std::cerr << "Invalid arguments. Use --help.\n";

        return 2;
    }

    try {
        auto initialToken = readToken();

        if (!initialToken) {
            std::cerr << "Missing or invalid initial Basic credentials.\n";

            return 1;
        }

        auto credentials = std::make_shared<Credentials>();
        credentials->update(std::move(*initialToken));

        api::EndpointOptions options;
        options.tls = examples::tlsOptions(argv[2], argv[3], credentials);
        options.maximumAttempts = 2;

        api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint(options);
        endpoint->connect(argv[1]);

        const auto initialEpoch = awaitEpoch(*endpoint, *credentials);

        if (!initialEpoch) {
            return 1;
        }

        std::cout << "Authenticated epoch " << *initialEpoch << '\n' << std::flush;

        auto replacementToken = readToken();

        if (!replacementToken) {
            std::cerr << "Missing or invalid replacement Basic credentials.\n";

            return 1;
        }

        credentials->update(std::move(*replacementToken));
        endpoint->reconnect();

        const auto replacementEpoch = awaitEpoch(*endpoint, *credentials);
        const auto close = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (!replacementEpoch) {
            return 1;
        }

        if (*replacementEpoch <= *initialEpoch) {
            std::cerr << "Reconnect did not establish a new source epoch.\n";

            return 1;
        }

        if (close.deadlineExpired) {
            std::cerr << "Endpoint shutdown deadline expired.\n";

            return 1;
        }

        std::cout << "Authenticated epoch " << *replacementEpoch << '\n' << std::flush;

        return std::cout ? 0 : 1;
    } catch (const api::Error &error) {
        std::cerr << "Credentials refresh failed (" << error.diagnostic().stage << ").\n";

        return 1;
    } catch (const std::exception &) {
        std::cerr << "Credentials refresh failed. Check the address, TLS settings and trust file.\n";

        return 1;
    }
}
