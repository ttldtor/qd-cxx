// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public TLS recovery with finite caller work and isolated loopback peers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_test_support.hpp"
#include "transport/tls_test_support.hpp"

#include "qd/transport/tls_recovery.hpp"

#include <functional>
#include <iostream>
#include <optional>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace d = root::client;
namespace m = root::mux;
namespace t = root::transport;
namespace h = root::test;
namespace r = root::test::recovery;
namespace x = root::test::tls;
using namespace std::chrono_literals;

constexpr std::u16string_view TEST_TOKEN = u"Basic cWRjeHg6dGVzdA=="; ///< Public synthetic credential only.

/** Convert independent primitive-codec fixture bytes to the existing process-peer script format.
 * @param source Complete owned fixture frame, borrowed only during copying.
 * @return Independently owned exact wire bytes.
 */
std::vector<std::uint8_t> wire(std::span<const std::byte> source) {
    std::vector<std::uint8_t> result;
    result.reserve(source.size());

    for (auto byte : source) {
        result.push_back(std::to_integer<std::uint8_t>(byte));
    }

    return result;
}

/** Build one source-defined DXP3 property descriptor without the production control composer.
 * @param key Exact synthetic property name.
 * @param value Exact synthetic property value; never printed on failure.
 * @return One complete independently framed descriptor.
 */
std::vector<std::uint8_t> property(std::u16string_view key, std::u16string_view value) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    r::number(payload, 1);
    r::text(payload, key);
    r::text(payload, value);
    r::number(payload, 0);
    r::number(payload, 0);

    return wire(r::frame(1, payload));
}

/** Encode the selected STREAM client's protocol independently of its control composer.
 * @param stream Whether one STREAM source is adopted.
 * @return Exact initial protocol frame, containing no authorization property.
 */
std::vector<std::uint8_t> clientProtocol(bool stream = true) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    r::number(payload, 1);
    r::text(payload, u"opt");
    r::text(payload, u"hs");
    r::number(payload, stream ? 5 : 3);
    constexpr std::array<std::int32_t, 5> send{0, 1, 2, 16, 17};
    constexpr std::array<std::u16string_view, 5> sendNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                           u"STREAM_ADD_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION"};

    for (std::size_t i = 0; i != (stream ? send.size() : 3); ++i) {
        r::number(payload, send[i]);
        r::text(payload, sendNames[i]);
        r::number(payload, 0);
    }

    r::number(payload, stream ? 4 : 3);
    constexpr std::array<std::int32_t, 4> receive{0, 1, 2, 15};
    constexpr std::array<std::u16string_view, 4> receiveNames{u"HEARTBEAT", u"DESCRIBE_PROTOCOL", u"DESCRIBE_RECORDS",
                                                              u"STREAM_DATA"};

    for (std::size_t i = 0; i != (stream ? receive.size() : 3); ++i) {
        r::number(payload, receive[i]);
        r::text(payload, receiveNames[i]);
        r::number(payload, 0);
    }

    return wire(r::frame(1, payload));
}

/** Append a complete owned byte sequence.
 * @param target Destination bytes.
 * @param source Exact additional envelope bytes.
 */
void append(std::vector<std::uint8_t> &target, const std::vector<std::uint8_t> &source) {
    target.insert(target.end(), source.begin(), source.end());
}

/** Encode one raw-symbol STREAM ADD independently of the production writer.
 * @param symbol One ASCII synthetic symbol.
 * @return Complete type16 envelope for client sender71.
 */
std::vector<std::uint8_t> add(std::u16string_view symbol) {
    std::vector<std::byte> payload{std::byte{0xfd}};
    r::number(payload, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        REQUIRE(unit < 128);
        payload.push_back(static_cast<std::byte>(unit));
    }

    r::number(payload, 71);

    return wire(r::frame(16, payload));
}

/** Declare the controlled executable's finite same-image SDK scope once for a recovery run.
 * All parent SDK users are serialized recovery owners; the peer has a separate process image.
 * No prior failed initialization is induced. The three/four failed-prefix reserves cover one
 * possible terminal factory failure for the whole run, rather than being multiplied by attempts.
 * @return Explicit host-qualified resource allowances, never observed heap usage or product defaults.
 */
t::TlsOptions tlsOptions() {
    auto result = x::options();
    result.host.failedPrivatePrefixes = 3;
    result.host.failedAmbientPrefixes = 4;

    return result;
}

