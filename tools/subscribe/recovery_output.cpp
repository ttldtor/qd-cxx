// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_output.hpp"

#include <algorithm>
#include <charconv>
#include <concepts>
#include <iterator>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::string_view HEX_DIGITS = "0123456789ABCDEF"; ///< Locale-independent exact uppercase alphabet.

struct TextHex {
    std::u16string_view value;
}; ///< Call-local exact UTF-16 borrow, never retained.

struct BytesHex {
    std::span<const std::byte> value;
}; ///< Call-local exact opaque-byte borrow.

/** Write literal protocol text without inheriting caller numeric/locale stream settings.
 * @param output Borrowed stream.
 * @param value Fixed bounded ASCII token.
 */
void cell(std::ostream &output, std::string_view value) {
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

/** Write one unambiguous absent value.
 * @param output Borrowed stream.
 */
void cell(std::ostream &output, std::nullopt_t) {
    cell(output, std::string_view("null"));
}

/** Write a boolean independently of boolalpha.
 * @param output Borrowed stream.
 * @param value Actual observation.
 */
void cell(std::ostream &output, bool value) {
    output.put(value ? '1' : '0');
}

/** Write an exact base-ten integer independently of flags, locale and signedness.
 * @tparam Integer Integral scalar other than bool.
 * @param output Borrowed stream.
 * @param value Actual scalar.
 */
template <std::integral Integer>
    requires(!std::same_as<Integer, bool>)
void cell(std::ostream &output, Integer value) {
    char buffer[std::numeric_limits<Integer>::digits10 + 3];
    const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);

    if (converted.ec != std::errc{})
        throw std::runtime_error("recovery output integer failure");

    output.write(buffer, converted.ptr - buffer);
}

/** Write the existing typed enumeration's numeric value, without reassigning error identities.
 * @tparam Enum Existing enumeration.
 * @param output Borrowed stream.
 * @param value Actual enum value.
 */
template <class Enum>
    requires std::is_enum_v<Enum>
void cell(std::ostream &output, Enum value) {
    cell(output, std::to_underlying(value));
}

/** Write exact code units, including empty/unpaired UTF-16, without Unicode conversion.
 * @param output Borrowed stream.
 * @param value Callback/call-local borrowed units.
 */
void cell(std::ostream &output, TextHex value) {
    for (const auto unit : value.value) {
        for (int shift = 12; shift >= 0; shift -= 4)
            output.put(HEX_DIGITS[(unit >> shift) & 15]);
    }
}

/** Write every opaque byte without parsing, escaping or discarding zero bytes.
 * @param output Borrowed stream.
 * @param value Callback/call-local borrowed bytes.
 */
void cell(std::ostream &output, BytesHex value) {
    for (const auto byte : value.value) {
        const auto number = std::to_integer<unsigned>(byte);
        output.put(HEX_DIGITS[number >> 4]);
        output.put(HEX_DIGITS[number & 15]);
    }
}

/** Preserve optional absence independently of an engaged empty or numeric zero.
 * @tparam Value Supported scalar/hex presentation.
 * @param output Borrowed stream.
 * @param value Actual optional value.
 */
template <class Value> void cell(std::ostream &output, const std::optional<Value> &value) {
    if (value)
        cell(output, *value);
    else
        cell(output, std::nullopt);
}

/** Emit exactly one TSV row; data-bearing text is supplied only through explicit hex wrappers.
 * @tparam Values Fixed field types.
 * @param output Borrowed stream.
 * @param name Static row name.
 * @param values Exact cells in the paired row specification.
 */
template <class... Values> void row(std::ostream &output, std::string_view name, const Values &...values) {
    cell(output, name);
    ((output.put('\t'), cell(output, values)), ...);
    output.put('\n');
}

/** Flush/check one complete publication unit and erase arbitrary stream exception text.
 * @tparam Write Synchronous writer, retaining no callback borrow.
 * @param output Borrowed sink.
 * @param write Complete row or whole-batch operation.
 * @throws std::runtime_error Static failure, with no rollback/retry claim.
 */
