// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "../qtp-inspect/inspect.hpp"
#include "inspect.hpp"

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
            {"--max-input", &t.inputBytes},
            {"--max-frame", &t.frame.maximumBodyBytes},
            {"--max-frames", &t.frames},
            {"--max-descriptions", &t.context.descriptions},
            {"--max-actions", &t.context.bindings.fields},
            {"--max-name-units", &t.context.bindings.nameUnits},
            {"--max-total-name-units", &t.context.bindings.totalNameUnits},
            {"--max-records", &t.data.processedRecords},
            {"--max-fields", &t.data.wireFields},
            {"--max-values", &t.data.emittedValues},
            {"--max-symbol-units", &t.data.ownedSymbolUnits},
            {"--symbol-units", &t.data.symbol.codeUnits},
            {"--max-construction", &t.constructionUnits},
            {"--max-properties", &t.descriptor.properties},
            {"--max-messages", &t.descriptor.messages},
            {"--max-descriptor-units", &t.descriptor.textUnits},
            {"--max-text-units", &t.descriptor.text.codeUnits}};

        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
            std::cout << "qdcxx-tape-inspect --input PATH --profile oracle-sre-v1 [--output-format tsv|human] [LIMIT "
                         "VALUE ...]\n"
                         "Explicit BINARY, uncompressed, time NONE, EMPTY_SET, STREAM_DATA, exact type 8, zero flags.\n"
                         "Diagnostic local S/R/E schema; no Quote, stdin, network, recovery or general tape support.\n"
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
        auto format = presentation::OutputFormat::TSV;
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
            } else {
                auto option = options.find(key);
                valid = option != options.end() && number(operand, *option->second);
            }
        }

        if (!valid || path.empty() || path == "-" || profile != "oracle-sre-v1" || !ti::prepareLimits(limits)) {
            ti::report(std::cerr, 2, "usage");

            return 2;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input) {
            ti::report(std::cerr, 3, "open");

            return 3;
        }

        auto schema = presentation::oracleSchema();

        if (!schema) {
            ti::report(std::cerr, 6, "allocation");

            return 6;
        }

        return ti::inspect(input, std::cout, std::cerr, *schema, limits, format,
                           presentation::DisplayProfile::ORACLE_SRE_V1);
    } catch (const std::bad_alloc &) {
        ti::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::length_error &) {
        ti::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::ios_base::failure &) {
        ti::report(std::cerr, 3, "write");

        return 3;
    }
}
