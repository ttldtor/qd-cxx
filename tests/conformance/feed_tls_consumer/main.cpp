// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

#if defined(ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS)
#    include <algorithm>
#    include <atomic>
#    include <condition_variable>
#    include <cstddef>
#    include <cstdint>
#    include <expected>
#    include <filesystem>
#    include <fstream>
#    include <memory>
#    include <mutex>
#    include <span>
#    include <string>
#    include <utility>
#endif

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
using namespace std::chrono_literals;

int fail(std::string_view stage, api::Diagnostic diagnostic = {}) {
    std::cerr << stage << ": " << diagnostic.stage << " (" << static_cast<int>(diagnostic.code) << ", "
              << diagnostic.nativeCode << ", " << diagnostic.detailCode << ")\n";

    return 1;
}

#if defined(ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS)
constexpr std::u16string_view SYNTHETIC_TOKEN = u"Basic dGVzdDpmaXh0dXJl";
constexpr q::transport::TlsResourceLimits INPUT_LIMITS{524288, 256, 65536, 65536, 255};
constexpr std::size_t TLS_NATIVE_ALLOWANCE = 32 * 1024 * 1024;
constexpr std::size_t FEED_NATIVE_ALLOWANCE = 128 * 1024 * 1024;

class Provider final : public q::client::CredentialProvider {
    public:
    std::atomic<unsigned> calls{0};

    std::expected<std::size_t, q::client::CredentialError> basicToken(std::span<char16_t> output) override {
        ++calls;

        if (output.size() < SYNTHETIC_TOKEN.size())
            return std::unexpected(q::client::CredentialError::REJECTED);

        std::ranges::copy(SYNTHETIC_TOKEN, output.begin());

        return SYNTHETIC_TOKEN.size();
    }
};

