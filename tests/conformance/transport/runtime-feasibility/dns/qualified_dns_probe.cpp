// Internal CTest runner: invoke with no arguments; no end-user command-line interface.
// Input: owned loopback UDP/TCP blackholes. Output: status/timing records; exit 0 on success, 1 on failure.
// No external resolver, network settings, worker threads or borrowed-socket ownership changes are used.
#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#    include <arpa/inet.h>
#    include <fcntl.h>
#    include <sys/select.h>
#    include <sys/socket.h>
#    include <unistd.h>
using Socket = int;
constexpr Socket INVALID_SOCKET_VALUE = -1;
#endif
#include "receive_quota.hpp"
#include <ares.h>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using org::ttldtor::qdcxx::transport::test::ReceiveQuota;

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr auto POLL_TICK = 5ms;
constexpr auto QUERY_ARRIVAL_TIMEOUT = 3s;
constexpr auto POST_CANCEL_OBSERVATION = 25ms;
constexpr int QUERY_TIMEOUT_MS = 30000;
constexpr unsigned CANCEL_TRIALS = 10;
constexpr std::size_t MAX_RESOLVER_FDS = 16;
constexpr std::size_t DNS_HEADER_BYTES = 12;
constexpr std::size_t TCP_LENGTH_BYTES = 2;
static_assert(ARES_QDCXX_PROFILE == 1);
static_assert(ARES_QDCXX_PROFILE_MAX_SERVERS == 4);

/** @brief Validates the explicit bounded-server configuration before creating a channel. @param channel Output channel
 * pointer, cleared on failure. @param options c-ares options containing the server list. @param mask Option mask, which
 * must include ARES_OPT_SERVERS. @return c-ares initialization status; invalid or absent servers return ARES_ENOSERVER.
 */
int initializeQualifiedChannel(ares_channel_t **channel, const ares_options *options, int mask) noexcept {
    if (!channel) {
        return ARES_EFORMERR;
    }

    *channel = nullptr;

    if (!options || !(mask & ARES_OPT_SERVERS) || !options->servers || options->nservers <= 0 ||
        options->nservers > ARES_QDCXX_PROFILE_MAX_SERVERS) {
        return ARES_ENOSERVER;
    }

    for (int i = 0; i != options->nservers; ++i) {
        if (options->servers[i].s_addr == 0) {
            return ARES_ENOSERVER;
        }
    }

    return ares_init_options(channel, options, mask);
}

/** @brief Fails the probe when a required condition is false. @param value Condition to check. @param message Failure
 * description used in the thrown runtime_error. */
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

/** @brief RAII owner for one native test socket; closes it on destruction. */
struct SocketOwner {
    Socket value = INVALID_SOCKET_VALUE;

    /** @brief Closes value when it contains a valid owned socket. */
    ~SocketOwner() {
        if (value != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(value);
#else
            close(value);
#endif
        }
    }
};

/** @brief Enables nonblocking mode on an owned test socket. @param socket Socket to configure. */
void nonblocking(Socket socket) {
#ifdef _WIN32
    u_long enabled = 1;
    require(ioctlsocket(socket, FIONBIO, &enabled) == 0, "nonblocking setup failed");
#else
    require(fcntl(socket, F_SETFL, O_NONBLOCK) == 0, "nonblocking setup failed");
#endif
}

/** @brief Performs a zero-timeout readability check. @param socket Socket to poll. @return True when the socket is
 * readable; throws on select failure. */
bool readable(Socket socket) {
    fd_set readSet;
    FD_ZERO(&readSet);
#ifndef _WIN32
    require(socket >= 0 && socket < FD_SETSIZE, "test socket exceeds select descriptor range");
#endif
    FD_SET(socket, &readSet);
    timeval zero{};
    const int ready = select(static_cast<int>(socket + 1), &readSet, nullptr, nullptr, &zero);
    require(ready >= 0, "test select failed");

    return ready != 0;
}

/** @brief Bounded owner-thread snapshot of descriptors and their requested readiness. */
struct FdSnapshot {
    struct Entry {
        ares_socket_t socket = ARES_SOCKET_BAD;
        /** @brief Whether read readiness was requested. */
        bool read = false;
        /** @brief Whether write readiness was requested. */
        bool write = false;
    };

