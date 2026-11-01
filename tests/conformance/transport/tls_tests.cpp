// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: explicit immutable ordinary fixtures and finite owned loopback peers only.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "tls_dns_peer.hpp"
#include "tls_framing_peer.hpp"
#include "tls_test_support.hpp"

#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/sslerr.h>

#include <array>
#include <thread>

namespace {
namespace x = org::ttldtor::qdcxx::test::tls;
namespace t = org::ttldtor::qdcxx::transport;
namespace h = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

/** Count forbidden credential invocation in preverification/deadline scenarios. */
class UnavailableProvider final : public x::d::CredentialProvider {
    public:
    unsigned calls = 0; ///< Actual serialized invocations; no token is produced.

    /** Observe any invocation without writing credential data.
     * @param destination Borrowed output, deliberately untouched.
     * @return Static unavailable result.
     */
    std::expected<std::size_t, x::d::CredentialError> basicToken(std::span<char16_t> destination) override {
        (void)destination;
        ++calls;

        return std::unexpected(x::d::CredentialError::UNAVAILABLE);
    }
};

/** Run one expected factory rejection with a usable composer and no network activation.
 * @param input Structurally valid snapshot input; cryptographic contents may be invalid.
 * @param options Explicit TLS proof/backing limits.
 * @param error Expected detached category.
 * @param stage Expected static checkpoint, empty when more than one early layer is permitted.
 */
void factoryReject(const x::d::EndpointSettings &input, t::TlsOptions options, t::TlsError error,
                   std::string_view stage = {}) {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = x::schema();
    auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = x::writer(local, input.budgets);
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto work = x::sessionTurn();
    auto tlsWork = x::tlsTurn(*requirements);
    auto result = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == error);

    if (!stage.empty()) {
        CHECK(result.error().stage == stage);
    }

    const std::array<std::int32_t, 1> ids{17};
    auto writerWork = input.budgets.writerPreparation;
    CHECK(composer.describe(ids, writerWork));
}

/** Check successful TLS work after explicit abort without hiding documented ambiguous native evidence.
 * @param outcome Detached terminal after the test explicitly requested local abort.
 */
void checkNoTlsFailureAfterAbort(const t::TlsTerminal &outcome) {
    bool documentedNativeAbort = false;
#ifdef _WIN32
    constexpr int AMBIGUOUS_OWNED_CLOSE = 1236; ///< ERROR_CONNECTION_ABORTED is preserved by TcpTerminal's contract.
    documentedNativeAbort = outcome.aborted && outcome.primary.error == t::TlsError::TRANSPORT_FAILURE &&
                            outcome.primary.stage == "socket-read" && outcome.primary.sdkCode == 0 &&
                            outcome.primary.verifyCode == 0 &&
                            outcome.primary.transport.error == t::TcpError::READ_FAILURE &&
                            outcome.primary.transport.nativeCode == AMBIGUOUS_OWNED_CLOSE &&
                            outcome.wire.primary.error == t::TcpError::READ_FAILURE &&
                            outcome.wire.primary.nativeCode == AMBIGUOUS_OWNED_CLOSE;
#endif
    INFO("abort terminal category=" << static_cast<int>(outcome.primary.error) << " stage=" << outcome.primary.stage
                                    << " native=" << outcome.primary.transport.nativeCode
                                    << " sdk=" << outcome.primary.sdkCode);
    CHECK((outcome.primary.error == t::TlsError::NONE || documentedNativeAbort));
}
} // namespace

TEST_CASE("verified TLS 1.2 and 1.3 admit a real heartbeat over independent route and DNS identity") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
        auto options = x::peerOptions(version, h::PeerCloseMode::HOLD);
        options.replyBytes = {1, 0}; // Canonical empty type0 envelope, inside actual encrypted records.
        auto started = h::TlsTestPeer::start(std::move(options));
        REQUIRE(started);
        auto peer = std::move(*started);
        auto value = x::runtime(*bootstrap, x::settings(peer.port()));
        CHECK(value.state() == t::TlsState::CONFIGURED);
        REQUIRE(value.start());
        bool event = false;
        const auto deadline = std::chrono::steady_clock::now() + 5s;

        while (!event && value.state() != t::TlsState::CLOSED && std::chrono::steady_clock::now() < deadline) {
            auto result = x::poll(value);
            event |= result.event.has_value();
            std::this_thread::sleep_for(1ms);
        }

        REQUIRE(event);
        CHECK(value.state() == t::TlsState::CONNECTED);
        x::until(value, [&] {
            return peer.snapshot().handshakeComplete && peer.snapshot().sniLocalhost;
        });
        CHECK(peer.snapshot().sniPresent);
        CHECK(peer.snapshot().sniLocalhost);
        CHECK(peer.snapshot().tlsVersion == static_cast<std::uint32_t>(version));
        const auto negotiated = peer.snapshot();
        MESSAGE("negotiated version=" << negotiated.tlsVersion << " cipher=" << negotiated.cipherSuite << " group="
                                      << negotiated.groupId << " signatureHash=" << negotiated.signatureHashNid
                                      << " signatureScheme=" << negotiated.signatureScheme
                                      << " signatureBytes=" << negotiated.signatureBytes);
        CHECK(negotiated.signatureBytes == 256);
        CHECK(negotiated.cipherSuite == (version == h::PeerTlsVersion::TLS_1_2 ? 0xc02fU : 0x1301U));
        CHECK((negotiated.groupId == NID_X25519 || negotiated.groupId == NID_X9_62_prime256v1));
        CHECK((negotiated.signatureHashNid == NID_sha256 || negotiated.signatureHashNid == NID_sha384 ||
               negotiated.signatureHashNid == NID_sha512));
        x::close(value);
        const auto terminal = value.terminal();
        REQUIRE(terminal);
        CHECK(terminal->aborted);
        CHECK_FALSE(terminal->orderly);
        INFO("terminal stage=" << terminal->primary.stage << " native=" << terminal->primary.transport.nativeCode);
        checkNoTlsFailureAfterAbort(*terminal);
    }
}

