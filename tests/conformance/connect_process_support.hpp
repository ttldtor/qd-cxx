// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#ifdef _WIN32
#    include <windows.h>
#else
#    include <cerrno>
#    include <csignal>
#    include <fcntl.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::connect_tests::process {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

/** Fail a real harness condition without depending on debug assertions.
 * @param condition Required observed condition.
 * @param message Static harness explanation.
 */
inline void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Read bounded synthetic child output only after that owned child has retired.
 * @param path Test-owned output file.
 * @return Exact bytes, bounded to 64 KiB.
 */
inline std::string read(const std::filesystem::path &path) {
    require(std::filesystem::file_size(path) <= 65536, "child output limit");
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "child capture open");

    return {std::istreambuf_iterator<char>(stream), {}};
}

/** Count exact output row prefixes without conflating layout/schema rows.
 * @param output Complete synthetic output.
 * @param prefix Exact row prefix.
 * @return Number of complete matching rows.
 */
inline std::size_t rows(std::string_view output, std::string_view prefix) {
    std::size_t count = 0;

    while (!output.empty()) {
        const auto end = output.find('\n');
        require(end != output.npos, "incomplete output line");
        count += output.substr(0, end).starts_with(prefix) ? 1 : 0;
        output.remove_prefix(end + 1);
    }

    return count;
}

#ifdef _WIN32
/** Sole owner of one native test handle, including partial construction paths. */
class Handle {
    HANDLE value_ = INVALID_HANDLE_VALUE;

    public:
    /** Adopt an exact test-owned handle.
     * @param value Handle or invalid sentinel.
     */
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {
    }

    /** Close only the currently owned handle. */
    ~Handle() {
        reset();
    }

    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;

    /** Replace ownership after closing the previous test handle.
     * @param value New owned handle or invalid sentinel.
     */
    void reset(HANDLE value = INVALID_HANDLE_VALUE) {
        if (value_ && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }

        value_ = value;
    }

    /** Borrow the currently owned native identity.
     * @return Native handle, never transferred.
     */
    HANDLE get() const {
        return value_;
    }
};

/** Quote a Windows CRT argument without invoking a command shell.
 * @param value Complete owned argument.
 * @return Correctly quoted native command-line token.
 */
inline std::wstring quote(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const auto unit : value) {
        if (unit == L'\\') {
            ++slashes;
        } else {
            result.append(slashes * (unit == L'"' ? 2 : 1), L'\\');
            slashes = 0;

            if (unit == L'"') {
                result += L'\\';
            }

            result += unit;
        }
    }

    result.append(slashes * 2, L'\\');
    result += L'"';

    return result;
}

/** Decode explicit UTF-8 argv bytes for native CreateProcessW without codepage mutation.
 * @param value Complete argument in the frontend's UTF-8 boundary policy.
 * @return Exact UTF-16 argument, rejecting invalid sequences before a child is created.
 */
inline std::wstring nativeArgument(std::string_view value) {
    require(value.size() <= INT_MAX, "child argument byte limit");

    if (value.empty())
        return {};

    const auto bytes = static_cast<int>(value.size());
    const auto units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), bytes, nullptr, 0);
    require(units > 0, "child argument UTF-8");
    std::wstring result(static_cast<std::size_t>(units), L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), bytes, result.data(), units) == units,
            "child argument UTF-16 conversion");

    return result;
}
#else
/** Sole owner of one native test descriptor. */
class Descriptor {
    int value_ = -1;

    public:
    /** Adopt a test-owned descriptor.
     * @param value Exact descriptor or invalid sentinel.
     */
    explicit Descriptor(int value = -1) : value_(value) {
    }

    /** Close only this owned descriptor. */
    ~Descriptor() {
        reset();
    }

    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;

    /** Replace ownership after closing the prior descriptor.
     * @param value New exact descriptor or invalid sentinel.
     */
    void reset(int value = -1) {
        if (value_ != -1) {
            ::close(value_);
        }

        value_ = value;
    }

