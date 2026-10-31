// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "output.hpp"

#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::string_view HEX_DIGITS = "0123456789ABCDEF"; ///< Exact locale-independent uppercase alphabet.
constexpr events::MappingLimits MAPPING_LIMITS{128, 256, 32,    64,
                                               64,  128, 16384, 1000000}; ///< Finite prepared mapping work.

/** Copy within the same finite application schema profile before output traversal.
 * @param schema Immutable source with preserved provenance.
 * @return Independently bounded declaration copy, or static preparation failure.
 */
core::LocalSchema copySchema(const core::LocalSchema &schema) {
    auto result = schema.boundedCopy({16, 128, 256, 128, 16384});

    if (!result) {
        throw std::runtime_error("output schema limit");
    }

    return std::move(*result);
}

/** Check publication after each complete application output unit.
 * @param output Borrowed sink.
 * @throws std::runtime_error Static failure without copying stream/peer exception text.
 */
void check(std::ostream &output) {
    if (!output) {
        throw std::runtime_error("output sink failure");
    }
}

/** Write exact UTF-16 code units without Unicode normalization or surrogate replacement.
 * @param output Borrowed sink.
 * @param units Borrowed exact UTF-16, including empty or unpaired units.
 */
void text(std::ostream &output, std::u16string_view units) {
    for (const auto unit : units) {
        for (int shift = 12; shift >= 0; shift -= 4) {
            output.put(HEX_DIGITS[(unit >> shift) & 15]);
        }
    }
}

/** Emit an explicitly nullable exact text value; empty and null remain distinct cells.
 * @param output Borrowed sink.
 * @param value Borrowed nullable UTF-16.
 */
void nullableText(std::ostream &output, std::optional<std::u16string_view> value) {
    if (value) {
        text(output, *value);
    } else {
        output << "null";
    }
}

/** Write one optional integer without conflating absence and zero.
 * @tparam Integer Signed or unsigned printable integer type.
 * @param output Borrowed sink.
 * @param value Optional exact integer.
 */
template <class Integer> void nullableNumber(std::ostream &output, std::optional<Integer> value) {
    if (value) {
        output << *value;
    } else {
        output << "null";
    }
}

/** Deep-copy already bounded callback fields for an existing prepared event mapper.
 * @param record Borrowed generic values, preserved through this call.
 * @return Complete owned typed local-order values, including nullable payloads and extra fields.
 */
std::vector<core::FieldValue> copyFields(const client::BatchRecord &record) {
    std::vector<core::FieldValue> values;
    values.reserve(record.typedValues.size());

    for (const auto &field : record.typedValues) {
        core::FieldValue value{field.type, field.raw};

        if (field.units) {
            value.units.emplace(*field.units);
        }

        if (field.bytes) {
            value.bytes.emplace(field.bytes->begin(), field.bytes->end());
        }

        values.push_back(std::move(value));
    }

    if (record.typedValues.empty()) {
        for (const auto raw : record.values) {
            values.push_back({core::SerialFieldType::COMPACT_INT, raw});
        }
    }

    return values;
}

/** Emit complete generic record metadata followed by every local typed field.
 * @param output Borrowed sink.
 * @param record Borrowed immutable generic callback view.
 * @param ordinal One-based application record coordinate, scoped to this output run.
 */
void generic(std::ostream &output, const client::BatchRecord &record, std::size_t ordinal) {
    output << "record\t" << ordinal << '\t' << record.localId << '\t' << record.senderId << '\t' << record.revision
           << '\t' << record.bindingGeneration << '\t' << record.cipher << '\t' << record.eventFlags << '\t';
    text(output, record.symbol);
    output << '\t';
    nullableNumber(output, record.removalTime);
    output << '\t';
    nullableNumber(output, record.fieldTime ? std::optional(record.fieldTime->rawSeconds) : std::nullopt);
    output << '\t';
    nullableNumber(output, record.fieldTime ? record.fieldTime->rawSequence : std::nullopt);
    output << '\t';
    nullableNumber(output, record.fieldTime ? std::optional(record.fieldTime->sourceOrdinal) : std::nullopt);
    output << '\n';

    for (std::size_t index = 0; index < record.typedValues.size(); ++index) {
        const auto &field = record.typedValues[index];
        output << "field\t" << ordinal << '\t' << index << '\t' << static_cast<std::int32_t>(field.type) << '\t'
               << field.raw << '\t';
        nullableText(output, field.units);
        output << '\t';

        if (field.bytes) {
            for (const auto byte : *field.bytes) {
                const auto value = std::to_integer<unsigned>(byte);
                output.put(HEX_DIGITS[value >> 4]);
                output.put(HEX_DIGITS[value & 15]);
            }
        } else {
            output << "null";
        }

        output << '\n';
    }

    if (record.typedValues.empty()) {
        for (std::size_t index = 0; index < record.values.size(); ++index) {
            output << "field\t" << ordinal << '\t' << index << "\t8\t" << record.values[index] << "\tnull\tnull\n";
        }
    }
}
} // namespace

OutputConsumer::OutputConsumer(std::ostream &output, const core::LocalSchema &schema, OutputProfile profile)
    : output_(output), schema_(copySchema(schema)), profile_(profile) {
    if (profile == OutputProfile::QUOTE) {
        auto mapping = events::PreparedQuote::prepare(schema_, 0, MAPPING_LIMITS);

        if (!mapping) {
            throw std::runtime_error("Quote preparation failure");
        }

        quote_.emplace(std::move(*mapping));
    } else if (profile == OutputProfile::PROFILE) {
        auto mapping = events::PreparedProfile::prepare(schema_, 0, MAPPING_LIMITS);

        if (!mapping) {
            throw std::runtime_error("Profile preparation failure");
        }

        profileMapping_.emplace(std::move(*mapping));
    }
}

