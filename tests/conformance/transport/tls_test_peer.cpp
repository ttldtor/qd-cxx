// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_test_peer.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <csignal>
#    include <fcntl.h>
#    include <spawn.h>
#    include <sys/stat.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char **environ;
#endif

namespace org::ttldtor::qdcxx::test {
namespace {
using Clock = std::chrono::steady_clock;                       ///< Monotonic parent deadlines.
constexpr std::uint64_t CONTROL_MAGIC = 0x5144435854503035ULL; ///< Internal control format version 5.
constexpr std::uint64_t REPORT_MAGIC = 0x5144435852503031ULL;  ///< Internal numeric report version 1.
constexpr std::size_t MAX_SCRIPT_BYTES = 4U * 1024U * 1024U;   ///< Finite test-only script ceiling per direction.
constexpr std::size_t MAX_REPORT_BYTES = 512;                  ///< Finite numeric report input ceiling.
constexpr std::size_t MAX_PATH_BYTES = 4096;                   ///< Finite UTF-8 path encoding ceiling.
constexpr auto POLL_INTERVAL = std::chrono::milliseconds(10);  ///< Bounded local polling cadence.
constexpr auto TERMINATE_WAIT = std::chrono::seconds(2);       ///< Finite forced-child retirement observation.
std::atomic<std::uint64_t> directorySequence{}; ///< Process-local collision avoidance for owned directories.

/** Encode a path without locale-dependent narrow conversion.
 * @param path Explicit filesystem path.
 * @return Owned UTF-8 bytes, never diagnostic text.
 */
std::string pathBytes(const std::filesystem::path &path) {
    const auto encoded = path.u8string();

    return {reinterpret_cast<const char *>(encoded.data()), encoded.size()};
}

/** Validate a finite millisecond timeout.
 * @param value Explicit duration.
 * @return Whether this test-only timeout is in 1..60000 milliseconds.
 */
bool validTimeout(std::chrono::milliseconds value) noexcept {
    return value.count() > 0 && value.count() <= 60000;
}

/** Validate an explicit absolute path before any child launch.
 * @param path Caller-provided ordinary path.
 * @return Whether its UTF-8 encoding fits and contains no embedded NUL.
 */
bool validPath(const std::filesystem::path &path) {
    const auto bytes = pathBytes(path);

    return path.is_absolute() && !bytes.empty() && bytes.size() <= MAX_PATH_BYTES &&
           bytes.find('\0') == std::string::npos;
}

/** Validate enum, allocation and deadline bounds without reading fixture contents.
 * @param options Explicit child configuration.
 * @return Whether all finite limits and ordinary fixture paths are valid.
 */
bool validOptions(const PeerOptions &options) {
    if (!validPath(options.childExecutable) || !validPath(options.certificateFile) ||
        !validPath(options.privateKeyFile) || !validPath(options.controlRoot)) {
        return false;
    }

    const auto version = static_cast<std::uint32_t>(options.tlsVersion);
    const auto policy = static_cast<std::uint32_t>(options.policy);

    if (version < 10 || version > 13 || policy > 3 || options.tickets > 2 || (options.tickets != 0 && version != 13) ||
        ((policy == 1 || policy == 3) && version != 12) || (version < 12 && options.policy != PeerPolicy::MODERN) ||
        (options.closeMode != PeerCloseMode::CLEAN && options.closeMode != PeerCloseMode::ABRUPT &&
         options.closeMode != PeerCloseMode::HOLD)) {
        return false;
    }

    if (options.initialBytes.size() > MAX_SCRIPT_BYTES || options.replyBytes.size() > MAX_SCRIPT_BYTES ||
        options.finalBytes.size() > MAX_SCRIPT_BYTES ||
        (options.expectedReplyClientBytes && options.expectedReplyClientBytes->size() > MAX_SCRIPT_BYTES) ||
        (options.expectedClientBytes && options.expectedClientBytes->size() > MAX_SCRIPT_BYTES)) {
        return false;
    }

    if (options.successor && (!options.captureUntilClose || options.successor->initialBytes.size() > MAX_SCRIPT_BYTES ||
                              options.successor->replyBytes.size() > MAX_SCRIPT_BYTES ||
                              options.successor->finalBytes.size() > MAX_SCRIPT_BYTES ||
                              (options.successor->expectedReplyClientBytes &&
                               options.successor->expectedReplyClientBytes->size() > MAX_SCRIPT_BYTES) ||
                              (options.successor->expectedClientBytes &&
                               options.successor->expectedClientBytes->size() > MAX_SCRIPT_BYTES) ||
                              options.successor->pauseBeforeHandshake.count() < 0 ||
                              options.successor->pauseBeforeHandshake > options.overallTimeout)) {
        return false;
    }

    const auto captureFits = [](const auto &script) {
        const auto first = script.expectedClientBytes ? script.expectedClientBytes->size() : 0;
        const auto second = script.expectedReplyClientBytes ? script.expectedReplyClientBytes->size() : 0;

        return second <= MAX_SCRIPT_BYTES - first;
    };

    if (options.captureUntilClose &&
        (!captureFits(options) || (options.successor && !captureFits(*options.successor)))) {
        return false;
    }

    if (!validTimeout(options.startupTimeout) || !validTimeout(options.handshakeTimeout) ||
        !validTimeout(options.readTimeout) || !validTimeout(options.writeTimeout) ||
        !validTimeout(options.overallTimeout) || !validTimeout(options.stopGrace)) {
        return false;
    }

    if (options.pauseBeforeHandshake.count() < 0 || options.pauseBeforeHandshake > options.overallTimeout ||
        options.readDelay.count() < 0 || options.readDelay > options.overallTimeout) {
        return false;
    }

    return std::filesystem::is_regular_file(options.childExecutable) &&
           std::filesystem::is_regular_file(options.certificateFile) &&
           std::filesystem::is_regular_file(options.privateKeyFile) &&
           std::filesystem::is_directory(options.controlRoot);
}

/** Write a portable little-endian control word.
 * @param output Owned binary control stream.
 * @param value Unsigned numeric field.
 */
void writeWord(std::ostream &output, std::uint64_t value) {
    std::array<char, 8> bytes{};

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<char>((value >> (index * 8)) & 0xffU);
    }

    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

/** Write an explicit path with an independently bounded byte length.
 * @param output Owned binary control stream.
 * @param path Ordinary certificate/key path.
 */
void writePath(std::ostream &output, const std::filesystem::path &path) {
    const auto bytes = pathBytes(path);
    writeWord(output, bytes.size());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

#ifdef _WIN32
/** Quote one real Windows argument using the CommandLineToArgvW/MSVCRT rules.
 * @param value Exact argument, including possible spaces or trailing backslashes.
 * @return One quoted command-line argument.
 */
std::wstring quoteArgument(const std::wstring &value) {
    std::wstring result(1, L'"');
    std::size_t slashes = 0;

    for (const auto character : value) {
        if (character == L'\\') {
            ++slashes;

            continue;
        }

        result.append(slashes * (character == L'"' ? 2 : 1), L'\\');
        slashes = 0;

        if (character == L'"') {
            result.push_back(L'\\');
        }

        result.push_back(character);
    }

    result.append(slashes * 2, L'\\');
    result.push_back(L'"');

    return result;
}
#endif
} // namespace

/** Sole parent owner; all paths below directory are created by this invocation. */
struct TlsTestPeer::State {
    std::filesystem::path root;            ///< Canonical explicit caller-owned base; never removed.
    std::filesystem::path directory;       ///< Exact successfully created child directory.
    std::chrono::milliseconds stopGrace{}; ///< Cooperative parent wait before exact-child termination.
    PeerSnapshot last;                     ///< Last complete validated payload-free observations.
    bool exitFailed{};     ///< A nonzero/signaled child exit remains visible even if final report I/O failed.
    bool directoryOwned{}; ///< Creation succeeded; cleanup may remove only the named files and directory.
#ifdef _WIN32
    HANDLE process{}; ///< Exact CreateProcess process handle, never opened by PID.
#else
    pid_t process{-1}; ///< Exact posix_spawn child PID; cleared immediately after waitpid reaping.
#endif