/** Prepare a public owner while retiring all caller-side copies of the credential provider.
 * @param bootstrap Process state outliving the owner and every returning poll.
 * @param port Owned isolated loopback peer port.
 * @param tls Explicit unchanged qualification used on every subsequent poll.
 * @param options Explicit recovery policy, including any inactivity deadline.
 * @param provider Optional callback owner, transferred into the immutable endpoint snapshot.
 * @return Unactivated move-only public owner; no native/SDK/provider activity has occurred.
 */
t::TlsRecovery recovery(t::RuntimeBootstrap &bootstrap, std::uint16_t port, const t::TlsOptions &tls,
                        const t::RecoveryOptions &options, std::shared_ptr<d::CredentialProvider> provider = {}) {
    auto local = r::schema();
    auto input = x::settings(port, std::move(provider));
    input.retry = {3, 5s};
    auto endpoint = r::endpoint(local, input);
    auto blueprint = r::blueprint();
    auto preparation = r::preparation();
    t::RecoveryQueues queues;
    auto result =
        t::TlsRecovery::create(bootstrap, endpoint, std::move(blueprint), std::move(queues), options, tls, preparation);
    REQUIRE(result);

    return std::move(*result);
}

/** Spend one fresh finite common/TLS caller turn without retaining either work reference.
 * @param owner Current live public owner on its creating thread.
 * @param requirements Immutable qualification recomputed from the explicit test settings.
 * @return Detached actual progress state, including typed terminal evidence.
 */
t::TlsRecoveryStatus poll(t::TlsRecovery &owner, const t::TlsRequirements &requirements) {
    auto work = r::turn();
    auto tlsWork = x::tlsTurn(requirements);
    auto result = owner.poll(work, tlsWork);
    REQUIRE(result);

    return *result;
}

/** Request local cutoff and actually drain with independent fresh caller credits.
 * @param owner Public owner retained through each operation.
 * @param requirements Explicit fixed qualification for every turn.
 */
void stop(t::TlsRecovery &owner, const t::TlsRequirements &requirements) {
    REQUIRE(owner.requestStop());
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (owner.status().recovery.phase != t::RecoveryPhase::CLOSED && std::chrono::steady_clock::now() < deadline) {
        (void)poll(owner, requirements);
        std::this_thread::sleep_for(1ms);
    }

    CHECK(owner.status().recovery.phase == t::RecoveryPhase::CLOSED);
}

/** Callback observations outlive both the provider and the public recovery handle. */
struct Observation {
    std::size_t calls = 0;       ///< Actual serialized provider invocations.
    bool destroyed = false;      ///< The final provider owner retired.
    bool actionReturned = false; ///< Callback-side cutoff completed before the result was returned.
};

/** Bounded provider supporting a terminal refusal or a callback-side public-owner cutoff. */
class Provider final : public d::CredentialProvider {
    std::shared_ptr<Observation> observed_; ///< Independent test-owned observations.
    std::function<void()> action_;          ///< Optional serialized owner destruction, never an SDK hook.

    public:
    /** Prepare a provider without invoking it.
     * @param observed Independent observations, retained through callback return.
     * @param action Optional owner-thread action; absent means explicit credential rejection.
     */
    Provider(std::shared_ptr<Observation> observed, std::function<void()> action = {})
        : observed_(std::move(observed)), action_(std::move(action)) {
    }

    /** Record final retirement without touching the recovery owner. */
    ~Provider() override {
        observed_->destroyed = true;
    }

    /** Invoke the optional cutoff before returning a valid synthetic token or static refusal.
     * @param destination Borrowed finite staging; no pointer or reference escapes this call.
     * @return Written synthetic units after cutoff, otherwise explicit terminal rejection.
     */
    std::expected<std::size_t, d::CredentialError> basicToken(std::span<char16_t> destination) override {
        ++observed_->calls;

        if (!action_) {
            return std::unexpected(d::CredentialError::REJECTED);
        }

        action_();
        observed_->actionReturned = true;
        REQUIRE(destination.size() >= TEST_TOKEN.size());
        std::ranges::copy(TEST_TOKEN, destination.begin());

        return TEST_TOKEN.size();
    }
};

/** Create a finite real STREAM source with one-record callback batches.
 * @param local Exact immutable schema provenance shared with the TLS session.
 * @return Unbound queue; no source event or callback has been fabricated.
 */
m::MuxQueue stream(const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto queue = m::MuxQueue::createStream(
        local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024}, {16, 32, 128, 4096, 64, 8192, 64},
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024}, work);
    REQUIRE(queue);

    return std::move(*queue);
}