template <class Write> void publish(std::ostream &output, Write &&write) {
    try {
        write();
        output.flush();

        if (!output)
            throw std::runtime_error("recovery output sink failure");
    } catch (...) {
        throw std::runtime_error("recovery output publication failure");
    }
}

/** Name the actual explicitly selected family using a finite static vocabulary.
 * @param family Immutable selected family.
 * @return Static lowercase name.
 */
std::string_view familyName(core::SubscriptionFamily family) {
    switch (family) {
    case core::SubscriptionFamily::STREAM:
        return "stream";
    case core::SubscriptionFamily::TICKER:
        return "ticker";
    case core::SubscriptionFamily::HISTORY:
        return "history";
    }

    throw std::runtime_error("recovery output family failure");
}

/** Name an explicit prepared output selection without inspecting record names.
 * @param profile Selected frontend presentation.
 * @return Static lowercase name.
 */
std::string_view profileName(OutputProfile profile) {
    switch (profile) {
    case OutputProfile::GENERIC:
        return "generic";
    case OutputProfile::QUOTE:
        return "quote";
    case OutputProfile::PROFILE:
        return "profile";
    }

    throw std::runtime_error("recovery output profile failure");
}

/** Name a frontend command without accepting arbitrary strings.
 * @param command Actual frontend operation kind.
 * @return Static protocol name.
 */
std::string_view commandName(RecoveryOutputCommand command) {
    switch (command) {
    case RecoveryOutputCommand::INITIAL_REPLACE:
        return "initial-replace";
    case RecoveryOutputCommand::REMOVE_INITIAL:
        return "remove-initial";
    }

    throw std::runtime_error("recovery output command failure");
}

/** Name a detached diagnostic scope.
 * @param scope Actual frontend observation boundary.
 * @return Static protocol name.
 */
std::string_view diagnosticName(RecoveryOutputDiagnostic scope) {
    switch (scope) {
    case RecoveryOutputDiagnostic::FIRST:
        return "first";
    case RecoveryOutputDiagnostic::SECONDARY:
        return "secondary";
    case RecoveryOutputDiagnostic::TURN:
        return "turn";
    case RecoveryOutputDiagnostic::HISTORY_QUERY:
        return "history-query";
    }

    throw std::runtime_error("recovery output diagnostic failure");
}

/** Inspect one actual outer queue cause.
 * @param failure Actual optional queue failure.
 * @return Existing enum or absence.
 */
std::optional<mux::QueueError> queueError(const std::optional<mux::QueueFailure> &failure) {
    return failure ? std::optional(failure->error) : std::nullopt;
}

/** Preserve the discriminant of an actual nested queue cause.
 * @param failure Actual optional failure.
 * @return Existing QueueSourceError variant position, or absence.
 */
std::optional<std::size_t> sourceKind(const std::optional<mux::QueueFailure> &failure) {
    return failure && failure->source ? std::optional(failure->source->index()) : std::nullopt;
}

/** Preserve the actual typed nested error code without formatting arbitrary source text.
 * @param failure Actual optional failure.
 * @return Exact underlying code under sourceKind's discriminant, or absence.
 */
std::optional<std::int32_t> sourceError(const std::optional<mux::QueueFailure> &failure) {
    if (!failure || !failure->source)
        return std::nullopt;

    return std::visit(
        [](auto error) {
            return static_cast<std::int32_t>(std::to_underlying(error));
        },
        *failure->source);
}

/** Copy no text while preserving optional empty UTF-16 values.
 * @param value Borrowed nullable units.
 * @return Call-local presentation wrapper.
 */
std::optional<TextHex> textHex(std::optional<std::u16string_view> value) {
    return value ? std::optional(TextHex{*value}) : std::nullopt;
}

/** Copy no bytes while preserving optional empty byte values.
 * @param value Borrowed nullable bytes.
 * @return Call-local presentation wrapper.
 */
std::optional<BytesHex> bytesHex(std::optional<std::span<const std::byte>> value) {
    return value ? std::optional(BytesHex{*value}) : std::nullopt;
}

