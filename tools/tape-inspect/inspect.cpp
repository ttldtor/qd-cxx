// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "inspect.hpp"
#include "../../src/encoding/compact_limits.hpp"
#include "catalog_segment.hpp"
#include "diagnostics.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
/// Exact diagnostic schema identities, shared by construction and presentation validation.
constexpr std::array FIELD_TYPES{
    core::SerialFieldType::UTF_CHAR_ARRAY, core::SerialFieldType::SHORT_STRING, core::SerialFieldType::SEQUENCE,
    core::SerialFieldType::DATE,           core::SerialFieldType::COMPACT_INT,  core::SerialFieldType::UTF_CHAR,
    core::SerialFieldType::TIME_SECONDS,   core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::UTF_CHAR_ARRAY};
constexpr std::array<std::u16string_view, 9> FIELD_NAMES{
    u"Text", u"Short",   u"Sequence", u"Date", u"Count",
    u"Char", u"Seconds", u"Wide",     u"Tail"}; ///< Exact logical lookup names.

/** Internal IO/resource exit, with no parser state. */
struct Failure {
    int code; ///< Native exit code.
};

/** Check that presentation can represent every field in the selected local schema.
 * @param schema Validated immutable schema.
 * @param profile Selected output grammar.
 * @return True for a legacy schema/display pair or the exact default Quote field shape.
 */
bool compatibleProfile(const core::LocalSchema &schema, inspect::DisplayProfile profile) noexcept {
    if (profile == inspect::DisplayProfile::EXTERNAL_SCHEMA_V1) {
        return schema.profile() == core::LocalProfile::EXTENDED_FIELDS_V1;
    }

    if (profile == inspect::DisplayProfile::DEFAULT_PROFILE_V1) {
        if (schema.profile() != core::LocalProfile::EXTENDED_FIELDS_V1 || schema.records().size() != 1) {
            return false;
        }

        const auto &record = schema.records().front();

        if (record.name != u"Profile" || record.fields.size() != events::PROFILE_FIELD_TYPES.size()) {
            return false;
        }

        for (std::size_t i = 0; i < record.fields.size(); ++i) {
            if (record.fields[i].serialTypeId != std::to_underlying(events::PROFILE_FIELD_TYPES[i])) {
                return false;
            }

            const auto name = events::PROFILE_FIELD_NAMES[i];

            if (std::none_of(record.lookups.begin(), record.lookups.end(), [&](const auto &lookup) {
                    return lookup.fieldId == i && lookup.name.size() == name.size() &&
                           std::equal(lookup.name.begin(), lookup.name.end(), name.begin());
                })) {
                return false;
            }
        }

        return true;
    }

    if (profile == inspect::DisplayProfile::ORACLE_FIELD_TYPES_V1) {
        if (schema.profile() != core::LocalProfile::EXTENDED_FIELDS_V1 || schema.records().size() != 1) {
            return false;
        }

        const auto &record = schema.records().front();

        if (record.name != u"FieldTypes" || record.fields.size() != FIELD_TYPES.size()) {
            return false;
        }

        for (std::size_t i = 0; i < FIELD_TYPES.size(); ++i) {
            if (record.fields[i].serialTypeId != std::to_underlying(FIELD_TYPES[i])) {
                return false;
            }
        }

        return true;
    }

    if (profile != inspect::DisplayProfile::DEFAULT_QUOTE_V1) {
        return schema.profile() == core::LocalProfile::COMPACT_INT_V1;
    }

    if (schema.profile() != core::LocalProfile::QUOTE_FIELDS_V1 || schema.records().size() != 1) {
        return false;
    }

    const auto &record = schema.records().front();

    if (record.name != u"Quote" || record.fields.size() != events::QUOTE_FIELD_TYPES.size()) {
        return false;
    }

    for (std::size_t i = 0; i < record.fields.size(); ++i) {
        if (record.fields[i].serialTypeId != static_cast<std::int32_t>(events::QUOTE_FIELD_TYPES[i])) {
            return false;
        }
    }

    return true;
}

/** Map a protocol failure to the native exit contract.
 * @param error Terminal category.
 * @return Nonzero process code.
 */
int exitCode(protocol::TapeError error) {
    if (error == protocol::TapeError::UNSUPPORTED) {
        return 4;
    }

    if (error == protocol::TapeError::RESOURCE_LIMIT || error == protocol::TapeError::ALLOCATION_FAILURE) {
        return 6;
    }

    return 5;
}