TEST_CASE("TLS numeric IP SAN succeeds and wrong DNS identity fails without protocol publication") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (bool wrong : {false, true}) {
        auto options = x::peerOptions();
        options.replyBytes = {1, 0};
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto settings = x::settings(peer->port());
        settings.tls->referenceIdentity = wrong ? "wrong.invalid" : "127.0.0.1";
        auto value = x::runtime(*bootstrap, settings);
        REQUIRE(value.start());

        if (wrong) {
            x::until(value, [&] {
                return value.state() == t::TlsState::CLOSED;
            });
            REQUIRE(value.terminal());
            CHECK(value.terminal()->primary.error == t::TlsError::IDENTITY_FAILURE);
            CHECK_FALSE(value.terminal()->orderly);
            CHECK_FALSE(value.terminal()->aborted);
        } else {
            x::until(value, [&] {
                return value.state() == t::TlsState::CONNECTED;
            });
            x::until(value, [&] {
                return peer->snapshot().handshakeComplete;
            });
            CHECK_FALSE(peer->snapshot().sniPresent);
            CHECK_FALSE(peer->snapshot().sniLocalhost);
            x::close(value);
        }
    }
}

TEST_CASE("TLS untrusted chain and expired leaf remain typed trust failures") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (bool expired : {false, true}) {
        auto options = x::peerOptions();

        if (expired) {
            options.certificateFile = x::fixture("expired.pem");
        }

        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto settings = x::settings(peer->port());

        if (!expired) {
            settings.tls->trustPem = x::read("other-ca.pem");
        }

        auto value = x::runtime(*bootstrap, settings);
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CLOSED;
        });
        REQUIRE(value.terminal());
        CHECK(value.terminal()->primary.error == t::TlsError::TRUST_FAILURE);
        CHECK_FALSE(value.terminal()->orderly);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
    }
}

TEST_CASE("TLS abort during handshake drains all owned native operations") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = x::peerOptions();
    options.pauseBeforeHandshake = 500ms;
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.state() == t::TlsState::HANDSHAKING;
    });
    x::close(value);
    CHECK(value.resources().transport.pendingOperations == 0);
    CHECK(value.terminal()->aborted);
    CHECK_FALSE(value.terminal()->orderly);
    CHECK(value.terminal()->acceptedPlaintextBytes == 0);
}

TEST_CASE("TLS public owner move transfers state and wrong-thread checks do not mutate it") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(x::peerOptions());
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    auto moved = std::move(value);
    CHECK(value.state() == t::TlsState::CLOSED);
    CHECK_FALSE(value.terminal());
    REQUIRE_FALSE(value.start());
    std::optional<t::TlsDiagnostic> rejection;
    std::thread foreign([&] {
        const auto result = moved.start();

        if (!result) {
            rejection = result.error();
        }
    });
    foreign.join();
    REQUIRE(rejection);
    CHECK(rejection->error == t::TlsError::WRONG_THREAD);
    CHECK(moved.state() == t::TlsState::CONFIGURED);
    REQUIRE(moved.start());
    x::until(moved, [&] {
        return moved.state() == t::TlsState::CONNECTED;
    });
    x::close(moved);
}

TEST_CASE("TLS factory precharges every nonzero coordinate atomically before consuming the composer") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = x::schema();
    const auto input = x::settings(4321);
    const auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    const auto options = x::options();
    const auto need = t::tlsRequirements(options.limits, options.host);
    REQUIRE(need);

    for (std::size_t coordinate = 0; coordinate < t::TLS_WORK_UNIT_COUNT; ++coordinate) {
        if (need->factoryAdmission.units[coordinate] == 0) {
            continue;
        }

        auto composer = x::writer(local, input.budgets);
        auto tlsWork = t::TlsWork{need->shape, need->factoryAdmission};
        --tlsWork.remaining.units[coordinate];
        const auto original = tlsWork;
        auto work = x::sessionTurn();
        auto rejected = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::TlsError::WORK_LIMIT);
        CHECK(tlsWork.shape == original.shape);
        CHECK(tlsWork.remaining == original.remaining);
        auto writerWork = input.budgets.writerPreparation;
        const std::array<std::int32_t, 1> ids{17};
        CHECK(composer.describe(ids, writerWork));
    }
}

TEST_CASE("TLS rejects a shallow empty outer certificate through its real factory without consuming composer") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = x::schema();
    auto input = x::settings(4321);
    // Exact two-byte DER 30 00: empty outer SEQUENCE, with no nested key or SPKI content.
    input.tls->trustPem = "-----BEGIN CERTIFICATE-----\nMAA=\n-----END CERTIFICATE-----\n";
    auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = x::writer(local, input.budgets);
    const auto options = x::options();
    const auto need = t::tlsRequirements(options.limits, options.host);
    REQUIRE(need);
    auto tlsWork = t::TlsWork{need->shape, need->factoryAdmission};
    auto work = x::sessionTurn();
    auto rejected = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::TlsError::TRUST_FAILURE);
    CHECK(rejected.error().stage == "tls-trust-anchor");
    CHECK(tlsWork.remaining == t::TlsWorkVector{});
    auto writerWork = input.budgets.writerPreparation;
    const std::array<std::int32_t, 1> ids{17};
    CHECK(composer.describe(ids, writerWork));
}

