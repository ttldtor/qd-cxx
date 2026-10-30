// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "catalog_segment.hpp"
#include "sequence_test_seam.hpp"
#include "tape_sequence.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
constexpr std::size_t TERMINAL_RESOURCE_ROWS = 92;
constexpr std::size_t TERMINAL_DIAGNOSTICS = 4;

/** Preserve the first causal failure; later semantic causes do not create a dynamic history. */
void primary(TapeSequenceResult &result, const SourceDiagnostic &diagnostic) noexcept {
    if (result.primary.exitCode == 0) {
        result.primary = diagnostic;
    }
}

SourceDiagnostic failure(int code, std::string_view stage, std::string_view reason, std::optional<std::size_t> ordinal,
                         std::string_view layer) noexcept {
    SourceDiagnostic diagnostic;

    diagnostic.exitCode = code;
    diagnostic.stage = stage;
    diagnostic.staticReason = reason;
    diagnostic.sourceOrdinal = ordinal;
    diagnostic.layer = layer;

    return diagnostic;
}

int containerCode(ContainerError error) noexcept {
    switch (error) {
    case ContainerError::SOURCE_IO:
        return 3;
    case ContainerError::UNSUPPORTED:
        return 4;
    case ContainerError::COMPRESSED_LIMIT:
    case ContainerError::INFLATED_LIMIT:
    case ContainerError::RATIO_LIMIT:
    case ContainerError::HEADER_LIMIT:
    case ContainerError::MEMBER_ENTRY_LIMIT:
    case ContainerError::WORK_LIMIT:
    case ContainerError::ALLOCATION:
    case ContainerError::INVALID_ARGUMENT:
        return 6;
    default:
        return 5;
    }
}

std::string_view containerReason(ContainerError error) noexcept {
    switch (error) {
    case ContainerError::INVALID_HEADER:
        return "invalid-header";
    case ContainerError::INVALID_DATA:
        return "invalid-data";
    case ContainerError::TRUNCATED:
        return "truncated";
    case ContainerError::CHECKSUM:
        return "checksum";
    case ContainerError::INVALID_DIRECTORY:
        return "invalid-directory";
    case ContainerError::UNSUPPORTED:
        return "unsupported-container";
    case ContainerError::COMPRESSED_LIMIT:
        return "compressed-limit";
    case ContainerError::INFLATED_LIMIT:
        return "inflated-limit";
    case ContainerError::RATIO_LIMIT:
        return "ratio-limit";
    case ContainerError::HEADER_LIMIT:
        return "header-limit";
    case ContainerError::MEMBER_ENTRY_LIMIT:
        return "member-entry-limit";
    case ContainerError::WORK_LIMIT:
        return "work-limit";
    case ContainerError::SOURCE_IO:
        return "source-io";
    case ContainerError::ALLOCATION:
        return "allocation-failure";
    case ContainerError::INVALID_ARGUMENT:
        return "invalid-argument";
    case ContainerError::NOT_FINISHED:
        return "not-finished";
    case ContainerError::NONE:
        return "none";
    }

    return "invalid-container-state";
}

int parserCode(protocol::TapeError error) noexcept {
    switch (error) {
    case protocol::TapeError::UNSUPPORTED:
        return 4;
    case protocol::TapeError::RESOURCE_LIMIT:
    case protocol::TapeError::ALLOCATION_FAILURE:
        return 6;
    default:
        return 5;
    }
}

int scannerCode(SidecarError error) noexcept {
    switch (error) {
    case SidecarError::UNSUPPORTED_TIME:
        return 4;
    case SidecarError::MALFORMED_TIME:
    case SidecarError::MALFORMED_POSITION:
    case SidecarError::POSITION_AFTER_DATA:
        return 5;
    default:
        return 6;
    }
}

SourceDiagnostic containerFailure(ContainerError error, const ContainerCounters &counts, std::size_t ordinal,
                                  bool companion, std::uint64_t accepted) noexcept {
    auto diagnostic =
        failure(containerCode(error), "container", containerReason(error), ordinal, companion ? "time" : "data");

    diagnostic.physicalFetched = counts.compressedPhysicalRead;
    diagnostic.physicalConsumed = counts.compressedConsumed;
    diagnostic.logicalProduced = counts.logicalProduced;
    diagnostic.parserAccepted = accepted;

    return diagnostic;
}

SourceDiagnostic parserFailure(const protocol::TapeReader &reader, std::size_t ordinal) noexcept {
    const auto &cause = reader.diagnostic();
    auto diagnostic = failure(parserCode(cause.error), "parse", "parser-failure", ordinal, "parser");

    diagnostic.parserAccepted = cause.accepted;
    diagnostic.frameStart = cause.frameStart;
    diagnostic.payloadStart = cause.payloadStart;

    return diagnostic;
}