    /** Borrow the owned descriptor.
     * @return Exact native identity.
     */
    int get() const {
        return value_;
    }
};
#endif

/** One exact owned child with bounded observation and automatic failure-path retirement.
 * Child stdin is an open pipe with no available bytes: help cannot succeed by reading stdin.
 */
class Child {
#ifdef _WIN32
    Handle process_;
    Handle inputWriter_;
    Handle outputReader_;
#else
    pid_t process_ = -1;
    Descriptor inputWriter_;
    Descriptor outputReader_;
#endif
    bool completed_ = false;

    public:
    /** Start one native child, without a shell or process-name lookup.
     * @param program Exact CLI executable path.
     * @param arguments Explicit owned working/help arguments.
     * @param output Test-owned stdout capture path.
     * @param error Test-owned stderr capture path.
     * @param pipeOutput Capture stdout in an owned pipe for real downstream sink failure.
     * @param brokenError Provide a stderr pipe whose read end is already closed.
     */
    Child(const std::filesystem::path &program, const std::vector<std::string> &arguments,
          const std::filesystem::path &output, const std::filesystem::path &error, bool pipeOutput = false,
          bool brokenError = false) {
#ifdef _WIN32
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
        HANDLE inputRead = nullptr;
        HANDLE inputWrite = nullptr;
        require(CreatePipe(&inputRead, &inputWrite, &attributes, 0), "child stdin pipe");
        Handle input(inputRead);
        inputWriter_.reset(inputWrite);
        require(SetHandleInformation(inputWriter_.get(), HANDLE_FLAG_INHERIT, 0), "stdin parent ownership");
        Handle out;

        if (pipeOutput) {
            HANDLE outputRead = nullptr;
            HANDLE outputWrite = nullptr;
            require(CreatePipe(&outputRead, &outputWrite, &attributes, 0), "child stdout pipe");
            outputReader_.reset(outputRead);
            out.reset(outputWrite);
            require(SetHandleInformation(outputReader_.get(), HANDLE_FLAG_INHERIT, 0), "stdout parent ownership");
        } else {
            out.reset(CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
        }

        Handle err;

        if (brokenError) {
            HANDLE errorRead = nullptr;
            HANDLE errorWrite = nullptr;
            require(CreatePipe(&errorRead, &errorWrite, &attributes, 0), "child broken stderr pipe");
            Handle reader(errorRead);
            err.reset(errorWrite);
        } else {
            err.reset(CreateFileW(error.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
        }

        require(out.get() != INVALID_HANDLE_VALUE && err.get() != INVALID_HANDLE_VALUE, "child captures");
        std::wstring command = quote(program.wstring());

        for (const auto &argument : arguments) {
            command += L' ';
            command += quote(nativeArgument(argument));
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input.get();
        startup.hStdOutput = out.get();
        startup.hStdError = err.get();
        PROCESS_INFORMATION information{};
        require(CreateProcessW(program.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                               nullptr, &startup, &information),
                "CreateProcessW");
        process_.reset(information.hProcess);
        Handle thread(information.hThread);
#else
        int inputPipe[2]{};
        require(::pipe(inputPipe) == 0, "child stdin pipe");
        Descriptor input(inputPipe[0]);
        inputWriter_.reset(inputPipe[1]);
        Descriptor out;

        if (pipeOutput) {
            int outputPipe[2]{};
            require(::pipe(outputPipe) == 0, "child stdout pipe");
            outputReader_.reset(outputPipe[0]);
            out.reset(outputPipe[1]);
            require(fcntl(outputReader_.get(), F_SETFL, O_NONBLOCK) == 0, "stdout nonblocking parent");
        } else {
            out.reset(open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
        }

        Descriptor err;

        if (brokenError) {
            int errorPipe[2]{};
            require(::pipe(errorPipe) == 0, "child broken stderr pipe");
            Descriptor reader(errorPipe[0]);
            err.reset(errorPipe[1]);
        } else {
            err.reset(open(error.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
        }

        require(out.get() >= 0 && err.get() >= 0, "child captures");
        std::vector<std::string> values{program.string()};
        values.insert(values.end(), arguments.begin(), arguments.end());
        std::vector<char *> argv;

        for (auto &value : values) {
            argv.push_back(value.data());
        }

        argv.push_back(nullptr);
        process_ = fork();
        require(process_ >= 0, "fork exact child");

        if (process_ == 0) {
            if (dup2(input.get(), STDIN_FILENO) < 0 || dup2(out.get(), STDOUT_FILENO) < 0 ||
                dup2(err.get(), STDERR_FILENO) < 0) {
                _exit(125);
            }

            ::close(inputWriter_.get());

            if (outputReader_.get() >= 0) {
                ::close(outputReader_.get());
            }

            execv(argv[0], argv.data());
            _exit(126);
        }

#endif
    }

    /** Retire only this exact owned child if an assertion/timeout prevented ordinary observation. */
    ~Child() {
        if (!completed_) {
#ifdef _WIN32

            if (process_.get() != INVALID_HANDLE_VALUE && WaitForSingleObject(process_.get(), 0) == WAIT_TIMEOUT) {
                TerminateProcess(process_.get(), 124);
                WaitForSingleObject(process_.get(), 5000);
            }

#else

            if (process_ > 0) {
                kill(process_, SIGKILL);
                int status = 0;

                while (waitpid(process_, &status, 0) < 0 && errno == EINTR) {
                }
            }

#endif
        }
    }

    /** Close a real downstream pipe after optionally observing the flushed output header.
     * @param waitForHeader Require a real initial output prefix before injecting closure.
     */
    void closeOutput(bool waitForHeader) {
        if (waitForHeader) {
            const auto deadline = Clock::now() + 5s;
            bool observed = false;

            while (Clock::now() < deadline && !observed) {
#ifdef _WIN32
                DWORD available = 0;
                require(PeekNamedPipe(outputReader_.get(), nullptr, 0, nullptr, &available, nullptr),
                        "stdout header pipe");
                observed = available != 0;
#else
                char byte = 0;
                const auto count = ::read(outputReader_.get(), &byte, 1);
                observed = count == 1;
#endif
                std::this_thread::sleep_for(1ms);
            }

            require(observed, "stdout header deadline");
        }

        outputReader_.reset();
    }

#ifndef _WIN32
    /** Deliver SIGINT only to this exact owned child after its header signaled handler installation. */
    void interrupt() {
        require(kill(process_, SIGINT) == 0, "owned child SIGINT");
    }

    /** Deliver SIGTERM only to this live exact owned child, never by process name. */
    void terminate() {
        require(!completed_ && process_ > 0, "live owned child SIGTERM target");
        require(kill(process_, SIGTERM) == 0, "owned child SIGTERM");
    }
#endif

    /** Observe one owned child completion under a finite deadline.
     * @return Native exit status; signal termination is mapped to 128 plus signal on POSIX.
     */
    int wait() {
        const auto deadline = Clock::now() + 8s;

        while (Clock::now() < deadline) {
#ifdef _WIN32
            const auto state = WaitForSingleObject(process_.get(), 0);
            require(state != WAIT_FAILED, "child wait failure");

            if (state == WAIT_OBJECT_0) {
                DWORD code = 0;
                require(GetExitCodeProcess(process_.get(), &code), "child exit observation");
                completed_ = true;

                return static_cast<int>(code);
            }

#else
            int status = 0;
            const auto state = waitpid(process_, &status, WNOHANG);
            require(state >= 0 || errno == EINTR, "child wait failure");

            if (state == process_) {
                completed_ = true;

                return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
            }

#endif
            std::this_thread::sleep_for(1ms);
        }

        throw std::runtime_error("owned child deadline; cleanup will retire it");
    }
};

} // namespace org::ttldtor::qdcxx::connect_tests::process
