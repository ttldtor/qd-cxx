// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: configuration/profile/adoption checks; no bootstrap, socket or TLS execution.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "recovery_configuration.hpp"
#include "recovery_copy_profile.hpp"

#include <atomic>
#include <doctest/doctest.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace p = root::protocol;
namespace m = root::mux;
namespace d = root::client;
namespace t = root::transport;
namespace detail = t::detail;
using namespace std::chrono_literals;

/** Build a simple valid record with caller-selected exact name lengths.
 * @param nameUnits Record and lookup lengths, used around SSO boundaries.
 * @return Owned independent provenance.
 */
c::LocalSchema schema(std::size_t nameUnits = 1) {
    const std::vector<c::LocalRecord> records{
        {0, std::u16string(nameUnits, u'R'), false, {{8}}, {{std::u16string(nameUnits, u'V'), 0}}, {}}};
    auto result = c::LocalSchema::create(records, {16, 128, 256, 128, 16384}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Supply explicit example component budgets without activating any runtime.
 * @return Structurally valid plain endpoint input.
 */
d::EndpointSettings settings() {
    auto value = d::controlledStreamSettings();
    REQUIRE(value);
    value->host = "127.0.0.1";
    value->port = 4321;
    value->retry = {3, 10ms};

    return std::move(*value);
}

/** Construct a complete immutable endpoint.
 * @param local Valid immutable schema.
 * @param input Explicit borrowed settings.
 * @return Prepared snapshot with no native owner.
 */
d::EndpointOptions endpoint(const c::LocalSchema &local, const d::EndpointSettings &input) {
    auto result = d::EndpointOptions::create(local, input);
    REQUIRE(result);

    return std::move(*result);
}

/** Construct matching explicit outgoing declarations.
 * @param local Immutable schema with one ordinary field.
 * @return Independently owned blueprint.
 */
std::vector<p::OutgoingRecord> blueprint(const c::LocalSchema &local) {
    return {{71, 0, local.records()[0].name, {{local.records()[0].lookups[0].name, 8, 0}}}};
}

/** Give independent finite owner credits, without production defaults.
 * @return Queue test turn.
 */
m::QueueWork queueWork() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Provide generous but finite capacities; individual tests tighten the required coordinate.
 * @return Independent recovery limits.
 */
t::RecoveryOptions options() {
    t::RecoveryOptions result;
    result.limits.schemaCopies = {1024, 1024, 1024, 32768};
    result.limits.blueprintCopy = {1024, 1024, 32768};
    result.limits.adoptedBlueprint = {1024, 1024, 32768};
    result.limits.endpointCopyBytes = 2 * 1024 * 1024;
    result.limits.writerConfigurationBytes = 4 * 1024 * 1024;
    result.limits.replay[1] = {32, 4096};

    return result;
}

/** Provide actual mutable independent preparation coordinates.
 * @return One funded preparation turn.
 */
t::RecoveryPreparationWork work() {
    return {{10000, 10000000, 10000000, 10000000},
            {10000, 10000000, 10000000, 10000000},
            {queueWork(), queueWork(), queueWork()}};
}

/** Create an unbound stable STREAM owner with finite future desired capacities.
 * @param local Required provenance.
 * @return Source owner, never admitted a protocol event.
 */
m::MuxQueue stream(const c::LocalSchema &local) {
    auto credits = queueWork();
    auto result = m::MuxQueue::createStream(
        local, {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024}, {16, 32, 128, 4096, 64, 8192, 64},
        {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
        {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024}, credits);
    REQUIRE(result);

    return std::move(*result);
}

/** Reject any attempted credential acquisition during configuration. */
class Provider final : public d::CredentialProvider {
    std::atomic<unsigned> &calls_; ///< External counter outlives the provider.
    public:
    /** Retain an externally owned observation.
     * @param calls Callback counter.
     */
    explicit Provider(std::atomic<unsigned> &calls) : calls_(calls) {
    }

    /** Count forbidden acquisition without writing credential bytes.
     * @param destination Borrowed output, left unchanged.
     * @return Static unavailable error.
     */
    std::expected<std::size_t, d::CredentialError> basicToken(std::span<char16_t> destination) override {
        (void)destination;
        ++calls_;

        return std::unexpected(d::CredentialError::UNAVAILABLE);
    }
};
} // namespace

TEST_CASE("recovery configuration retains one real first writer and normalizes only its endpoint retry") {
    REQUIRE(detail::recoveryCopyProfile() != 0);
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    const auto *backing = declarations.data();
    t::RecoveryQueues queues;
    auto credits = work();
    auto prepared = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE(prepared);
    CHECK(original.settings().retry.maximumAttempts == 3);
    CHECK(prepared->endpoint.settings().retry.maximumAttempts == 1);
    CHECK(prepared->endpoint.settings().retry.delay == 0ms);
    CHECK(prepared->scheduler.maximumAttempts == 3);
    CHECK(prepared->scheduler.retryDelay == 10ms);
    CHECK(declarations.data() == backing);
    CHECK(prepared->blueprint.empty());
    REQUIRE(prepared->adopt(std::move(declarations), std::move(queues)));
    CHECK(prepared->blueprint.data() == backing);
    REQUIRE(prepared->firstWriter);
    CHECK(prepared->firstWriter->matchesSchema(local));
    CHECK(prepared->firstWriter->retainedBytes() == 0);
    CHECK(prepared->capacity.writerConfigurationBytes == prepared->firstWriter->storageBytes());
    const c::SubscriptionRequest request{{local.recordContext(0), u"AAPL"}, {}};
    auto composerWork = work().writer;
    auto output = prepared->firstWriter->compose(p::SubscriptionMessage::STREAM_ADD, {&request, 1}, composerWork);
    REQUIRE(output);
    CHECK_FALSE(output->bytes().empty());
}

TEST_CASE("recovery copy admission includes three schemas and refuses every one-short coordinate") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    auto credits = work();
    auto admitted = detail::inspectRecoveryCopies(original, declarations, options().limits, credits.recovery);
    REQUIRE(admitted);
    CHECK(admitted->schemaCopies.records == 3);
    CHECK(admitted->schemaCopies.fields == 3);
    CHECK(admitted->schemaCopies.lookups == 3);
    CHECK(admitted->schemaCopies.nameUnits == 54);
    CHECK(admitted->blueprintCopy.nameUnits == 18);
    CHECK(admitted->endpointCopyBytes == 50);

    for (unsigned coordinate = 0; coordinate != 11; ++coordinate) {
        auto bounds = options();
        bounds.limits.schemaCopies = admitted->schemaCopies;
        bounds.limits.blueprintCopy = admitted->blueprintCopy;
        bounds.limits.adoptedBlueprint = admitted->adoptedBlueprint;
        bounds.limits.endpointCopyBytes = admitted->endpointCopyBytes;
        std::array<std::size_t *, 11> axes{
            &bounds.limits.schemaCopies.records,    &bounds.limits.schemaCopies.fields,
            &bounds.limits.schemaCopies.lookups,    &bounds.limits.schemaCopies.nameUnits,
            &bounds.limits.blueprintCopy.records,   &bounds.limits.blueprintCopy.fields,
            &bounds.limits.blueprintCopy.nameUnits, &bounds.limits.adoptedBlueprint.records,
            &bounds.limits.adoptedBlueprint.fields, &bounds.limits.adoptedBlueprint.nameUnits,
            &bounds.limits.endpointCopyBytes};
        REQUIRE(*axes[coordinate] != 0);
        --*axes[coordinate];
        auto currentWork = work();
        t::RecoveryQueues queues;
        const auto *backing = declarations.data();
        auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                               d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, currentWork);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::RecoveryError::RESOURCE_LIMIT);
        CHECK(declarations.data() == backing);
        CHECK(declarations.size() == 1);
    }

    auto bounds = options();
    bounds.limits.schemaCopies = admitted->schemaCopies;
    bounds.limits.blueprintCopy = admitted->blueprintCopy;
    bounds.limits.adoptedBlueprint = admitted->adoptedBlueprint;
    bounds.limits.endpointCopyBytes = admitted->endpointCopyBytes;
    credits = work();
    t::RecoveryQueues queues;
    CHECK(detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                 d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits));
}

