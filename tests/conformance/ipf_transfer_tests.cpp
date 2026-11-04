// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/ipf/instrument_profile_url_reader.hpp"

#include "dns_resolver.hpp"
#include "ipf_http_peer.hpp"
#include "raw_byte_owner.hpp"
#include "tls_byte_owner.hpp"
#ifndef _WIN32
#    include <sys/stat.h>
#endif
#include "transfer_native.hpp"
#include "transfer_response.hpp"
#include "transfer_source.hpp"
#include "transport/tls_test_support.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <thread>
#include <type_traits>

namespace {
thread_local std::ptrdiff_t allocationCountdown = -1; ///< Test creator only; peer threads never inherit failures.
}

/** Inject one real allocation failure only on the test's serialized caller.
 * @param bytes Requested storage.
 * @return Ordinary C allocation, or bad_alloc at the selected call.
 */
void *operator new(std::size_t bytes) {
    if (allocationCountdown > 0 && --allocationCountdown == 0) {
        allocationCountdown = -1;

        throw std::bad_alloc{};
    }

    if (auto memory = std::malloc(bytes == 0 ? 1 : bytes)) {
        return memory;
    }

    throw std::bad_alloc{};
}

/** Use the same counted owner for array storage.
 * @param bytes Requested array storage.
 * @return Matching allocation.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Retire the matching test allocation.
 * @param memory Owned allocation or null.
 */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** Retire sized scalar storage.
 * @param memory Owned allocation.
 * @param bytes Original size, unused by the C allocator.
 */
void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

/** Retire matching array storage.
 * @param memory Owned allocation.
 */
void operator delete[](void *memory) noexcept {
    std::free(memory);
}

/** Retire sized array storage.
 * @param memory Owned allocation.
 * @param bytes Original size.
 */
void operator delete[](void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

namespace i = org::ttldtor::qdcxx::ipf;
namespace d = i::detail;
namespace t = org::ttldtor::qdcxx::transport;
namespace h = org::ttldtor::qdcxx::test;
namespace x = h::tls;
using namespace std::chrono_literals;

namespace {
constexpr std::string_view COMPLETE_IPF = "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n##COMPLETE\n";

/** Make one finite content-length response from synthetic uncompressed IPF.
 * @param body Exact body bytes.
 * @param fields Extra complete header lines.
 * @return Whole raw HTTP response.
 */
std::string response(std::string_view body = COMPLETE_IPF, std::string_view fields = {}) {
    return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + std::string(fields) + "\r\n" +
           std::string(body);
}

/** Make a loopback source without ambient DNS.
 * @param port Owned listener port.
 * @param path Exact target.
 * @param secure Select verified HTTPS.
 * @return Synthetic URL, never actual credentials.
 */
std::string url(std::uint16_t port, std::string_view path = "/profiles.ipf", bool secure = false) {
    return std::string(secure ? "https://127.0.0.1:" : "http://127.0.0.1:") + std::to_string(port) + std::string(path);
}

/** Create an explicit borrowed reader, preserving process startup before fixture threads.
 * @param bootstrap Live same-image owner.
 * @param options Explicit finite settings.
 * @return Nonmovable owning facade.
 */
std::unique_ptr<i::InstrumentProfileReader> reader(t::RuntimeBootstrap &bootstrap, i::IpfTransferOptions options = {}) {
    auto created = i::InstrumentProfileReader::create(bootstrap, options);
    REQUIRE(created);

    return std::move(*created);
}

/** Qualify HTTPS using only existing ordinary fixtures and declared controlled-image maxima.
 * @return Explicit finite TLS allowances and reader-lifetime work.
 */
i::IpfTransferOptions tlsOptions() {
    i::IpfTransferOptions options;
    options.trustPem = x::read("ca.pem");
    options.tls = x::options();
    options.tls.residentBytes += options.nativeLimits.residentBytes;
    options.tlsWork.units.fill((std::numeric_limits<std::uint64_t>::max)());
    options.handshakeTimeout = 2s;
    options.closeTimeout = 1s;

    return options;
}

/** Start the existing isolated TLS peer with a finite HTTP response.
 * @param reply Synthetic raw HTTP reply.
 * @param expectRequest Require the known first GET line before replying.
 * @return Owned isolated peer, no SDK context in this test process.
 */
h::TlsTestPeer tlsPeer(std::string reply, bool expectRequest = true) {
    auto options = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::CLEAN);
    options.replyBytes.assign(reply.begin(), reply.end());
    options.captureUntilClose = !expectRequest;
    options.readHttpHeaders = expectRequest;

    if (expectRequest) {
        const std::string prefix = "GET /profiles.ipf HTTP/1.1\r\n";
        options.expectedClientBytes.emplace(prefix.begin(), prefix.end());
    }

    auto started = h::TlsTestPeer::start(options);
    REQUIRE(started);

    return std::move(*started);
}

/** Synthetic provider with an explicit test action; no real credential is ever used. */
class Provider final : public i::HttpAuthorizationProvider {
    public:
    std::function<void()> action; ///< Creator-thread test reentry.
    unsigned calls = 0;           ///< Exact invocations.
    bool throwFailure = false;    ///< Inject a secret-bearing exception without logging it.

    /** Supply synthetic bounded test authorization.
     * @param request Sanitized verified origin.
     * @param destination Borrowed bounded output.
     * @return Whole synthetic value length.
     */
    std::expected<std::size_t, i::HttpAuthorizationError> authorization(i::HttpAuthorizationRequest request,
                                                                        std::span<char> destination) override {
        ++calls;
        CHECK(request.scheme == "https");
        CHECK(request.host == "127.0.0.1");

        if (action) {
            action();
        }

        if (throwFailure) {
            throw std::runtime_error("synthetic-secret-exception");
        }

        constexpr std::string_view TOKEN = "Bearer synthetic-test-token";

        if (TOKEN.size() > destination.size()) {
            return std::unexpected(i::HttpAuthorizationError::INVALID);
        }

        std::memcpy(destination.data(), TOKEN.data(), TOKEN.size());

        return TOKEN.size();
    }
};

} // namespace

TEST_CASE("IPF URL reader distinguishes HTTP framing and owned IPF completion") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfTransferOptions fragmented;
    fragmented.nativeLimits.inputBytes = 1;
    fragmented.nativeLimits.writeBytes = 1;
    auto facade = reader(*bootstrap, fragmented);
    h::IpfHttpPeer peer(
        {response(COMPLETE_IPF, "X-Live: provided\r\nLast-Modified: Sun, 06 Nov 1994 08:49:37 GMT\r\n")}, 1);
    auto result = facade->loadURL(url(peer.port()));
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " stage=" << (result.error ? result.error->stage : "none")
                     << " tls=" << (result.error ? static_cast<int>(result.error->tls.error) : -1)
                     << " native=" << (result.error ? static_cast<int>(result.error->transport.error) : -1));
    REQUIRE_FALSE(result.error);
    CHECK(result.status == i::IpfTransferStatus::SUCCESS);
    CHECK(result.httpStatus == 200);
    CHECK(result.httpFinished);
    CHECK(result.nativeRetired);
    CHECK(result.ipf.finished);
    CHECK(result.ipf.complete);
    CHECK(result.ipf.profiles.size() == 1);
    CHECK(result.lastModified == d::parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT"));
    CHECK(result.live == i::IpfLiveSupport::PROVIDED);
    CHECK(result.metadataCommitted);
    facade.reset();
    CHECK(result.ipf.profiles.size() == 1);
    const auto requests = peer.requests();
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].find("X-Live: no\r\n") != std::string::npos);
    CHECK(requests[0].find("Accept-Encoding: identity\r\n") != std::string::npos);
}

TEST_CASE("IPF conditional304 is not a new complete empty snapshot") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    h::IpfHttpPeer peer({"HTTP/1.1 304 Not Modified\r\nLast-Modified: Sun, 06 Nov 1994 08:49:37 GMT\r\n\r\n"});
    i::IpfUrlRequest request;
    request.ifModifiedSince = d::parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT");
    const auto result = facade->loadURL(url(peer.port()), request);
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " stage=" << (result.error ? result.error->stage : "none")
                     << " tls=" << (result.error ? static_cast<int>(result.error->tls.error) : -1)
                     << " native=" << (result.error ? static_cast<int>(result.error->transport.error) : -1));
    REQUIRE_FALSE(result.error);
    CHECK(result.status == i::IpfTransferStatus::NOT_MODIFIED);
    CHECK(result.httpFinished);
    CHECK_FALSE(result.ipf.finished);
    CHECK_FALSE(result.ipf.complete);
    CHECK_FALSE(result.metadataCommitted);
    CHECK(result.ipf.profiles.empty());
    const auto requests = peer.requests();
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].find("If-Modified-Since: Sun, 06 Nov 1994 08:49:37 GMT\r\n") != std::string::npos);
}

