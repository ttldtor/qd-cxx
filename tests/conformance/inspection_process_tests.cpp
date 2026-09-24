// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "evidence_cells.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <csignal>
#    include <fcntl.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace {
using org::ttldtor::qdcxx::test_support::split;

/** Reject a failed harness condition in optimized builds too.
 * @param condition Required condition.
 * @param message Failure explanation.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Read the complete raw diagnostic after the child has terminated.
 * @param path Captured stderr file.
 * @return Exact bytes, or throw on read failure.
 */
std::string readBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "stderr capture missing");
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    require(!input.bad(), "stderr capture read failed");

    return bytes;
}

/** Parse an unsigned diagnostic cell without accepting noncanonical syntax.
 * @param cell Decimal uint64 cell.
 * @return Parsed value, or throw on bad grammar or overflow.
 */
std::uint64_t number(const std::string &cell) {
    require(std::regex_match(cell, std::regex("0|[1-9][0-9]*")), "diagnostic number grammar");
    std::size_t consumed{};
    const auto result = std::stoull(cell, &consumed);
    require(consumed == cell.size(), "diagnostic number tail");

    return result;
}

/** Check the actual closed-pipe failure's full diagnostic contract.
 * @param bytes Raw stderr.
 */
void validateFailure(std::string bytes) {
    require(!bytes.empty() && bytes.back() == '\n', "diagnostic LF");
    bytes.pop_back();
    require(bytes.find_first_of("\r\n") == bytes.npos, "diagnostic must be one LF row");
    const auto cells = split(bytes);
    require(cells.size() == 11 && cells[0] == "incomplete" && cells[1] == "3", "diagnostic arity/exit");
    require(cells[2] == "write" || cells[2] == "flush", "closed pipe stage");

    for (std::size_t i = 3; i < cells.size(); ++i) {
        if (cells[i] != "-") {
            static_cast<void>(number(cells[i]));
        }
    }

    for (std::size_t i = 7; i <= 8; ++i) {
        if (cells[6] != "-" && cells[i] != "-") {
            const auto base = number(cells[6]);
            const auto relative = number(cells[i]);
            require(relative <= std::numeric_limits<std::uint64_t>::max() - base, "diagnostic overflow");
            require(base + relative == number(cells[i + 2]), "diagnostic absolute offset");
        }
    }
}

#ifdef _WIN32
/** Owned Win32 handle closed on every error path. */
class Handle {
    HANDLE handle_;

    public:
    /** Adopt an OS handle.
     * @param handle Handle, including invalid sentinels.
     */
    explicit Handle(HANDLE handle) : handle_(handle) {
    }

    ~Handle() {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;

    /** Borrow the OS value.
     * @return Handle owned by this wrapper.
     */
    HANDLE get() const {
        return handle_;
    }
};

/** Encode one Windows CRT argument, including empty and terminal backslashes.
 * @param argument Exact wide argument.
 * @return Quoted command-line token.
 */
std::wstring quote(const std::wstring &argument) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const auto unit : argument) {
        if (unit == L'\\') {
            ++slashes;
        } else {
            result.append(unit == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
            result.push_back(unit);
            slashes = 0;
        }
    }

    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');

    return result;
}
#endif

/** Launch with a stdout pipe whose reader is already closed; stderr is a raw file.
 * The deadline covers the entire child lifetime. No pipe-drain read can block it.
 * @param arguments Executable followed by its exact arguments.
 * @param errorPath Raw stderr destination in the test's private run directory.
 * @param timeout Finite child lifetime.
 * @return Child exit, or -1 after bounded timeout cleanup; launch failures throw.
 */
