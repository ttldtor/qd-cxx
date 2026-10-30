// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "formatter.hpp"
#include "format_row.hpp"

#include "qd/events/quote.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <optional>
#include <stdexcept>

namespace org::ttldtor::qdcxx::inspect {
namespace {
constexpr std::int32_t DESCRIBE_PROTOCOL = 1;       ///< Tape header, named only by tape presentation.
constexpr std::int32_t DESCRIBE_RECORDS = 2;        ///< Initial wire descriptions.
constexpr std::int32_t STREAM_DATA = 15;            ///< Supported data message type.
constexpr std::uint32_t CATALOG_EVENT_FLAGS = 0x5F; ///< Six supported raw data-event bits.

using detail::build;
using detail::Row;

/** Validate the complete borrowed schema and values before counting or allocating a row. */
std::u16string_view validateCatalogRecord(const protocol::StreamDataRecord &record, const core::LocalSchema &schema,
                                          std::size_t generation) {
    if (schema.profile() != core::LocalProfile::BINARY_FIELDS_V1 || record.localId < 0 ||
        static_cast<std::size_t>(record.localId) >= schema.records().size()) {
        throw std::invalid_argument("invalid catalog schema or local ID");
    }

    const auto &declaration = schema.records()[static_cast<std::size_t>(record.localId)];

    if (!record.context.matches(schema.recordContext(record.localId)) ||
        record.typedValues.size() != declaration.fields.size()) {
        throw std::invalid_argument("catalog record context or shape mismatch");
    }

    for (std::size_t i = 0; i < declaration.fields.size(); ++i) {
        if (static_cast<std::int32_t>(record.typedValues[i].type) != declaration.fields[i].serialTypeId) {
            throw std::invalid_argument("catalog field identity mismatch");
        }
    }

    const std::u16string_view name = declaration.name;

    if (record.senderId < 0 || record.localId < 0 || generation == 0 || !record.context.valid() ||
        !record.values.empty() || (static_cast<std::uint32_t>(record.symbol.eventFlags) & ~CATALOG_EVENT_FLAGS) != 0) {
        throw std::invalid_argument("invalid catalog record metadata");
    }

    for (const auto &field : record.typedValues) {
        if (!core::validFieldValue(field)) {
            throw std::invalid_argument("noncanonical catalog value");
        }
    }

    return name;
}

/** Emit exact output8 typed-value cells, sharing all nineteen identities with output9. */
void catalogValues(Row &row, const protocol::StreamDataRecord &record, bool tsv) {
    for (std::size_t i = 0; i < record.typedValues.size(); ++i) {
        const auto &field = record.typedValues[i];

        if (i != 0) {
            row.add(",");
        }

        row.number(static_cast<std::int32_t>(field.type));
        row.add(":");
        row.number(field.raw);
        row.add(":");
        const bool text =
            field.type == core::SerialFieldType::UTF_CHAR_ARRAY || field.type == core::SerialFieldType::STRING;
        const bool bytes = field.type == core::SerialFieldType::BYTE_ARRAY ||
                           field.type == core::SerialFieldType::CUSTOM_OBJECT ||
                           field.type == core::SerialFieldType::SERIAL_OBJECT;

        if (!text) {
            row.add("-");
        } else if (!field.units) {
            row.add("null");
        } else if (field.units->empty()) {
            row.add("empty");
        } else if (tsv) {
            row.units(*field.units);
        } else {
            row.quote(*field.units);
        }

        row.add(":");

        if (!bytes) {
            row.add("-");
        } else if (!field.bytes) {
            row.add("null");
        } else if (field.bytes->empty()) {
            row.add("empty");
        } else {
            row.bytes(*field.bytes);
        }
    }
}

/** Emit shared source declaration rows; only a validated leading virtual prefix may lack targets. */
void wireDescriptionRows(Row &row, std::size_t generation, std::size_t index,
                         const protocol::WireRecordDescription &record, const protocol::StreamRecordPlan &plan,
                         bool tsv, std::size_t virtualFields) {
    const auto separator = tsv ? "\t" : " ";

    row.add("wire-record");
    row.add(separator);
    row.number(generation);
    row.add(separator);
    row.number(index);
    row.add(separator);
    row.number(record.senderId);
    row.add(separator);
    row.number(*plan.binding.localRecordId);
    row.add(separator);

    if (tsv) {
        row.units(record.name);
    } else {
        row.quote(record.name);
    }

    row.add(separator);
    row.number(record.fields.size());
    row.add("\n");

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto &action = plan.binding.fields[field];

        if ((!action.targetFieldId && field >= virtualFields) ||
            std::to_underlying(action.type) != record.fields[field].serialTypeId) {
            throw std::invalid_argument("wire source field binding");
        }

        row.add("wire-field");
        row.add(separator);
        row.number(generation);
        row.add(separator);
        row.number(record.senderId);
        row.add(separator);
        row.number(field);
        row.add(separator);

        if (tsv) {
            row.units(record.fields[field].name);
        } else {
            row.quote(record.fields[field].name);
        }

        row.add(separator);
        row.number(record.fields[field].serialTypeId);
        row.add(separator);

        if (action.targetFieldId) {
            row.number(*action.targetFieldId);
        } else {
            row.add("-");
        }

        row.add("\n");
    }
}
} // namespace

std::string Formatter::header(std::size_t budget) const {
    return build(budget, [&](Row &row) {
        if (format_ == OutputFormat::TSV) {
            row.add("format\tqd-qtp-inspect\t1\n");
        } else {
            row.add("qd-qtp-inspect human 1 profile=");
            row.add(profile_ == DisplayProfile::ORACLE_SRE_V1 ? "oracle-sre-v1" : "local-schema");
            row.add(" identities=local\n");
        }
    });
}