/** Publish original source and trigger facts separately, including explicit absence.
 * @param output Borrowed sink.
 * @param record Actual callback projection.
 * @param ordinal Run-local record coordinate.
 */
void provenance(std::ostream &output, const mux::MuxRecordView &record, std::uint64_t ordinal) {
    if (record.source) {
        const auto &source = *record.source;
        row(output, "source", ordinal, true, source.senderId, source.cipher, source.flags, source.bindingGeneration,
            source.removalTime, source.fieldTime ? std::optional(source.fieldTime->rawSeconds) : std::nullopt,
            source.fieldTime ? source.fieldTime->rawSequence : std::nullopt,
            source.fieldTime ? std::optional(source.fieldTime->sourceOrdinal) : std::nullopt);
    } else {
        row(output, "source", ordinal, false, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt);
    }

    if (record.trigger) {
        const auto &trigger = *record.trigger;
        row(output, "trigger", ordinal, true, trigger.senderId, trigger.cipher, trigger.flags,
            trigger.bindingGeneration, trigger.rawTime,
            trigger.fieldTime ? std::optional(trigger.fieldTime->rawSeconds) : std::nullopt,
            trigger.fieldTime ? trigger.fieldTime->rawSequence : std::nullopt,
            trigger.fieldTime ? std::optional(trigger.fieldTime->sourceOrdinal) : std::nullopt);
    } else {
        row(output, "trigger", ordinal, false, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt, std::nullopt);
    }
}

/** Publish an explicit existing prepared projection, never an inferred event type.
 * @param output Borrowed sink.
 * @param profile Caller-selected output type.
 * @param projection Independently owned callback-local receiver result.
 * @param ordinal Run-local record coordinate.
 */
void prepared(std::ostream &output, OutputProfile profile, const client::SubscriptionProjection &projection,
              std::uint64_t ordinal) {
    const std::string_view origin = projection.eventTimeMs ? "field" : "unspecified";

    if (profile == OutputProfile::QUOTE) {
        const auto *quote = std::get_if<events::Quote>(&projection.value);

        if (!quote)
            throw std::runtime_error("recovery output Quote projection failure");

        row(output, "quote", ordinal, TextHex{quote->symbol}, quote->bidTimeMs,
            static_cast<std::uint16_t>(quote->bidExchangeCode), quote->bidPrice.raw, quote->bidSize.raw,
            quote->askTimeMs, static_cast<std::uint16_t>(quote->askExchangeCode), quote->askPrice.raw,
            quote->askSize.raw, quote->timeMs, quote->sequence, quote->timeNanoPart, origin, projection.eventTimeMs);
    } else if (profile == OutputProfile::PROFILE) {
        const auto *value = std::get_if<events::Profile>(&projection.value);

        if (!value)
            throw std::runtime_error("recovery output Profile projection failure");

        row(output, "profile", ordinal, TextHex{value->symbol}, value->beta.raw, value->earningsPerShare.raw,
            value->dividendFrequency.raw, value->exDividendAmount.raw, value->exDividendDayId,
            value->high52WeekPrice.raw, value->low52WeekPrice.raw, value->shares.raw, value->freeFloat.raw,
            value->highLimitPrice.raw, value->lowLimitPrice.raw, value->haltStartTimeMs, value->haltEndTimeMs,
            value->flags,
            textHex(value->description ? std::optional<std::u16string_view>(*value->description) : std::nullopt),
            textHex(value->statusReason ? std::optional<std::u16string_view>(*value->statusReason) : std::nullopt),
            origin, projection.eventTimeMs);
    }
}
} // namespace

RecoveryEpochLabels::RecoveryEpochLabels(std::uint32_t maximumAttempts, std::size_t maximumBytes) : limit_(0) {
    const auto slots = static_cast<std::uint64_t>(maximumAttempts) + 1;

    if (maximumAttempts == 0 || slots > std::numeric_limits<std::size_t>::max() ||
        slots > maximumBytes / sizeof(mux::QueueIdentity))
        throw std::runtime_error("recovery output epoch capacity failure");

    limit_ = static_cast<std::size_t>(slots);
    epochs_.reserve(limit_);

    if (epochs_.capacity() > maximumBytes / sizeof(mux::QueueIdentity))
        throw std::runtime_error("recovery output epoch capacity failure");
}