    /** @brief Fixed-capacity descriptor registry. */
    std::array<Entry, MAX_RESOLVER_FDS> entries{};
    /** @brief Number of live descriptors. */
    std::size_t count = 0;
    /** @brief True when the descriptor registry capacity was exceeded. */
    bool overflow = false;
    /** @brief True when a socket-state callback ran off the recorded owner thread. */
    bool wrongThread = false;
    /** @brief Thread that owns this snapshot, captured during construction. */
    std::thread::id owner = std::this_thread::get_id();
};

/** @brief Updates the bounded descriptor snapshot from the c-ares socket-state callback. @param argument FdSnapshot
 * callback context. @param socket Changed descriptor. @param read Nonzero when read readiness is requested. @param
 * write Nonzero when write readiness is requested. */
void socketChanged(void *argument, ares_socket_t socket, int read, int write) {
    auto &snapshot = *static_cast<FdSnapshot *>(argument);
    snapshot.wrongThread |= std::this_thread::get_id() != snapshot.owner;

    std::size_t index = 0;

    while (index != snapshot.count && snapshot.entries[index].socket != socket) {
        ++index;
    }

    if (!read && !write) {
        if (index != snapshot.count) {
            snapshot.entries[index] = snapshot.entries[--snapshot.count];
        }

        return;
    }

    if (index == snapshot.count) {
        if (snapshot.count == MAX_RESOLVER_FDS) {
            snapshot.overflow = true;

            return;
        }

        ++snapshot.count;
    }

    snapshot.entries[index] = {socket, read != 0, write != 0};
}

/** @brief Polls the current descriptors without waiting and advances one quota-limited c-ares turn. @param channel
 * Channel to process. @param snapshot Owner-thread descriptor state. @param quota Per-channel receive accounting. */
void tick(ares_channel_t *channel, FdSnapshot &snapshot, ReceiveQuota &quota) {
    require(!snapshot.overflow && !snapshot.wrongThread, "fd callback capacity/thread violation");
    const auto copy = snapshot;
    fd_set readSet;
    fd_set writeSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    Socket maxSocket = 0;

    for (std::size_t i = 0; i != copy.count; ++i) {
        const auto &entry = copy.entries[i];
#ifndef _WIN32
        require(entry.socket >= 0 && entry.socket < FD_SETSIZE, "resolver fd exceeds select descriptor range");
#endif

        if (entry.read) {
            FD_SET(entry.socket, &readSet);
        }

        if (entry.write) {
            FD_SET(entry.socket, &writeSet);
        }

        if (entry.socket > maxSocket) {
            maxSocket = entry.socket;
        }
    }

    timeval zero{};
    const int ready =
        copy.count == 0 ? 0 : select(static_cast<int>(maxSocket + 1), &readSet, &writeSet, nullptr, &zero);
    require(ready >= 0, "resolver select failed");
    std::array<ares_fd_events_t, MAX_RESOLVER_FDS> events{};
    std::size_t count = 0;

    for (std::size_t i = 0; i != copy.count; ++i) {
        const auto socket = copy.entries[i].socket;
        const unsigned flags = (FD_ISSET(socket, &readSet) ? ARES_FD_EVENT_READ : 0) |
                               (FD_ISSET(socket, &writeSet) ? ARES_FD_EVENT_WRITE : 0);

        if (flags) {
            events[count++] = {socket, flags};
        }
    }

    quota.beginTurn();
    const auto status = ares_process_fds(channel, events.data(), count, ARES_PROCESS_FLAG_NONE);
    quota.finishTurn();
    require(status == ARES_SUCCESS && !quota.violated, "resolver processing/quota failed");
}

/** @brief Stores completion observations and the expected callback owner thread. */
struct CallbackState {
    unsigned count = 0;
    /** @brief Most recently observed c-ares completion status. */
    int status = -1;
    /** @brief Whether the completion ran on the recorded owner thread. */
    bool onOwner = false;
    /** @brief Whether the completion supplied an address list. */
    bool resultPresent = false;
    /** @brief Expected owner thread for completion callbacks, captured at construction. */
    std::thread::id owner = std::this_thread::get_id();
};

/** @brief Records a resolver completion and frees any returned address list. @param argument CallbackState context.
 * @param status c-ares completion status. @note The unnamed third callback parameter is the unused c-ares timeout
 * value. @param result Owned address list, freed before return when non-null. */