TEST_CASE("TLS counts all trust blocks before decoding an excess anchor") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = x::schema();
    auto input = x::settings(4321);
    input.tls->trustPem += input.tls->trustPem;
    auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = x::writer(local, input.budgets);
    auto options = x::options();
    options.limits.anchors = 1;
    const auto need = t::tlsRequirements(options.limits, options.host);
    REQUIRE(need);
    auto tlsWork = x::tlsTurn(*need);
    auto work = x::sessionTurn();
    auto rejected = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::TlsError::TRUST_FAILURE);
    CHECK(rejected.error().stage == "trust-anchor-count");
    auto writerWork = input.budgets.writerPreparation;
    const std::array<std::int32_t, 1> ids{17};
    CHECK(composer.describe(ids, writerWork));
}

TEST_CASE("TLS ordinary key policy admits selected boundaries and rejects explicit unsupported or oversized keys") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto &[name, accepted] :
         std::array<std::pair<std::string_view, bool>, 10>{{{"ca-explicit-p256.pem", false},
                                                            {"ca-p521.pem", false},
                                                            {"ca-rsa5120.pem", false},
                                                            {"ca-rsa1024.pem", false},
                                                            {"ca-rsa4096.pem", true},
                                                            {"ca-p256.pem", true},
                                                            {"ca-p384.pem", true},
                                                            {"ca-rsa-exponent3.pem", false},
                                                            {"ca-rsa-pss.pem", true},
                                                            {"ca-ed25519.pem", false}}}) {
        CAPTURE(name);
        auto local = x::schema();
        auto input = x::settings(4321);
        input.tls->trustPem = x::read(name);
        auto endpoint = x::d::EndpointOptions::create(local, input);
        REQUIRE(endpoint);
        auto composer = x::writer(local, input.budgets);
        const auto options = x::options();
        const auto need = t::tlsRequirements(options.limits, options.host);
        REQUIRE(need);
        auto tlsWork = x::tlsTurn(*need);
        auto sessionWork = x::sessionTurn();
        auto result = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, sessionWork);
        CHECK(result.has_value() == accepted);

        if (!accepted) {
            REQUIRE_FALSE(result);
            CHECK(result.error().error == t::TlsError::TRUST_FAILURE);
            CHECK(result.error().stage == "tls-trust-anchor");
            auto work = input.budgets.writerPreparation;
            const std::array<std::int32_t, 1> ids{17};
            CHECK(composer.describe(ids, work));
        } else {
            REQUIRE(result);
            CHECK(result->state() == t::TlsState::CONFIGURED);
            x::close(*result);
        }
    }
}

TEST_CASE("TLS clean close exchanges notify while withheld notify remains pending until explicit deadline") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    SUBCASE("actual peer close completes orderly") {
        auto options = x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::CLEAN);
        options.replyBytes = {8, 1, 'D', 'X', 'P', '3', 0, 0, 0}; // Complete empty peer descriptor before EOF.
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto value = x::runtime(*bootstrap, x::settings(peer->port()));
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CLOSED;
        });
        REQUIRE(value.terminal());
        INFO("close primary=" << value.terminal()->primary.stage << " wire=" << value.terminal()->wire.primary.stage
                              << " notify=" << value.terminal()->closeNotifyReceived
                              << " pending=" << value.terminal()->pendingCiphertextBytes
                              << " sdkPending=" << value.terminal()->tlsDataPending);
        CHECK(value.terminal()->primary.error == t::TlsError::NONE);
        CHECK(value.terminal()->orderly);
        CHECK(value.terminal()->closeNotifyReceived);
        CHECK_FALSE(value.terminal()->aborted);
        CHECK(value.terminal()->pendingCiphertextBytes == 0);
        CHECK(value.resources().transport.pendingOperations == 0);
    }
    SUBCASE("local first shutdown remains incomplete without peer notify") {
        auto peer = h::TlsTestPeer::start(x::peerOptions());
        REQUIRE(peer);
        auto input = x::settings(peer->port());
        input.tls->closeTimeout = 150ms;
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CONNECTED;
        });
        x::until(value, [&] {
            return peer->snapshot().handshakeComplete;
        });

        for (unsigned turn = 0; turn < 16; ++turn) {
            (void)x::poll(value);
        }

        REQUIRE(value.requestTlsClose());
        const auto requirements = value.requirements();

        for (std::size_t coordinate = 0; coordinate < t::TLS_WORK_UNIT_COUNT; ++coordinate) {
            if (requirements.close.units[coordinate] == 0) {
                continue;
            }

            auto shortWork = t::TlsWork{requirements.shape, requirements.close};
            --shortWork.remaining.units[coordinate];
            const auto original = shortWork;
            auto native = x::nativeTurn();
            auto session = x::sessionTurn();
            auto rejected = value.poll(native, shortWork, session);
            REQUIRE_FALSE(rejected);
            CHECK(rejected.error().error == t::TlsError::WORK_LIMIT);
            CHECK(shortWork.remaining == original.remaining);
        }

        auto exact = t::TlsWork{requirements.shape, requirements.close};
        auto native = x::nativeTurn();
        auto session = x::sessionTurn();
        REQUIRE(value.poll(native, exact, session));
        CHECK(exact.remaining == t::TlsWorkVector{});
        const auto pendingUntil = std::chrono::steady_clock::now() + 30ms;

        while (std::chrono::steady_clock::now() < pendingUntil) {
            (void)x::poll(value);
            REQUIRE(value.state() == t::TlsState::CLOSING);
            REQUIRE_FALSE(value.terminal());
            std::this_thread::sleep_for(1ms);
        }

        x::until(value, [&] {
            return value.state() == t::TlsState::CLOSED;
        });
        REQUIRE(value.terminal());
        CHECK(value.terminal()->primary.error == t::TlsError::DEADLINE);
        CHECK_FALSE(value.terminal()->orderly);
        CHECK_FALSE(value.terminal()->closeNotifyReceived);
        CHECK_FALSE(value.terminal()->aborted);
    }
}

