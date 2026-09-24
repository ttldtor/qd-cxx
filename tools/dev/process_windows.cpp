// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "process.hpp"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

namespace org::ttldtor::qdcxx::dev {
namespace {
volatile LONG interrupted = 0;
std::atomic<bool> outputCancelled{false};

/** Records a console cancellation for bounded cleanup.
 * @param event Console control event.
 * @return TRUE for supported graceful cancellation events.
 */
BOOL WINAPI interruptHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        InterlockedExchange(&interrupted, 1);

        return TRUE;
    }

    return FALSE;
}

/** Owns a kernel handle. */
struct Handle {
    /// Native handle, null when empty.
    HANDLE value = nullptr;

    ~Handle() {
        reset();
    }

    /** Closes this handle, without affecting other owners. */
    void reset() {
        if (value && value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }

        value = nullptr;
    }
};

/** Reads monotonic milliseconds for the shared cleanup deadline.
 * @return Milliseconds since an unspecified steady-clock epoch.
 */
long long monotonicMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/** Observes root lifetime and cancels stalled output independently of the writing thread. */
class RootWatch {
    HANDLE root_;
    HANDLE job_;
    Handle writer_;
    std::jthread watcher_;

    public:
    /// First observed exit, interruption or destination failure time.
    std::atomic<long long> startedAt{0};
    /// A native observation or owned termination failed.
    std::atomic<bool> failed{false};

    /** Starts the single owned termination operation and shared five-second grace. */
    void begin() {
        long long expected = 0;

        if (startedAt.compare_exchange_strong(expected, monotonicMs()) && !TerminateJobObject(job_, 1)) {
            failed = true;
        }
    }

    /** Watches only handles owned by the current invocation.
     * @param root Owned root process handle, retained for this watcher's lifetime.
     * @param job Owned containment job, retained for this watcher's lifetime.
     */
    RootWatch(HANDLE root, HANDLE job) : root_(root), job_(job) {
        outputCancelled = false;

        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &writer_.value, 0, FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            TerminateJobObject(job_, 1);
            WaitForSingleObject(root_, 5000);
            throw std::runtime_error("output cancellation setup failed");
        }

        try {
            watcher_ = std::jthread([this](std::stop_token stop) {
                while (!stop.stop_requested()) {
                    const DWORD state = WaitForSingleObject(root_, 0);

                    if (state == WAIT_OBJECT_0 || state == WAIT_FAILED ||
                        InterlockedCompareExchange(&interrupted, 0, 0)) {
                        if (state == WAIT_FAILED) {
                            failed = true;
                        }

                        begin();
                    }

                    // Reserve one second of the five-second grace for confirmed owned completion.
                    if (startedAt &&
                        (monotonicMs() - startedAt >= 4000 || InterlockedCompareExchange(&interrupted, 0, 0))) {
                        outputCancelled = true;

                        if (!CancelSynchronousIo(writer_.value) && GetLastError() != ERROR_NOT_FOUND) {
                            failed = true;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
        } catch (...) {
            // Construction happens while the root is still suspended, before user code can run.
            TerminateJobObject(job_, 1);
            WaitForSingleObject(root_, 5000);
            throw;
        }
    }

    ~RootWatch() {
        watcher_.request_stop();
        watcher_.join();
        outputCancelled = false;
    }
};

/** Converts UTF-8 command text with strict validation.
 * @param text UTF-8 bytes.
 * @return UTF-16 text.
 */
std::wstring toWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);

    if (!count) {
        throw std::runtime_error("invalid UTF-8 argument");
    }

    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                        count);

    return result;
}

/** Encodes an argument for the Windows CRT command-line parser.
 * @param value Exact UTF-16 argument.
 * @return Quoted command-line argument.
 */
std::wstring quote(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;

    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++slashes;
        } else {
            result.append(slashes * (ch == L'"' ? 2 : 1), L'\\');
            slashes = 0;

            if (ch == L'"') {
                result += L'\\';
            }

            result += ch;
        }
    }

    result.append(slashes * 2, L'\\');

    return result + L'"';
}

/** Creates an inheritable write end and a private read end.
 * @param ends Initially empty pipe handle owners.
 */
void makePipe(std::array<Handle, 2> &ends) {
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};

    if (!CreatePipe(&ends[0].value, &ends[1].value, &attributes, 0) ||
        !SetHandleInformation(ends[0].value, HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("pipe creation failed");
    }
}