TEST_CASE("recovery charges sparse adopted capacities independently from future copied sizes") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    declarations.reserve(17);
    declarations[0].fields.reserve(19);
    declarations[0].wireName.reserve(101);
    auto credits = work();
    auto required = detail::inspectRecoveryCopies(original, declarations, options().limits, credits.recovery);
    REQUIRE(required);
    CHECK(required->blueprintCopy.records == 1);
    CHECK(required->blueprintCopy.fields == 1);
    CHECK(required->adoptedBlueprint.records >= 17);
    CHECK(required->adoptedBlueprint.fields >= 19);
    CHECK(required->adoptedBlueprint.nameUnits >= 102);
    auto bounds = options();
    bounds.limits.adoptedBlueprint.records = 1;
    credits = work();
    t::RecoveryQueues queues;
    auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::RecoveryError::RESOURCE_LIMIT);
    CHECK(declarations.capacity() >= 17);
}

TEST_CASE("recovery qualified string copies fit actual nested capacities across SSO boundaries") {
    for (const auto length : {1U, 6U, 7U, 8U, 14U, 15U, 16U, 31U}) {
        auto local = schema(length);
        auto input = settings();
        input.host.assign(length, 'a');
        auto original = endpoint(local, input);
        auto declarations = blueprint(local);
        auto credits = work();
        t::RecoveryQueues queues;
        auto prepared = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, options(),
                                                               d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
        REQUIRE(prepared);
        CHECK(prepared->capacity.schemaCopies.nameUnits == 3 * 2 * (length + 8));
        CHECK(prepared->capacity.blueprintCopy.nameUnits == 2 * (length + 8));
        CHECK(prepared->endpoint.settings().host.capacity() + 1 <= length + 16);
        CHECK(detail::recoverySchemaFits(prepared->endpoint.schema(), {1, 1, 1, 2 * (length + 8)}));
    }
}