std::string Formatter::frame(const protocol::Frame &frame, std::size_t index, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        row.add(tsv ? "frame\t" : "Frame ");
        row.number(index);

        if (!tsv) {
            row.add(": ");
            row.add(!frame.type                       ? "EMPTY_HEARTBEAT"
                    : *frame.type == DESCRIBE_RECORDS ? "DESCRIBE_RECORDS"
                    : *frame.type == STREAM_DATA      ? "STREAM_DATA"
                                                      : "UNSUPPORTED");
            row.add(" (");

            if (frame.type) {
                row.number(*frame.type);
            } else {
                row.add("-");
            }

            row.add(") bytes=[");
        } else {
            row.add("\t");
        }

        row.number(frame.startOffset);
        row.add(tsv ? "\t" : ",");
        row.number(frame.endOffset);

        if (tsv) {
            row.add("\t");

            if (frame.type) {
                row.number(*frame.type);
            } else {
                row.add("-");
            }
        }

        row.add(tsv ? "\t" : ") payload=");

        if (frame.type) {
            row.number(frame.endOffset - frame.payload.size());
        } else {
            row.add("-");
        }

        row.add(tsv ? "\t" : " size=");
        row.number(frame.payload.size());
        row.add("\n");
    });
}

std::string Formatter::catalogRecord(const protocol::StreamDataRecord &record, const core::LocalSchema &schema,
                                     std::size_t frameIndex, std::size_t index, std::size_t budget,
                                     const protocol::TapeTiming *timing, std::size_t generation) const {
    if (record.fieldTime) {
        throw std::invalid_argument("unsupported FIELD metadata format");
    }

    if (profile_ != DisplayProfile::BINARY_CATALOG_V1) {
        throw std::invalid_argument("invalid catalog display profile");
    }

    const auto name = validateCatalogRecord(record, schema, generation);

    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const auto separator = tsv ? "\t" : " ";
        row.add(tsv ? "wire-data-v8\t" : "Fields8 ");
        row.number(frameIndex);
        row.add(separator);
        row.number(index);
        row.add(separator);
        row.number(generation);
        row.add(separator);
        row.number(record.senderId);
        row.add(separator);
        row.number(record.localId);
        row.add(separator);

        if (tsv) {
            row.units(name);
        } else {
            row.quote(name);
        }

        row.add(separator);

        if (tsv) {
            row.units(record.symbol.units);
        } else {
            row.quote(record.symbol.units);
        }

        row.add(separator);

        catalogValues(row, record, tsv);

        const protocol::TapeTiming empty{};
        const auto &clock = timing ? *timing : empty;
        row.add(separator);

        if (clock.timestampMs) {
            row.number(*clock.timestampMs);
        } else {
            row.add("-");
        }

        row.add(separator);
        row.number(clock.eventTimeSequence);
        row.add(separator);
        row.number(clock.eventTimeMs());
        row.add(separator);
        row.number(record.symbol.eventFlags);
        row.add(separator);

        if (record.removalTime) {
            row.number(*record.removalTime);
        } else {
            row.add("-");
        }

        row.add("\n");
    });
}

std::string Formatter::sourceCatalogRecord(const protocol::StreamDataRecord &record, const core::LocalSchema &schema,
                                           std::size_t frameIndex, std::size_t segmentRecordIndex,
                                           std::size_t aggregateRecordIndex, std::size_t generation, std::size_t budget,
                                           const tape_inspect::SourceRecordTiming &timing) const {
    if (profile_ != DisplayProfile::BINARY_CATALOG_V1) {
        throw std::invalid_argument("invalid source catalog display profile");
    }

    const auto name = validateCatalogRecord(record, schema, generation);
    std::string_view policy;

    switch (timing.policy) {
    case tape_inspect::SourceTimePolicy::NONE:
        policy = "none";
        break;
    case tape_inspect::SourceTimePolicy::MESSAGE:
        policy = "message";
        break;
    case tape_inspect::SourceTimePolicy::FIELD:
        policy = "field";
        break;
    case tape_inspect::SourceTimePolicy::LONG:
        policy = "long";
        break;
    case tape_inspect::SourceTimePolicy::TEXT:
        policy = "text";
        break;
    default:
        throw std::invalid_argument("invalid source time policy");
    }

    std::string_view externalOrigin;

    switch (timing.externalOrigin) {
    case tape_inspect::ExternalTimeOrigin::NONE:
        externalOrigin = "none";
        break;
    case tape_inspect::ExternalTimeOrigin::MESSAGE:
        externalOrigin = "message";
        break;
    case tape_inspect::ExternalTimeOrigin::SIDECAR:
        externalOrigin = "sidecar";
        break;
    default:
        throw std::invalid_argument("invalid external time origin");
    }

    if (timing.external.has_value() != (timing.externalOrigin != tape_inspect::ExternalTimeOrigin::NONE) ||
        (timing.external && !timing.external->timestampMs)) {
        throw std::invalid_argument("invalid external time assignment");
    }

    const protocol::TapeTiming empty{};
    const auto &external = timing.external ? *timing.external : empty;
    std::string_view effectiveOrigin = "zero";
    std::int64_t effectivePacked = 0;
    std::int64_t effectiveMs = 0;

    if (timing.policy != tape_inspect::SourceTimePolicy::NONE) {
        if (external.eventTimeSequence != 0) {
            effectiveOrigin = "external";
            effectivePacked = external.eventTimeSequence;
            effectiveMs = external.eventTimeMs();
        } else if (record.fieldTime) {
            effectiveOrigin = "field";
            effectivePacked = record.fieldTime->packed();
            effectiveMs = record.fieldTime->eventTimeMs();
        }
    }

    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const auto separator = tsv ? "\t" : " ";
        row.add(tsv ? "wire-data-v9" : "Fields9");

        for (const auto value :
             {timing.sourceOrdinal, frameIndex, segmentRecordIndex, aggregateRecordIndex, generation}) {
            row.add(separator);
            row.number(value);
        }

        row.add(separator);
        row.number(record.senderId);
        row.add(separator);
        row.number(record.localId);
        row.add(separator);

        if (tsv) {
            row.units(name);
        } else {
            row.quote(name);
        }

        row.add(separator);

        if (tsv) {
            row.units(record.symbol.units);
        } else {
            row.quote(record.symbol.units);
        }

        row.add(separator);
        catalogValues(row, record, tsv);
        row.add(separator);

        if (external.timestampMs) {
            row.number(*external.timestampMs);
        } else {
            row.add("-");
        }

        row.add(separator);
        row.number(external.eventTimeSequence);
        row.add(separator);
        row.number(external.eventTimeMs());
        row.add(separator);
        row.number(record.symbol.eventFlags);
        row.add(separator);

        if (record.removalTime) {
            row.number(*record.removalTime);
        } else {
            row.add("-");
        }

        row.add(separator);
        row.number(record.fieldTime ? 1 : 0);
        row.add(separator);

        if (record.fieldTime) {
            row.number(record.fieldTime->rawSeconds);
            row.add(separator);

            if (record.fieldTime->rawSequence) {
                row.number(*record.fieldTime->rawSequence);
            } else {
                row.add("-");
            }

            row.add(separator);
            row.number(record.fieldTime->sourceOrdinal);
            row.add(separator);
            row.number(record.fieldTime->packed());
            row.add(separator);
            row.number(record.fieldTime->eventTimeMs());
        } else {
            row.add("-");

            for (std::size_t i = 0; i < 4; ++i) {
                row.add(separator);
                row.add("-");
            }
        }

        row.add(separator);
        row.add(policy);
        row.add(separator);
        row.add(externalOrigin);
        row.add(separator);
        row.add(effectiveOrigin);
        row.add(separator);
        row.number(effectivePacked);
        row.add(separator);
        row.number(effectiveMs);
        row.add("\n");
    });
}