std::optional<std::uint64_t> RecoveryEpochLabels::label(const mux::QueueIdentity &epoch) {
    if (!epoch.matches(epoch))
        return std::nullopt;

    for (std::size_t index = 0; index < epochs_.size(); ++index)
        if (epochs_[index].matches(epoch))
            return static_cast<std::uint64_t>(index) + 1;

    if (epochs_.size() == limit_)
        throw std::runtime_error("recovery output epoch capacity failure");

    epochs_.push_back(epoch);

    return static_cast<std::uint64_t>(epochs_.size());
}

RecoveryOutputConsumer::RecoveryOutputConsumer(std::ostream &output, RecoveryEpochLabels &epochs,
                                               const core::LocalSchema &schema, std::int32_t localId,
                                               core::SubscriptionFamily family, OutputProfile profile,
                                               client::ReceiverProjectionWork projectionPerBatch)
    : output_(output), epochs_(epochs), context_(schema.recordContext(localId)), family_(family), localId_(localId),
      profile_(profile), projectionPerBatch_(projectionPerBatch), fieldCount_(0), compactOnly_(true) {
    (void)familyName(family);
    (void)profileName(profile);

    if (!context_.valid())
        throw std::runtime_error("recovery output record selection failure");

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    fieldCount_ = record.fields.size();
    compactOnly_ = std::ranges::all_of(record.fields, [](const auto &field) {
        return field.serialTypeId == std::to_underlying(core::SerialFieldType::COMPACT_INT);
    });
}

void RecoveryOutputConsumer::begin() {
    if (begun_ || failed_)
        throw std::runtime_error("recovery output lifecycle failure");

    try {
        publish(output_, [&] {
            row(output_, "qdcxx-subscriptions", 1);
            row(output_, "selection", familyName(family_), localId_, profileName(profile_));
        });
        begun_ = true;
    } catch (...) {
        failed_ = true;
        throw std::runtime_error("recovery output begin failure");
    }
}

mux::BatchAction RecoveryOutputConsumer::onBatch(const client::SubscriptionBatch &batch) {
    if (!begun_ || failed_)
        throw std::runtime_error("recovery output lifecycle failure");

    try {
        const auto records = batch.records();

        if (records.size() > std::numeric_limits<std::uint64_t>::max() - ordinal_)
            throw std::runtime_error("recovery output record ordinal failure");

        for (const auto &record : records) {
            if (!record.context.matches(context_) || record.family != family_ || !record.epoch.matches(record.epoch))
                throw std::runtime_error("recovery output provenance failure");

            if (record.typedValues.empty()) {
                if (!compactOnly_ || record.values.size() != fieldCount_)
                    throw std::runtime_error("recovery output field shape failure");
            } else if (!record.values.empty() || record.typedValues.size() != fieldCount_) {
                throw std::runtime_error("recovery output field shape failure");
            }
        }

        auto projectionWork = projectionPerBatch_;
        publish(output_, [&] {
            for (std::size_t index = 0; index < records.size(); ++index) {
                const auto &record = records[index];
                std::optional<client::SubscriptionProjection> projection;

                if (profile_ != OutputProfile::GENERIC) {
                    auto result = batch.project(index, projectionWork);

                    if (!result)
                        throw std::runtime_error("recovery output projection failure");

                    projection.emplace(std::move(*result));
                }

                const auto ordinal = ++ordinal_;
                row(output_, "record", ordinal, familyName(family_), localId_, epochs_.label(record.epoch),
                    record.revision, record.admissionRevision, TextHex{record.symbol}, record.rawTime,
                    record.eventFlags, fieldCount_);
                provenance(output_, record, ordinal);

                for (std::size_t field = 0; field < record.typedValues.size(); ++field) {
                    const auto &value = record.typedValues[field];
                    row(output_, "field", ordinal, field, value.type, value.raw, textHex(value.units),
                        bytesHex(value.bytes));
                }

                if (record.typedValues.empty()) {
                    for (std::size_t field = 0; field < record.values.size(); ++field)
                        row(output_, "field", ordinal, field, core::SerialFieldType::COMPACT_INT, record.values[field],
                            std::nullopt, std::nullopt);
                }

                if (projection)
                    prepared(output_, profile_, *projection, ordinal);
            }
        });
        completedRecords_ = ordinal_;

        return mux::BatchAction::CONTINUE;
    } catch (...) {
        failed_ = true;
        throw std::runtime_error("recovery output callback failure");
    }
}