void add(std::uint64_t &sum, std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - sum) {
        throw detail::SequenceAbort{6, "aggregate", "counter-overflow"};
    }

    sum += value;
}

/** Observe real adapter destruction before the physical-close test seam is reached. */
struct AdapterLifetime {
    detail::SequenceFileTest *test;
    unsigned code;

    AdapterLifetime(detail::SequenceFileTest *observation, unsigned value) noexcept : test(observation), code(value) {
        if (test) {
            ++test->liveAdapters;
        }
    }

    ~AdapterLifetime() {
        if (test) {
            --test->liveAdapters;

            if (test->cleanupCount < test->cleanupOrder.size()) {
                test->cleanupOrder[test->cleanupCount] = code;
            }

            ++test->cleanupCount;
        }
    }
};

/** Snapshot actual prefix counters while the borrowed reader still exists, including late errors. */
struct SegmentProgress {
    detail::CatalogSegment &segment;
    const CompressedInput &input;
    TapeSequenceResult &result;
    std::size_t ordinal;

    ~SegmentProgress() {
        if (result.primary.exitCode != 0 && result.primary.layer == "parser" &&
            result.primary.sourceOrdinal == ordinal) {
            const auto counts = input.counters();

            result.primary.physicalFetched = counts.compressedPhysicalRead;
            result.primary.physicalConsumed = counts.compressedConsumed;
            result.primary.logicalProduced = counts.logicalProduced;
        }

        try {
            add(result.counters.accepted, segment.reader().acceptedBytes());
            add(result.counters.frames, segment.reader().frameCount());
            add(result.counters.emitted, segment.emittedRecords());
            add(result.counters.processed, segment.reader().processedRecords());
            add(result.counters.wireFields, segment.reader().wireFields());
        } catch (const detail::SequenceAbort &cause) {
            primary(result, failure(cause.code, cause.stage, cause.reason, ordinal, "sequence"));
        }
    }
};

void closeFile(std::ifstream &file, bool companion, std::size_t ordinal, TapeSequenceResult &result,
               detail::SequenceFileTest *test) noexcept {
    if (!file.is_open()) {
        return;
    }

    bool failed = false;
    try {
        file.clear();
        file.close();
        failed = file.fail();
    } catch (...) {
        failed = true;
    }

    if (test) {
        if (companion) {
            ++test->timeCloses;
            failed = failed || test->failTimeClose == ordinal;
        } else {
            ++test->dataCloses;
            failed = failed || test->failDataClose == ordinal;
        }

        if (test->cleanupCount < test->cleanupOrder.size()) {
            test->cleanupOrder[test->cleanupCount] = companion ? 3U : 4U;
        }

        ++test->cleanupCount;

        if (test->liveAdapters != 0) {
            failed = true;
        }
    }

    if (failed) {
        auto diagnostic = failure(3, "close", "source-close", ordinal, companion ? "time" : "data");

        (companion ? result.timeClose : result.dataClose) = diagnostic;
        primary(result, diagnostic);
    }
}

/** One concrete paired scanner cursor; owns bounded unread bytes and at most one pending mark. */
class CompanionCursor {
    CompressedInput &input_;
    SidecarTime &scanner_;
    detail::CatalogOutput &output_;
    TapeSequenceResult &result_;
    detail::SequenceFileTest *test_;
    std::size_t ordinal_;
    std::vector<std::byte> buffer_;
    std::span<const std::byte> unread_;
    bool inputComplete_ = false;
    bool ended_ = false;
    bool sourceFailed_ = false;

    public:
    CompanionCursor(CompressedInput &input, SidecarTime &scanner, std::size_t chunk, detail::CatalogOutput &output,
                    TapeSequenceResult &result, detail::SequenceFileTest *test, std::size_t ordinal)
        : input_(input), scanner_(scanner), output_(output), result_(result), test_(test), ordinal_(ordinal),
          buffer_(chunk) {
    }

