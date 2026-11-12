// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: argv[1] is the built demo, argv[2] is owned capture storage.
// It starts only owned children/loopback peers; every child is bounded and retired on failure.
#include "qd/api/feed.hpp"
#include "signal_stop.hpp"

#include "connect_peer.hpp"
#include "connect_process_support.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace process = q::connect_tests::process;
using process::require;
using namespace std::chrono_literals;

/** Run a bridge without a signal or cancel a checked SDK wait with a real owned-process signal.
 * @param signal Zero for no-interrupt teardown; otherwise the POSIX target signal (ignored on Windows).
 */
void checkBridge(int signal) {
    std::stop_source stop;
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    {
        feed_quote::SignalStop bridge(stop);

        if (signal != 0) {
#ifdef _WIN32
            // This mode is launched only in our own exclusive hidden console/process group.
            require(GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, GetCurrentProcessId()), "owned console Ctrl+Break");
#else
            require(kill(getpid(), signal) == 0, "owned process signal");
#endif
            const auto result = endpoint->awaitState(q::api::EndpointState::CONNECTED,
                                                     std::chrono::steady_clock::now() + 2s, stop.get_token());
            require(result.status == q::api::WaitStatus::CANCELLED, "signal cancelled SDK state wait");
            require(bridge.interrupted() && !bridge.failed(), "real interruption flag");
        } else {
            require(!stop.stop_requested() && !bridge.interrupted() && !bridge.failed(), "no synthetic interruption");
        }

        require(endpoint->closeAndAwaitTermination().drained, "bridge inactive retirement");
        runtime.closeAndAwaitTermination();
    }

    require(stop.stop_requested() == (signal != 0), "teardown did not request cancellation");
}

#ifdef _WIN32
/** Exercise Ctrl+Break only in an owned hidden console; never broadcast to an inherited user console.
 * @param executable Exact native path of this internal runner.
 */
void checkOwnedConsole(const std::filesystem::path &executable) {
    std::wstring command = process::quote(executable.wstring()) + L" --console-child";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION information{};
    require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &startup, &information),
            "owned hidden console child");
    process::Handle child(information.hProcess);
    process::Handle thread(information.hThread);
    const auto waited = WaitForSingleObject(child.get(), 5000);

    if (waited != WAIT_OBJECT_0) {
        TerminateProcess(child.get(), 124);
        WaitForSingleObject(child.get(), 5000);
    }

    require(waited == WAIT_OBJECT_0, "hidden console child deadline");
    DWORD exit = 0;
    require(GetExitCodeProcess(child.get(), &exit) && exit == 0, "hidden console child interruption result");
}
#else
/** Retain this test process's original policy even if a real assertion fails. */
struct SavedSignals {
    struct sigaction actions[3]{};                                ///< Complete original test-process actions.
    sigset_t mask{};                                              ///< Original test-thread mask.
    static constexpr int SIGNALS[3] = {SIGINT, SIGTERM, SIGPIPE}; ///< Exactly this bridge's scoped dispositions.

    /** Save before installing the inherited-ignore regression's explicit policy. */
    SavedSignals() {
        require(pthread_sigmask(SIG_SETMASK, nullptr, &mask) == 0, "save test mask");

        for (unsigned index = 0; index < 3; ++index) {
            require(sigaction(SIGNALS[index], nullptr, &actions[index]) == 0, "save test action");
        }
    }

    /** Restore only this test process's policy, after all owned readers and SDK workers have retired. */
    ~SavedSignals() {
        for (unsigned index = 0; index < 3; ++index) {
            sigaction(SIGNALS[index], &actions[index], nullptr);
        }

        pthread_sigmask(SIG_SETMASK, &mask, nullptr);
    }
};