std::uint64_t RecoveryOutputConsumer::completedRecords() const noexcept {
    return completedRecords_;
}

bool RecoveryOutputConsumer::failed() const noexcept {
    return failed_;
}

void RecoveryOutputConsumer::flush() {
    if (!begun_ || failed_)
        throw std::runtime_error("recovery output lifecycle failure");

    try {
        publish(output_, [] {
        });
    } catch (...) {
        failed_ = true;
        throw std::runtime_error("recovery output flush failure");
    }
}

void writeRecoveryStatus(std::ostream &output, const transport::RecoveryStatus &status) {
    publish(output, [&] {
        row(output, "recovery-status", status.phase, status.attempts, status.demandedFamilies, status.convergedFamilies,
            status.stopped, status.terminal.has_value());
    });
}

void writeRecoveryDiagnostic(std::ostream &output, RecoveryOutputDiagnostic scope,
                             const transport::RecoveryDiagnostic &diagnostic) {
    publish(output, [&] {
        row(output, "recovery-diagnostic", diagnosticName(scope), diagnostic.error, diagnostic.transport.error,
            diagnostic.transport.nativeCode, diagnostic.transport.protocolError, queueError(diagnostic.queue),
            sourceKind(diagnostic.queue), sourceError(diagnostic.queue),
            diagnostic.endpoint ? std::optional(diagnostic.endpoint->error) : std::nullopt, diagnostic.writer);
    });
}

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
void writeRecoveryTlsDiagnostic(std::ostream &output, RecoveryOutputDiagnostic scope,
                                const transport::TlsDiagnostic &diagnostic) {
    publish(output, [&] {
        row(output, "tls-diagnostic", diagnosticName(scope), diagnostic.error, diagnostic.sdkCode,
            diagnostic.verifyCode, diagnostic.transport.error, diagnostic.transport.nativeCode,
            diagnostic.transport.protocolError);
    });
}
#endif

void writeRecoveryCommandAccepted(std::ostream &output, RecoveryOutputCommand command,
                                  const mux::MuxCommandTicket &ticket) {
    const auto id = ticket.id();

    if (id == 0)
        throw std::runtime_error("recovery output command ticket failure");

    publish(output, [&] {
        row(output, "command-accepted", commandName(command), id);
    });
}

void writeRecoveryCommandResult(std::ostream &output, RecoveryOutputCommand command,
                                const mux::MuxCommandResult &result) {
    publish(output, [&] {
        row(output, "command-result", commandName(command), result.id, result.status, result.revision, result.changed,
            result.error);
    });
}

void writeRecoveryReset(std::ostream &output, RecoveryEpochLabels &epochs, const mux::QueueResetNotice &notice) {
    publish(output, [&] {
        const auto from = epochs.label(notice.fromEpoch);
        const auto to = notice.toEpoch ? epochs.label(*notice.toEpoch) : std::nullopt;
        row(output, "reset", notice.sequence, from, to, notice.knownDiscardedQueueRecords,
            notice.knownDiscardedSourceRecords, notice.skippedResets, notice.unknownEligibility,
            notice.unknownUpstreamLoss, notice.countsSaturated, notice.sourceCommitted, notice.globalAckPending,
            queueError(notice.firstCause), sourceKind(notice.firstCause), sourceError(notice.firstCause),
            queueError(notice.secondaryCause), sourceKind(notice.secondaryCause), sourceError(notice.secondaryCause));
    });
}

