// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "inspect.hpp"

#include <algorithm>
#include <charconv>
#include <csignal>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>

#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif

namespace {
/** One numeric option and its policy storage member. */
struct Option {
    std::string_view name;                                     ///< Full exact CLI spelling.
    std::size_t org::ttldtor::qdcxx::inspect::Limits::*member; ///< Finite size_t budget member.
};

constexpr Option OPTIONS[]{
    {"--chunk-size", &org::ttldtor::qdcxx::inspect::Limits::chunkSize},
    {"--max-input", &org::ttldtor::qdcxx::inspect::Limits::input},
    {"--max-frame", &org::ttldtor::qdcxx::inspect::Limits::frame},
    {"--max-frames", &org::ttldtor::qdcxx::inspect::Limits::frames},
    {"--max-descriptions", &org::ttldtor::qdcxx::inspect::Limits::descriptions},
    {"--max-actions", &org::ttldtor::qdcxx::inspect::Limits::actions},
    {"--max-name-units", &org::ttldtor::qdcxx::inspect::Limits::nameUnits},
    {"--max-total-name-units", &org::ttldtor::qdcxx::inspect::Limits::totalNameUnits},
    {"--max-records", &org::ttldtor::qdcxx::inspect::Limits::records},
    {"--max-fields", &org::ttldtor::qdcxx::inspect::Limits::fields},
    {"--max-values", &org::ttldtor::qdcxx::inspect::Limits::values},
    {"--max-symbol-units", &org::ttldtor::qdcxx::inspect::Limits::symbolUnits},
    {"--max-output", &org::ttldtor::qdcxx::inspect::Limits::output},
    {"--message-records", &org::ttldtor::qdcxx::inspect::Limits::messageRecords},
    {"--message-fields", &org::ttldtor::qdcxx::inspect::Limits::messageFields},
    {"--message-values", &org::ttldtor::qdcxx::inspect::Limits::messageValues},
    {"--message-symbol-units", &org::ttldtor::qdcxx::inspect::Limits::messageSymbolUnits},
    {"--symbol-units", &org::ttldtor::qdcxx::inspect::Limits::symbol}}; ///< Supported exact numeric option spellings.

/** Validate a whole unsigned decimal operand before narrowing.
 * @param text Borrowed operand.
 * @param value Receives a valid count only on success.
 * @return True for digits-only input fitting size_t and uint64_t.
 */
bool number(std::string_view text, std::size_t &value) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) {
            return c >= '0' && c <= '9';
        })) {
        return false;
    }

    std::uint64_t parsed{};
    auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > std::numeric_limits<std::size_t>::max()) {
        return false;
    }

    value = static_cast<std::size_t>(parsed);

    return true;
}
} // namespace

/** Run only the bounded file profile, with binary stdout/stderr on Windows.
 * @param argc Argument count including executable name.
 * @param argv Named file, explicit profile and optional finite limits.
 * @return Documented process exit code; never success after output failure.
 */
int main(int argc, char **argv) {
#ifdef _WIN32

    if (_setmode(_fileno(stdout), _O_BINARY) == -1 || _setmode(_fileno(stderr), _O_BINARY) == -1) {
        org::ttldtor::qdcxx::inspect::report(std::cerr, 3, "open");

        return 3;
    }

#else
    // A closed pipe must surface as an output failure with the contract's nonzero exit.
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "qdcxx-qtp-inspect --profile oracle-sre-v1 --input PATH [--output-format tsv|human] [LIMIT "
                         "VALUE ...]\n"
                         "Raw offline QTP (not QD tape), type 2 before type 15, type 8 fields, zero flags.\n"
                         "Numeric limits: ASCII decimal, finite; chunk-size must be positive.\n"
                         "Defaults:\n";
            const org::ttldtor::qdcxx::inspect::Limits defaults;

            for (const auto &option : OPTIONS) {
                std::cout << option.name << ' ' << defaults.*(option.member) << '\n';
            }

            std::cout << "Exit: 0 success; 2 usage; 3 I/O; 4 unsupported; 5 malformed; 6 resource.\n"
                         "Output: default tsv (exact UTF-16 hex) or human (quoted safe UTF-8), LF, no BOM.\n"
                         "Human IDs/fields are local. No tape/compression/time-file support. Check exit status.\n";
            std::cout.flush();

            if (!std::cout) {
                org::ttldtor::qdcxx::inspect::report(std::cerr, 3, "flush");

                return 3;
            }

            return 0;
        }

        org::ttldtor::qdcxx::inspect::Limits limits;
        auto format = org::ttldtor::qdcxx::inspect::OutputFormat::TSV;
        std::string path;
        std::string profile;
        std::set<std::string_view> seen;
        bool valid = argc > 1;

        for (int i = 1; valid && i < argc; i += 2) {
            const std::string_view key(argv[i]);

            if (i + 1 == argc || !seen.insert(key).second) {
                valid = false;
                break;
            }

            const std::string_view operand(argv[i + 1]);

            if (key == "--input") {
                path = operand;
            } else if (key == "--profile") {
                profile = operand;
            } else if (key == "--output-format") {
                valid = operand == "tsv" || operand == "human";
                format = operand == "human" ? org::ttldtor::qdcxx::inspect::OutputFormat::HUMAN
                                            : org::ttldtor::qdcxx::inspect::OutputFormat::TSV;
            } else {
                const auto option = std::find_if(std::begin(OPTIONS), std::end(OPTIONS), [&](const auto &candidate) {
                    return candidate.name == key;
                });
                valid = option != std::end(OPTIONS) && number(operand, limits.*(option->member));
            }
        }

        if (!valid || profile != "oracle-sre-v1" || path.empty() || path == "-" ||
            !org::ttldtor::qdcxx::inspect::validLimits(limits)) {
            org::ttldtor::qdcxx::inspect::report(std::cerr, 2, "usage");

            return 2;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input) {
            org::ttldtor::qdcxx::inspect::report(std::cerr, 3, "open");

            return 3;
        }

        auto schema = org::ttldtor::qdcxx::inspect::oracleSchema();

        if (!schema) {
            org::ttldtor::qdcxx::inspect::report(std::cerr, 6, "allocation");

            return 6;
        }

        return org::ttldtor::qdcxx::inspect::inspect(input, std::cout, std::cerr, *schema, limits, format,
                                                     org::ttldtor::qdcxx::inspect::DisplayProfile::ORACLE_SRE_V1);
    } catch (const std::bad_alloc &) {
        org::ttldtor::qdcxx::inspect::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::length_error &) {
        org::ttldtor::qdcxx::inspect::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::ios_base::failure &) {
        org::ttldtor::qdcxx::inspect::report(std::cerr, 3, "write");

        return 3;
    }
}
