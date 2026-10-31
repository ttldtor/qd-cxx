// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "stream_client.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::examples {
namespace {
namespace c = core;
namespace d = client;
namespace p = protocol;
namespace t = transport;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr c::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Fixed bounded example schema profile.
constexpr std::int32_t OUTGOING_RECORD_ID = 0;                     ///< Explicit direction-local sender identity.
constexpr std::size_t MAXIMUM_SYMBOLS = 16;                        ///< Exact selection ceiling.
constexpr std::size_t MAXIMUM_SYMBOL_UNITS = 128;                  ///< Maximum units in one exact selector.
constexpr std::size_t MEBIBYTE = 1024 * 1024; ///< Byte unit used by independent requested-storage allowances.
constexpr auto MAXIMUM_DURATION = 600000ms;   ///< Maximum explicit admission interval.

/** Start one bounded application owner turn, separate from feed lifetime.
 * @return Finite dispatcher and nested mutation/collector work.
 */
d::DispatchWork dispatchTurn() {
    return {10000,
            10000,
            10000,
            1000000,
            1000000,
            1000000,
            100 * MEBIBYTE,
            {10000, 1000000, 1000000},
            {10000, 1000000, 1000000, 100 * MEBIBYTE}};
}

/** Start one bounded protocol owner turn.
 * @return Finite preparation/parse/compose credits.
 */
p::LiveWork protocolTurn() {
    return {10000, 10000000, 10000000, 100 * MEBIBYTE};
}

/** Specify the credential-free, zero-event-flag binary profile without automatic timing.
 * @return Independent finite session capacities, not process RSS or cumulative feed quotas.
 */
p::LiveLimits sessionLimits() {
    p::LiveLimits result{};
    result.frame = {8192, 8201};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    result.context = {SCHEMA_LIMITS, 32, {512, 1024, 16384}};
    result.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    result.residentBytes = 4 * MEBIBYTE;
    result.owners = 128;
    result.snapshotBytes = 65536;
    result.scratchBytes = 65536;
    result.eventBytes = 65536;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::ZERO_FLAGS;
    result.historySnapshot = false;

    return result;
}

/** Validate borrowed settings before allocating owners or accessing the borrowed bootstrap.
 * @param schema Immutable local configuration.
 * @param options Borrowed run settings.
 * @return Whether the bounded ordinary exact-selector profile is representable.
 */
bool validOptions(const c::LocalSchema &schema, StreamOptions options) {
    if (options.host.empty() || options.host.size() > 45 || options.port == 0 || options.recordId < 0 ||
        static_cast<std::size_t>(options.recordId) >= schema.records().size() || options.symbols.empty() ||
        options.symbols.size() > MAXIMUM_SYMBOLS || options.duration <= 0ms || options.duration > MAXIMUM_DURATION ||
        (options.removeAfter && (*options.removeAfter <= 0ms || *options.removeAfter >= options.duration)) ||
        (options.maximumRecords && *options.maximumRecords == 0) || options.ownerOperationsPerTurn == 0 ||
        options.ownerOperationsPerTurn > 10000) {
        return false;
    }

    const auto &record = schema.records()[static_cast<std::size_t>(options.recordId)];

    if (record.hasTime) {
        return false;
    }

    return std::ranges::all_of(options.symbols, [](const auto &symbol) {
        return symbol.size() <= MAXIMUM_SYMBOL_UNITS && (symbol.empty() || symbol.front() != u'*');
    });
}

/** Build the single explicit complete outgoing record without inferring aliases.
 * @param record Selected ordinary local declaration.
 * @return Exact independently owned outgoing layout; allocation failure propagates before runtime creation.
 */
p::OutgoingRecord declaration(const c::LocalRecord &record) {
    p::OutgoingRecord output{OUTGOING_RECORD_ID, record.id, record.name, {}};

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });
        output.fields.push_back({lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    return output;
}

/** Bounded local pump result; a yield cannot authorize later source or command work. */
enum class LocalProgress { IDLE, YIELD, TERMINAL, FAILED };

/** Apply all available prior local work, including callback stop flags, before polling the source.
 * @param dispatcher Serialized local owner.
 * @param work Mutable credits shared across this complete application owner turn.
 * @param pristineWorkLimit Optional output: actual WORK_LIMIT on the first call before any successful step.
 * @return Idle/terminal, or a local-only retry/failure requirement.
 */
LocalProgress progressLocal(d::StreamDispatcher &dispatcher, d::DispatchWork &work, bool *pristineWorkLimit = nullptr) {
    bool succeeded = false;

    if (pristineWorkLimit) {
        *pristineWorkLimit = false;
    }

    for (;;) {
        const auto result = dispatcher.progress(work);

        if (!result) {
            if (pristineWorkLimit && !succeeded && result.error() == d::ClientError::WORK_LIMIT) {
                *pristineWorkLimit = true;
            }

            return result.error() == d::ClientError::WORK_LIMIT ? LocalProgress::YIELD : LocalProgress::FAILED;
        }

        succeeded = true;

        if (result->state == d::DispatchState::TERMINAL) {
            return LocalProgress::TERMINAL;
        }

        if (result->kind == d::ProgressKind::IDLE) {
            return LocalProgress::IDLE;
        }
    }
}

/** Inspect only explicit temporary composition refusal categories.
 * @param error Static/numeric runtime output rejection.
 * @return True only for the occupied slot or peer readiness, never general resource exhaustion.
 */
bool retryOutput(const t::TcpDiagnostic &error) {
    return (error.error == t::TcpError::RESOURCE_LIMIT && error.stage == "output-slot") ||
           (error.error == t::TcpError::PROTOCOL_FAILURE && error.protocolError == p::LiveError::NOT_READY);
}

/** Retain the first application failure without replacing lower-owner evidence.
 * @param result Mutable run result.
 * @param failure Static application category.
 */
void fail(StreamResult &result, StreamFailure failure) {
    if (result.failure == StreamFailure::NONE) {
        result.failure = failure;
    }
}

/** Drive already prepared owners without allocating app-owned configuration in the loop.
 * @param runtime Sole session/socket owner.
 * @param dispatcher Independent serialized local dispatcher.
 * @param handle Private public control reference, retained independently of the published slot.
 * @param requests Exact owned-provenance input views; symbol backing outlives this call.
 * @param keys Matching removal keys with the same lifetime.
 * @param options Borrowed application controls.
 * @param result Result initialized by preparation, completed with both terminal scopes.
 * @param configuration Optional borrowed snapshot settings; null preserves exact legacy work/default behavior.
 */
void drive(t::TcpRuntime &runtime, d::StreamDispatcher &dispatcher, const d::ClientHandle &handle,
           std::span<const c::SubscriptionRequest> requests, std::span<const c::SubscriptionKey> keys,
           StreamOptions options, StreamResult &result, const d::EndpointSettings *configuration) {
    const auto started = Clock::now();
    const auto cutoff = started + options.duration;
    const auto removal = started + options.removeAfter.value_or(options.duration);
    unsigned outputStage = 0;
    bool stopping = false;
    bool removeSubmitted = false;
    std::optional<d::CommandTicket> mutation;
    auto submitWork = configuration ? configuration->budgets.submitTurn : d::SubmitWork{16, 1000000, 1000000};
    auto added = handle.add(requests, submitWork);

    if (!added) {
        fail(result, StreamFailure::DISPATCH);
        stopping = true;
        (void)handle.requestStop(d::StopMode::ABORT);
        (void)runtime.requestClose();
    } else {
        mutation.emplace(std::move(*added));
    }

    const auto beginStop = [&](bool abort) {
        stopping = true;
        (void)handle.requestStop(abort ? d::StopMode::ABORT : d::StopMode::GRACEFUL);
        (void)runtime.requestClose();
    };
    const auto observeCutoff = [&] {
        if (!stopping && options.interrupted && *options.interrupted != 0) {
            result.interrupted = true;
            beginStop(false);
        }

        if (options.maximumRecords && dispatcher.counts().completedRecords >= *options.maximumRecords) {
            result.targetReached = true;

            if (!stopping) {
                beginStop(false);
            }
        }

        if (!stopping && Clock::now() >= cutoff) {
            result.intervalExpired = true;
            beginStop(false);
        }
    };

    for (;;) {
        // A callback may only have set the port flag: observe it before another source admission.
        auto dispatchWork = configuration ? configuration->budgets.dispatchTurn : dispatchTurn();
        dispatchWork.operations = configuration ? std::min(dispatchWork.operations, options.ownerOperationsPerTurn)
                                                : options.ownerOperationsPerTurn;
        bool pristineWorkLimit = false;
        const auto local = progressLocal(dispatcher, dispatchWork, configuration ? &pristineWorkLimit : nullptr);

        if (configuration && pristineWorkLimit) {
            result.diagnostic = t::TcpDiagnostic{t::TcpError::WORK_LIMIT, "endpoint-dispatch-work"};
            fail(result, StreamFailure::DISPATCH);
            beginStop(true);
        }

        observeCutoff();

        if (local == LocalProgress::YIELD) {
            continue;
        }

        if (local == LocalProgress::FAILED) {
            fail(result, StreamFailure::DISPATCH);
            beginStop(true);
        }

        if (dispatcher.state() != d::DispatchState::OPEN) {
            beginStop(false);
        }

        if (mutation && mutation->result()) {
            const auto completed = *mutation->result();

            if (completed.status != d::CommandStatus::APPLIED) {
                fail(result, StreamFailure::DISPATCH);
                beginStop(true);
            } else if (removeSubmitted) {
                result.removeApplied = true;
            } else {
                result.addApplied = true;
            }

            mutation.reset();
        }

        const auto now = Clock::now();

        if (!stopping && result.addComposed && options.removeAfter && now >= removal && !removeSubmitted) {
            auto work = configuration ? configuration->budgets.submitTurn : d::SubmitWork{16, 1000000, 1000000};
            auto removed = handle.remove(keys, work);

            if (!removed) {
                fail(result, StreamFailure::DISPATCH);
                beginStop(true);
            } else {
                mutation.emplace(std::move(*removed));
                removeSubmitted = true;
                continue;
            }
        }

        if (!stopping) {
            auto work = configuration ? configuration->budgets.protocolTurn : protocolTurn();
            std::expected<void, t::TcpDiagnostic> output;
            bool attempted = true;

            if (outputStage == 0) {
                const p::LiveMessageView send[]{{16, u"STREAM_ADD_SUBSCRIPTION", {}},
                                                {17, u"STREAM_REMOVE_SUBSCRIPTION", {}}};
                const p::LiveMessageView receive[]{{15, u"STREAM_DATA", {}}};
                output = runtime.composeProtocol({{}, send, receive}, work);
            } else if (outputStage == 1) {
                output = runtime.describe(std::span(&OUTGOING_RECORD_ID, 1), work);
            } else if (outputStage == 2 && result.addApplied) {
                output = runtime.subscribe(p::SubscriptionMessage::STREAM_ADD, requests, work);
            } else if (outputStage == 3 && result.removeApplied) {
                output = runtime.subscribe(p::SubscriptionMessage::STREAM_REMOVE, requests, work);
            } else {
                attempted = false;
            }

            if (attempted && output) {
                ++outputStage;
                result.addComposed = outputStage >= 3;
                result.removeComposed = outputStage >= 4;
            } else if (attempted && !retryOutput(output.error())) {
                result.diagnostic = output.error();
                fail(result, StreamFailure::OUTPUT);
                beginStop(true);
            }
        }

        if (stopping) {
            (void)runtime.requestClose();
        }

        if (!runtime.terminal()) {
            const auto credits = configuration ? configuration->budgets.runtimeTurn
                                               : d::EndpointRuntimeWork{128, 128, 10000000, 0, 0, 0, 0};
            t::TcpWork work{credits.steps,      credits.handlers, credits.socketBytes, credits.dnsCalls,
                            credits.dnsPackets, credits.dnsBytes, credits.dnsUnits};
            auto sessionWork = configuration ? configuration->budgets.protocolTurn : protocolTurn();
            auto next = runtime.poll(work, sessionWork);

            if (!next) {
                result.diagnostic = next.error();
                fail(result, StreamFailure::DISPATCH);
                beginStop(true);
            } else if (next->event) {
                // There is no ordinary command/progress between poll and whole-event ownership admission.
                if (!stopping && !dispatcher.admit(std::move(*next->event))) {
                    fail(result, StreamFailure::DISPATCH);
                    beginStop(true);
                } else if (!stopping) {
                    // Preserve this turn's remaining credits: an admitted event can legitimately yield.
                    const auto pending = progressLocal(dispatcher, dispatchWork);

                    if (pending == LocalProgress::YIELD &&
                        result.eventWorkYields != std::numeric_limits<std::size_t>::max()) {
                        ++result.eventWorkYields;
                    } else if (pending == LocalProgress::FAILED) {
                        fail(result, StreamFailure::DISPATCH);
                        beginStop(true);
                    }
                }

                continue;
            }
        }

        if (runtime.terminal() && !stopping) {
            beginStop(false);
        }

        if (runtime.terminal() && dispatcher.terminal()) {
            result.transport = runtime.terminal();
            result.local = dispatcher.terminal();

            return;
        }

        std::this_thread::sleep_for(configuration ? configuration->timeouts.poll : 1ms);
    }
}

/** Prepare and execute one mode without changing the legacy rejection order.
 * @param bootstrap Borrowed explicit process owner.
 * @param schema Immutable selected declarations.
 * @param options Borrowed operation selection.
 * @param consumer Retained callable through retirement.
 * @param publishedHandle Initially empty output slot.
 * @param configuration Borrowed snapshot, or null for the unchanged legacy mode.
 * @return Separate preparation and retirement evidence.
 */
StreamResult runPrepared(t::RuntimeBootstrap &bootstrap, const c::LocalSchema &schema, StreamOptions options,
                         const std::shared_ptr<d::Consumer> &consumer, std::optional<d::ClientHandle> &publishedHandle,
                         const d::EndpointSettings *configuration) {
    StreamResult result;

    if (publishedHandle || !consumer || !validOptions(schema, options)) {
        result.failure = StreamFailure::CONFIGURATION;

        return result;
    }

    try {
        auto copied = schema.boundedCopy(configuration ? configuration->budgets.schema : SCHEMA_LIMITS);

        if (!copied) {
            result.failure = StreamFailure::PREPARATION;

            return result;
        }

        auto outgoing = declaration(schema.records()[static_cast<std::size_t>(options.recordId)]);
        std::vector<c::SubscriptionRequest> requests;
        std::vector<c::SubscriptionKey> keys;

        for (const auto &symbol : options.symbols) {
            const c::SubscriptionKey key{schema.recordContext(options.recordId), symbol};
            requests.push_back({key, {}});
            keys.push_back(key);
        }

        auto writerWork = configuration ? configuration->budgets.writerPreparation
                                        : p::SubscriptionWriterWork{10000, 1000000, 1000000, 1000000};
        auto writer = p::SubscriptionWriter::create(
            schema, std::span(&outgoing, 1),
            configuration ? configuration->budgets.layout : p::LayoutLimits{SCHEMA_LIMITS, 16, 128, 128, 16384},
            configuration ? configuration->budgets.writer
                          : p::SubscriptionWriterLimits{8192, 8201, 512, 128, {8192, 1024}, 65536, 65536, 128},
            writerWork);

        if (!writer) {
            result.failure = StreamFailure::PREPARATION;

            return result;
        }

        auto work = configuration ? configuration->budgets.dispatchTurn : dispatchTurn();

        if (configuration) {
            work.operations = std::min(work.operations, options.ownerOperationsPerTurn);
        }

        auto dispatcher = d::StreamDispatcher::create(
            schema,
            configuration ? configuration->budgets.desired : c::SubscriptionLimits{16, 32, 128, 2048, 64, 4096, 32},
            configuration ? configuration->budgets.collector
                          : collectors::StreamLimits{16, 128, 2 * MEBIBYTE, 4 * MEBIBYTE},
            configuration ? configuration->budgets.client
                          : d::ClientLimits{4, 8, 16, 2048, 1, 128, 128, 8192, 8192, 65536, 4, MEBIBYTE, 2 * MEBIBYTE},
            consumer, work);

        if (!dispatcher) {
            result.failure = StreamFailure::PREPARATION;

            return result;
        }

        auto handle = dispatcher->handle();
        auto sessionWork = configuration ? configuration->budgets.protocolTurn : protocolTurn();
        t::TcpOptions tcp{options.host,
                          options.port,
                          {},
                          53,
                          1s,
                          1s,
                          options.duration,
                          options.duration,
                          1ms,
                          {8192, 8192, 16, 4, 65536, 8192, 8 * MEBIBYTE}};

        if (configuration) {
            const auto &timeouts = configuration->timeouts;
            const auto &limits = configuration->budgets.tcp;
            tcp.dnsTimeout = timeouts.dns;
            tcp.connectTimeout = timeouts.connect;
            tcp.readTimeout = timeouts.read;
            tcp.writeTimeout = timeouts.write;
            tcp.pollInterval = timeouts.poll;
            tcp.limits = {limits.inputBytes,   limits.writeBytes,   limits.addresses,    limits.cnameDepth,
                          limits.contextBytes, limits.handlerBytes, limits.residentBytes};
        }

        auto runtime =
            t::TcpRuntime::create(bootstrap, std::move(*copied), std::move(*writer),
                                  configuration ? configuration->budgets.session : sessionLimits(), tcp, sessionWork);

        if (!handle || !runtime) {
            result.failure = StreamFailure::PREPARATION;

            if (!runtime) {
                result.diagnostic = runtime.error();
            }

            return result;
        }

        publishedHandle.emplace(*handle);
        const auto started = runtime->start();

        if (!started) {
            result.diagnostic = started.error();
            fail(result, StreamFailure::PREPARATION);
            (void)handle->requestStop(d::StopMode::ABORT);
            (void)runtime->requestClose();
        }

        drive(*runtime, *dispatcher, *handle, requests, keys, options, result, configuration);
    } catch (const std::bad_alloc &) {
        fail(result, StreamFailure::PREPARATION);
    }

    return result;
}
} // namespace