std::string Formatter::record(const protocol::StreamDataRecord &record, std::u16string_view name,
                              std::size_t frameIndex, std::size_t index, std::size_t budget,
                              const protocol::TapeTiming *timing, std::size_t generation) const {
    if (record.fieldTime) {
        throw std::invalid_argument("unsupported FIELD metadata format");
    }

    if (profile_ == DisplayProfile::BINARY_CATALOG_V1) {
        throw std::invalid_argument("catalog presentation requires a validated schema");
    }

    if (record.symbol.eventFlags != 0 || record.removalTime.has_value()) {
        throw std::invalid_argument("unsupported record metadata format");
    }

    const bool wire = profile_ == DisplayProfile::WIRE_SCHEMA_V1;

    if (profile_ == DisplayProfile::ORACLE_FIELD_TYPES_V1 || profile_ == DisplayProfile::EXTERNAL_SCHEMA_V1 || wire) {
        if (wire && (record.senderId < 0 || generation == 0)) {
            throw std::invalid_argument("missing source metadata");
        }

        if (!record.values.empty()) {
            throw std::invalid_argument("typed field representation");
        }

        for (const auto &field : record.typedValues) {
            if (!core::validFieldValue(field)) {
                throw std::invalid_argument("noncanonical field value");
            }

            if (field.type == core::SerialFieldType::BYTE_ARRAY || field.type == core::SerialFieldType::STRING ||
                field.type == core::SerialFieldType::CUSTOM_OBJECT ||
                field.type == core::SerialFieldType::SERIAL_OBJECT) {
                throw std::invalid_argument("unsupported field payload format");
            }
        }

        return build(budget, [&](Row &row) {
            const bool tsv = format_ == OutputFormat::TSV;
            row.add(tsv ? (wire ? "wire-data\t" : "field-record\t") : "Fields ");
            row.number(tsv ? frameIndex : index);
            row.add(tsv ? "\t" : ": frame=");
            row.number(tsv ? index : frameIndex);

            if (wire) {
                row.add(tsv ? "\t" : " generation=");
                row.number(generation);
                row.add(tsv ? "\t" : " sender=");
                row.number(record.senderId);
            }

            row.add(tsv ? "\t" : " local=");
            row.number(record.localId);
            row.add(tsv ? "\t" : " name=");

            if (tsv) {
                row.units(name);
            } else {
                row.quote(name);
            }

            row.add(tsv ? "\t" : " symbol=");

            if (tsv) {
                row.units(record.symbol.units);
            } else {
                row.quote(record.symbol.units);
            }

            row.add(tsv ? "\t" : " fields={");

            for (std::size_t i = 0; i < record.typedValues.size(); ++i) {
                const auto &field = record.typedValues[i];

                if (i) {
                    row.add(tsv ? "," : ", ");
                }

                if (!tsv) {
                    row.add("field[");
                    row.number(i);
                    row.add("]=(type:");
                }

                row.number(static_cast<std::int32_t>(field.type));
                row.add(tsv ? ":" : " raw:");
                row.number(field.raw);

                if (field.type == core::SerialFieldType::UTF_CHAR_ARRAY) {
                    row.add(tsv ? ":" : " units:");

                    if (!field.units) {
                        row.add("null");
                    } else if (tsv) {
                        if (field.units->empty()) {
                            row.add("empty");
                        } else {
                            row.units(*field.units);
                        }
                    } else {
                        row.quote(*field.units);
                    }
                } else if (tsv) {
                    row.add(":-");
                }

                if (!tsv) {
                    row.add(")");
                }
            }

            if (!tsv) {
                row.add("}");
            }

            if (wire && timing) {
                row.add(tsv ? "\t" : " timestamp-ms=");

                if (timing->timestampMs) {
                    row.number(*timing->timestampMs);
                } else {
                    row.add(tsv ? "-" : "absent");
                }

                row.add(tsv ? "\t" : " sequence=");
                row.number(timing->eventTimeSequence);
                row.add(tsv ? "\t" : " event-time-ms=");
                row.number(timing->eventTimeMs());
            }

            row.add("\n");
        });
    }

    if (profile_ == DisplayProfile::DEFAULT_QUOTE_V1) {
        // Mapping scalar fields needs no second owned copy of the decoded symbol.
        const auto quote = events::mapQuote({}, record.typedValues, 0);

        if (!quote) {
            throw std::invalid_argument("default Quote field shape");
        }

        const std::array<double, 4> decimalViews{quote->bidPrice.toDouble(), quote->bidSize.toDouble(),
                                                 quote->askPrice.toDouble(), quote->askSize.toDouble()};
        // Populate only when the count pass reaches the field, preserving output-budget precedence.
        std::array<std::optional<std::string>, events::QUOTE_FIELD_TYPES.size()> decimalText;

        return build(budget, [&](Row &row) {
            const bool tsv = format_ == OutputFormat::TSV;
            row.add(tsv ? "typed-record\t" : "Record ");
            row.number(tsv ? frameIndex : index);
            row.add(tsv ? "\t" : ": frame=");
            row.number(tsv ? index : frameIndex);
            row.add(tsv ? "\t" : " local=");
            row.number(record.localId);
            row.add(tsv ? "\t" : " name=");

            if (tsv) {
                row.units(name);
            } else {
                row.quote(name);
            }

            row.add(tsv ? "\t" : " symbol=");

            if (tsv) {
                row.units(record.symbol.units);
            } else {
                row.quote(record.symbol.units);
            }

            row.add(tsv ? "\t" : " fields={");

            for (std::size_t i = 0; i < record.typedValues.size(); ++i) {
                const auto &field = record.typedValues[i];

                if (i) {
                    row.add(tsv ? "," : ", ");
                }

                if (!tsv) {
                    row.add(events::QUOTE_FIELD_NAMES[i]);
                    row.add("=");

                    if (i == 0 || i == 4) {
                        row.number(i == 0 ? quote->bidTimeMs : quote->askTimeMs);
                        row.add(" ms");
                    } else if (i == 1 || i == 5) {
                        const auto exchange = i == 1 ? quote->bidExchangeCode : quote->askExchangeCode;
                        row.quote(std::u16string_view(&exchange, 1));
                    } else {
                        if (!decimalText[i]) {
                            decimalText[i] = core::WideDecimal{field.raw}.toString();
                        }

                        row.add(*decimalText[i]);
                    }

                    row.add(" (type:");
                }

                row.number(static_cast<std::int32_t>(field.type));
                row.add(tsv ? ":" : " raw:");
                row.number(field.raw);

                if (!tsv) {
                    row.add(")");
                }
            }

            row.add(tsv ? "\nquote\t" : "}\nQuote ");
            row.number(index);
            row.add(tsv ? "\t" : ": bidTimeMs=");
            row.number(quote->bidTimeMs);
            row.add(tsv ? "\t" : " bidExchangeCode=");
            row.number(static_cast<std::uint16_t>(quote->bidExchangeCode));
            row.add(tsv ? "\t" : " bidPriceBits=");
            row.doubleBits(decimalViews[0]);
            row.add(tsv ? "\t" : " bidSizeBits=");
            row.doubleBits(decimalViews[1]);
            row.add(tsv ? "\t" : " askTimeMs=");
            row.number(quote->askTimeMs);
            row.add(tsv ? "\t" : " askExchangeCode=");
            row.number(static_cast<std::uint16_t>(quote->askExchangeCode));
            row.add(tsv ? "\t" : " askPriceBits=");
            row.doubleBits(decimalViews[2]);
            row.add(tsv ? "\t" : " askSizeBits=");
            row.doubleBits(decimalViews[3]);
            row.add(tsv ? "\t" : " timeMs=");
            row.number(quote->timeMs);
            row.add(tsv ? "\t" : " sequence=");
            row.number(quote->sequence);
            row.add(tsv ? "\t" : " timeNanoPart=");
            row.number(quote->timeNanoPart);
            row.add(tsv ? "\t" : " eventTime=");
            row.number(timing ? timing->eventTimeMs() : quote->eventTime);
            row.add("\n");
        });
    }

    return build(budget, [&](Row &row) {
        if (format_ == OutputFormat::TSV) {
            row.add("record\t");
            row.number(frameIndex);
            row.add("\t");
            row.number(index);
            row.add("\t");
            row.number(record.localId);
            row.add("\t");
            row.units(name);
            row.add("\t");
            row.units(record.symbol.units);
            row.add("\t");

            if (record.values.empty()) {
                row.add("-");
            }

            for (std::size_t i = 0; i < record.values.size(); ++i) {
                if (i) {
                    row.add(",");
                }

                row.number(record.values[i]);
            }
        } else {
            row.add("Record ");
            row.number(index);
            row.add(": frame=");
            row.number(frameIndex);
            row.add(" local=");
            row.number(record.localId);
            row.add(" name=");
            row.quote(name);
            row.add(" symbol=");
            row.quote(record.symbol.units);
            row.add(" fields={");

            for (std::size_t i = 0; i < record.values.size(); ++i) {
                if (i) {
                    row.add(", ");
                }

                if (profile_ == DisplayProfile::ORACLE_SRE_V1 && record.localId == 0 && i == 0) {
                    row.add("S.v");
                } else if (profile_ == DisplayProfile::ORACLE_SRE_V1 && record.localId == 1 && i < 3) {
                    constexpr std::array<std::string_view, 3> LABELS{"R.y", "R.x", "R.z"};
                    row.add(LABELS[i]);
                } else {
                    row.add("field[");
                    row.number(i);
                    row.add("]");
                }

                row.add("=");
                row.number(record.values[i]);
            }

            row.add("}");
        }

        row.add("\n");
    });
}

