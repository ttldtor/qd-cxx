// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "arguments.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace org::ttldtor::qdcxx::examples::quote_subscription {
namespace {
using Clock = std::chrono::steady_clock;
using cli::Arguments;
volatile std::sig_atomic_t interrupted = 0; ///< Signal-safe request observed only by the creating owner thread.

/** Preserve a signal request without using allocation, streams or recovery APIs in its handler.
 * @param signal Process signal number; only a pending interruption flag is needed.
 */
void onInterrupt(int signal) noexcept {
    static_cast<void>(signal);
    interrupted = 1;
}

/** Print typed static/numeric recovery causes without echoing endpoint or peer payload text.
 * @param error Existing detached diagnostic supplied by the real owner.
 */
void printRecovery(const transport::RecoveryDiagnostic &error) {
    std::cerr << "recovery error=" << static_cast<int>(error.error) << " stage=" << error.stage
              << " transport=" << static_cast<int>(error.transport.error) << " transportStage=" << error.transport.stage
              << " native=" << error.transport.nativeCode
              << " protocol=" << static_cast<int>(error.transport.protocolError) << '\n';
}

/** Name the actual public recovery phase without interpreting a local phase as peer acknowledgement.
 * @param phase Public detached owner status.
 * @return Static human-readable phase name.
 */
std::string_view phaseName(transport::RecoveryPhase phase) noexcept {
    using Phase = transport::RecoveryPhase;

    switch (phase) {
    case Phase::CONFIGURED:
        return "CONFIGURED";
    case Phase::PREPARING:
        return "PREPARING";
    case Phase::STARTING:
        return "STARTING";
    case Phase::NEGOTIATING:
        return "NEGOTIATING";
    case Phase::REPLAYING:
        return "REPLAYING";
    case Phase::ACTIVE:
        return "ACTIVE";
    case Phase::RETIRING:
        return "RETIRING";
    case Phase::RESETTING:
        return "RESETTING";
    case Phase::BACKOFF:
        return "BACKOFF";
    case Phase::STOPPING:
        return "STOPPING";
    case Phase::CLOSED:
        return "CLOSED";
    case Phase::EXHAUSTED:
        return "EXHAUSTED";
    case Phase::FAILED:
        return "FAILED";
    }

    return "UNKNOWN";
}

/** Print an exact ASCII subscription symbol without changing the host console configuration.
 * @param symbol Exact symbol from the owned prepared event; caller requests only printable ASCII.
 */
void printSymbol(std::u16string_view symbol) {
    for (const auto unit : symbol) {
        if (unit >= FIRST_PRINTABLE_ASCII && unit <= LAST_PRINTABLE_ASCII) {
            std::cout.put(static_cast<char>(unit));
        } else {
            std::cout.put('?');
        }
    }
}

/** One application callback boundary; owned projected events are used only within this call. */
class QuoteConsumer final : public client::SubscriptionConsumer {
    std::optional<std::size_t> maximum_; ///< Printed-row limit, independent of queue delivery accounting.
    std::size_t printed_ = 0;            ///< Application-owned count, read only on the same creating thread.
    bool failed_ = false;                ///< Mapping or output failure, observed immediately by the owner loop.

    public:
    /** Store a finite optional output cutoff without retaining an owner or command handle.
     * @param maximum Maximum printed records; duration still supplies an unconditional cutoff.
     */
    explicit QuoteConsumer(std::optional<std::size_t> maximum) : maximum_(maximum) {
    }