void OutputConsumer::begin() {
    output_ << (profile_ == OutputProfile::GENERIC ? "qdcxx-stream\t1\n" : "qdcxx-stream-summary\t1\n");

    if (profile_ == OutputProfile::GENERIC) {
        for (const auto &record : schema_.records()) {
            output_ << "schema\t" << record.id << '\t';
            text(output_, record.name);
            output_ << '\t' << record.fields.size() << '\n';

            for (const auto &lookup : record.lookups) {
                output_ << "layout\t" << record.id << '\t' << lookup.fieldId << '\t'
                        << record.fields[lookup.fieldId].serialTypeId << '\t';
                text(output_, lookup.name);
                output_ << '\n';
            }
        }
    }

    output_.flush();
    check(output_);
}

void OutputConsumer::onBatch(const client::BorrowedBatch &batch) {
    for (const auto &record : batch.records()) {
        if (record.localId < 0 || static_cast<std::size_t>(record.localId) >= schema_.records().size() ||
            !record.context.matches(schema_.recordContext(record.localId))) {
            throw std::runtime_error("record provenance failure");
        }

        if (ordinal_ == std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error("output ordinal exhausted");
        }

        ++ordinal_;

        if (profile_ == OutputProfile::GENERIC) {
            generic(output_, record, ordinal_);
        } else {
            const auto values = copyFields(record);

            if (quote_) {
                const auto quote = quote_->map(record.symbol, {record.context, values}, 128);

                if (!quote) {
                    throw std::runtime_error("Quote projection failure");
                }

                output_ << "quote\t" << ordinal_ << '\t';
                text(output_, quote->symbol);
                output_ << '\t' << quote->bidTimeMs << '\t' << quote->askTimeMs << '\t' << quote->bidPrice.raw << '\t'
                        << quote->askPrice.raw << '\t' << quote->timeMs << '\n';
            } else {
                const auto profile = profileMapping_->map(record.symbol, {record.context, values}, {128, 8192, 16512});

                if (!profile) {
                    throw std::runtime_error("Profile projection failure");
                }

                output_ << "profile\t" << ordinal_ << '\t';
                text(output_, profile->symbol);
                output_ << '\t' << profile->flags << '\t';
                nullableText(output_, profile->description ? std::optional<std::u16string_view>(*profile->description)
                                                           : std::nullopt);
                output_ << '\t';
                nullableText(output_, profile->statusReason ? std::optional<std::u16string_view>(*profile->statusReason)
                                                            : std::nullopt);
                output_ << '\n';
            }
        }

        // Flush inside the callback so failed publication remains a visible callback failure.
        output_.flush();
        check(output_);
    }
}

void OutputConsumer::flush() {
    output_.flush();
    check(output_);
}

int exitCode(const examples::StreamResult &result, bool targetRequested) noexcept {
    if (result.failure != examples::StreamFailure::NONE || !result.local || !result.transport ||
        result.local->outcome == client::DispatchOutcome::FAILED ||
        result.transport->primary.error != transport::TcpError::NONE ||
        result.transport->secondary.error != transport::TcpError::NONE) {
        return 1;
    }

    if (result.transport->peerEnded && !result.intervalExpired && !result.targetReached && !result.interrupted) {
        return 1;
    }

    if (result.interrupted) {
        return 130;
    }

    if (!result.addComposed || (result.removeApplied && !result.removeComposed) ||
        (targetRequested && !result.targetReached)) {
        return 2;
    }

    return 0;
}

void writeResult(std::ostream &output, const examples::StreamResult &result) {
    output << "application failure=" << static_cast<int>(result.failure) << " add-applied=" << result.addApplied
           << " add-composed=" << result.addComposed << " remove-applied=" << result.removeApplied
           << " remove-composed=" << result.removeComposed << " interval-expired=" << result.intervalExpired
           << " target-reached=" << result.targetReached << " interrupted=" << result.interrupted
           << " event-work-yields=" << result.eventWorkYields << '\n';

    if (result.local) {
        output << "local outcome=" << static_cast<int>(result.local->outcome)
               << " completed-records=" << result.local->counts.completedRecords
               << " discarded-records=" << result.local->counts.discardedRecords
               << " discarded-pending=" << result.local->counts.discardedPendingRecords
               << " unknown-eligibility=" << result.local->unknownEligibility << '\n';
    }

    if (result.transport) {
        const auto &terminal = *result.transport;
        output << "transport primary=" << static_cast<int>(terminal.primary.error)
               << " primary-stage=" << terminal.primary.stage << " primary-native=" << terminal.primary.nativeCode
               << " primary-protocol=" << static_cast<int>(terminal.primary.protocolError)
               << " secondary=" << static_cast<int>(terminal.secondary.error)
               << " secondary-stage=" << terminal.secondary.stage << " aborted=" << terminal.aborted
               << " secondary-native=" << terminal.secondary.nativeCode
               << " secondary-protocol=" << static_cast<int>(terminal.secondary.protocolError)
               << " peer-ended=" << terminal.peerEnded << " reported-write-bytes=" << terminal.reportedWriteBytes
               << " pending-output-bytes=" << terminal.pendingOutputBytes
               << " write-acceptance-unknown=" << terminal.writeAcceptanceUnknown
               << " discarded-input-bytes=" << terminal.discardedInputBytes << '\n';
    }

    if (result.diagnostic) {
        output << "diagnostic error=" << static_cast<int>(result.diagnostic->error)
               << " stage=" << result.diagnostic->stage << " native=" << result.diagnostic->nativeCode
               << " protocol=" << static_cast<int>(result.diagnostic->protocolError) << '\n';
    }

    output.flush();
    check(output);
}
} // namespace org::ttldtor::qdcxx::connect