/** Emit exact structured parser diagnostics without throwing on broken stderr.
 * @param out Binary error destination.
 * @param d First parser failure.
 * @param version Diagnostic grammar, 1 or 2.
 * @param format Whether a bounded human explanation follows the structured row.
 * @param mode Selected container policy for the explanation.
 * @return Native nonzero error code.
 */
int diagnose(std::ostream &out, const protocol::TapeDiagnostic &d, unsigned version, inspect::OutputFormat format,
             protocol::TapeMode mode) noexcept {
    auto code = exitCode(d.error);
    try {
        out << (version == 2 ? "incomplete-v2\t" : "incomplete\t") << code << '\t' << d.stage << '\t' << d.detail
            << '\t' << d.frameStart << '\t' << d.accepted << '\t';

        if (d.payloadStart) {
            out << *d.payloadStart;
        } else {
            out << '-';
        }

        out << '\t';

        if (d.consumed) {
            out << *d.consumed;
        } else {
            out << '-';
        }

        out << '\t';

        if (d.errorOffset) {
            out << *d.errorOffset;
        } else {
            out << '-';
        }

        if (version == 2) {
            out << '\t' << tapeRejectionName(d.rejection);
        }

        out << '\n';

        if (version == 2 && format == inspect::OutputFormat::HUMAN) {
            out << "Reason: " << tapeRejectionExplanation(d.rejection, mode) << '\n';
        }

        out.flush();
    } catch (...) {
    }

    return code;
}
} // namespace