std::string Formatter::profileRecord(const protocol::StreamDataRecord &record, std::u16string_view name,
                                     std::size_t frameIndex, std::size_t index, const events::Profile &event,
                                     std::size_t budget) const {
    if (record.fieldTime) {
        throw std::invalid_argument("unsupported FIELD metadata format");
    }

    if (record.symbol.eventFlags != 0 || record.removalTime.has_value()) {
        throw std::invalid_argument("unsupported record metadata format");
    }

    if (!record.values.empty() || record.typedValues.size() != events::PROFILE_FIELD_TYPES.size()) {
        throw std::invalid_argument("default Profile field shape");
    }

    for (std::size_t i = 0; i < record.typedValues.size(); ++i) {
        if (record.typedValues[i].type != events::PROFILE_FIELD_TYPES[i] ||
            !core::validFieldValue(record.typedValues[i])) {
            throw std::invalid_argument("default Profile canonical field");
        }
    }

    const std::array decimals{event.beta,
                              event.earningsPerShare,
                              event.dividendFrequency,
                              event.exDividendAmount,
                              event.high52WeekPrice,
                              event.low52WeekPrice,
                              event.shares,
                              event.freeFloat,
                              event.highLimitPrice,
                              event.lowLimitPrice};
    constexpr std::array<std::string_view, 10> LABELS{
        "beta",           "earningsPerShare", "dividendFrequency", "exDividendAmount", "high52WeekPrice",
        "low52WeekPrice", "shares",           "freeFloat",         "highLimitPrice",   "lowLimitPrice"};
    std::array<std::optional<std::string>, 10> decimalText;

    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const auto text = [&](std::u16string_view value) {
            if (tsv) {
                row.units(value);
            } else {
                row.quote(value);
            }
        };
        const auto nullable = [&](const std::optional<std::u16string> &value) {
            if (!value) {
                row.add("null");
            } else if (tsv && value->empty()) {
                row.add("empty");
            } else {
                text(*value);
            }
        };
        const auto scalar = [&](std::string_view label, auto value) {
            row.add(tsv ? "\t" : " ");

            if (!tsv) {
                row.add(label);
                row.add("=");
            }

            row.number(value);
        };
        row.add(tsv ? "profile-record\t" : "Profile ");
        row.number(tsv ? frameIndex : index);
        row.add(tsv ? "\t" : ": frame=");
        row.number(tsv ? index : frameIndex);
        scalar("local", record.localId);
        row.add(tsv ? "\t" : " name=");
        text(name);
        row.add(tsv ? "\t" : " symbol=");
        text(record.symbol.units);
        row.add(tsv ? "\t" : " fields={");

        for (std::size_t i = 0; i < record.typedValues.size(); ++i) {
            const auto &field = record.typedValues[i];

            if (i) {
                row.add(tsv ? "," : ", ");
            }

            if (!tsv) {
                row.add(events::PROFILE_FIELD_NAMES[i]);
                row.add("=(type:");
            }

            row.number(std::to_underlying(field.type));
            row.add(tsv ? ":" : " raw:");
            row.number(field.raw);

            if (field.type == core::SerialFieldType::UTF_CHAR_ARRAY) {
                row.add(tsv ? ":" : " units:");
                nullable(field.units);
            } else if (tsv) {
                row.add(":-");
            }

            if (!tsv) {
                row.add(")");
            }
        }

        row.add(tsv ? "\nprofile\t" : "}\nProfileValues ");
        row.number(index);

        if (!tsv) {
            row.add(":");
        }

        for (std::size_t i = 0; i < decimals.size(); ++i) {
            row.add(tsv ? "\t" : " ");

            if (tsv) {
                row.doubleBits(decimals[i].toDouble());
            } else {
                row.add(LABELS[i]);
                row.add("=");

                if (!decimalText[i]) {
                    decimalText[i] = decimals[i].toString();
                }

                row.add(*decimalText[i]);
            }

            if (i == 3) {
                scalar("exDividendDayId", event.exDividendDayId);
            }
        }

        scalar("haltStartTimeMs", event.haltStartTimeMs);
        scalar("haltEndTimeMs", event.haltEndTimeMs);
        scalar("flags", event.flags);
        row.add(tsv ? "\t" : " description=");
        nullable(event.description);
        row.add(tsv ? "\t" : " statusReason=");
        nullable(event.statusReason);
        scalar("tradingStatus", std::to_underlying(event.tradingStatus()));
        scalar("shortSaleRestriction", std::to_underlying(event.shortSaleRestriction()));
        scalar("eventTime", event.eventTime);
        row.add("\n");
    });
}

