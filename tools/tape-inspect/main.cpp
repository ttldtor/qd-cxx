// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "../qtp-inspect/inspect.hpp"
#include "inspect.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <charconv>
#include <csignal>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif
namespace {
/** Validate a digits-only finite operand.
 * @param text Borrowed CLI value.
 * @param value Receives count on success.
 * @return True for a representable complete unsigned decimal.
 */
bool number(std::string_view text, std::size_t &value) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) {
            return c >= '0' && c <= '9';
        })) {
        return false;
    }

    auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
} // namespace

/** Run one explicit local file profile.
 * @param argc Number of process arguments.
 * @param argv Native named input/profile and finite options.
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
        ti::Limits limits;
        auto &t = limits.tape;
        std::map<std::string_view, std::size_t *> options{
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
            std::cout << "qdcxx-tape-inspect --input PATH --profile "
                         "oracle-sre-v1|default-quote-v1|default-profile-v1|oracle-field-types-v1 [--output-format "
                         "tsv|human] [--diagnostics-version 1|2] [--tape-mode legacy|raw-message-v1] [LIMIT "
                         "VALUE ...]\n"
                         "Explicit BINARY, uncompressed, time NONE, EMPTY_SET, STREAM_DATA, zero flags.\n"
                         "oracle-sre-v1: diagnostic S/R/E, exact type 8, diagnostics default 1.\n"
                         "default-quote-v1: composite Quote, exact types 2/8/56/120, diagnostics default 2.\n"
                         "default-profile-v1: complete Profile, exact types 8/10/56/88/120; output 5, diagnostics "
                         "default 2.\n"
                         "oracle-field-types-v1: diagnostic FieldTypes, exact 2/8/10/40/56/72/88/120; output 4, "
                         "diagnostics default 2.\n"
                         "--tape-mode raw-message-v1: explicit BINARY/RAW_DATA/MESSAGE, opt absent/empty/hs, zero "
                         "flags; timed output 3 (4 for oracle-field-types-v1, 5 for default-profile-v1).\n"
                         "Omitted tape-mode or legacy preserves the restricted NONE/STREAM_DATA policy.\n"
                         "No stdin, network, recovery or general tape support.\n"
                         "Numeric limits are finite unsigned decimal; chunk-size must be positive. Defaults:\n";

            for (const auto &[name, value] : options) {
                std::cout << name << ' ' << *value << '\n';
            }

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
        std::set<std::string_view> seen;
        bool valid = true;

        for (int i = 1; valid && i < argc; i += 2) {
            std::string_view key(argv[i]);

            if (i + 1 == argc || !seen.insert(key).second) {
                valid = false;
                break;
            }

            std::string_view operand(argv[i + 1]);

            if (key == "--input") {
                path = operand;
            } else if (key == "--profile") {
                profile = operand;
            } else if (key == "--output-format") {
                valid = operand == "tsv" || operand == "human";
                format = operand == "human" ? presentation::OutputFormat::HUMAN : presentation::OutputFormat::TSV;
            } else if (key == "--diagnostics-version") {
                valid = operand == "1" || operand == "2";
                diagnosticsVersion = operand == "2" ? 2U : 1U;
            } else if (key == "--tape-mode") {
                valid = operand == "legacy" || operand == "raw-message-v1";
                mode = operand == "raw-message-v1" ? protocol::TapeMode::RAW_MESSAGE_V1 : protocol::TapeMode::LEGACY;
            } else {
                auto option = options.find(key);
                valid = option != options.end() && number(operand, *option->second);
            }
        }

        const bool quoteProfile = profile == "default-quote-v1";
        const bool fieldProfile = profile == "oracle-field-types-v1";
        const bool standardProfile = profile == "default-profile-v1";
        const bool knownProfile = quoteProfile || fieldProfile || standardProfile || profile == "oracle-sre-v1";

        if ((quoteProfile || fieldProfile || standardProfile) && !seen.contains("--diagnostics-version")) {
            diagnosticsVersion = 2;
        }

        if (!valid || path.empty() || path == "-" || !knownProfile || !ti::prepareLimits(limits)) {
            ti::report(std::cerr, 2, "usage", diagnosticsVersion,
                       knownProfile ? protocol::TapeRejection::NONE : protocol::TapeRejection::LOCAL_PROFILE, format);

            return 2;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input) {
            ti::report(std::cerr, 3, "open", diagnosticsVersion, protocol::TapeRejection::NONE, format);

            return 3;
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
