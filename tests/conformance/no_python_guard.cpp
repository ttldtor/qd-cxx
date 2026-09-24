// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <cerrno>
#    include <csignal>
#    include <sys/ptrace.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace {
/** Decide whether an executable image is a Python interpreter or launcher.
 * @param path Resolved image path, including absolute invocations.
 * @return Whether execution must be stopped before the image runs.
 */
bool forbidden(const std::filesystem::path &path) {
    auto name = path.filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return std::regex_match(name, std::regex("(python([0-9].*)?|pythonw([0-9].*)?|py)(\\.exe)?"));
}

/** Write every resolved image and flush before allowing execution.
 * @param log Task-local execution inventory.
 * @param path Actual OS image path.
 * @return Whether the image is denied; write failures throw.
 */
bool record(std::ofstream &log, const std::filesystem::path &path) {
    const bool denied = forbidden(path);
    const auto utf8 = path.u8string();
    log << (denied ? "DENIED\t" : "EXEC\t") << std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size())
        << '\n'
        << std::flush;

    if (!log) {
        throw std::runtime_error("execution inventory write failed");
    }

    return denied;
}

#ifdef _WIN32
/** Quote one Windows CRT argument without losing empty strings or slashes.
 * @param value Original argument.
 * @return Exact encoded token.
 */
std::wstring quote(const std::wstring &value) {
    std::wstring result = L"\"";
    std::size_t slashes{};

    for (const auto unit : value) {
        if (unit == L'\\') {
            ++slashes;
        } else {
            result.append(unit == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
            result += unit;
            slashes = 0;
        }
    }

    result.append(slashes * 2, L'\\');

    return result + L'\"';
}

/** Inspect a debug-stopped process image through its OS file handle.
 * @param file Image file handle supplied by the debugger.
 * @return Fully resolved path; missing resolution fails closed.
 */
std::filesystem::path imagePath(HANDLE file) {
    std::wstring path(32768, L'\0');
    const auto length =
        GetFinalPathNameByHandleW(file, path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);

    if (length == 0 || length >= path.size()) {
        throw std::runtime_error("cannot resolve debug image");
    }

    path.resize(length);

    return path;
}
#endif

/** Run one command under a child-tree image guard, never changing host settings.
 * @param arguments Guard, inventory path, absolute command and command arguments.
 * @return Root exit code, 97 for a denied image, or 98 for guard failure.
 */
int run(const std::vector<std::filesystem::path> &arguments) {
    try {
        if (arguments.size() < 3) {
            throw std::runtime_error("usage: guard inventory command [arguments]");
        }

        std::ofstream log(arguments[1], std::ios::binary | std::ios::trunc);

        if (!log) {
            throw std::runtime_error("cannot open execution inventory");
        }

        bool denied = false;
        int rootCode = 98;
#ifdef _WIN32
        std::wstring command;

        for (std::size_t i = 2; i < arguments.size(); ++i) {
            if (i != 2) {
                command += L' ';
            }

            command += quote(arguments[i].native());
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION root{};

        if (!CreateProcessW(arguments[2].c_str(), command.data(), nullptr, nullptr, TRUE, DEBUG_PROCESS, nullptr,
                            nullptr, &startup, &root)) {
            throw std::runtime_error("guard launch failed");
        }

        CloseHandle(root.hThread);
        CloseHandle(root.hProcess);
        std::set<DWORD> live;
        bool rootSeen = false;
        bool rootExited = false;

        while (!rootSeen || !live.empty()) {
            DEBUG_EVENT event{};

            if (!WaitForDebugEvent(&event, INFINITE)) {
                throw std::runtime_error("debug wait failed");
            }

            DWORD disposition = DBG_CONTINUE;

            if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
                auto &info = event.u.CreateProcessInfo;
                live.insert(event.dwProcessId);
                rootSeen = true;
                try {
                    const bool forbiddenImage = record(log, imagePath(info.hFile));
                    denied |= forbiddenImage;

                    if (forbiddenImage || rootExited) {
                        if (!TerminateProcess(info.hProcess, forbiddenImage ? 97 : 0)) {
                            throw std::runtime_error("deny termination failed");
                        }
                    }
                } catch (...) {
                    TerminateProcess(info.hProcess, 98);

                    if (info.hFile) {
                        CloseHandle(info.hFile);
                    }

                    CloseHandle(info.hThread);
                    CloseHandle(info.hProcess);
                    throw;
                }

                if (info.hFile) {
                    CloseHandle(info.hFile);
                }

                CloseHandle(info.hThread);
                CloseHandle(info.hProcess);
            } else if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
                CloseHandle(event.u.CreateThread.hThread);
            } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
                if (event.u.LoadDll.hFile) {
                    CloseHandle(event.u.LoadDll.hFile);
                }
            } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
                if (event.dwProcessId == root.dwProcessId) {
                    rootCode = static_cast<int>(event.u.ExitProcess.dwExitCode);
                    rootExited = true;

                    // A completed command may leave compiler telemetry helpers alive. They are
                    // owned by this traced invocation and must not keep verification open.
                    for (const auto id : live) {
                        if (id == root.dwProcessId) {
                            continue;
                        }

                        const auto child =
                            OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id);

                        if (!child) {
                            throw std::runtime_error("owned child cleanup open failed");
                        }

                        DWORD code{};
                        const bool exited = GetExitCodeProcess(child, &code) && code != STILL_ACTIVE;
                        const bool stopped = exited || TerminateProcess(child, 0);
                        CloseHandle(child);

                        if (!stopped) {
                            throw std::runtime_error("owned child cleanup failed");
                        }
                    }
                }

                live.erase(event.dwProcessId);
            } else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
                if (event.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT) {
                    disposition = DBG_EXCEPTION_NOT_HANDLED;
                }
            }

            if (!ContinueDebugEvent(event.dwProcessId, event.dwThreadId, disposition)) {
                throw std::runtime_error("debug continuation failed");
            }
        }