    /** Project and print actual received Quotes using the existing one-batch callback contract.
     * @param batch Borrowed generic records and prepared projection plan, valid only during this call.
     * @return CONTINUE acknowledges the batch; PAUSE gates further callbacks after an application failure.
     */
    mux::BatchAction onBatch(const client::SubscriptionBatch &batch) override {
        auto work = projectionWork();

        for (std::size_t index = 0; index < batch.records().size(); ++index) {
            if (maximum_ && printed_ >= *maximum_) {
                break;
            }

            auto projected = batch.project(index, work);

            if (!projected) {
                std::cerr << "Quote projection failed; kind=" << projected.error().index() << " code=";
                std::visit(
                    [](auto cause) {
                        std::cerr << static_cast<int>(cause);
                    },
                    projected.error());
                std::cerr << '\n';
                failed_ = true;

                return mux::BatchAction::PAUSE;
            }

            if (!std::holds_alternative<events::Quote>(projected->value)) {
                std::cerr << "An admitted Quote record has no prepared Quote projection.\n";
                failed_ = true;

                return mux::BatchAction::PAUSE;
            }

            const auto &quote = std::get<events::Quote>(projected->value);
            std::cout << "symbol=";
            printSymbol(quote.symbol);
            std::cout << " bid.raw=" << quote.bidPrice.raw << " bidSize.raw=" << quote.bidSize.raw
                      << " ask.raw=" << quote.askPrice.raw << " askSize.raw=" << quote.askSize.raw << '\n';
            std::cout.flush();

            if (!std::cout) {
                failed_ = true;

                return mux::BatchAction::PAUSE;
            }

            ++printed_;
        }

        return mux::BatchAction::CONTINUE;
    }

    /** Inspect application output progress on the serialized owner.
     * @return Number of successfully printed rows.
     */
    [[nodiscard]] std::size_t printed() const noexcept {
        return printed_;
    }