    /** Refresh liveness and reap only this process when it has exited.
     * @return Whether the exact child remains live or unreaped.
     */
    bool running() noexcept {
#ifdef _WIN32

        if (process == nullptr) {
            return false;
        }

        const auto result = WaitForSingleObject(process, 0);
        last.childRunning = result != WAIT_OBJECT_0;

        if (result == WAIT_FAILED) {
            last.exitQueryError = GetLastError();
        }

        if (!last.childRunning) {
            DWORD code{};
            last.exitObserved = GetExitCodeProcess(process, &code) != 0;
            last.exitQueryError = last.exitObserved ? 0 : GetLastError();
            last.exitCode = last.exitObserved ? code : 0;
            exitFailed = !last.exitObserved || code != 0;
            last.failed = last.failed || exitFailed;
        }

#else

        if (process <= 0) {
            return false;
        }

        int status{};
        const auto result = waitpid(process, &status, WNOHANG);
        last.exitQueryError = result < 0 ? static_cast<std::uint32_t>(errno) : 0;

        if (result == process || (result < 0 && errno == ECHILD)) {
            last.exitObserved = result == process && (WIFEXITED(status) || WIFSIGNALED(status));
            last.exitCode = 0;

            if (result == process && WIFEXITED(status)) {
                last.exitCode = static_cast<std::uint32_t>(WEXITSTATUS(status));
            } else if (result == process && WIFSIGNALED(status)) {
                last.exitCode = 128U + static_cast<std::uint32_t>(WTERMSIG(status));
            }

            exitFailed = result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
            last.failed = last.failed || exitFailed;
            process = -1;
            last.childRunning = false;
        } else {
            last.childRunning = true;
        }

#endif

        return last.childRunning;
    }