TEST_CASE("IPF clean missing marker and framing truncation retain different owned evidence") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfTransferOptions config;
    config.ipf.completion = i::IpfCompletionPolicy::REQUIRE;
    auto facade = reader(*bootstrap, config);
    const std::string noMarker = "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n";
    h::IpfHttpPeer peer(
        {response(noMarker), "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n" + std::string(COMPLETE_IPF)});
    const auto missing = facade->loadURL(url(peer.port()));
    REQUIRE(missing.error);
    CHECK(missing.error->error == i::IpfTransferError::IPF_FAILURE);
    REQUIRE(missing.ipf.error);
    CHECK(missing.ipf.error->error == i::IpfReadError::MISSING_COMPLETE);
    CHECK(missing.httpFinished);
    CHECK_FALSE(missing.ipf.complete);
    CHECK(missing.ipf.profiles.size() == 1);
    const auto truncated = facade->loadURL(url(peer.port()));
    REQUIRE(truncated.error);
    CHECK(truncated.error->error == i::IpfTransferError::HTTP_FAILURE);
    CHECK_FALSE(truncated.httpFinished);
    CHECK(truncated.ipf.completionSeen);
    CHECK_FALSE(truncated.ipf.finished);
    CHECK(truncated.ipf.profiles.size() == 1);
}

TEST_CASE("IPF stop_token cancels actual controlled header and body waits without facade access") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto bodyWait : {false, true}) {
        auto facade = reader(*bootstrap);
        const auto prefix = bodyWait ? "HTTP/1.1 200 OK\r\nContent-Length: 10000\r\n\r\n" + std::string(COMPLETE_IPF)
                                     : std::string("HTTP/1.1 200 OK\r\nContent-");
        h::IpfHttpPeer peer({prefix}, 4096, true);
        std::stop_source cancellation;
        std::jthread requester([&] {
            const auto deadline = std::chrono::steady_clock::now() + 3s;

            while (peer.sentBytes() < prefix.size() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }

            // Let the client consume the owned prefix; only the stop state crosses threads.
            std::this_thread::sleep_for(25ms);
            cancellation.request_stop();
        });
        const auto result = facade->loadURL(url(peer.port()), {}, cancellation.get_token());
        REQUIRE(result.error);
        CHECK(result.status == i::IpfTransferStatus::CANCELLED);
        CHECK(result.error->error == i::IpfTransferError::CANCELLED);
        CHECK(result.nativeRetired);
        CHECK_FALSE(result.httpFinished);
        CHECK_FALSE(result.ipf.complete);

        if (bodyWait) {
            CHECK(result.ipf.profiles.size() == 1);
        }
    }
}

TEST_CASE("IPF fixed resolution follows RFC3986 normal abnormal and encoded-dot vectors") {
    const i::IpfTransferLimits limits;
    const auto base = d::parseTransferSource("http://a/b/c/d;p?q", limits, true);
    REQUIRE(base);
    const std::pair<std::string_view, std::string_view> vectors[] = {{"g", "http://a/b/c/g"},
                                                                     {"./g", "http://a/b/c/g"},
                                                                     {"g/", "http://a/b/c/g/"},
                                                                     {"/g", "http://a/g"},
                                                                     {"//g", "http://g/"},
                                                                     {"?y", "http://a/b/c/d;p?y"},
                                                                     {"g?y", "http://a/b/c/g?y"},
                                                                     {"#s", "http://a/b/c/d;p?q"},
                                                                     {"g#s", "http://a/b/c/g"},
                                                                     {"g?y#s", "http://a/b/c/g?y"},
                                                                     {";x", "http://a/b/c/;x"},
                                                                     {"g;x", "http://a/b/c/g;x"},
                                                                     {"g;x?y#s", "http://a/b/c/g;x?y"},
                                                                     {"", "http://a/b/c/d;p?q"},
                                                                     {".", "http://a/b/c/"},
                                                                     {"./", "http://a/b/c/"},
                                                                     {"..", "http://a/b/"},
                                                                     {"../", "http://a/b/"},
                                                                     {"../g", "http://a/b/g"},
                                                                     {"../..", "http://a/"},
                                                                     {"../../", "http://a/"},
                                                                     {"../../g", "http://a/g"},
                                                                     {"../../../g", "http://a/g"},
                                                                     {"../../../../g", "http://a/g"},
                                                                     {"/./g", "http://a/g"},
                                                                     {"/../g", "http://a/g"},
                                                                     {"g.", "http://a/b/c/g."},
                                                                     {".g", "http://a/b/c/.g"},
                                                                     {"g..", "http://a/b/c/g.."},
                                                                     {"..g", "http://a/b/c/..g"},
                                                                     {"./../g", "http://a/b/g"},
                                                                     {"./g/.", "http://a/b/c/g/"},
                                                                     {"g/./h", "http://a/b/c/g/h"},
                                                                     {"g/../h", "http://a/b/c/h"},
                                                                     {"g;x=1/./y", "http://a/b/c/g;x=1/y"},
                                                                     {"g;x=1/../y", "http://a/b/c/y"},
                                                                     {"g?y/./x", "http://a/b/c/g?y/./x"},
                                                                     {"g?y/../x", "http://a/b/c/g?y/../x"},
                                                                     {"g#s/./x", "http://a/b/c/g"},
                                                                     {"%2e/%2e%2e/%2f", "http://a/b/c/%2e/%2e%2e/%2f"}};

    for (const auto &[reference, expected] : vectors) {
        INFO(reference);
        const auto resolved = d::resolveTransferSource(*base, reference, limits);
        REQUIRE(resolved);
        CHECK(resolved->address() == expected);
    }

    CHECK_FALSE(d::resolveTransferSource(*base, "g:h", limits));
    CHECK_FALSE(d::resolveTransferSource(*base, "file:/private", limits));
    CHECK_FALSE(d::resolveTransferSource(*base, "http://user:password@a/x", limits));
    auto secure = d::parseTransferSource("https://EXAMPLE.test:443/path", limits, true);
    auto same = d::parseTransferSource("https://example.test./other", limits, true);
    REQUIRE(secure);
    REQUIRE(same);
    CHECK(d::sameOrigin(*secure, *same));
    CHECK_FALSE(d::resolveTransferSource(*secure, "http://example.test/", limits));
    auto shortLimits = limits;
    shortLimits.sourceBytes = std::string_view("http://a/g").size() - 1;
    CHECK_FALSE(d::resolveTransferSource(*base, "/g", shortLimits));
}

TEST_CASE("IPF redirects and informational responses preserve finite aggregate limits") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    h::IpfHttpPeer peer({"HTTP/1.1 302 Found\r\nLocation: ../final?opaque\r\nContent-Length: 0\r\n\r\n",
                         "HTTP/1.1 100 Continue\r\n\r\n" + response()});
    const auto result = facade->loadURL(url(peer.port(), "/dir/start"));
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " stage=" << (result.error ? result.error->stage : "none")
                     << " tls=" << (result.error ? static_cast<int>(result.error->tls.error) : -1)
                     << " native=" << (result.error ? static_cast<int>(result.error->transport.error) : -1));
    REQUIRE_FALSE(result.error);
    CHECK(result.redirects == 1);
    CHECK(result.ipf.complete);
    const auto requests = peer.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1].find("GET /final?opaque HTTP/1.1\r\n") == 0);
}

TEST_CASE("IPF source auth values are owned bounded and never allowed over plaintext") {
    auto basic = i::HttpAuthorization::basic("a%3A", "b:c");
    REQUIRE(basic);
    CHECK(basic->value() == "Basic YSUzQTpiOmM=");
    CHECK_FALSE(i::HttpAuthorization::custom("bad scheme", "value"));
    CHECK_FALSE(i::HttpAuthorization::bearer("bad\r\ninjection"));
    CHECK_FALSE(i::HttpAuthorization::bearer("two words"));
    CHECK_FALSE(i::HttpAuthorization::custom("Basic", "invalid-padding==="));
    CHECK_FALSE(i::HttpAuthorization::basic("colon:user", "pw"));
    const auto exact = i::HttpAuthorization::bearer("abc", 10);
    CHECK(exact);
    CHECK_FALSE(i::HttpAuthorization::bearer("abc", 9));
    const i::IpfTransferLimits limits;
    auto source = d::parseTransferSource("https://a%3A:b:c@EXAMPLE.test/path?opaque-secret", limits, true);
    REQUIRE(source);
    CHECK(source->address() == "https://example.test/path?opaque-secret");
    REQUIRE(source->userinfo);
    CHECK(source->userinfo->value() == basic->value());
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    const auto result = facade->loadURL("http://user:password@127.0.0.1:1/");
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::AUTHORIZATION_FAILURE);
    CHECK(result.error->stage == "plaintext-authorization");
    CHECK_FALSE(d::parseTransferSource("unknown://host/path", limits, true));
}

