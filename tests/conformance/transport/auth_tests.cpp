// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_test_support.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {
namespace x = org::ttldtor::qdcxx::test::tls;
namespace t = org::ttldtor::qdcxx::transport;
namespace p = org::ttldtor::qdcxx::protocol;
namespace d = org::ttldtor::qdcxx::client;
namespace c = org::ttldtor::qdcxx::core;
namespace h = org::ttldtor::qdcxx::test;
namespace e = org::ttldtor::qdcxx::encoding;
using namespace std::chrono_literals;
constexpr std::u16string_view TEST_TOKEN = u"Basic cWRjeHg6dGVzdA=="; ///< Public synthetic test credential only.

/** Append canonical numeric framing, independently of LiveSession's control composer.
 * @param output Test-owned complete-message storage.
 * @param value Exact numeric grammar field.
 */
void number(std::vector<std::uint8_t> &output, std::int64_t value) {
    std::array<std::byte, 9> encoded{};
    const auto result = e::writeCompactLong(value, encoded);
    REQUIRE(result.status == e::Status::OK);

    for (std::size_t index = 0; index < result.written; ++index) {
        output.push_back(std::to_integer<std::uint8_t>(encoded[index]));
    }
}

/** Append an exact non-null UTFString, without printing its contents on failure.
 * @param output Test-owned complete-message storage.
 * @param value Borrowed synthetic property key or value.
 */
void text(std::vector<std::uint8_t> &output, std::u16string_view value) {
    std::array<std::byte, 1024> encoded{};
    const auto result = e::writeUtfString(value, encoded, {1024, 256});
    REQUIRE(result.status == e::Status::OK);

    for (std::size_t index = 0; index < result.written; ++index) {
        output.push_back(std::to_integer<std::uint8_t>(encoded[index]));
    }
}

/** Build the source-defined type1 DXP3 descriptor with one property and optional STREAM receive readiness.
 * @param key Exact synthetic property key.
 * @param value Exact synthetic property value, including an empty authentication acknowledgement.
 * @param streamReady Whether the peer advertises both STREAM add/remove receive identities.
 * @return Independently assembled complete wire envelope; never a production-composer oracle.
 */
std::vector<std::uint8_t> descriptor(std::u16string_view key, std::u16string_view value, bool streamReady = false) {
    std::vector<std::uint8_t> body{1, 'D', 'X', 'P', '3'};
    number(body, 1);
    text(body, key);
    text(body, value);
    number(body, 0);
    number(body, streamReady ? 2 : 0);

    if (streamReady) {
        number(body, 16);
        text(body, u"STREAM_ADD_SUBSCRIPTION");
        number(body, 0);
        number(body, 17);
        text(body, u"STREAM_REMOVE_SUBSCRIPTION");
        number(body, 0);
    }

    std::vector<std::uint8_t> framed;
    number(framed, static_cast<std::int64_t>(body.size()));
    framed.insert(framed.end(), body.begin(), body.end());

    return framed;
}

/** Observations remain alive independently of both the provider and every public runtime owner. */
struct Observation {
    std::size_t calls = 0;            ///< Exact callback count; never inferred from native writes.
    bool destroyed = false;           ///< Last provider owner has retired.
    bool actionReturned = false;      ///< Callback action completed before its token result was supplied.
    bool reentryRejected = false;     ///< Nested poll returned the checked busy rejection.
    bool compositionRejected = false; ///< Nested application composition returned the checked busy rejection.
    bool closeAccepted = false;       ///< Callback-side request-only cutoff was accepted.
};

/** Deliberate provider outcomes, excluding allocation or SDK fault injection. */
enum class ProviderMode {
    VALID,     ///< Return the exact public synthetic Basic token.
    REJECTED,  ///< Return the static caller rejection.
    THROWS,    ///< Throw fixed text which must never escape the runtime.
    EMPTY,     ///< Return an invalid zero length.
    OVERSIZED, ///< Return a length beyond the borrowed destination.
    BAD_PREFIX ///< Return non-Basic text of a valid bounded length.
};