    /** Read one exact numeric report, retaining the previous value during atomic replacement. */
    void refreshReport() noexcept {
        if (!directoryOwned) {
            return;
        }

        try {
            const auto reportPath = directory / "report";
#ifdef _WIN32
            // Allow deletion while reading this exact old file; child replacement also has bounded sharing retries.
            const auto handle =
                CreateFileW(reportPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (handle == INVALID_HANDLE_VALUE) {
                return;
            }

            LARGE_INTEGER size{};

            if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 || size.QuadPart > MAX_REPORT_BYTES) {
                CloseHandle(handle);

                return;
            }

            std::array<char, MAX_REPORT_BYTES> bytes{};
            DWORD read{};
            const bool succeeded = ReadFile(handle, bytes.data(), static_cast<DWORD>(size.QuadPart), &read, nullptr);
            CloseHandle(handle);

            if (!succeeded || read != static_cast<DWORD>(size.QuadPart)) {
                return;
            }

            std::istringstream input(std::string(bytes.data(), read));
#else
            std::error_code sizeError;
            const auto size = std::filesystem::file_size(reportPath, sizeError);

            if (sizeError || size > MAX_REPORT_BYTES) {
                return;
            }

            std::ifstream input(reportPath, std::ios::binary);
#endif
            std::array<std::uint64_t, 26> words{};

            for (std::size_t index = 0; index < 15; ++index) {
                if (!(input >> words[index])) {
                    return;
                }
            }

            constexpr std::uint64_t REPORT_MAGIC_V2 = 0x5144435852503032ULL;
            constexpr std::uint64_t REPORT_MAGIC_V3 = 0x5144435852503033ULL;

            if (words[0] == REPORT_MAGIC_V2 || words[0] == REPORT_MAGIC_V3) {
                const auto count = words[0] == REPORT_MAGIC_V3 ? words.size() : 21;

                for (std::size_t index = 15; index < count; ++index) {
                    if (!(input >> words[index])) {
                        return;
                    }
                }
            }

            input >> std::ws;

            if (!input.eof() ||
                (words[0] != REPORT_MAGIC && words[0] != REPORT_MAGIC_V2 && words[0] != REPORT_MAGIC_V3) ||
                words[1] == 0 || words[1] > 65535 || words[15] > 65535 ||
                words[16] > std::numeric_limits<std::uint32_t>::max() ||
                words[17] > std::numeric_limits<std::uint32_t>::max() ||
                words[18] > std::numeric_limits<std::uint32_t>::max() || words[19] > 65535 || words[20] > 65535 ||
                (words[2] != 0 && (words[2] < 10 || words[2] > 13)) || words[3] > MAX_SCRIPT_BYTES ||
                words[4] > 3 * MAX_SCRIPT_BYTES || words[21] > 2 || words[22] > 2 || words[23] > 2 || words[24] > 3 ||
                words[25] > 3) {
                return;
            }

            for (std::size_t index = 5; index < 15; ++index) {
                if (words[index] > 1) {
                    return;
                }
            }

            last.port = static_cast<std::uint16_t>(words[1]);
            last.tlsVersion = static_cast<std::uint32_t>(words[2]);
            last.bytesRead = words[3];
            last.bytesWritten = words[4];
            last.ready = words[5] != 0;
            last.accepted = words[6] != 0;
            last.handshakeComplete = words[7] != 0;
            last.sniPresent = words[13] != 0;
            last.sniLocalhost = words[14] != 0;
            last.cipherSuite = static_cast<std::uint32_t>(words[15]);
            last.groupId = static_cast<std::uint32_t>(words[16]);
            last.signatureHashNid = static_cast<std::uint32_t>(words[17]);
            last.ticketsSent = static_cast<std::uint32_t>(words[18]);
            last.signatureScheme = static_cast<std::uint32_t>(words[19]);
            last.signatureBytes = static_cast<std::uint32_t>(words[20]);
            last.expectedBytesMatched = words[8] != 0;
            last.closeNotifyComplete = words[9] != 0;
            last.completed = words[10] != 0;
            last.timedOut = words[11] != 0;
            last.childReportedFailure = words[12] != 0;
            last.failed = last.childReportedFailure || exitFailed;
            last.reportReadable = true;
            last.completedSessions = static_cast<std::uint32_t>(words[21]);
            last.successfulHandshakes = static_cast<std::uint32_t>(words[22]);
            last.matchedSessions = static_cast<std::uint32_t>(words[23]);
            last.firstCaptureEnd = static_cast<std::uint32_t>(words[24]);
            last.secondCaptureEnd = static_cast<std::uint32_t>(words[25]);

            for (const auto &entry :
                 {std::pair{"capture-1", &last.firstClientBytes}, std::pair{"capture-2", &last.secondClientBytes}}) {
                std::error_code error;
                const auto path = directory / entry.first;
                const auto bytes = std::filesystem::file_size(path, error);

                if (!error && bytes <= MAX_SCRIPT_BYTES) {
                    std::ifstream capture(path, std::ios::binary);
                    std::vector<std::uint8_t> value(static_cast<std::size_t>(bytes));
                    capture.read(reinterpret_cast<char *>(value.data()), static_cast<std::streamsize>(bytes));

                    if (capture) {
                        *entry.second = std::move(value);
                    }
                }
            }
        } catch (...) {
            // Destruction and polling never expose paths, payloads or exception diagnostics.
        }
    }