TEST_CASE("IPF verified HTTPS performs200 and provider runs only after identity verification") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto peer = tlsPeer(response());
    auto provider = std::make_shared<Provider>();
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true), request);
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " stage=" << (result.error ? result.error->stage : "none")
                     << " tls=" << (result.error ? static_cast<int>(result.error->tls.error) : -1)
                     << " native=" << (result.error ? static_cast<int>(result.error->transport.error) : -1));
    REQUIRE_FALSE(result.error);
    CHECK(result.ipf.complete);
    CHECK(result.nativeRetired);
    CHECK(provider->calls == 1);
}

TEST_CASE("IPF auth cancellation deletion replacement and exceptions cannot publish stale output") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto mode : {0, 1, 2, 3}) {
        auto config = tlsOptions();
        auto facade = reader(*bootstrap, config);
        auto peer = tlsPeer(response(), false);
        auto provider = std::make_shared<Provider>();
        provider->throwFailure = mode == 3;
        provider->action = [&] {
            if (mode == 0) {
                REQUIRE(facade->cancel());
            } else if (mode == 1 || mode == 2) {
                facade.reset();

                if (mode == 2) {
                    facade = reader(*bootstrap, config);
                }
            }
        };
        i::IpfUrlRequest request;
        request.provider = provider;
        const auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true), request);
        REQUIRE(result.error);
        CHECK(provider->calls == 1);
        CHECK(result.nativeRetired);
        CHECK(result.error->error ==
              (mode == 3 ? i::IpfTransferError::AUTHORIZATION_FAILURE : i::IpfTransferError::CANCELLED));
        const auto snapshot = peer.snapshot();
        CHECK(snapshot.bytesRead == 0);
    }
}

TEST_CASE("IPF TLS verify failure invokes no provider and publishes no plaintext") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto config = tlsOptions();
    config.trustPem = x::read("other-ca.pem");
    auto facade = reader(*bootstrap, config);
    auto peer = tlsPeer(response(), false);
    auto provider = std::make_shared<Provider>();
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true), request);
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::TLS_FAILURE);
    CHECK(provider->calls == 0);
    CHECK(result.nativeRetired);
    CHECK(peer.snapshot().bytesRead == 0);
}

TEST_CASE("IPF checked owner methods reject wrong thread and pre-stopped load performs no network") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    std::jthread wrong([&] {
        const auto cancelled = facade->cancel();
        REQUIRE_FALSE(cancelled);
        CHECK(cancelled.error().error == i::IpfTransferError::WRONG_THREAD);
        const auto read = facade->loadURL("http://127.0.0.1:1/");
        REQUIRE(read.error);
        CHECK(read.error->error == i::IpfTransferError::WRONG_THREAD);
    });
    wrong.join();
    std::stop_source stop;
    stop.request_stop();
    const auto result = facade->loadURL("http://127.0.0.1:1/", {}, stop.get_token());
    REQUIRE(result.error);
    CHECK(result.status == i::IpfTransferStatus::CANCELLED);
    CHECK(result.wireBytes == 0);
}

TEST_CASE("IPF clean HTTPS server confirms whole request and explicit userinfo override") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto peer = tlsPeer(response());
    auto token = i::HttpAuthorization::bearer("synthetic-explicit");
    REQUIRE(token);
    i::IpfUrlRequest request;
    request.authorization = std::move(*token);
    const auto address = "https://invaliduserinfo@127.0.0.1:" + std::to_string(peer.port()) + "/profiles.ipf";
    const auto result = facade->loadURL(address, request);
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " tls-stage=" << (result.error ? result.error->tls.stage : "none"));
    REQUIRE_FALSE(result.error);
    CHECK(result.ipf.complete);
    const auto snapshot = peer.snapshot();
    CHECK(snapshot.expectedBytesMatched);
    const std::string capture(snapshot.firstClientBytes.begin(), snapshot.firstClientBytes.end());
    CHECK(capture.find("\r\nAuthorization: Bearer synthetic-explicit\r\n") != std::string::npos);
    CHECK(capture.find("invaliduserinfo") == std::string::npos);
    auto finalPeer = peer.snapshot();
    const auto peerDeadline = std::chrono::steady_clock::now() + 2s;

    while (!finalPeer.completed && std::chrono::steady_clock::now() < peerDeadline) {
        std::this_thread::sleep_for(1ms);
        finalPeer = peer.snapshot();
    }

    INFO("peerCompleted=" << finalPeer.completed << " peerFailed=" << finalPeer.failed
                          << " peerEnd=" << finalPeer.firstCaptureEnd << " peerRead=" << finalPeer.bytesRead
                          << " captured=" << finalPeer.firstClientBytes.size() << " exit=" << finalPeer.exitCode);
    CHECK(finalPeer.closeNotifyComplete);
}

TEST_CASE("IPF complete marker plus deliberate TLS truncated close remains failure with owned prefix") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto options = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::CLEAN);
    options.captureUntilClose = true;
    const auto reply = response();
    options.replyBytes.assign(reply.begin(), reply.end());
    const std::string prefix = "GET /profiles.ipf HTTP/1.1\r\n";
    options.expectedClientBytes.emplace(prefix.begin(), prefix.end());
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    const auto result = facade->loadURL(url(peer->port(), "/profiles.ipf", true));
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::TLS_FAILURE);
    CHECK(result.error->tls.stage == "tls-truncated-eof");
    CHECK(result.status == i::IpfTransferStatus::FAILED);
    CHECK(result.httpFinished);
    CHECK(result.ipf.completionSeen);
    CHECK(result.ipf.profiles.size() == 1);
    CHECK_FALSE(result.metadataCommitted);
    CHECK(result.nativeRetired);
}

TEST_CASE("IPF cross-origin HTTPS redirect never inherits initial provider or secret") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto destination = tlsPeer(response());
    const auto location = url(destination.port(), "/profiles.ipf", true);
    auto origin = tlsPeer("HTTP/1.1 302 Found\r\nLocation: " + location + "\r\nContent-Length: 0\r\n\r\n");
    auto provider = std::make_shared<Provider>();
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(origin.port(), "/profiles.ipf", true), request);
    INFO("transfer=" << (result.error ? static_cast<int>(result.error->error) : -1)
                     << " tls-stage=" << (result.error ? result.error->tls.stage : "none"));
    REQUIRE_FALSE(result.error);
    CHECK(result.redirects == 1);
    CHECK(provider->calls == 1);
    const auto snapshot = destination.snapshot();
    const std::string capture(snapshot.firstClientBytes.begin(), snapshot.firstClientBytes.end());
    CHECK(capture.find("\r\nAuthorization:") == std::string::npos);
    CHECK(result.ipf.complete);
}

TEST_CASE("IPF finite HTTP framing body chunk trailer and parser work caps reject deterministically") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto whole = response();
    const auto headerBytes = whole.find("\r\n\r\n") + 4;

    for (const auto below : {false, true}) {
        i::IpfTransferOptions config;
        config.limits.headerBytes = headerBytes - (below ? 1 : 0);
        config.limits.bodyBytes = COMPLETE_IPF.size();
        auto facade = reader(*bootstrap, config);
        h::IpfHttpPeer peer({whole});
        const auto result = facade->loadURL(url(peer.port()));
        CHECK(result.error.has_value() == below);
        CHECK(result.nativeRetired);
    }

    for (const auto mode : {0, 1, 2, 3, 4}) {
        i::IpfTransferOptions config;

        if (mode == 0)
            config.limits.bodyBytes = COMPLETE_IPF.size() - 1;

        if (mode == 1)
            config.limits.framingBytes = headerBytes - 1;

        if (mode == 2)
            config.limits.parserCalls = 0;

        if (mode == 3)
            config.limits.presentedBytes = 1;

        if (mode == 4)
            config.ipf.limits.profiles = 0;

        auto facade = reader(*bootstrap, config);
        h::IpfHttpPeer peer({whole});
        const auto result = facade->loadURL(url(peer.port()));
        REQUIRE(result.error);
        CHECK(result.status == i::IpfTransferStatus::FAILED);
        CHECK(result.nativeRetired);
        CHECK_FALSE(result.metadataCommitted);
    }

    std::ostringstream chunkSize;
    chunkSize << std::hex << COMPLETE_IPF.size();
    const auto chunked = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + chunkSize.str() + ";x=y\r\n" +
                         std::string(COMPLETE_IPF) + "\r\n0\r\nX-Test: ok\r\n\r\n";

    for (const auto mode : {0, 1, 2, 3}) {
        i::IpfTransferOptions config;

        if (mode == 1)
            config.limits.chunkExtensionBytes = 3;

        if (mode == 2)
            config.limits.totalChunkExtensionBytes = 3;

        if (mode == 3)
            config.limits.trailerBytes = 1 + std::string_view("X-Test: ok\r\n").size();

        auto facade = reader(*bootstrap, config);
        h::IpfHttpPeer peer({chunked}, 1);
        const auto result = facade->loadURL(url(peer.port()));
        CHECK(result.error.has_value() == (mode != 0));
        CHECK(result.nativeRetired);
    }
}

