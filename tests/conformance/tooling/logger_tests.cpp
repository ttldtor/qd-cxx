// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "process.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <csignal>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace {
using org::ttldtor::qdcxx::dev::runOwned;

/** Fails a named independent expectation.
 * @param condition Required condition.
 * @param message Diagnostic naming the violated contract.
 */
void require(bool condition, const std::string &message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Builds a native filesystem path without consulting the Windows ANSI code page.
 * @param value Valid UTF-8 path.
 * @return Native path with identical Unicode contents.
 */
std::filesystem::path utf8Path(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}

/** Reads a file without newline or encoding translation.
 * @param path Existing file.
 * @return Exact bytes.
 */
std::string readFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "cannot read test file");

    return {std::istreambuf_iterator<char>(file), {}};
}

/** Makes exact argv expectations independent of launcher quoting.
 * @param value Expected raw UTF-8 bytes.
 * @return Lowercase hexadecimal.
 */
std::string hex(std::string_view value) {
    constexpr char DIGITS[] = "0123456789abcdef";
    std::string result;

    for (const unsigned char byte : value) {
        result += DIGITS[byte >> 4];
        result += DIGITS[byte & 15];
    }

    return result;
}

/** Captures raw stdout/stderr and exit status. */
struct Result {
    /// Normal process exit status.
    int code;
    /// Exact stdout bytes.
    std::string output;
    /// Exact stderr bytes.
    std::string error;
};

/** Runs one bounded-by-outer-CTest command.
 * @param command Executable plus exact arguments.
 * @return Captured command result.
 */
Result invoke(const std::vector<std::string> &command) {
    Result result{};
    result.code = runOwned(command, [&](int stream, std::string_view bytes) {
        (stream == 0 ? result.output : result.error) += bytes;
    });

    return result;
}

/** Validates exact console/log equality, then removes only timestamp prefixes.
 * @param result Captured launcher output.
 * @param path Log path.
 * @return Ordered messages including metadata.
 */
std::vector<std::string> messages(const Result &result, const std::filesystem::path &path) {
    const auto text = readFile(path);
    require(text == result.output, "console/log bytes differ");
    require(text.find('\r') == std::string::npos, "CR in UTF-8 LF log");
    require(!text.starts_with("\xef\xbb\xbf"), "file BOM");
    require(!text.empty() && text.back() == '\n', "missing final LF");
    const std::regex stamp("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z ");
    std::vector<std::string> lines;
    std::size_t offset = 0;

    while (offset < text.size()) {
        const auto end = text.find('\n', offset);
        require(end != std::string::npos && end - offset >= 25, "short timestamped line");
        require(std::regex_match(text.substr(offset, 25), stamp), "UTC millisecond timestamp");
        lines.push_back(text.substr(offset + 25, end - offset - 25));
        offset = end + 1;
    }

    require(lines.front().starts_with("COMMAND ["), "COMMAND metadata");
    require(lines.back() == "EXIT " + std::to_string(result.code), "EXIT metadata");

    return lines;
}

/** Queries only a PID read from this test's private child-owned file.
 * @param pid Child process identifier.
 * @return Whether that process is still live.
 */
bool live(unsigned long pid) {
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);

    if (!process) {
        return false;
    }

    const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);

    return running;
#else

    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

/** Distinguishes an exited root from an unreaped Linux zombie during a blocked write.
 * @param pid This test's recorded root PID.
 * @return Whether the root has exited, even if its logger has not reaped it yet.
 */
bool rootHasExited(unsigned long pid) {
    if (!live(pid)) {
        return true;
    }

#ifdef __linux__
    std::ifstream status("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    std::getline(status, line);
    const auto nameEnd = line.rfind(')');

    return nameEnd != std::string::npos && line.size() > nameEnd + 2 && line[nameEnd + 2] == 'Z';
#else

    return false;
#endif
}

#ifdef _WIN32
/** Converts exact test paths to native Windows text.
 * @param value Valid UTF-8 bytes.
 * @return UTF-16 string.
 */
std::wstring wide(const std::string &value) {
    const int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    require(size > 0 || value.empty(), "test UTF-8 conversion");
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                        size);

    return result;
}

/** Quotes test paths which are checked to have no quote or trailing slash.
 * @param argument Test executable/path or mode.
 * @return Native quoted argument.
 */