    /** Remove only exact owned files, then the empty verified child directory; never recurse. */
    void cleanup() noexcept {
        try {
            if (!directoryOwned || directory.parent_path() != root ||
                directory.filename().string().find("qdcxx-tls-peer-") != 0) {
                return;
            }

            std::error_code error;
            const auto status = std::filesystem::symlink_status(directory, error);

            if (error || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status)) {
                return;
            }

            for (const auto *name : {"control", "report", "report.next", "stop", "capture-1", "capture-2"}) {
                std::filesystem::remove(directory / name, error);
            }

            std::filesystem::remove(directory, error);
            directoryOwned = false;
        } catch (...) {
            // Cleanup is exact-path best effort; it must not throw from destruction.
        }
    }

    /** Write stop marker, finitely await exit, and terminate only this owned process if needed. */
    void retire() noexcept {
        try {
            if (running()) {
                std::ofstream request(directory / "stop", std::ios::binary);
                request.put('1');
                request.close();
                const auto graceEnd = Clock::now() + stopGrace;

                while (running() && Clock::now() < graceEnd) {
                    std::this_thread::sleep_for(POLL_INTERVAL);
                }

                if (running()) {
                    last.parentTerminationRequested = true;
#ifdef _WIN32
                    TerminateProcess(process, 124);
#else
                    kill(process, SIGKILL);
#endif
                    const auto terminationEnd = Clock::now() + TERMINATE_WAIT;

                    while (running() && Clock::now() < terminationEnd) {
                        std::this_thread::sleep_for(POLL_INTERVAL);
                    }
                }
            }
        } catch (...) {
#ifdef _WIN32

            if (process != nullptr && WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
                last.parentTerminationRequested = true;
                TerminateProcess(process, 124);
                WaitForSingleObject(process, 2000);
            }

#else

            if (running()) {
                last.parentTerminationRequested = true;
                kill(process, SIGKILL);
                const auto terminationEnd = Clock::now() + TERMINATE_WAIT;

                while (running() && Clock::now() < terminationEnd) {
                    std::this_thread::sleep_for(POLL_INTERVAL);
                }
            }

#endif
        }

        refreshReport();
        running();
#ifdef _WIN32

        if (process != nullptr) {
            CloseHandle(process);
            process = nullptr;
        }

#endif
        cleanup();
    }

