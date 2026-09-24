// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "process.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <pthread.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#ifdef __linux__
#    include <dirent.h>
#    include <sys/prctl.h>
#    include <sys/syscall.h>
#endif

namespace org::ttldtor::qdcxx::dev {
namespace {
std::atomic<int> interrupted{0};
static_assert(std::atomic<int>::is_always_lock_free);
std::atomic<bool> outputCancelled{false};

/** Reads monotonic milliseconds for the shared cleanup deadline.
 * @return Milliseconds since an unspecified steady-clock epoch.
 */
long long monotonicMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/** Keeps root observation and owned termination independent of stalled destinations. */
class RootWatch {
    pid_t child_;
    std::jthread watcher_;

    public:
    /// Root exit observed without reaping, so its PID remains reserved until main-thread cleanup.
    std::atomic<bool> exited{false};
    /// First root exit, interruption or failure time; zero before cleanup starts.
    std::atomic<long long> startedAt{0};
    /// A failed native wait or termination operation.
    std::atomic<bool> failed{false};

    /** Starts cleanup exactly once, while the owned root PID is still reserved.
     * @param rootDead Whether the root is already known to have exited.
     */
    void begin(bool rootDead) {
        long long expected = 0;

        if (!startedAt.compare_exchange_strong(expected, monotonicMs())) {
            return;
        }

        if (!rootDead && kill(child_, SIGKILL) < 0 && errno != ESRCH) {
            failed = true;
        }

        if (kill(-child_, SIGKILL) < 0 && errno != ESRCH) {
            failed = true;
        }
    }