void writeRecoveryResetAcknowledgement(std::ostream &output, std::uint64_t sequence, mux::ControlAdmission admission) {
    publish(output, [&] {
        row(output, "reset-ack", sequence, admission);
    });
}

void writeRecoveryHistory(std::ostream &output, RecoveryEpochLabels &epochs, std::int32_t localId,
                          std::u16string_view symbol, const mux::QueueRangeStatus &status) {
    publish(output, [&] {
        row(output, "history", localId, TextHex{symbol}, epochs.label(status.epoch), status.authoritativeRevision,
            status.clientDesiredRevision, status.mappingDesiredRevision, status.requestedFrom, status.mappingCurrent,
            status.acceptedUnstagedData, status.paused, status.resetPending, status.hadGap, status.queuedForKey,
            status.executingForKey, status.completeDeliveredRange, status.source.has_value());

        if (status.source) {
            const auto &source = *status.source;
            row(output, "history-source", localId, TextHex{symbol}, source.revision, source.observationSerial,
                source.partitionActivation, source.requestedFrom, source.sourceFrontier, source.knownFrontier,
                source.snipFrontier, source.snapshot, source.sourceTransactionPending, source.initialPending,
                source.viewTransactionDirty, source.completeRequestedRange);
        } else {
            row(output, "history-source", localId, TextHex{symbol}, std::nullopt, std::nullopt, std::nullopt,
                std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                std::nullopt, std::nullopt);
        }
    });
}

void writeRecoveryHistoryUnavailable(std::ostream &output, std::int32_t localId, std::u16string_view symbol,
                                     std::int64_t requestedFrom, const transport::RecoveryDiagnostic &diagnostic) {
    publish(output, [&] {
        row(output, "history-unavailable", localId, TextHex{symbol}, requestedFrom, diagnostic.error);
    });
}

void writeRecoveryTerminal(std::ostream &output, const transport::RecoveryTerminal &terminal) {
    publish(output, [&] {
        row(output, "recovery-terminal", terminal.outcome, terminal.deadlineExpired, terminal.heldDataDiscarded,
            terminal.nativeOperationsAtCutoff, terminal.wire.has_value());

        if (terminal.wire) {
            const auto &wire = *terminal.wire;
            row(output, "wire-terminal", wire.primary.error, wire.primary.nativeCode, wire.primary.protocolError,
                wire.secondary.error, wire.secondary.nativeCode, wire.secondary.protocolError, wire.aborted,
                wire.peerEnded, wire.writeAcceptanceUnknown, wire.reportedWriteBytes, wire.discardedInputBytes,
                wire.pendingOutputBytes);
        }
    });
}

void writeRecoveryQueueTerminal(std::ostream &output, RecoveryEpochLabels &epochs, core::SubscriptionFamily family,
                                const mux::QueueTerminal &terminal) {
    publish(output, [&] {
        const auto name = familyName(family);
        row(output, "queue-terminal", name, epochs.label(terminal.epoch), terminal.outcome,
            terminal.authoritativeRevision, terminal.hadGap, terminal.unknownEligibility, terminal.unknownUpstreamLoss,
            terminal.failedRegistrationCount, queueError(terminal.primary), sourceKind(terminal.primary),
            sourceError(terminal.primary), queueError(terminal.secondary), sourceKind(terminal.secondary),
            sourceError(terminal.secondary));
        const auto &counts = terminal.counts;
        row(output, "queue-counts", name, counts.acceptedFrames, counts.acceptedRecords, counts.collectorAcknowledged,
            counts.queueAdmitted, counts.withdrawn, counts.discarded, counts.sourceRecordsDiscarded,
            counts.callbackStartedBatches, counts.callbackStartedRecords, counts.callbackCompletedBatches,
            counts.callbackCompletedRecords, counts.callbackFailedBatches, counts.callbackFailedRecords,
            counts.commandsApplied, counts.commandsRejected, counts.commandsCancelled, counts.saturated);
    });
}

} // namespace org::ttldtor::qdcxx::connect