/** Bounded explicit provider with an optional owner-thread action executed during the callback. */
class Provider final : public d::CredentialProvider {
    std::shared_ptr<Observation> observed_; ///< Test-owned trace, independent of this object's lifetime.
    ProviderMode mode_;                     ///< Explicit selected failure or success mode.
    std::function<void()> action_;          ///< Optional serialized lifetime/reentry action; never an SDK hook.
    std::u16string token_{TEST_TOKEN};      ///< Current explicit synthetic token, renewed only outside callbacks.

    public:
    /** Prepare one provider without invoking application or native work.
     * @param observed Independent trace retained by the test.
     * @param mode Explicit outcome.
     * @param action Optional callback-side owner manipulation.
     */
    Provider(std::shared_ptr<Observation> observed, ProviderMode mode = ProviderMode::VALID,
             std::function<void()> action = {})
        : observed_(std::move(observed)), mode_(mode), action_(std::move(action)) {
    }

    /** Record the last provider release without accessing any runtime. */
    ~Provider() override {
        observed_->destroyed = true;
    }

    /** Set the next synthetic token between completely retired independent attempts.
     * @param token Exact new public test token, copied without invoking the provider.
     */
    void setToken(std::u16string_view token) {
        token_.assign(token);
    }

    /** Supply a synthetic token after the optional callback-side owner operation.
     * @param destination Borrowed bounded UTF-16 staging; never retained.
     * @return Exact written units or static rejection; THROWS exercises the adapter's exception boundary.
     */
    std::expected<std::size_t, d::CredentialError> basicToken(std::span<char16_t> destination) override {
        ++observed_->calls;

        if (action_) {
            action_();
        }

        observed_->actionReturned = true;

        switch (mode_) {
        case ProviderMode::REJECTED:
            return std::unexpected(d::CredentialError::REJECTED);
        case ProviderMode::THROWS:
            throw std::runtime_error("synthetic provider failure");
        case ProviderMode::EMPTY:
            return 0;
        case ProviderMode::OVERSIZED:
            return destination.size() + 1;
        case ProviderMode::BAD_PREFIX:
            REQUIRE(destination.size() >= 6);
            std::fill_n(destination.begin(), 6, u'X');

            return 6;
        case ProviderMode::VALID:
            REQUIRE(destination.size() >= token_.size());
            std::ranges::copy(token_, destination.begin());

            return token_.size();
        }

        return std::unexpected(d::CredentialError::UNAVAILABLE);
    }
};

/** Configure a real challenge/read/reply exchange; application bytes are compared inside the child.
 * @param version Exact native TLS version.
 * @param response Empty auth ACK, a denial, or no bytes for the silent-ACK case.
 * @return Owned explicit peer script.
 */
h::PeerOptions exchange(h::PeerTlsVersion version = h::PeerTlsVersion::TLS_1_3,
                        std::vector<std::uint8_t> response = descriptor(u"authentication", u"")) {
    auto peer = x::peerOptions(version, h::PeerCloseMode::HOLD);
    peer.initialBytes = descriptor(u"authentication", u"LOGIN synthetic-realm");
    peer.expectedClientBytes = descriptor(u"authorization", TEST_TOKEN);
    peer.replyBytes = std::move(response);

    return peer;
}

/** Await a typed runtime failure while keeping provider and public owner assertions separate.
 * @param value Live runtime on its serialized owner.
 * @param expected Required first detached cause.
 */
void fails(t::TlsRuntime &value, t::TlsError expected) {
    x::until(value, [&] {
        return value.state() == t::TlsState::CLOSED;
    });
    REQUIRE(value.terminal());
    CHECK(value.terminal()->primary.error == expected);
}

