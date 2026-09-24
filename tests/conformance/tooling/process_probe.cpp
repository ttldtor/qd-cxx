// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#else
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace {

/** Encode UTF-8 bytes without locale-sensitive output.
 * @param value Exact bytes to encode.
 * @return Lowercase hexadecimal, including an empty result for an empty argument.
 */
std::string hex(const std::string &value) {
    constexpr char DIGITS[] = "0123456789abcdef";
    std::string result;

    for (const unsigned char byte : value) {
        result += DIGITS[byte >> 4];
        result += DIGITS[byte & 15];
    }

    return result;
}

#ifdef _WIN32
/** Convert native UTF-16 arguments without consulting the active code page.
 * @param value NUL-terminated native argument.
 * @return Exact UTF-8 representation; throws if conversion fails.
 */
std::string utf8(const wchar_t *value) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);

    if (size == 0) {
        throw std::runtime_error("argument conversion failed");
    }

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), size, nullptr, nullptr);
    result.pop_back();

    return result;
}

/** Convert UTF-8 paths to Windows native text.
 * @param value Valid UTF-8 text.
 * @return Native UTF-16 text; throws if conversion fails.
 */
std::wstring wide(const std::string &value) {
    const int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);

    if (size == 0 && !value.empty()) {
        throw std::runtime_error("path conversion failed");
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                        size);

    return result;
}

/** Quote one Windows CRT argument, including trailing slashes and empty strings.
 * @param value Native argument text.
 * @return One quoted command-line field.
 */
std::wstring quote(const std::wstring &value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const wchar_t character : value) {
        if (character == L'\\') {
            ++slashes;
        } else {
            result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            result += character;
        }
    }

    result.append(slashes * 2, L'\\');

    return result + L'"';
}
#endif

/** Save the probe PID for externally verified lifetime checks.
 * @param filename Exact UTF-8 destination path.
 */
void savePid(const std::string &filename) {
    std::ofstream output(std::filesystem::path(std::u8string(filename.begin(), filename.end())));
#ifdef _WIN32
    output << GetCurrentProcessId();
#else
    output << getpid();
#endif
    output.flush();

    if (!output) {
        throw std::runtime_error("cannot write pid file");
    }
}

/** Launch a child with inherited streams, without a shell or detached process group.
 * @param arguments Executable followed by exact UTF-8 argument strings.
 * @param wait Whether to wait and report the child's status.
 * @return Child exit when waited, otherwise zero after creation.
 */
int spawn(const std::vector<std::string> &arguments, bool wait) {
    if (arguments.empty()) {
        throw std::runtime_error("spawn requires an executable");
    }

#ifdef _WIN32
    std::wstring command;

    for (const auto &argument : arguments) {
        if (!command.empty()) {
            command += L' ';
        }

        command += quote(wide(argument));
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("probe child launch failed");
    }

    CloseHandle(process.hThread);
    DWORD result = 0;

    if (wait) {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &result);
    }

    CloseHandle(process.hProcess);

    return static_cast<int>(result);
#else
    std::vector<char *> native;

    for (const auto &argument : arguments) {
        native.push_back(const_cast<char *>(argument.c_str()));
    }

    native.push_back(nullptr);
    const pid_t child = fork();

    if (child < 0) {
        throw std::runtime_error("probe fork failed");
    }

    if (child == 0) {
        execvp(native[0], native.data());
        _exit(127);
    }

    if (!wait) {
        return 0;
    }

    int status = 0;

    if (waitpid(child, &status, 0) < 0) {
        throw std::runtime_error("probe wait failed");
    }

    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
}

/** Exercise one intentionally bounded process or byte-output behavior.
 * @param arguments Complete UTF-8 argument vector, including executable.
 * @return Requested child exit or nonzero for invalid invocation.
 */