void completed(void *argument, int status, int, ares_addrinfo *result) {
    auto &state = *static_cast<CallbackState *>(argument);
    state.status = status;
    state.onOwner = std::this_thread::get_id() == state.owner;
    state.resultPresent = result != nullptr;
    ++state.count;

    if (result) {
        ares_freeaddrinfo(result);
    }
}

/** @brief RAII owner that destroys its c-ares channel during scope exit. */
struct ChannelOwner {
    ares_channel_t *value = nullptr;

    /** @brief Destroys value when a channel has been initialized. */
    ~ChannelOwner() {
        if (value) {
            ares_destroy(value);
        }
    }
};

/** @brief Runs one owned UDP or TCP DNS mechanism probe against a loopback peer. @param tcp Selects stream transport
 * when true, datagram otherwise. @param answer When true, the peer sends a response instead of remaining silent. */
void runProtocol(bool tcp, bool answer = false) {
    SocketOwner server;
    SocketOwner accepted;
    FdSnapshot snapshot;
    ReceiveQuota quota;
    // The callback state outlives channel cleanup, including exceptional exit.
    CallbackState state;
    ChannelOwner channel;
    server.value = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, tcp ? IPPROTO_TCP : IPPROTO_UDP);
    require(server.value != INVALID_SOCKET_VALUE, "server socket failed");
    nonblocking(server.value);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(server.value, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "server bind failed");

    if (tcp) {
        require(listen(server.value, 1) == 0, "server listen failed");
    }