    /** Inspect a callback's bounded output/projection result.
     * @return True after an application failure; no retry is attempted implicitly.
     */
    [[nodiscard]] bool failed() const noexcept {
        return failed_;
    }
};

/** Preserve the full default Quote field order and explicit aliases in the writer blueprint.
 * @param schema Validated full eight-field default Quote declarations.
 * @return Complete owned direction-local record mapping, before native initialization.
 */
std::vector<protocol::OutgoingRecord> blueprint(const core::LocalSchema &schema) {
    if (schema.records().size() != 1 || schema.records()[0].fields.size() != events::QUOTE_FIELD_TYPES.size()) {
        throw std::runtime_error("The complete default Quote schema is required");
    }

    const auto &record = schema.records()[0];
    protocol::OutgoingRecord output{0, 0, record.name, {}};

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });

        if (lookup == record.lookups.end()) {
            throw std::runtime_error("The default Quote lookup table is incomplete");
        }

        output.fields.push_back({lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    std::vector<protocol::OutgoingRecord> result;
    result.push_back(std::move(output));

    return result;
}

/** Poll only the existing recovery owner, with the same bounded profile during run and shutdown.
 * @param owner Actual prepared plain TCP owner on its creating thread.
 * @param endpoint Immutable explicit native/session settings.
 * @param ticket Accepted desired replacement; completion is distinct from server acknowledgement.
 * @param consumer Strong application callback state, accessed on this same owner thread.
 * @param input Validated duration, output limit and absolute drain allowance.
 * @return Application exit status; zero requires an actual completed DRAINED terminal.
 */
int drive(transport::TcpRecovery &owner, const client::EndpointOptions &endpoint, mux::MuxCommandTicket ticket,
          const QuoteConsumer &consumer, const Arguments &input) {
    const auto cutoff = Clock::now() + input.duration;
    bool draining = false;
    bool aborting = false;
    bool failed = false;
    bool commandObserved = false;
    std::size_t rejectedPolls = 0;
    std::optional<transport::RecoveryPhase> previousPhase;
    std::uint32_t previousAttempts = 0;

    for (std::size_t turns = 0; turns < MAXIMUM_OWNER_TURNS; ++turns) {
        if ((interrupted != 0 || consumer.failed() || failed) && !aborting) {
            const auto stopped = owner.requestStop();
            aborting = true;

            if (!stopped) {
                printRecovery(stopped.error());

                return interrupted ? 130 : 1;
            }
        }

        if (!draining && !aborting &&
            (Clock::now() >= cutoff || (input.maximumRecords && consumer.printed() >= *input.maximumRecords))) {
            const auto stopped = owner.requestDrain(Clock::now() + input.drain);
            draining = true;

            if (!stopped) {
                printRecovery(stopped.error());
                failed = true;
            }
        }

        auto work = recoveryTurn(endpoint);
        auto progressed = owner.poll(work);

        if (!progressed) {
            printRecovery(progressed.error());
            failed = true;

            if (++rejectedPolls >= MAXIMUM_REJECTED_POLLS) {
                std::cerr << "Repeated progress refusal; local quiescence is not certified.\n";

                return interrupted ? 130 : 1;
            }

            continue;
        }

        rejectedPolls = 0;

        if (previousPhase != progressed->phase || previousAttempts != progressed->attempts) {
            std::cerr << "phase=" << phaseName(progressed->phase) << " attempts=" << progressed->attempts << '\n';
            previousPhase = progressed->phase;
            previousAttempts = progressed->attempts;
        }

        if (!commandObserved) {
            const auto command = ticket.result();

            if (command) {
                commandObserved = true;

                if (command->status != mux::CommandStatus::APPLIED) {
                    std::cerr << "Desired replacement was not applied locally; status="
                              << static_cast<int>(command->status) << '\n';
                    failed = true;
                } else {
                    std::cerr << "Subscription applied locally; revision=" << command->revision << '\n';
                }
            }
        }

        if (progressed->terminal) {
            const auto &terminal = *progressed->terminal;
            std::cerr << "Stopped: records=" << consumer.printed() << " attempts=" << progressed->attempts
                      << " outcome=" << static_cast<int>(terminal.outcome)
                      << " deadlineExpired=" << terminal.deadlineExpired << '\n';

            if (progressed->first.error != transport::RecoveryError::NONE) {
                printRecovery(progressed->first);
            }

            if (interrupted != 0) {
                return 130;
            }

            return !failed && !consumer.failed() && commandObserved &&
                           terminal.outcome == mux::TerminalOutcome::DRAINED && !terminal.deadlineExpired
                       ? 0
                       : 1;
        }

        std::this_thread::sleep_for(endpoint.settings().timeouts.poll);
    }

    (void)owner.requestStop();
    std::cerr << "Owner-turn limit reached; local quiescence is not certified.\n";

    return interrupted ? 130 : 1;
}

/** Build the real endpoint/receiver/queue/recovery graph, then explicitly activate its native route.
 * @param input Validated caller-selected endpoint, explicit DNS settings and finite application limits.
 * @return Success only after the requested local drain actually completes.
 */
int subscribe(const Arguments &input) {
    auto settings = client::controlledStreamSettings(input.duration);

    if (!settings) {
        std::cerr << "Controlled endpoint settings were refused.\n";

        return 1;
    }

    settings->host = input.host;
    settings->port = input.port;
    settings->retry = {MAXIMUM_ATTEMPTS, RETRY_DELAY};

    if (input.dnsServer && !cli::numericHost(input.host)) {
        const auto configured = configureDnsProfile(*settings);

        if (!configured) {
            std::cerr << "DNS preparation profile refused; error=" << static_cast<int>(configured.error().error)
                      << " stage=" << configured.error().stage << '\n';

            return 1;
        }
    }

    auto schema = events::defaultQuoteSchema(settings->budgets.schema);

    if (!schema) {
        std::cerr << "Default Quote schema failed; error=" << static_cast<int>(schema.error()) << '\n';

        return 1;
    }

    auto endpoint = client::EndpointOptions::create(*schema, *settings);

    if (!endpoint) {
        std::cerr << "Endpoint refused; error=" << static_cast<int>(endpoint.error().error)
                  << " stage=" << endpoint.error().stage << '\n';

        return 2;
    }

    auto queueCredits = queueWork();
    auto queue = createQueue(*schema, endpoint->settings(), queueCredits);

    if (!queue) {
        std::cerr << "Queue creation failed; error=" << static_cast<int>(queue.error().error) << '\n';

        return 1;
    }

    auto consumer = std::make_shared<QuoteConsumer>(input.maximumRecords);
    const std::array routes{client::PreparedSubscriptionBinding{
        0, client::PreparedEventKind::QUOTE, MAPPING_LIMITS, {MAXIMUM_SYMBOL_UNITS, 0, MAXIMUM_SYMBOL_UNITS}}};
    client::ReceiverWork receiverCredits{100000, 1000000};
    queueCredits = queueWork();
    auto handle =
        client::openSubscriptionReceiver(*queue, *schema, core::SubscriptionFamily::STREAM, SELECTORS, routes, consumer,
                                         {1, 128, 16}, clientLimits(), receiverCredits, queueCredits);

    if (!handle) {
        std::cerr << "Receiver preparation failed before activation; kind=" << handle.error().cause.index() << " code=";
        std::visit(
            [](const auto &cause) {
                if constexpr (std::is_same_v<std::decay_t<decltype(cause)>, mux::QueueFailure>) {
                    std::cerr << static_cast<int>(cause.error);
                } else {
                    std::cerr << static_cast<int>(cause);
                }
            },
            handle.error().cause);
        std::cerr << '\n';

        return 1;
    }

    const core::SubscriptionRequest request{{schema->recordContext(0), input.symbol}, std::nullopt};
    mux::QueueSubmitWork submitCredits{16, 1000000, 1000000, MEBIBYTE};
    auto ticket = handle->replace(std::span(&request, 1), submitCredits);

    if (!ticket) {
        std::cerr << "Desired replacement was not accepted; error=" << static_cast<int>(ticket.error()) << '\n';

        return 1;
    }

    auto declarations = blueprint(*schema);
    transport::RecoveryQueues queues;
    queues.stream.emplace(std::move(*queue));
    auto bootstrap = transport::RuntimeBootstrap::create();

    if (!bootstrap) {
        std::cerr << "Native bootstrap failed; error=" << static_cast<int>(bootstrap.error().error) << '\n';

        return 1;
    }

    auto preparation = preparationWork();
    auto recovery = recoveryOptions();

    if (input.dnsServer) {
        recovery.dns.servers[0] = *input.dnsServer;
        recovery.dns.serverCount = 1;
        recovery.dns.port = input.dnsPort;
    }

    auto owner = transport::TcpRecovery::create(*bootstrap, *endpoint, std::move(declarations), std::move(queues),
                                                recovery, preparation);

    if (!owner) {
        printRecovery(owner.error());

        return 1;
    }

    if (interrupted == 0) {
        const auto started = owner->start();

        if (!started) {
            printRecovery(started.error());

            return 1;
        }
    }

    return drive(*owner, *endpoint, std::move(*ticket), *consumer, input);
}
} // namespace