    std::optional<SidecarMark> next(std::uint64_t accepted, bool allowRead = true) {
        while (!ended_) {
            output_.checkpoint();
            SidecarFeed feed;

            if (!unread_.empty()) {
                if (test_) {
                    ++test_->scannerFeeds;
                }

                feed = scanner_.feed(unread_);
                unread_ = unread_.subspan(feed.accepted);
            } else if (sourceFailed_) {
                throw detail::SequenceAbort{result_.primary.exitCode, "container", "container-failure"};
            } else if (inputComplete_) {
                if (test_) {
                    ++test_->scannerFeeds;
                }

                feed = scanner_.endInput();
            } else {
                if (!allowRead) {
                    throw detail::SequenceAbort{result_.primary.exitCode, "container", "recorded-source-failure"};
                }

                if (test_) {
                    ++test_->reads;
                    ++test_->timeReads;

                    if (result_.primary.exitCode != 0) {
                        ++test_->afterPrimaryReads;
                    }
                }

                const auto read = input_.read(buffer_);

                if (read.error != ContainerError::NONE) {
                    if (test_ && result_.primary.exitCode == 0) {
                        test_->timeReadsAtSourceCause = test_->timeReads;
                        test_->dataReadsAtSourceCause = test_->dataReads;
                    }

                    primary(result_, containerFailure(read.error, input_.counters(), ordinal_, true, accepted));
                    sourceFailed_ = true;
                }

                inputComplete_ = read.physicalComplete;
                unread_ = std::span(buffer_).first(read.produced);

                continue;
            }

            if (feed.status == SidecarStatus::FAILED) {
                const auto cause = scanner_.diagnostic();
                auto diagnostic = failure(scannerCode(cause.error), "sidecar", "scanner-failure", ordinal_, "time");

                diagnostic.sidecarLine = cause.line;
                diagnostic.logicalProduced = input_.counters().logicalProduced;
                diagnostic.parserAccepted = accepted;
                primary(result_, diagnostic);

                throw detail::SequenceAbort{diagnostic.exitCode, "sidecar", "scanner-failure"};
            }

            if (feed.mark) {
                if (sourceFailed_) {
                    continue;
                }

                return feed.mark;
            }

            if (feed.status == SidecarStatus::END) {
                ended_ = true;
            }
        }

        return {};
    }

    void finish(std::uint64_t logicalBytes) {
        while (next(logicalBytes)) {
        }

        output_.checkpoint();

        if (input_.finish() != ContainerError::NONE) {
            primary(result_, containerFailure(input_.finish(), input_.counters(), ordinal_, true, logicalBytes));

            throw detail::SequenceAbort{result_.primary.exitCode, "container", "container-failure"};
        }

        const auto error = scanner_.finish(logicalBytes);

        if (error != SidecarError::NONE) {
            const auto cause = scanner_.diagnostic();
            auto diagnostic = failure(scannerCode(error), "sidecar", "scanner-finish", ordinal_, "time");

            diagnostic.sidecarLine = cause.line;
            diagnostic.parserAccepted = logicalBytes;
            primary(result_, diagnostic);

            throw detail::SequenceAbort{diagnostic.exitCode, "sidecar", "scanner-finish"};
        }
    }
};

/** Fixed successful-source snapshots; no reader, adapter, schema or path view escapes. */
struct ClosedSegment {
    SourceEnd summary;
    ContainerCounters data;
    std::optional<ContainerCounters> time;
    std::optional<SidecarCounters> scanner;
    std::uint64_t markers = 0;
};