/** Retain detached callback data, then cut off the public handle executing the current TLS poll. */
struct StreamConsumer final : m::MuxConsumer {
    std::optional<t::TlsRecovery> &owner; ///< External sole public handle; no ownership cycle is created.
    m::OwnedMuxBatch retained;            ///< Application-owned data surviving old runtime/queue destruction.
    m::QueueIdentity deliveredEpoch;      ///< Exact valid epoch captured before public owner destruction.
    std::size_t callbacks = 0;            ///< Actual invocations; each supplied batch has at most one record.
    bool ownerPresent = false;            ///< Callback began with the actual public owner still present.
    bool stopAccepted = false;            ///< Callback-side request-only cutoff was accepted.

    /** Bind the creating-thread public handle without taking ownership.
     * @param current Public owner whose in-flight poll may dispatch this consumer.
     */
    explicit StreamConsumer(std::optional<t::TlsRecovery> &current) noexcept : owner(current) {
    }

    /** Retain only an owning batch, then request cutoff and destroy the public handle.
     * Assertions stay outside this production consumer exception boundary.
     * @param batch Callback-only borrow; no view or pointer escapes.
     * @return Continue acknowledges this batch; cutoff must prevent a later callback.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        ++callbacks;

        if (callbacks != 1) {
            return m::BatchAction::CONTINUE;
        }

        retained = batch.retain();
        deliveredEpoch = retained.epoch();
        ownerPresent = owner.has_value();

        if (ownerPresent) {
            stopAccepted = owner->requestStop().has_value();
            owner.reset();
        }

        return m::BatchAction::CONTINUE;
    }
};

/** Register one finite callback domain and submit ordinary desired STREAM state before adoption.
 * @param queue Actual unbound source that recovery will adopt.
 * @param consumer Externally owned consumer retaining detached output only.
 * @param local Exact subscription provenance shared with the incoming record binding.
 * @return Producer-safe public handle surviving source and recovery destruction.
 */
m::MuxHandle subscribe(m::MuxQueue &queue, const std::shared_ptr<m::MuxConsumer> &consumer,
                       const c::LocalSchema &local) {
    auto work = r::queueWork();
    auto handle =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, consumer, work);
    REQUIRE(handle);
    const std::array<c::SubscriptionRequest, 1> desired{{{{local.recordContext(0), u"A"}, {}}}};
    m::QueueSubmitWork submission{10000, 1000000, 1000000, 1000000};
    const auto accepted = handle->replace(desired, submission);
    REQUIRE(accepted);

    return std::move(*handle);
}

/** Preserve admitted TLS payload while requesting local drain from its first callback. */
struct DrainConsumer final : m::MuxConsumer {
    std::optional<t::TlsRecovery> &owner;   ///< External owner; callback creates no ownership cycle.
    std::vector<m::OwnedMuxBatch> retained; ///< Real payloads readable after native disposal.
    std::vector<t::RecoveryPhase> phases;   ///< Callback-entry scheduler observations.
    bool accepted = false;                  ///< Allocation-free request acceptance, asserted after callback return.
    bool expired = false;                   ///< Select explicit inclusive past-deadline abort.

    /** Bind the sole creating-thread handle.
     * @param current Live owner retained by its actual poll operation.
     * @param past Request an already expired drain deadline.
     */
    DrainConsumer(std::optional<t::TlsRecovery> &current, bool past) noexcept : owner(current), expired(past) {
    }

    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        phases.push_back(owner->status().recovery.phase);
        retained.push_back(batch.retain());

        if (retained.size() == 1) {
            accepted = owner
                           ->requestDrain(expired ? std::chrono::steady_clock::time_point::min()
                                                  : std::chrono::steady_clock::now() + 3s)
                           .has_value();
        }

        return m::BatchAction::CONTINUE;
    }
};
} // namespace

