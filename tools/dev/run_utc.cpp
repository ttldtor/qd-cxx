// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "process.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <stdexcept>

namespace org::ttldtor::qdcxx::dev {
namespace {
constexpr std::string_view HELP = "Usage: qdcxx-run-utc --log PATH -- COMMAND [ARG ...]\n"
                                  "Run with child-only TZ=UTC and timestamped UTF-8 text logging.\n";
constexpr std::string_view REPLACEMENT = "\xef\xbf\xbd";

/** Produces UTC milliseconds without changing process or host timezone.
 * @return ISO 8601 UTC timestamp.
 */
std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char date[32]{};
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &utc);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    char fraction[8]{};
    std::snprintf(fraction, sizeof(fraction), ".%03dZ", static_cast<int>(milliseconds));

    return std::string(date) + fraction;
}

/** Serializes one command argument without embedded log line terminators.
 * @param value UTF-8 argument.
 * @return JSON string literal.
 */
std::string quoteJson(std::string_view value) {
    std::string result = "\"";
    constexpr char HEX[] = "0123456789abcdef";

    for (const unsigned char byte : value) {
        if (byte == '"' || byte == '\\') {
            result += '\\';
            result += static_cast<char>(byte);
        } else if (byte < 0x20) { // JSON control characters must not split metadata records.
            result += "\\u00";
            result += HEX[byte >> 4];
            result += HEX[byte & 15];
        } else {
            result += static_cast<char>(byte);
        }
    }

    return result + '"';
}

/** Incremental maximal-subpart UTF-8 replacement and universal newline decoding. */
class TextStream {
    std::string pending_;
    std::string line_;
    bool afterCr_ = false;

    /** Adds a decoded code point while preserving per-stream line boundaries.
     * @param value One UTF-8 scalar, or a replacement scalar.
     * @param emit Receives complete text lines.
     */
    void append(std::string_view value, const std::function<void(std::string_view)> &emit) {
        if (value == "\n" && afterCr_) {
            afterCr_ = false;

            return;
        }

        afterCr_ = false;

        if (value == "\r" || value == "\n") {
            emit(line_);
            line_.clear();
            afterCr_ = value == "\r";
        } else {
            line_ += value;
        }
    }

    public:
    /** Decodes a chunk; empty input finalizes only this stream.
     * @param bytes Raw child bytes, empty at EOF.
     * @param emit Receives complete lines and the final nonempty fragment.
     */
    void accept(std::string_view bytes, const std::function<void(std::string_view)> &emit) {
        pending_ += bytes;
        std::size_t offset = 0;

        while (offset < pending_.size()) {
            const auto first = static_cast<unsigned char>(pending_[offset]);
            std::size_t length = 1;

            if (first >= 0xc2 && first <= 0xdf) {
                length = 2;
            } else if (first >= 0xe0 && first <= 0xef) {
                length = 3;
            } else if (first >= 0xf0 && first <= 0xf4) {
                length = 4;
            } else if (first >= 0x80) {
                append(REPLACEMENT, emit);
                ++offset;
                continue;
            }

            std::size_t valid = 1;

            while (valid < length && offset + valid < pending_.size()) {
                const auto next = static_cast<unsigned char>(pending_[offset + valid]);

                if (next < 0x80 || next > 0xbf ||
                    (valid == 1 && ((first == 0xe0 && next < 0xa0) || (first == 0xed && next > 0x9f) ||
                                    (first == 0xf0 && next < 0x90) || (first == 0xf4 && next > 0x8f)))) {
                    break;
                }

                ++valid;
            }

            if (valid != length) {
                if (!bytes.empty() && offset + valid == pending_.size()) {
                    break;
                }

                append(REPLACEMENT, emit);
                offset += valid;
            } else {
                append(std::string_view(pending_).substr(offset, length), emit);
                offset += length;
            }
        }

        pending_.erase(0, offset);

        if (bytes.empty() && !line_.empty()) {
            emit(line_);
            line_.clear();
        }
    }
};

/** Executes validated CLI input and reports destination failures.
 * @param arguments Arguments excluding argv[0], encoded as UTF-8.
 * @return Child result, 2 for usage, or 1 for launcher failure.
 */
int run(const std::vector<std::string> &arguments) {
    try {
        if (arguments.empty() || arguments == std::vector<std::string>{"--help"}) {
            writeConsole(HELP);

            return 0;
        }

        if (arguments.size() < 4 || arguments[0] != "--log" || arguments[1].empty() || arguments[2] != "--" ||
            arguments[3].empty()) {
            writeDiagnostic("Invalid arguments. ", HELP);

            return 2;
        }

        const auto logPath = std::filesystem::path(std::u8string(arguments[1].begin(), arguments[1].end()));

        if (!logPath.parent_path().empty()) {
            std::filesystem::create_directories(logPath.parent_path());
        }

        LogFile log(logPath);
        const auto emit = [&](std::string_view message) {
            const auto line = timestamp() + " " + std::string(message) + "\n";
            writeConsole(line);
            log.write(line);
        };
        const std::vector<std::string> command(arguments.begin() + 3, arguments.end());
        std::string metadata = "COMMAND [";

        for (std::size_t index = 0; index < command.size(); ++index) {
            metadata += (index ? "," : "") + quoteJson(command[index]);
        }

        const auto cwd = std::filesystem::current_path().u8string();
        metadata += "]; cwd=" + quoteJson(std::string(cwd.begin(), cwd.end())) + "; TZ=UTC";
        emit(metadata);
        std::array<TextStream, 2> streams;
        const int result = runOwned(
            command,
            [&](int stream, std::string_view bytes) {
                streams[stream].accept(bytes, emit);
            },
            [&](int code) {
                emit("EXIT " + std::to_string(code));
                log.close();
            });

        return result;
    } catch (const std::exception &error) {
        writeDiagnostic("UTC LAUNCHER FAILED: ", error.what());

        return 1;
    }
}
} // namespace
} // namespace org::ttldtor::qdcxx::dev

#ifdef _WIN32
/** Unicode console entry point.
 * @param argc Native argument count.
 * @param argv Native argument vector.
 * @return Launcher exit status.
 */
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> arguments;
    try {
        for (int index = 1; index < argc; ++index) {
            arguments.push_back(org::ttldtor::qdcxx::dev::toUtf8(argv[index]));
        }
    } catch (...) {
        return 1;
    }

    return org::ttldtor::qdcxx::dev::run(arguments);
}
#else
/** POSIX entry point.
 * @param argc Argument count.
 * @param argv Argument vector in the caller's UTF-8 locale.
 * @return Launcher exit status.
 */
int main(int argc, char **argv) {
    return org::ttldtor::qdcxx::dev::run(std::vector<std::string>(argv + 1, argv + argc));
}
#endif