#else
        std::vector<std::string> owned;

        for (std::size_t i = 2; i < arguments.size(); ++i) {
            owned.push_back(arguments[i].native());
        }

        std::vector<char *> argv;

        for (auto &argument : owned) {
            argv.push_back(argument.data());
        }

        argv.push_back(nullptr);
        const auto root = fork();

        if (root < 0) {
            throw std::runtime_error("guard fork failed");
        }

        if (root == 0) {
            if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
                _exit(98);
            }

            raise(SIGSTOP);
            execv(argv[0], argv.data());
            _exit(98);
        }

        int status{};

        if (waitpid(root, &status, 0) != root || !WIFSTOPPED(status)) {
            throw std::runtime_error("guard attach failed");
        }

        constexpr long OPTIONS =
            PTRACE_O_TRACEEXEC | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE | PTRACE_O_EXITKILL;

        if (ptrace(PTRACE_SETOPTIONS, root, nullptr, OPTIONS) < 0 || ptrace(PTRACE_CONT, root, nullptr, nullptr) < 0) {
            kill(root, SIGKILL);
            throw std::runtime_error("guard options failed");
        }

        for (;;) {
            const auto child = waitpid(-1, &status, __WALL);

            if (child < 0) {
                if (errno == EINTR) {
                    continue;
                }

                if (errno == ECHILD) {
                    break;
                }

                throw std::runtime_error("guard wait failed");
            }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                if (child == root) {
                    rootCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                }

                continue;
            }

            if (!WIFSTOPPED(status)) {
                continue;
            }

            const auto event = static_cast<unsigned>(status) >> 16;
            int signal = WSTOPSIG(status);

            if (event == PTRACE_EVENT_EXEC) {
                const auto path = std::filesystem::read_symlink("/proc/" + std::to_string(child) + "/exe");

                if (record(log, path)) {
                    denied = true;
                    kill(child, SIGKILL);
                }

                signal = 0;
            } else if (event != 0 || signal == SIGSTOP || signal == SIGTRAP) {
                signal = 0;
            }

            if (ptrace(PTRACE_CONT, child, nullptr, signal) < 0 && errno != ESRCH) {
                throw std::runtime_error("guard continuation failed");
            }
        }

#endif

        return denied ? 97 : rootCode;
    } catch (const std::exception &error) {
        std::cerr << "Python guard: " << error.what() << '\n';

        return 98;
    }
}
} // namespace

/** Preserve native command-line paths and dispatch the process-image guard.
 * @param argc Count including this executable.
 * @param argv Native command-line arguments borrowed for this call.
 * @return Child exit, 97 for a denied image, or 98 for a harness failure.
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