TEST_CASE("TLS abrupt native EOF is truncation rather than orderly close") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(x::peerOptions(h::PeerTlsVersion::TLS_1_3, h::PeerCloseMode::ABRUPT));
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.state() == t::TlsState::CLOSED;
    });
    REQUIRE(value.terminal());
    CHECK(value.terminal()->primary.error == t::TlsError::TRANSPORT_FAILURE);
    CHECK(value.terminal()->primary.stage == "tls-truncated-eof");
    CHECK_FALSE(value.terminal()->orderly);
    CHECK_FALSE(value.terminal()->closeNotifyReceived);
}

TEST_CASE("TLS endpoints retain independent verification failures and immutable peer epochs") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto firstPeer = h::TlsTestPeer::start(x::peerOptions());
    auto secondPeer = h::TlsTestPeer::start(x::peerOptions());
    REQUIRE(firstPeer);
    REQUIRE(secondPeer);
    auto first = x::runtime(*bootstrap, x::settings(firstPeer->port()));
    auto secondInput = x::settings(secondPeer->port());
    secondInput.tls->trustPem = x::read("other-ca.pem");
    auto second = x::runtime(*bootstrap, secondInput);
    REQUIRE(first.start());
    REQUIRE(second.start());
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while ((first.state() != t::TlsState::CONNECTED || second.state() != t::TlsState::CLOSED) &&
           std::chrono::steady_clock::now() < deadline) {
        (void)x::poll(first);
        (void)x::poll(second);
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(first.state() == t::TlsState::CONNECTED);
    REQUIRE(second.state() == t::TlsState::CLOSED);
    REQUIRE(second.terminal());
    CHECK(second.terminal()->primary.error == t::TlsError::TRUST_FAILURE);
    CHECK_FALSE(first.terminal());
    CHECK_FALSE(first.peer().identity().sameEpoch(second.peer().identity()));
    x::close(first);
    checkNoTlsFailureAfterAbort(*first.terminal());
}

TEST_CASE(
    "TLS partial native ciphertext remains owned under zero socket credit and resumes without duplicate plaintext") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool abort : {false, true}) {
        auto peerOptions = x::peerOptions();
        peerOptions.expectedClientBytes = std::vector<std::uint8_t>{1, 0};
        auto peer = h::TlsTestPeer::start(peerOptions);
        REQUIRE(peer);
        auto input = x::settings(peer->port());
        input.budgets.tcp.writeBytes = 7;
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CONNECTED;
        });
        x::until(value, [&] {
            return peer->snapshot().handshakeComplete;
        });

        for (unsigned turn = 0; turn < 16; ++turn) {
            (void)x::poll(value);
        }

        auto work = x::sessionTurn();
        REQUIRE(value.composeHeartbeat({false, {}, {}, {}, {}}, work));

        for (unsigned turn = 0; turn < 32; ++turn) {
            auto native = x::nativeTurn();
            native.socketBytes = 0;
            auto tlsWork = x::tlsTurn(value.requirements());
            auto sessionWork = x::sessionTurn();
            REQUIRE(value.poll(native, tlsWork, sessionWork));
        }

        CHECK(peer->snapshot().bytesRead == 0);

        if (abort) {
            x::close(value);
            REQUIRE(value.terminal());
            CHECK(value.terminal()->acceptedPlaintextBytes == 2);
            CHECK(value.terminal()->pendingPlaintextBytes == 0);
            CHECK(value.terminal()->pendingCiphertextBytes > 2);
            CHECK_FALSE(value.terminal()->orderly);
        } else {
            x::until(value, [&] {
                return peer->snapshot().expectedBytesMatched;
            });
            CHECK(peer->snapshot().bytesRead == 2);
            x::close(value);
            REQUIRE(value.terminal());
            CHECK(value.terminal()->acceptedPlaintextBytes == 2);
            CHECK(value.terminal()->pendingCiphertextBytes == 0);
            CHECK(value.terminal()->wire.reportedWriteBytes > value.terminal()->acceptedPlaintextBytes);
        }
    }
}

TEST_CASE("TLS exact initial and ordinary entry vectors are sufficient and every one-short coordinate is atomic") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = x::peerOptions();
    options.expectedClientBytes = std::vector<std::uint8_t>{1, 0};
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.state() == t::TlsState::HANDSHAKING;
    });
    const auto requirements = value.requirements();

    // Native connection completes before the first possible SSL operation is admitted.
    for (const auto &need : {requirements.initialStart, requirements.entry}) {
        if (need == requirements.entry) {
            x::until(value, [&] {
                return value.state() == t::TlsState::CONNECTED;
            });
            x::until(value, [&] {
                return peer->snapshot().handshakeComplete;
            });

            for (unsigned turn = 0; turn < 16; ++turn) {
                (void)x::poll(value);
            }

            auto work = x::sessionTurn();
            REQUIRE(value.composeHeartbeat({false, {}, {}, {}, {}}, work));
        }

        for (std::size_t coordinate = 0; coordinate < t::TLS_WORK_UNIT_COUNT; ++coordinate) {
            if (need.units[coordinate] == 0) {
                continue;
            }

            auto shortWork = t::TlsWork{requirements.shape, need};
            --shortWork.remaining.units[coordinate];
            const auto original = shortWork;
            auto native = x::nativeTurn();
            auto session = x::sessionTurn();
            auto result = value.poll(native, shortWork, session);
            REQUIRE_FALSE(result);
            CHECK(result.error().error == t::TlsError::WORK_LIMIT);
            CHECK(shortWork.remaining == original.remaining);
        }

        auto exact = t::TlsWork{requirements.shape, need};
        auto native = x::nativeTurn();
        auto session = x::sessionTurn();
        REQUIRE(value.poll(native, exact, session));
        CHECK(exact.remaining == t::TlsWorkVector{});
    }

    x::until(value, [&] {
        return peer->snapshot().expectedBytesMatched;
    });
    x::close(value);
    REQUIRE(value.terminal());
    CHECK(value.terminal()->acceptedPlaintextBytes == 2);
}