    /** Observes only the explicitly owned root.
     * @param child Owned session leader, not yet reaped.
     */
    explicit RootWatch(pid_t child) : child_(child) {
        outputCancelled = false;
        try {
            watcher_ = std::jthread([this](std::stop_token stop) {
                while (!stop.stop_requested()) {
                    if (!exited) {
                        siginfo_t status{};
                        const int result =
                            waitid(P_PID, static_cast<id_t>(child_), &status, WEXITED | WNOHANG | WNOWAIT);

                        if (result == 0 && status.si_pid == child_) {
                            begin(true);
                            exited = true;
                        } else if (result < 0 && errno != EINTR) {
                            failed = true;
                            begin(false);
                        }
                    }

                    if (interrupted) {
                        begin(exited);
                    }

                    // Reserve the final second of the five-second grace for confirmed cleanup.
                    if (interrupted || (startedAt && monotonicMs() - startedAt >= 4000)) {
                        outputCancelled = true;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
        } catch (...) {
            // The child is still at the pre-exec gate and cannot have descendants yet.
            kill(child_, SIGKILL);
            const auto deadline = monotonicMs() + 5000;
            int status = 0;

            while (waitpid(child_, &status, WNOHANG) == 0 && monotonicMs() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            throw;
        }
    }

    ~RootWatch() {
        watcher_.request_stop();
        watcher_.join();
        outputCancelled = false;
    }
};

/** Records a graceful cancellation request without unsafe signal-handler work.
 * @param signal Received signal number.
 */
void interruptHandler(int signal) {
    interrupted = signal;
}

/** Owns one descriptor, including descriptors zero through two. */
struct Descriptor {
    /// Owned descriptor, or -1 after closure.
    int value = -1;

    ~Descriptor() {
        if (value >= 0) {
            close(value);
        }
    }

    /** Closes an owned descriptor exactly once. */
    void reset() {
        if (value >= 0) {
            close(value);
            value = -1;
        }
    }
};

#ifdef __linux__
/** Owns the Linux process inventory without retaining process names or unrelated state. */
struct ProcessDirectory {
    /// Native process-directory stream, closed by this owner.
    DIR *value = opendir("/proc");

    ProcessDirectory() {
        if (!value) {
            throw std::runtime_error("session process inventory unavailable");
        }
    }

    ~ProcessDirectory() {
        closedir(value);
    }
};

/** Verifies safe session-cleanup primitives before permitting any user command to execute. */
void verifySessionCleanup() {
    ProcessDirectory processes;
    Descriptor self{static_cast<int>(syscall(SYS_pidfd_open, getpid(), 0))};
    siginfo_t status{};
    struct sigaction childAction{};

    if (sigaction(SIGCHLD, nullptr, &childAction) != 0 || childAction.sa_handler == SIG_IGN ||
        (childAction.sa_flags & SA_NOCLDWAIT)) {
        throw std::runtime_error("Linux session cleanup requires a waitable root child");
    }

    if (self.value < 0 || syscall(SYS_pidfd_send_signal, self.value, 0, nullptr, 0) < 0 ||
        waitid(P_PIDFD, static_cast<id_t>(self.value), &status, WEXITED | WNOHANG) != -1 || errno != ECHILD) {
        throw std::runtime_error("Linux session cleanup requires pidfd open, signal and wait support");
    }
}

/** Terminates and reaps session members even if they changed process group.
 * The unreaped root reserves the session ID. A pidfd pins each observed process,
 * so a recycled numeric PID can never redirect a signal or wait to another process.
 * @param root Owned session leader, retained unreaped until this operation confirms emptiness.
 * @param deadlineMs Existing cleanup deadline in monotonic milliseconds.
 * @return True only when a complete inventory contains no remaining session member except root.
 */
bool cleanSession(pid_t root, long long deadlineMs) {
    ProcessDirectory processes;
    bool empty = true;

    while (true) {
        if (monotonicMs() >= deadlineMs) {
            throw std::runtime_error("owned session cleanup exceeded five seconds");
        }

        errno = 0;
        const auto *entry = readdir(processes.value);

        if (!entry) {
            if (errno) {
                throw std::runtime_error("session process inventory failed");
            }

            return empty;
        }

        pid_t candidate = 0;
        const auto *end = entry->d_name + std::strlen(entry->d_name);
        const auto parsed = std::from_chars(entry->d_name, end, candidate);

        if (parsed.ec != std::errc{} || parsed.ptr != end || candidate <= 0 || candidate == root ||
            getsid(candidate) != root) {
            continue;
        }

        Descriptor process{static_cast<int>(syscall(SYS_pidfd_open, candidate, 0))};

        if (process.value < 0) {
            if (errno == ESRCH) {
                continue;
            }

            throw std::runtime_error("owned session process handle failed");
        }

        // Recheck after opening: the PID may have disappeared or been recycled during open.
        if (getsid(candidate) != root) {
            continue;
        }

        empty = false;

        if (syscall(SYS_pidfd_send_signal, process.value, SIGKILL, nullptr, 0) < 0 && errno != ESRCH) {
            throw std::runtime_error("owned session process termination failed");
        }

        siginfo_t status{};

        // Nonchildren become waitable after their killed parent is reaped by the subreaper.
        if (waitid(P_PIDFD, static_cast<id_t>(process.value), &status, WEXITED | WNOHANG) < 0 && errno != ECHILD &&
            errno != EINTR) {
            throw std::runtime_error("owned session process wait failed");
        }
    }
}
#endif

/** Creates close-on-exec descriptors above the standard descriptor range.
 * @param ends Two initially empty owners receiving read/write ends.
 */
void makePipe(std::array<Descriptor, 2> &ends) {
    int raw[2];

    if (pipe(raw) != 0) {
        throw std::runtime_error("pipe failed");
    }

    ends[0].value = raw[0];
    ends[1].value = raw[1];

    for (auto &end : ends) {
        const int duplicated = fcntl(end.value, F_DUPFD_CLOEXEC, 3);

        if (duplicated < 0) {
            throw std::runtime_error("pipe descriptor setup failed");
        }

        end.reset();
        end.value = duplicated;
    }
}

/** Restores process-local signal handlers after the command. */
struct Signals {
    /// Previous SIGINT handler.
    struct sigaction oldInt{};
    /// Previous SIGTERM handler.
    struct sigaction oldTerm{};
    /// Previous SIGPIPE handler, restored after pipe-error handling.
    struct sigaction oldPipe{};

    Signals() {
        interrupted = 0;
        struct sigaction action{};
        action.sa_handler = interruptHandler;
        sigemptyset(&action.sa_mask);

        if (sigaction(SIGINT, &action, &oldInt) != 0) {
            throw std::runtime_error("signal handler setup failed");
        }

        if (sigaction(SIGTERM, &action, &oldTerm) != 0) {
            sigaction(SIGINT, &oldInt, nullptr);
            throw std::runtime_error("signal handler setup failed");
        }

        action.sa_handler = SIG_IGN;

        if (sigaction(SIGPIPE, &action, &oldPipe) != 0) {
            sigaction(SIGINT, &oldInt, nullptr);
            sigaction(SIGTERM, &oldTerm, nullptr);
            throw std::runtime_error("broken-pipe handler setup failed");
        }
    }

    ~Signals() {
        sigaction(SIGINT, &oldInt, nullptr);
        sigaction(SIGTERM, &oldTerm, nullptr);
        sigaction(SIGPIPE, &oldPipe, nullptr);
    }
};
} // namespace

namespace {
/** Writes to one native destination using the active owned-process deadline.
 * @param descriptor Borrowed output descriptor.
 * @param bytes Bytes remaining to write.
 */
void writeDestination(int descriptor, std::string_view bytes) {
    while (!bytes.empty()) {
        if (outputCancelled) {
            throw std::runtime_error("destination stalled during owned cleanup");
        }

        pollfd output{descriptor, POLLOUT, 0};
        const int ready = poll(&output, 1, 10);

        if (!ready || (ready < 0 && errno == EINTR)) {
            continue;
        }

        if (ready < 0 || (output.revents & (POLLERR | POLLNVAL | POLLHUP))) {
            throw std::runtime_error("output destination failed");
        }

        // POLLOUT permits an atomic-sized pipe write by this sole writer without blocking.
        // Preserve the inherited open-file description's flags, which the caller may share.
        const auto atomicBytes = fpathconf(descriptor, _PC_PIPE_BUF);
        const auto chunk = (std::min)(bytes.size(), static_cast<std::size_t>(atomicBytes > 0 ? atomicBytes : 512));
        const auto written = write(descriptor, bytes.data(), chunk);

        if (written < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }

        if (written <= 0) {
            throw std::runtime_error("destination write failed");
        }

        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
}

} // namespace

LogFile::LogFile(const std::filesystem::path &path) {
    handle_ = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NONBLOCK, 0666);

    if (handle_ < 0) {
        throw std::runtime_error("log open failed");
    }
}

LogFile::~LogFile() {
    if (handle_ >= 0) {
        ::close(static_cast<int>(handle_));
    }
}

void LogFile::write(std::string_view bytes) {
    writeDestination(static_cast<int>(handle_), bytes);
}

void LogFile::close() {
    const int descriptor = static_cast<int>(handle_);
    handle_ = -1;

    if (::close(descriptor) != 0) {
        throw std::runtime_error("log close failed");
    }
}

void writeConsole(std::string_view bytes) {
    // Broken pipes are ordinary launcher failures, including during help before ownership starts.
    struct sigaction action{};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    sigaction(SIGPIPE, &action, nullptr);
    writeDestination(STDOUT_FILENO, bytes);
}

void writeDiagnostic(std::string_view prefix, std::string_view bytes) noexcept {
    try {
        std::string owned(prefix);
        owned += bytes;

        if (owned.empty() || owned.back() != '\n') {
            owned += '\n';
        }

        const auto completed = std::make_shared<std::atomic<bool>>(false);
        std::thread writer([message = std::move(owned), completed] {
            sigset_t signals;
            sigemptyset(&signals);
            sigaddset(&signals, SIGPIPE);

            if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) == 0) {
                const auto ignored = ::write(STDERR_FILENO, message.data(), message.size());
                (void)ignored;
            }

            completed->store(true);
        });
        writer.detach();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);

        while (!completed->load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } catch (...) {
        // Allocation/thread-creation failure cannot replace the original authoritative failure exit.
    }
}

int runOwned(const std::vector<std::string> &command, const StreamSink &sink,
             const std::function<void(int)> &complete) {
    Signals signals;
#ifdef __linux__

    verifySessionCleanup();

    // Reap inherited descendants ourselves so group-empty confirmation does not depend on PID 1.
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0) {
        throw std::runtime_error("subreaper setup failed");
    }

#endif
    std::array<Descriptor, 2> output;
    std::array<Descriptor, 2> errors;
    std::array<Descriptor, 2> launch;
    std::array<Descriptor, 2> gate;
    makePipe(output);
    makePipe(errors);
    makePipe(launch);
    makePipe(gate);
    std::vector<char *> native;

    for (const auto &argument : command) {
        native.push_back(const_cast<char *>(argument.c_str()));
    }

    native.push_back(nullptr);
    const auto child = fork();

    if (child < 0) {
        throw std::runtime_error("fork failed");
    }

    if (child == 0) {
        close(launch[0].value);
        int failure = 0;

        close(gate[1].value);
        char permitted = 0;

        if (setsid() < 0 || read(gate[0].value, &permitted, 1) != 1 || dup2(output[1].value, STDOUT_FILENO) < 0 ||
            dup2(errors[1].value, STDERR_FILENO) < 0 || setenv("TZ", "UTC", 1) != 0) {
            failure = errno;
        }

        if (!failure) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            execvp(native[0], native.data());
            failure = errno;
        }

        const auto ignored = write(launch[1].value, &failure, sizeof(failure));
        (void)ignored;
        _exit(127);
    }