    /** Ensure a failed start retires the same owned resources as ordinary destruction. */
    ~State() {
        retire();
    }
};

TlsTestPeer::TlsTestPeer(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

TlsTestPeer::TlsTestPeer(TlsTestPeer &&other) noexcept = default;
TlsTestPeer &TlsTestPeer::operator=(TlsTestPeer &&other) noexcept = default;
TlsTestPeer::~TlsTestPeer() = default;

std::expected<TlsTestPeer, PeerError> TlsTestPeer::start(PeerOptions options) {
    try {
        if (!validOptions(options)) {
            return std::unexpected(PeerError::INVALID_OPTIONS);
        }

        auto state = std::make_unique<State>();
        state->root = std::filesystem::canonical(options.controlRoot);
        state->stopGrace = options.stopGrace;
#ifdef _WIN32
        const auto parentId = GetCurrentProcessId();
#else
        const auto parentId = getpid();
#endif

        for (int attempt = 0; attempt < 32; ++attempt) {
            const auto sequence = directorySequence.fetch_add(1, std::memory_order_relaxed);
            const auto ticks = Clock::now().time_since_epoch().count();
            state->directory = state->root / ("qdcxx-tls-peer-" + std::to_string(parentId) + "-" +
                                              std::to_string(ticks) + "-" + std::to_string(sequence));
#ifdef _WIN32
            std::error_code error;
            state->directoryOwned = std::filesystem::create_directory(state->directory, error);
#else
            state->directoryOwned = ::mkdir(state->directory.c_str(), 0700) == 0;
#endif

            if (state->directoryOwned) {
                break;
            }
        }

        if (!state->directoryOwned) {
            return std::unexpected(PeerError::CONTROL_IO);
        }

        const auto control = state->directory / "control";
        std::ofstream output(control, std::ios::binary | std::ios::trunc);
        writeWord(output, CONTROL_MAGIC);
        writeWord(output, static_cast<std::uint32_t>(options.tlsVersion));
        writeWord(output, static_cast<std::uint32_t>(options.closeMode));
        writeWord(output, static_cast<std::uint32_t>(options.policy));
        writeWord(output, options.tickets);
        writeWord(output, static_cast<std::uint64_t>(options.handshakeTimeout.count()));
        writeWord(output, static_cast<std::uint64_t>(options.readTimeout.count()));
        writeWord(output, static_cast<std::uint64_t>(options.writeTimeout.count()));
        writeWord(output, static_cast<std::uint64_t>(options.overallTimeout.count()));
        writeWord(output, static_cast<std::uint64_t>(options.pauseBeforeHandshake.count()));
        writeWord(output, static_cast<std::uint64_t>(options.readDelay.count()));
        writeWord(output, options.expectedClientBytes.has_value());
        writeWord(output, options.expectedClientBytes ? options.expectedClientBytes->size() : 0);
        writeWord(output, options.replyBytes.size());
        writeWord(output, options.initialBytes.size());
        writePath(output, options.certificateFile);
        writePath(output, options.privateKeyFile);

        if (options.expectedClientBytes) {
            output.write(reinterpret_cast<const char *>(options.expectedClientBytes->data()),
                         static_cast<std::streamsize>(options.expectedClientBytes->size()));
        }

        output.write(reinterpret_cast<const char *>(options.replyBytes.data()),
                     static_cast<std::streamsize>(options.replyBytes.size()));
        output.write(reinterpret_cast<const char *>(options.initialBytes.data()),
                     static_cast<std::streamsize>(options.initialBytes.size()));
        writeWord(output, options.captureUntilClose);
        writeWord(output, options.expectedReplyClientBytes.has_value());
        writeWord(output, options.expectedReplyClientBytes ? options.expectedReplyClientBytes->size() : 0);
        writeWord(output, options.finalBytes.size());

        if (options.expectedReplyClientBytes) {
            output.write(reinterpret_cast<const char *>(options.expectedReplyClientBytes->data()),
                         static_cast<std::streamsize>(options.expectedReplyClientBytes->size()));
        }

        output.write(reinterpret_cast<const char *>(options.finalBytes.data()),
                     static_cast<std::streamsize>(options.finalBytes.size()));
        writeWord(output, options.successor.has_value());

        if (options.successor) {
            const auto &next = *options.successor;
            writeWord(output, static_cast<std::uint64_t>(next.pauseBeforeHandshake.count()));
            writeWord(output, next.expectedClientBytes.has_value());
            writeWord(output, next.expectedClientBytes ? next.expectedClientBytes->size() : 0);
            writeWord(output, next.replyBytes.size());
            writeWord(output, next.initialBytes.size());

            if (next.expectedClientBytes) {
                output.write(reinterpret_cast<const char *>(next.expectedClientBytes->data()),
                             static_cast<std::streamsize>(next.expectedClientBytes->size()));
            }

            output.write(reinterpret_cast<const char *>(next.replyBytes.data()),
                         static_cast<std::streamsize>(next.replyBytes.size()));
            output.write(reinterpret_cast<const char *>(next.initialBytes.data()),
                         static_cast<std::streamsize>(next.initialBytes.size()));
            writeWord(output, next.expectedReplyClientBytes.has_value());
            writeWord(output, next.expectedReplyClientBytes ? next.expectedReplyClientBytes->size() : 0);
            writeWord(output, next.finalBytes.size());

            if (next.expectedReplyClientBytes) {
                output.write(reinterpret_cast<const char *>(next.expectedReplyClientBytes->data()),
                             static_cast<std::streamsize>(next.expectedReplyClientBytes->size()));
            }

            output.write(reinterpret_cast<const char *>(next.finalBytes.data()),
                         static_cast<std::streamsize>(next.finalBytes.size()));
        }

        output.close();

        if (!output) {
            return std::unexpected(PeerError::CONTROL_IO);
        }

#ifdef _WIN32
        auto command =
            quoteArgument(options.childExecutable.native()) + L" --control " + quoteArgument(control.native());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION information{};

        if (!CreateProcessW(options.childExecutable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &startup, &information)) {
            return std::unexpected(PeerError::PROCESS_START);
        }

        state->process = information.hProcess;
        state->last.processId = information.dwProcessId;
        CloseHandle(information.hThread);
#else
        struct sigaction childDisposition{};

        if (::sigaction(SIGCHLD, nullptr, &childDisposition) != 0 || childDisposition.sa_handler == SIG_IGN ||
            (childDisposition.sa_flags & SA_NOCLDWAIT) != 0) {
            return std::unexpected(PeerError::PROCESS_START);
        }

        auto executable = options.childExecutable.native();
        auto controlArgument = control.native();
        std::array<char *, 4> arguments{executable.data(), const_cast<char *>("--control"), controlArgument.data(),
                                        nullptr};
        posix_spawn_file_actions_t actions{};

        if (posix_spawn_file_actions_init(&actions) != 0) {
            return std::unexpected(PeerError::PROCESS_START);
        }

        const bool redirected =
            posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) == 0 &&
            posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) == 0 &&
            posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) == 0;
#    if defined(__GLIBC__)
        const bool descriptorsClosed = posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1) == 0;
