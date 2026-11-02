// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_application.hpp"

#include "recovery_output.hpp"
#include "recovery_preparation.hpp"

#include "qd/transport/tcp_recovery.hpp"
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
#    include "qd/transport/tls_recovery.hpp"
#    include "tls_host_preflight.hpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <streambuf>
#include <string_view>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::connect {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t MAXIMUM_SYMBOLS = 16; ///< Same finite selection ceiling as the recovery grammar/profile.
constexpr std::size_t MAXIMUM_CONTROL_ROW_BYTES = 4096; ///< Fixed presentation backing; no peer text is rendered.
constexpr std::size_t MAXIMUM_OWNER_TURNS = 1000000;    ///< Caller-loop guard, including unchanged-profile retirement.
constexpr std::size_t MAXIMUM_REJECTED_POLLS = 2;       ///< Repeated profile refusal cannot certify local quiescence.

/** Bounded synchronous row staging; overflow becomes an ordinary checked stream failure. */
class ControlRow final : public std::streambuf {
    std::array<char, MAXIMUM_CONTROL_ROW_BYTES> bytes_{};

    public:
    /** Expose only the fixed application-owned character storage. */
    ControlRow() {
        setp(bytes_.data(), bytes_.data() + bytes_.size());
    }

    /** Inspect bytes already published by a versioned evidence writer.
     * @return Borrow valid until this staging object dies.
     */
    std::string_view view() const noexcept {
        return {pbase(), static_cast<std::size_t>(pptr() - pbase())};
    }
};

/** Last successfully published presentation row, never an authoritative range/cache snapshot. */
struct PublishedRow {
    std::array<char, MAXIMUM_CONTROL_ROW_BYTES> bytes{};
    std::size_t size = 0;

    /** Publish a changed complete row before replacing this bounded presentation copy.
     * @param output Actual borrowed diagnostic sink.
     * @param writer Existing versioned writer; invoked only against bounded local staging.
     * @return True when a changed row was completely flushed.
     * @throws std::runtime_error Static formatting/output failure; previous copy is preserved.
     */
    template <class Writer> bool publish(std::ostream &output, Writer writer) {
        ControlRow buffer;
        std::ostream staged(&buffer);
        writer(staged);
        const auto row = buffer.view();

        if (row == std::string_view(bytes.data(), size)) {
            return false;
        }

        output.write(row.data(), static_cast<std::streamsize>(row.size()));
        output.flush();

        if (!output) {
            throw std::runtime_error("recovery diagnostic sink failure");
        }

        std::memcpy(bytes.data(), row.data(), row.size());
        size = row.size();

        return true;
    }
};

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
/** One fixed TLS profile; the independent host preflight and later runtime share one retained SDK scope. */
struct TlsExecution {
    transport::TlsOptions options;
    transport::TlsRequirements requirements;
    transport::TlsWorkVector turn;
};

/** Admit the complete closed-host initialization before any trust decode or TLS runtime factory.
 * Wrapper capacity is an explicit application ceiling checked by the public factory, not an opaque sizeof claim.
 * @return Original bounded host refusal, or one same-shape immutable runtime profile.
 */