int run(const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        throw std::runtime_error("probe requires mode");
    }

    const auto &mode = arguments[1];

    if (mode == "args") {
        for (std::size_t index = 2; index < arguments.size(); ++index) {
            std::cout << "ARG " << index - 2 << ' ' << hex(arguments[index]) << '\n';
        }

        const char *timezone = std::getenv("TZ");
        std::cout << "TZ=" << (timezone == nullptr ? "<unset>" : timezone) << '\n';
        const char *sentinel = std::getenv("QDCXX_LOGGER_TEST_SENTINEL");

        if (sentinel != nullptr) {
            std::cout << "SENTINEL=" << sentinel << '\n';
        }

        const auto cwd = std::filesystem::current_path().u8string();
        std::cout << "CWD=" << hex(std::string(cwd.begin(), cwd.end())) << '\n';
#ifdef _WIN32
        std::cout << "ACP=" << GetACP() << '\n';
#endif

        return 0;
    }

    if (mode == "exit") {
        return arguments.size() > 2 ? std::stoi(arguments[2]) : 37;
    }

    if (mode == "signal") {
#ifdef _WIN32

        return 37;
#else
        std::raise(SIGTERM);

        return 2;
#endif
    }

    if (mode == "text" || mode == "split-text") {
        // Exact malformed bytes and newline combinations are independent decoder expectations.
        const std::string output = "\xef\xbb\xbf"
                                   "A\r\nB\rC\n\nD\r\r\nE\xff\xc0\xaf\nF";

        for (const char byte : output) {
            std::cout.put(byte).flush();

            if (mode == "split-text") {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }

        std::cerr << "\xff\xfeZ\n";

        return 0;
    }

    if (mode == "utf8") {
        const std::string output = "\xe9\x9b\xaa\r\n\xf0\x9f\x98\x80\n\xe2\x82";

        for (const char byte : output) {
            std::cout.put(byte).flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }

        std::cerr << "stderr-final";

        return 0;
    }

    if (mode == "large") {
        const std::string fill(256, 'x');
        std::thread errorThread([&fill] {
            for (int index = 0; index < 5000; ++index) {
                std::cerr << 'E' << index << ':' << fill << '\n';
            }

            std::cerr << "E-final";
        });

        for (int index = 0; index < 5000; ++index) {
            std::cout << 'O' << index << ':' << fill << '\n';
        }

        std::cout << "O-final\n";
        errorThread.join();

        return 0;
    }

    if (mode == "gated-exit" && arguments.size() == 3) {
        savePid(arguments[2]);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

        while (!std::filesystem::exists(
            std::filesystem::path(std::u8string(arguments[2].begin(), arguments[2].end())) += ".ready")) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("exit gate not released");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return 0;
    }

    if (mode == "burst" && arguments.size() == 3) {
        savePid(arguments[2]);
        std::cout << std::string(4096, 'x') << '\n' << std::flush;

        return 0;
    }

    if ((mode == "running" || mode == "hold" || mode == "hold-group") && arguments.size() == 3) {
#ifndef _WIN32

        if (mode == "hold-group" && setpgid(0, 0) != 0) {
            throw std::runtime_error("probe process-group setup failed");
        }

#endif
        savePid(arguments[2]);
        // Outer containment must finish first; a blocked write still requires external termination.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);

        while (std::chrono::steady_clock::now() < deadline) {
            if (mode == "running") {
                std::cout << "running " << std::string(4096, 'x') << '\n' << std::flush;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return 0;
    }

    if ((mode == "orphan" || mode == "group-orphan") && arguments.size() == 4) {
        savePid(arguments[3]);
        spawn({arguments[0], mode == "group-orphan" ? "hold-group" : "hold", arguments[2]}, false);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

        while (
            !std::filesystem::exists(std::filesystem::path(std::u8string(arguments[2].begin(), arguments[2].end())))) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("descendant did not publish pid");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "orphan-ready\n";

        return 0;
    }

    if (mode == "spawn-environment") {
#ifdef _WIN32
        _putenv_s("TZ", "Pacific/Honolulu");
        _putenv_s("QDCXX_LOGGER_TEST_SENTINEL", "preserved");
#else
        setenv("TZ", "Pacific/Honolulu", 1);
        setenv("QDCXX_LOGGER_TEST_SENTINEL", "preserved", 1);
#endif
        const int result = spawn({arguments.begin() + 2, arguments.end()}, true);

        if (std::string(std::getenv("TZ")) != "Pacific/Honolulu" ||
            std::string(std::getenv("QDCXX_LOGGER_TEST_SENTINEL")) != "preserved") {
            throw std::runtime_error("launcher mutated parent environment");
        }

        return result;
    }

    if (mode == "spawn" || mode == "spawn-swallow") {
        const int result = spawn({arguments.begin() + 2, arguments.end()}, true);

        return mode == "spawn-swallow" ? 0 : result;
    }

    throw std::runtime_error("unknown probe mode: " + mode);
}

} // namespace

#ifdef _WIN32
/** Native test entry point; converts Windows Unicode arguments exactly.
 * @param argc Number of arguments.
 * @param argv Native UTF-16 argument vector.
 * @return Probe status, or 2 for a harness failure.
 */
int wmain(int argc, wchar_t **argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#else
/** Native test entry point, preserving argument bytes.
 * @param argc Number of arguments.
 * @param argv Native argument vector.
 * @return Probe status, or 2 for a harness failure.
 */
int main(int argc, char **argv) {
#endif
    try {
        std::vector<std::string> arguments;

        for (int index = 0; index < argc; ++index) {
#ifdef _WIN32
            arguments.push_back(utf8(argv[index]));
#else
            arguments.emplace_back(argv[index]);
#endif
        }

        return run(arguments);
    } catch (const std::exception &error) {
        std::cerr << "PROBE FAILED: " << error.what() << '\n';

        return 2;
    }
}