TEST_CASE("TLS recovery local drain delivers only admitted payload and freezes shutdown evidence") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool expired : {false, true}) {
        auto bytes = r::ready();
        r::append(bytes, r::description(41));
        std::vector<std::byte> payload;

        for (const auto value : {101, 102}) {
            payload.push_back(std::byte{0xfd});
            r::number(payload, 1);
            payload.push_back(std::byte{'A'});
            r::number(payload, 41);
            r::number(payload, 0);
            r::number(payload, 1);
            r::number(payload, value);
        }

        r::append(bytes, r::frame(15, payload));
        auto script = x::peerOptions(h::PeerTlsVersion::TLS_1_3);
        script.initialBytes = wire(bytes);
        auto peer = h::TlsTestPeer::start(std::move(script));
        REQUIRE(peer);
        const auto tls = tlsOptions();
        const auto requirements = t::tlsRequirements(tls.limits, tls.host);
        REQUIRE(requirements);
        auto local = r::schema();
        std::optional<t::TlsRecovery> owner;
        auto consumer = std::make_shared<DrainConsumer>(owner, expired);
        auto queue = stream(local);
        auto handle = subscribe(queue, consumer, local);
        t::RecoveryQueues queues;
        queues.stream = std::move(queue);
        auto input = x::settings(peer->port());
        input.retry = {3, 5s};
        auto endpoint = r::endpoint(local, input);
        auto preparation = r::preparation();
        auto created = t::TlsRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(),
                                              tls, preparation);
        REQUIRE(created);
        owner.emplace(std::move(*created));
        REQUIRE(owner->start());
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto status = owner->status();

        while (!status.recovery.terminal && std::chrono::steady_clock::now() < deadline) {
            status = poll(*owner, *requirements);
            std::this_thread::sleep_for(1ms);
        }

        INFO("phase=", static_cast<int>(status.recovery.phase), " recovery stage=", status.recovery.first.stage,
             " TLS stage=", status.first.stage);
        REQUIRE(status.recovery.terminal);
        CHECK(consumer->accepted);
        CHECK(status.recovery.phase == t::RecoveryPhase::CLOSED);
        CHECK(status.recovery.terminal->outcome ==
              (expired ? m::TerminalOutcome::ABORTED : m::TerminalOutcome::DRAINED));
        CHECK(status.recovery.terminal->deadlineExpired == expired);
        REQUIRE(status.recovery.terminal->queues[1]);
        REQUIRE(status.recovery.terminal->wire);
        const auto &wire = *status.recovery.terminal->wire;

        if (wire.primary.error != t::TcpError::NONE) {
            CHECK(status.recovery.first.transport.error == wire.primary.error);
            CHECK(status.recovery.first.transport.nativeCode == wire.primary.nativeCode);
            CHECK(status.recovery.first.transport.stage == wire.primary.stage);
        }

        std::cout << "TLS_DRAIN_WIRE\t" << expired << '\t' << static_cast<int>(wire.primary.error) << '\t'
                  << wire.primary.nativeCode << '\t' << wire.primary.stage << '\n';
        CHECK(status.recovery.terminal->queues[1]->counts.acceptedRecords == 2);
        REQUIRE(consumer->retained.size() == (expired ? 1U : 2U));

        if (!expired) {
            REQUIRE(consumer->phases.size() == 2);
            CHECK(consumer->phases[1] == t::RecoveryPhase::STOPPING);
        }

        const auto terminal = *status.recovery.terminal;
        REQUIRE(owner->requestStop());
        REQUIRE(owner->requestDrain(std::chrono::steady_clock::time_point::min()));
        const auto repeated = poll(*owner, *requirements);
        REQUIRE(repeated.recovery.terminal);
        CHECK(repeated.recovery.terminal->outcome == terminal.outcome);
        CHECK(repeated.recovery.terminal->deadlineExpired == terminal.deadlineExpired);
        owner.reset();

        for (std::size_t index = 0; index != consumer->retained.size(); ++index) {
            const auto records = consumer->retained[index].records();
            REQUIRE(records.size() == 1);
            REQUIRE(records[0].typedValues.size() == 3);
            CHECK(records[0].typedValues[2].raw == 101 + static_cast<std::int64_t>(index));
        }

        REQUIRE(handle.terminal());
        CHECK(terminal.queues[1]->epoch.matches(handle.terminal()->epoch));
        const auto report = peer->stop();
        CHECK(report.handshakeComplete);
    }
}

TEST_CASE("TLS recovery reaches actual protocol ACTIVE with empty demand using TLS 1.2 and TLS 1.3") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (auto version : {h::PeerTlsVersion::TLS_1_2, h::PeerTlsVersion::TLS_1_3}) {
        CAPTURE(static_cast<unsigned>(version));
        auto script = x::peerOptions(version);
        script.initialBytes = wire(r::ready());
        auto peer = h::TlsTestPeer::start(std::move(script));
        REQUIRE(peer);
        const auto tls = tlsOptions();
        const auto requirements = t::tlsRequirements(tls.limits, tls.host);
        REQUIRE(requirements);
        auto owner = recovery(*bootstrap, peer->port(), tls, r::options());
        CHECK(owner.status().recovery.phase == t::RecoveryPhase::CONFIGURED);
        CHECK(owner.status().recovery.attempts == 0);
        REQUIRE(owner.start());
        CHECK(owner.status().recovery.attempts == 0);
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto status = owner.status();

        while (status.recovery.phase != t::RecoveryPhase::ACTIVE && status.recovery.phase != t::RecoveryPhase::FAILED &&
               std::chrono::steady_clock::now() < deadline) {
            status = poll(owner, *requirements);
            std::this_thread::sleep_for(1ms);
        }

        INFO("phase=", static_cast<int>(status.recovery.phase), " stage=", status.recovery.first.stage,
             " tls stage=", status.first.stage);
        CHECK(status.recovery.phase == t::RecoveryPhase::ACTIVE);
        CHECK(status.recovery.attempts == 1);
        CHECK(status.recovery.demandedFamilies == 0);
        CHECK(status.recovery.first.error == t::RecoveryError::NONE);
        stop(owner, *requirements);
        CHECK(owner.status().recovery.attempts == 1);
        const auto report = peer->stop();
        CHECK(report.handshakeComplete);
        CHECK(report.tlsVersion == static_cast<std::uint32_t>(version));
        CHECK(report.sniLocalhost);
        CHECK(report.bytesWritten == r::ready().size());
    }
}

