// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "catalog_segment.hpp"
#include <limits>
#include <stdexcept>

namespace org::ttldtor::qdcxx::tape_inspect::detail {
CatalogOutput::CatalogOutput(std::ostream &output, std::ostream &diagnostic, std::size_t outputBytes,
                             std::size_t terminalBytes, std::stop_token stop) noexcept
    : output_(output), diagnostic_(diagnostic), stop_(stop), remaining_(outputBytes),
      terminalRemaining_(terminalBytes) {
}

void CatalogOutput::checkpoint() const {
    if (stop_.stop_requested()) {
        throw SequenceAbort{7, "stop", "cancelled"};
    }
}

std::size_t CatalogOutput::available(bool terminal) const noexcept {
    if (terminal) {
        return std::min(remaining_, terminalRemaining_);
    }

    const auto normal = remaining_ >= terminalRemaining_ ? remaining_ - terminalRemaining_ : 0;

    return sourceRemaining_ ? std::min(normal, *sourceRemaining_) : normal;
}

std::size_t CatalogOutput::rowBudget() const {
    checkpoint();

    return available();
}

void CatalogOutput::beginSource(std::size_t outputBytes) noexcept {
    sourceRemaining_ = outputBytes;
}

void CatalogOutput::endSource() noexcept {
    sourceRemaining_.reset();
}

void CatalogOutput::write(const std::string &row, bool terminal, bool diagnostic) {
    if (!terminal) {
        checkpoint();
    }

    if (row.size() > available(terminal) ||
        row.size() > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw SequenceAbort{6, "write", "output-limit"};
    }

    remaining_ -= row.size();
    emittedBytes_ += row.size();

    if (terminal) {
        terminalRemaining_ -= row.size();
    } else if (sourceRemaining_) {
        *sourceRemaining_ -= row.size();
    }

    auto &destination = diagnostic ? diagnostic_ : output_;

    try {
        destination.write(row.data(), static_cast<std::streamsize>(row.size()));
    } catch (const std::ios_base::failure &) {
        throw SequenceAbort{3, "write", "sink-io"};
    }

    if (!destination) {
        throw SequenceAbort{3, "write", "sink-io"};
    }
}

void CatalogOutput::flush(bool diagnostic) {
    auto &destination = diagnostic ? diagnostic_ : output_;

    try {
        destination.flush();
    } catch (const std::ios_base::failure &) {
        throw SequenceAbort{3, "flush", "sink-io"};
    }

    if (!destination) {
        throw SequenceAbort{3, "flush", "sink-io"};
    }
}

std::size_t CatalogOutput::emittedBytes() const noexcept {
    return emittedBytes_;
}

CatalogSegment::CatalogSegment(Limits limits, inspect::OutputFormat format, CatalogOutput &output,
                               std::size_t &aggregateEmitted, std::optional<SourceRecordTiming> source,
                               SourceDiagnostic *firstCause)
    : reader_(limits.tape, protocol::TapeDiscovery::BINARY_CATALOG_V1,
              source && source->policy == SourceTimePolicy::FIELD ? protocol::FieldTimePolicy::READ
                                                                  : protocol::FieldTimePolicy::DISABLED,
              source && (source->policy == SourceTimePolicy::LONG || source->policy == SourceTimePolicy::TEXT)
                  ? protocol::TapeExternalTimePolicy::SIDECAR
                  : protocol::TapeExternalTimePolicy::DISABLED),
      formatter_(format, inspect::DisplayProfile::BINARY_CATALOG_V1), output_(output), source_(source),
      aggregateEmitted_(aggregateEmitted), firstCause_(firstCause) {
}

void CatalogSegment::preserveParserFailure() noexcept {
    if (!source_ || !firstCause_ || firstCause_->exitCode != 0) {
        return;
    }

    const auto &cause = reader_.diagnostic();

    firstCause_->exitCode =
        cause.error == protocol::TapeError::UNSUPPORTED ? 4
        : cause.error == protocol::TapeError::RESOURCE_LIMIT || cause.error == protocol::TapeError::ALLOCATION_FAILURE
            ? 6
            : 5;
    firstCause_->stage = "parse";
    firstCause_->staticReason = "parser-failure";
    firstCause_->sourceOrdinal = source_->sourceOrdinal;
    firstCause_->layer = "parser";
    firstCause_->parserAccepted = cause.accepted;
    firstCause_->frameStart = cause.frameStart;
    firstCause_->payloadStart = cause.payloadStart;
}

void CatalogSegment::checkDescriptor() {
    if (!source_) {
        return;
    }

    const auto &descriptor = reader_.descriptor();

    if (!descriptor) {
        throw SequenceAbort{4, "profile", "missing-source-header"};
    }

    if (!descriptor->time && source_->policy == SourceTimePolicy::NONE) {
        descriptorSeen_ = true;

        return;
    }

    if (!descriptor->time) {
        throw SequenceAbort{4, "profile", "missing-time-declaration"};
    }

    const auto &time = *descriptor->time;
    bool matches = false;

    switch (source_->policy) {
    case SourceTimePolicy::NONE:
        matches = time == u"NONE" || time == u"none";
        break;
    case SourceTimePolicy::MESSAGE:
        matches = time == u"MESSAGE" || time == u"message";
        break;
    case SourceTimePolicy::FIELD:
        matches = time == u"FIELD" || time == u"field";
        break;
    case SourceTimePolicy::LONG:
        matches = time == u"LONG" || time == u"long";
        break;
    case SourceTimePolicy::TEXT:
        matches = time == u"TEXT" || time == u"text";
        break;
    }

    if (!matches) {
        throw SequenceAbort{4, "profile", "mixed-time-declaration"};
    }

    descriptorSeen_ = true;
}

void CatalogSegment::render(const protocol::TapeFeedResult &result, const core::RecordContext &before,
                            const std::optional<protocol::TapeTiming> &sidecarTiming) {
    if (!result.frame) {
        return;
    }

    const auto index = reader_.frameCount() - 1;
    const auto emit = [&](const std::string &row) {
        output_.write(row);
    };

    output_.checkpoint();

    if (source_) {
        if (sidecarTiming) {
            source_->external = sidecarTiming;
            source_->externalOrigin = ExternalTimeOrigin::SIDECAR;
            timestampAssigned_ = true;
        }

        if (result.frame->type == 1 && result.status != protocol::TapeStatus::FAILED) {
            checkDescriptor();
        }

        if (result.heartbeat && result.heartbeat->timestampMs) {
            if (source_->policy != SourceTimePolicy::MESSAGE) {
                throw SequenceAbort{4, "profile", "message-time-overlay"};
            }

            timestampAssigned_ = true;
        }

        constexpr std::int32_t RAW_DATA = 5;
        constexpr std::int32_t TICKER_DATA = 10;
        constexpr std::int32_t STREAM_DATA = 15;
        constexpr std::int32_t HISTORY_DATA = 20;
        const auto type = result.frame->type;
        const bool dataFrame = type == RAW_DATA || type == TICKER_DATA || type == STREAM_DATA || type == HISTORY_DATA;

        if (dataFrame && !descriptorSeen_) {
            throw SequenceAbort{4, "profile", "missing-source-header"};
        }

        if (dataFrame &&
            (source_->policy == SourceTimePolicy::MESSAGE || source_->policy == SourceTimePolicy::LONG ||
             source_->policy == SourceTimePolicy::TEXT) &&
            !timestampAssigned_) {
            throw SequenceAbort{4, "profile", "missing-time-assignment"};
        }
    }

    emit(formatter_.tapeFrame(*result.frame, index, output_.rowBudget(), protocol::TapeMode::BINARY_CATALOG_V1));

    if (result.frame->type == 1 && result.status != protocol::TapeStatus::FAILED && reader_.descriptor()) {
        emit(formatter_.catalogDescriptor(*reader_.descriptor(), index, output_.rowBudget()));
    }

    if (result.heartbeat) {
        emit(formatter_.tapeHeartbeat(*result.heartbeat, index, output_.rowBudget()));
    }

    if (result.timing) {
        emit(formatter_.tapeTiming(*result.timing, index, output_.rowBudget()));
    }

    constexpr std::int32_t DESCRIBE_RECORDS = 2;
    const auto after = reader_.schema().recordContext(0);

    if (result.frame->type == DESCRIBE_RECORDS && result.status != protocol::TapeStatus::FAILED && after.valid() &&
        !before.matches(after)) {
        if (generation_ == std::numeric_limits<std::size_t>::max()) {
            throw SequenceAbort{6, "schema", "ordinal-overflow"};
        }

        ++generation_;
        emit(formatter_.wireSchema(index, generation_, reader_.schema().records().size(), reader_.descriptions().size(),
                                   output_.rowBudget()));

        for (const auto &record : reader_.schema().records()) {
            output_.checkpoint();
            emit(formatter_.wireLocalRecord(generation_, record, output_.rowBudget()));
        }

        std::size_t descriptionIndex = 0;

        for (const auto &description : reader_.descriptions()) {
            output_.checkpoint();
            const auto *plan = reader_.context()->find(description.senderId);

            if (!plan) {
                throw SequenceAbort{4, "schema", "missing-source-plan"};
            }

            emit(source_ ? formatter_.sourceWireDescription(generation_, descriptionIndex++, description, *plan,
                                                            output_.rowBudget())
                         : formatter_.wireDescription(generation_, descriptionIndex++, description, *plan,
                                                      output_.rowBudget()));
        }

        emit(formatter_.wireSchemaEnd(generation_, output_.rowBudget()));
    }

    for (const auto &record : result.records) {
        output_.checkpoint();

        if (emitted_ == std::numeric_limits<std::size_t>::max() ||
            aggregateEmitted_ == std::numeric_limits<std::size_t>::max()) {
            throw SequenceAbort{6, "record", "ordinal-overflow"};
        }

        if (source_) {
            if (!descriptorSeen_) {
                throw SequenceAbort{4, "profile", "missing-source-header"};
            }

            if (source_->policy == SourceTimePolicy::FIELD && !record.fieldTime) {
                throw SequenceAbort{4, "profile", "missing-field-time"};
            }

            if ((source_->policy == SourceTimePolicy::MESSAGE || source_->policy == SourceTimePolicy::LONG ||
                 source_->policy == SourceTimePolicy::TEXT) &&
                !timestampAssigned_) {
                throw SequenceAbort{4, "profile", "missing-time-assignment"};
            }

            if (source_->policy == SourceTimePolicy::MESSAGE && result.timing) {
                source_->external = result.timing;
                source_->externalOrigin = ExternalTimeOrigin::MESSAGE;
            }

            emit(formatter_.sourceCatalogRecord(record, reader_.schema(), index, emitted_, aggregateEmitted_,
                                                generation_, output_.rowBudget(), *source_));
        } else {
            emit(formatter_.catalogRecord(record, reader_.schema(), index, emitted_, output_.rowBudget(),
                                          result.timing ? &*result.timing : nullptr, generation_));
        }

        ++emitted_;
        ++aggregateEmitted_;
    }
}

CatalogFeedCheckpoint CatalogSegment::feed(std::span<const std::byte> input,
                                           const std::optional<protocol::TapeTiming> &sidecarTiming) {
    output_.checkpoint();
    const auto before = reader_.schema().recordContext(0);
    auto result = reader_.feed(input);

    if (result.status == protocol::TapeStatus::FAILED) {
        preserveParserFailure();
    }

    render(result, before, sidecarTiming);

    return {result.accepted, result.status};
}

protocol::TapeStatus CatalogSegment::finish() {
    output_.checkpoint();

    if (source_ && !descriptorSeen_) {
        throw SequenceAbort{4, "profile", "missing-source-header"};
    }

    return reader_.finish();
}

const protocol::TapeReader &CatalogSegment::reader() const noexcept {
    return reader_;
}

std::size_t CatalogSegment::emittedRecords() const noexcept {
    return emitted_;
}
} // namespace org::ttldtor::qdcxx::tape_inspect::detail