#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    require(getsockname(server.value, reinterpret_cast<sockaddr *>(&address), &length) == 0, "server name failed");

    ares_options options{};
    char dnsOnly[] = "b";
    options.flags = ARES_FLAG_NOSEARCH | ARES_FLAG_NOALIASES | ARES_FLAG_NO_DFLT_SVR | (tcp ? ARES_FLAG_USEVC : 0);
    options.lookups = dnsOnly;
    options.timeout = QUERY_TIMEOUT_MS;
    options.tries = 1;
    options.qcache_max_ttl = 0;
    options.sock_state_cb = socketChanged;
    options.sock_state_cb_data = &snapshot;
    in_addr explicitServer = address.sin_addr;
    options.servers = &explicitServer;
    options.nservers = 1;
    options.udp_port = ntohs(address.sin_port);
    options.tcp_port = ntohs(address.sin_port);
    const int mask = ARES_OPT_FLAGS | ARES_OPT_LOOKUPS | ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES | ARES_OPT_QUERY_CACHE |
                     ARES_OPT_SOCK_STATE_CB | ARES_OPT_SERVERS | ARES_OPT_UDP_PORT | ARES_OPT_TCP_PORT;
    require(initializeQualifiedChannel(&channel.value, &options, mask) == ARES_SUCCESS,
            "channel initialization failed");
    require(ares_reinit(channel.value) == ARES_ENOTIMP, "qualified reinit unexpectedly supported");
    const auto functions = quota.functions();
    require(ares_set_socket_functions_ex(channel.value, &functions, &quota) == ARES_SUCCESS,
            "socket functions setup failed");

    const auto lastIteration = answer ? 0 : CANCEL_TRIALS;

    for (unsigned iteration = 0; iteration <= lastIteration; ++iteration) {
        state = CallbackState{};
        ares_addrinfo_hints hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = ARES_AI_NOSORT;
        require(ares_queue_active_queries(channel.value) == 0, "wire query overlap before submission");
        ares_getaddrinfo(channel.value, "hold.invalid.", nullptr, &hints, completed, &state);
        require(ares_queue_active_queries(channel.value) == 1, "profile did not submit exactly one wire query");

        std::array<char, 4096> packet{};
        std::size_t bytes = 0;
        sockaddr_in peer{};
        const auto arrivalDeadline = Clock::now() + QUERY_ARRIVAL_TIMEOUT;
        const std::size_t framing = tcp ? TCP_LENGTH_BYTES : 0;
        constexpr char EXPECTED_QUESTION[] = "\4hold\7invalid\0\0\1\0\1";
        constexpr auto QUESTION_BYTES = sizeof(EXPECTED_QUESTION) - 1;
        const auto expectedBytes = framing + DNS_HEADER_BYTES + QUESTION_BYTES;

        while (bytes < expectedBytes && Clock::now() < arrivalDeadline) {
            tick(channel.value, snapshot, quota);

            if (tcp && accepted.value == INVALID_SOCKET_VALUE && readable(server.value)) {
                accepted.value = accept(server.value, nullptr, nullptr);
                require(accepted.value != INVALID_SOCKET_VALUE, "accept failed");
                nonblocking(accepted.value);
            }

            const auto source = tcp ? accepted.value : server.value;

            if (source != INVALID_SOCKET_VALUE && readable(source)) {
                ares_socklen_t peerLength = sizeof(peer);
                const auto received =
                    tcp ? recv(source, packet.data() + bytes, static_cast<int>(packet.size() - bytes), 0)
                        : recvfrom(source, packet.data() + bytes, static_cast<int>(packet.size() - bytes), 0,
                                   reinterpret_cast<sockaddr *>(&peer), &peerLength);
                require(received > 0, "query receive failed or TCP closed prematurely");
                bytes += static_cast<std::size_t>(received);
            }

            std::this_thread::sleep_for(POLL_TICK);
        }

        require(bytes >= expectedBytes, "expected query did not arrive");
        require(std::memcmp(packet.data() + framing + DNS_HEADER_BYTES, EXPECTED_QUESTION, QUESTION_BYTES) == 0,
                "wrong query question");
        require(state.count == 0, "query was not pending");

        if (answer) {
            // A matching NXDOMAIN answer carries the original question and query ID.
            packet[framing + 2] = static_cast<char>(0x81);
            packet[framing + 3] = static_cast<char>(0x83);

            if (tcp) {
                constexpr int FIRST_FRAGMENT_BYTES = 3;
                require(send(accepted.value, packet.data(), FIRST_FRAGMENT_BYTES, 0) == FIRST_FRAGMENT_BYTES,
                        "first TCP response fragment failed");

                for (unsigned fragmentTick = 0; fragmentTick != 4; ++fragmentTick) {
                    tick(channel.value, snapshot, quota);
                    std::this_thread::sleep_for(POLL_TICK);
                }

                require(state.count == 0, "partial TCP response completed prematurely");
                const auto remaining = static_cast<int>(bytes) - FIRST_FRAGMENT_BYTES;
                require(send(accepted.value, packet.data() + FIRST_FRAGMENT_BYTES, remaining, 0) == remaining,
                        "remaining TCP response failed");
            } else {
                constexpr unsigned NONMATCHING_REPLIES = 6;
                const auto originalId = packet[0];
                packet[0] ^= static_cast<char>(0x80);

                for (unsigned reply = 0; reply != NONMATCHING_REPLIES; ++reply) {
                    require(sendto(server.value, packet.data(), static_cast<int>(bytes), 0,
                                   reinterpret_cast<sockaddr *>(&peer), sizeof(peer)) == static_cast<int>(bytes),
                            "nonmatching UDP response failed");
                }

                packet[0] = originalId;
                require(sendto(server.value, packet.data(), static_cast<int>(bytes), 0,
                               reinterpret_cast<sockaddr *>(&peer), sizeof(peer)) == static_cast<int>(bytes),
                        "matching UDP response failed");
            }

            const auto responseDeadline = Clock::now() + QUERY_ARRIVAL_TIMEOUT;

            while (state.count == 0 && Clock::now() < responseDeadline) {
                tick(channel.value, snapshot, quota);
                std::this_thread::sleep_for(POLL_TICK);
            }

            require(state.count == 1 && state.status == ARES_ENOTFOUND && state.onOwner && !state.resultPresent,
                    "NXDOMAIN completion failed");
            require(tcp || quota.totalQuotaReturns >= 3, "UDP flood did not exercise quota yield");
        }

        const auto cancelStart = Clock::now();
        const bool destroyPending = !answer && iteration == CANCEL_TRIALS;

        if (destroyPending) {
            ares_destroy(channel.value);
            channel.value = nullptr;
        } else if (!answer) {
            ares_cancel(channel.value);
        }

        const auto retired = Clock::now();
        require(channel.value == nullptr || ares_queue_active_queries(channel.value) == 0,
                "wire query retained after retirement");
        require(state.count == 1 && state.status == (answer           ? ARES_ENOTFOUND
                                                     : destroyPending ? ARES_EDESTRUCTION
                                                                      : ARES_ECANCELLED),
                "retirement callback count/status mismatch");
        require(state.onOwner && !state.resultPresent, "callback context/result mismatch");

        const auto observeUntil = Clock::now() + POST_CANCEL_OBSERVATION;

        while (Clock::now() < observeUntil) {
            if (channel.value) {
                tick(channel.value, snapshot, quota);
            }

            std::this_thread::sleep_for(POLL_TICK);
        }

        require(state.count == 1, "late callback after retirement");
        require(!snapshot.overflow && !snapshot.wrongThread, "socket callback violation");
        std::cout << "protocol=" << (tcp ? "tcp" : "udp") << " iteration=" << iteration << " mode="
                  << (answer           ? "answer_resume"
                      : destroyPending ? "destroy_pending"
                                       : "cancel_reuse")
                  << " pending_before=1"
                  << " query_bytes=" << bytes << " callbacks=" << state.count << " status=" << state.status
                  << " callback_on_owner=" << state.onOwner << " retire_us="
                  << std::chrono::duration_cast<std::chrono::microseconds>(retired - cancelStart).count()
                  << " fd_count=" << snapshot.count << " late_callbacks=0"
                  << " max_native_reads=" << quota.maxCalls << " max_read_bytes=" << quota.maxBytes
                  << " max_read_packets=" << quota.maxPackets << " quota_yields=" << quota.totalQuotaReturns
                  << " turns=" << quota.turns << '\n';

        // c-ares owns only its endpoint; this accepted peer endpoint belongs to the test.
        if (tcp && accepted.value != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(accepted.value);
#else
            close(accepted.value);
#endif
            accepted.value = INVALID_SOCKET_VALUE;
        }
    }

    if (channel.value) {
        ares_destroy(channel.value);
        channel.value = nullptr;
    }

    require(snapshot.count == 0, "destroy did not retire every borrowed fd");
    require(quota.socketCount == 0 && !quota.violated, "native socket ownership/quota violation");
}