int closedPipe(const std::vector<std::filesystem::path> &arguments, const std::filesystem::path &errorPath,
               std::chrono::milliseconds timeout) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE reader = nullptr;
    HANDLE writer = nullptr;
    require(CreatePipe(&reader, &writer, &security, 0) != 0, "CreatePipe failed");
    Handle output(writer);
    CloseHandle(reader);
    Handle error(CreateFileW(errorPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr));
    require(error.get() != INVALID_HANDLE_VALUE, "stderr file open failed");
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr));
    require(input.get() != INVALID_HANDLE_VALUE, "stdin open failed");
    Handle job(CreateJobObjectW(nullptr, nullptr));
    require(job.get() != nullptr, "job creation failed");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    require(SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != 0,
            "job limits failed");
    std::wstring command;

    for (const auto &argument : arguments) {
        if (!command.empty()) {
            command += L' ';
        }

        command += quote(argument.native());
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.get();
    startup.hStdOutput = output.get();
    startup.hStdError = error.get();
    PROCESS_INFORMATION info{};
    require(CreateProcessW(arguments.front().c_str(), command.data(), nullptr, nullptr, TRUE,
                           CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info) != 0,
            "launch failed");
    Handle process(info.hProcess);
    Handle thread(info.hThread);

    if (!AssignProcessToJobObject(job.get(), process.get())) {
        TerminateProcess(process.get(), 125);
        WaitForSingleObject(process.get(), 1000);
        throw std::runtime_error("job assignment failed");
    }

    require(ResumeThread(thread.get()) != static_cast<DWORD>(-1), "resume failed");
    const auto wait = WaitForSingleObject(process.get(), static_cast<DWORD>(timeout.count()));

    if (wait == WAIT_TIMEOUT) {
        require(TerminateJobObject(job.get(), 124) != 0, "timeout termination failed");
        require(WaitForSingleObject(process.get(), 1000) == WAIT_OBJECT_0, "timeout cleanup failed");

        return -1;
    }

    require(wait == WAIT_OBJECT_0, "process wait failed");
    DWORD code{};
    require(GetExitCodeProcess(process.get(), &code) != 0, "exit status unavailable");

    return static_cast<int>(code);
#else
    require(std::filesystem::is_regular_file(arguments.front()), "launch executable missing");
    std::vector<std::string> owned;

    for (const auto &argument : arguments) {
        owned.push_back(argument.native());
    }

    std::vector<char *> argv;

    for (auto &argument : owned) {
        argv.push_back(argument.data());
    }

    argv.push_back(nullptr);
    int descriptors[2];
    require(pipe(descriptors) == 0, "pipe failed");
    close(descriptors[0]);
    const int error = open(errorPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (error < 0) {
        close(descriptors[1]);
        throw std::runtime_error("stderr open failed");
    }

    const auto child = fork();

    if (child == 0) {
        if (setpgid(0, 0) != 0 || dup2(descriptors[1], STDOUT_FILENO) < 0 || dup2(error, STDERR_FILENO) < 0) {
            _exit(126);
        }

        close(descriptors[1]);
        close(error);
        execv(argv[0], argv.data());
        _exit(127);
    }

    close(descriptors[1]);
    close(error);
    require(child > 0, "fork failed");
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    int status{};

    for (;;) {
        const auto waited = waitpid(child, &status, WNOHANG);

        if (waited == child) {
            require(WIFEXITED(status), "child exited by signal");
            require(WEXITSTATUS(status) != 126 && WEXITSTATUS(status) != 127, "launch failed");

            return WEXITSTATUS(status);
        }

        if (waited < 0 && errno != EINTR) {
            kill(-child, SIGKILL);
            kill(child, SIGKILL);
            throw std::runtime_error("waitpid failed");
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            kill(-child, SIGKILL);
            kill(child, SIGKILL);

            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
            }

            return -1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

#endif
}

/** Execute real preclosed-output tests and harness negative controls.
 * @param arguments Executable path, inspector, fixture and private run directory.
 * @return Zero for complete success, one for harness/contract failure.
 */
int run(const std::vector<std::filesystem::path> &arguments) {
    if (arguments.size() == 2 && arguments[1] == "--hang") {
        // Larger than usual pipe capacity, then keep stderr open beyond the deadline.
        std::cerr << std::string(256 * 1024, 'x') << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(60));

        return 0;
    }

    try {
        if (arguments.size() >= 2 && arguments[1] == "--argument-probe") {
            require(arguments.size() == 6, "argument probe count");
            require(arguments[2].empty(), "empty argument changed");
            require(arguments[3].u8string() == u8"space \u042e\u043d\u0438\u043a\u043e\u0434;literal",
                    "Unicode/space/semicolon argument changed");
            require(arguments[4].u8string() == u8"\"quoted\"", "quoted argument changed");
            require(arguments[5].u8string() == u8"trailing\\", "terminal backslash changed");
            std::cout << "native arguments preserved";

            return std::cout ? 0 : 1;
        }

#ifdef _WIN32

        if (arguments.size() == 2 && arguments[1] == "--code-page") {
            std::cout << GetACP();

            return std::cout ? 0 : 1;
        }

#endif

        require(arguments.size() == 4, "expected inspector, fixture, run directory");
        const auto &program = arguments[1];
        const auto &fixture = arguments[2];
        const auto &directory = arguments[3];
        std::filesystem::create_directories(directory);
        const std::vector<std::vector<std::filesystem::path>> cases{
            {program},
            {program, "--help"},
            {program, "--profile", "oracle-sre-v1", "--input", fixture},
            {program, "--profile", "oracle-sre-v1", "--input", fixture, "--output-format", "human"}};
        std::size_t index = 0;

        for (const auto &command : cases) {
            const auto error = directory / ("closed-" + std::to_string(index++) + ".err");
            require(closedPipe(command, error, std::chrono::seconds(30)) == 3, "closed stdout must exit 3");
            validateFailure(readBytes(error));
        }

        bool missingRejected = false;
        try {
            static_cast<void>(
                closedPipe({directory / "nonexistent-executable"}, directory / "missing.err", std::chrono::seconds(1)));
        } catch (const std::exception &) {
            missingRejected = true;
        }
        require(missingRejected, "missing executable accepted");
        const auto start = std::chrono::steady_clock::now();
        require(closedPipe({arguments[0], "--hang"}, directory / "timeout.err", std::chrono::milliseconds(250)) == -1,
                "timeout not detected");
        require(std::chrono::steady_clock::now() - start < std::chrono::seconds(5), "timeout cleanup unbounded");
        require(std::filesystem::file_size(directory / "timeout.err") >= 256 * 1024, "stderr flood not exercised");
        std::cout << "preclosed stdout: noargs, help, TSV, human; missing launch and deadline rejected\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "process harness: " << error.what() << '\n';

        return 1;
    }
}
} // namespace

/** Preserve native command-line paths and dispatch the process contract checks.
 * @param argc Count including this executable.
 * @param argv Native command-line arguments borrowed for this call.
 * @return Zero on success or one on a harness/contract failure.
 */
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    std::vector<std::filesystem::path> arguments;

    for (int i = 0; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }

    return run(arguments);
}