TEST_CASE("TLS recovery cannot activate empty demand from a verified handshake without current protocol input") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto peer = h::TlsTestPeer::start(x::peerOptions());
    REQUIRE(peer);
    const auto tls = tlsOptions();
    const auto requirements = t::tlsRequirements(tls.limits, tls.host);
    REQUIRE(requirements);
    auto options = r::options();
    options.protocolInactivity = 80ms;
    auto owner = recovery(*bootstrap, peer->port(), tls, options);
    REQUIRE(owner.start());
    bool active = false;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    auto status = owner.status();

    while (status.recovery.phase != t::RecoveryPhase::BACKOFF && status.recovery.phase != t::RecoveryPhase::FAILED &&
           std::chrono::steady_clock::now() < deadline) {
        status = poll(owner, *requirements);
        active = active || status.recovery.phase == t::RecoveryPhase::ACTIVE;
        std::this_thread::sleep_for(1ms);
    }

    INFO("phase=", static_cast<int>(status.recovery.phase), " stage=", status.recovery.first.stage,
         " tls stage=", status.first.stage);
    CHECK_FALSE(active);
    CHECK(status.recovery.phase == t::RecoveryPhase::BACKOFF);
    CHECK(status.recovery.first.stage == "recovery-protocol-inactivity");
    CHECK(status.recovery.attempts == 1);
    stop(owner, *requirements);
    CHECK(owner.status().recovery.attempts == 1);
    CHECK(peer->stop().handshakeComplete);
}

TEST_CASE("TLS recovery credential callback refusal destruction and drain cannot authorize a successor") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    enum class ProviderAction { REFUSE, DESTROY, DRAIN };

    for (const auto selected : {ProviderAction::REFUSE, ProviderAction::DESTROY, ProviderAction::DRAIN}) {
        CAPTURE(static_cast<unsigned>(selected));
        const bool destroyOwner = selected == ProviderAction::DESTROY;
        const bool drainOwner = selected == ProviderAction::DRAIN;
        auto script = x::peerOptions();
        script.initialBytes = property(u"authentication", u"LOGIN synthetic-realm");
        script.captureUntilClose = true;
        auto peer = h::TlsTestPeer::start(std::move(script));
        REQUIRE(peer);
        const auto tls = tlsOptions();
        const auto requirements = t::tlsRequirements(tls.limits, tls.host);
        REQUIRE(requirements);
        auto observed = std::make_shared<Observation>();
        std::optional<t::TlsRecovery> owner;
        std::shared_ptr<Provider> provider;
        std::function<void()> action;
        bool drainAccepted = false;

        if (destroyOwner) {
            action = [&] {
                provider.reset();
                owner.reset();
                CHECK_FALSE(observed->destroyed);
            };
        } else if (drainOwner) {
            action = [&] {
                drainAccepted = owner->requestDrain(std::chrono::steady_clock::now() + 3s).has_value();
            };
        }

        provider = std::make_shared<Provider>(observed, std::move(action));
        owner.emplace(recovery(*bootstrap, peer->port(), tls, r::options(), provider));
        REQUIRE(owner->start());
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        t::TlsRecoveryStatus returned;

        while (owner && owner->status().recovery.phase != t::RecoveryPhase::FAILED &&
               owner->status().recovery.phase != t::RecoveryPhase::CLOSED &&
               std::chrono::steady_clock::now() < deadline) {
            // The callback may destroy the public handle; only the detached return is used afterwards.
            returned = poll(*owner, *requirements);
            std::this_thread::sleep_for(1ms);
        }

        CHECK(observed->calls == 1);
        CHECK(returned.recovery.attempts == 1);
        CHECK(returned.recovery.phase != t::RecoveryPhase::ACTIVE);

        if (destroyOwner) {
            CHECK_FALSE(owner);
            CHECK(observed->actionReturned);
            CHECK(observed->destroyed);
            CHECK(returned.recovery.stopped);
        } else if (drainOwner) {
            REQUIRE(owner);
            CHECK(drainAccepted);
            CHECK(observed->actionReturned);
            CHECK(returned.recovery.phase == t::RecoveryPhase::CLOSED);
            REQUIRE(returned.recovery.terminal);
            CHECK(returned.recovery.terminal->outcome == m::TerminalOutcome::DRAINED);
            CHECK(returned.recovery.stopped);
        } else {
            REQUIRE(owner);
            CHECK(returned.recovery.phase == t::RecoveryPhase::FAILED);
            CHECK(returned.recovery.first.error == t::RecoveryError::AUTHENTICATION);
            CHECK(returned.first.error == t::TlsError::CREDENTIAL_FAILURE);
            const auto terminal = poll(*owner, *requirements);
            CHECK(terminal.recovery.phase == t::RecoveryPhase::FAILED);
            CHECK(terminal.recovery.attempts == 1);
            CHECK(observed->calls == 1);
        }

        owner.reset();
        const auto peerDeadline = std::chrono::steady_clock::now() + 3s;

        while (peer->snapshot().childRunning && std::chrono::steady_clock::now() < peerDeadline) {
            std::this_thread::sleep_for(1ms);
        }

        REQUIRE(peer->snapshot().completed);
        REQUIRE_FALSE(peer->snapshot().childRunning);
        REQUIRE_FALSE(peer->snapshot().failed);
        const auto report = peer->stop();
        CHECK(report.handshakeComplete);
        REQUIRE(report.completedSessions == 1);
        REQUIRE(report.firstCaptureEnd != 0);
        CHECK_FALSE(report.failed);
        auto permitted = clientProtocol(false);
        append(permitted, wire(r::description(71)));
        REQUIRE(report.firstClientBytes.size() <= permitted.size());
        CHECK(std::equal(report.firstClientBytes.begin(), report.firstClientBytes.end(), permitted.begin()));
        std::cout << (drainOwner ? "TLS_RECOVERY_DRAIN_PROVIDER_CAPTURE\t" : "TLS_RECOVERY_CUTOFF_CAPTURE\t")
                  << destroyOwner << '\t' << report.firstCaptureEnd << '\t' << report.firstClientBytes.size() << '\n';
    }
}