/** Observe the independent numeric report without treating one retained snapshot as current.
 * The child compares and publishes before sending its reply, but parent snapshot() deliberately
 * preserves an older complete value when an independent report read cannot be validated.
 * This wait never progresses TLS, invokes a provider, or retries an application operation.
 * @param peer Owned child whose exact comparison must become observable.
 * @return Latest numeric report after match, failure, child exit, or the finite test deadline.
 */
h::PeerSnapshot matchedReport(h::TlsTestPeer &peer) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    auto report = peer.snapshot();

    while (!report.expectedBytesMatched && !report.failed && report.childRunning &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        report = peer.snapshot();
    }

    CHECK_FALSE(report.failed);

    return report;
}

/** Progress an owner which the callback may destroy or replace; never reuse its old reference.
 * @param owner Sole test-visible public runtime handle.
 * @param observed Independent callback trace.
 */
void untilCallback(std::optional<t::TlsRuntime> &owner, const Observation &observed) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (owner && observed.calls == 0 && std::chrono::steady_clock::now() < deadline) {
        auto native = x::nativeTurn();
        auto tlsWork = x::tlsTurn(owner->requirements());
        auto session = x::sessionTurn();
        const auto result = owner->poll(native, tlsWork, session);
        REQUIRE(result);
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(observed.calls == 1);
}

/** Keep the exact schema provenance used by the snapshot and outgoing writer available to requests. */
struct SelectedRuntime {
    t::TlsRuntime value;    ///< Independent real runtime; the bootstrap remains externally owned.
    c::RecordContext quote; ///< Exact local Quote identity used by the outgoing writer.
};

/** Prepare a matched real Quote subscription without substituting equal-shaped foreign provenance.
 * @param bootstrap Application owner retained beyond the returned runtime.
 * @param input Explicit immutable settings and provider selection.
 * @return Runtime and an owning record context with the same original schema identity.
 */
SelectedRuntime selectedRuntime(t::RuntimeBootstrap &bootstrap, const d::EndpointSettings &input) {
    auto local = x::schema();
    auto quote = local.recordContext(0);
    auto endpoint = d::EndpointOptions::create(local, input);
    REQUIRE(endpoint);
    auto composer = x::writer(local, input.budgets);
    const auto config = x::options();
    const auto requirements = t::tlsRequirements(config.limits, config.host);
    REQUIRE(requirements);
    auto tlsWork = x::tlsTurn(*requirements);
    auto work = x::sessionTurn();
    auto result = t::TlsRuntime::create(bootstrap, *endpoint, std::move(composer), config, tlsWork, work);
    REQUIRE(result);

    return {std::move(*result), std::move(quote)};
}

/** Observe consumption of a locally admitted subscription without claiming peer acknowledgement.
 * A newly accepted marker proves the prior sole output slot was consumed. The marker is never
 * progressed; requestClose then records separate plaintext/native acceptance at full retirement.
 * @param value Real connected runtime with a single already-admitted subscription output.
 * @param priorPlaintext Known SSL application bytes accepted before this subscription.
 */
void retireAfterSubscription(t::TlsRuntime &value, std::size_t priorPlaintext) {
    bool consumed = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!consumed && std::chrono::steady_clock::now() < deadline) {
        auto work = x::sessionTurn();
        const auto marker = value.composeHeartbeat({}, work);

        if (marker) {
            consumed = true;
        } else {
            REQUIRE(marker.error().error == t::TlsError::RESOURCE_LIMIT);
            (void)x::poll(value);
            std::this_thread::sleep_for(1ms);
        }
    }

    REQUIRE(consumed);
    x::close(value);
    const auto terminal = value.terminal();
    REQUIRE(terminal);
    CHECK(terminal->acceptedPlaintextBytes > priorPlaintext);
    CHECK(terminal->wire.reportedWriteBytes > 0);
    CHECK(value.resources().transport.pendingOperations == 0);
}

/** Produce canonical Basic credentials with a distinct synthetic password for each fresh attempt.
 * @param ordinal Unique bounded test attempt number.
 * @return Complete owned UTF-16 Basic token; never used as a real credential or printed.
 */