std::string Formatter::complete(std::uint64_t bytes, std::size_t frames, std::size_t records,
                                std::size_t chargedRecords, std::size_t fields, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        row.add(tsv ? "complete\t" : "Complete: bytes=");
        row.number(bytes);
        row.add(tsv ? "\t" : " frames=");
        row.number(frames);
        row.add(tsv ? "\t" : " records=");
        row.number(records);
        row.add(tsv ? "\t" : " charged-records=");
        row.number(chargedRecords);
        row.add(tsv ? "\t" : " wire-fields=");
        row.number(fields);
        row.add("\n");
    });
}

std::string Formatter::wireSchema(std::size_t frameIndex, std::size_t generation, std::size_t records,
                                  std::size_t descriptions, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const auto separator = format_ == OutputFormat::TSV ? "\t" : " ";

        row.add("schema");
        row.add(separator);
        row.number(frameIndex);
        row.add(separator);
        row.number(generation);
        row.add(separator);
        row.number(records);
        row.add(separator);
        row.number(descriptions);
        row.add("\n");
    });
}

std::string Formatter::wireLocalRecord(std::size_t generation, const core::LocalRecord &record,
                                       std::size_t budget) const {
    if (record.lookups.size() != record.fields.size()) {
        throw std::invalid_argument("wire local lookup shape");
    }

    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const auto separator = tsv ? "\t" : " ";

        row.add("schema-record");
        row.add(separator);
        row.number(generation);
        row.add(separator);
        row.number(record.id);
        row.add(separator);

        if (tsv) {
            row.units(record.name);
        } else {
            row.quote(record.name);
        }

        row.add(separator);
        row.number(record.hasTime ? 1 : 0);
        row.add(separator);
        row.number(record.fields.size());
        row.add("\n");

        for (std::size_t index = 0; index < record.fields.size(); ++index) {
            if (record.lookups[index].fieldId != index) {
                throw std::invalid_argument("wire local lookup order");
            }

            row.add("schema-field");
            row.add(separator);
            row.number(generation);
            row.add(separator);
            row.number(record.id);
            row.add(separator);
            row.number(index);
            row.add(separator);

            if (tsv) {
                row.units(record.lookups[index].name);
            } else {
                row.quote(record.lookups[index].name);
            }

            row.add(separator);
            row.number(record.fields[index].serialTypeId);
            row.add("\n");
        }
    });
}