std::expected<TlsExecution, closed_tls::PreflightDiagnostic> prepareTlsExecution(const PreparedRecovery &profile) {
    using Error = closed_tls::PreflightError;

    if (!profile.tlsLimits || !profile.endpoint.settings().tls) {
        return std::unexpected(closed_tls::PreflightDiagnostic{Error::RESOURCE_SHAPE, "preflight.cli-input"});
    }

    auto need = closed_tls::preflightRequirements(*profile.tlsLimits);

    if (!need) {
        return std::unexpected(need.error());
    }

    constexpr std::size_t TLS_ADAPTER_BYTES =
        256 * 1024; ///< Fixed wrapper/token ceiling; actual factory admission remains authoritative.
    const auto &settings = profile.endpoint.settings();
    const auto &tls = *settings.tls;
    std::size_t resident = need->tls.sdk.totalBytes;
    const std::array additions{TLS_ADAPTER_BYTES,
                               profile.endpoint.schema().storageBytes(),
                               sizeof(client::EndpointSettings),
                               alignof(std::max_align_t),
                               4 * sizeof(void *),
                               settings.host.capacity() + 1,
                               tls.referenceIdentity.capacity() + 1,
                               tls.trustPem.capacity() + 1,
                               settings.budgets.tcp.residentBytes};

    for (const auto amount : additions) {
        if (amount > std::numeric_limits<std::size_t>::max() - resident) {
            return std::unexpected(closed_tls::PreflightDiagnostic{Error::RESOURCE_LIMIT, "preflight.cli-resident"});
        }

        resident += amount;
    }

    closed_tls::PreflightOnce once;
    closed_tls::PreflightCapacity capacity{need->tls.sdk.sharedBytes};
    closed_tls::PreflightWork work{need->tls.shape, need->preflightRevision, need->work};
    auto checked = closed_tls::checkClosedTlsHost(once, *profile.tlsLimits, capacity, work);

    if (!checked) {
        return std::unexpected(checked.error());
    }

    transport::TlsOptions options;
    options.limits = *profile.tlsLimits;
    options.host = checked->limits;
    options.sdkInstanceBytes = need->tls.sdk.instanceBytes;
    options.sdkSharedBytes = checked->reservedSharedBytes;
    options.residentBytes = resident;
    auto turn = need->tls.factoryAdmission;
    const std::array alternatives{need->tls.initialStart, need->tls.entry, need->tls.close};

    for (const auto &alternative : alternatives) {
        for (std::size_t coordinate = 0; coordinate < turn.units.size(); ++coordinate) {
            turn.units[coordinate] = std::max(turn.units[coordinate], alternative.units[coordinate]);
        }
    }

    return TlsExecution{options, need->tls, turn};
}
#endif

/** One finite ordinary command observation, independent of source/wire acknowledgement. */
struct PendingCommand {
    mux::MuxCommandTicket ticket;
    bool observed = false;
    bool applied = false;
};

/** Preserve a control sink failure while still allowing the actual owner to retire. */
struct ControlPublication {
    std::ostream &output;
    bool failed = false;

    /** Attempt one finite publication; after failure no later write or ACK is permitted.
     * @param writer Direct typed evidence writer, without owner progress or callbacks.
     */
    template <class Writer> void publish(Writer writer) noexcept {
        if (failed) {
            return;
        }

        try {
            writer(output);
        } catch (...) {
            failed = true;
        }
    }
};

/** Observe an actual detached command completion once.
 * @return True only for an authoritative rejection; closing cancellation is classified at terminal.
 */
bool observeCommand(PendingCommand &pending, RecoveryOutputCommand kind, ControlPublication &control) {
    if (pending.observed) {
        return false;
    }

    const auto result = pending.ticket.result();

    if (!result) {
        return false;
    }

    pending.observed = true;
    pending.applied = result->status == mux::CommandStatus::APPLIED;
    control.publish([&](std::ostream &out) {
        writeRecoveryCommandResult(out, kind, *result);
    });

    return result->status == mux::CommandStatus::REJECTED;
}

/** Find the actual adopted-family terminal coordinate without relying on unrelated enum ordinals. */
std::size_t familySlot(core::SubscriptionFamily family) {
    switch (family) {
    case core::SubscriptionFamily::TICKER:
        return static_cast<std::size_t>(transport::RecoveryFamily::TICKER);
    case core::SubscriptionFamily::STREAM:
        return static_cast<std::size_t>(transport::RecoveryFamily::STREAM);
    case core::SubscriptionFamily::HISTORY:
        return static_cast<std::size_t>(transport::RecoveryFamily::HISTORY);
    }

    throw std::runtime_error("unsupported recovery family");
}

/** Classify only actual frozen retirement; elapsed duration alone never establishes success. */
int classifyTerminal(const transport::RecoveryTerminal &terminal, bool failed, bool interrupted, bool active,
                     bool commandsApplied, bool targetMet) noexcept {
    if (failed || terminal.outcome == mux::TerminalOutcome::FAILED || terminal.deadlineExpired) {
        return 1;
    }

    if (interrupted) {
        return terminal.outcome == mux::TerminalOutcome::ABORTED ? 130 : 1;
    }

    if (terminal.outcome != mux::TerminalOutcome::DRAINED) {
        return 1;
    }

    return active && commandsApplied && targetMet ? 0 : 2;
}

