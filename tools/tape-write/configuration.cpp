// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "configuration.hpp"

#include "../qtp-inspect/cli_number.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <limits>
#include <ostream>
#include <set>

namespace org::ttldtor::qdcxx::tape_write {
namespace {
/** One finite native option and its explicit default. */
struct LimitOption {
    std::string_view name; ///< Separate-token option spelling.
    std::size_t value;     ///< Finite policy default, independent of Java defaults.
};

/** Positions shared by option resolution and explicit library mapping. */
enum LimitIndex : std::size_t {
    INPUT_BYTES,             ///< Complete owned input bytes.
    OUTPUT_BYTES,            ///< Cumulative writer output bytes.
    FRAME_BYTES,             ///< One envelope body.
    FRAMES,                  ///< Envelope attempts per pass.
    RECORDS,                 ///< Data/description record work.
    FIELDS,                  ///< Wire field work.
    VALUES,                  ///< Decoded local values.
    DESCRIPTIONS,            ///< Retained declarations.
    DESCRIPTION_FIELDS,      ///< Retained declaration fields.
    NAME_UNITS,              ///< One declaration name.
    DESCRIPTION_NAME_UNITS,  ///< All declaration names.
    CONSTRUCTION_UNITS,      ///< Reader reconstruction work.
    ADMISSION_WORK,          ///< Additional preflight or composition work.
    ADMISSION_BYTES,         ///< Additional logical byte storage.
    ADMISSION_UNITS,         ///< Additional logical UTF-16 storage.
    WRITER_WORK,             ///< Composer visits.
    DESCRIPTOR_PROPERTIES,   ///< Combined descriptor pairs.
    DESCRIPTOR_MESSAGES,     ///< Combined message declarations.
    DESCRIPTOR_BYTES,        ///< One descriptor UTFString body.
    DESCRIPTOR_STRING_UNITS, ///< One descriptor UTFString units.
    DESCRIPTOR_UNITS,        ///< All retained descriptor text.
    TIMING_EVENTS,           ///< Explicit heartbeat/data metadata attempts.
    SYMBOL_UNITS,            ///< One symbol.
    TOTAL_SYMBOL_UNITS,      ///< Cumulative symbol visits/copies.
    FIELD_STRING_BYTES,      ///< One type-10 encoding.
    FIELD_STRING_UNITS,      ///< One type-10 value.
    FIELD_BYTES,             ///< Cumulative type-10 byte visits.
    FIELD_UNITS,             ///< Cumulative type-10 unit visits.
    OWNED_FIELD_UNITS,       ///< Decoded temporary/completed field units.
    SCRATCH_BYTES,           ///< Writer private byte storage.
    SCRATCH_UNITS            ///< Writer private UTF-16 storage.
};

constexpr std::array<LimitOption, 31> LIMIT_OPTIONS{{{"--max-input-bytes", 16777216},
                                                     {"--max-output-bytes", 16777216},
                                                     {"--max-frame-bytes", 1048576},
                                                     {"--max-frames", 65536},
                                                     {"--max-records", 1048576},
                                                     {"--max-fields", 8388608},
                                                     {"--max-values", 8388608},
                                                     {"--max-descriptions", 1024},
                                                     {"--max-description-fields", 16384},
                                                     {"--max-name-units", 1024},
                                                     {"--max-description-name-units", 1048576},
                                                     {"--max-construction-units", 67108864},
                                                     {"--max-admission-work-units", 67108864},
                                                     {"--max-admission-scratch-bytes", 16777216},
                                                     {"--max-admission-scratch-units", 4194304},
                                                     {"--max-writer-work-units", 67108864},
                                                     {"--max-descriptor-properties", 16},
                                                     {"--max-descriptor-messages", 1},
                                                     {"--descriptor-string-bytes", 65536},
                                                     {"--descriptor-string-units", 16384},
                                                     {"--max-descriptor-units", 65536},
                                                     {"--max-timing-events", 65536},
                                                     {"--symbol-units", 16384},
                                                     {"--max-symbol-units", 4194304},
                                                     {"--field-string-bytes", 65536},
                                                     {"--field-string-units", 16384},
                                                     {"--max-field-bytes", 16777216},
                                                     {"--max-field-units", 4194304},
                                                     {"--max-owned-field-units", 4194304},
                                                     {"--max-scratch-bytes", 1048585},
                                                     {"--max-scratch-units", 32768}}};
constexpr std::array<std::string_view, 7> SCHEMA_OPTIONS{
    "--max-schema-bytes",   "--max-schema-depth",      "--max-schema-records",         "--max-schema-fields",
    "--max-schema-lookups", "--max-schema-name-units", "--max-schema-total-name-units"};
constexpr std::size_t COMPACT_LENGTH_PREFIX_BYTES = 9; ///< Maximum complete compact-long envelope prefix.

/** Select native schema bound addresses without reading ambient settings.
 * @param limits Explicit native loading bounds.
 * @return Addresses valid while limits lives.
 */
std::array<std::size_t *, 7> schemaCounts(schema::JsonSchemaLimits &limits) {
    return {&limits.documentBytes,        &limits.depth,          &limits.schema.records,
            &limits.schema.fields,        &limits.schema.lookups, &limits.schema.nameUnits,
            &limits.schema.totalNameUnits};
}

/** Preserve literal native paths after process-boundary UTF-8 conversion on Windows.
 * @param value Separate nonempty path operand.
 * @return Owned native path, with no normalization or discovery.
 */
std::filesystem::path nativePath(std::string_view value) {
#ifdef _WIN32

    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
#else

    return std::filesystem::path(value);
#endif
}
} // namespace

ArgumentStatus parseArguments(int argc, char **argv, Configuration &result) {
    for (std::size_t i = 0; i < LIMIT_OPTIONS.size(); ++i) {
        result.limits[i] = LIMIT_OPTIONS[i].value;
    }

    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        return ArgumentStatus::HELP;
    }