TEST_CASE("TLS trust accepts actual 512 KiB and 256 valid blocks and rejects excess or trailing material") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    const auto anchor = x::read("ca.pem");
    auto input = x::settings(4321);
    input.tls->trustPem.clear();

    for (unsigned index = 0; index < 256; ++index) {
        input.tls->trustPem += anchor;
    }

    REQUIRE(input.tls->trustPem.size() < 524288);
    input.tls->trustPem.resize(524288, ' ');
    auto accepted = x::runtime(*bootstrap, input);
    CHECK(accepted.state() == t::TlsState::CONFIGURED);
    x::close(accepted);
    CHECK(accepted.terminal()->acceptedPlaintextBytes == 0);
    input.tls->trustPem.clear();

    for (unsigned index = 0; index < 257; ++index) {
        input.tls->trustPem += anchor;
    }

    factoryReject(input, x::options(), t::TlsError::TRUST_FAILURE, "trust-anchor-count");
    input.tls->trustPem = anchor + "trailing-noncertificate";
    factoryReject(input, x::options(), t::TlsError::TRUST_FAILURE, "trust-framing");
    input.tls->trustPem = "not-a-PEM-certificate";
    factoryReject(input, x::options(), t::TlsError::TRUST_FAILURE, "trust-framing");
    input.tls->trustPem.assign(524289, ' ');
    auto local = x::schema();
    CHECK_FALSE(x::d::EndpointOptions::create(local, input));
    input.tls->trustPem.clear();
    CHECK_FALSE(x::d::EndpointOptions::create(local, input));
    input.tls->trustPem = anchor;
    input.tls->referenceIdentity.clear();
    CHECK_FALSE(x::d::EndpointOptions::create(local, input));
}

TEST_CASE("TLS whole resident reservation exact boundary succeeds and one-short allowances reject before activation") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto local = x::schema();
    const auto input = x::settings(4321);
    auto endpoint = x::d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto options = x::options();
    const auto requirements = t::tlsRequirements(options.limits, options.host);
    REQUIRE(requirements);
    auto composer = x::writer(local, input.budgets);
    auto tlsWork = x::tlsTurn(*requirements);
    auto session = x::sessionTurn();
    auto first = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), options, tlsWork, session);
    REQUIRE(first);
    const auto reservation = first->resources();
    CHECK(reservation.totalBytes <= options.residentBytes);
    CHECK(reservation.peakBytes == reservation.totalBytes);
    CHECK(reservation.transport.totalBytes <= input.budgets.tcp.residentBytes);

    for (const unsigned selection : {0U, 1U, 2U}) {
        auto tooSmall = options;

        if (selection == 0) {
            tooSmall.residentBytes = reservation.totalBytes - 1;
        }

        if (selection == 1) {
            --tooSmall.sdkInstanceBytes;
        }

        if (selection == 2) {
            --tooSmall.sdkSharedBytes;
        }

        factoryReject(input, tooSmall, t::TlsError::RESOURCE_LIMIT);
    }

    options.residentBytes = reservation.totalBytes;
    auto exactComposer = x::writer(local, input.budgets);
    tlsWork = x::tlsTurn(*requirements);
    session = x::sessionTurn();
    auto exact = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(exactComposer), options, tlsWork, session);
    REQUIRE(exact);
    CHECK(exact->resources().totalBytes == options.residentBytes);
    CHECK(exact->resources().peakBytes <= options.residentBytes);
    x::close(*exact);
    x::close(*first);
}

TEST_CASE("TLS handshake deadline retires a silent verified-ineligible attempt without credentials") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = x::peerOptions();
    options.pauseBeforeHandshake = 500ms;
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    auto provider = std::make_shared<UnavailableProvider>();
    auto input = x::settings(peer->port(), provider);
    input.tls->handshakeTimeout = 50ms;
    auto value = x::runtime(*bootstrap, input);
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.state() == t::TlsState::CLOSED;
    });
    REQUIRE(value.terminal());
    CHECK(value.terminal()->primary.error == t::TlsError::DEADLINE);
    CHECK(provider->calls == 0);
    CHECK(value.terminal()->acceptedPlaintextBytes == 0);
    CHECK(value.resources().transport.pendingOperations == 0);
    CHECK_FALSE(value.terminal()->orderly);
}

TEST_CASE("TLS clears ordinary stale SDK errors before handshake and application entries") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = x::peerOptions();
    options.expectedClientBytes = std::vector<std::uint8_t>{1, 0};
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.state() == t::TlsState::HANDSHAKING;
    });
    ERR_raise(ERR_LIB_USER, 1); // Ordinary same-owner harmless queue state, without custom hooks or text.
    REQUIRE(ERR_peek_error() != 0);
    (void)x::poll(value);
    x::until(value, [&] {
        return value.state() == t::TlsState::CONNECTED;
    });
    x::until(value, [&] {
        return peer->snapshot().handshakeComplete;
    });

    for (unsigned turn = 0; turn < 16; ++turn) {
        (void)x::poll(value);
    }

    ERR_raise(ERR_LIB_USER, 1);
    REQUIRE(ERR_peek_error() != 0);
    auto work = x::sessionTurn();
    REQUIRE(value.composeHeartbeat({false, {}, {}, {}, {}}, work));
    x::until(value, [&] {
        return peer->snapshot().expectedBytesMatched;
    });
    CHECK(ERR_peek_error() == 0);
    x::close(value);
    REQUIRE(value.terminal());
    checkNoTlsFailureAfterAbort(*value.terminal());
    CHECK(value.terminal()->acceptedPlaintextBytes == 2);
}