std::string Formatter::wireDescription(std::size_t generation, std::size_t index,
                                       const protocol::WireRecordDescription &record,
                                       const protocol::StreamRecordPlan &plan, std::size_t budget) const {
    if (plan.binding.senderId != record.senderId || !plan.binding.localRecordId ||
        plan.binding.fields.size() != record.fields.size()) {
        throw std::invalid_argument("wire source binding");
    }

    return build(budget, [&](Row &row) {
        wireDescriptionRows(row, generation, index, record, plan, format_ == OutputFormat::TSV, 0);
    });
}

std::string Formatter::sourceWireDescription(std::size_t generation, std::size_t index,
                                             const protocol::WireRecordDescription &record,
                                             const protocol::StreamRecordPlan &plan, std::size_t budget) const {
    if (profile_ != DisplayProfile::BINARY_CATALOG_V1 || generation == 0 || record.senderId < 0 ||
        record.name.empty() || plan.binding.senderId != record.senderId || !plan.binding.localRecordId ||
        *plan.binding.localRecordId < 0 || !plan.context.valid() ||
        plan.binding.fields.size() != record.fields.size()) {
        throw std::invalid_argument("invalid source description binding");
    }

    std::size_t recognized = 0;

    if (!record.fields.empty() && record.fields[0].name == u"EventTime" &&
        record.fields[0].serialTypeId == std::to_underlying(core::SerialFieldType::TIME_SECONDS)) {
        recognized = 1;

        if (record.fields.size() > 1 && record.fields[1].name == u"EventSequence" &&
            record.fields[1].serialTypeId == std::to_underlying(core::SerialFieldType::SEQUENCE)) {
            recognized = 2;
        }
    }

    if (plan.eventTimeFieldCount != 0 && plan.eventTimeFieldCount != recognized) {
        throw std::invalid_argument("invalid source FIELD prefix");
    }

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto &declaration = record.fields[field];
        const auto &action = plan.binding.fields[field];
        const bool virtualField = field < plan.eventTimeFieldCount;

        if (declaration.name.empty() ||
            !core::supportsSerialType(declaration.serialTypeId, core::LocalProfile::BINARY_FIELDS_V1) ||
            std::to_underlying(action.type) != declaration.serialTypeId || (virtualField && action.targetFieldId) ||
            (!virtualField && (!action.targetFieldId || *action.targetFieldId >= plan.localFieldCount))) {
            throw std::invalid_argument("invalid source field action");
        }
    }

    return build(budget, [&](Row &row) {
        wireDescriptionRows(row, generation, index, record, plan, format_ == OutputFormat::TSV,
                            plan.eventTimeFieldCount);
    });
}

std::string Formatter::wireSchemaEnd(std::size_t generation, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        row.add(format_ == OutputFormat::TSV ? "schema-end\t" : "schema-end ");
        row.number(generation);
        row.add("\n");
    });
}