std::u16string attemptToken(std::size_t ordinal) {
    constexpr std::u16string_view ALPHABET = u"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    constexpr unsigned BYTE_BITS = 8;
    constexpr unsigned GROUP_BITS = 6;
    constexpr std::uint32_t GROUP_MASK = 0x3f;
    constexpr std::size_t PAD_QUANTUM = 4;
    const auto payload = std::string("qdcxx:attempt-") + std::to_string(ordinal);
    std::u16string result(u"Basic ");
    const auto prefixUnits = result.size();
    std::uint32_t pending{};
    unsigned bits{};

    for (const unsigned char byte : payload) {
        pending = (pending << BYTE_BITS) | byte;
        bits += BYTE_BITS;

        while (bits >= GROUP_BITS) {
            bits -= GROUP_BITS;
            result.push_back(ALPHABET[(pending >> bits) & GROUP_MASK]);
        }
    }

    if (bits != 0) {
        result.push_back(ALPHABET[(pending << (GROUP_BITS - bits)) & GROUP_MASK]);
    }

    while ((result.size() - prefixUnits) % PAD_QUANTUM != 0) {
        result.push_back(u'=');
    }

    return result;
}
} // namespace

TEST_CASE("TLS authentication sends exact authorization after verified TLS and waits for an empty ACK") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
        auto peer = h::TlsTestPeer::start(exchange(version));
        REQUIRE(peer);
        auto observed = std::make_shared<Observation>();
        auto provider = std::make_shared<Provider>(observed);
        auto value = x::runtime(*bootstrap, x::settings(peer->port(), provider));
        CHECK(observed->calls == 0);
        REQUIRE(value.start());
        x::until(value, [&] {
            return value.authentication() == p::LiveAuthentication::ACKNOWLEDGED;
        });
        CHECK(value.state() == t::TlsState::CONNECTED);
        CHECK(observed->calls == 1);
        const auto report = matchedReport(*peer);
        CHECK(report.handshakeComplete);
        CHECK(report.expectedBytesMatched);
        CHECK(report.bytesRead == descriptor(u"authorization", TEST_TOKEN).size());

        for (int turn = 0; turn < 8; ++turn) {
            (void)x::poll(value);
        }

        CHECK(observed->calls == 1);
        x::close(value);
    }
}

TEST_CASE("TLS authentication denial never invokes the provider twice and silent ACK reaches its deadline") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool silent : {false, true}) {
        auto peer = h::TlsTestPeer::start(
            exchange(h::PeerTlsVersion::TLS_1_3,
                     silent ? std::vector<std::uint8_t>{} : descriptor(u"authentication", u"synthetic-refused")));
        REQUIRE(peer);
        auto observed = std::make_shared<Observation>();
        auto input = x::settings(peer->port(), std::make_shared<Provider>(observed));
        input.tls->authenticationTimeout = 2s;
        auto value = x::runtime(*bootstrap, input);
        REQUIRE(value.start());
        fails(value, silent ? t::TlsError::DEADLINE : t::TlsError::AUTHENTICATION_DENIED);
        CHECK(observed->calls == 1);
        CHECK(matchedReport(*peer).expectedBytesMatched);
    }
}

TEST_CASE("TLS invalid provider results fail without exposing or transmitting an authorization") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const auto mode : {ProviderMode::REJECTED, ProviderMode::THROWS, ProviderMode::EMPTY, ProviderMode::OVERSIZED,
                            ProviderMode::BAD_PREFIX}) {
        auto peer = h::TlsTestPeer::start(exchange());
        REQUIRE(peer);
        auto observed = std::make_shared<Observation>();
        auto value = x::runtime(*bootstrap, x::settings(peer->port(), std::make_shared<Provider>(observed, mode)));
        REQUIRE(value.start());
        fails(value, t::TlsError::CREDENTIAL_FAILURE);
        CHECK(observed->calls == 1);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        CHECK(peer->snapshot().bytesRead == 0);
        CHECK_FALSE(peer->snapshot().expectedBytesMatched);
    }
}

