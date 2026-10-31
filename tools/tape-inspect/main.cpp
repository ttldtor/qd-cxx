// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "../qtp-inspect/cli_number.hpp"
#include "../qtp-inspect/inspect.hpp"
#include "inspect.hpp"
#include "qd/events/quote.hpp"
#include "qd/schema/json_schema.hpp"
#include "schema_input.hpp"
#include "source_cli.hpp"

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif

namespace {
constexpr std::size_t BROAD_FIELD_PAYLOAD_ENCODED_BYTES = 65536;
constexpr std::size_t BROAD_FIELD_PAYLOAD_BYTES = 65536;
constexpr std::size_t BROAD_PAYLOAD_ENCODED_BYTES = 16777216;
constexpr std::size_t BROAD_PAYLOAD_WORK_BYTES = 16777216;
constexpr std::size_t BROAD_OWNED_PAYLOAD_BYTES = 8388608;
constexpr std::size_t BROAD_SNAPSHOT_UNITS = 16777216;

/** Emit a bounded loader/source diagnostic without input text or protocol coordinates.
 * @param code Documented process exit for the loading failure.
 * @param token Static error name from the native schema contract.
 * @return The authoritative nonzero exit even if stderr is broken.
 */
int schemaError(int code, std::string_view token) noexcept {
    try {
        std::cerr << "schema-error\t1\t" << token << '\n';
        std::cerr.flush();
    } catch (...) {
    }

    return code;
}

/** Validate one explicitly selected binary schema without opening a tape or starting network work.
 * @param path Explicit local file path, borrowed only for this call and never printed.
 * @param limits Independent input/parser-request/logical work/schema-model bounds.
 * @return Zero only after a complete schema and successful fixed success-row write/flush; otherwise
 * a bounded schema diagnostic and the existing IO/unsupported/malformed/resource exit category.
 */
int validateBinarySchema(const std::string &path, org::ttldtor::qdcxx::schema::BinaryJsonSchemaLimits limits) {
#ifdef ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON
    namespace loader = org::ttldtor::qdcxx::schema;
    namespace ti = org::ttldtor::qdcxx::tape_inspect;

    try {
        std::ifstream input(path, std::ios::binary);

        if (!input) {
            return schemaError(3, "IO_ERROR");
        }

        auto document = ti::readSchemaDocument(input, limits.loading.documentBytes);

        if (!document) {
            const auto error = document.error();

            return schemaError(error == ti::SchemaInputError::IO_ERROR ? 3 : 6,
                               error == ti::SchemaInputError::IO_ERROR         ? "IO_ERROR"
                               : error == ti::SchemaInputError::RESOURCE_LIMIT ? "RESOURCE_LIMIT"
                                                                               : "ALLOCATION_FAILURE");
        }

        auto schema = loader::loadBinaryJsonSchema(*document, limits);

        if (!schema) {
            const auto error = schema.error();
            const int code =
                error == loader::JsonSchemaError::MALFORMED_JSON || error == loader::JsonSchemaError::INVALID_SCHEMA ? 5
                : error == loader::JsonSchemaError::RESOURCE_LIMIT ||
                        error == loader::JsonSchemaError::ALLOCATION_FAILURE
                    ? 6
                    : 4;

            return schemaError(code, loader::jsonSchemaErrorName(error));
        }

        std::cout << "schema-valid\t1\tqdcxx-native-binary\t1\n";
        std::cout.flush();

        return std::cout ? 0 : schemaError(3, "IO_ERROR");
    } catch (const std::bad_alloc &) {
        return schemaError(6, "ALLOCATION_FAILURE");
    } catch (const std::length_error &) {
        return schemaError(6, "ALLOCATION_FAILURE");
    } catch (const std::ios_base::failure &) {
        return schemaError(3, "IO_ERROR");
    }
#else
    (void)path;
    (void)limits;

    return schemaError(4, "LOADER_DISABLED");
#endif
}
} // namespace

/** Inspect one local tape with an explicit schema source.
 * @param argc Number of process arguments.
 * @param argv Native named input, schema source and finite options.
 * @return Documented exit, including nonzero help-output failure.
 */