/** Observe current HISTORY evidence using one distinct debited allowance for the whole turn. */
template <class Owner>
void observeHistory(Owner &owner, const RecoveryConfiguration &config, const PreparedRecovery &profile,
                    const mux::MuxHandle &handle, std::span<const core::SubscriptionKey> keys,
                    RecoveryEpochLabels &epochs, std::array<PublishedRow, MAXIMUM_SYMBOLS> &rows,
                    ControlPublication &control) {
    if (config.family != core::SubscriptionFamily::HISTORY || control.failed) {
        return;
    }

    auto work = freshHistoryObservationWork(profile);

    for (std::size_t index = 0; index < keys.size() && !control.failed; ++index) {
        const auto observed = owner.historyRangeStatus(handle, keys[index], work);
        control.publish([&](std::ostream &out) {
            rows[index].publish(out, [&](std::ostream &staged) {
                if (observed) {
                    writeRecoveryHistory(staged, epochs, config.recordId, keys[index].symbol, *observed);
                } else {
                    writeRecoveryHistoryUnavailable(staged, config.recordId, keys[index].symbol, *config.historyFromRaw,
                                                    observed.error());
                }
            });
        });
    }
}

/** Drive real recovery while keeping presentation/control failures separate from native retirement. */
template <class Owner, class Poll>
int drive(Owner &owner, Poll poll, const RecoveryConfiguration &config, const PreparedRecovery &profile,
          mux::MuxHandle &handle, std::span<const core::SubscriptionKey> keys, PendingCommand initial,
          const RecoveryOutputConsumer &consumer, RecoveryEpochLabels &epochs, const volatile std::sig_atomic_t *signal,
          std::ostream &diagnostics, Clock::time_point started) {
    ControlPublication control{diagnostics};
    PublishedRow statusRow;
    PublishedRow resetRow;
    std::array<PublishedRow, MAXIMUM_SYMBOLS> historyRows;
    std::optional<PendingCommand> removal;
    std::optional<std::uint64_t> acknowledged;
    const auto cutoff = started + config.duration;
    bool draining = false;
    bool aborting = false;
    bool failed = false;
    bool active = false;
    bool interrupted = false;
    std::size_t rejectedPolls = 0;

    for (std::size_t turn = 0; turn < MAXIMUM_OWNER_TURNS; ++turn) {
        const auto now = Clock::now();
        interrupted = interrupted || (signal != nullptr && *signal != 0);
        failed = failed || consumer.failed() || control.failed;

        if ((failed || interrupted) && !aborting) {
            const auto stopped = owner.requestStop();
            aborting = true;

            if (!stopped) {
                failed = true;
                control.publish([&](std::ostream &out) {
                    writeRecoveryDiagnostic(out, RecoveryOutputDiagnostic::TURN, stopped.error());
                });
            }
        }

        if (!draining && !aborting &&
            (now >= cutoff || (config.maximumRecords && consumer.completedRecords() >= *config.maximumRecords))) {
            const auto stopped = owner.requestDrain(now + config.drain);
            draining = true;

            if (!stopped) {
                failed = true;
                control.publish([&](std::ostream &out) {
                    writeRecoveryDiagnostic(out, RecoveryOutputDiagnostic::TURN, stopped.error());
                });
            }
        }

        if (!draining && !aborting && config.removeAfter && !removal && now >= started + *config.removeAfter) {
            auto credits = profile.submitWork;
            auto ticket = handle.remove(keys, credits);

            if (ticket) {
                removal.emplace(PendingCommand{std::move(*ticket)});
                control.publish([&](std::ostream &out) {
                    writeRecoveryCommandAccepted(out, RecoveryOutputCommand::REMOVE_INITIAL, removal->ticket);
                });
            } else {
                failed = true;
                control.publish([](std::ostream &out) {
                    out << "qdcxx-connect: recovery removal submission failed\n";
                    out.flush();

                    if (!out) {
                        throw std::runtime_error("recovery diagnostic sink failure");
                    }
                });
            }
        }

        auto work = freshRecoveryTurn(profile);
        auto status = poll(work, control);

        if (!status) {
            failed = true;
            control.publish([&](std::ostream &out) {
                writeRecoveryDiagnostic(out, RecoveryOutputDiagnostic::TURN, status.error());
            });

            if (++rejectedPolls >= MAXIMUM_REJECTED_POLLS) {
                control.publish([](std::ostream &out) {
                    out << "qdcxx-connect: repeated progress refusal; local retirement is not certified\n";
                    out.flush();
                });

                return 1;
            }

            continue;
        }

        rejectedPolls = 0;
        active = active || status->phase == transport::RecoveryPhase::ACTIVE;
        control.publish([&](std::ostream &out) {
            statusRow.publish(out, [&](std::ostream &staged) {
                writeRecoveryStatus(staged, *status);
            });
        });
        failed = observeCommand(initial, RecoveryOutputCommand::INITIAL_REPLACE, control) || failed;

        if (removal) {
            failed = observeCommand(*removal, RecoveryOutputCommand::REMOVE_INITIAL, control) || failed;
        }

        if (const auto notice = handle.resetNotice(); notice && !control.failed) {
            bool changed = false;
            control.publish([&](std::ostream &out) {
                changed = resetRow.publish(out, [&](std::ostream &staged) {
                    writeRecoveryReset(staged, epochs, *notice);
                });
            });

            if (!control.failed && (changed || acknowledged != notice->sequence)) {
                const auto accepted = handle.acknowledgeReset(notice->sequence);

                if (accepted == mux::ControlAdmission::RECORDED || accepted == mux::ControlAdmission::COALESCED) {
                    acknowledged = notice->sequence;
                }

                control.publish([&](std::ostream &out) {
                    writeRecoveryResetAcknowledgement(out, notice->sequence, accepted);
                });
            }
        }

        observeHistory(owner, config, profile, handle, keys, epochs, historyRows, control);

        if (const auto local = handle.terminal(); local && local->outcome == mux::TerminalOutcome::FAILED) {
            failed = true;
        }

        if (status->terminal) {
            control.publish([&](std::ostream &out) {
                writeRecoveryTerminal(out, *status->terminal);
                const auto &family = status->terminal->queues[familySlot(config.family)];

                if (family) {
                    writeRecoveryQueueTerminal(out, epochs, config.family, *family);
                }

                writeRecoveryDiagnostic(out, RecoveryOutputDiagnostic::FIRST, status->first);
                writeRecoveryDiagnostic(out, RecoveryOutputDiagnostic::SECONDARY, status->secondary);
            });
            const bool commandsApplied = initial.applied && (!config.removeAfter || (removal && removal->applied));
            const bool targetMet = !config.maximumRecords || consumer.completedRecords() >= *config.maximumRecords;

            return classifyTerminal(*status->terminal, failed || control.failed || consumer.failed(), interrupted,
                                    active, commandsApplied, targetMet);
        }

        auto wake = Clock::now() + profile.endpoint.settings().timeouts.poll;

        if (const auto native = owner.nextWake(); native && *native < wake) {
            wake = *native;
        }

        if (!draining && !aborting) {
            wake = std::min(wake, cutoff);

            if (config.removeAfter && !removal) {
                wake = std::min(wake, started + *config.removeAfter);
            }
        }

        if (wake > Clock::now()) {
            std::this_thread::sleep_until(wake);
        }
    }

    static_cast<void>(owner.requestStop());
    control.publish([](std::ostream &out) {
        out << "qdcxx-connect: owner turn limit; local retirement is not certified\n";
        out.flush();
    });

    return 1;
}
} // namespace