TEST_CASE("IPF malformed framing status compression and redirects never commit metadata") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const std::vector<std::string> responses{
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nx",
        "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n",
        "HTTP/1.1 304 Not Modified\r\n\r\n",
        "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n",
        "HTTP/1.1 302 Found\r\nLocation: file:/synthetic\r\nContent-Length: 0\r\n\r\n",
        response("compressed", "Content-Encoding: gzip\r\n"),
        response(std::string("\x1f\x8b\x08\x00", 4)),
        "HTTP/1.1 101 Switching Protocols\r\n\r\n"};

    for (const auto &reply : responses) {
        auto facade = reader(*bootstrap);
        h::IpfHttpPeer peer({reply});
        const auto result = facade->loadURL(url(peer.port()));
        REQUIRE(result.error);
        CHECK(result.status == i::IpfTransferStatus::FAILED);
        CHECK_FALSE(result.metadataCommitted);
        CHECK_FALSE(result.ipf.complete);
        CHECK(result.nativeRetired);
    }
}

TEST_CASE("IPF local binary file and file URI share parser while public result owns prefix") {
    auto owned = i::InstrumentProfileReader::create();
    REQUIRE(owned);
    const auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_TLS_TEST_CONTROL);
    std::filesystem::create_directories(root);
    const auto path = root / "local profiles.ipf";
    {
        std::ofstream file(path, std::ios::binary);
        file << COMPLETE_IPF;
    }
    const auto local = (*owned)->loadURL(path.generic_string());
    REQUIRE_FALSE(local.error);
    CHECK(local.ipf.complete);
    CHECK(local.httpStatus == 0);
    CHECK(local.nativeRetired);
    auto fileUri = "file:/" + path.generic_string();
#ifdef _WIN32
    fileUri = "file:/" + path.generic_string();
#else
    fileUri = "file:" + path.generic_string();
#endif
    std::size_t position = 0;

    while ((position = fileUri.find(' ', position)) != std::string::npos) {
        fileUri.replace(position, 1, "%20");
        position += 3;
    }

    const auto fromUri = (*owned)->loadURL(fileUri);
    REQUIRE_FALSE(fromUri.error);
    CHECK(fromUri.ipf.complete);
    owned->reset();
    CHECK(local.ipf.profiles.size() == 1);
    std::filesystem::remove(path);
}

TEST_CASE("IPF callback first cause wins over bad close and cleanup reserve is finite") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto reserve : {0U, 1U, 256U}) {
        i::IpfTransferOptions config;
        config.limits.cleanupTurns = reserve;
        auto facade = reader(*bootstrap, config);
        h::IpfHttpPeer peer({response()}, 4096, true);
        const auto result = facade->loadURL(url(peer.port()));
        CHECK(result.nativeRetired);

        if (reserve == 0) {
            REQUIRE(result.error);
            CHECK(result.error->error == i::IpfTransferError::WORK_LIMIT);
        } else {
            INFO("reserve=" << reserve);
            CHECK_FALSE(result.error);
        }
    }

    auto facade = reader(*bootstrap, tlsOptions());
    auto peer = tlsPeer(response(), false);
    auto provider = std::make_shared<Provider>();
    provider->throwFailure = true;
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true), request);
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::AUTHORIZATION_FAILURE);
    CHECK(result.error->stage == "authorization-provider");
    CHECK(result.nativeRetired);
}

TEST_CASE("IPF allocation failure before IO and after a whole profile retains truthful owned prefix") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    bool beforeInput = false;
    bool afterPrefix = false;
    const std::string body = "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\nSTOCK,AAPL\n##COMPLETE\n";

    for (std::ptrdiff_t nth = 1; nth <= 128 && !(beforeInput && afterPrefix); ++nth) {
        auto facade = reader(*bootstrap);
        h::IpfHttpPeer peer({response(body)});
        const auto address = url(peer.port());
        INFO("allocationNth=" << nth);
        allocationCountdown = nth;
        const auto result = facade->loadURL(address);
        const auto remaining = allocationCountdown;
        allocationCountdown = -1;
        const auto allocation =
            result.error && (result.error->error == i::IpfTransferError::ALLOCATION_FAILURE ||
                             (result.ipf.error && result.ipf.error->error == i::IpfReadError::ALLOCATION_FAILURE));

        if (allocation) {
            beforeInput |= result.ipf.profiles.empty();
            afterPrefix |= result.ipf.profiles.size() == 1;
            CHECK(result.nativeRetired);
            CHECK_FALSE(result.metadataCommitted);

            if (result.ipf.profiles.size() == 1) {
                CHECK(result.ipf.acceptedBytes >= std::string_view("#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n").size());
            }
        }

        if (!result.error && remaining > 0) {
            break;
        }
    }

    CHECK(beforeInput);
    CHECK(afterPrefix);
}

TEST_CASE("IPF transfer-owned reservation admits exact capacity and rejects one byte below before IO") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfTransferOptions options;
    std::size_t low = 0;
    std::size_t high = options.limits.residentBytes;

    while (low < high) {
        const auto candidate = low + (high - low) / 2;
        options.limits.residentBytes = candidate;
        const auto made = i::InstrumentProfileReader::create(*bootstrap, options);

        if (made) {
            high = candidate;
        } else {
            CHECK(made.error().error == i::IpfTransferError::RESOURCE_LIMIT);
            low = candidate + 1;
        }
    }

    REQUIRE(low > 0);
    std::cout << "Transfer resident exact=" << low << " sourceScratch=" << d::transferSourceWorkingBytes() << "\n";
    options.limits.residentBytes = low;
    auto exact = i::InstrumentProfileReader::create(*bootstrap, options);
    REQUIRE(exact);
    const auto location = "a/../" + std::string(7000, 'q') + "/last?x=1";
    h::IpfHttpPeer peer({"HTTP/1.1 302 Found\r\nLocation: " + location + "\r\nContent-Length: 0\r\n\r\n", response()});
    const auto redirected = (*exact)->loadURL(url(peer.port()));
    REQUIRE_FALSE(redirected.error);
    CHECK(redirected.redirects == 1);
    CHECK(redirected.ipf.complete);
    const auto requests = peer.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests.back().starts_with("GET /" + std::string(7000, 'q') + "/last?x=1 "));
    options.limits.residentBytes = low - 1;
    const auto below = i::InstrumentProfileReader::create(*bootstrap, options);
    REQUIRE_FALSE(below);
    CHECK(below.error().error == i::IpfTransferError::RESOURCE_LIMIT);
}

TEST_CASE("IPF controlled refused TCP and owned DNS blackhole preserve detached native first cause") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    {
        auto facade = reader(*bootstrap);
        h::IpfHttpPeer refused({""}, 4096, false, 0ms, true);
        const auto result = facade->loadURL(url(refused.port()));
        REQUIRE(result.error);
        CHECK(result.error->error == i::IpfTransferError::TRANSPORT_FAILURE);
        CHECK(result.error->transport.error == t::TcpError::CONNECT_FAILURE);
        CHECK(result.nativeRetired);
    }
    {
        h::IpfDnsBlackhole blackhole;
        i::IpfTransferOptions options;
        options.dnsServers.push_back({{127, 0, 0, 1}});
        options.dnsPort = blackhole.port();
        options.connectTimeout = 50ms;
        auto facade = reader(*bootstrap, options);
        const auto result = facade->loadURL("http://source.invalid:1234/profiles.ipf");
        REQUIRE(result.error);
        CHECK(result.error->error == i::IpfTransferError::TRANSPORT_FAILURE);
        CHECK((result.error->transport.error == t::TcpError::DNS_FAILURE ||
               result.error->transport.error == t::TcpError::DEADLINE));
        CHECK(result.nativeRetired);
    }
}

TEST_CASE("IPF provider alias-null graph is rejected before retention and callback recursion is closed") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    bool deleted = false;
    std::shared_ptr<int> owner(new int(1), [&](int *pointer) {
        deleted = true;
        delete pointer;
    });
    i::IpfUrlRequest invalid;
    invalid.provider = std::shared_ptr<i::HttpAuthorizationProvider>(owner, nullptr);
    const auto rejected = facade->loadURL("https://127.0.0.1:1/", invalid);
    REQUIRE(rejected.error);
    CHECK(rejected.error->error == i::IpfTransferError::INVALID_OPTIONS);
    invalid.provider.reset();
    owner.reset();
    CHECK(deleted);

    auto peer = tlsPeer(response());
    auto provider = std::make_shared<Provider>();
    provider->action = [&] {
        const auto recursive = facade->loadURL("http://127.0.0.1:1/");
        REQUIRE(recursive.error);
        CHECK(recursive.error->error == i::IpfTransferError::INVALID_STATE);
    };
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true), request);
    REQUIRE_FALSE(result.error);
    CHECK(provider->calls == 1);
}

TEST_CASE("IPF initial UTF8 userinfo preserves raw bytes without percent decoding or credential URL owner") {
    const i::IpfTransferLimits limits;
    const auto token = i::HttpAuthorization::basic("\xc3\xa9", "\xe5\xaf\x86");
    REQUIRE(token);
    CHECK(token->value() == "Basic w6k65a+G");
    const auto source = d::parseTransferSource("https://\xc3\xa9:\xe5\xaf\x86@example.test/path", limits, true);
    REQUIRE(source);
    REQUIRE(source->userinfo);
    CHECK(source->userinfo->value() == token->value());
    CHECK(source->address() == "https://example.test/path");
}