TEST_CASE("recovery writer post-adoption refusal preserves inputs and already spent preparation work") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    const auto *backing = declarations.data();
    auto bounds = options();
    bounds.limits.writerConfigurationBytes = 1;
    auto credits = work();
    const auto operations = credits.writer.operations;
    t::RecoveryQueues queues;
    auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "recovery-writer-adoption");
    CHECK(credits.writer.operations < operations);
    CHECK(declarations.data() == backing);
    CHECK(declarations[0].senderId == 71);
}

TEST_CASE("recovery preactivation replay limits cover future source capacity and preserve queue on refusal") {
    for (unsigned coordinate = 0; coordinate != 3; ++coordinate) {
        auto local = schema();
        auto original = endpoint(local, settings());
        auto declarations = blueprint(local);
        t::RecoveryQueues queues;
        queues.stream.emplace(stream(local));
        const auto before = queues.stream->capacity();
        auto bounds = options();

        if (coordinate == 0) {
            bounds.limits.replay[1].entries = 31;
        } else if (coordinate == 1) {
            bounds.limits.replay[1].symbolUnits = 4095;
        } else {
            bounds.limits.replay[1].entries = std::numeric_limits<std::size_t>::max();
        }

        auto credits = work();
        auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                               d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::RecoveryError::RESOURCE_LIMIT);
        REQUIRE(queues.stream);
        CHECK(queues.stream->state() == m::QueueState::OPEN);
        CHECK(queues.stream->capacity().queueBytes == before.queueBytes);
        CHECK(declarations.size() == 1);
    }
}

TEST_CASE("recovery exact replay source maximum allocates three banks before adopting stable owner") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto credits = work();
    auto prepared = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE(prepared);
    CHECK(prepared->capacity.replayEntries == 96);
    CHECK(prepared->capacity.replaySymbolUnits == 12288);
    CHECK(prepared->scheduler.families == 2);
    CHECK(queues.stream->state() == m::QueueState::OPEN);
    REQUIRE(prepared->adopt(std::move(declarations), std::move(queues)));
    CHECK(prepared->queues.stream->state() == m::QueueState::OPEN);

    for (const auto &bank : prepared->replay[1].banks) {
        CHECK(bank.entries != nullptr);
        CHECK(bank.symbols != nullptr);
        CHECK(bank.size == 0);
    }
}

TEST_CASE("recovery rejects foreign queue provenance without consuming its owner") {
    auto local = schema();
    auto foreign = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(foreign));
    auto credits = work();
    auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::RecoveryError::INVALID_CONFIGURATION);
    REQUIRE(rejected.error().queue);
    CHECK(rejected.error().queue->error == m::QueueError::INVALID_CONTEXT);
    CHECK(queues.stream->state() == m::QueueState::OPEN);
    CHECK(declarations.size() == 1);
}

TEST_CASE("recovery configuration never invokes a provider even after late writer rejection") {
    auto local = schema();
    auto input = settings();
    std::atomic<unsigned> calls{0};
    input.security = d::EndpointSecurity::TLS;
    input.tls.emplace();
    input.tls->referenceIdentity = "localhost";
    input.tls->trustPem = "synthetic-not-certificate";
    input.tls->handshakeTimeout = 100ms;
    input.tls->closeTimeout = 100ms;
    input.tls->authenticationTimeout = 100ms;
    input.tls->tokenUnits = 128;
    input.tls->credentials = std::make_shared<Provider>(calls);
    auto original = endpoint(local, input);
    auto declarations = blueprint(local);
    auto credits = work();
    auto bounds = options();
    bounds.limits.writerConfigurationBytes = 1;
    t::RecoveryQueues queues;
    auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, bounds,
                                                           d::EndpointSecurity::TLS, credits);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "recovery-writer-adoption");
    CHECK(calls == 0);
    CHECK(declarations.size() == 1);
}