TEST_CASE("TLS refuses legacy versions and unselected cipher group or signature without a credential callback") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    using Profile = std::pair<h::PeerTlsVersion, h::PeerPolicy>;
    const std::array<Profile, 5> profiles{{{h::PeerTlsVersion::TLS_1_0, h::PeerPolicy::MODERN},
                                           {h::PeerTlsVersion::TLS_1_1, h::PeerPolicy::MODERN},
                                           {h::PeerTlsVersion::TLS_1_2, h::PeerPolicy::CBC_ONLY},
                                           {h::PeerTlsVersion::TLS_1_3, h::PeerPolicy::P521_ONLY},
                                           {h::PeerTlsVersion::TLS_1_2, h::PeerPolicy::SHA1_ONLY}}};

    for (const auto &[version, policy] : profiles) {
        CAPTURE(static_cast<unsigned>(version));
        CAPTURE(static_cast<unsigned>(policy));
        auto options = x::peerOptions(version);
        options.policy = policy;
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto provider = std::make_shared<UnavailableProvider>();
        auto value = x::runtime(*bootstrap, x::settings(peer->port(), provider));
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CLOSED;
        });
        REQUIRE(value.terminal());
        CHECK(value.terminal()->primary.error == t::TlsError::HANDSHAKE_FAILURE);
        CHECK(provider->calls == 0);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        CHECK_FALSE(value.terminal()->orderly);
    }
}

TEST_CASE("TLS rejects future validity CN-only partial wildcard and wrong IP before credentials") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    /** Exact ordinary certificate/reference/category fixture expectation. */
    struct Case {
        std::string_view certificate; ///< Ordinary committed PEM, using the same committed test-only key.
        std::string_view identity;    ///< Explicit immutable reference, independent of the loopback route.
        t::TlsError error;            ///< Expected detached category.
    };

    const std::array<Case, 4> cases{{{"server-not-yet-valid.pem", "localhost", t::TlsError::TRUST_FAILURE},
                                     {"server-cn-only.pem", "localhost", t::TlsError::IDENTITY_FAILURE},
                                     {"server-partial-wildcard.pem", "localxhost", t::TlsError::IDENTITY_FAILURE},
                                     {"server.pem", "192.0.2.1", t::TlsError::IDENTITY_FAILURE}}};

    for (const auto &test : cases) {
        CAPTURE(test.certificate);
        auto options = x::peerOptions();
        options.certificateFile = x::fixture(test.certificate);
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto provider = std::make_shared<UnavailableProvider>();
        auto input = x::settings(peer->port(), provider);
        input.tls->referenceIdentity = test.identity;
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CLOSED;
        });
        REQUIRE(value.terminal());
        CHECK(value.terminal()->primary.error == test.error);
        CHECK(provider->calls == 0);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        CHECK_FALSE(value.terminal()->orderly);
    }
}

TEST_CASE("TLS ordinary certificate body limit and chain depth are enforced by actual handshakes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    /** Exact ordinary signed fixture and expected production outcome. */
    struct Case {
        std::string_view file; ///< Leaf-only or ordered leaf/intermediate PEM; root is explicitly trusted separately.
        bool accepted;         ///< Whether the configured body/depth policy permits this exact fixture.
        t::TlsError error;     ///< Expected rejection category for a disallowed fixture.
        int verifyCode;        ///< Exact expected X509 cause, or zero for handshake-header rejection.
    };

    const std::array<Case, 4> cases{{{"server-certificate-max.pem", true, t::TlsError::NONE, 0},
                                     {"server-certificate-over.pem", false, t::TlsError::HANDSHAKE_FAILURE, 0},
                                     {"server-depth4-chain.pem", true, t::TlsError::NONE, 0},
                                     {"server-depth5-chain.pem", false, t::TlsError::TRUST_FAILURE, 22}}};

    for (const auto &test : cases) {
        CAPTURE(test.file);
        auto options = x::peerOptions(h::PeerTlsVersion::TLS_1_2);
        options.certificateFile = x::fixture(test.file);
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto provider = std::make_shared<UnavailableProvider>();
        auto input = x::settings(peer->port(), test.accepted ? nullptr : provider);
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());

        if (test.accepted) {
            x::until(value, [&] {
                return value.state() == t::TlsState::CONNECTED;
            });
            x::until(value, [&] {
                return peer->snapshot().handshakeComplete;
            });
            x::close(value);
        } else {
            x::until(value, [&] {
                return value.state() == t::TlsState::CLOSED;
            });
            REQUIRE(value.terminal());
            CHECK(value.terminal()->primary.error == test.error);
            CHECK(value.terminal()->primary.verifyCode == test.verifyCode);

            if (test.verifyCode == 0) {
                CHECK(ERR_GET_REASON(value.terminal()->primary.sdkCode) == SSL_R_EXCESSIVE_MESSAGE_SIZE);
            }

            CHECK(provider->calls == 0);
            CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        }
    }
}