TEST_CASE("TLS recovery consumer cutoff preserves retained STREAM data with two back-to-back DATA frames") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto bytes = r::ready();
    r::append(bytes, r::description(41));
    r::append(bytes, r::data(41, u"A", 101));
    r::append(bytes, r::data(41, u"A", 102));
    auto script = x::peerOptions(h::PeerTlsVersion::TLS_1_3);
    script.initialBytes = wire(bytes);
    auto peer = h::TlsTestPeer::start(std::move(script));
    REQUIRE(peer);
    const auto tls = tlsOptions();
    const auto requirements = t::tlsRequirements(tls.limits, tls.host);
    REQUIRE(requirements);
    auto local = r::schema();
    std::optional<t::TlsRecovery> owner;
    auto consumer = std::make_shared<StreamConsumer>(owner);
    auto queue = stream(local);
    auto handle = subscribe(queue, consumer, local);
    t::RecoveryQueues queues;
    queues.stream = std::move(queue);
    auto input = x::settings(peer->port());
    input.retry = {3, 5s};
    auto endpoint = r::endpoint(local, input);
    auto preparation = r::preparation();
    auto created =
        t::TlsRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), r::options(), tls, preparation);
    REQUIRE(created);
    owner.emplace(std::move(*created));
    REQUIRE(owner->start());
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    t::TlsRecoveryStatus returned;

    while (owner && consumer->callbacks == 0 && std::chrono::steady_clock::now() < deadline) {
        // The consumer may destroy the public handle; only detached old-operation state is used afterwards.
        returned = poll(*owner, *requirements);

        if (returned.recovery.phase == t::RecoveryPhase::FAILED ||
            returned.recovery.phase == t::RecoveryPhase::EXHAUSTED) {
            break;
        }

        std::this_thread::sleep_for(1ms);
    }

    INFO("phase=", static_cast<int>(returned.recovery.phase), " attempts=", returned.recovery.attempts,
         " first stage=", returned.recovery.first.stage,
         " protocolError=", static_cast<int>(returned.recovery.first.transport.protocolError),
         " tls stage=", returned.first.stage);
    REQUIRE(consumer->callbacks == 1);
    CHECK(consumer->ownerPresent);
    CHECK(consumer->stopAccepted);
    CHECK_FALSE(owner);
    CHECK(returned.recovery.stopped);
    CHECK(returned.recovery.attempts == 1);
    const auto terminal = handle.terminal();
    REQUIRE(terminal);
    CHECK(terminal->counts.callbackStartedBatches == 1);
    CHECK(terminal->counts.callbackCompletedBatches == 1);
    handle = {};
    const auto report = peer->stop();
    CHECK(report.handshakeComplete);
    CHECK(report.tlsVersion == static_cast<std::uint32_t>(h::PeerTlsVersion::TLS_1_3));
    CHECK(report.bytesWritten == bytes.size());
    CHECK(consumer->callbacks == 1);
    const auto records = consumer->retained.records();
    REQUIRE(records.size() == 1);
    CHECK(records[0].symbol == u"A");
    REQUIRE(records[0].typedValues.size() == 3);
    CHECK(records[0].typedValues[2].raw == 101);
    REQUIRE(records[0].source);
    CHECK(records[0].source->senderId == 41);
    CHECK(consumer->retained.epoch().matches(consumer->deliveredEpoch));
}