int main(int argc, char **argv) {
    namespace ti = org::ttldtor::qdcxx::tape_inspect;
    namespace presentation = org::ttldtor::qdcxx::inspect;
    namespace protocol = org::ttldtor::qdcxx::protocol;
    unsigned diagnosticsVersion = 1;
    auto format = presentation::OutputFormat::TSV;
    auto mode = protocol::TapeMode::LEGACY;
#ifdef _WIN32

    if (_setmode(_fileno(stdout), _O_BINARY) == -1 || _setmode(_fileno(stderr), _O_BINARY) == -1) {
        return 3;
    }

#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        for (int index = 1; index < argc; index += 2) {
            if (std::string_view(argv[index]) != "--source-profile") {
                continue;
            }

            return ti::runSourceCli(argc, argv, std::cout, std::cerr);
        }

        ti::Limits limits;
        org::ttldtor::qdcxx::schema::JsonSchemaLimits schemaLimits;
        org::ttldtor::qdcxx::schema::BinaryJsonSchemaLimits binaryLimits;
        const std::map<std::string_view, std::size_t *> binaryOptions{
            {"--max-schema-parser-bytes", &binaryLimits.parserBytes},
            {"--max-schema-work", &binaryLimits.workUnits},
            {"--max-schema-model-bytes", &binaryLimits.schemaModelBytes}};
        auto &t = limits.tape;
        const std::map<std::string_view, std::pair<std::size_t *, std::size_t>> broadOptions{
            {"--field-payload-encoded-bytes", {&t.data.fieldPayloadEncodedBytes, BROAD_FIELD_PAYLOAD_ENCODED_BYTES}},
            {"--field-payload-bytes", {&t.data.fieldPayloadBytes, BROAD_FIELD_PAYLOAD_BYTES}},
            {"--max-payload-encoded-bytes", {&t.data.payloadEncodedBytes, BROAD_PAYLOAD_ENCODED_BYTES}},
            {"--max-payload-work-bytes", {&t.data.payloadWorkBytes, BROAD_PAYLOAD_WORK_BYTES}},
            {"--max-owned-payload-bytes", {&t.data.ownedPayloadBytes, BROAD_OWNED_PAYLOAD_BYTES}},
            {"--max-snapshot-units", {&t.snapshotUnits, BROAD_SNAPSHOT_UNITS}}};
        std::map<std::string_view, std::size_t *> options{
            {"--max-schema-bytes", &schemaLimits.documentBytes},
            {"--max-schema-depth", &schemaLimits.depth},
            {"--max-schema-records", &schemaLimits.schema.records},
            {"--max-schema-fields", &schemaLimits.schema.fields},
            {"--max-schema-lookups", &schemaLimits.schema.lookups},
            {"--max-schema-name-units", &schemaLimits.schema.nameUnits},
            {"--max-schema-total-name-units", &schemaLimits.schema.totalNameUnits},
            {"--chunk-size", &limits.chunkSize},
            {"--max-output", &limits.output},
            {"--max-event-units", &limits.eventUnits},
            {"--max-input", &t.inputBytes},
            {"--max-frame", &t.frame.maximumBodyBytes},
            {"--max-frames", &t.frames},
            {"--max-descriptions", &t.context.descriptions},
            {"--max-actions", &t.context.bindings.fields},
            {"--max-name-units", &t.context.bindings.nameUnits},
            {"--max-total-name-units", &t.context.bindings.totalNameUnits},
            {"--max-records", &t.data.processedRecords},
            {"--max-fields", &t.data.wireFields},
            {"--field-string-bytes", &t.data.fieldString.encodedBytes},
            {"--field-string-units", &t.data.fieldString.codeUnits},
            {"--max-field-bytes", &t.data.fieldBytes},
            {"--max-field-units", &t.data.fieldUnits},
            {"--max-owned-field-units", &t.data.ownedFieldUnits},
            {"--max-values", &t.data.emittedValues},
            {"--max-symbol-units", &t.data.ownedSymbolUnits},
            {"--symbol-units", &t.data.symbol.codeUnits},
            {"--max-construction", &t.constructionUnits},
            {"--max-properties", &t.descriptor.properties},
            {"--max-messages", &t.descriptor.messages},
            {"--max-descriptor-units", &t.descriptor.textUnits},
            {"--max-text-units", &t.descriptor.text.codeUnits},
            {"--max-timing-events", &t.timingEvents}};

        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            std::cout
                << "qdcxx-tape-inspect --input PATH (--schema PATH | --profile "
                   "oracle-sre-v1|default-quote-v1|default-profile-v1|oracle-field-types-v1 | --wire-schema "
                   "ordinary-v1|binary-catalog-v1) [--output-format "
                   "tsv|human] [--diagnostics-version 1|2] [--tape-mode legacy|raw-message-v1|binary-catalog-v1] "
                   "[LIMIT "
                   "VALUE ...]\n"
                   "qdcxx-tape-inspect --schema PATH --schema-format binary-v1 --validate-schema [SCHEMA-LIMIT VALUE "
                   "...]\n"
                   "Explicit BINARY, uncompressed, time NONE, EMPTY_SET, STREAM_DATA, zero flags.\n"
                   "oracle-sre-v1: diagnostic S/R/E, exact type 8, diagnostics default 1.\n"
                   "default-quote-v1: composite Quote, exact types 2/8/56/120, diagnostics default 2.\n"
                   "default-profile-v1: complete Profile, exact types 8/10/56/88/120; output 5, diagnostics "
                   "default 2.\n"
                   "oracle-field-types-v1: diagnostic FieldTypes, exact 2/8/10/40/56/72/88/120; output 4, "
                   "diagnostics default 2.\n"
                   "--schema PATH: native resolved JSON, generic typed output 6, diagnostics default 2.\n"
                   "--schema-format binary-v1 --validate-schema: validate qdcxx-native-binary profile1 only; "
                   "no tape/network work. Both flags are required; tape/output options are refused.\n"
                   "Success: schema-valid<TAB>1<TAB>qdcxx-native-binary<TAB>1; errors: static schema-error row.\n"
                   "Binary-only limits intersect input/count bounds: parser allocation-request bytes, logical "
                   "work units (not bytes), logical two-copy schema-model bytes (not total heap/RSS).\n"
                   "--wire-schema ordinary-v1: explicit ordinary exact 2/8/10/40/56/72/88/120 discovery; "
                   "self-describing output 7 with names, source IDs/order and successful generations.\n"
                   "--wire-schema binary-catalog-v1 --tape-mode binary-catalog-v1: explicit all19 finite fields, "
                   "RAW/TICKER/STREAM/HISTORY, flags/removal metadata, NONE/MESSAGE/headerless; generic typed "
                   "output8.\n"
                   "No indexed layout is inferred. Empty description batches publish no broad generation.\n"
                   "Wire schema limits apply before owned union construction; JSON byte/depth limits are loader-only.\n"
                   "Exactly one schema source is required; external schema loads before tape IO.\n"
                   "--tape-mode raw-message-v1: explicit BINARY/RAW_DATA/MESSAGE, opt absent/empty/hs, zero "
                   "flags; timed output 3 (4 for oracle-field-types-v1, 5 for default-profile-v1).\n"
                   "Omitted tape-mode or legacy preserves the restricted NONE/STREAM_DATA policy.\n"
                   "No stdin, network, recovery or general tape support.\n"
                   "Numeric limits are finite unsigned decimal; chunk-size must be positive. Defaults:\n";

            for (const auto &[name, value] : options) {
                std::cout << name << ' ' << *value << '\n';
            }

            std::cout << "Binary validation-only defaults (zero literal; equality admitted):\n";

            for (const auto &[name, value] : binaryOptions) {
                std::cout << name << ' ' << *value << '\n';
            }

            std::cout << "Broad-only defaults (bytes except snapshot metadata units; explicit0 stays0):\n";

            for (const auto &[name, option] : broadOptions) {
                std::cout << name << ' ' << option.second << '\n';
            }

            ti::sourceCliHelp(std::cout);

            std::cout << "Exit: 0 success/help; 2 usage; 3 IO; 4 unsupported; 5 malformed; 6 resource.\n"
                         "Empty markers are not authenticated footers. Check the exit status after final flush.\n";
            std::cout.flush();

            if (!std::cout) {
                ti::report(std::cerr, 3, "flush");

                return 3;
            }

            return 0;
        }

        std::string path;
        std::string profile;
        std::string schemaPath;
        std::string wireSchema;
        std::string schemaFormat;
        std::set<std::string_view> seen;
        bool valid = true;
        bool validateSchema = false;

        for (int i = 1; valid && i < argc; ++i) {
            std::string_view key(argv[i]);

            if (!seen.insert(key).second) {
                valid = false;
                break;
            }

            if (key == "--validate-schema") {
                validateSchema = true;
                continue;
            }

            if (i + 1 == argc) {
                valid = false;
                break;
            }

            std::string_view operand(argv[++i]);

            if (key == "--input") {
                path = operand;
            } else if (key == "--profile") {
                profile = operand;
            } else if (key == "--schema") {
                schemaPath = operand;
            } else if (key == "--schema-format") {
                schemaFormat = operand;
            } else if (key == "--wire-schema") {
                wireSchema = operand;
            } else if (key == "--output-format") {
                valid = operand == "tsv" || operand == "human";
                format = operand == "human" ? presentation::OutputFormat::HUMAN : presentation::OutputFormat::TSV;
            } else if (key == "--diagnostics-version") {
                valid = operand == "1" || operand == "2";
                diagnosticsVersion = operand == "2" ? 2U : 1U;
            } else if (key == "--tape-mode") {
                valid = operand == "legacy" || operand == "raw-message-v1" || operand == "binary-catalog-v1";
                mode = operand == "binary-catalog-v1" ? protocol::TapeMode::BINARY_CATALOG_V1
                       : operand == "raw-message-v1"  ? protocol::TapeMode::RAW_MESSAGE_V1
                                                      : protocol::TapeMode::LEGACY;
            } else {
                auto option = options.find(key);
                const auto broadOption = broadOptions.find(key);
                const auto binaryOption = binaryOptions.find(key);
                valid = option != options.end() ? presentation::detail::parseUnsignedCount(operand, *option->second)
                        : binaryOption != binaryOptions.end()
                            ? presentation::detail::parseUnsignedCount(operand, *binaryOption->second)
                            : broadOption != broadOptions.end() &&
                                  presentation::detail::parseUnsignedCount(operand, *broadOption->second.first);
            }
        }

        bool binarySelection = validateSchema || seen.contains("--schema-format");

        for (const auto &[name, value] : binaryOptions) {
            (void)value;
            binarySelection = binarySelection || seen.contains(name);
        }

        if (binarySelection) {
            valid = valid && validateSchema && schemaFormat == "binary-v1" && seen.contains("--schema") &&
                    !schemaPath.empty() && schemaPath != "-";

            for (const auto key : seen) {
                const bool schemaLimit = key.starts_with("--max-schema-");
                valid = valid &&
                        (key == "--schema" || key == "--schema-format" || key == "--validate-schema" || schemaLimit);
            }

            if (!valid) {
                ti::report(std::cerr, 2, "usage");

                return 2;
            }

            binaryLimits.loading = schemaLimits;

            return validateBinarySchema(schemaPath, binaryLimits);
        }

        const bool quoteProfile = profile == "default-quote-v1";
        const bool fieldProfile = profile == "oracle-field-types-v1";
        const bool standardProfile = profile == "default-profile-v1";
        const bool knownProfile = quoteProfile || fieldProfile || standardProfile || profile == "oracle-sre-v1";
        const bool external = seen.contains("--schema");
        const bool wire = seen.contains("--wire-schema");
        const bool catalog = mode == protocol::TapeMode::BINARY_CATALOG_V1;
        const auto sourceCount = static_cast<unsigned>(external) + static_cast<unsigned>(wire) +
                                 static_cast<unsigned>(seen.contains("--profile"));
        const bool validSource =
            sourceCount == 1 && (wire ? (catalog ? wireSchema == "binary-catalog-v1" : wireSchema == "ordinary-v1")
                                 : external ? !schemaPath.empty() && schemaPath != "-"
                                            : knownProfile);
        valid = valid && (!catalog || wire);

        for (const auto &[name, option] : broadOptions) {
            if (catalog) {
                if (!seen.contains(name)) {
                    *option.first = option.second;
                }
            } else if (seen.contains(name)) {
                valid = false;
            }
        }

        if ((wire || external || quoteProfile || fieldProfile || standardProfile) &&
            !seen.contains("--diagnostics-version")) {
            diagnosticsVersion = 2;
        }

        if (!valid || path.empty() || path == "-" || !validSource || !ti::prepareLimits(limits)) {
            ti::report(std::cerr, 2, "usage", diagnosticsVersion,
                       knownProfile ? protocol::TapeRejection::NONE : protocol::TapeRejection::LOCAL_PROFILE, format);

            return 2;
        }

        std::optional<org::ttldtor::qdcxx::core::LocalSchema> externalSchema;

        if (wire) {
            limits.tape.context.schema = schemaLimits.schema;
        }

        if (external) {
#ifdef ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON
            try {
                std::ifstream schemaInput(schemaPath, std::ios::binary);

                if (!schemaInput) {
                    return schemaError(3, "IO_ERROR");
                }

                auto document = ti::readSchemaDocument(schemaInput, schemaLimits.documentBytes);

                if (!document) {
                    switch (document.error()) {
                    case ti::SchemaInputError::IO_ERROR:
                        return schemaError(3, "IO_ERROR");
                    case ti::SchemaInputError::RESOURCE_LIMIT:
                        return schemaError(6, "RESOURCE_LIMIT");
                    case ti::SchemaInputError::ALLOCATION_FAILURE:
                        return schemaError(6, "ALLOCATION_FAILURE");
                    }
                }

                namespace loader = org::ttldtor::qdcxx::schema;
                auto loaded = loader::loadJsonSchema(*document, schemaLimits);

                if (!loaded) {
                    int code = 4;

                    switch (loaded.error()) {
                    case loader::JsonSchemaError::MALFORMED_JSON:
                    case loader::JsonSchemaError::INVALID_SCHEMA:
                        code = 5;
                        break;
                    case loader::JsonSchemaError::RESOURCE_LIMIT:
                    case loader::JsonSchemaError::ALLOCATION_FAILURE:
                        code = 6;
                        break;
                    default:
                        break;
                    }

                    return schemaError(code, loader::jsonSchemaErrorName(loaded.error()));
                }

                externalSchema.emplace(std::move(*loaded));
                limits.tape.context.schema = schemaLimits.schema;
            } catch (const std::bad_alloc &) {
                return schemaError(6, "ALLOCATION_FAILURE");
            } catch (const std::length_error &) {
                return schemaError(6, "ALLOCATION_FAILURE");
            } catch (const std::ios_base::failure &) {
                return schemaError(3, "IO_ERROR");
            }
#else

            return schemaError(4, "LOADER_DISABLED");
#endif
        }

        std::ifstream input(path, std::ios::binary);

        if (!input) {
            ti::report(std::cerr, 3, "open", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 3;
        }

        if (wire) {
            return catalog ? ti::inspectBinaryWire(input, std::cout, std::cerr, limits, format, diagnosticsVersion)
                           : ti::inspectWire(input, std::cout, std::cerr, limits, format, diagnosticsVersion, mode);
        }

        if (externalSchema) {
            return ti::inspect(input, std::cout, std::cerr, *externalSchema, limits, format,
                               presentation::DisplayProfile::EXTERNAL_SCHEMA_V1, diagnosticsVersion, mode);
        }

        auto schema = standardProfile ? org::ttldtor::qdcxx::events::defaultProfileSchema(limits.tape.context.schema)
                      : fieldProfile  ? ti::oracleFieldTypesSchema(limits.tape.context.schema)
                      : quoteProfile  ? org::ttldtor::qdcxx::events::defaultQuoteSchema(limits.tape.context.schema)
                                      : presentation::oracleSchema();

        if (!schema) {
            ti::report(std::cerr, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 6;
        }

        return ti::inspect(input, std::cout, std::cerr, *schema, limits, format,
                           standardProfile ? presentation::DisplayProfile::DEFAULT_PROFILE_V1
                           : fieldProfile  ? presentation::DisplayProfile::ORACLE_FIELD_TYPES_V1
                           : quoteProfile  ? presentation::DisplayProfile::DEFAULT_QUOTE_V1
                                           : presentation::DisplayProfile::ORACLE_SRE_V1,
                           diagnosticsVersion, mode);
    } catch (const std::bad_alloc &) {
        ti::report(std::cerr, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::length_error &) {
        ti::report(std::cerr, 6, "allocation", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 6;
    } catch (const std::ios_base::failure &) {
        ti::report(std::cerr, 3, "write", diagnosticsVersion, protocol::TapeRejection::NONE, format);

        return 3;
    }
}