/** Registers and restores the launcher console handler. */
struct Cancellation {
    Cancellation() {
        InterlockedExchange(&interrupted, 0);

        if (!SetConsoleCtrlHandler(interruptHandler, TRUE)) {
            throw std::runtime_error("console control setup failed");
        }
    }

    ~Cancellation() {
        SetConsoleCtrlHandler(interruptHandler, FALSE);
    }
};
} // namespace

std::string toUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                                          nullptr, 0, nullptr, nullptr);

    if (!count) {
        throw std::runtime_error("invalid UTF-16 argument");
    }

    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(),
                        count, nullptr, nullptr);

    return result;
}

namespace {
/** Writes one native destination while permitting watchdog cancellation of synchronous I/O.
 * @param handle Borrowed output handle.
 * @param bytes Bytes remaining to write.
 */
void writeDestination(HANDLE handle, std::string_view bytes) {
    while (!bytes.empty()) {
        if (outputCancelled) {
            throw std::runtime_error("destination stalled during owned cleanup");
        }

        DWORD count = 0;
        const auto chunk = static_cast<DWORD>((std::min)(bytes.size(), static_cast<std::size_t>(MAXDWORD)));

        if (!WriteFile(handle, bytes.data(), chunk, &count, nullptr) || !count) {
            throw std::runtime_error("destination write failed");
        }

        bytes.remove_prefix(count);
    }
}

} // namespace

LogFile::LogFile(const std::filesystem::path &path) {
    const auto native = path.native();
    const DWORD creation = native.starts_with(L"\\\\.\\pipe\\") ? OPEN_EXISTING : CREATE_ALWAYS;
    const auto handle = CreateFileW(native.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    creation, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("log open failed");
    }

    handle_ = reinterpret_cast<std::intptr_t>(handle);
}

LogFile::~LogFile() {
    if (handle_ != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(handle_));
    }
}

void LogFile::write(std::string_view bytes) {
    writeDestination(reinterpret_cast<HANDLE>(handle_), bytes);
}

void LogFile::close() {
    const auto handle = reinterpret_cast<HANDLE>(handle_);
    handle_ = -1;

    if (!CloseHandle(handle)) {
        throw std::runtime_error("log close failed");
    }
}

