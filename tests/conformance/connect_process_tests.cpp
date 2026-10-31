// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: PROGRAM RUN are explicit; only owned local peers/children/files are touched.
#include "connect_peer.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::events;
namespace t = root::transport;
namespace s = root::connect_tests;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

/** Fail a real harness condition without depending on debug assertions.
 * @param condition Required observed condition.
 * @param message Static harness explanation.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Read bounded synthetic child output only after that owned child has retired.
 * @param path Test-owned output file.
 * @return Exact bytes, bounded to 64 KiB.
 */
std::string read(const std::filesystem::path &path) {
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
std::size_t rows(std::string_view output, std::string_view prefix) {
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
std::wstring quote(std::wstring_view value) {
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
            command += quote(std::wstring(argument.begin(), argument.end()));
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

/** Build complete standard binary schema and finite synthetic values.
 * @param profile Select complete Profile rather than Quote.
 * @return Owning validated immutable schema.
 */
c::LocalSchema schema(bool profile) {
    constexpr c::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto defaults = profile ? e::defaultProfileSchema(LIMITS) : e::defaultQuoteSchema(LIMITS);
    require(defaults.has_value(), "full standard schema");
    auto result = c::LocalSchema::create(defaults->records(), LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    require(result.has_value(), "binary standard schema");

    return std::move(*result);
}

/** Fill every local standard field, without reducing the actual schema.
 * @param local Complete explicit standard schema.
 * @return Exact finite typed values.
 */
std::vector<c::FieldValue> fields(const c::LocalSchema &local) {
    std::vector<c::FieldValue> result;

    for (const auto &field : local.records()[0].fields) {
        const auto type = static_cast<c::SerialFieldType>(field.serialTypeId);
        c::FieldValue value{type, 0};

        if (type == c::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"CLI";
        }

        result.push_back(std::move(value));
    }

    return result;
}
} // namespace

/** Run real CLI help, loopback, peer-loss, broken-pipe and owned-process cleanup scenarios.
 * @param argc Must contain exact executable and private output-directory arguments.
 * @param argv Test invocation values, borrowed through main.
 * @return Zero only for all actual child/peer observations; diagnostics name the failing harness condition.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "PROGRAM RUN required");
        const std::filesystem::path program = std::filesystem::absolute(argv[1]);
        const std::filesystem::path run = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(run);
        {
            Child noargs(program, {}, run / "noargs.out", run / "noargs.err");
            require(noargs.wait() == 0, "noargs with blocked stdin");
            Child help(program, {"--help"}, run / "help.out", run / "help.err");
            require(help.wait() == 0, "help with blocked stdin");
            require(read(run / "noargs.out") == read(run / "help.out"), "help equality");
            require(read(run / "noargs.err").empty() && read(run / "help.err").empty(), "help empty stderr");
            Child failedHelp(program, {"--help"}, run / "unused.out", run / "help-failed.err", true);
            failedHelp.closeOutput(false);
            require(failedHelp.wait() != 0, "closed help pipe cannot succeed");
        }

        auto bootstrap = t::RuntimeBootstrap::create();
        require(bootstrap.has_value(), "parent main-thread bootstrap before peer threads");

        for (const bool profile : {false, true}) {
            auto local = schema(profile);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}, {u"OTHER", fields(local)}};
            s::Peer peer(local, records, selected);
            const std::string name = profile ? "profile" : "quote";
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--schema-profile", name, "--output", name, "--max-records", "1", "--duration-ms", "2000"},
                        run / (name + ".out"), run / (name + ".err"));
            require(child.wait() == 0, "actual standard CLI target exit");
            require(rows(read(run / (name + ".out")), name + "\t") == 1, "exact selected CLI standard record");
            require(peer.adds() == 1 && !peer.failed(), "actual peer add observation");
            require(read(run / (name + ".err")).find("target-reached=1") != std::string::npos, "data target evidence");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "600", "--remove-after-ms", "300"},
                        run / "remove.out", run / "remove.err");
            const auto code = child.wait();
            const auto errors = read(run / "remove.err");
#ifdef _WIN32
            const bool ambiguity =
                errors.find("primary=10 primary-stage=socket-read primary-native=1236") != std::string::npos;
            require(code == (ambiguity ? 1 : 0), "Windows removal close status preserves native failure");
#else
            require(code == 0, "removal intentional cutoff exit");
#endif
            require(peer.adds() == 1 && peer.removes() == 1 && !peer.failed(), "actual peer add/remove tuples");
            require(rows(read(run / "remove.out"), "record\t") == 1, "no post-remove callback");
            require(errors.find("remove-applied=1 remove-composed=1") != std::string::npos, "separate removal stages");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected, 8192, s::PeerMode::PARTIAL_END);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "2000"},
                        run / "partial.out", run / "partial.err");
            require(child.wait() == 1, "partial-frame peer loss fails");
            require(rows(read(run / "partial.out"), "record\t") == 1, "complete admitted prefix before peer loss");
            require(read(run / "partial.err").find("primary=14") != std::string::npos,
                    "protocol terminal remains visible");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "2000"},
                        run / "unused-sink.out", run / "sink.err", true);
            child.closeOutput(true);
            require(child.wait() == 1, "real callback broken-pipe output fails");
            require(peer.adds() == 1 && !peer.failed(), "sink fails after actual add and source activation");
            require(read(run / "sink.err").find("local outcome=2") != std::string::npos, "sink failure local terminal");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM", "--output",
                         "quote", "--max-records", "1", "--duration-ms", "2000"},
                        run / "stderr-failed.out", run / "unused-stderr.err", false, true);
            require(child.wait() == 1, "failed diagnostic stderr cannot report successful target");
            require(rows(read(run / "stderr-failed.out"), "quote\t") == 1,
                    "stderr failure follows complete data output");
            require(peer.adds() == 1 && !peer.failed(), "stderr failure follows actual peer activation");
        }

#ifndef _WIN32
        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            s::Peer peer(local, {}, selected, 8192, s::PeerMode::SILENT);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "5000", "--max-records", "1"},
                        run / "unused-interrupt.out", run / "interrupt.err", true);
            child.closeOutput(true);
            const auto deadline = Clock::now() + 3s;

            while (peer.adds() == 0 && Clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }

            require(peer.adds() == 1, "interrupt after actual connection and peer add");
            child.interrupt();
            require(child.wait() == 130, "actual owned-child SIGINT cooperative exit");
            require(read(run / "interrupt.err").find("interrupted=1") != std::string::npos,
                    "actual interrupt diagnostic");
        }
#endif

        std::cout << "CLI blocked-stdin help, failed-help pipe, full Quote/Profile, add/remove, peer loss, callback "
                     "broken pipe; owned children retired\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
