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
    std::string_view name;                    ///< Full exact CLI spelling.
    std::size_t qd::inspect::Limits::*member; ///< Finite size_t budget member.
};

constexpr Option OPTIONS[]{
    {"--chunk-size", &qd::inspect::Limits::chunkSize},
    {"--max-input", &qd::inspect::Limits::input},
    {"--max-frame", &qd::inspect::Limits::frame},
    {"--max-frames", &qd::inspect::Limits::frames},
    {"--max-descriptions", &qd::inspect::Limits::descriptions},
    {"--max-actions", &qd::inspect::Limits::actions},
    {"--max-name-units", &qd::inspect::Limits::nameUnits},
    {"--max-total-name-units", &qd::inspect::Limits::totalNameUnits},
    {"--max-records", &qd::inspect::Limits::records},
    {"--max-fields", &qd::inspect::Limits::fields},
    {"--max-values", &qd::inspect::Limits::values},
    {"--max-symbol-units", &qd::inspect::Limits::symbolUnits},
    {"--max-output", &qd::inspect::Limits::output},
    {"--message-records", &qd::inspect::Limits::messageRecords},
    {"--message-fields", &qd::inspect::Limits::messageFields},
    {"--message-values", &qd::inspect::Limits::messageValues},
    {"--message-symbol-units", &qd::inspect::Limits::messageSymbolUnits},
    {"--symbol-units", &qd::inspect::Limits::symbol}}; ///< Supported exact numeric option spellings.

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
        qd::inspect::report(std::cerr, 3, "open");

        return 3;
    }

#else
    // A closed pipe must surface as an output failure with the contract's nonzero exit.
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "qd-qtp-inspect --profile oracle-sre-v1 --input PATH [LIMIT VALUE ...]\n"
                         "Raw offline QTP, type 2 before type 15, type 8 fields, zero flags.\n"
                         "Numeric limits: ASCII decimal, finite; chunk-size must be positive.\n"
                         "Defaults:\n";
            const qd::inspect::Limits defaults;

            for (const auto &option : OPTIONS) {
                std::cout << option.name << ' ' << defaults.*(option.member) << '\n';
            }

            std::cout << "Exit: 0 success; 2 usage; 3 I/O; 4 unsupported; 5 malformed; 6 resource.\n"
                         "Output: version 1 TSV, exact UTF-16 hex, LF. Check exit status for completeness.\n";
            std::cout.flush();

            if (!std::cout) {
                qd::inspect::report(std::cerr, 3, "flush");

                return 3;
            }

            return 0;
        }

        qd::inspect::Limits limits;
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
            } else {
                const auto option = std::find_if(std::begin(OPTIONS), std::end(OPTIONS), [&](const auto &candidate) {
                    return candidate.name == key;
                });
                valid = option != std::end(OPTIONS) && number(operand, limits.*(option->member));
            }
        }

        if (!valid || profile != "oracle-sre-v1" || path.empty() || path == "-" || !qd::inspect::validLimits(limits)) {
            qd::inspect::report(std::cerr, 2, "usage");

            return 2;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input) {
            qd::inspect::report(std::cerr, 3, "open");

            return 3;
        }

        auto schema = qd::inspect::oracleSchema();

        if (!schema) {
            qd::inspect::report(std::cerr, 6, "allocation");

            return 6;
        }

        return qd::inspect::inspect(input, std::cout, std::cerr, *schema, limits);
    } catch (const std::bad_alloc &) {
        qd::inspect::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::length_error &) {
        qd::inspect::report(std::cerr, 6, "allocation");

        return 6;
    } catch (const std::ios_base::failure &) {
        qd::inspect::report(std::cerr, 3, "write");

        return 3;
    }
}