void writeConsole(std::string_view bytes) {
    writeDestination(GetStdHandle(STD_OUTPUT_HANDLE), bytes);
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
            DWORD count = 0;
            WriteFile(GetStdHandle(STD_ERROR_HANDLE), message.data(),
                      static_cast<DWORD>((std::min)(message.size(), static_cast<std::size_t>(MAXDWORD))), &count,
                      nullptr);
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
    Cancellation cancellation;
    std::array<Handle, 2> output;
    std::array<Handle, 2> errors;
    makePipe(output);
    makePipe(errors);
    Handle input;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};

    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE), GetCurrentProcess(), &input.value, 0,
                         TRUE, DUPLICATE_SAME_ACCESS)) {
        input.value =
            CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    }

    if (input.value == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("stdin setup failed");
    }

    Handle job;
    job.value = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        throw std::runtime_error("job creation failed");
    }

    std::wstring commandLine;

    for (const auto &argument : command) {
        if (!commandLine.empty()) {
            commandLine += L' ';
        }

        commandLine += quote(toWide(argument));
    }

    std::vector<std::wstring> entries;
    auto *inherited = GetEnvironmentStringsW();

    if (!inherited) {
        throw std::runtime_error("environment read failed");
    }

    for (auto *entry = inherited; *entry; entry += wcslen(entry) + 1) {
        if (_wcsnicmp(entry, L"TZ=", 3) != 0) {
            entries.emplace_back(entry);
        }
    }

    FreeEnvironmentStringsW(inherited);
    entries.emplace_back(L"TZ=UTC");
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    });
    std::vector<wchar_t> environment;

    for (const auto &entry : entries) {
        environment.insert(environment.end(), entry.begin(), entry.end());
        environment.push_back(L'\0');
    }

    environment.push_back(L'\0');
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.value;
    startup.StartupInfo.hStdOutput = output[1].value;
    startup.StartupInfo.hStdError = errors[1].value;
    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    std::vector<unsigned char> attributeStorage(attributeBytes);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());

    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeBytes)) {
        throw std::runtime_error("handle inheritance setup failed");
    }

    const std::array<HANDLE, 3> inheritedHandles{input.value, output[1].value, errors[1].value};

    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   const_cast<HANDLE *>(inheritedHandles.data()), sizeof(inheritedHandles), nullptr,
                                   nullptr)) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        throw std::runtime_error("handle inheritance list failed");
    }

    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                                        environment.data(), nullptr, &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(startup.lpAttributeList);

    if (!created) {
        throw std::runtime_error("executable launch failed");
    }

    Handle root;
    root.value = process.hProcess;
    Handle thread;
    thread.value = process.hThread;

    if (!AssignProcessToJobObject(job.value, root.value)) {
        TerminateProcess(root.value, 1);
        WaitForSingleObject(root.value, 5000);
        throw std::runtime_error("pre-execution process ownership failed");
    }

    RootWatch watch(root.value, job.value);

    if (ResumeThread(thread.value) == static_cast<DWORD>(-1)) {
        watch.begin();
        WaitForSingleObject(root.value, 5000);
        throw std::runtime_error("owned root resume failed");
    }

    thread.reset();
    output[1].reset();
    errors[1].reset();
    input.reset();
    std::exception_ptr failure;
    bool rootExited = false;
    bool cleaning = false;
    DWORD rootExit = 1;
    auto deadline = std::chrono::steady_clock::time_point::max();
    std::array<Handle *, 2> readers{&output[0], &errors[0]};
    std::array<char, 8192> buffer{};

    while (true) {
        const auto rootState = WaitForSingleObject(root.value, 0);

        if (rootState == WAIT_FAILED && !failure) {
            failure = std::make_exception_ptr(std::runtime_error("root wait failed"));
        }

        if (!rootExited && rootState == WAIT_OBJECT_0) {
            rootExited = true;

            if (!GetExitCodeProcess(root.value, &rootExit)) {
                failure = std::make_exception_ptr(std::runtime_error("root exit query failed"));
            }
        }

        if (InterlockedCompareExchange(&interrupted, 0, 0) && !failure) {
            failure = std::make_exception_ptr(std::runtime_error("launcher interrupted"));
        }

        if (watch.failed && !failure) {
            failure = std::make_exception_ptr(std::runtime_error("owned observation or termination failed"));
        }

        if (!cleaning && (rootExited || failure)) {
            cleaning = true;
            watch.begin();
            deadline = std::chrono::steady_clock::time_point(std::chrono::milliseconds(watch.startedAt.load())) +
                       std::chrono::seconds(5);
        }

        for (std::size_t index = 0; index < readers.size(); ++index) {
            auto &reader = *readers[index];

            if (!reader.value) {
                continue;
            }

            DWORD available = 0;

            if (!PeekNamedPipe(reader.value, nullptr, 0, nullptr, &available, nullptr)) {
                const auto error = GetLastError();
                reader.reset();

                if (error != ERROR_BROKEN_PIPE) {
                    failure = std::make_exception_ptr(std::runtime_error("child pipe query failed"));
                } else if (!failure) {
                    try {
                        sink(static_cast<int>(index), {});
                    } catch (...) {
                        failure = std::current_exception();
                    }
                }
            } else if (available) {
                DWORD read = 0;

                if (!ReadFile(reader.value, buffer.data(), (std::min)(available, static_cast<DWORD>(buffer.size())),
                              &read, nullptr)) {
                    failure = std::make_exception_ptr(std::runtime_error("child pipe read failed"));
                } else if (!failure) {
                    try {
                        sink(static_cast<int>(index), std::string_view(buffer.data(), read));
                    } catch (...) {
                        failure = std::current_exception();
                    }
                }
            }
        }

        if (cleaning && rootExited) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};

            if (!QueryInformationJobObject(job.value, JobObjectBasicAccountingInformation, &accounting,
                                           sizeof(accounting), nullptr)) {
                failure = std::make_exception_ptr(std::runtime_error("owned job completion query failed"));
            } else if (!accounting.ActiveProcesses && (failure || (!output[0].value && !errors[0].value))) {
                break;
            }
        }

        if (cleaning && std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("owned cleanup exceeded five seconds");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (failure) {
        std::rethrow_exception(failure);
    }

    const int result = static_cast<int>(rootExit);

    if (complete) {
        complete(result);
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::dev