/** Dispatch side-effect-free help or run one finite console subscription.
 * @param count Raw main argument count, including program name.
 * @param values Borrowed process arguments, unchanged for the complete invocation.
 * @return Documented CLI exit status.
 */
int run(int count, char **values) {
    if (count == 1 || (count == 2 && std::string_view(values[1]) == "--help")) {
        return cli::help(false);
    }

    if (count == 2 && std::string_view(values[1]) == "--help-ru") {
        return cli::help(true);
    }

    const auto input = cli::arguments(count, values);

    if (!input) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    if (std::signal(SIGINT, onInterrupt) == SIG_ERR || std::signal(SIGTERM, onInterrupt) == SIG_ERR) {
        std::cerr << "Cannot install interruption handlers.\n";

        return 1;
    }

#ifdef SIGPIPE

    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::cerr << "Cannot handle output-pipe failures.\n";

        return 1;
    }

#endif

    return subscribe(*input);
}
} // namespace org::ttldtor::qdcxx::examples::quote_subscription

/** Run the standalone consumer using only installed public library interfaces.
 * @param argc Raw argument count.
 * @param argv Borrowed arguments supplied by the process runtime.
 * @return Documented help/run/usage/interruption exit status.
 */
int main(int argc, char **argv) {
    try {
        return org::ttldtor::qdcxx::examples::quote_subscription::run(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "Application failure: " << error.what() << '\n';

        return 1;
    }
}