    std::set<std::string_view> seen;
    std::string mode;
    const auto schemaValues = schemaCounts(result.schemaLimits);

    for (int i = 1; i < argc; i += 2) {
        const std::string_view key(argv[i]);

        if (key == "--help" || i + 1 == argc || !seen.insert(key).second) {
            return ArgumentStatus::INVALID;
        }

        const std::string_view value(argv[i + 1]);

        if (value.empty()) {
            return ArgumentStatus::INVALID;
        }

        if (key == "--input") {
            result.input = nativePath(value);
        } else if (key == "--output") {
            result.output = nativePath(value);
        } else if (key == "--schema") {
            result.schemaPath = nativePath(value);
        } else if (key == "--profile") {
            result.profile = value;
        } else if (key == "--tape-mode") {
            mode = value;
        } else {
            const auto option = std::find_if(LIMIT_OPTIONS.begin(), LIMIT_OPTIONS.end(), [&](const auto &entry) {
                return entry.name == key;
            });
            const auto schemaOption = std::find(SCHEMA_OPTIONS.begin(), SCHEMA_OPTIONS.end(), key);
            std::size_t *target = nullptr;

            if (option != LIMIT_OPTIONS.end()) {
                target = &result.limits[static_cast<std::size_t>(option - LIMIT_OPTIONS.begin())];
            } else if (schemaOption != SCHEMA_OPTIONS.end()) {
                target = schemaValues[static_cast<std::size_t>(schemaOption - SCHEMA_OPTIONS.begin())];
            }

            if (target == nullptr || !inspect::detail::parseUnsignedCount(value, *target)) {
                return ArgumentStatus::INVALID;
            }
        }
    }

    if (result.input.empty() || result.output.empty() || mode.empty() ||
        (result.schemaPath.empty() == result.profile.empty())) {
        return ArgumentStatus::INVALID;
    }

    if (mode != "raw-message-v1" ||
        (!result.profile.empty() && result.profile != "oracle-sre-v1" && result.profile != "default-quote-v1" &&
         result.profile != "default-profile-v1" && result.profile != "oracle-field-types-v1")) {
        return ArgumentStatus::UNSUPPORTED;
    }

    return ArgumentStatus::RUN;
}

void writeHelp(std::ostream &output) {
    output << "qdcxx-tape-write --input INPUT --output OUTPUT --tape-mode raw-message-v1 "
              "(--profile oracle-sre-v1|default-quote-v1|default-profile-v1|oracle-field-types-v1 | --schema PATH) "
              "[LIMIT VALUE ...]\n"
              "Strict complete offline admission; create-only publication to a fresh destination.\n"
              "All limits are finite unsigned decimal counts; zero is valid. Defaults:\n";

    for (const auto &option : LIMIT_OPTIONS) {
        output << "  " << option.name << ' ' << option.value << '\n';
    }

    schema::JsonSchemaLimits schemaLimits;
    const auto schemaValues = schemaCounts(schemaLimits);

    for (std::size_t i = 0; i < SCHEMA_OPTIONS.size(); ++i) {
        output << "  " << SCHEMA_OPTIONS[i] << ' ' << *schemaValues[i] << '\n';
    }
}

