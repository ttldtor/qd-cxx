#pragma once

// Per-channel, single-owner native socket callbacks for the bounded DNS feasibility runner.
// The channel owns returned native handles and closes them through closeSocket; no handle is duplicated.
// beginTurn/finishTurn enclose one ares_process_fds call. No callback throws or calls c-ares/user code.
#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <fcntl.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif
#include <algorithm>
#include <ares.h>
#include <array>
#include <cerrno>
#include <climits>
#ifndef _WIN32
#    include <netinet/tcp.h>
#endif

namespace org::ttldtor::qdcxx::transport::test {

/** @brief Bounds native socket receives for one single-owner DNS processing turn; c-ares owns registered handles. */
struct ReceiveQuota {
    /** @brief Maximum DNS wire-message size in bytes. */
    static constexpr std::size_t MAX_DNS_WIRE_BYTES = 65535;
    /** @brief Aggregate byte limit in one turn. */
    static constexpr std::size_t TURN_BYTES = 2 * MAX_DNS_WIRE_BYTES;
    /** @brief Successful receive-call limit per turn. */
    static constexpr unsigned TURN_PACKETS = 2;
    /** @brief Native receive syscall-attempt limit per turn. */
    static constexpr unsigned TURN_NATIVE_READS = 2;
    /** @brief Maximum requested bytes in one TCP receive. */
    static constexpr std::size_t TCP_READ_CHUNK = 7;
    /** @brief Maximum tracked channel-owned socket count. */
    static constexpr std::size_t SOCKET_CAPACITY = 16;

    /** @brief Tracked native channel socket and transport kind. */
    struct Entry {
        /** @brief Native handle, or ARES_SOCKET_BAD for an empty entry. */
        ares_socket_t socket = ARES_SOCKET_BAD;
        /** @brief True for a stream socket; false for a datagram socket. */
        bool tcp = false;
    };

    /** @brief Fixed-capacity registry of channel-owned sockets. */
    std::array<Entry, SOCKET_CAPACITY> sockets{};
    /** @brief Number of live entries in sockets. */
    std::size_t socketCount = 0;
    /** @brief Bytes received in the current turn. */
    std::size_t bytes = 0;
    /** @brief Successful receive calls in the current turn. */
    unsigned packets = 0;
    /** @brief Native receive syscall attempts in the current turn. */
    unsigned calls = 0;
    /** @brief Quota-driven would-block returns in the current turn. */
    unsigned quotaReturns = 0;
    /** @brief Quota-driven would-block returns accumulated across turns. */
    unsigned totalQuotaReturns = 0;
    /** @brief Completed processing turns. */
    unsigned turns = 0;
    /** @brief Largest receive-attempt count observed in one turn. */
    unsigned maxCalls = 0;
    /** @brief Largest successful receive count observed in one turn. */
    unsigned maxPackets = 0;
    /** @brief Largest byte count observed in one turn. */
    std::size_t maxBytes = 0;
    /** @brief True after unsupported callback use or a receive-limit violation. */
    bool violated = false;

    /** @brief Resets counters scoped to one processing turn. */
    void beginTurn() noexcept {
        bytes = 0;
        packets = 0;
        calls = 0;
        quotaReturns = 0;
    }

    /** @brief Updates maxima and flags receive-limit violations. */
    void finishTurn() noexcept {
        ++turns;
        maxCalls = (std::max)(maxCalls, calls);
        maxPackets = (std::max)(maxPackets, packets);
        maxBytes = (std::max)(maxBytes, bytes);
        totalQuotaReturns += quotaReturns;
        violated |= calls > TURN_NATIVE_READS || packets > TURN_PACKETS || bytes > TURN_BYTES;
    }

    /** @brief Sets the platform error for a nonblocking would-block result. */
    static void wouldBlock() noexcept {
#ifdef _WIN32
        WSASetLastError(WSAEWOULDBLOCK);
#else
        errno = EWOULDBLOCK;
#endif
    }

    /** @brief Sets the platform error for an unsupported operation. */
    static void unsupported() noexcept {
#ifdef _WIN32
        WSASetLastError(WSAEOPNOTSUPP);
#else
        errno = ENOSYS;
#endif
    }

    /** @brief Closes a native handle. @param socket Handle. @return Platform close result; zero means success. */
    static int closeNative(ares_socket_t socket) noexcept {
#ifdef _WIN32

        return closesocket(socket);
#else

        return close(socket);
#endif
    }

    /** @brief Opens a nonblocking socket and tracks ownership. @param domain Address family. @param type Socket type;
     * SOCK_STREAM is tracked as TCP. @param protocol Native protocol. @param argument Owning ReceiveQuota callback
     * context. @return Handle, or ARES_SOCKET_BAD on capacity/native failure. */
    static ares_socket_t openSocket(int domain, int type, int protocol, void *argument) noexcept {
        auto &quota = *static_cast<ReceiveQuota *>(argument);

        if (quota.socketCount == SOCKET_CAPACITY) {
#ifdef _WIN32
            WSASetLastError(WSAEMFILE);
#else
            errno = EMFILE;
#endif

            return ARES_SOCKET_BAD;
        }

        const auto socket = ::socket(domain, type, protocol);

        if (socket == ARES_SOCKET_BAD) {
            return socket;
        }

#ifdef _WIN32
        u_long enabled = 1;
        const int status = ioctlsocket(socket, FIONBIO, &enabled);
#else
        const int status = fcntl(socket, F_SETFL, O_NONBLOCK);

        if (status == 0) {
            fcntl(socket, F_SETFD, FD_CLOEXEC);
        }

#endif

        if (status != 0) {
            closeNative(socket);

            return ARES_SOCKET_BAD;
        }

        if (type == SOCK_STREAM) {
            const int enabled = 1;
            setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
        }

        quota.sockets[quota.socketCount++] = {socket, type == SOCK_STREAM};

        return socket;
    }