std::wstring simpleQuote(const std::string &argument) {
    require(argument.find('"') == std::string::npos && !argument.ends_with('\\'), "fault test path precondition");

    return L"\"" + wide(argument) + L"\"";
}
#endif

/** Exercises a real closed stdout pipe after a running child has published its PID.
 * The standalone test process is itself contained by the outer native harness.
 * @param command Logger executable plus arguments; paths contain no quote/trailing backslash.
 * @param pidPath Child PID synchronization file.
 * @param interrupt Whether to send a graceful cancellation instead of closing stdout.
 * @return Launcher status after bounded cleanup.
 */
int destinationFailure(const std::vector<std::string> &command, const std::filesystem::path &pidPath, bool interrupt,
                       bool stalled = false, bool finalOnly = false) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE reader = nullptr;
    HANDLE writer = nullptr;
    require(CreatePipe(&reader, &writer, &attributes, 4096) != 0, "fault pipe creation");
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    std::wstring native;

    for (const auto &argument : command) {
        native += simpleQuote(argument) + L' ';
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writer;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    require(CreateProcessW(nullptr, native.data(), nullptr, nullptr, TRUE, CREATE_NEW_PROCESS_GROUP, nullptr, nullptr,
                           &startup, &process) != 0,
            "fault logger start");

    if (!finalOnly) {
        CloseHandle(writer);
    }

    CloseHandle(process.hThread);
#else
    int pipeEnds[2];
    require(pipe(pipeEnds) == 0, "fault pipe creation");
#    ifdef __linux__

    if (stalled) {
        require(fcntl(pipeEnds[1], F_SETPIPE_SZ, 4096) >= 0, "pipe capacity setup");
    }

#    endif
    const auto child = fork();
    require(child >= 0, "fault logger fork");

    if (!child) {
        dup2(pipeEnds[1], STDOUT_FILENO);
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        std::vector<char *> native;

        for (const auto &argument : command) {
            native.push_back(const_cast<char *>(argument.c_str()));
        }

        native.push_back(nullptr);
        execvp(native[0], native.data());
        _exit(127);
    }

    if (!finalOnly) {
        close(pipeEnds[1]);
    }

#endif
    const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!std::filesystem::exists(pidPath) && std::chrono::steady_clock::now() < readyDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const bool ready = std::filesystem::exists(pidPath);

    if (ready && finalOnly) {
        // Consume precisely COMMAND, then fill the pipe before allowing the silent root to exit.
        char byte = 0;

        do {
#ifdef _WIN32
            DWORD count = 0;
            require(ReadFile(reader, &byte, 1, &count, nullptr) && count == 1, "COMMAND synchronization");
#else
            require(read(pipeEnds[0], &byte, 1) == 1, "COMMAND synchronization");
#endif
        } while (byte != '\n');

        const std::string fill(4096, 'x');
#ifdef _WIN32
        DWORD count = 0;
        require(WriteFile(writer, fill.data(), static_cast<DWORD>(fill.size()), &count, nullptr) &&
                    count == fill.size(),
                "pipe fill");
        CloseHandle(writer);
#else
        require(write(pipeEnds[1], fill.data(), fill.size()) == static_cast<ssize_t>(fill.size()), "pipe fill");
        close(pipeEnds[1]);
#endif
        auto gatePath = pidPath;
        gatePath += ".ready";
        std::ofstream gate(gatePath);
        gate << "ready";
        gate.close();
    }

    const auto start = std::chrono::steady_clock::now();
    bool rootExitObserved = !stalled;

    if (ready && stalled) {
        const auto rootPid = std::stoul(readFile(pidPath));

        while (!rootHasExited(rootPid) && std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        rootExitObserved = rootHasExited(rootPid);
    }

#ifdef _WIN32

    if (interrupt) {
        require(GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process.dwProcessId) != 0, "console cancellation signal");
    }

    if (!interrupt && !stalled) {
        CloseHandle(reader);
    }

    const bool completed = WaitForSingleObject(process.hProcess, 5500) == WAIT_OBJECT_0;

    if (!completed) {
        TerminateProcess(process.hProcess, 99);
    }

    DWORD status = 99;
    GetExitCodeProcess(process.hProcess, &status);
    CloseHandle(process.hProcess);

    if (interrupt || stalled) {
        CloseHandle(reader);
    }

#else

    if (interrupt) {
        kill(child, SIGTERM);
    }

    if (!interrupt && !stalled) {
        close(pipeEnds[0]);
    }

    int rawStatus = 0;
    bool completed = false;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5500)) {
        if (waitpid(child, &rawStatus, WNOHANG) == child) {
            completed = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!completed) {
        kill(child, SIGKILL);
        waitpid(child, &rawStatus, 0);
    }

    if (interrupt || stalled) {
        close(pipeEnds[0]);
    }

    const int status = WIFEXITED(rawStatus) ? WEXITSTATUS(rawStatus) : 128 + WTERMSIG(rawStatus);
#endif
    require(ready, "running child synchronization timeout");
    require(rootExitObserved, "stalled test root did not actually exit before destination cleanup");
    require(completed, "cleanup exceeded five-second grace plus scheduling tolerance");
    const auto pid = std::stoul(readFile(pidPath));
    require(!live(pid), "owned child survived destination failure/cancellation");

    return static_cast<int>(status);
}