std::expected<protocol::AdmissionLimits, int> admissionLimits(const Configuration &configuration) {
    const auto &v = configuration.limits;

    if (v[FRAME_BYTES] > std::numeric_limits<std::size_t>::max() - COMPACT_LENGTH_PREFIX_BYTES ||
        v[FRAME_BYTES] > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        v[DESCRIPTOR_UNITS] > std::numeric_limits<std::size_t>::max() - v[DESCRIPTION_NAME_UNITS]) {
        return std::unexpected(6);
    }

    protocol::TapeLimits reader{{v[FRAME_BYTES], v[FRAME_BYTES] + COMPACT_LENGTH_PREFIX_BYTES},
                                v[INPUT_BYTES],
                                v[FRAMES],
                                {v[DESCRIPTOR_PROPERTIES],
                                 v[DESCRIPTOR_MESSAGES],
                                 {v[DESCRIPTOR_BYTES], v[DESCRIPTOR_STRING_UNITS]},
                                 v[DESCRIPTOR_UNITS]},
                                {configuration.schemaLimits.schema,
                                 v[DESCRIPTIONS],
                                 {v[DESCRIPTION_FIELDS], v[NAME_UNITS], v[DESCRIPTION_NAME_UNITS]}},
                                v[CONSTRUCTION_UNITS],
                                {v[FRAME_BYTES],
                                 v[RECORDS],
                                 v[FIELDS],
                                 v[RECORDS],
                                 v[VALUES],
                                 v[TOTAL_SYMBOL_UNITS],
                                 {v[FRAME_BYTES], v[SYMBOL_UNITS], v[SYMBOL_UNITS]},
                                 {v[FIELD_STRING_BYTES], v[FIELD_STRING_UNITS]},
                                 v[FIELD_BYTES],
                                 v[FIELD_UNITS],
                                 v[OWNED_FIELD_UNITS]},
                                v[TIMING_EVENTS]};

    return protocol::AdmissionLimits{v[INPUT_BYTES],
                                     reader,
                                     {configuration.schemaLimits.schema, v[DESCRIPTIONS], v[DESCRIPTION_FIELDS],
                                      v[NAME_UNITS], v[DESCRIPTION_NAME_UNITS]},
                                     v[ADMISSION_WORK],
                                     v[ADMISSION_BYTES],
                                     v[ADMISSION_UNITS]};
}

protocol::WriterLimits writerLimits(const Configuration &configuration) {
    const auto &v = configuration.limits;
    // Descriptor and name visits share the writer's documented text ledger.
    const auto textUnits = v[DESCRIPTOR_UNITS] + v[DESCRIPTION_NAME_UNITS];

    return {v[FRAME_BYTES],   v[OUTPUT_BYTES],
            v[FRAMES],        v[RECORDS],
            v[FIELDS],        {v[DESCRIPTOR_BYTES], v[DESCRIPTOR_STRING_UNITS]},
            textUnits,        {v[FIELD_STRING_BYTES], v[FIELD_STRING_UNITS]},
            v[FIELD_BYTES],   v[FIELD_UNITS],
            v[SYMBOL_UNITS],  v[TOTAL_SYMBOL_UNITS],
            v[SCRATCH_BYTES], v[SCRATCH_UNITS],
            v[WRITER_WORK]};
}

std::expected<core::LocalSchema, core::SchemaError> builtinSchema(const Configuration &configuration) {
    const auto limits = configuration.schemaLimits.schema;

    if (configuration.profile == "default-quote-v1") {
        return events::defaultQuoteSchema(limits);
    }

    if (configuration.profile == "default-profile-v1") {
        return events::defaultProfileSchema(limits);
    }

    if (configuration.profile == "oracle-field-types-v1") {
        constexpr std::array TYPES{10, 40, 72, 88, 8, 2, 56, 120, 10}; ///< Exact FIELD-TYPES diagnostic identities.
        constexpr std::array<std::u16string_view, 9> NAMES{u"Text", u"Short",   u"Sequence", u"Date", u"Count",
                                                           u"Char", u"Seconds", u"Wide",     u"Tail"};
        core::LocalRecord record{0, u"FieldTypes", false, {}, {}};

        for (std::size_t i = 0; i < TYPES.size(); ++i) {
            record.fields.push_back({TYPES[i]});
            record.lookups.push_back({std::u16string(NAMES[i]), i});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::EXTENDED_FIELDS_V1);
    }

    constexpr auto TYPE = std::to_underlying(core::SerialFieldType::COMPACT_INT);
    const std::vector<core::LocalRecord> records{
        {0, u"S", false, {{TYPE}}, {{u"v", 0}, {u"S.v", 0}}},
        {1,
         u"R",
         false,
         {{TYPE}, {TYPE}, {TYPE}},
         {{u"y", 0}, {u"R.y", 0}, {u"x", 1}, {u"R.x", 1}, {u"xdot", 1}, {u"z", 2}, {u"R.z", 2}}},
        {2, u"E", false, {}, {}}};

    return core::LocalSchema::create(records, limits);
}
} // namespace org::ttldtor::qdcxx::tape_write