/** @brief Verifies rejected server configurations do not create a c-ares channel. */
void rejectInvalidConfiguration() {
    std::array<in_addr, ARES_QDCXX_PROFILE_MAX_SERVERS + 1> servers{};

    for (auto &server : servers) {
        server.s_addr = htonl(INADDR_LOOPBACK);
    }

    ares_options options{};
    ares_channel_t *channel = nullptr;
    require(initializeQualifiedChannel(&channel, nullptr, 0) == ARES_ENOSERVER && channel == nullptr,
            "wrapper accepted missing server options");
    require(ares_init_options(&channel, nullptr, 0) == ARES_ENOSERVER && channel == nullptr,
            "SDK accepted missing server options");
    options.servers = servers.data();
    options.nservers = 0;
    require(initializeQualifiedChannel(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "wrapper accepted empty server list");
    require(ares_init_options(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "SDK accepted empty server list");
    options.nservers = static_cast<int>(servers.size());
    require(initializeQualifiedChannel(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "wrapper accepted excessive server list");
    require(ares_init_options(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "SDK accepted excessive server list");
    servers[0].s_addr = 0;
    options.nservers = 1;
    require(initializeQualifiedChannel(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "wrapper accepted unspecified IPv4 server");
    require(ares_init_options(&channel, &options, ARES_OPT_SERVERS) == ARES_ENOSERVER && channel == nullptr,
            "SDK accepted unspecified IPv4 server");
    std::cout << "configuration_rejections=8 no_channel_created=1\n";
}

} // namespace

/** @brief Initializes platform/SDK state and runs the bounded DNS feasibility scenarios. @return Zero on success; one
 * when an exception reports a failed requirement. */
int main() {
#ifdef _WIN32
    WSADATA winsock{};

    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        return 2;
    }

#endif

    if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) {
        return 2;
    }

    int result = 0;
    try {
        require(ares_threadsafety() == ARES_FALSE, "proof requires threads disabled");
        require(std::string(ares_version(nullptr)) == "1.34.9+qdcxx1", "wrong qualified runtime version");
        rejectInvalidConfiguration();
        std::cout << "version=" << ares_version(nullptr)
                  << " threads=0 event_thread=0 tick_ms=5 fd_capacity=" << MAX_RESOLVER_FDS << '\n';
        runProtocol(false);
        runProtocol(true);
        runProtocol(false, true);
        runProtocol(true, true);
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        result = 1;
    }
    ares_library_cleanup();
#ifdef _WIN32
    WSACleanup();
#endif

    return result;
}