std::expected<core::LocalSchema, core::SchemaError> oracleFieldTypesSchema(core::SchemaLimits limits) {
    try {
        core::LocalRecord record{0, u"FieldTypes", false, {}, {}};

        for (std::size_t i = 0; i < FIELD_TYPES.size(); ++i) {
            record.fields.push_back({std::to_underlying(FIELD_TYPES[i])});
            record.lookups.push_back({std::u16string(FIELD_NAMES[i]), i});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::EXTENDED_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

bool prepareLimits(Limits &limits) noexcept {
    if (limits.chunkSize == 0 ||
        limits.chunkSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
        limits.tape.frame.maximumBodyBytes >
            std::numeric_limits<std::size_t>::max() - encoding::detail::MAX_COMPACT_LONG_BYTES ||
        limits.tape.frame.maximumBodyBytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }

    limits.tape.frame.maximumPendingBytes =
        limits.tape.frame.maximumBodyBytes + encoding::detail::MAX_COMPACT_LONG_BYTES;
    limits.tape.data.payloadBytes = limits.tape.frame.maximumBodyBytes;
    limits.tape.data.symbol.inputBytes = limits.tape.frame.maximumBodyBytes;
    limits.tape.data.symbol.stateUnits = limits.tape.data.symbol.codeUnits;
    limits.tape.data.emittedRecords = limits.tape.data.processedRecords;

    return true;
}

void report(std::ostream &output, int code, std::string_view stage, unsigned diagnosticsVersion,
            protocol::TapeRejection reason, inspect::OutputFormat format, protocol::TapeMode mode) noexcept {
    try {
        output << (diagnosticsVersion == 2 ? "incomplete-v2\t" : "incomplete\t") << code << '\t' << stage
               << "\t0\t-\t-\t-\t-\t-";

        if (diagnosticsVersion == 2) {
            output << '\t' << tapeRejectionName(reason);
        }

        output << '\n';

        if (diagnosticsVersion == 2 && format == inspect::OutputFormat::HUMAN) {
            output << "Reason: " << tapeRejectionExplanation(reason, mode) << '\n';
        }

        output.flush();
    } catch (...) {
    }
}

namespace {
/** Preserve the existing output8 IO/terminal contract around the shared concrete catalog operation.
 * @param input Borrowed binary source for this synchronous call.
 * @param output Borrowed bounded output destination.
 * @param diagnostic Existing bounded diagnostic grammar destination.
 * @param limits Existing per-file limits, resolved before reader construction.
 * @param format Existing safe record presentation.
 * @param diagnosticsVersion Existing grammar1 or2.
 * @return Existing exit code; this legacy operation does not own a physical file close.
 */
int inspectCatalogImpl(std::istream &input, std::ostream &output, std::ostream &diagnostic, Limits limits,
                       inspect::OutputFormat format, unsigned diagnosticsVersion) {
    constexpr auto MODE = protocol::TapeMode::BINARY_CATALOG_V1;
    std::string_view operation = "configuration";

    try {
        if (!prepareLimits(limits) || (diagnosticsVersion != 1 && diagnosticsVersion != 2)) {
            report(diagnostic, 2, "usage", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 2;
        }

        detail::CatalogOutput destination(output, diagnostic, limits.output, 0);
        std::size_t aggregateEmitted = 0;
        detail::CatalogSegment segment(limits, format, destination, aggregateEmitted);
        inspect::Formatter formatter(format, inspect::DisplayProfile::BINARY_CATALOG_V1);

        destination.write(formatter.tapeHeader(destination.rowBudget()));
        std::vector<char> buffer(limits.chunkSize);
        bool eof = false;

        while (!eof) {
            operation = "read";
            const auto remaining = limits.tape.inputBytes - static_cast<std::size_t>(segment.reader().acceptedBytes());

            if (remaining == 0) {
                const auto next = input.peek();

                if (input.bad() || (input.fail() && !input.eof())) {
                    throw Failure{3};
                }

                if (next != std::char_traits<char>::eof()) {
                    operation = "input";
                    throw Failure{6};
                }

                break;
            }

            const auto request = std::min(buffer.size(), remaining);

            input.read(buffer.data(), static_cast<std::streamsize>(request));
            const auto count = static_cast<std::size_t>(input.gcount());

            if (input.bad() || (input.fail() && !input.eof())) {
                throw Failure{3};
            }

            eof = input.eof();
            auto bytes = std::as_bytes(std::span(buffer.data(), count));

            while (!bytes.empty()) {
                const auto result = segment.feed(bytes);

                bytes = bytes.subspan(result.accepted);

                if (result.status == protocol::TapeStatus::FAILED) {
                    return diagnose(diagnostic, segment.reader().diagnostic(), diagnosticsVersion, format, MODE);
                }
            }
        }

        if (segment.finish() != protocol::TapeStatus::FINISHED) {
            return diagnose(diagnostic, segment.reader().diagnostic(), diagnosticsVersion, format, MODE);
        }

        operation = "flush";
        destination.flush();
        destination.write(formatter.tapeMarkers(segment.reader().markerCount(), destination.rowBudget()));
        destination.write(formatter.complete(segment.reader().acceptedBytes(), segment.reader().frameCount(),
                                             segment.emittedRecords(), segment.reader().processedRecords(),
                                             segment.reader().wireFields(), destination.rowBudget()));
        destination.flush();

        return 0;
    } catch (const inspect::OutputLimit &) {
        report(diagnostic, 6, "write", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const detail::SequenceAbort &cause) {
        report(diagnostic, cause.code, cause.stage, diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return cause.code;
    } catch (const Failure &cause) {
        report(diagnostic, cause.code, operation, diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return cause.code;
    } catch (const std::bad_alloc &) {
        report(diagnostic, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::length_error &) {
        report(diagnostic, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::invalid_argument &) {
        report(diagnostic, 6, "configuration", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::ios_base::failure &) {
        report(diagnostic, 3, operation, diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 3;
    }
}

/** Execute one synchronous bounded inspection, selecting schema ownership only at construction.
 * @param input Borrowed binary source.
 * @param output Borrowed output destination.
 * @param diagnostic Borrowed error destination.
 * @param schema Explicit schema, or null only for the wire discovery profile.
 * @param limits Finite IO and protocol bounds.
 * @param format Selected safe output representation.
 * @param profile Explicit output identity, consistent with the schema source.
 * @param diagnosticsVersion Existing diagnostic grammar.
 * @param mode Explicit container mode.
 * @return Native complete/error exit code.
 */
int inspectImpl(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema *schema,
                Limits limits, inspect::OutputFormat format, inspect::DisplayProfile profile,
                unsigned diagnosticsVersion, protocol::TapeMode mode) {
    std::string_view operation = "configuration";
    try {
        const bool catalog = profile == inspect::DisplayProfile::BINARY_CATALOG_V1;

        if (!prepareLimits(limits) || (diagnosticsVersion != 1 && diagnosticsVersion != 2) ||
            (catalog ? mode != protocol::TapeMode::BINARY_CATALOG_V1
                     : (mode != protocol::TapeMode::LEGACY && mode != protocol::TapeMode::RAW_MESSAGE_V1))) {
            report(diagnostic, 2, "usage", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 2;
        }

        const bool wire = schema == nullptr;

        if ((profile == inspect::DisplayProfile::WIRE_SCHEMA_V1 || catalog) != wire ||
            (!wire && !compatibleProfile(*schema, profile))) {
            report(diagnostic, 4, "profile", diagnosticsVersion, protocol::TapeRejection::LOCAL_PROFILE, format);

            return 4;
        }

        if (catalog) {
            return inspectCatalogImpl(input, output, diagnostic, limits, format, diagnosticsVersion);
        }

        std::optional<events::PreparedProfile> profileMapping;

        if (profile == inspect::DisplayProfile::DEFAULT_PROFILE_V1) {
            // The preset has exactly sixteen fields; construction remains bounded independently of file input.
            auto prepared = events::PreparedProfile::prepare(*schema, 0, {16, 64, 16, 32, 32, 128, 4096, 131072});

            if (!prepared) {
                const bool resource = prepared.error() == events::MappingError::RESOURCE_LIMIT ||
                                      prepared.error() == events::MappingError::ALLOCATION_FAILURE;
                report(diagnostic, resource ? 6 : 4, "profile", diagnosticsVersion,
                       protocol::TapeRejection::LOCAL_PROFILE, format);

                return resource ? 6 : 4;
            }

            profileMapping.emplace(std::move(*prepared));
        }

        std::optional<protocol::TapeReader> storage;

        if (wire) {
            storage.emplace(limits.tape, mode);
        } else {
            storage.emplace(*schema, limits.tape, mode);
        }

        auto &reader = *storage;
        const bool timed = mode == protocol::TapeMode::RAW_MESSAGE_V1;
        org::ttldtor::qdcxx::inspect::Formatter formatter(format, profile);
        std::size_t remainingOutput = limits.output;
        std::size_t remainingEventUnits = limits.eventUnits;
        std::size_t emitted = 0;
        std::size_t generation = 0;
        const auto emit = [&](const std::string &row) {
            operation = "write";

            if (row.size() > remainingOutput ||
                row.size() > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
                throw Failure{6};
            }

            remainingOutput -= row.size();
            output.write(row.data(), static_cast<std::streamsize>(row.size()));

            if (!output) {
                throw Failure{3};
            }
        };
        const auto flush = [&] {
            operation = "flush";
            output.flush();

            if (!output) {
                throw Failure{3};
            }
        };
        emit(timed ? formatter.rawTapeHeader(remainingOutput) : formatter.tapeHeader(remainingOutput));
        std::vector<char> buffer(limits.chunkSize);
        bool eof = false;

        while (!eof) {
            operation = "read";
            auto remaining = limits.tape.inputBytes - static_cast<std::size_t>(reader.acceptedBytes());

            if (remaining == 0) {
                auto next = input.peek();

                if (input.bad() || (input.fail() && !input.eof())) {
                    throw Failure{3};
                }

                if (next != std::char_traits<char>::eof()) {
                    operation = "input";
                    throw Failure{6};
                }

                break;
            }

            auto request = std::min(buffer.size(), remaining);
            input.read(buffer.data(), static_cast<std::streamsize>(request));
            auto count = static_cast<std::size_t>(input.gcount());

            if (input.bad() || (input.fail() && !input.eof())) {
                throw Failure{3};
            }

            eof = input.eof();
            auto bytes = std::as_bytes(std::span(buffer.data(), count));

            while (!bytes.empty()) {
                auto result = reader.feed(bytes);
                bytes = bytes.subspan(result.accepted);

                if (result.frame) {
                    auto index = reader.frameCount() - 1;
                    emit(formatter.tapeFrame(*result.frame, index, remainingOutput, mode));

                    if (index == 0 && reader.descriptor()) {
                        emit(timed ? formatter.rawTapeDescriptor(*reader.descriptor(), remainingOutput)
                                   : formatter.tapeDescriptor(*reader.descriptor(), remainingOutput));
                    }

                    if (result.heartbeat) {
                        emit(formatter.tapeHeartbeat(*result.heartbeat, index, remainingOutput));
                    }

                    if (result.timing) {
                        emit(formatter.tapeTiming(*result.timing, index, remainingOutput));
                    }

                    // A returned frame may contain a rejected batch. Publish metadata only after
                    // the reader has committed the complete description event successfully.
                    constexpr std::int32_t DESCRIBE_RECORDS = 2;

                    if (wire && result.frame->type == DESCRIBE_RECORDS &&
                        result.status != protocol::TapeStatus::FAILED) {
                        ++generation;
                        emit(formatter.wireSchema(index, generation, reader.schema().records().size(),
                                                  reader.descriptions().size(), remainingOutput));

                        for (const auto &record : reader.schema().records()) {
                            emit(formatter.wireLocalRecord(generation, record, remainingOutput));
                        }

                        std::size_t descriptionIndex = 0;

                        for (const auto &description : reader.descriptions()) {
                            const auto *plan = reader.context()->find(description.senderId);

                            if (plan == nullptr) {
                                throw std::invalid_argument("missing wire source plan");
                            }

                            emit(formatter.wireDescription(generation, descriptionIndex++, description, *plan,
                                                           remainingOutput));
                        }

                        emit(formatter.wireSchemaEnd(generation, remainingOutput));
                    }

                    for (const auto &record : result.records) {
                        if (profileMapping) {
                            operation = "mapping";
                            auto event =
                                profileMapping->map(record.symbol.units, {record.context, record.typedValues},
                                                    {limits.tape.data.symbol.codeUnits,
                                                     limits.tape.data.fieldString.codeUnits, remainingEventUnits},
                                                    result.timing ? result.timing->eventTimeMs() : 0);

                            if (!event) {
                                throw Failure{event.error() == events::MappingError::RESOURCE_LIMIT ||
                                                      event.error() == events::MappingError::ALLOCATION_FAILURE
                                                  ? 6
                                                  : 4};
                            }

                            remainingEventUnits -= event->symbol.size() +
                                                   (event->description ? event->description->size() : 0) +
                                                   (event->statusReason ? event->statusReason->size() : 0);
                            emit(formatter.profileRecord(
                                record, reader.schema().records()[static_cast<std::size_t>(record.localId)].name, index,
                                emitted, *event, remainingOutput));
                        } else {
                            emit(formatter.record(
                                record, reader.schema().records()[static_cast<std::size_t>(record.localId)].name, index,
                                emitted, remainingOutput, result.timing ? &*result.timing : nullptr, generation));
                        }

                        ++emitted;
                    }
                }

                if (result.status == protocol::TapeStatus::FAILED) {
                    return diagnose(diagnostic, reader.diagnostic(), diagnosticsVersion, format, mode);
                }
            }
        }

        if (reader.finish() != protocol::TapeStatus::FINISHED) {
            return diagnose(diagnostic, reader.diagnostic(), diagnosticsVersion, format, mode);
        }

        flush();
        emit(formatter.tapeMarkers(reader.markerCount(), remainingOutput));
        emit(formatter.complete(reader.acceptedBytes(), reader.frameCount(), emitted, reader.processedRecords(),
                                reader.wireFields(), remainingOutput));
        flush();

        return 0;
    } catch (const org::ttldtor::qdcxx::inspect::OutputLimit &) {
        report(diagnostic, 6, "write", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const Failure &f) {
        report(diagnostic, f.code, operation, diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return f.code;
    } catch (const std::bad_alloc &) {
        report(diagnostic, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::length_error &) {
        report(diagnostic, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::invalid_argument &) {
        report(diagnostic, 6, "configuration", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::ios_base::failure &) {
        report(diagnostic, 3, operation, diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 3;
    }
}
} // namespace

int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, inspect::OutputFormat format, inspect::DisplayProfile profile, unsigned diagnosticsVersion,
            protocol::TapeMode mode) {
    return inspectImpl(input, output, diagnostic, &schema, limits, format, profile, diagnosticsVersion, mode);
}

int inspectWire(std::istream &input, std::ostream &output, std::ostream &diagnostic, Limits limits,
                inspect::OutputFormat format, unsigned diagnosticsVersion, protocol::TapeMode mode) {
    return inspectImpl(input, output, diagnostic, nullptr, limits, format, inspect::DisplayProfile::WIRE_SCHEMA_V1,
                       diagnosticsVersion, mode);
}

int inspectBinaryWire(std::istream &input, std::ostream &output, std::ostream &diagnostic, Limits limits,
                      inspect::OutputFormat format, unsigned diagnosticsVersion) {
    return inspectCatalogImpl(input, output, diagnostic, limits, format, diagnosticsVersion);
}
} // namespace org::ttldtor::qdcxx::tape_inspect