/** Read one owning pair; all adapters die before either explicit physical close. */
std::optional<ClosedSegment> runSegment(const TapeSegmentSpec &spec, std::size_t ordinal,
                                        const TapeSequenceOptions &options, detail::CatalogOutput &output,
                                        TapeSequenceResult &result, std::size_t &aggregateEmitted,
                                        detail::SequenceFileTest *test) {
    std::ifstream data;
    std::ifstream time;
    std::optional<ClosedSegment> completed;

    try {
        output.checkpoint();

        if (test) {
            ++test->dataOpens;
        }

        if (!(test && test->failDataOpen == ordinal)) {
            data.open(spec.path, std::ios::binary);
        }

        if (!data.is_open()) {
            throw detail::SequenceAbort{3, "open", "data-open"};
        }

        result.maximumOpenHandles = std::max(result.maximumOpenHandles, std::size_t{1});

        if (spec.companion) {
            output.checkpoint();

            if (test) {
                ++test->timeOpens;
            }

            if (!(test && test->failTimeOpen == ordinal)) {
                time.open(spec.companion->path, std::ios::binary);
            }

            if (!time.is_open()) {
                throw detail::SequenceAbort{3, "open", "time-open"};
            }

            result.maximumOpenHandles = std::max(result.maximumOpenHandles, std::size_t{2});
        }

        {
            AdapterLifetime dataLifetime(test, 2);
            CompressedInput dataInput(data, spec.container, spec.containerLimits);
            std::optional<AdapterLifetime> timeLifetime;
            std::optional<CompressedInput> timeInput;
            std::optional<SidecarTime> scanner;
            std::optional<CompanionCursor> cursor;

            if (spec.companion) {
                timeLifetime.emplace(test, 1);
                timeInput.emplace(time, spec.companion->container, spec.companion->containerLimits);
                scanner.emplace(options.time == SourceTimePolicy::LONG ? SidecarFormat::LONG : SidecarFormat::TEXT,
                                spec.companion->scannerLimits);
                cursor.emplace(*timeInput, *scanner, spec.companion->containerLimits.chunkBytes, output, result, test,
                               ordinal);
            }

            detail::CatalogSegment segment(spec.limits, options.format, output, aggregateEmitted,
                                           SourceRecordTiming{options.time, ordinal, {}, ExternalTimeOrigin::NONE},
                                           &result.primary);
            SegmentProgress progress{segment, dataInput, result, ordinal};
            std::vector<std::byte> buffer(spec.limits.chunkSize);
            std::optional<SidecarMark> pending;
            std::optional<protocol::TapeTiming> timing;
            bool companionPrimed = false;
            bool dataComplete = false;

            while (!dataComplete) {
                output.checkpoint();

                if (cursor && !companionPrimed) {
                    pending = cursor->next(segment.reader().acceptedBytes());
                    companionPrimed = true;
                }

                output.checkpoint();

                if (test) {
                    ++test->reads;
                    ++test->dataReads;

                    if (result.primary.exitCode != 0) {
                        ++test->afterPrimaryReads;
                    }
                }

                const auto read = dataInput.read(buffer);

                if (read.error != ContainerError::NONE) {
                    if (test && result.primary.exitCode == 0) {
                        test->timeReadsAtSourceCause = test->timeReads;
                        test->dataReadsAtSourceCause = test->dataReads;
                    }

                    primary(result, containerFailure(read.error, dataInput.counters(), ordinal, false,
                                                     segment.reader().acceptedBytes()));
                }

                auto bytes = std::span<const std::byte>(buffer).first(read.produced);

                while (!bytes.empty()) {
                    output.checkpoint();
                    const auto accepted = segment.reader().acceptedBytes();

                    while (pending && pending->reached(accepted)) {
                        timing = protocol::TapeTiming{pending->timestampMs, pending->packed()};
                        pending = cursor->next(accepted, result.primary.exitCode == 0);
                    }

                    auto submitted = bytes;

                    if (pending && pending->position >= 0 && static_cast<std::uint64_t>(pending->position) > accepted) {
                        const auto untilMark = static_cast<std::uint64_t>(pending->position) - accepted;

                        if (untilMark < submitted.size()) {
                            submitted = submitted.first(static_cast<std::size_t>(untilMark));
                        }
                    }

                    if (test) {
                        ++test->parserFeeds;
                    }

                    const auto fed = segment.feed(submitted, timing);
                    bytes = bytes.subspan(fed.accepted);

                    if (fed.status == protocol::TapeStatus::FAILED) {
                        primary(result, parserFailure(segment.reader(), ordinal));

                        throw detail::SequenceAbort{result.primary.exitCode, "parse", "parser-failure"};
                    }

                    if (fed.accepted == 0 && fed.status != protocol::TapeStatus::EVENT) {
                        throw detail::SequenceAbort{5, "parse", "no-progress"};
                    }
                }

                if (read.error != ContainerError::NONE) {
                    throw detail::SequenceAbort{result.primary.exitCode, "container", "container-failure"};
                }

                dataComplete = read.physicalComplete;
            }

            if (dataInput.counters().logicalProduced == 0) {
                throw detail::SequenceAbort{4, "profile", "empty-segment"};
            }

            if (segment.finish() != protocol::TapeStatus::FINISHED) {
                primary(result, parserFailure(segment.reader(), ordinal));

                throw detail::SequenceAbort{result.primary.exitCode, "parse", "parser-finish"};
            }

            if (dataInput.finish() != ContainerError::NONE) {
                primary(result, containerFailure(dataInput.finish(), dataInput.counters(), ordinal, false,
                                                 segment.reader().acceptedBytes()));

                throw detail::SequenceAbort{result.primary.exitCode, "container", "container-finish"};
            }

            if (cursor) {
                cursor->finish(dataInput.counters().logicalProduced);
            }

            const auto &reader = segment.reader();
            const auto counts = dataInput.counters();

            completed = ClosedSegment{{ordinal, reader.acceptedBytes(), reader.frameCount(), segment.emittedRecords(),
                                       reader.processedRecords(), reader.wireFields(), counts.compressedPhysicalRead,
                                       counts.compressedConsumed},
                                      counts,
                                      timeInput ? std::optional(timeInput->counters()) : std::nullopt,
                                      scanner ? std::optional(scanner->counters()) : std::nullopt,
                                      reader.markerCount()};
        }
    } catch (const detail::SequenceAbort &cause) {
        primary(result, failure(cause.code, cause.stage, cause.reason, ordinal,
                                cause.stage == "write" || cause.stage == "flush" ? "sink" : "sequence"));
    } catch (const inspect::OutputLimit &) {
        primary(result, failure(6, "write", "output-limit", ordinal, "sink"));
    } catch (const std::ios_base::failure &) {
        primary(result, failure(3, "io", "stream-failure", ordinal, "sink"));
    } catch (const std::bad_alloc &) {
        primary(result, failure(6, "allocation", "allocation-failure", ordinal, "sequence"));
    } catch (const std::length_error &) {
        primary(result, failure(6, "allocation", "length-limit", ordinal, "sequence"));
    } catch (const std::invalid_argument &) {
        primary(result, failure(6, "configuration", "invalid-argument", ordinal, "sequence"));
    } catch (...) {
        primary(result, failure(3, "io", "stream-failure", ordinal, "sink"));
    }

    closeFile(time, true, ordinal, result, test);
    closeFile(data, false, ordinal, result, test);

    return result.primary.exitCode == 0 ? completed : std::optional<ClosedSegment>{};
}