TEST_CASE("TLS authentication rejects insufficient work before invoking the provider and retries once") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto script = exchange();
    script.expectedClientBytes->insert(script.expectedClientBytes->begin(), {1, 0}); // Empty type0 heartbeat.
    auto peer = h::TlsTestPeer::start(std::move(script));
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    auto value = x::runtime(*bootstrap, x::settings(peer->port(), std::make_shared<Provider>(observed)));
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.authentication() == p::LiveAuthentication::REQUIRED;
    });
    CHECK(observed->calls == 0);

    for (int coordinate = 0; coordinate < 4; ++coordinate) {
        auto native = x::nativeTurn();
        auto tlsWork = x::tlsTurn(value.requirements());
        auto session = x::sessionTurn();

        switch (coordinate) {
        case 0:
            session.operations = 0;
            break;
        case 1:
            session.visits = 0;
            break;
        case 2:
            session.textUnits = 0;
            break;
        case 3:
            session.bytes = 0;
            break;
        }

        const auto original = session;
        const auto result = value.poll(native, tlsWork, session);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == t::TlsError::WORK_LIMIT);
        CHECK(result.error().stage == "tls-credential-work");
        CHECK(session.operations == original.operations);
        CHECK(session.visits == original.visits);
        CHECK(session.textUnits == original.textUnits);
        CHECK(session.bytes == original.bytes);
        CHECK(observed->calls == 0);
    }

    CHECK(observed->calls == 0);
    CHECK(value.authentication() == p::LiveAuthentication::REQUIRED);
    auto outputWork = x::sessionTurn();
    REQUIRE(value.composeHeartbeat(p::LiveHeartbeat{false}, outputWork));
    auto native = x::nativeTurn();
    auto tlsWork = x::tlsTurn(value.requirements());
    tlsWork.remaining.units.fill(0);
    auto session = x::sessionTurn();
    const auto occupied = value.poll(native, tlsWork, session);
    REQUIRE_FALSE(occupied);
    CHECK(occupied.error().error == t::TlsError::WORK_LIMIT);
    CHECK(observed->calls == 0);
    x::until(value, [&] {
        return value.authentication() == p::LiveAuthentication::ACKNOWLEDGED;
    });
    CHECK(observed->calls == 1);
    CHECK(matchedReport(*peer).expectedBytesMatched);
    x::close(value);
}

TEST_CASE("TLS provider reentry is rejected while requestClose remains an allocation-free cutoff") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(exchange());
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    std::optional<t::TlsRuntime> owner;
    auto provider = std::make_shared<Provider>(observed, ProviderMode::VALID, [&] {
        auto native = x::nativeTurn();
        auto tlsWork = x::tlsTurn(owner->requirements());
        auto work = x::sessionTurn();
        const auto nested = owner->poll(native, tlsWork, work);
        observed->reentryRejected = !nested && nested.error().error == t::TlsError::INVALID_STATE;
        const auto composed = owner->composeHeartbeat({}, work);
        observed->compositionRejected = !composed && composed.error().error == t::TlsError::INVALID_STATE;
        observed->closeAccepted = owner->requestClose().has_value();
    });
    owner.emplace(x::runtime(*bootstrap, x::settings(peer->port(), provider)));
    REQUIRE(owner->start());
    untilCallback(owner, *observed);
    REQUIRE(owner);
    x::until(*owner, [&] {
        return owner->state() == t::TlsState::CLOSED;
    });
    CHECK(observed->reentryRejected);
    CHECK(observed->compositionRejected);
    CHECK(observed->closeAccepted);
    CHECK(owner->terminal()->aborted);
    CHECK(owner->terminal()->acceptedPlaintextBytes == 0);
    CHECK(peer->snapshot().bytesRead == 0);
}