TEST_CASE("IPF move noexcept matches actual backing and failed move preserves source and assignment destination") {
    using Backing = std::map<std::u16string, std::u16string, std::less<>>;
    constexpr bool NOTHROW = std::is_nothrow_move_constructible_v<i::InstrumentProfile>;
    CHECK(NOTHROW == std::is_nothrow_default_constructible_v<Backing>);
    std::cout << "InstrumentProfile move noexcept=" << NOTHROW << '\n';
    i::InstrumentProfile source;
    REQUIRE(source.setSymbol(u"IBM"));
    REQUIRE(source.setField(u"custom", u"owned-value"));
    const auto before = source;

    if constexpr (NOTHROW) {
        allocationCountdown = 1;
        i::InstrumentProfile moved(std::move(source));
        const auto remaining = allocationCountdown;
        allocationCountdown = -1;
        CHECK(remaining == 1);
        CHECK(moved == before);
        CHECK(source.getSymbol().empty());
    } else {
        bool caught = false;
        allocationCountdown = 1;

        try {
            i::InstrumentProfile moved(std::move(source));
        } catch (const std::bad_alloc &) {
            caught = true;
        }

        allocationCountdown = -1;
        CHECK(caught);
        CHECK(source == before);
        i::InstrumentProfile destination;
        REQUIRE(destination.setSymbol(u"DESTINATION"));
        const auto destinationBefore = destination;
        caught = false;
        allocationCountdown = 1;

        try {
            destination = std::move(source);
        } catch (const std::bad_alloc &) {
            caught = true;
        }

        allocationCountdown = -1;
        CHECK(caught);
        CHECK(source == before);
        CHECK(destination == destinationBefore);
    }
}

TEST_CASE("IPF LastModified accepts all bounded HTTP-date forms and conditional serialization range") {
    const auto expected = d::parseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT");
    REQUIRE(expected);
    CHECK(d::parseHttpDate("Sunday, 06-Nov-94 08:49:37 GMT") == expected);
    CHECK(d::parseHttpDate("Sun Nov  6 08:49:37 1994") == expected);
    CHECK(d::formatHttpDate(*expected) == "Sun, 06 Nov 1994 08:49:37 GMT");
    CHECK_FALSE(d::parseHttpDate("Sun, 31 Feb 1994 08:49:37 GMT"));
    CHECK_FALSE(d::parseHttpDate("Sun, 06 Nov 1994 25:49:37 GMT"));
    CHECK(d::formatHttpDate(std::chrono::sys_seconds::max()).empty());
    CHECK(d::formatHttpDate(std::chrono::sys_seconds::min()).empty());
}

TEST_CASE("IPF one DNS entry exact grant sends one question while one-below performs no DNS IO") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    t::detail::DnsResolver profile;
    const auto required = profile.workRequirement();
    REQUIRE(required.dnsUnits > 0);

    for (const auto below : {false, true}) {
        h::IpfDnsBlackhole sink;
        i::IpfTransferOptions options;
        options.dnsServers.push_back({{127, 0, 0, 1}});
        options.dnsPort = sink.port();
        options.nativeWork.dnsUnits = required.dnsUnits - (below ? 1 : 0);
        auto facade = reader(*bootstrap, options);
        const auto result = facade->loadURL("http://source.invalid:1234/profiles.ipf");
        REQUIRE(result.error);
        CHECK(result.error->error == i::IpfTransferError::WORK_LIMIT);
        CHECK(result.nativeRetired);
        const auto deadline = std::chrono::steady_clock::now() + 50ms;

        while (!below && sink.queries() == 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }

        if (below) {
            std::this_thread::sleep_for(10ms);
            CHECK(sink.queries() == 0);
        } else {
            CHECK(sink.queries() == 1);
        }
    }
}

TEST_CASE("IPF stop_token cancels actual DNS and paused TLS handshake without SDK calls on requester") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    {
        h::IpfDnsBlackhole sink;
        i::IpfTransferOptions options;
        options.dnsServers.push_back({{127, 0, 0, 1}});
        options.dnsPort = sink.port();
        auto facade = reader(*bootstrap, options);
        std::stop_source stop;
        std::jthread requester([&] {
            const auto deadline = std::chrono::steady_clock::now() + 2s;

            while (sink.queries() == 0 && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }

            stop.request_stop();
        });
        const auto result = facade->loadURL("http://source.invalid:1234/profiles.ipf", {}, stop.get_token());
        requester.join();
        REQUIRE(result.error);
        CHECK(result.status == i::IpfTransferStatus::CANCELLED);
        CHECK(sink.queries() > 0);
        CHECK(result.nativeRetired);
    }
    {
        auto options = tlsOptions();
        auto facade = reader(*bootstrap, options);
        auto serverOptions = x::peerOptions();
        serverOptions.pauseBeforeHandshake = 1s;
        auto server = h::TlsTestPeer::start(serverOptions);
        REQUIRE(server);
        const auto address = url(server->port(), "/profiles.ipf", true);
        std::stop_source stop;
        std::jthread requester([&] {
            const auto deadline = std::chrono::steady_clock::now() + 2s;

            while (!server->snapshot().accepted && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }

            stop.request_stop();
        });
        const auto result = facade->loadURL(address, {}, stop.get_token());
        requester.join();
        REQUIRE(result.error);
        CHECK(result.status == i::IpfTransferStatus::CANCELLED);
        CHECK(result.nativeRetired);
        CHECK_FALSE(server->snapshot().handshakeComplete);
        CHECK(server->snapshot().bytesRead == 0);
    }
}

TEST_CASE("IPF literal UTF8 initial credentials are sent only on verified controlled HTTPS") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto peer = tlsPeer(response());
    const auto address = "https://\xc3\xa9:\xe5\xaf\x86@127.0.0.1:" + std::to_string(peer.port()) + "/profiles.ipf";
    const auto result = facade->loadURL(address);
    REQUIRE_FALSE(result.error);
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    auto snapshot = peer.snapshot();

    while (!snapshot.completed && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        snapshot = peer.snapshot();
    }

    const std::string capture(snapshot.firstClientBytes.begin(), snapshot.firstClientBytes.end());
    CHECK(capture.find("\r\nAuthorization: Basic w6k65a+G\r\n") != std::string::npos);
    CHECK(snapshot.closeNotifyComplete);
    CHECK(result.ipf.complete);
}

TEST_CASE("IPF internal completed-error prefix instrumentation preserves HTTP and profile evidence") {
    // Internal owner-state instrumentation, not an operating-system reproduction.
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const i::IpfTransferOptions options;
    t::TcpOptions native{"127.0.0.1",
                         9,
                         {},
                         options.dnsPort,
                         options.connectTimeout,
                         options.connectTimeout,
                         options.readTimeout,
                         options.writeTimeout,
                         options.pollInterval,
                         options.nativeLimits};
    auto copy = options.byteWork;
    auto work = options.nativeWork;
    auto made = t::detail::RawByteOwner::create(*bootstrap, native, {4096, 4096}, copy);
    REQUIRE(made);
    auto &owner = **made;
    auto &core = owner.core();
    const std::string pendingOutput = "pending-request-output";
    REQUIRE(owner.write(std::as_bytes(std::span(pendingOutput)), copy));
    const auto writesBefore = core.outcome.reportedWriteBytes;
    const auto wire = response();
    REQUIRE(wire.size() <= native.limits.inputBytes);
    std::memcpy(core.input.get(), wire.data(), wire.size());
    core.runState = t::TcpState::CONNECTED;
    core.readDeadline = std::chrono::steady_clock::now() + 5s;
#ifdef _WIN32
    core.readPending = true;
    core.pending = 1;
    core.completed(true, asio::error::connection_reset, wire.size());
#else
    // epoll reports readiness with zero bytes: exercise its already-owned input counterpart.
    core.inputBegin = 0;
    core.inputEnd = wire.size();
    core.recordFailure({t::TcpError::READ_FAILURE, "instrumented-owned-prefix"});
#endif
    REQUIRE(core.outcome.primary.error == t::TcpError::READ_FAILURE);
    REQUIRE_FALSE(core.closeRequested);
    REQUIRE(d::transferHasOwnedInput(core));
    i::IpfTransferResult result;
    result.error =
        i::IpfTransferDiagnostic{i::IpfTransferError::TRANSPORT_FAILURE, "transfer-native", core.outcome.primary};
    d::TransferWork counters;
    d::TransferProfiles profiles(options.ipf, result, counters, options.limits);
    d::TransferResponse parser(options.limits, counters, profiles, result);
    std::size_t turns = 0;

    while (!parser.is_done() && turns++ < 32) {
        const auto input = owner.input();
        REQUIRE(input);

        if (input->empty()) {
            REQUIRE(d::transferHasOwnedInput(core));
            REQUIRE(d::drainTransferPlainInput(owner, work, copy));

            continue;
        }

        parser.ipfWork = options.ipf.turnWork;
        boost::system::error_code error;
        const auto accepted = parser.put(boost::asio::buffer(input->data(), input->size()), error);
        REQUIRE((!error || error == boost::beast::http::error::need_more));
        REQUIRE(accepted != 0);
        REQUIRE(owner.consumeInput(accepted));
    }

    REQUIRE(parser.is_done());
    REQUIRE(result.ipf.profiles.size() == 1);
    CHECK(result.ipf.profiles.front().getSymbol() == u"IBM");
    CHECK(result.ipf.completionSeen);
    CHECK_FALSE(result.ipf.finished);
    CHECK(result.error->error == i::IpfTransferError::TRANSPORT_FAILURE);
    CHECK(result.error->transport.error == t::TcpError::READ_FAILURE);
    CHECK_FALSE(result.metadataCommitted);
    CHECK_FALSE(d::transferHasOwnedInput(core));
    CHECK(owner.pendingOutput() == pendingOutput.size());
    CHECK(core.outcome.reportedWriteBytes == writesBefore);
    CHECK_FALSE(core.writePending);
    REQUIRE(owner.requestClose());

    while (core.state() != t::TcpState::CLOSED && turns++ < 64) {
        REQUIRE(owner.poll(work, copy));
    }

    REQUIRE(core.state() == t::TcpState::CLOSED);
    CHECK(core.resources().pendingOperations == 0);
    CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
}