TEST_CASE("TLS named route CNAME lookup preserves independent localhost identity and actual SNI") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peerOptions = x::peerOptions();
    peerOptions.replyBytes = {1, 0};
    auto peer = h::TlsTestPeer::start(peerOptions);
    REQUIRE(peer);
    h::TlsDnsPeer dns;
    auto input = x::settings(peer->port());
    input.host = "tls.route.invalid";
    constexpr std::size_t DNS_SDK_RESERVATION = 80 * 1024 * 1024; ///< Qualified DNS-RESOURCES.md per-channel bound.
    input.budgets.tcp.residentBytes += DNS_SDK_RESERVATION;
    const std::array<t::DnsServer, 1> servers{{{{127, 0, 0, 1}}}};
    auto options = x::options();
    options.dns = {servers, dns.port()};
    options.residentBytes += DNS_SDK_RESERVATION;
    auto value = x::runtime(*bootstrap, input, options);
    REQUIRE(value.start());
    bool event = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!event && value.state() != t::TlsState::CLOSED && std::chrono::steady_clock::now() < deadline) {
        dns.step();
        auto result = x::poll(value);
        event |= result.event.has_value();
        std::this_thread::sleep_for(1ms);
    }

    if (!event) {
        const auto terminal = value.terminal();
        INFO("named route state=" << static_cast<int>(value.state())
                                  << " stage=" << (terminal ? terminal->primary.stage : "active"));
        REQUIRE(event);
    }

    CHECK(value.state() == t::TlsState::CONNECTED);
    CHECK(dns.queries() >= 2);
    x::until(value, [&] {
        return peer->snapshot().handshakeComplete && peer->snapshot().sniLocalhost;
    });
    CHECK(peer->snapshot().sniPresent);
    CHECK(value.resources().transport.dnsEnvelopeBytes == 0);
    CHECK(value.resources().transport.peakBytes >= DNS_SDK_RESERVATION);
    CHECK(input.tls->referenceIdentity == "localhost");
    x::close(value);
}

TEST_CASE("TLS ordinary peer tickets are processed before a real application heartbeat") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto options = x::peerOptions();
    options.tickets = 2;
    options.replyBytes = {1, 0};
    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    auto value = x::runtime(*bootstrap, x::settings(peer->port()));
    REQUIRE(value.start());
    bool event = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!event && value.state() != t::TlsState::CLOSED && std::chrono::steady_clock::now() < deadline) {
        auto result = x::poll(value);
        event |= result.event.has_value();
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(event);
    x::until(value, [&] {
        return peer->snapshot().handshakeComplete;
    });
    MESSAGE("actual outgoing peer NewSessionTicket count=" << peer->snapshot().ticketsSent);
    CHECK(peer->snapshot().ticketsSent > 0);
    CHECK(value.state() == t::TlsState::CONNECTED);
    x::close(value);
}

TEST_CASE("TLS sustained large control traffic spans explicit caller batches with simultaneous native owners") {
    constexpr unsigned FRAMES = 128;           ///< Finite total, spanning four caller-selected32-operation batches.
    constexpr unsigned BATCH_OPERATIONS = 32;  ///< Credits are renewed explicitly by this test, never by the runtime.
    constexpr std::size_t VALUE_UNITS = 1000;  ///< Three-byte UTF-8 units create an ordinary roughly3KiB envelope.
    constexpr std::size_t WRITE_QUANTUM = 257; ///< Forces independent partial ciphertext completion.
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto input = x::settings(4321);
    // This one-way workload deliberately has no new peer input until all client bytes arrive.
    // Keep its idle-read allowance separate from the short deadline-failure scenarios above.
    input.timeouts.read = input.timeouts.write = 45s;
    input.budgets.tcp.writeBytes = WRITE_QUANTUM;
    const std::u16string valueText(VALUE_UNITS, char16_t{0x0800});
    const x::p::LivePropertyView property{u"padding", std::u16string_view(valueText)};
    const x::p::LiveProtocolView descriptor{std::span(&property, 1), {}, {}};
    // The already-conformant pure protocol composer supplies expected transport bytes independently of TLS.
    auto pureSchema = x::schema();
    auto pureWriter = x::writer(pureSchema, input.budgets);
    auto pureWork = x::sessionTurn();
    auto pure =
        x::p::LiveSession::create(std::move(pureSchema), std::move(pureWriter), input.budgets.session, pureWork);
    REQUIRE(pure);
    auto envelope = pure->composeProtocol(descriptor, pureWork);
    REQUIRE(envelope);
    const auto expectedBytes = envelope->bytes().size() * FRAMES;
    auto options = x::peerOptions();
    options.initialBytes = {8, 1, 'D', 'X', 'P', '3', 0, 0, 0, 1, 0};
    options.readDelay = 500ms;
    options.readTimeout = 45s;
    options.overallTimeout = 60s;
    options.expectedClientBytes.emplace();
    options.expectedClientBytes->reserve(expectedBytes);

    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        for (const auto byte : envelope->bytes()) {
            options.expectedClientBytes->push_back(std::to_integer<std::uint8_t>(byte));
        }
    }

    auto peer = h::TlsTestPeer::start(options);
    REQUIRE(peer);
    input.port = peer->port();
    auto value = x::runtime(*bootstrap, input);
    REQUIRE(value.start());
    unsigned incoming = 0;
    const auto deadline = std::chrono::steady_clock::now() + 50s;

    while (incoming < 2 && value.state() != t::TlsState::CLOSED && std::chrono::steady_clock::now() < deadline) {
        if (x::poll(value).event) {
            ++incoming;
        }

        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(incoming == 2);

    for (unsigned turn = 0; turn < 16; ++turn) {
        (void)x::poll(value);
    }

    auto batch = x::sessionTurn();
    batch.operations = BATCH_OPERATIONS;
    unsigned batches = 1;
    unsigned admitted = 0;
    std::size_t peakPending = 0;

    while ((admitted < FRAMES || !peer->snapshot().expectedBytesMatched) && value.state() != t::TlsState::CLOSED &&
           std::chrono::steady_clock::now() < deadline) {
        if (admitted < FRAMES) {
            const auto output = value.composeProtocol(descriptor, batch);

            if (output) {
                ++admitted;
            } else if (output.error().transport.protocolError == x::p::LiveError::WORK_LIMIT) {
                REQUIRE(batch.operations == 0);
                batch = x::sessionTurn();
                batch.operations = BATCH_OPERATIONS;
                ++batches;
            } else {
                REQUIRE(output.error().error == t::TlsError::RESOURCE_LIMIT);
            }
        }

        const auto progress = x::poll(value);
        peakPending = (std::max)(peakPending, value.resources().transport.pendingOperations);

        if (progress.status == t::TcpPollStatus::IDLE || progress.status == t::TcpPollStatus::YIELD) {
            std::this_thread::sleep_for(1ms);
        }
    }

    const auto observed = peer->snapshot();
    INFO("sustained admitted=" << admitted << " peerRead=" << observed.bytesRead << " peerFailed=" << observed.failed
                               << " peerTimedOut=" << observed.timedOut << " state=" << static_cast<int>(value.state())
                               << " testDeadlineReached=" << (std::chrono::steady_clock::now() >= deadline));

    const auto observedTerminal = value.terminal();
    INFO("terminal category=" << (observedTerminal ? static_cast<int>(observedTerminal->primary.error) : -1)
                              << " stage=" << (observedTerminal ? observedTerminal->primary.stage : "none")
                              << " native=" << (observedTerminal ? observedTerminal->primary.transport.nativeCode : 0)
                              << " sdk=" << (observedTerminal ? observedTerminal->primary.sdkCode : 0)
                              << " acceptedPlaintext="
                              << (observedTerminal ? observedTerminal->acceptedPlaintextBytes : 0));

    REQUIRE(admitted == FRAMES);
    REQUIRE(peer->snapshot().expectedBytesMatched);
    CHECK(peer->snapshot().bytesRead == expectedBytes);
    CHECK(batches == FRAMES / BATCH_OPERATIONS);
    CHECK(peakPending >= 2);

    for (unsigned turn = 0; turn < 16; ++turn) {
        (void)x::poll(value);
    }

    x::close(value);
    REQUIRE(value.terminal());
    CHECK(value.terminal()->acceptedPlaintextBytes == expectedBytes);
    CHECK(value.terminal()->pendingPlaintextBytes == 0);
    checkNoTlsFailureAfterAbort(*value.terminal());
    MESSAGE("sustained frames=" << admitted << " plaintext=" << expectedBytes
                                << " ciphertext=" << value.terminal()->wire.reportedWriteBytes << " batches=" << batches
                                << " peakPending=" << peakPending);
}

