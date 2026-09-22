// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "inspect.hpp"
#include "diagnostics.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
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
 * @return Native nonzero error code.
 */
int diagnose(std::ostream &out, const protocol::TapeDiagnostic &d, unsigned version,
             inspect::OutputFormat format) noexcept {
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
            out << "Reason: " << tapeRejectionExplanation(d.rejection) << '\n';
        }

        out.flush();
    } catch (...) {
    }

    return code;
}
} // namespace

bool prepareLimits(Limits &limits) noexcept {
    constexpr std::size_t MAX_PREFIX = 9; // Maximum compact-long envelope prefix.

    if (limits.chunkSize == 0 ||
        limits.chunkSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
        limits.tape.frame.maximumBodyBytes > std::numeric_limits<std::size_t>::max() - MAX_PREFIX ||
        limits.tape.frame.maximumBodyBytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }

    limits.tape.frame.maximumPendingBytes = limits.tape.frame.maximumBodyBytes + MAX_PREFIX;
    limits.tape.data.payloadBytes = limits.tape.frame.maximumBodyBytes;
    limits.tape.data.symbol.inputBytes = limits.tape.frame.maximumBodyBytes;
    limits.tape.data.symbol.stateUnits = limits.tape.data.symbol.codeUnits;
    limits.tape.data.emittedRecords = limits.tape.data.processedRecords;

    return true;
}

void report(std::ostream &output, int code, std::string_view stage, unsigned diagnosticsVersion,
            protocol::TapeRejection reason, inspect::OutputFormat format) noexcept {
    try {
        output << (diagnosticsVersion == 2 ? "incomplete-v2\t" : "incomplete\t") << code << '\t' << stage
               << "\t0\t-\t-\t-\t-\t-";

        if (diagnosticsVersion == 2) {
            output << '\t' << tapeRejectionName(reason);
        }

        output << '\n';

        if (diagnosticsVersion == 2 && format == inspect::OutputFormat::HUMAN) {
            output << "Reason: " << tapeRejectionExplanation(reason) << '\n';
        }

        output.flush();
    } catch (...) {
    }
}

int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, inspect::OutputFormat format, inspect::DisplayProfile profile, unsigned diagnosticsVersion) {
    std::string_view operation = "configuration";
    try {
        if (!prepareLimits(limits) || (diagnosticsVersion != 1 && diagnosticsVersion != 2)) {
            report(diagnostic, 2, "usage", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 2;
        }

        if (!compatibleProfile(schema, profile)) {
            report(diagnostic, 4, "profile", diagnosticsVersion, protocol::TapeRejection::LOCAL_PROFILE, format);

            return 4;
        }

        protocol::TapeReader reader(schema, limits.tape);
        org::ttldtor::qdcxx::inspect::Formatter formatter(format, profile);
        std::size_t remainingOutput = limits.output;
        std::size_t emitted = 0;
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
        emit(formatter.tapeHeader(remainingOutput));
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
                    emit(formatter.tapeFrame(*result.frame, index, remainingOutput));

                    if (index == 0 && reader.descriptor()) {
                        emit(formatter.tapeDescriptor(*reader.descriptor(), remainingOutput));
                    }

                    for (const auto &record : result.records) {
                        emit(formatter.record(record, schema.records()[static_cast<std::size_t>(record.localId)].name,
                                              index, emitted, remainingOutput));
                        ++emitted;
                    }
                }

                if (result.status == protocol::TapeStatus::FAILED) {
                    return diagnose(diagnostic, reader.diagnostic(), diagnosticsVersion, format);
                }
            }
        }

        if (reader.finish() != protocol::TapeStatus::FINISHED) {
            return diagnose(diagnostic, reader.diagnostic(), diagnosticsVersion, format);
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
} // namespace org::ttldtor::qdcxx::tape_inspect