/** Verify ignored inherited targets cannot break teardown and complete prior policy/mask is restored. */
void checkInheritedIgnored() {
    SavedSignals saved;
    struct sigaction ignored{};
    ignored.sa_handler = SIG_IGN;
    ignored.sa_flags = SA_RESTART;
    sigemptyset(&ignored.sa_mask);
    sigaddset(&ignored.sa_mask, SIGUSR2);

    for (const auto signal : SavedSignals::SIGNALS) {
        require(sigaction(signal, &ignored, nullptr) == 0, "install inherited ignore");
    }

    sigset_t before{};
    require(pthread_sigmask(SIG_SETMASK, nullptr, &before) == 0, "query inherited mask");
    struct sigaction actions[3]{};

    for (unsigned index = 0; index < 3; ++index) {
        require(sigaction(SavedSignals::SIGNALS[index], nullptr, &actions[index]) == 0, "query inherited action");
    }

    checkBridge(0);
    checkBridge(SIGINT);
    checkBridge(SIGTERM);
    sigset_t after{};
    require(pthread_sigmask(SIG_SETMASK, nullptr, &after) == 0, "query restored mask");

    for (int signal = 1; signal < NSIG; ++signal) {
        require(sigismember(&before, signal) == sigismember(&after, signal), "complete mask restoration");
    }

    for (unsigned index = 0; index < 3; ++index) {
        struct sigaction restored{};
        require(sigaction(SavedSignals::SIGNALS[index], nullptr, &restored) == 0, "query restored action");
        require(restored.sa_handler == actions[index].sa_handler && restored.sa_flags == actions[index].sa_flags,
                "complete action restoration");

        for (int signal = 1; signal < NSIG; ++signal) {
            require(sigismember(&restored.sa_mask, signal) == sigismember(&actions[index].sa_mask, signal),
                    "complete action mask restoration");
        }
    }
}
#endif

/** Receive an actual full standard Quote through the standalone example's new deadline wait.
 * @param demo Exact built/installed sample executable.
 * @param captures Test-owned output directory.
 */
void checkSample(const std::filesystem::path &demo, const std::filesystem::path &captures) {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    constexpr q::core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto builtin = q::events::defaultQuoteSchema(LIMITS);
    require(static_cast<bool>(builtin), "full Quote schema");
    auto schema = q::core::LocalSchema::create(builtin->records(), LIMITS, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(static_cast<bool>(schema), "full Quote binary schema");
    std::vector<q::core::FieldValue> fields;

    for (const auto &field : schema->records()[0].fields) {
        const auto type = static_cast<q::core::SerialFieldType>(field.serialTypeId);
        fields.push_back({type, type == q::core::SerialFieldType::UTF_CHAR ? u'Q' : 0});
    }

    const std::vector<q::connect_tests::Record> records{{u"IBM", std::move(fields)}};
    const std::vector<std::u16string> symbols{u"IBM"};
    q::connect_tests::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.canonicalSymbols = true;
    q::connect_tests::Peer peer(*schema, records, symbols, 8192, q::connect_tests::PeerMode::HOLD, script);
    std::filesystem::create_directories(captures);
    const auto output = captures / "sample.stdout";
    const auto error = captures / "sample.stderr";
    constexpr std::chrono::milliseconds SAMPLE_ADMISSION_DURATION = 2s;
    process::Child child(demo,
                         {"--run", "--address", "127.0.0.1:" + std::to_string(peer.port()), "--symbol", "IBM",
                          "--duration-ms", std::to_string(SAMPLE_ADMISSION_DURATION.count())},
                         output, error);
    const auto status = child.wait();

    if (status != 0) {
        constexpr std::size_t MAXIMUM_STDERR_BYTES = 2048;
        std::array<char, MAXIMUM_STDERR_BYTES> prefix;
        std::ifstream capturedError(error, std::ios::binary);
        capturedError.read(prefix.data(), static_cast<std::streamsize>(prefix.size()));

        std::cerr << "sample exit=" << status << " peerAdds=" << peer.adds()
                  << " peerDataSent=" << peer.initialDataSent() << " peerFailed=" << peer.failed()
                  << "\nsample stderr prefix:\n";
        std::cerr.write(prefix.data(), capturedError.gcount());
        std::cerr << '\n';
    }

    require(status == 0, "sample actual Quote and deadline drain");
    require(process::read(output).find("IBM bid=") != std::string::npos, "sample actual Quote output");
    require(peer.adds() == 1 && !peer.failed(), "sample actual wire ADD");
}
} // namespace

/** Run finite bridge/current-wait integration cases or the exclusive Windows console child.
 * @param argc Three for the parent: executable and capture directory; two for the owned console child.
 * @param argv Explicit test harness inputs, borrowed through return.
 * @return Zero for all real observations, one on a bounded failure, two for invalid internal invocation.
 */
int main(int argc, char **argv) {
    try {
#ifdef _WIN32

        if (argc == 2 && std::string_view(argv[1]) == "--console-child") {
            checkBridge(1);

            return 0;
        }

#endif

        if (argc != 3) {
            return 2;
        }

        checkBridge(0);
#ifdef _WIN32
        checkOwnedConsole(std::filesystem::absolute(argv[0]));
#else
        checkBridge(SIGINT);
        checkBridge(SIGTERM);
        checkInheritedIgnored();
#endif
        checkSample(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]));
        std::cout << "Scoped signal teardown, owned interruption and sample Quote deadline: PASS\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