std::string Formatter::tapeHeader(std::size_t budget) const {
    return build(budget, [&](Row &row) {
        if (profile_ == DisplayProfile::BINARY_CATALOG_V1) {
            row.add(format_ == OutputFormat::TSV
                        ? "format\tqdcxx-tape-inspect\t8\tbinary-catalog-v1\tbinary-catalog-v1\n"
                        : "qdcxx-tape-inspect human 8 profile=binary-catalog-v1 tape-mode=binary-catalog-v1 "
                          "identities=local-and-sender\n");
        } else if (profile_ == DisplayProfile::WIRE_SCHEMA_V1) {
            row.add(
                format_ == OutputFormat::TSV
                    ? "format\tqdcxx-tape-inspect\t7\tordinary-v1\tlegacy\n"
                    : "qdcxx-tape-inspect human 7 profile=ordinary-v1 tape-mode=legacy identities=local-and-sender\n");
        } else if (profile_ == DisplayProfile::EXTERNAL_SCHEMA_V1) {
            row.add(format_ == OutputFormat::TSV
                        ? "format\tqdcxx-tape-inspect\t6\texternal-schema-v1\tlegacy\n"
                        : "qdcxx-tape-inspect human 6 profile=external-schema-v1 tape-mode=legacy identities=local\n");
        } else if (profile_ == DisplayProfile::DEFAULT_PROFILE_V1) {
            row.add(format_ == OutputFormat::TSV
                        ? "format\tqdcxx-tape-inspect\t5\tdefault-profile-v1\tlegacy\n"
                        : "qdcxx-tape-inspect human 5 profile=default-profile-v1 tape-mode=legacy identities=local\n");
        } else if (profile_ == DisplayProfile::ORACLE_FIELD_TYPES_V1) {
            row.add(
                format_ == OutputFormat::TSV
                    ? "format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\tlegacy\n"
                    : "qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=legacy identities=local\n");
        } else if (profile_ == DisplayProfile::DEFAULT_QUOTE_V1) {
            row.add(format_ == OutputFormat::TSV
                        ? "format\tqdcxx-tape-inspect\t2\tdefault-quote-v1\n"
                        : "qdcxx-tape-inspect human 2 profile=default-quote-v1 identities=local\n");
        } else if (format_ == OutputFormat::TSV) {
            row.add("format\tqdcxx-tape-inspect\t1\n");
        } else {
            row.add("qdcxx-tape-inspect human 1 profile=");
            row.add(profile_ == DisplayProfile::ORACLE_SRE_V1 ? "oracle-sre-v1" : "local-schema");
            row.add(" identities=local\n");
        }
    });
}

std::string Formatter::tapeDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        row.add(tsv ? "descriptor\t" : "Descriptor: type=tape opt=EMPTY_SET version=");

        if (tsv) {
            row.number(descriptor.version ? 1 : 0);
            row.add("\t");
            row.units(descriptor.version ? *descriptor.version : std::u16string_view{});
        } else if (descriptor.version) {
            row.quote(*descriptor.version);
        } else {
            row.add("absent");
        }

        row.add(tsv ? "\t" : " time=");
        row.add(descriptor.explicitTimeNone ? "NONE" : "absent");
        row.add(tsv ? "\t" : " send=");
        row.add(descriptor.explicitStreamSend ? "15" : "-");
        row.add(tsv ? "\t-\n" : " receive=-\n");
    });
}

std::string Formatter::tapeFrame(const protocol::Frame &value, std::size_t index, std::size_t budget,
                                 protocol::TapeMode mode) const {
    constexpr std::int32_t RAW_DATA = 5;  ///< The explicitly selected RAW tape data message.
    constexpr std::int32_t HEARTBEAT = 0; ///< Explicit timing envelope, distinct from an empty marker.
    const bool raw = mode == protocol::TapeMode::RAW_MESSAGE_V1;

    if (mode == protocol::TapeMode::BINARY_CATALOG_V1 && format_ == OutputFormat::HUMAN && value.type) {
        std::string_view name;

        switch (*value.type) {
        case 0:
            name = "HEARTBEAT";
            break;
        case 1:
            name = "DESCRIBE_PROTOCOL";
            break;
        case 2:
            name = "DESCRIBE_RECORDS";
            break;
        case 3:
            name = "DESCRIBE_RESERVED";
            break;
        case 5:
            name = "RAW_DATA";
            break;
        case 10:
            name = "TICKER_DATA";
            break;
        case 15:
            name = "STREAM_DATA";
            break;
        case 20:
            name = "HISTORY_DATA";
            break;
        default:
            break;
        }

        if (!name.empty()) {
            return build(budget, [&](Row &row) {
                row.add("Frame ");
                row.number(index);
                row.add(": ");
                row.add(name);
                row.add(" (");
                row.number(*value.type);
                row.add(") bytes=[");
                row.number(value.startOffset);
                row.add(",");
                row.number(value.endOffset);
                row.add(") payload=");
                row.number(value.endOffset - value.payload.size());
                row.add(" size=");
                row.number(value.payload.size());
                row.add("\n");
            });
        }
    }

    if (format_ == OutputFormat::TSV ||
        (value.type != DESCRIBE_PROTOCOL && !(raw && (value.type == RAW_DATA || value.type == HEARTBEAT)))) {
        return frame(value, index, budget);
    }

    return build(budget, [&](Row &row) {
        row.add("Frame ");
        row.number(index);
        row.add(value.type == RAW_DATA    ? ": RAW_DATA (5) bytes=["
                : value.type == HEARTBEAT ? ": HEARTBEAT (0) bytes=["
                                          : ": DESCRIBE_PROTOCOL (1) bytes=[");
        row.number(value.startOffset);
        row.add(",");
        row.number(value.endOffset);
        row.add(") payload=");
        row.number(value.endOffset - value.payload.size());
        row.add(" size=");
        row.number(value.payload.size());
        row.add("\n");
    });
}