#ifdef __linux__
/** Verifies the log destination also obeys cleanup deadlines when a FIFO reader stalls.
 * @param logger Native launcher executable.
 * @param probe Native burst writer executable.
 * @param work Task-private test directory.
 */
void stalledLog(const std::string &logger, const std::string &probe, const std::filesystem::path &work) {
    // Windows-mounted WSL worktrees cannot host FIFOs; use a unique native temporary directory.
    const auto temporary = std::filesystem::temp_directory_path() / ("qdcxx-log-fifo-" + std::to_string(getpid()));
    require(std::filesystem::create_directory(temporary), "private FIFO directory creation");
    const auto fifoPath = temporary / "stalled-log.fifo";
    const auto pidPath = temporary / "stalled-log.pid";
    (void)work;
    std::filesystem::remove(fifoPath);
    std::filesystem::remove(pidPath);
    require(mkfifo(fifoPath.c_str(), 0600) == 0, "log FIFO creation");
    const int reader = open(fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
    require(reader >= 0 && fcntl(reader, F_SETPIPE_SZ, 4096) >= 0, "log FIFO capacity");
    const auto child = fork();
    require(child >= 0, "log FIFO logger fork");

    if (!child) {
        close(reader);
        const int discarded = open("/dev/null", O_WRONLY);
        dup2(discarded, STDOUT_FILENO);
        close(discarded);
        execl(logger.c_str(), logger.c_str(), "--log", fifoPath.c_str(), "--", probe.c_str(), "burst", pidPath.c_str(),
              nullptr);
        _exit(127);
    }

    const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    char byte = 0;

    while (byte != '\n' && std::chrono::steady_clock::now() < readyDeadline) {
        if (read(reader, &byte, 1) != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    while (!std::filesystem::exists(pidPath) && std::chrono::steady_clock::now() < readyDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto start = std::chrono::steady_clock::now();
    bool rootExited = false;

    if (std::filesystem::exists(pidPath)) {
        const auto pid = std::stoul(readFile(pidPath));

        while (!rootHasExited(pid) && std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        rootExited = rootHasExited(pid);
    }

    int status = 0;
    bool completed = false;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5500)) {
        if (waitpid(child, &status, WNOHANG) == child) {
            completed = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!completed) {
        kill(child, SIGKILL);
        waitpid(child, &status, 0);
    }

    close(reader);
    std::filesystem::remove(fifoPath);
    std::filesystem::remove(pidPath);
    std::filesystem::remove(temporary);
    require(rootExited, "FIFO regression root did not exit");
    require(completed, "stalled log FIFO exceeded cleanup grace");
    require(WIFEXITED(status) && WEXITSTATUS(status) == 1, "stalled log FIFO exit");
    std::cout << "PASS stalled log FIFO after observed root exit within same grace\n";
}
#endif

/** Verifies error reporting cannot block exit after child cleanup.
 * @param logger Native logger executable.
 * @param probe Native child executable.
 * @param work Private test directory.
 * @param invalidUsage Whether to exercise usage reporting before a child exists.
 */
void stalledDiagnostic(const std::string &logger, const std::string &probe, const std::filesystem::path &work,
                       bool invalidUsage = false) {
    const auto pidPath = work / "diagnostic.pid";
    auto gatePath = pidPath;
    gatePath += ".ready";
    const auto logPath = work / "diagnostic.log";
    std::filesystem::remove(pidPath);
    std::filesystem::remove(gatePath);
    const auto pidUtf8 = pidPath.u8string();
    const auto logUtf8 = logPath.u8string();
    const std::vector<std::string> command =
        invalidUsage ? std::vector<std::string>{logger, "--invalid"}
                     : std::vector<std::string>{logger, "--log",      std::string(logUtf8.begin(), logUtf8.end()), "--",
                                                probe,  "gated-exit", std::string(pidUtf8.begin(), pidUtf8.end())};
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    HANDLE errorRead = nullptr;
    HANDLE errorWrite = nullptr;
    require(CreatePipe(&outputRead, &outputWrite, &security, 4096) &&
                CreatePipe(&errorRead, &errorWrite, &security, 4096),
            "diagnostic pipe creation");
    SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errorRead, HANDLE_FLAG_INHERIT, 0);
    const std::string fill(4096, 'x');
    DWORD count = 0;
    require(WriteFile(errorWrite, fill.data(), static_cast<DWORD>(fill.size()), &count, nullptr) &&
                count == fill.size(),
            "diagnostic pipe prefill");
    std::wstring native;

    for (const auto &argument : command) {
        native += simpleQuote(argument) + L' ';
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = outputWrite;
    startup.hStdError = errorWrite;
    PROCESS_INFORMATION process{};
    require(CreateProcessW(nullptr, native.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process),
            "diagnostic logger launch");
    CloseHandle(process.hThread);
    CloseHandle(outputWrite);
    CloseHandle(errorWrite);

    if (!invalidUsage) {
        char byte = 0;

        do {
            require(ReadFile(outputRead, &byte, 1, &count, nullptr) && count == 1,
                    "diagnostic COMMAND synchronization");
        } while (byte != '\n');
    }

    CloseHandle(outputRead);
#else
    int output[2];
    int errors[2];
    require(pipe(output) == 0 && pipe(errors) == 0, "diagnostic pipe creation");
    require(fcntl(errors[1], F_SETFL, O_NONBLOCK) == 0, "diagnostic prefill setup");
    const std::string fill(4096, 'x');

    while (write(errors[1], fill.data(), fill.size()) > 0) {
    }

    require(fcntl(errors[1], F_SETFL, 0) == 0, "diagnostic blocking restore");
    const auto child = fork();
    require(child >= 0, "diagnostic logger fork");

    if (!child) {
        dup2(output[1], STDOUT_FILENO);
        dup2(errors[1], STDERR_FILENO);

        for (const int descriptor : {output[0], output[1], errors[0], errors[1]}) {
            close(descriptor);
        }

        std::vector<char *> native;

        for (const auto &argument : command) {
            native.push_back(const_cast<char *>(argument.c_str()));
        }

        native.push_back(nullptr);
        execvp(native[0], native.data());
        _exit(127);
    }

    close(output[1]);
    close(errors[1]);

    if (!invalidUsage) {
        char byte = 0;

        do {
            require(read(output[0], &byte, 1) == 1, "diagnostic COMMAND synchronization");
        } while (byte != '\n');
    }

    close(output[0]);
#endif
    const auto start = std::chrono::steady_clock::now();

    while (!invalidUsage && !std::filesystem::exists(pidPath) &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const bool ready = invalidUsage || std::filesystem::exists(pidPath);
    std::ofstream gate(gatePath);
    gate << "ready";
    gate.close();
#ifdef _WIN32
    const bool completed = WaitForSingleObject(process.hProcess, 5500) == WAIT_OBJECT_0;

    if (!completed) {
        TerminateProcess(process.hProcess, 99);
        WaitForSingleObject(process.hProcess, 1000);
    }

    DWORD result = 99;
    GetExitCodeProcess(process.hProcess, &result);
    CloseHandle(process.hProcess);
    CloseHandle(errorRead);
#else
    int status = 0;
    bool completed = false;

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5500)) {
        if (waitpid(child, &status, WNOHANG) == child) {
            completed = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!completed) {
        kill(child, SIGKILL);
        waitpid(child, &status, 0);
    }

    close(errors[0]);
    const int result = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
    require(ready, "diagnostic root did not start");
    require(completed, "stalled diagnostic exceeded cleanup grace");
    require(result == (invalidUsage ? 2 : 1), "stalled diagnostic exit");

    if (!invalidUsage) {
        require(!live(std::stoul(readFile(pidPath))), "diagnostic failure left owned root alive");
    }

    std::cout << "PASS stalled stderr diagnostic; " << (invalidUsage ? "usage" : "post-cleanup")
              << " failure remains bounded\n";
}

/** Executes the native logger contract with independent expected values.
 * @param args Executable, logger, probe and task-private work directory.
 * @return Zero only when all assertions pass.
 */
int run(const std::vector<std::string> &args) {
    require(args.size() == 4 || args.size() == 5, "expected LOGGER PROBE WORK [FAULT_LOGGER]");
    const auto &logger = args[1];
    const auto &probe = args[2];
    const auto work = utf8Path(args[3]);
    std::filesystem::create_directories(work);
    const auto noArgs = invoke({logger});
    const auto help = invoke({logger, "--help"});
    require(noArgs.code == 0 && help.code == 0 && noArgs.output == help.output && noArgs.error.empty() &&
                help.error.empty(),
            "no-argument/help equality");

    for (const auto &invalid : std::vector<std::vector<std::string>>{{logger, "--log"},
                                                                     {logger, "--help", "extra"},
                                                                     {logger, "--log", "unused", "--"},
                                                                     {logger, "--log", "unused", probe}}) {
        require(invoke(invalid).code == 2, "invalid CLI accepted");
    }

    std::cout << "PASS help and invalid arguments\n";
    const auto path = [&](const std::string &name) {
        const auto value = (work / utf8Path(name)).u8string();

        return std::string(value.begin(), value.end());
    };
    const auto execute = [&](const std::string &name, const std::vector<std::string> &child) {
        std::vector<std::string> command{logger, "--log", path(name + ".log"), "--", probe};
        command.insert(command.end(), child.begin(), child.end());

        return invoke(command);
    };
    const std::vector<std::string> awkward{"",   "space value", "\xe9\x9b\xaa", "a;b",
                                           "\"", "C:\\end\\",   "a\"b\\",       "-option"};
    std::vector<std::string> childArgs{"args"};
    childArgs.insert(childArgs.end(), awkward.begin(), awkward.end());
    const auto beforeCwd = std::filesystem::current_path();
    const auto *tzValue = std::getenv("TZ");
    const std::string beforeTz = tzValue ? tzValue : "<unset>";
    std::vector<std::string> environmentCommand{
        probe, "spawn-environment", logger, "--log", path("args \xe9\x9b\xaa;value.log"), "--", probe};
    environmentCommand.insert(environmentCommand.end(), childArgs.begin(), childArgs.end());
    const auto argumentResult = invoke(environmentCommand);
    require(argumentResult.code == 0 && argumentResult.error.empty(), "argument invocation");
    const auto argumentLines = messages(argumentResult, utf8Path(path("args \xe9\x9b\xaa;value.log")));

    for (std::size_t index = 0; index < awkward.size(); ++index) {
        require(argumentLines[index + 1] == "ARG " + std::to_string(index) + " " + hex(awkward[index]),
                "argv round trip");
    }

    require(argumentLines[awkward.size() + 1] == "TZ=UTC", "child-only TZ override");
    require(argumentLines[awkward.size() + 2] == "SENTINEL=preserved", "inherited environment lost");
    const auto cwdBytes = beforeCwd.u8string();
    require(argumentLines[awkward.size() + 3] == "CWD=" + hex(std::string(cwdBytes.begin(), cwdBytes.end())),
            "cwd inheritance");
    tzValue = std::getenv("TZ");
    require(beforeTz == (tzValue ? tzValue : "<unset>") && beforeCwd == std::filesystem::current_path(),
            "parent mutation");
#ifdef _WIN32
    require(GetACP() == 1252 && argumentLines[awkward.size() + 4] == "ACP=1252", "actual Windows ACP 1252 required");
    std::cout << "PASS actual ACP=1252 Unicode argv and Unicode log path\n";
#endif
    std::cout << "PASS exact argv, child TZ, parent environment and cwd\n";

    for (const std::string mode : {"text", "split-text"}) {
        const auto result = execute(mode, {mode});
        require(result.code == 0 && result.error.empty(), "text invocation");
        auto lines = messages(result, utf8Path(path(mode + ".log")));
        lines.erase(lines.begin());
        lines.pop_back();
        const std::string stderrLine = "\xef\xbf\xbd\xef\xbf\xbdZ";
        require(std::count(lines.begin(), lines.end(), stderrLine) == 1, "UTF-16 BOM replacement");
        std::erase(lines, stderrLine);
        require(lines == std::vector<std::string>{"\xef\xbb\xbf"
                                                  "A",
                                                  "B", "C", "", "D", "", "E\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd", "F"},
                "UTF-8 replacement, BOM, universal newline or final fragment");
    }

    const auto utf8 = execute("utf8", {"utf8"});
    auto utf8Lines = messages(utf8, utf8Path(path("utf8.log")));
    require(std::count(utf8Lines.begin(), utf8Lines.end(), "stderr-final") == 1, "separate stderr fragment");
    std::erase(utf8Lines, "stderr-final");
    require(utf8Lines.size() == 5 && utf8Lines[1] == "\xe9\x9b\xaa" && utf8Lines[2] == "\xf0\x9f\x98\x80" &&
                utf8Lines[3] == "\xef\xbf\xbd",
            "incremental scalar/truncated maximal subpart");
    std::cout << "PASS split UTF-8/CRLF, invalid/truncated UTF-8, BOMs, final fragments\n";
    const auto large = execute("large", {"large"});
    require(large.code == 0 && large.error.empty(), "large invocation");
    const auto largeLines = messages(large, utf8Path(path("large.log")));

    for (const char prefix : {'O', 'E'}) {
        int index = 0;

        for (const auto &line : largeLines) {
            if (!line.empty() && line[0] == prefix && !line.starts_with("EXIT")) {
                const auto expected = std::string(1, prefix) +
                                      (index == 5000 ? "-final" : std::to_string(index) + ":" + std::string(256, 'x'));
                require(line == expected, "large per-stream ordering/content");
                ++index;
            }
        }

        require(index == 5001, "large stream line count");
    }

    std::cout << "PASS concurrent 5000+final lines per stream\n";

    for (const int code : {0, 37}) {
        const auto name = "exit" + std::to_string(code);
        const auto result = execute(name, {"exit", std::to_string(code)});
        require(result.code == code && result.error.empty(), "child exit propagation");
        messages(result, utf8Path(path(name + ".log")));
    }

    require(invoke({logger, "--log", path("missing.log"), "--", path("absent-executable")}).code == 1,
            "missing executable");
    require(invoke({logger, "--log", args[3], "--", probe, "exit", "0"}).code == 1, "directory log failure");
#ifndef _WIN32
    require(invoke({logger, "--log", "/dev/full", "--", probe, "exit", "0"}).code == 1, "real /dev/full failure");
    const auto signaled = execute("signal", {"signal"});
    require(signaled.code == 143, "POSIX SIGTERM mapping");
    messages(signaled, utf8Path(path("signal.log")));
#endif
    std::cout << "PASS exit 0/37, launch/open/write errors and platform signal mapping\n";
    const auto orphanPid = path("orphan-child.pid");
    const auto rootPid = path("orphan-root.pid");
    std::filesystem::remove(utf8Path(orphanPid));
    std::filesystem::remove(utf8Path(rootPid));
    const auto start = std::chrono::steady_clock::now();
    const auto orphan = execute("orphan", {"orphan", orphanPid, rootPid});
    require(orphan.code == 0 && orphan.error.empty(), "orphan command");
    require(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5500), "orphan cleanup deadline");
    require(!live(std::stoul(readFile(utf8Path(orphanPid)))), "owned orphan still live");
    messages(orphan, utf8Path(path("orphan.log")));
    std::cout << "PASS already-exited root with pipe-holding descendant\n";
#ifndef _WIN32
    const auto groupedPid = path("grouped-child.pid");
    const auto groupedRoot = path("grouped-root.pid");
    std::filesystem::remove(utf8Path(groupedPid));
    std::filesystem::remove(utf8Path(groupedRoot));
    const auto groupedStart = std::chrono::steady_clock::now();
    const auto grouped = execute("grouped", {"group-orphan", groupedPid, groupedRoot});
    const auto groupChild = std::stoul(readFile(utf8Path(groupedPid)));
    const bool groupSurvived = live(groupChild);

    if (groupSurvived) {
        kill(static_cast<pid_t>(groupChild), SIGKILL);
    }

    require(grouped.code == 0 && grouped.error.empty(), "same-session process-group command");
    require(std::chrono::steady_clock::now() - groupedStart < std::chrono::milliseconds(5500),
            "same-session group cleanup deadline");
    require(!groupSurvived, "same-session group child survived cleanup");
    std::cout << "PASS same-session child with separate process group is cleaned\n";
#endif
    stalledDiagnostic(logger, probe, work);
    stalledDiagnostic(logger, probe, work, true);

#ifdef __linux__
    stalledLog(logger, probe, work);
#endif
    const auto stalledPid = utf8Path(path("stalled.pid"));
    std::filesystem::remove(stalledPid);
    require(destinationFailure({logger, "--log", path("stalled.log"), "--", probe, "burst", path("stalled.pid")},
                               stalledPid, false, true) == 1,
            "stalled-open output status");
    std::cout << "PASS stalled-open console after root exit within cleanup grace\n";
    const auto finalPid = utf8Path(path("stalled-final.pid"));
    std::filesystem::remove(finalPid);
    std::filesystem::remove(utf8Path(path("stalled-final.pid.ready")));
    require(destinationFailure(
                {logger, "--log", path("stalled-final.log"), "--", probe, "gated-exit", path("stalled-final.pid")},
                finalPid, false, true, true) == 1,
            "stalled EXIT status");
    std::cout << "PASS stalled EXIT record after root exit within same grace\n";

    for (const bool interrupt : {false, true}) {
        const auto name = interrupt ? "interrupted" : "closed-console";
        const auto pidPath = utf8Path(path(std::string(name) + ".pid"));
        std::filesystem::remove(pidPath);
        require(destinationFailure({logger, "--log", path(std::string(name) + ".log"), "--", probe,
                                    interrupt ? "hold" : "running", path(std::string(name) + ".pid")},
                                   pidPath, interrupt) == 1,
                "destination/cancellation exit");
        std::cout << "PASS " << name << "; owned child stopped within grace\n";
    }

    if (args.size() == 5) {
        const auto failurePid = path("fault-child.pid");
        const auto failureRoot = path("fault-root.pid");
        std::filesystem::remove(utf8Path(failurePid));
        std::filesystem::remove(utf8Path(failureRoot));
        const auto failureStart = std::chrono::steady_clock::now();
        const auto failure =
            invoke({args[4], "--log", path("fault.log"), "--", probe, "orphan", failurePid, failureRoot});
        require(failure.code == 1 && failure.error.find("injected destination flush failure") != std::string::npos,
                "injected post-root destination failure");
        require(std::chrono::steady_clock::now() - failureStart < std::chrono::milliseconds(5600),
                "post-root failure deadline");
        require(!live(std::stoul(readFile(utf8Path(failureRoot)))) && !live(std::stoul(readFile(utf8Path(failurePid)))),
                "owned process survived post-root failure");
        std::cout << "PASS injected destination failure after root exit; descendant stopped\n";
    }

    std::cout << "PASS native logger contract\n";

    return 0;
}
} // namespace

#ifdef _WIN32
/** Native Unicode test entry point.
 * @param argc Argument count.
 * @param argv UTF-16 arguments.
 * @return Contract test status.
 */
int wmain(int argc, wchar_t **argv) {
#else
/** Native UTF-8 test entry point.
 * @param argc Argument count.
 * @param argv UTF-8 arguments.
 * @return Contract test status.
 */
int main(int argc, char **argv) {
#endif
    try {
        std::vector<std::string> arguments;

        for (int index = 0; index < argc; ++index) {
#ifdef _WIN32
            arguments.push_back(org::ttldtor::qdcxx::dev::toUtf8(argv[index]));
#else
            arguments.emplace_back(argv[index]);
#endif
        }

        return run(arguments);
    } catch (const std::exception &error) {
        std::cerr << "LOGGER TEST FAILED: " << error.what() << '\n';

        return 1;
    }
}