int runRecoveryApplication(const RecoveryConfiguration &config, PreparedRecovery &profile,
                           const volatile std::sig_atomic_t *interrupted, std::ostream &output,
                           std::ostream &diagnostics) {
    if (config.symbols.empty() || config.symbols.size() > MAXIMUM_SYMBOLS ||
        (config.family == core::SubscriptionFamily::HISTORY && !config.historyFromRaw)) {
        return 2;
    }

    auto credits = freshQueueWork(profile);
    auto queue = createSelectedQueue(profile, config, credits);

    if (!queue) {
        diagnostics << "qdcxx-connect: recovery queue preparation failed\n";

        return 1;
    }

    RecoveryEpochLabels epochs(config.attempts, profile.epochLabelBytes);
    auto consumer = std::make_shared<RecoveryOutputConsumer>(output, epochs, profile.schema, config.recordId,
                                                             config.family, config.output, profile.projectionWork);
    auto receiverCredits = profile.receiverWork;
    credits = freshQueueWork(profile);
    auto handle = client::openSubscriptionReceiver(*queue, profile.schema, config.family, config.selectors,
                                                   profile.receiverBindings, consumer, profile.receiverLimits,
                                                   profile.clientLimits, receiverCredits, credits);

    if (!handle) {
        diagnostics << "qdcxx-connect: recovery receiver preparation failed\n";

        return 1;
    }

    std::vector<core::SubscriptionRequest> requests;
    std::vector<core::SubscriptionKey> keys;
    requests.reserve(config.symbols.size());
    keys.reserve(config.symbols.size());

    for (const auto &symbol : config.symbols) {
        keys.push_back({profile.selectedContext, symbol});
        requests.push_back({keys.back(), config.historyFromRaw});
    }

    auto submission = profile.submitWork;
    auto initial = handle->replace(requests, submission);

    if (!initial) {
        diagnostics << "qdcxx-connect: recovery replacement submission failed\n";

        return 1;
    }

    consumer->begin();
    writeRecoveryCommandAccepted(diagnostics, RecoveryOutputCommand::INITIAL_REPLACE, *initial);
    transport::RecoveryQueues queues;

    switch (config.family) {
    case core::SubscriptionFamily::TICKER:
        queues.ticker.emplace(std::move(*queue));
        break;
    case core::SubscriptionFamily::STREAM:
        queues.stream.emplace(std::move(*queue));
        break;
    case core::SubscriptionFamily::HISTORY:
        queues.history.emplace(std::move(*queue));
        break;
    }

    auto bootstrap = transport::RuntimeBootstrap::create();

    if (!bootstrap) {
        diagnostics << "qdcxx-connect: recovery bootstrap failed\n";

        return 1;
    }

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)

    if (config.security == client::EndpointSecurity::TLS) {
        auto tls = prepareTlsExecution(profile);

        if (!tls) {
            diagnostics << "qdcxx-connect: TLS host preparation failed code=" << static_cast<int>(tls.error().error)
                        << '\n';

            return 1;
        }

        auto owner = transport::TlsRecovery::create(*bootstrap, profile.endpoint, std::move(profile.blueprint),
                                                    std::move(queues), profile.recoveryOptions, tls->options,
                                                    profile.preparationWork);

        if (!owner) {
            writeRecoveryDiagnostic(diagnostics, RecoveryOutputDiagnostic::TURN, owner.error());

            return 1;
        }

        PublishedRow firstTls;
        PublishedRow secondaryTls;
        const auto started = Clock::now();
        const auto active = owner->start();

        if (!active) {
            writeRecoveryDiagnostic(diagnostics, RecoveryOutputDiagnostic::TURN, active.error());

            return 1;
        }

        const auto result = drive(
            *owner,
            [&](transport::RecoveryTurnWork &work, ControlPublication &control)
                -> std::expected<transport::RecoveryStatus, transport::RecoveryDiagnostic> {
                transport::TlsWork tlsWork{tls->requirements.shape, tls->turn};
                auto status = owner->poll(work, tlsWork);

                if (!status) {
                    return std::unexpected(status.error());
                }

                control.publish([&](std::ostream &out) {
                    firstTls.publish(out, [&](std::ostream &staged) {
                        writeRecoveryTlsDiagnostic(staged, RecoveryOutputDiagnostic::FIRST, status->first);
                    });
                    secondaryTls.publish(out, [&](std::ostream &staged) {
                        writeRecoveryTlsDiagnostic(staged, RecoveryOutputDiagnostic::SECONDARY, status->secondary);
                    });
                });

                return std::move(status->recovery);
            },
            config, profile, *handle, keys, PendingCommand{std::move(*initial)}, *consumer, epochs, interrupted,
            diagnostics, started);
        consumer->flush();

        return result;
    }

#endif

    auto owner = transport::TcpRecovery::create(*bootstrap, profile.endpoint, std::move(profile.blueprint),
                                                std::move(queues), profile.recoveryOptions, profile.preparationWork);

    if (!owner) {
        writeRecoveryDiagnostic(diagnostics, RecoveryOutputDiagnostic::TURN, owner.error());

        return 1;
    }

    const auto started = Clock::now();
    const auto active = owner->start();

    if (!active) {
        writeRecoveryDiagnostic(diagnostics, RecoveryOutputDiagnostic::TURN, active.error());

        return 1;
    }

    const auto result = drive(
        *owner,
        [&](transport::RecoveryTurnWork &work, ControlPublication &) {
            return owner->poll(work);
        },
        config, profile, *handle, keys, PendingCommand{std::move(*initial)}, *consumer, epochs, interrupted,
        diagnostics, started);
    consumer->flush();

    return result;
}
} // namespace org::ttldtor::qdcxx::connect