#    else
        const bool descriptorsClosed = true;
#    endif
        const auto spawnResult =
            redirected && descriptorsClosed
                ? posix_spawn(&state->process, executable.c_str(), &actions, nullptr, arguments.data(), environ)
                : -1;
        posix_spawn_file_actions_destroy(&actions);

        if (spawnResult != 0) {
            state->process = -1;

            return std::unexpected(PeerError::PROCESS_START);
        }

        state->last.processId = static_cast<std::uint64_t>(state->process);
#endif
        const auto deadline = Clock::now() + options.startupTimeout;

        while (Clock::now() < deadline) {
            state->refreshReport();

            if (state->last.ready && state->running()) {
                return TlsTestPeer(std::move(state));
            }

            if (!state->running()) {
                return std::unexpected(PeerError::CHILD_FAILED);
            }

            std::this_thread::sleep_for(POLL_INTERVAL);
        }

        return std::unexpected(PeerError::START_TIMEOUT);
    } catch (...) {
        return std::unexpected(PeerError::CONTROL_IO);
    }
}

std::uint16_t TlsTestPeer::port() const noexcept {
    return state_ ? state_->last.port : 0;
}

PeerSnapshot TlsTestPeer::snapshot() noexcept {
    if (!state_) {
        return {};
    }

    state_->refreshReport();
    state_->running();

    return state_->last;
}

PeerSnapshot TlsTestPeer::stop() noexcept {
    if (!state_) {
        return {};
    }

    state_->retire();

    return state_->last;
}

} // namespace org::ttldtor::qdcxx::test