TEST_CASE("IPF borrowed reader follows moved bootstrap state after original wrapper destruction") {
    std::optional<t::RuntimeBootstrap> moved;
    auto created = t::RuntimeBootstrap::create();
    REQUIRE(created);
    auto original = std::make_unique<t::RuntimeBootstrap>(std::move(*created));
    auto facade = reader(*original);
    h::IpfHttpPeer peer({response(), response()});
    moved.emplace(std::move(*original));
    const auto beforeDestruction = facade->loadURL(url(peer.port()));
    REQUIRE(beforeDestruction.status == i::IpfTransferStatus::SUCCESS);
    CHECK(beforeDestruction.ipf.complete);
    original.reset();
    const auto afterDestruction = facade->loadURL(url(peer.port()));
    REQUIRE(afterDestruction.status == i::IpfTransferStatus::SUCCESS);
    CHECK(afterDestruction.ipf.complete);
    CHECK(afterDestruction.nativeRetired);
    CHECK(beforeDestruction.ipf.profiles.front().getSymbol() == u"IBM");
}

TEST_CASE("IPF HTTPS callback may move destroy bootstrap wrapper before redirected child acquisition") {
    std::optional<t::RuntimeBootstrap> moved;
    auto created = t::RuntimeBootstrap::create();
    REQUIRE(created);
    auto original = std::make_unique<t::RuntimeBootstrap>(std::move(*created));
    auto facade = reader(*original, tlsOptions());
    auto destination = tlsPeer(response());
    const auto location = url(destination.port(), "/profiles.ipf", true);
    auto origin = tlsPeer("HTTP/1.1 302 Found\r\nLocation: " + location + "\r\nContent-Length: 0\r\n\r\n");
    auto provider = std::make_shared<Provider>();
    provider->action = [&] {
        moved.emplace(std::move(*original));
        original.reset();
    };
    i::IpfUrlRequest request;
    request.provider = provider;
    const auto result = facade->loadURL(url(origin.port(), "/profiles.ipf", true), request);
    REQUIRE_FALSE(result.error);
    CHECK(result.redirects == 1);
    CHECK(provider->calls == 1);
    CHECK(result.ipf.complete);
    CHECK(result.nativeRetired);
    CHECK_FALSE(original);
    CHECK(moved.has_value());
}

TEST_CASE("IPF stable child borrow preserves bounded lease admission and original refusal order") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto lease = t::detail::BootstrapLease::acquireForRuntime(*bootstrap);
    REQUIRE(lease);
    const auto initial = lease->state()->borrowers.load();
    {
        auto child = t::detail::BootstrapLease::acquire(*lease);
        REQUIRE(child);
        CHECK(child->state() == lease->state());
        CHECK(lease->state()->borrowers.load() == initial + 1);
    }
    CHECK(lease->state()->borrowers.load() == initial);

    const i::IpfTransferOptions options;
    t::TcpOptions native{"127.0.0.1",
                         9,
                         {},
                         options.dnsPort,
                         options.connectTimeout,
                         options.connectTimeout,
                         options.readTimeout,
                         options.writeTimeout,
                         options.pollInterval,
                         options.nativeLimits};

    struct BorrowRestore {
        decltype(*lease->state()) state;
        std::size_t original;

        ~BorrowRestore() {
            state.borrowers.store(original);
        }
    } restore{*lease->state(), initial};

    lease->state()->borrowers.store((std::numeric_limits<std::size_t>::max)());
    auto empty = options.byteWork;
    empty.operations = 0;
    auto workRefusal = t::detail::RawByteOwner::create(*lease, native, {4096, 4096}, empty);
    REQUIRE_FALSE(workRefusal);
    CHECK(workRefusal.error().error == t::TcpError::WORK_LIMIT);
    CHECK(workRefusal.error().stage == "raw-create-attempt");
    auto copy = options.byteWork;
    auto capacityRefusal = t::detail::RawByteOwner::create(*lease, native, {4096, 4096}, copy);
    REQUIRE_FALSE(capacityRefusal);
    CHECK(capacityRefusal.error().error == t::TcpError::RESOURCE_LIMIT);
    CHECK(capacityRefusal.error().stage == "bootstrap-lease-borrowers");
    CHECK(lease->state()->borrowers.load() == (std::numeric_limits<std::size_t>::max)());
}

TEST_CASE("IPF strict UTF8 source domains reject malformed scalars and preserve encoded credential text") {
    const i::IpfTransferLimits limits;
    const std::vector<std::string> invalid{
        std::string("\x80", 1),         std::string("\xC0\xAF", 2),         std::string("\xE0\x80\xAF", 3),
        std::string("\xED\xA0\x80", 3), std::string("\xF4\x90\x80\x80", 4), std::string("\xF0\x9F", 2),
        std::string("\xFF", 1)};

    for (const auto &bytes : invalid) {
        CHECK_FALSE(d::parseTransferSource("https://" + bytes + ":p@localhost/a", limits, true));
        CHECK_FALSE(d::parseTransferSource("/tmp/" + bytes, limits, true));
        CHECK_FALSE(i::HttpAuthorization::basic(bytes, "p"));
    }

    for (const auto encoded : {"%FF", "%80", "%C0%AF", "%ED%A0%80", "%F4%90%80%80", "%F0%9F"}) {
        CHECK_FALSE(d::parseTransferSource(std::string("file:///tmp/") + encoded, limits, true));
    }

    const auto scalar = d::parseTransferSource("https://\xF0\x9F\x9A\x80:p@localhost/a", limits, true);
    REQUIRE(scalar);
    REQUIRE(scalar->userinfo);
    CHECK(scalar->userinfo->value() == "Basic 8J+agDpw");
    const auto encoded = d::parseTransferSource("https://%FF:p@localhost/a", limits, true);
    REQUIRE(encoded);
    REQUIRE(encoded->userinfo);
    CHECK(encoded->userinfo->value() == "Basic JUZGOnA=");
    CHECK(d::parseTransferSource("file:///tmp/%F0%9F%9A%80", limits, true));
    const std::string external(1024 * 1024, 'x');
    CHECK_FALSE(i::HttpAuthorization::basic(external, external, 1));
    CHECK_FALSE(i::HttpAuthorization::custom(external, external, 1));
    CHECK_FALSE(i::HttpAuthorization::bearer(external, 1));
    CHECK_FALSE(d::validAuthorization(external, 1));
}

#ifndef _WIN32
TEST_CASE("IPF local opened regular identity follows symlink and rejects FIFO device without waiting") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    // Special inode fixtures require native POSIX storage; Windows-mounted filesystems may reject mkfifo.
    const auto root =
        std::filesystem::path("/tmp") / ("qdcxx-ipf-regular-" + std::to_string(::getpid()) + "-" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(root));

    struct Cleanup {
        std::filesystem::path root;

        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(root / "regular", error);
            std::filesystem::remove(root / "link", error);
            std::filesystem::remove(root / "fifo", error);
            std::filesystem::remove(root, error);
        }
    } cleanup{root};

    {
        std::ofstream output(root / "regular", std::ios::binary);
        output << COMPLETE_IPF;
        REQUIRE(output.good());
    }
    std::filesystem::create_symlink(root / "regular", root / "link");
    const auto regular = facade->loadURL((root / "link").string());
    REQUIRE_FALSE(regular.error);
    CHECK(regular.ipf.complete);
    REQUIRE(::mkfifo((root / "fifo").c_str(), 0600) == 0);
    const auto begin = std::chrono::steady_clock::now();
    const auto fifo = facade->loadURL((root / "fifo").string());
    REQUIRE(fifo.error);
    CHECK(fifo.error->error == i::IpfTransferError::INVALID_SOURCE);
    CHECK(fifo.error->stage == "source-not-regular");
    CHECK(std::chrono::steady_clock::now() - begin < 1s);
    CHECK(fifo.wireBytes == 0);
    const auto device = facade->loadURL("/dev/zero");
    REQUIRE(device.error);
    CHECK(device.error->stage == "source-not-regular");
    CHECK(device.wireBytes == 0);
}
#endif