/** Publish actual per-source layer counters only after every opened source has closed. */
void sourceResources(const TapeSegmentSpec &spec, const ClosedSegment &closed, detail::CatalogOutput &output) {
    const auto ordinal = closed.summary.sourceOrdinal;
    const auto emit = [&](std::string_view scope, std::string_view name, std::uint64_t reserved,
                          std::optional<std::uint64_t> observed) {
        output.write(sourceResource(scope, ordinal, name, reserved, observed, output.rowBudget()));
    };
    const auto container = [&](std::string_view scope, const ContainerLimits &limits, const ContainerCounters &counts) {
        emit(scope, "physicalFetched", limits.compressedBytes, counts.compressedPhysicalRead);
        emit(scope, "compressedConsumed", limits.compressedBytes, counts.compressedConsumed);
        emit(scope, "logicalProduced", limits.inflatedBytes, counts.logicalProduced);
        emit(scope, "headerBytes", limits.totalHeaderBytes, counts.headerBytes);
        emit(scope, "workBytes", limits.workBytes, counts.workBytes);
        emit(scope, "inflateCalls", limits.inflateCalls, counts.inflateCalls);
        emit(scope, "physicalProbes", 1,
             counts.compressedPhysicalRead > limits.compressedBytes
                 ? counts.compressedPhysicalRead - limits.compressedBytes
                 : 0);
        emit(scope, "inflatedProbes", 1, {});
    };

    container("data", spec.containerLimits, closed.data);

    if (closed.time) {
        container("time", spec.companion->containerLimits, *closed.time);
    }

    if (closed.scanner) {
        const auto &limits = spec.companion->scannerLimits;
        const auto &counts = *closed.scanner;

        emit("scanner", "inputBytes", limits.inputBytes, counts.inputBytes);
        emit("scanner", "workBytes", limits.workBytes, counts.workBytes);
        emit("scanner", "lines", limits.lines, counts.lines);
        emit("scanner", "marks", limits.marks, counts.marks);
        emit("scanner", "maximumPosition", spec.containerLimits.inflatedBytes, counts.maximumPosition);
    }

    emit("parser", "parserAccepted", spec.limits.tape.inputBytes, closed.summary.accepted);
    emit("parser", "frames", spec.limits.tape.frames, closed.summary.frames);
    emit("parser", "markers", spec.limits.tape.frames, closed.markers);
    emit("parser", "emittedRecords", spec.limits.tape.data.emittedRecords, closed.summary.emitted);
    emit("parser", "processedRecords", spec.limits.tape.data.processedRecords, closed.summary.processed);
    emit("parser", "wireFields", spec.limits.tape.data.wireFields, closed.summary.wireFields);
}