TEST_CASE("TLS provider may destroy the sole public runtime owner without authorizing its returned token") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(exchange());
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    std::optional<t::TlsRuntime> owner;
    std::shared_ptr<Provider> provider;
    provider = std::make_shared<Provider>(observed, ProviderMode::VALID, [&] {
        provider.reset();
        owner.reset();
        CHECK_FALSE(observed->destroyed);
    });
    owner.emplace(x::runtime(*bootstrap, x::settings(peer->port(), provider)));
    REQUIRE(owner->start());
    untilCallback(owner, *observed);
    CHECK_FALSE(owner);
    CHECK(observed->actionReturned);
    CHECK(observed->destroyed);
    CHECK(peer->stop().bytesRead == 0);
}

TEST_CASE("TLS provider replacement cuts off the old attempt and preserves the distinct prepared runtime") {
    static_assert(!std::is_copy_constructible_v<t::TlsRuntime>);
    static_assert(!std::is_copy_assignable_v<t::TlsRuntime>);
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto oldPeer = h::TlsTestPeer::start(exchange());
    auto newPeer = h::TlsTestPeer::start(exchange());
    REQUIRE(oldPeer);
    REQUIRE(newPeer);
    auto observed = std::make_shared<Observation>();
    auto replacementObserved = std::make_shared<Observation>();
    std::optional<t::TlsRuntime> owner;
    std::optional<t::TlsRuntime> replacement;
    replacement.emplace(
        x::runtime(*bootstrap, x::settings(newPeer->port(), std::make_shared<Provider>(replacementObserved))));
    auto provider = std::make_shared<Provider>(observed, ProviderMode::VALID, [&] {
        *owner = std::move(*replacement);
        replacement.reset();
    });
    owner.emplace(x::runtime(*bootstrap, x::settings(oldPeer->port(), provider)));
    REQUIRE(owner->start());
    untilCallback(owner, *observed);
    REQUIRE(owner);
    CHECK(owner->state() == t::TlsState::CONFIGURED);
    CHECK(owner->authentication() == p::LiveAuthentication::UNCHALLENGED);
    CHECK(oldPeer->stop().bytesRead == 0);
    CHECK(replacementObserved->calls == 0);
    REQUIRE(owner->start());
    x::until(*owner, [&] {
        return owner->authentication() == p::LiveAuthentication::ACKNOWLEDGED;
    });
    CHECK(replacementObserved->calls == 1);
    CHECK(matchedReport(*newPeer).expectedBytesMatched);
    x::close(*owner);
}

TEST_CASE("TLS provider move construction and self move preserve the original live attempt") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool destroyMoved : {false, true}) {
        auto peer = h::TlsTestPeer::start(exchange());
        REQUIRE(peer);
        auto observed = std::make_shared<Observation>();
        std::optional<t::TlsRuntime> owner;
        std::optional<t::TlsRuntime> moved;
        auto provider = std::make_shared<Provider>(observed, ProviderMode::VALID, [&] {
            *owner = std::move(*owner);
            moved.emplace(std::move(*owner));
            owner.reset();

            if (destroyMoved) {
                moved.reset();
            }
        });
        owner.emplace(x::runtime(*bootstrap, x::settings(peer->port(), provider)));
        REQUIRE(owner->start());
        untilCallback(owner, *observed);
        CHECK_FALSE(owner);

        if (destroyMoved) {
            CHECK_FALSE(moved);
            CHECK(peer->stop().bytesRead == 0);
        } else {
            REQUIRE(moved);
            x::until(*moved, [&] {
                return moved->authentication() == p::LiveAuthentication::ACKNOWLEDGED;
            });
            CHECK(matchedReport(*peer).expectedBytesMatched);
            x::close(*moved);
        }

        CHECK(observed->calls == 1);
    }
}