int runTls(std::string_view mode, const std::filesystem::path &trustFile) {
    const bool authenticated = mode == "authenticated";

    if (!authenticated && mode != "wrong-ca" && mode != "wrong-san")
        return fail("mode");

    unsigned port = 0;

    if (!(std::cin >> port) || port == 0 || port > 65535)
        return fail("port");

    std::ifstream input(trustFile, std::ios::binary);

    if (!input)
        return fail("trust-file");

    std::string trustPem(INPUT_LIMITS.trustBytes + 1, '\0');
    input.read(trustPem.data(), static_cast<std::streamsize>(trustPem.size()));

    if (!input.eof() || input.bad())
        return fail("trust-file-size");

    trustPem.resize(static_cast<std::size_t>(input.gcount()));

    constexpr q::core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384};
    const auto quoteSchema = q::events::defaultQuoteSchema(SCHEMA_LIMITS);

    if (!quoteSchema)
        return fail("quote-schema");

    const auto schema =
        q::core::LocalSchema::create(quoteSchema->records(), SCHEMA_LIMITS, q::core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema)
        return fail("binary-schema");

    const api::EventRegistration quoteRoute{api::EventType::QUOTE, schema->records().front().id};
    api::EventRegistryWork registryWork;
    auto registry = api::EventRegistry::create(*schema, std::span(&quoteRoute, 1), {}, registryWork);

    if (!registry)
        return fail("quote-registry");

    // This fixture process owns the only SDK client image. The TLS peer runs in another process.
    constexpr q::transport::TlsHostLimits HOST_LIMITS{5000, 16, 64, 64, 128,   32768, 512,   8,  128, 64,
                                                      8,    16, 4,  8,  65536, 4096,  65536, 64, 8,   2048};
    const auto resources = q::transport::tlsRequirements(INPUT_LIMITS, HOST_LIMITS);

    if (!resources)
        return fail("tls-resources");

    auto provider = std::make_shared<Provider>();
    api::EndpointOptions options;
    options.maximumAttempts = 1;
    options.eventRegistry = std::move(*registry);
    auto &tls = options.tls.emplace();
    tls.referenceIdentity = mode == "wrong-san" ? "wrong.example" : "localhost";
    tls.trustPem = std::move(trustPem);
    tls.limits = INPUT_LIMITS;
    tls.host = HOST_LIMITS;
    tls.sdkInstanceBytes = resources->sdk.instanceBytes;
    tls.sdkSharedBytes = resources->sdk.sharedBytes;
    tls.residentBytes = resources->sdk.totalBytes + TLS_NATIVE_ALLOWANCE + FEED_NATIVE_ALLOWANCE;
    tls.credentials = api::BasicTlsCredentials{provider};

    const auto configuration = api::EndpointConfiguration::create(options);
    options.tls.reset();
    std::mutex quoteMutex;
    std::condition_variable quoteChanged;
    std::size_t quoteCount = 0;
    bool quoteMatches = false;
    api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    api::ApiRuntime runtime(std::move(runtimeOptions));
    auto endpoint = runtime.createEndpoint(configuration);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    auto capture = std::make_shared<int>();
    std::weak_ptr<int> lifetime = capture;
    subscription->addEventListener(
        [capture = std::move(capture), &quoteMutex, &quoteChanged, &quoteCount, &quoteMatches](auto quotes) {
            std::lock_guard lock(quoteMutex);

            for (const auto &quote : quotes) {
                ++quoteCount;
                quoteMatches =
                    capture && quote.symbol == u"IBM" && quote.bidExchangeCode == u'Q' && quote.askExchangeCode == u'Z';
            }

            quoteChanged.notify_all();
        });
    subscription->addSymbols("IBM");

    if (provider->calls != 0 || lifetime.expired())
        return fail("pre-activation");

    endpoint->connect("127.0.0.1:" + std::to_string(port));
    const auto deadline = std::chrono::steady_clock::now() + 8s;
    const auto target = authenticated ? api::EndpointState::CONNECTED : api::EndpointState::FAILED;
    const auto observed = endpoint->awaitState(target, deadline);

    if (observed.status == api::WaitStatus::REACHED && authenticated) {
        std::unique_lock lock(quoteMutex);
        quoteChanged.wait_until(lock, deadline, [&] {
            return quoteCount != 0;
        });
    }

    const auto diagnostic = endpoint->getDiagnostic();
    const auto active = endpoint->getStatistics();
    const auto closed = endpoint->closeAndAwaitTermination();
    const auto final = endpoint->getStatistics();
    runtime.closeAndAwaitTermination();

    if (observed.status != api::WaitStatus::REACHED)
        return fail("endpoint-state", diagnostic);

    // A retained diagnostic changes terminal state independently of whether the admitted prefix drained.
    const auto terminalState =
        closed.diagnostic.code == api::ErrorCode::NONE ? api::EndpointState::CLOSED : api::EndpointState::FAILED;

    if (!lifetime.expired() || !final.final || final.connection.sourceEpoch || endpoint->getState() != terminalState)
        return fail("retirement", closed.diagnostic);

    if (authenticated) {
        if (diagnostic.code != api::ErrorCode::NONE || provider->calls != 1 || quoteCount != 1 || !quoteMatches ||
            active.connection.sourceEpoch != 1 || active.connection.readBytes.value == 0 || !closed.drained ||
            closed.hadGap || closed.deadlineExpired || final.connection.closedConnections.value != 1)
            return fail("authenticated-quote-close", closed.diagnostic);
    } else if (terminalState != api::EndpointState::FAILED || diagnostic.code != api::ErrorCode::PROTOCOL_FAILURE ||
               !diagnostic.stage.starts_with("tls-") || provider->calls != 0 || quoteCount != 0 ||
               active.connection.sourceEpoch || final.connection.establishedConnections.value != 0 ||
               closed.deadlineExpired) {
        return fail("verification-refusal", diagnostic);
    }

    return 0;
}
#else
int runUnavailable() {
    api::EndpointOptions secure;
    secure.tls.emplace();

    try {
        static_cast<void>(api::EndpointConfiguration::create(secure));

        return fail("expected-tls-refusal");
    } catch (const api::Error &error) {
        const auto diagnostic = error.diagnostic();

        if (diagnostic.code != api::ErrorCode::UNSUPPORTED_PROFILE || diagnostic.stage != "endpoint-tls-unavailable")
            return fail("tls-refusal", diagnostic);
    }

    api::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    api::ApiRuntime runtime(runtimeOptions);
    const auto endpoint = runtime.createEndpoint();
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    if (!closed.drained || closed.hadGap || closed.deadlineExpired || !endpoint->getStatistics().final)
        return fail("plain-offline-close", closed.diagnostic);

    return 0;
}
#endif
} // namespace

int main(int argc, char **argv) {
    try {
#if defined(ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS)

        if (argc == 3)
            return runTls(argv[1], argv[2]);

#else

        if (argc == 2 && std::string_view(argv[1]) == "unavailable")
            return runUnavailable();

#endif

        return fail("arguments");
    } catch (const api::Error &error) {
        return fail("feed", error.diagnostic());
    } catch (const std::exception &) {
        return fail("unexpected");
    }
}