void resources(const SequenceAdmission &admission, const TapeSequenceOptions &options, const TapeSequenceResult &result,
               detail::CatalogOutput &output) {
    const auto &reserved = admission.reserved;

    struct Capacity {
        std::string_view name;
        std::uint64_t value;
    };

    const std::array<Capacity, 78> capacities{{
        {"tape-frame-maximumBodyBytes", reserved.tape.frame.maximumBodyBytes},
        {"tape-frame-maximumPendingBytes", reserved.tape.frame.maximumPendingBytes},
        {"tape-inputBytes", reserved.tape.inputBytes},
        {"tape-frames", reserved.tape.frames},
        {"tape-descriptor-properties", reserved.tape.descriptor.properties},
        {"tape-descriptor-messages", reserved.tape.descriptor.messages},
        {"tape-descriptor-text-bodyBytes", reserved.tape.descriptor.text.bodyBytes},
        {"tape-descriptor-text-codeUnits", reserved.tape.descriptor.text.codeUnits},
        {"tape-descriptor-textUnits", reserved.tape.descriptor.textUnits},
        {"tape-context-schema-records", reserved.tape.context.schema.records},
        {"tape-context-schema-fields", reserved.tape.context.schema.fields},
        {"tape-context-schema-lookups", reserved.tape.context.schema.lookups},
        {"tape-context-schema-nameUnits", reserved.tape.context.schema.nameUnits},
        {"tape-context-schema-totalNameUnits", reserved.tape.context.schema.totalNameUnits},
        {"tape-context-descriptions", reserved.tape.context.descriptions},
        {"tape-context-bindings-fields", reserved.tape.context.bindings.fields},
        {"tape-context-bindings-nameUnits", reserved.tape.context.bindings.nameUnits},
        {"tape-context-bindings-totalNameUnits", reserved.tape.context.bindings.totalNameUnits},
        {"tape-constructionUnits", reserved.tape.constructionUnits},
        {"tape-data-payloadBytes", reserved.tape.data.payloadBytes},
        {"tape-data-processedRecords", reserved.tape.data.processedRecords},
        {"tape-data-wireFields", reserved.tape.data.wireFields},
        {"tape-data-emittedRecords", reserved.tape.data.emittedRecords},
        {"tape-data-emittedValues", reserved.tape.data.emittedValues},
        {"tape-data-ownedSymbolUnits", reserved.tape.data.ownedSymbolUnits},
        {"tape-data-symbol-inputBytes", reserved.tape.data.symbol.inputBytes},
        {"tape-data-symbol-codeUnits", reserved.tape.data.symbol.codeUnits},
        {"tape-data-symbol-stateUnits", reserved.tape.data.symbol.stateUnits},
        {"tape-data-fieldString-encodedBytes", reserved.tape.data.fieldString.encodedBytes},
        {"tape-data-fieldString-codeUnits", reserved.tape.data.fieldString.codeUnits},
        {"tape-data-fieldBytes", reserved.tape.data.fieldBytes},
        {"tape-data-fieldUnits", reserved.tape.data.fieldUnits},
        {"tape-data-ownedFieldUnits", reserved.tape.data.ownedFieldUnits},
        {"tape-data-fieldPayloadEncodedBytes", reserved.tape.data.fieldPayloadEncodedBytes},
        {"tape-data-fieldPayloadBytes", reserved.tape.data.fieldPayloadBytes},
        {"tape-data-payloadEncodedBytes", reserved.tape.data.payloadEncodedBytes},
        {"tape-data-payloadWorkBytes", reserved.tape.data.payloadWorkBytes},
        {"tape-data-ownedPayloadBytes", reserved.tape.data.ownedPayloadBytes},
        {"tape-data-ownedFieldTimeRecords", reserved.tape.data.ownedFieldTimeRecords},
        {"tape-timingEvents", reserved.tape.timingEvents},
        {"tape-snapshotUnits", reserved.tape.snapshotUnits},
        {"dataContainer-compressedBytes", reserved.dataContainer.compressedBytes},
        {"companionContainer-compressedBytes", reserved.companionContainer.compressedBytes},
        {"dataContainer-inflatedBytes", reserved.dataContainer.inflatedBytes},
        {"companionContainer-inflatedBytes", reserved.companionContainer.inflatedBytes},
        {"dataContainer-expansionRatio", reserved.dataContainer.expansionRatio},
        {"companionContainer-expansionRatio", reserved.companionContainer.expansionRatio},
        {"dataContainer-ratioSlack", reserved.dataContainer.ratioSlack},
        {"companionContainer-ratioSlack", reserved.companionContainer.ratioSlack},
        {"dataContainer-gzipMembers", reserved.dataContainer.gzipMembers},
        {"companionContainer-gzipMembers", reserved.companionContainer.gzipMembers},
        {"dataContainer-zipEntries", reserved.dataContainer.zipEntries},
        {"companionContainer-zipEntries", reserved.companionContainer.zipEntries},
        {"dataContainer-headerBytes", reserved.dataContainer.headerBytes},
        {"companionContainer-headerBytes", reserved.companionContainer.headerBytes},
        {"dataContainer-totalHeaderBytes", reserved.dataContainer.totalHeaderBytes},
        {"companionContainer-totalHeaderBytes", reserved.companionContainer.totalHeaderBytes},
        {"dataContainer-workBytes", reserved.dataContainer.workBytes},
        {"companionContainer-workBytes", reserved.companionContainer.workBytes},
        {"dataContainer-inflateCalls", reserved.dataContainer.inflateCalls},
        {"companionContainer-inflateCalls", reserved.companionContainer.inflateCalls},
        {"dataContainer-retainedBytes", reserved.dataContainer.retainedBytes},
        {"companionContainer-retainedBytes", reserved.companionContainer.retainedBytes},
        {"dataContainer-inflaterBytes", reserved.dataContainer.inflaterBytes},
        {"companionContainer-inflaterBytes", reserved.companionContainer.inflaterBytes},
        {"dataContainer-chunkBytes", reserved.dataContainer.chunkBytes},
        {"companionContainer-chunkBytes", reserved.companionContainer.chunkBytes},
        {"scanner-inputBytes", reserved.scanner.inputBytes},
        {"scanner-lineBytes", reserved.scanner.lineBytes},
        {"scanner-lines", reserved.scanner.lines},
        {"scanner-marks", reserved.scanner.marks},
        {"scanner-workBytes", reserved.scanner.workBytes},
        {"scanner-retainedBytes", reserved.scanner.retainedBytes},
        {"chunkBytes", reserved.chunkBytes},
        {"outputBytes", reserved.outputBytes},
        {"eventUnits", reserved.eventUnits},
        {"physicalProbes", reserved.physicalProbes},
        {"inflatedProbes", reserved.inflatedProbes},

    }};

    for (const auto &capacity : capacities) {
        output.checkpoint();
        output.write(sourceResource("sequence", {}, capacity.name, capacity.value, {}, output.available(true)), true);
    }

    const auto emit = [&](std::string_view name, std::uint64_t capacity, std::optional<std::uint64_t> observed) {
        output.checkpoint();
        output.write(sourceResource("sequence", {}, name, capacity, observed, output.available(true)), true);
    };

    emit("path-native-units", options.totalPathNativeUnits, admission.pathNativeUnits);
    emit("path-encoded-bytes", options.pathEncodedBytes, admission.pathEncodedBytes);
    emit("path-storage-bytes", options.pathStorageBytes, admission.pathStorageBytes);
    emit("occurrences", options.occurrences, result.counters.sources);
    emit("source-handles", options.sourceHandles, result.maximumOpenHandles);
    emit("diagnostic-bytes", options.diagnosticBytes, {});
    emit("terminal-bytes", options.terminalBytes, {});
    emit("output-charged-bytes", options.outputBytes, output.emittedBytes());
    emit("closed-sources", options.occurrences, result.counters.sources);
    emit("accepted-bytes", reserved.tape.inputBytes, result.counters.accepted);
    emit("frames", reserved.tape.frames, result.counters.frames);
    emit("emitted-records", reserved.tape.data.emittedRecords, result.counters.emitted);
    emit("processed-records", reserved.tape.data.processedRecords, result.counters.processed);
    emit("wire-fields", reserved.tape.data.wireFields, result.counters.wireFields);
}