StreamResult runStream(t::RuntimeBootstrap &bootstrap, const c::LocalSchema &schema, StreamOptions options,
                       const std::shared_ptr<d::Consumer> &consumer, std::optional<d::ClientHandle> &publishedHandle) {
    return runPrepared(bootstrap, schema, options, consumer, publishedHandle, nullptr);
}

StreamResult runStream(t::RuntimeBootstrap &bootstrap, const d::EndpointOptions &endpoint, StreamOptions options,
                       const std::shared_ptr<d::Consumer> &consumer, std::optional<d::ClientHandle> &publishedHandle) {
    const auto supported = endpoint.requirePlainStream();
    const auto &settings = endpoint.settings();

    if (!supported || (!options.host.empty() && options.host != settings.host) ||
        (options.port != 0 && options.port != settings.port)) {
        StreamResult result;
        result.failure = StreamFailure::CONFIGURATION;
        result.diagnostic = t::TcpDiagnostic{t::TcpError::INVALID_CONFIGURATION,
                                             supported ? "endpoint-selection" : supported.error().stage};

        return result;
    }

    if (options.recordId >= 0 && static_cast<std::size_t>(options.recordId) < endpoint.schema().records().size()) {
        const auto fields = endpoint.schema().records()[static_cast<std::size_t>(options.recordId)].fields.size();

        if (fields > std::numeric_limits<std::size_t>::max() / 2 || settings.budgets.dispatchTurn.fields < fields * 2) {
            StreamResult result;
            result.failure = StreamFailure::CONFIGURATION;
            result.diagnostic = t::TcpDiagnostic{t::TcpError::WORK_LIMIT, "endpoint-record-work"};

            return result;
        }
    }

    options.host = settings.host;
    options.port = settings.port;

    return runPrepared(bootstrap, endpoint.schema(), options, consumer, publishedHandle, &settings);
}
} // namespace org::ttldtor::qdcxx::examples