TEST_CASE("TLS provider may release external settings snapshot and provider handles during its callback") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(exchange());
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    std::optional<d::EndpointOptions> endpoint;
    std::shared_ptr<Provider> provider;
    d::EndpointSettings input;
    provider = std::make_shared<Provider>(observed, ProviderMode::VALID, [&] {
        endpoint.reset();
        input.tls.reset();
        provider.reset();
        CHECK_FALSE(observed->destroyed);
    });
    input = x::settings(peer->port(), provider);
    auto local = x::schema();
    auto snapshot = d::EndpointOptions::create(local, input);
    REQUIRE(snapshot);
    endpoint.emplace(std::move(*snapshot));
    auto composer = x::writer(local, input.budgets);
    const auto config = x::options();
    const auto requirements = t::tlsRequirements(config.limits, config.host);
    REQUIRE(requirements);
    auto tlsWork = x::tlsTurn(*requirements);
    auto work = x::sessionTurn();
    auto prepared = t::TlsRuntime::create(*bootstrap, *endpoint, std::move(composer), config, tlsWork, work);
    REQUIRE(prepared);
    std::optional<t::TlsRuntime> owner(std::move(*prepared));
    REQUIRE(owner->start());
    x::until(*owner, [&] {
        return owner->authentication() == p::LiveAuthentication::ACKNOWLEDGED;
    });
    CHECK_FALSE(endpoint);
    CHECK_FALSE(input.tls);
    CHECK_FALSE(provider);
    CHECK_FALSE(observed->destroyed);
    CHECK(observed->calls == 1);
    CHECK(matchedReport(*peer).expectedBytesMatched);
    x::close(*owner);
    owner.reset();
    CHECK(observed->destroyed);
}

TEST_CASE("TLS provider is never invoked before start after configured abort or failed identity verification") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool wrongIdentity : {false, true}) {
        auto peer = h::TlsTestPeer::start(exchange());
        REQUIRE(peer);
        auto observed = std::make_shared<Observation>();
        auto input = x::settings(peer->port(), std::make_shared<Provider>(observed));

        if (wrongIdentity) {
            input.tls->referenceIdentity = "wrong.invalid";
        }

        auto value = x::runtime(*bootstrap, input);
        CHECK(observed->calls == 0);

        if (wrongIdentity) {
            REQUIRE(value.start());
            fails(value, t::TlsError::IDENTITY_FAILURE);
        } else {
            x::close(value);
            CHECK_FALSE(peer->snapshot().accepted);
        }

        CHECK(observed->calls == 0);
        CHECK(value.terminal()->acceptedPlaintextBytes == 0);
        CHECK(peer->snapshot().bytesRead == 0);
    }
}

TEST_CASE("TLS authentication admits the matched STREAM subscription only after its peer ACK") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto script = exchange();
    script.initialBytes = descriptor(u"authentication", u"LOGIN synthetic-realm", true);
    script.replyBytes = descriptor(u"authentication", u"", true);
    auto peer = h::TlsTestPeer::start(std::move(script));
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    auto selected = selectedRuntime(*bootstrap, x::settings(peer->port(), std::make_shared<Provider>(observed)));
    auto &value = selected.value;
    const c::SubscriptionRequest request{{selected.quote, u"IBM"}, {}};
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.authentication() == p::LiveAuthentication::REQUIRED;
    });
    auto work = x::sessionTurn();
    const auto blocked = value.subscribe(p::SubscriptionMessage::STREAM_ADD, std::span(&request, 1), work);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().transport.protocolError == p::LiveError::AUTHENTICATION_REQUIRED);
    CHECK(observed->calls == 0);
    x::until(value, [&] {
        return value.authentication() == p::LiveAuthentication::ACKNOWLEDGED;
    });
    REQUIRE(value.state() == t::TlsState::CONNECTED);
    CHECK(observed->calls == 1);
    const auto report = matchedReport(*peer);
    REQUIRE(report.expectedBytesMatched);
    CHECK(report.bytesRead == descriptor(u"authorization", TEST_TOKEN).size());
    work = x::sessionTurn();
    REQUIRE(value.subscribe(p::SubscriptionMessage::STREAM_ADD, std::span(&request, 1), work));
    retireAfterSubscription(value, report.bytesRead);
    // The child's script compares authorization only. No subscription receipt/ACK is inferred.
    CHECK(peer->snapshot().bytesRead == report.bytesRead);
}