void reportFailure(TapeSequenceResult &result, detail::CatalogOutput &output) noexcept {
    try {
        output.write(sourceDiagnostic(result.primary, false, output.available(true)), true, true);

        for (const auto *cause : {&result.timeClose, &result.dataClose, &result.flush}) {
            if (*cause && (*cause)->exitCode != 0) {
                output.write(sourceDiagnostic(**cause, true, output.available(true)), true, true);
            }
        }

        output.flush(true);
    } catch (const inspect::OutputLimit &) {
        if (!result.flush) {
            result.flush = failure(6, "write", "diagnostic-output-limit", result.primary.sourceOrdinal, "sink");
        }
    } catch (...) {
        if (!result.flush) {
            result.flush = failure(3, "flush", "diagnostic-io", result.primary.sourceOrdinal, "sink");
        }
    }
}

TapeSequenceResult runSequence(std::span<const TapeSegmentSpec> segments, std::ostream &destination,
                               std::ostream &diagnostic, const TapeSequenceOptions &options, std::stop_token stop,
                               detail::SequenceFileTest *test) {
    TapeSequenceResult result;
    const auto admission = admitSequence(segments, options);
    detail::CatalogOutput output(destination, diagnostic, options.outputBytes,
                                 std::min(options.terminalBytes, options.outputBytes), stop);
    std::optional<std::size_t> activeOrdinal;

    result.reserved = admission.reserved;

    if (admission.diagnostic.error != SequenceError::NONE) {
        const auto code = (admission.diagnostic.error == SequenceError::UNSUPPORTED_SOURCE_PROFILE ||
                           admission.diagnostic.error == SequenceError::UNSUPPORTED_CONTAINER)
                              ? 4
                              : 6;

        primary(result, failure(code, "admission", "configuration-refused", {}, "sequence"));
        reportFailure(result, output);
        result.outputChargedBytes = output.emittedBytes();

        return result;
    }

    try {
        output.checkpoint();

        if (options.format != inspect::OutputFormat::TSV && options.format != inspect::OutputFormat::HUMAN) {
            throw detail::SequenceAbort{6, "configuration", "invalid-presentation"};
        }

        // Identity rows are admitted exactly before allocating rows or copying path objects.
        const auto headerBytes =
            std::string_view("format\tqdcxx-tape-inspect\t9\tbinary-catalog-v1\tself-contained-segments-v1\n").size();
        std::uint64_t identityBytes = headerBytes;

        for (std::size_t ordinal = 0; ordinal < segments.size(); ++ordinal) {
            const auto &segment = segments[ordinal];
            const auto count =
                sourceIdentityBytes(ordinal, segment.path, segment.companion ? &segment.companion->path : nullptr);

            if (!count) {
                throw detail::SequenceAbort{6, "admission", "identity-overflow"};
            }

            add(identityBytes, *count);
        }

        const auto publicationCapacity = options.outputBytes - options.terminalBytes;

        if (identityBytes > publicationCapacity ||
            admission.reserved.outputBytes > publicationCapacity - identityBytes) {
            throw detail::SequenceAbort{6, "admission", "identity-output-limit"};
        }

        std::vector<TapeSegmentSpec> owned;

        owned.reserve(segments.size());

        for (const auto &segment : segments) {
            output.checkpoint();
            owned.push_back(segment);
        }

        output.write(sourceHeader(output.rowBudget()));

        for (std::size_t ordinal = 0; ordinal < owned.size(); ++ordinal) {
            const auto &segment = owned[ordinal];

            output.write(sourceIdentity(ordinal, segment.path, segment.companion ? &segment.companion->path : nullptr,
                                        output.rowBudget()));
        }

        std::size_t aggregateEmitted = 0;

        for (std::size_t ordinal = 0; ordinal < owned.size(); ++ordinal) {
            output.checkpoint();
            const auto &segment = owned[ordinal];

            activeOrdinal = ordinal;

            output.beginSource(segment.limits.output);
            output.write(sourceBegin(ordinal, output.rowBudget()));
            output.write(sourcePolicy(options.time, segment.container,
                                      segment.companion ? std::optional(segment.companion->container) : std::nullopt,
                                      output.rowBudget()));
            const auto completed = runSegment(segment, ordinal, options, output, result, aggregateEmitted, test);

            if (!completed) {
                break;
            }

            add(result.counters.sources, 1);
            sourceResources(segment, *completed, output);
            output.write(sourceEnd(completed->summary, output.rowBudget()));
            output.endSource();
            activeOrdinal.reset();
        }

        if (result.primary.exitCode == 0) {
            output.checkpoint();
            output.flush();
            output.checkpoint();
            resources(admission, options, result, output);
            output.checkpoint();
            output.write(sourceComplete(result.counters, output.available(true)), true);
            output.flush();
            output.checkpoint();
            result.status = SequenceStatus::COMPLETE;
        }
    } catch (const detail::SequenceAbort &cause) {
        const auto failed = failure(cause.code, cause.stage, cause.reason, activeOrdinal,
                                    cause.stage == "write" || cause.stage == "flush" ? "sink" : "sequence");

        primary(result, failed);

        if (cause.stage == "flush") {
            result.flush = failed;
        }
    } catch (const inspect::OutputLimit &) {
        primary(result, failure(6, "write", "output-limit", activeOrdinal, "sink"));
    } catch (const std::ios_base::failure &) {
        primary(result, failure(3, "io", "stream-failure", activeOrdinal, "sink"));
    } catch (const std::bad_alloc &) {
        primary(result, failure(6, "allocation", "allocation-failure", activeOrdinal, "sequence"));
    } catch (const std::length_error &) {
        primary(result, failure(6, "allocation", "length-limit", activeOrdinal, "sequence"));
    } catch (const std::invalid_argument &) {
        primary(result, failure(6, "configuration", "invalid-argument", activeOrdinal, "sequence"));
    } catch (...) {
        primary(result, failure(3, "io", "stream-failure", activeOrdinal, "sink"));
    }

    if (result.primary.exitCode != 0) {
        result.status = result.primary.exitCode == 7 ? SequenceStatus::STOPPED : SequenceStatus::FAILED;
        reportFailure(result, output);
    }

    result.outputChargedBytes = output.emittedBytes();

    return result;
}
} // namespace

TapeSequenceResult inspectSequence(std::span<const TapeSegmentSpec> segments, std::ostream &output,
                                   std::ostream &diagnostic, const TapeSequenceOptions &options, std::stop_token stop) {
    return runSequence(segments, output, diagnostic, options, stop, nullptr);
}

TapeSequenceResult detail::inspectSequenceForTest(std::span<const TapeSegmentSpec> segments, std::ostream &output,
                                                  std::ostream &diagnostic, const TapeSequenceOptions &options,
                                                  SequenceFileTest &test, std::stop_token stop) {
    return runSequence(segments, output, diagnostic, options, stop, &test);
}
} // namespace org::ttldtor::qdcxx::tape_inspect