TEST_CASE("TLS real RSA4096 server signs both TLS 1.2 key exchange and TLS 1.3 certificate verification") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
        auto options = x::peerOptions(version);
        options.certificateFile = x::fixture("server-rsa4096.pem");
        options.privateKeyFile = x::fixture("server-rsa4096.key");
        auto peer = h::TlsTestPeer::start(options);
        REQUIRE(peer);
        auto value = x::runtime(*bootstrap, x::settings(peer->port()));
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.state() == t::TlsState::CONNECTED;
        });
        x::until(value, [&] {
            return peer->snapshot().handshakeComplete;
        });
        const auto observed = peer->snapshot();
        MESSAGE("RSA4096 version=" << observed.tlsVersion << " cipher=" << observed.cipherSuite
                                   << " group=" << observed.groupId << " signatureHash=" << observed.signatureHashNid
                                   << " signatureScheme=" << observed.signatureScheme
                                   << " signatureBytes=" << observed.signatureBytes);
        CHECK(observed.signatureBytes == 512);
        CHECK(observed.tlsVersion == static_cast<std::uint32_t>(version));
        CHECK((observed.signatureHashNid == NID_sha256 || observed.signatureHashNid == NID_sha384 ||
               observed.signatureHashNid == NID_sha512));
        x::close(value);
    }
}

TEST_CASE("TLS ordinary ServerHello length-only header respects the exact SDK bound before any body") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (bool overLimit : {false, true}) {
        h::TlsFramingPeer peer(overLimit);
        auto provider = std::make_shared<UnavailableProvider>();
        auto input = x::settings(peer.port(), provider);
        input.tls->handshakeTimeout = 500ms;
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());
        const auto deadline = std::chrono::steady_clock::now() + 2s;

        while (peer.sentBytes() != 9 && value.state() != t::TlsState::CLOSED &&
               std::chrono::steady_clock::now() < deadline) {
            peer.step();
            (void)x::poll(value);
            std::this_thread::sleep_for(1ms);
        }

        REQUIRE(peer.receivedBytes() > 0);
        REQUIRE(peer.sentBytes() == 9);

        if (overLimit) {
            x::until(value, [&] {
                return value.state() == t::TlsState::CLOSED;
            });
            REQUIRE(value.terminal());
            CHECK(value.terminal()->primary.error == t::TlsError::HANDSHAKE_FAILURE);
            CHECK(ERR_GET_REASON(value.terminal()->primary.sdkCode) == SSL_R_EXCESSIVE_MESSAGE_SIZE);
        } else {
            const auto pendingUntil = std::chrono::steady_clock::now() + 30ms;

            while (std::chrono::steady_clock::now() < pendingUntil) {
                (void)x::poll(value);
                REQUIRE(value.state() == t::TlsState::HANDSHAKING);
                REQUIRE_FALSE(value.terminal());
                std::this_thread::sleep_for(1ms);
            }

            x::close(value);
            REQUIRE(value.terminal());
            checkNoTlsFailureAfterAbort(*value.terminal());
        }

        CHECK(provider->calls == 0);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        CHECK_FALSE(value.terminal()->orderly);
    }
}