TEST_CASE("TLS empty authentication ACK without a challenge enables readiness without a provider callback") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto script = x::peerOptions();
    script.initialBytes = descriptor(u"authentication", u"", true);
    auto peer = h::TlsTestPeer::start(std::move(script));
    REQUIRE(peer);
    auto observed = std::make_shared<Observation>();
    auto selected = selectedRuntime(*bootstrap, x::settings(peer->port(), std::make_shared<Provider>(observed)));
    auto &value = selected.value;
    REQUIRE(value.start());
    x::until(value, [&] {
        return value.authentication() == p::LiveAuthentication::ACKNOWLEDGED;
    });
    CHECK(value.state() == t::TlsState::CONNECTED);
    CHECK(observed->calls == 0);
    const c::SubscriptionRequest request{{selected.quote, u"IBM"}, {}};
    auto work = x::sessionTurn();
    REQUIRE(value.subscribe(p::SubscriptionMessage::STREAM_ADD, std::span(&request, 1), work));
    retireAfterSubscription(value, 0);
    CHECK(observed->calls == 0);
    CHECK(peer->snapshot().bytesRead == 0);
}

TEST_CASE("TLS fresh attempts call the retained provider once each and never reuse an earlier token") {
    constexpr std::size_t ATTEMPTS = 64; ///< Exact planned finite successful fresh-run churn workload.
    constexpr auto CHURN_DEADLINE = 90s; ///< Whole bounded harness limit, independent of SDK wall-time promises.
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto observed = std::make_shared<Observation>();
    auto provider = std::make_shared<Provider>(observed);
    std::size_t completed{};
    const auto deadline = std::chrono::steady_clock::now() + CHURN_DEADLINE;
    std::u16string previous;

    for (std::size_t attempt = 0; attempt < ATTEMPTS; ++attempt) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        const auto token = attemptToken(attempt);
        const bool distinctToken = token != previous;
        CHECK(distinctToken);
        previous = token;
        provider->setToken(token);
        auto script = exchange();
        script.expectedClientBytes = descriptor(u"authorization", token);
        auto peer = h::TlsTestPeer::start(std::move(script));
        REQUIRE(peer);
        {
            auto value = x::runtime(*bootstrap, x::settings(peer->port(), provider));
            CHECK(observed->calls == attempt);
            REQUIRE(value.start());
            x::until(value, [&] {
                return value.authentication() == p::LiveAuthentication::ACKNOWLEDGED;
            });
            CHECK(value.state() == t::TlsState::CONNECTED);
            CHECK(observed->calls == attempt + 1);
            const auto report = matchedReport(*peer);
            REQUIRE(report.expectedBytesMatched);
            CHECK(report.bytesRead == descriptor(u"authorization", token).size());
            x::close(value);
            CHECK(value.resources().transport.pendingOperations == 0);
        }
        const auto stopped = peer->stop();
        INFO("fresh attempt=" << attempt << " completed=" << stopped.completed << " timedOut=" << stopped.timedOut
                              << " reportReadable=" << stopped.reportReadable
                              << " childRunning=" << stopped.childRunning << " bytesRead=" << stopped.bytesRead
                              << " bytesWritten=" << stopped.bytesWritten << " childReportedFailure="
                              << stopped.childReportedFailure << " exitObserved=" << stopped.exitObserved
                              << " exitCode=" << stopped.exitCode << " exitQueryError=" << stopped.exitQueryError
                              << " parentTerminationRequested=" << stopped.parentTerminationRequested);
        CHECK_FALSE(stopped.childRunning);
        CHECK_FALSE(stopped.failed);
        ++completed;
    }

    CHECK(completed == ATTEMPTS);
    CHECK(observed->calls == ATTEMPTS);
    CHECK_FALSE(observed->destroyed);
    provider.reset();
    CHECK(observed->destroyed);
}