TEST_CASE("TLS recovery performs a fresh successor handshake authentication and latest STREAM replay") {
    struct Consumer final : m::MuxConsumer {
        /** Ignore synthetic traffic; this test observes actual outgoing subscription bytes.
         * @param batch Callback-only borrowed batch.
         * @return Ordinary acknowledged callback completion.
         */
        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            (void)batch;

            return m::BatchAction::CONTINUE;
        }
    };

    struct Credentials final : d::CredentialProvider {
        unsigned calls{}; ///< Actual verified-TLS challenges in distinct attempts.

        /** Supply the same public synthetic credential separately for each fresh challenge.
         * @param destination Finite runtime-owned staging, borrowed only during this callback.
         * @return Exact written units; no token or span is retained.
         */
        std::expected<std::size_t, d::CredentialError> basicToken(std::span<char16_t> destination) override {
            ++calls;

            if (destination.size() < TEST_TOKEN.size()) {
                return std::unexpected(d::CredentialError::REJECTED);
            }

            std::ranges::copy(TEST_TOKEN, destination.begin());

            return TEST_TOKEN.size();
        }
    };

    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto beforeReady = clientProtocol();
    append(beforeReady, wire(r::description(71)));
    auto reply = property(u"authentication", u"");
    append(reply, wire(r::ready()));
    auto script = x::peerOptions();
    script.captureUntilClose = true;
    script.expectedClientBytes = beforeReady;
    script.replyBytes = property(u"authentication", u"LOGIN synthetic-realm");
    script.expectedReplyClientBytes = property(u"authorization", TEST_TOKEN);
    script.finalBytes = reply;
    script.readTimeout = 8s;
    script.overallTimeout = 15s;
    script.successor.emplace();
    script.successor->expectedClientBytes = beforeReady;
    script.successor->replyBytes = script.replyBytes;
    script.successor->expectedReplyClientBytes = script.expectedReplyClientBytes;
    script.successor->finalBytes = reply;
    script.successor->pauseBeforeHandshake = 250ms;
    auto peer = h::TlsTestPeer::start(std::move(script));
    REQUIRE(peer);
    const auto tls = tlsOptions();
    const auto requirements = t::tlsRequirements(tls.limits, tls.host);
    REQUIRE(requirements);
    auto local = r::schema();
    auto queue = stream(local);
    auto consumer = std::make_shared<Consumer>();
    auto opening = r::queueWork();
    auto handle =
        queue.open({32, 512 * 1024, 8, 128 * 1024, 1, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, consumer, opening);
    REQUIRE(handle);
    std::array desired{c::SubscriptionRequest{{local.recordContext(0), u"A"}, {}},
                       c::SubscriptionRequest{{local.recordContext(0), u"B"}, {}}};
    m::QueueSubmitWork submission{10000, 1000000, 1000000, 1000000};
    auto initial = handle->replace(desired, submission);
    REQUIRE(initial);
    auto credentials = std::make_shared<Credentials>();
    auto input = x::settings(peer->port(), credentials);
    input.retry = {2, 2s};
    input.timeouts.read = 8s;
    auto endpoint = r::endpoint(local, input);
    t::RecoveryQueues queues;
    queues.stream = std::move(queue);
    auto preparing = r::preparation();
    auto options = r::options();
    options.protocolInactivity = 2s;
    auto owner =
        t::TlsRecovery::create(*bootstrap, endpoint, r::blueprint(), std::move(queues), options, tls, preparing);
    REQUIRE(owner);
    REQUIRE(owner->start());
    const auto deadline = std::chrono::steady_clock::now() + 12s;
    auto pump = [&](auto predicate) {
        while (!predicate() && std::chrono::steady_clock::now() < deadline) {
            const auto status = poll(*owner, *requirements);

            if (status.recovery.phase == t::RecoveryPhase::FAILED ||
                status.recovery.phase == t::RecoveryPhase::EXHAUSTED) {
                INFO("recovery stage=", status.recovery.first.stage, " TLS stage=", status.first.stage);

                return false;
            }

            std::this_thread::sleep_for(1ms);
        }

        return predicate();
    };
    REQUIRE(pump([&] {
        return owner->status().recovery.phase == t::RecoveryPhase::ACTIVE;
    }));
    CHECK(credentials->calls == 1);
    CHECK(owner->status().recovery.attempts == 1);
    REQUIRE(pump([&] {
        return owner->status().recovery.phase == t::RecoveryPhase::BACKOFF;
    }));
    m::QueueSubmitWork removalWork{10000, 1000000, 1000000, 1000000};
    const c::SubscriptionKey removed{local.recordContext(0), u"A"};
    auto removal = handle->remove(std::span(&removed, 1), removalWork);
    REQUIRE(removal);
    REQUIRE(pump([&] {
        return removal->result().has_value();
    }));
    REQUIRE(removal->result()->status == m::CommandStatus::APPLIED);
    CHECK(owner->status().recovery.phase == t::RecoveryPhase::BACKOFF);
    bool freshHandshakeObserved = false;
    const bool successorReady = pump([&] {
        const auto status = owner->status();
        const auto observed = peer->snapshot();

        if (status.recovery.attempts == 2 && observed.successfulHandshakes == 1 && observed.accepted &&
            !observed.handshakeComplete) {
            freshHandshakeObserved = true;
            CHECK(status.recovery.phase != t::RecoveryPhase::ACTIVE);
            CHECK(credentials->calls == 1);
        }

        return status.recovery.attempts == 2 && status.recovery.phase == t::RecoveryPhase::ACTIVE;
    });
    const auto diagnostic = owner->status();
    const auto peerDiagnostic = peer->snapshot();
    INFO("phase=", static_cast<int>(diagnostic.recovery.phase), " attempts=", diagnostic.recovery.attempts,
         " recoveryStage=", diagnostic.recovery.first.stage, " TLSStage=", diagnostic.first.stage,
         " childSessions=", peerDiagnostic.completedSessions, " handshakes=", peerDiagnostic.successfulHandshakes,
         " childFailed=", peerDiagnostic.failed, " childTimeout=", peerDiagnostic.timedOut,
         " childBytes=", peerDiagnostic.bytesRead, " credentials=", credentials->calls);
    REQUIRE(successorReady);
    CHECK(freshHandshakeObserved);
    CHECK(credentials->calls == 2);

    // Let the independently queued outgoing replay reach the peer before initiating cutoff.
    for (unsigned i = 0; i < 64; ++i) {
        (void)poll(*owner, *requirements);
        std::this_thread::sleep_for(1ms);
    }

    stop(*owner, *requirements);
    const auto peerDeadline = std::chrono::steady_clock::now() + 3s;

    while (peer->snapshot().childRunning && std::chrono::steady_clock::now() < peerDeadline) {
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(peer->snapshot().completed);
    REQUIRE_FALSE(peer->snapshot().childRunning);
    REQUIRE_FALSE(peer->snapshot().failed);
    const auto report = peer->stop();
    REQUIRE(report.completedSessions == 2);
    CHECK(report.firstCaptureEnd != 0);
    CHECK(report.secondCaptureEnd != 0);
    CHECK(report.successfulHandshakes == 2);
    CHECK(report.tlsVersion == static_cast<std::uint32_t>(h::PeerTlsVersion::TLS_1_3));
    CHECK(report.sniLocalhost);
    CHECK(report.matchedSessions == 2);
    CHECK_FALSE(report.failed);
    auto expectedFirst = beforeReady;
    append(expectedFirst, property(u"authorization", TEST_TOKEN));
    append(expectedFirst, add(u"A"));
    append(expectedFirst, add(u"B"));
    auto expectedSecond = beforeReady;
    append(expectedSecond, property(u"authorization", TEST_TOKEN));
    append(expectedSecond, add(u"B"));
    CHECK(report.firstClientBytes == expectedFirst);
    CHECK(report.secondClientBytes == expectedSecond);
    std::cout << "TLS_RECOVERY_SUCCESSOR_CAPTURE\t" << report.firstCaptureEnd << '\t' << report.secondCaptureEnd << '\t'
              << report.firstClientBytes.size() << '\t' << report.secondClientBytes.size() << '\n';
}