TEST_CASE("recovery preparation low work leaves all input backing usable") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    auto credits = work();
    credits.writer.operations = 0;
    t::RecoveryQueues queues;
    auto rejected = detail::RecoveryConfiguration::prepare(original, std::move(declarations), queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == t::RecoveryError::WORK_LIMIT);
    CHECK(declarations[0].wireName == u"R");
    CHECK(original.settings().retry.maximumAttempts == 3);
}

TEST_CASE("recovery candidate preparation leaves owners borrowed until the single nonthrowing commit") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    const auto *backing = declarations.data();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto credits = work();
    auto prepared = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    REQUIRE(prepared);
    CHECK_FALSE(prepared->adopted);
    CHECK(prepared->blueprint.empty());
    CHECK_FALSE(prepared->queues.stream);
    CHECK(declarations.data() == backing);
    CHECK(queues.stream->state() == m::QueueState::OPEN);
    REQUIRE(prepared->adopt(std::move(declarations), std::move(queues)));
    CHECK(prepared->blueprint.data() == backing);
    auto second = blueprint(local);
    const auto *secondBacking = second.data();
    t::RecoveryQueues secondQueues;
    CHECK_FALSE(prepared->adopt(std::move(second), std::move(secondQueues)));
    CHECK(second.data() == secondBacking);
    CHECK(second.size() == 1);
}

TEST_CASE("recovery scan-only work cannot fund endpoint copies and their later disposal") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    const auto *backing = declarations.data();
    auto credits = work();
    credits.recovery = {1, 9, 13, 0};
    t::RecoveryQueues queues;
    auto prepared = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
    CHECK_FALSE(prepared);

    if (!prepared) {
        CHECK(prepared.error().error == t::RecoveryError::WORK_LIMIT);
    }

    CHECK(declarations.data() == backing);
    CHECK(original.settings().retry.maximumAttempts == 3);
}

TEST_CASE("recovery exact preparation work includes copies postchecks and persistent disposal in every coordinate") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    const auto *backing = declarations.data();
    t::RecoveryQueues queues;
    queues.stream.emplace(stream(local));
    auto generous = work();
    const auto before = generous.recovery;
    auto baseline = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, generous);
    REQUIRE(baseline);
    const t::RecoveryWork consumed{
        before.operations - generous.recovery.operations, before.visits - generous.recovery.visits,
        before.textUnits - generous.recovery.textUnits, before.bytes - generous.recovery.bytes};
    CHECK(baseline->disposal.bytes > baseline->capacity.replayBytes);
    CHECK(baseline->disposal.visits > baseline->capacity.replayEntries);

    for (unsigned coordinate = 0; coordinate != 4; ++coordinate) {
        auto credits = work();
        credits.recovery = consumed;
        std::array<std::size_t *, 4> axes{&credits.recovery.operations, &credits.recovery.visits,
                                          &credits.recovery.textUnits, &credits.recovery.bytes};
        REQUIRE(*axes[coordinate] != 0);
        --*axes[coordinate];
        const auto writerOperations = credits.writer.operations;
        auto rejected = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                               d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, credits);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == t::RecoveryError::WORK_LIMIT);
        CHECK(credits.writer.operations == writerOperations);
        CHECK(declarations.data() == backing);
        CHECK(queues.stream->state() == m::QueueState::OPEN);
    }

    auto exact = work();
    exact.recovery = consumed;
    auto accepted = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, exact);
    REQUIRE(accepted);
    CHECK(exact.recovery.operations == 0);
    CHECK(exact.recovery.visits == 0);
    CHECK(exact.recovery.textUnits == 0);
    CHECK(exact.recovery.bytes == 0);
    CHECK(declarations.data() == backing);
    CHECK(queues.stream->state() == m::QueueState::OPEN);
}

TEST_CASE("recovery later writer refusal never refunds the earlier copy and rollback precharge") {
    auto local = schema();
    auto original = endpoint(local, settings());
    auto declarations = blueprint(local);
    t::RecoveryQueues queues;
    auto goodWork = work();
    auto accepted = detail::RecoveryConfiguration::prepare(original, declarations, queues, options(),
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, goodWork);
    REQUIRE(accepted);
    auto bounds = options();
    bounds.limits.writerConfigurationBytes = 1;
    auto rejectedWork = work();
    auto rejected = detail::RecoveryConfiguration::prepare(original, declarations, queues, bounds,
                                                           d::EndpointSecurity::CREDENTIAL_FREE_PLAIN, rejectedWork);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().stage == "recovery-writer-adoption");
    CHECK(rejectedWork.recovery.operations == goodWork.recovery.operations);
    CHECK(rejectedWork.recovery.visits == goodWork.recovery.visits);
    CHECK(rejectedWork.recovery.textUnits == goodWork.recovery.textUnits);
    CHECK(rejectedWork.recovery.bytes == goodWork.recovery.bytes);
    CHECK(declarations.size() == 1);
}