    output[1].reset();
    errors[1].reset();
    launch[1].reset();
    gate[0].reset();
    RootWatch root(child);
    std::array<Descriptor *, 3> readers{&output[0], &errors[0], &launch[0]};
    std::exception_ptr failure;
    constexpr char EXECUTE = 1;

    if (write(gate[1].value, &EXECUTE, 1) != 1) {
        failure = std::make_exception_ptr(std::runtime_error("pre-execution gate failed"));
    }

    gate[1].reset();

    for (auto *reader : readers) {
        if (fcntl(reader->value, F_SETFL, O_NONBLOCK) != 0) {
            failure = std::make_exception_ptr(std::runtime_error("nonblocking pipe setup failed"));
        }
    }

    bool rootExited = false;
    bool cleaning = false;
    int rootStatus = 0;
    auto deadline = std::chrono::steady_clock::time_point::max();
    std::array<char, 8192> buffer{};

    while (true) {
        if (!rootExited && root.exited) {
#ifdef __linux__
            // Pin the root PID/session ID until all other same-session processes are gone.
            rootExited = true;
#else
            const auto waited = waitpid(child, &rootStatus, WNOHANG);

            if (waited == child) {
                rootExited = true;
            } else if (waited < 0 && errno != EINTR) {
                failure = std::make_exception_ptr(std::runtime_error("root wait failed"));
            }

#endif
        }

        if (interrupted && !failure) {
            failure = std::make_exception_ptr(std::runtime_error("launcher interrupted"));
        }

        if (root.failed && !failure) {
            failure = std::make_exception_ptr(std::runtime_error("owned root observation or termination failed"));
        }

        if (!cleaning && (rootExited || failure)) {
            cleaning = true;
            root.begin(rootExited);
            deadline = std::chrono::steady_clock::time_point(std::chrono::milliseconds(root.startedAt.load())) +
                       std::chrono::seconds(5);
        }

        bool ownedEmpty = false;

        if (cleaning) {
#ifdef __linux__
            ownedEmpty = cleanSession(child, root.startedAt.load() + 5000);
#else

            // This unqualified fallback has only process-group containment; macOS is not qualified.
            if (rootExited) {
                int descendantStatus = 0;

                while (waitpid(-child, &descendantStatus, WNOHANG) > 0) {
                }
            }

            ownedEmpty = kill(-child, 0) < 0 && errno == ESRCH;
#endif
        }

        for (std::size_t index = 0; index < readers.size(); ++index) {
            auto &reader = *readers[index];

            if (reader.value < 0) {
                continue;
            }

            const auto count = read(reader.value, buffer.data(), buffer.size());

            if (count > 0) {
                if (index == 2) {
                    failure = std::make_exception_ptr(std::runtime_error("child setup or executable launch failed"));
                } else if (!failure) {
                    try {
                        sink(static_cast<int>(index), std::string_view(buffer.data(), static_cast<std::size_t>(count)));
                    } catch (...) {
                        failure = std::current_exception();
                    }
                }
            } else if (count == 0) {
                reader.reset();

                if (index != 2 && !failure) {
                    try {
                        sink(static_cast<int>(index), {});
                    } catch (...) {
                        failure = std::current_exception();
                    }
                }
            } else if (errno != EAGAIN && errno != EINTR) {
                reader.reset();
                failure = std::make_exception_ptr(std::runtime_error("child pipe read failed"));
            }
        }

        if (cleaning && rootExited && ownedEmpty &&
            (failure || (output[0].value < 0 && errors[0].value < 0 && launch[0].value < 0))) {
            break;
        }

        if (cleaning && std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("owned cleanup exceeded five seconds");
        }

        std::array<pollfd, 3> polls{};

        for (std::size_t index = 0; index < readers.size(); ++index) {
            polls[index] = {readers[index]->value, POLLIN, 0};
        }

        poll(polls.data(), polls.size(), 10);
    }

#ifdef __linux__

    // WNOWAIT observation already confirmed root exit; this cannot wait for running user code.
    if (waitpid(child, &rootStatus, WNOHANG) != child) {
        throw std::runtime_error("owned root final reap failed");
    }

#endif

    if (failure) {
        std::rethrow_exception(failure);
    }

    const int result = WIFEXITED(rootStatus) ? WEXITSTATUS(rootStatus) : 128 + WTERMSIG(rootStatus);

    if (complete) {
        complete(result);
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::dev