    /** @brief Closes a registered handle. @param socket Handle. @param argument Owning quota context. @return Native
     * close result, or -1 if unowned. */
    static int closeSocket(ares_socket_t socket, void *argument) noexcept {
        auto &quota = *static_cast<ReceiveQuota *>(argument);

        for (std::size_t i = 0; i != quota.socketCount; ++i) {
            if (quota.sockets[i].socket == socket) {
                quota.sockets[i] = quota.sockets[--quota.socketCount];

                return closeNative(socket);
            }
        }

        quota.violated = true;
        unsupported();

        return -1;
    }

    /** @brief Applies supported socket buffer options. @param socket Handle. @param option c-ares option. @param value
     * Option value. @param size Value size in bytes. @param context Unused SDK callback context.
     * @return Native setsockopt result, or -1 if unsupported. */
    static int setOption(ares_socket_t socket, ares_socket_opt_t option, const void *value, ares_socklen_t size,
                         [[maybe_unused]] void *context) noexcept {
        if (option == ARES_SOCKET_OPT_SENDBUF_SIZE || option == ARES_SOCKET_OPT_RECVBUF_SIZE) {
            return setsockopt(socket, SOL_SOCKET, option == ARES_SOCKET_OPT_SENDBUF_SIZE ? SO_SNDBUF : SO_RCVBUF,
                              static_cast<const char *>(value), size);
        }

        unsupported();

        return -1;
    }

    /** @brief Connects with zero c-ares flags. @param socket Handle. @param address Peer address. @param size Address
     * length in bytes. @param flags Only zero is supported. @param context Unused SDK callback context.
     * @return Native connect result, or -1 for unsupported flags. */
    static int connectSocket(ares_socket_t socket, const sockaddr *address, ares_socklen_t size, unsigned flags,
                             [[maybe_unused]] void *context) noexcept {
        if (flags != 0) {
            unsupported();

            return -1;
        }

        return ::connect(socket, address, size);
    }

    /** @brief Receives bounded TCP chunks or complete maximum-sized UDP datagrams. @param socket Tracked handle. @param
     * buffer Destination storage. @param length Capacity in bytes. @param flags Native receive flags. @param address
     * Optional source address. @param addressLength Optional source-address length. @param argument Owning quota
     * context. @return Native result; exhausted quota returns -1 with would-block. */
    static ares_ssize_t receive(ares_socket_t socket, void *buffer, std::size_t length, int flags, sockaddr *address,
                                ares_socklen_t *addressLength, void *argument) noexcept {
        auto &quota = *static_cast<ReceiveQuota *>(argument);
        bool tcp = false;
        bool found = false;

        for (std::size_t i = 0; i != quota.socketCount; ++i) {
            if (quota.sockets[i].socket == socket) {
                tcp = quota.sockets[i].tcp;
                found = true;
                break;
            }
        }

        if (!found || quota.bytes > TURN_BYTES) {
            quota.violated = true;
            unsupported();

            return -1;
        }

        const auto remaining = TURN_BYTES - quota.bytes;

        if (quota.calls == TURN_NATIVE_READS || quota.packets == TURN_PACKETS || remaining == 0 ||
            (!tcp && remaining < MAX_DNS_WIRE_BYTES)) {
            ++quota.quotaReturns;
            wouldBlock();

            return -1;
        }

        // A UDP datagram is never shortened to fit the remaining quota. Decline before consuming it.
        if (!tcp && length < MAX_DNS_WIRE_BYTES) {
            quota.violated = true;
            unsupported();

            return -1;
        }

        const auto readBytes = tcp ? (std::min)({length, remaining, TCP_READ_CHUNK}) : MAX_DNS_WIRE_BYTES;
        ++quota.calls;
        const auto received =
            recvfrom(socket, static_cast<char *>(buffer), static_cast<int>(readBytes), flags, address, addressLength);

        if (received > 0) {
            quota.bytes += static_cast<std::size_t>(received);
            ++quota.packets;
        }

        return received;
    }

    /** @brief Sends bytes, clamping length to INT_MAX. @param socket Handle. @param buffer Source bytes. @param length
     * Requested byte count. @param flags Native flags. @param address Peer address. @param size Address length.
     * @param context Unused SDK callback context. @return Native send result. */
    static ares_ssize_t send(ares_socket_t socket, const void *buffer, std::size_t length, int flags,
                             const sockaddr *address, ares_socklen_t size, [[maybe_unused]] void *context) noexcept {
        return sendto(socket, static_cast<const char *>(buffer),
                      static_cast<int>((std::min)(length, std::size_t(INT_MAX))), flags, address, size);
    }

    /** @brief Gets the local address. @param socket Handle. @param address Output address. @param length Input capacity
     * and output length in bytes. @param context Unused SDK callback context. @return Native getsockname result. */
    static int localAddress(ares_socket_t socket, sockaddr *address, ares_socklen_t *length,
                            [[maybe_unused]] void *context) noexcept {
        return getsockname(socket, address, length);
    }

    /** @brief Builds the nonblocking c-ares callback table. @return Table to install with this object as callback
     * context. */
    ares_socket_functions_ex functions() const noexcept {
        ares_socket_functions_ex result{};
        result.version = 1;
        result.flags = ARES_SOCKFUNC_FLAG_NONBLOCKING;
        result.asocket = openSocket;
        result.aclose = closeSocket;
        result.asetsockopt = setOption;
        result.aconnect = connectSocket;
        result.arecvfrom = receive;
        result.asendto = send;
        result.agetsockname = localAddress;

        return result;
    }
};

} // namespace org::ttldtor::qdcxx::transport::test
