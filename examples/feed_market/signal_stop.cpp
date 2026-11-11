// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "signal_stop.hpp"

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

#ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <csignal>
#    include <pthread.h>
#endif

namespace feed_market {
namespace {
#ifdef _WIN32
SRWLOCK consoleLock = SRWLOCK_INIT; ///< Process-lifetime lock, alive even for an already-selected late handler.
HANDLE consoleEvent = nullptr;      ///< Borrowed only while consoleLock is held; cleared before native close.

/** Publish only an OS event from the console callback, without invoking application/SDK callbacks.
 * @param type Windows console control event.
 * @return True only for an installed bridge's Ctrl+C/Break; other events retain prior process behavior.
 */
BOOL WINAPI onConsoleControl(DWORD type) {
    if (type != CTRL_C_EVENT && type != CTRL_BREAK_EVENT) {
        return FALSE;
    }

    AcquireSRWLockShared(&consoleLock);
    const bool handled = consoleEvent != nullptr;

    if (handled) {
        SetEvent(consoleEvent);
    }

    ReleaseSRWLockShared(&consoleLock);

    return handled ? TRUE : FALSE;
}
#endif
} // namespace

/** Platform state belongs only to this standalone example, never to the SDK runtime. */
struct SignalStop::Impl {
    std::stop_source source;              ///< Shared stop state copied independently of the bridge facade.
    std::atomic<bool> interrupted{false}; ///< Real OS interruption, excluding the owned teardown wake.
    std::atomic<bool> failed{false};      ///< Native wait failure, distinguished from interruption.
    std::mutex mutex;                  ///< Protects stopping and the POSIX reader's exit-before-directed-wake interval.
    std::condition_variable condition; ///< Error/Windows reader waits for explicit teardown, without polling.
    bool stopping = false;             ///< Published under mutex before waking the reader.
    std::thread reader;                ///< Started last, joined before native resource/policy restoration.
#ifdef _WIN32
    HANDLE event = nullptr;  ///< Owned manual-reset event, retained until handler exclusion and reader join.
    bool published = false;  ///< Global borrowed event has been published under consoleLock.
    bool registered = false; ///< This handler must be removed before native close.
#else
    sigset_t signals{};                    ///< SIGINT/SIGTERM, blocked before any new thread can inherit a mask.
    sigset_t previousMask{};               ///< Exact creator mask restored after owned reader/SDK worker retirement.
    struct sigaction previousActions[3]{}; ///< Exact prior SIGINT, SIGTERM and SIGPIPE dispositions.
    unsigned installedActions = 0;         ///< Successfully installed prefix, including partial-construction cleanup.
    bool maskInstalled = false;            ///< Creator mask has been saved and changed.
    static constexpr int SIGNALS[3] = {SIGINT, SIGTERM, SIGPIPE}; ///< Scoped CLI signal policy, restored in reverse.
#endif

    /** Retain shared application cancellation, without starting a thread.
     * @param source Application-owned stop state.
     */
    explicit Impl(std::stop_source source) : source(std::move(source)) {
    }

    /** Install native state before publishing the reader; destruction rolls back a partial installation. */
    void install() {
#ifdef _WIN32
        event = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        if (!event) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "console-event");
        }

        AcquireSRWLockExclusive(&consoleLock);
        const bool occupied = consoleEvent != nullptr;

        if (!occupied) {
            consoleEvent = event;
            published = true;
        }

        ReleaseSRWLockExclusive(&consoleLock);

        if (occupied) {
            throw std::system_error(std::make_error_code(std::errc::device_or_resource_busy), "console-bridge");
        }

        if (!SetConsoleCtrlHandler(onConsoleControl, TRUE)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "console-handler");
        }

        registered = true;
#else
        sigemptyset(&signals);
        sigaddset(&signals, SIGINT);
        sigaddset(&signals, SIGTERM);
        const auto maskError = pthread_sigmask(SIG_BLOCK, &signals, &previousMask);

        if (maskError != 0) {
            throw std::system_error(maskError, std::generic_category(), "signal-mask");
        }

        maskInstalled = true;

        for (unsigned index = 0; index < 3; ++index) {
            struct sigaction action{};
            sigemptyset(&action.sa_mask);
            // A known nonignored SIGTERM also makes the thread-directed teardown wake portable.
            action.sa_handler = SIGNALS[index] == SIGPIPE ? SIG_IGN : SIG_DFL;

            if (sigaction(SIGNALS[index], &action, &previousActions[index]) != 0) {
                throw std::system_error(errno, std::generic_category(), "signal-disposition");
            }

            ++installedActions;
        }

#endif
        reader = std::thread([this] {
            run();
        });
    }

    /** Wait only on owned native notifications; request_stop never runs under a bridge lock. */
    void run() noexcept {
#ifdef _WIN32
        const auto outcome = WaitForSingleObject(event, INFINITE);
        {
            std::lock_guard lock(mutex);

            if (stopping) {
                return;
            }

            failed = outcome != WAIT_OBJECT_0;
            interrupted = outcome == WAIT_OBJECT_0;
        }

        source.request_stop();
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] {
            return stopping;
        });
#else

        for (;;) {
            int signal = 0;
            const auto error = sigwait(&signals, &signal);
            {
                std::lock_guard lock(mutex);

                if (stopping) {
                    return;
                }

                if (error != 0) {
                    failed = true;
                } else {
                    interrupted = true;
                }
            }

            source.request_stop();

            if (error != 0) {
                std::unique_lock lock(mutex);
                condition.wait(lock, [&] {
                    return stopping;
                });

                return;
            }
        }

#endif
    }

    /** Exclude native callbacks, wake/join the reader and restore every successfully installed policy. */
    ~Impl() {
#ifdef _WIN32

        if (registered && !SetConsoleCtrlHandler(onConsoleControl, FALSE)) {
            std::terminate();
        }

        if (published) {
            AcquireSRWLockExclusive(&consoleLock);
            consoleEvent = nullptr;
            ReleaseSRWLockExclusive(&consoleLock);
        }

#endif

        if (reader.joinable()) {
            {
                std::lock_guard lock(mutex);
                stopping = true;
#ifdef _WIN32

                if (!SetEvent(event)) {
                    std::terminate();
                }

#else

                // The reader can exit only after obtaining this mutex: its pthread_t is live through this send.
                if (pthread_kill(reader.native_handle(), SIGTERM) != 0) {
                    std::terminate();
                }

#endif
            }

            condition.notify_all();
            reader.join();
        }

#ifdef _WIN32

        if (event && !CloseHandle(event)) {
            std::terminate();
        }

#else

        while (installedActions != 0) {
            --installedActions;

            if (sigaction(SIGNALS[installedActions], &previousActions[installedActions], nullptr) != 0) {
                std::terminate();
            }
        }

        if (maskInstalled && pthread_sigmask(SIG_SETMASK, &previousMask, nullptr) != 0) {
            std::terminate();
        }

#endif
    }
};

SignalStop::SignalStop(std::stop_source source) : impl_(std::make_unique<Impl>(std::move(source))) {
    impl_->install();
}

SignalStop::~SignalStop() = default;

bool SignalStop::interrupted() const noexcept {
    return impl_->interrupted.load();
}

bool SignalStop::failed() const noexcept {
    return impl_->failed.load();
}
} // namespace feed_market