std::string Formatter::catalogDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t index,
                                         std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const auto separator = tsv ? "\t" : " ";
        row.add(tsv ? "descriptor-v8\t" : "Descriptor8 ");
        row.number(index);

        for (const auto *value : {&descriptor.version, &descriptor.options, &descriptor.time}) {
            row.add(separator);
            row.number(value->has_value() ? 1 : 0);
            row.add(separator);

            if (tsv) {
                row.units(*value ? **value : std::u16string_view{});
            } else if (*value) {
                row.quote(**value);
            } else {
                row.add("-");
            }
        }

        const auto sends =
            (descriptor.explicitRawSend ? 1U << 5 : 0U) | (descriptor.explicitTickerSend ? 1U << 10 : 0U) |
            (descriptor.explicitStreamSend ? 1U << 15 : 0U) | (descriptor.explicitHistorySend ? 1U << 20 : 0U);
        row.add(separator);
        row.number(sends);
        row.add("\n");
    });
}

std::string Formatter::rawTapeHeader(std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;

        if (profile_ == DisplayProfile::WIRE_SCHEMA_V1) {
            row.add(tsv ? "format\tqdcxx-tape-inspect\t7\tordinary-v1\traw-message-v1\n"
                        : "qdcxx-tape-inspect human 7 profile=ordinary-v1 tape-mode=raw-message-v1 "
                          "identities=local-and-sender\n");

            return;
        }

        if (profile_ == DisplayProfile::EXTERNAL_SCHEMA_V1) {
            row.add(tsv ? "format\tqdcxx-tape-inspect\t6\texternal-schema-v1\traw-message-v1\n"
                        : "qdcxx-tape-inspect human 6 profile=external-schema-v1 tape-mode=raw-message-v1 "
                          "identities=local\n");

            return;
        }

        if (profile_ == DisplayProfile::DEFAULT_PROFILE_V1) {
            row.add(tsv ? "format\tqdcxx-tape-inspect\t5\tdefault-profile-v1\traw-message-v1\n"
                        : "qdcxx-tape-inspect human 5 profile=default-profile-v1 tape-mode=raw-message-v1 "
                          "identities=local\n");

            return;
        }

        if (profile_ == DisplayProfile::ORACLE_FIELD_TYPES_V1) {
            row.add(tsv ? "format\tqdcxx-tape-inspect\t4\toracle-field-types-v1\traw-message-v1\n"
                        : "qdcxx-tape-inspect human 4 profile=oracle-field-types-v1 tape-mode=raw-message-v1 "
                          "identities=local\n");

            return;
        }

        row.add(tsv ? "format\tqdcxx-tape-inspect\t3\t" : "qdcxx-tape-inspect human 3 profile=");
        row.add(profile_ == DisplayProfile::DEFAULT_QUOTE_V1 ? "default-quote-v1"
                : profile_ == DisplayProfile::ORACLE_SRE_V1  ? "oracle-sre-v1"
                                                             : "local-schema");
        row.add(tsv ? "\traw-message-v1\n" : " tape-mode=raw-message-v1 identities=local\n");
    });
}

std::string Formatter::rawTapeDescriptor(const protocol::TapeDescriptor &descriptor, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        const std::string_view options = !descriptor.options ? "absent" : descriptor.options->empty() ? "empty" : "hs";
        row.add(tsv ? "descriptor-v3\t" : "Descriptor: type=tape opt=");

        if (tsv) {
            row.number(descriptor.version ? 1 : 0);
            row.add("\t");
            row.units(descriptor.version ? *descriptor.version : std::u16string_view{});
        } else {
            row.add(options);
            row.add(" version=");

            if (descriptor.version) {
                row.quote(*descriptor.version);
            } else {
                row.add("absent");
            }
        }

        row.add(tsv ? "\t" : " time=");
        row.add(descriptor.time == u"message" ? "message" : "MESSAGE");
        row.add(tsv ? "\t" : " send=");
        row.add(descriptor.explicitRawSend ? "5" : "-");
        row.add(tsv ? "\t-\t" : " receive=-");

        if (tsv) {
            row.add(options);
        }

        row.add("\n");
    });
}

std::string Formatter::tapeHeartbeat(const protocol::TapeHeartbeat &heartbeat, std::size_t index,
                                     std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        row.add(tsv ? "heartbeat\t" : "Heartbeat: frame=");
        row.number(index);
        row.add(tsv ? "\t" : " content=");

        if (heartbeat.content) {
            row.number(*heartbeat.content);
        } else {
            row.add("-");
        }

        row.add(tsv ? "\t" : " timestampMs=");

        if (heartbeat.timestampMs) {
            row.number(*heartbeat.timestampMs);
        } else {
            row.add("-");
        }

        row.add("\n");
    });
}

std::string Formatter::tapeTiming(const protocol::TapeTiming &timing, std::size_t index, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        const bool tsv = format_ == OutputFormat::TSV;
        row.add(tsv ? "timing\t" : "Timing: frame=");
        row.number(index);
        row.add(tsv ? "\t" : " timestampMs=");

        if (timing.timestampMs) {
            row.number(*timing.timestampMs);
        } else {
            row.add("-");
        }

        row.add(tsv ? "\t" : " eventTimeSequence=");
        row.number(timing.eventTimeSequence);
        row.add(tsv ? "\t" : " eventTimeMs=");
        row.number(timing.eventTimeMs());
        row.add("\n");
    });
}

std::string Formatter::tapeMarkers(std::size_t markers, std::size_t budget) const {
    return build(budget, [&](Row &row) {
        row.add(format_ == OutputFormat::TSV ? "markers\t" : "Markers: count=");
        row.number(markers);
        row.add(format_ == OutputFormat::TSV ? "\n" : " authenticated-footer=no\n");
    });
}
} // namespace org::ttldtor::qdcxx::inspect