TEST_CASE("IPF real verified TLS cached prefix drains without ordinary progress with pending request output") {
    // Real verified codec plus internal completion-state injection, not an OS packet-order claim.
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = tlsPeer(response(), false);
    const auto options = tlsOptions();
    org::ttldtor::qdcxx::client::EndpointTls settings;
    settings.trustPem = options.trustPem;
    settings.referenceIdentity = "localhost";
    settings.handshakeTimeout = 2s;
    settings.closeTimeout = 1s;
    t::TcpOptions native{"127.0.0.1",
                         peer.port(),
                         {},
                         options.dnsPort,
                         options.connectTimeout,
                         options.connectTimeout,
                         options.readTimeout,
                         options.writeTimeout,
                         options.pollInterval,
                         options.nativeLimits};
    auto copy = options.byteWork;
    auto work = options.nativeWork;
    const auto requirements = t::tlsRequirements(options.tls.limits, options.tls.host);
    REQUIRE(requirements);
    t::TlsWork tls{requirements->shape, options.tlsWork};
    auto made = t::detail::TlsByteOwner::create(*bootstrap, native, {8192, 8192}, settings, options.tls, copy, tls);
    REQUIRE(made);
    auto &owner = **made;
    REQUIRE(owner.start());
    auto &core = owner.bytes().core();
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while ((!core.engine->verified() || core.engine->plaintext().empty()) &&
           std::chrono::steady_clock::now() < deadline) {
        REQUIRE(owner.poll(work, copy, tls));
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(core.engine->verified());
    REQUIRE_FALSE(core.engine->plaintext().empty());
    REQUIRE_FALSE(core.readPending);
    REQUIRE_FALSE(core.writePending);
    const std::string request = "GET /pending HTTP/1.1\r\nHost: localhost\r\n\r\n";
    REQUIRE(owner.write(std::as_bytes(std::span(request)), copy));
    const auto writesBefore = core.outcome.reportedWriteBytes;
    const auto wireBefore = core.engine->ciphertext().size();
    core.input[0] = std::byte{0};
    core.readPending = true;
    ++core.pending;
    core.completed(true, asio::error::connection_reset, 1);
    REQUIRE(core.outcome.primary.error == t::TcpError::READ_FAILURE);
    REQUIRE_FALSE(core.closeRequested);
    CHECK_FALSE(d::transferHasOwnedInput(core));
    const auto verified = d::transferVerifiedInput(core);
    REQUIRE_FALSE(verified.empty());
    std::vector<std::byte> retained(verified.begin(), verified.end());
    core.engine->consumePlaintext(verified.size());

    i::IpfTransferResult result;
    result.error =
        i::IpfTransferDiagnostic{i::IpfTransferError::TRANSPORT_FAILURE, "transfer-native", core.outcome.primary};
    d::TransferWork counters;
    d::TransferProfiles profiles(options.ipf, result, counters, options.limits);
    d::TransferResponse parser(options.limits, counters, profiles, result);
    std::size_t offset = 0;

    for (unsigned turns = 0; !parser.is_done() && turns < 32; ++turns) {
        parser.ipfWork = options.ipf.turnWork;
        boost::system::error_code error;
        const auto accepted =
            parser.put(boost::asio::buffer(retained.data() + offset, retained.size() - offset), error);
        REQUIRE((!error || error == boost::beast::http::error::need_more));
        REQUIRE(accepted != 0);
        offset += accepted;
    }

    REQUIRE(parser.is_done());
    REQUIRE(result.ipf.profiles.size() == 1);
    CHECK(result.ipf.profiles.front().getSymbol() == u"IBM");
    CHECK(result.ipf.completionSeen);
    CHECK_FALSE(result.ipf.finished);
    CHECK(result.error->transport.error == t::TcpError::READ_FAILURE);
    CHECK(owner.bytes().pendingOutput() == request.size());
    CHECK(core.outcome.reportedWriteBytes == writesBefore);
    CHECK(core.engine->ciphertext().size() == wireBefore);
    CHECK_FALSE(core.writePending);
    CHECK(core.readReady);
    CHECK(core.readCount == 1);
    CHECK(d::transferVerifiedInput(core).empty());
    REQUIRE(owner.requestClose());

    for (unsigned turns = 0; core.state() != t::TcpState::CLOSED && turns < 64; ++turns) {
        REQUIRE(owner.poll(work, copy, tls));
    }

    REQUIRE(core.state() == t::TcpState::CLOSED);
    CHECK(core.resources().pendingOperations == 0);
    CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
}

TEST_CASE("IPF raw error prefix zero copy operations never submits queued multichunk output") {
    // Internal core-state instrumentation; no live socket or OS error-order claim.
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const i::IpfTransferOptions options;
    t::TcpOptions native{"127.0.0.1",
                         9,
                         {},
                         options.dnsPort,
                         options.connectTimeout,
                         options.connectTimeout,
                         options.readTimeout,
                         options.writeTimeout,
                         options.pollInterval,
                         options.nativeLimits};
    native.limits.writeBytes = 1;
    auto copy = options.byteWork;
    auto work = options.nativeWork;
    auto made = t::detail::RawByteOwner::create(*bootstrap, native, {4096, 4096}, copy);
    REQUIRE(made);
    auto &owner = **made;
    auto &core = owner.core();
    const std::string output = "GET /pending HTTP/1.1\r\nHost: localhost\r\n\r\n";
    REQUIRE(owner.write(std::as_bytes(std::span(output)), copy));
    core.runState = t::TcpState::CONNECTED;
    core.readDeadline = std::chrono::steady_clock::now() + 5s;
    core.input[0] = std::byte{'x'};
    core.inputBegin = 0;
    core.inputEnd = 1;
    core.recordFailure({t::TcpError::READ_FAILURE, "instrumented-read-prefix"});
    copy.operations = 0;
    const auto bytesBefore = copy.bytes;
    const auto stepsBefore = work.steps;
    REQUIRE(d::transferHasOwnedInput(core));
    const auto drained = d::drainTransferPlainInput(owner, work, copy);
    REQUIRE(drained);
    CHECK(drained->status == t::TcpPollStatus::YIELD);
    CHECK_FALSE(core.writePending);
    CHECK(core.pending == 0);
    CHECK(core.outcome.reportedWriteBytes == 0);
    CHECK(owner.pendingOutput() == output.size());
    CHECK(core.inputBegin == 0);
    CHECK(core.inputEnd == 1);
    CHECK(copy.bytes == bytesBefore);
    CHECK(work.steps == stepsBefore);
    CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
    CHECK(core.outcome.primary.stage == "instrumented-read-prefix");
    copy.operations = 1;
    work.steps = 0;
    const auto nativeRefusal = d::drainTransferPlainInput(owner, work, copy);
    REQUIRE(nativeRefusal);
    CHECK(nativeRefusal->status == t::TcpPollStatus::YIELD);
    CHECK(core.inputEnd == 1);
    CHECK_FALSE(core.writePending);
    work.steps = 1;
    copy.bytes = 0;
    const auto copyRefusal = d::drainTransferPlainInput(owner, work, copy);
    REQUIRE(copyRefusal);
    CHECK(copyRefusal->status == t::TcpPollStatus::YIELD);
    CHECK(core.inputEnd == 1);
    CHECK_FALSE(core.writePending);
    copy.bytes = 1;
    const auto admitted = d::drainTransferPlainInput(owner, work, copy);
    REQUIRE(admitted);
    CHECK(admitted->status == t::TcpPollStatus::PROGRESS);
    REQUIRE(owner.input());
    CHECK(owner.input()->size() == 1);
    CHECK(owner.input()->front() == std::byte{'x'});
    CHECK(owner.pendingOutput() == output.size());
    CHECK_FALSE(core.writePending);
    CHECK(core.outcome.primary.error == t::TcpError::READ_FAILURE);
    REQUIRE(owner.requestClose());

    for (unsigned turns = 0; core.state() != t::TcpState::CLOSED && turns < 32; ++turns) {
        REQUIRE(owner.poll(work, copy));
    }

    REQUIRE(core.state() == t::TcpState::CLOSED);
}

TEST_CASE("IPF temporary settings trust overlap is funded before exact-cap real HTTPS") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto small = tlsOptions();
    auto large = small;
    large.trustPem.insert(large.trustPem.begin(),
                          static_cast<std::size_t>(large.tls.limits.trustBytes) - large.trustPem.size(), '\n');
    REQUIRE(large.trustPem.size() == large.tls.limits.trustBytes);
    const auto threshold = [&](i::IpfTransferOptions options) {
        std::size_t low = 0;
        std::size_t high = options.limits.residentBytes;

        while (low < high) {
            options.limits.residentBytes = low + (high - low) / 2;
            const auto candidate = i::InstrumentProfileReader::create(*bootstrap, options);

            if (candidate) {
                high = options.limits.residentBytes;
            } else {
                REQUIRE(candidate.error().error == i::IpfTransferError::RESOURCE_LIMIT);
                low = options.limits.residentBytes + 1;
            }
        }

        return low;
    };
    const auto smallCap = threshold(small);
    const auto largeCap = threshold(large);
    const auto addedTrust = large.trustPem.size() - small.trustPem.size();
    // Host settings and the caller's temporary EndpointTls coexist; B funds its own third copy.
    CHECK(largeCap - smallCap >= 2 * addedTrust);
    std::cout << "TLS trust resident small=" << smallCap << " large=" << largeCap
              << " trustBytes=" << large.trustPem.size() << "\n";
    large.limits.residentBytes = largeCap - 1;
    const auto below = i::InstrumentProfileReader::create(*bootstrap, large);
    REQUIRE_FALSE(below);
    CHECK(below.error().error == i::IpfTransferError::RESOURCE_LIMIT);
    large.limits.residentBytes = largeCap;
    auto exact = i::InstrumentProfileReader::create(*bootstrap, large);
    REQUIRE(exact);
    auto peer = tlsPeer(response());
    const auto result = (*exact)->loadURL(url(peer.port(), "/profiles.ipf", true));
    REQUIRE_FALSE(result.error);
    CHECK(result.ipf.complete);
    CHECK(result.nativeRetired);
    CHECK(result.metadataCommitted);
}

