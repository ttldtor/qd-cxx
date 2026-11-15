// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "fixture_peer.hpp"

#include <qd/api/feed.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <span>
#include <string_view>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
constexpr auto START = std::chrono::sys_time{std::chrono::milliseconds{1707748205000}};
constexpr auto WAIT = 10s;

/** Show help without creating the SDK or a socket.
 * @return Zero on successful output, otherwise one.
 */
int help() {
    std::cout << "Usage: qdcxx-on-demand --run\n"
                 "Replay synthetic MOCK quotes from a local OnDemand fixture with pause and resume.\n"
                 "--help prints help without starting the SDK or networking.\n"
                 "Creates an IPv4 loopback fixture server and SDK worker; all data is synthetic.\n"
                 "No account is required; the wait is limited to 10 seconds.\n"
                 "Exit codes: 0 — two quotes and clean retirement; 1 — failure; 2 — invalid arguments.\n";

    std::cout.flush();

    return std::cout ? 0 : 1;
}
} // namespace

/** Run a finite public API replay using a test-owned local fixture.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code documented by help.
 */
int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        return help();
    }

    if (argc != 2 || std::string_view(argv[1]) != "--run") {
        std::cerr << "Invalid arguments\n";

        return 2;
    }

    try {
        const auto peer = startFixturePeer();

        if (!peer->port()) {
            std::cerr << "Fixture listener failed\n";

            return 1;
        }

        std::atomic<unsigned> quotes{0};
        std::atomic<bool> resumed{false};
        std::atomic<bool> callbackFailed{false};
        std::mutex output;
        q::api::ApiRuntime runtime;
        const auto endpoint = runtime.createEndpoint();
        const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("MOCK");
        subscription->addEventListener([&](std::span<const q::events::Quote> batch) {
            for (const auto &quote : batch) {
                if (quote.symbol != u"MOCK") {
                    callbackFailed = true;
                    continue;
                }

                std::lock_guard lock(output);
                std::cout << "MOCK bid=" << quote.getBidPrice() << " ask=" << quote.getAskPrice()
                          << " eventTime=" << quote.eventTime << '\n';
                ++quotes;
            }
        });

        // Starting at zero pauses the clock; the covered callback resumes it without polling.
        const std::weak_ptr<q::api::Endpoint> weakEndpoint = endpoint;
        endpoint->addOnDemandPlaybackListener([&, weakEndpoint](const q::api::OnDemandPlaybackChange &change) {
            if (change.current.covered && change.current.speed == 0 && !resumed.exchange(true)) {
                try {
                    if (const auto owner = weakEndpoint.lock()) {
                        owner->setOnDemandSpeed(2);
                    } else {
                        callbackFailed = true;
                    }
                } catch (...) {
                    callbackFailed = true;
                }
            }
        });
        endpoint->connectOnDemand(peer->port(), u"MOCK", START, 0);
        const auto completion =
            endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + WAIT);
        const auto closed = endpoint->closeAndAwaitTermination();
        const auto fixture = peer->snapshot();
        runtime.closeAndAwaitTermination();

        if (completion.status != q::api::WaitStatus::REACHED || !closed.drained || closed.deadlineExpired ||
            closed.diagnostic.code != q::api::ErrorCode::NONE || callbackFailed || !resumed || quotes != 2 ||
            fixture.sessions.size() != 1 || !fixture.sessions.front().matched) {
            std::cerr << "Replay failed: " << completion.diagnostic.stage << '\n';

            return 1;
        }

        return std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "Replay failed: " << error.what() << '\n';

        return 1;
    }
}