namespace {
/** Load one finite encoded oracle fixture without filename-based format selection.
 * @param name Synthetic fixture basename.
 * @return Exact encoded body.
 */
std::string encodedFixture(std::string_view name) {
    std::ifstream stream(std::string(ORG_TTLDTOR_QDCXX_IPF_COMPRESSION_FIXTURES) + "/" + std::string(name) + ".bin",
                         std::ios::binary);
    REQUIRE(stream);

    return {std::istreambuf_iterator<char>(stream), {}};
}
} // namespace

TEST_CASE("IPF compressed local files HTTP body splits and declared gzip share one bounded stage") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);

    for (const auto name :
         {"gzip", "gzip-optional", "gzip-concatenated", "zip-stored", "zip-descriptor", "zip64-small",
          "zip-directory-first", "zip-followed-corruption", "zip-full-archive", "zip-descriptor-nosignature",
          "zip64-descriptor64-small", "zip64-directory-nosignature-first"}) {
        CAPTURE(name);
        const auto local = std::string(ORG_TTLDTOR_QDCXX_IPF_COMPRESSION_FIXTURES) + "/" + name + ".bin";
        auto file = facade->loadURL(local);
        INFO((file.error ? file.error->stage : "none"));
        REQUIRE_FALSE(file.error);
        CHECK(file.ipf.profiles.size() == 1);
        CHECK(file.ipf.complete);
        CHECK(file.envelope.finished);
        CHECK(file.nativeRetired);
        CHECK_FALSE(file.httpFinished);
        h::IpfHttpPeer peer({response(encodedFixture(name))}, 1);
        auto http = facade->loadURL(url(peer.port(), "/no-compression-suffix"));
        INFO((http.error ? http.error->stage : "none"));
        REQUIRE_FALSE(http.error);
        CHECK(http.httpFinished);
        CHECK(http.envelope.finished);
        CHECK(http.nativeRetired);
        CHECK(http.ipf.profiles.size() == 1);
        CHECK(http.ipf.acceptedBytes == COMPLETE_IPF.size());
        CHECK(http.envelope.inputBytes == encodedFixture(name).size());
    }

    h::IpfHttpPeer declared({response(encodedFixture("gzip"), "Content-Encoding: gzip\r\n")}, 1);
    auto gzip = facade->loadURL(url(declared.port()));
    REQUIRE_FALSE(gzip.error);
    CHECK(gzip.envelope.kind == i::IpfEnvelopeKind::GZIP);
    CHECK(gzip.ipf.complete);
    CHECK(gzip.httpFinished);
    CHECK(gzip.nativeRetired);
}

TEST_CASE("IPF gzip integrity failure preserves the owned marker prefix without success") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfTransferOptions options;
    options.ipf.turnWork = {1, 1, 1, 1};
    auto facade = reader(*bootstrap, options);

    for (const auto name : {"gzip-bad-crc", "gzip-truncated", "zip-bad-crc", "zip-truncated-descriptor"}) {
        CAPTURE(name);
        h::IpfHttpPeer peer({response(encodedFixture(name))});
        auto result = facade->loadURL(url(peer.port()));
        REQUIRE(result.error);
        CHECK(result.error->error == i::IpfTransferError::CONTAINER_FAILURE);
        CHECK(result.ipf.profiles.size() == 1);
        CHECK(result.ipf.completionSeen);
        CHECK_FALSE(result.envelope.finished);
        CHECK_FALSE(result.metadataCommitted);
        CHECK(result.nativeRetired);
    }

    for (const auto fields :
         {"Content-Encoding: br\r\n", "Content-Encoding: deflate\r\n", "Content-Encoding: gzip, gzip\r\n",
          "Content-Encoding: gzip\r\nContent-Encoding: identity\r\n"}) {
        CAPTURE(fields);
        h::IpfHttpPeer peer({response(encodedFixture("gzip"), fields)});
        auto result = facade->loadURL(url(peer.port()));
        REQUIRE(result.error);
        CHECK(result.ipf.profiles.empty());
        CHECK_FALSE(result.metadataCommitted);
    }

    h::IpfHttpPeer invalid({response(COMPLETE_IPF, "Content-Encoding: gzip\r\n")});
    auto plain = facade->loadURL(url(invalid.port()));
    REQUIRE(plain.error);
    CHECK(plain.error->error == i::IpfTransferError::CONTAINER_FAILURE);
    CHECK(plain.ipf.profiles.empty());
}

TEST_CASE("IPF compressed container and missing IPF completion are distinct") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    i::IpfTransferOptions options;
    options.ipf.completion = i::IpfCompletionPolicy::REQUIRE;
    auto facade = reader(*bootstrap, options);
    h::IpfHttpPeer peer({response(encodedFixture("gzip-missing-marker"))});
    auto result = facade->loadURL(url(peer.port()));
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::IPF_FAILURE);
    CHECK(result.envelope.finished);
    CHECK(result.httpFinished);
    CHECK(result.nativeRetired);
    CHECK(result.ipf.profiles.size() == 1);
    REQUIRE(result.ipf.error);
    CHECK(result.ipf.error->error == i::IpfReadError::MISSING_COMPLETE);
}

TEST_CASE("IPF gzip over verified HTTPS uses clean TLS retirement") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap, tlsOptions());
    auto peer = tlsPeer(response(encodedFixture("gzip"), "Content-Encoding: gzip\r\n"));
    auto result = facade->loadURL(url(peer.port(), "/profiles.ipf", true));
    INFO((result.error ? result.error->stage : "none"));
    REQUIRE_FALSE(result.error);
    CHECK(result.ipf.complete);
    CHECK(result.envelope.finished);
    CHECK(result.httpFinished);
    CHECK(result.nativeRetired);
    CHECK(result.metadataCommitted);
}

TEST_CASE("IPF compressed wait cancellation from another thread never calls SDK there") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    auto reply = response(encodedFixture("gzip"), "Content-Encoding: gzip\r\n");
    reply.resize(reply.size() - 8);
    h::IpfHttpPeer peer({reply}, 4096, true);
    std::stop_source stop;
    std::jthread requester([&] {
        const auto until = std::chrono::steady_clock::now() + 2s;

        while (peer.sentBytes() < reply.size() && std::chrono::steady_clock::now() < until) {
            std::this_thread::sleep_for(1ms);
        }

        stop.request_stop();
    });
    const auto result = facade->loadURL(url(peer.port()), {}, stop.get_token());
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::CANCELLED);
    CHECK(result.nativeRetired);
    CHECK_FALSE(result.envelope.finished);
    CHECK_FALSE(result.metadataCommitted);
}

TEST_CASE("IPF gzip envelope is not recursively decoded after HTTP coding") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto facade = reader(*bootstrap);
    h::IpfHttpPeer peer({response(encodedFixture("gzip-nested"), "Content-Encoding: gzip\r\n")}, 1);
    auto result = facade->loadURL(url(peer.port()));
    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::IPF_FAILURE);
    CHECK(result.envelope.kind == i::IpfEnvelopeKind::GZIP);
    CHECK(result.ipf.profiles.empty());
    CHECK(result.nativeRetired);
}
