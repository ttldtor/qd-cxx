// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "dns_resolver.hpp"

#include <ares_config.h>
#ifdef _WIN32
#    include <winsock2.h>
#else
#    include <cerrno>
#    include <fcntl.h>
#    include <poll.h>
#    include <sys/socket.h>
#    include <unistd.h>
#    if !defined(HAVE_ARC4RANDOM_BUF)
#        error The qualified Linux DNS profile requires the native arc4random_buf path
#    endif
#endif

#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <utility>

static_assert(ARES_QDCXX_PROFILE == 2);
static_assert(ARES_QDCXX_PROFILE_MAX_SERVERS == 4);

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

constexpr std::size_t MAX_DNS_LABEL = 63;      ///< Selected DNS label wire ceiling.
constexpr std::size_t MAX_SELECTED_NAME = 253; ///< Selected non-root ASCII presentation ceiling.
constexpr std::size_t NAME_BACKING = 1024;     ///< Pinned escaped SDK name backing, including its terminator.

/** Inspect one SDK-owned name only within its qualified finite backing.
 * @param text Borrowed NUL-terminated SDK name.
 * @return Nonowning complete view, or empty when null/unterminated.
 */
std::string_view boundedName(const char *text) noexcept {
    if (!text) {
        return {};
    }

    for (std::size_t i = 0; i < NAME_BACKING; ++i) {
        if (text[i] == '\0') {
            return {text, i};
        }
    }

    return {};
}

/** Compare selected ordinary DNS names case-insensitively without locale or escaped-name inference.
 * @param left Borrowed finite presentation.
 * @param right Borrowed finite presentation.
 * @return True for equal ASCII bytes except case and one optional root dot.
 */
bool sameName(std::string_view left, std::string_view right) noexcept {
    if (!left.empty() && left.back() == '.') {
        left.remove_suffix(1);
    }

    if (!right.empty() && right.back() == '.') {
        right.remove_suffix(1);
    }

    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto fold = [](unsigned char character) {
            return character >= 'A' && character <= 'Z' ? character + ('a' - 'A') : character;
        };

        if (fold(static_cast<unsigned char>(left[i])) != fold(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }

    return true;
}

/** Close a bridge-owned descriptor in the independent control allowance; never retry an interrupted close.
 * @param socket Descriptor, already removed from the bridge registry.
 * @return Native close result.
 */
int closeNative(ares_socket_t socket) noexcept {
#ifdef _WIN32

    return closesocket(socket);
#else

    return ::close(socket);
#endif
}

} // namespace

DnsResolver::~DnsResolver() {
    close();
}

std::uint64_t DnsResolver::progressUnits() noexcept {
    // One unit is one conservatively reserved byte/node/primitive visit, not one CPU instruction.
    constexpr std::uint64_t WIRE = MAX_DNS_WIRE;
    constexpr std::uint64_t RECORDS = WIRE / 11;
    constexpr std::uint64_t CONNECTIONS = 9;
    constexpr std::uint64_t BYTES = CONNECTIONS * (WIRE + 2) + READ_BYTES + 4;
    constexpr std::uint64_t FRAMES = BYTES / 2;
    constexpr std::uint64_t RECORD_VISITS = BYTES / 11;
    constexpr std::uint64_t NAMES = FRAMES + 3 * RECORD_VISITS;
    constexpr std::uint64_t LARGEST_ALLOCATION = 1024 * 1024;
    // Names, RR objects/auxiliary growth, TXT/payload segments and fixed parser structures.
    constexpr std::uint64_t PARSER_EVENTS = 8 * (1 + 3 * RECORDS) + 32 * RECORDS + 2 * WIRE + 32;
    constexpr std::uint64_t PARSER_ALLOCATIONS = FRAMES * PARSER_EVENTS;
    constexpr std::uint64_t CONTROL_ALLOCATIONS = 1024 * (CONNECTIONS + MAX_FDS + 4);
    constexpr std::uint64_t ALLOCATIONS = PARSER_ALLOCATIONS + CONTROL_ALLOCATIONS;
    static_assert(ALLOCATIONS <= std::numeric_limits<std::uint64_t>::max() / (2 * LARGEST_ALLOCATION));
    constexpr std::uint64_t ALLOCATION_BYTES = 2 * LARGEST_ALLOCATION * ALLOCATIONS;
    constexpr std::uint64_t ALLOCATION_VISITS = 2 * ALLOCATIONS;
    constexpr std::uint64_t PARSER_VISITS = FRAMES + 8 * RECORD_VISITS + 4 * BYTES + NAMES * (128 + 255 + 1);
    constexpr std::uint64_t OPTION_VISITS = (BYTES / 4) * (WIRE / 4);
    constexpr std::uint64_t LINEAR_BYTES = 8 * 1024 * NAMES + 128 * BYTES + 128 * RECORD_VISITS + 4096 * FRAMES;
    constexpr std::uint64_t CONNECTION_BYTES = 2 * (MAX_FDS + 2) * 524288;
    constexpr std::uint64_t CONTROL_VISITS = 1024 * (NATIVE_CALLS + MAX_FDS + CONNECTIONS + 4 + 1);
    // At most eighteen complete answer scans and sixteen bounded string passes per visited record.
    constexpr std::uint64_t COPIER_BYTES = 18 * RECORDS * 16 * 1024 + RECORDS * MAX_ADDRESSES * 16 + 17 * 17 * 4 * 256;
    constexpr std::uint64_t COPIER_VISITS = 18 * RECORDS + RECORDS * MAX_ADDRESSES + 17 * 17;
    constexpr std::uint64_t OTHER_UNITS = ALLOCATION_VISITS + PARSER_VISITS + OPTION_VISITS + LINEAR_BYTES +
                                          CONNECTION_BYTES + CONTROL_VISITS + COPIER_BYTES + COPIER_VISITS;
    static_assert(ALLOCATION_BYTES <= std::numeric_limits<std::uint64_t>::max() - OTHER_UNITS);
    constexpr auto TOTAL = ALLOCATION_BYTES + OTHER_UNITS;
    static_assert(TOTAL == 351280142190671838ULL);

    return TOTAL;
}

protocol::LiveWork DnsResolver::preparationWork(std::size_t servers) noexcept {
    constexpr std::size_t EVENTS_PER_SETUP_BUCKET = 1024;
    constexpr std::size_t LARGEST_ALLOCATION = 1024 * 1024;
    constexpr std::size_t CONFIGURATION_BYTES = 16 * 1024;
    const auto events = EVENTS_PER_SETUP_BUCKET * (servers + 1);

    return {0, 3 * events, 0, 2 * LARGEST_ALLOCATION * events + CONFIGURATION_BYTES};
}

bool DnsResolver::validName(std::string_view name) noexcept {
    if (!name.empty() && name.back() == '.') {
        name.remove_suffix(1);
    }

    if (name.empty() || name.size() > MAX_SELECTED_NAME) {
        return false;
    }

    std::size_t label = 0;
    bool hyphen = false;

    for (const unsigned char character : name) {
        if (character == '.') {
            if (label == 0 || hyphen) {
                return false;
            }

            label = 0;

            continue;
        }

        const bool alphanumeric = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                                  (character >= '0' && character <= '9');

        if ((!alphanumeric && character != '-') || (label == 0 && character == '-') || ++label > MAX_DNS_LABEL) {
            return false;
        }

        hyphen = character == '-';
    }

    return label != 0 && !hyphen;
}

void DnsResolver::wouldBlock() noexcept {
#ifdef _WIN32
    WSASetLastError(WSAEWOULDBLOCK);
#else
    errno = EWOULDBLOCK;
#endif
}

bool DnsResolver::nativeAttempt() noexcept {
    if (!work_ || work_->dnsCalls == 0 || nativeCalls_ == NATIVE_CALLS) {
        wouldBlock();

        return false;
    }

    --work_->dnsCalls;
    ++nativeCalls_;

    return true;
}

void DnsResolver::fail(TcpError error, std::string_view stage, int code) noexcept {
    if (!failure_) {
        failure_ = TcpDiagnostic{error, stage, code};
    }
}

ares_socket_t DnsResolver::openSocket(int domain, int type, int protocol, void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);

    if (state.socketCount_ == MAX_FDS || (type != SOCK_STREAM && type != SOCK_DGRAM)) {
        state.violation_ = true;
        wouldBlock();

        return ARES_SOCKET_BAD;
    }

    if (!state.nativeAttempt()) {
        return ARES_SOCKET_BAD;
    }

    const auto socket = ::socket(domain, type, protocol);

    if (socket == ARES_SOCKET_BAD) {
        return ARES_SOCKET_BAD;
    }

    if (!state.nativeAttempt()) {
        closeNative(socket);
        wouldBlock();

        return ARES_SOCKET_BAD;
    }

#ifdef _WIN32
    u_long enabled = 1;
    const int status = ioctlsocket(socket, FIONBIO, &enabled);
#else
    const int status = fcntl(socket, F_SETFL, O_NONBLOCK);
#endif

    if (status != 0) {
        closeNative(socket);

        return ARES_SOCKET_BAD;
    }

    state.sockets_[state.socketCount_++] = {socket, type == SOCK_STREAM, false, false, false};

    return socket;
}

int DnsResolver::closeSocket(ares_socket_t socket, void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);

    for (std::size_t i = 0; i < state.socketCount_; ++i) {
        if (state.sockets_[i].socket == socket) {
            state.sockets_[i] = state.sockets_[--state.socketCount_];
            const auto result = closeNative(socket);

            if (result != 0) {
#ifdef _WIN32
                state.fail(TcpError::CLOSE_FAILURE, "dns-socket-close", WSAGetLastError());
#else
                state.fail(TcpError::CLOSE_FAILURE, "dns-socket-close", errno);
#endif
            }

            return result;
        }
    }

    state.violation_ = true;
    wouldBlock();

    return -1;
}

int DnsResolver::setOption(ares_socket_t socket, ares_socket_opt_t option, const void *value, ares_socklen_t size,
                           void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);

    if (option == ARES_SOCKET_OPT_TCP_FASTOPEN) {
        // Decline the optional SDK optimization before a native call; ordinary TCP remains selected.
#ifdef _WIN32
        WSASetLastError(WSAEOPNOTSUPP);
#else
        errno = ENOSYS;
#endif

        return -1;
    }

    if (option != ARES_SOCKET_OPT_SENDBUF_SIZE && option != ARES_SOCKET_OPT_RECVBUF_SIZE) {
        state.violation_ = true;
        wouldBlock();

        return -1;
    }

    if (!state.nativeAttempt()) {
        return -1;
    }

    return setsockopt(socket, SOL_SOCKET, option == ARES_SOCKET_OPT_SENDBUF_SIZE ? SO_SNDBUF : SO_RCVBUF,
                      static_cast<const char *>(value), size);
}

int DnsResolver::connectSocket(ares_socket_t socket, const sockaddr *address, ares_socklen_t size, unsigned flags,
                               void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);

    if (flags != 0) {
        state.violation_ = true;
        wouldBlock();

        return -1;
    }

    if (!state.nativeAttempt()) {
        return -1;
    }

    return state.connectAttempt_ ? state.connectAttempt_(socket, address, size, state.connectContext_)
                                 : ::connect(socket, address, size);
}

ares_ssize_t DnsResolver::receive(ares_socket_t socket, void *buffer, std::size_t length, int flags, sockaddr *address,
                                  ares_socklen_t *addressLength, void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);
    const auto found =
        std::find_if(state.sockets_.begin(), state.sockets_.begin() + state.socketCount_, [socket](const Entry &entry) {
            return entry.socket == socket;
        });

    if (found == state.sockets_.begin() + state.socketCount_) {
        state.violation_ = true;
        wouldBlock();

        return -1;
    }

    if (!state.work_ || state.readCalls_ == READ_CALLS || state.readPackets_ == READ_CALLS ||
        state.work_->dnsPackets == 0 || state.readBytes_ == READ_BYTES ||
        (!found->tcp && (length < MAX_DNS_WIRE || state.work_->dnsBytes < MAX_DNS_WIRE ||
                         READ_BYTES - state.readBytes_ < MAX_DNS_WIRE))) {
        wouldBlock();

        return -1;
    }

    const auto capacity = (std::min)({length, state.work_->dnsBytes, READ_BYTES - state.readBytes_});

    if (capacity == 0 || !state.nativeAttempt()) {
        wouldBlock();

        return -1;
    }

    ++state.readCalls_;
    const auto count =
        recvfrom(socket, static_cast<char *>(buffer), static_cast<int>(capacity), flags, address, addressLength);

    if (count >= 0) {
        const auto bytes = static_cast<std::size_t>(count);
        --state.work_->dnsPackets;
        state.work_->dnsBytes -= bytes;
        state.readBytes_ += bytes;
        ++state.readPackets_;
    }

    return count;
}

ares_ssize_t DnsResolver::send(ares_socket_t socket, const void *buffer, std::size_t length, int flags,
                               const sockaddr *address, ares_socklen_t addressLength, void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);
    const auto found =
        std::find_if(state.sockets_.begin(), state.sockets_.begin() + state.socketCount_, [socket](const Entry &entry) {
            return entry.socket == socket;
        });

    if (found == state.sockets_.begin() + state.socketCount_) {
        state.violation_ = true;
        wouldBlock();

        return -1;
    }

    if (!state.work_ || length > state.work_->dnsBytes || length > DNS_SEND_BYTES || !state.nativeAttempt()) {
        found->udpWrite = !found->tcp;
        wouldBlock();

        return -1;
    }

    state.work_->dnsBytes -= length;
#ifndef _WIN32
    flags |= MSG_NOSIGNAL;
#endif

    const auto result = state.sendAttempt_ ? state.sendAttempt_(socket, buffer, length, flags, address, addressLength,
                                                                state.sendContext_)
                                           : sendto(socket, static_cast<const char *>(buffer), static_cast<int>(length),
                                                    flags, address, addressLength);

    if (!found->tcp) {
#ifdef _WIN32
        const auto error = result < 0 ? WSAGetLastError() : 0;
        found->udpWrite = result < 0 && error == WSAEWOULDBLOCK;
#else
        const auto error = result < 0 ? errno : 0;
        found->udpWrite = result < 0 && (error == EWOULDBLOCK || error == EAGAIN);
#endif
    }

    return result;
}

int DnsResolver::localAddress(ares_socket_t socket, sockaddr *address, ares_socklen_t *length,
                              void *argument) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);

    if (!state.nativeAttempt()) {
        return -1;
    }

    return getsockname(socket, address, length);
}

ares_socket_functions_ex DnsResolver::functions() const noexcept {
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

void DnsResolver::completed(void *argument, ares_status_t status, [[maybe_unused]] std::size_t timeouts,
                            const ares_dns_record_t *record) noexcept {
    auto &state = *static_cast<DnsResolver *>(argument);
    ++state.callbackDepth_;
    state.pending_ = false;

    if (!state.closing_) {
        state.completed_ = true;
        state.lastStatus_ = status;

        if (status == ARES_ENOMEM) {
            state.fail(TcpError::ALLOCATION_FAILURE, "dns-completion-storage", status);
        } else if (status == ARES_SUCCESS && record) {
            state.copyResult(record);
        }
    }

    --state.callbackDepth_;
}

void DnsResolver::copyResult(const ares_dns_record_t *record) noexcept {
    std::array<std::array<char, MAX_NAME + 1>, MAX_CNAME_DEPTH + 1> names{};
    const auto original = boundedName(name_.data());
    std::copy(original.begin(), original.end(), names[0].begin());
    std::string_view current{names[0].data(), original.size()};
    const auto count = ares_dns_record_rr_cnt(record, ARES_SECTION_ANSWER);
    std::size_t depth = 0;

    for (;;) {
        std::string_view target;

        for (std::size_t i = 0; i < count; ++i) {
            const auto *entry = ares_dns_record_rr_get_const(record, ARES_SECTION_ANSWER, i);

            if (ares_dns_rr_get_class(entry) != ARES_CLASS_IN || ares_dns_rr_get_type(entry) != ARES_REC_TYPE_CNAME ||
                !sameName(boundedName(ares_dns_rr_get_name(entry)), current)) {
                continue;
            }

            const auto candidate = boundedName(ares_dns_rr_get_str(entry, ARES_RR_CNAME_CNAME));

            if (!validName(candidate)) {
                fail(TcpError::UNSUPPORTED_DNS_NAME, "dns-cname-name");

                return;
            }

            if (!target.empty() && !sameName(target, candidate)) {
                fail(TcpError::DNS_FAILURE, "dns-cname-conflict");

                return;
            }

            target = candidate;
        }

        if (target.empty()) {
            break;
        }

        if (depth == depthLimit_) {
            fail(TcpError::RESOURCE_LIMIT, "dns-cname-depth");

            return;
        }

        for (std::size_t i = 0; i <= depth; ++i) {
            if (sameName(target, boundedName(names[i].data()))) {
                fail(TcpError::DNS_FAILURE, "dns-cname-loop");

                return;
            }
        }

        ++depth;
        std::copy(target.begin(), target.end(), names[depth].begin());
        current = {names[depth].data(), target.size()};
    }

    const auto type = family_ == 0 ? ARES_REC_TYPE_A : ARES_REC_TYPE_AAAA;

    for (std::size_t i = 0; i < count; ++i) {
        const auto *entry = ares_dns_record_rr_get_const(record, ARES_SECTION_ANSWER, i);

        if (ares_dns_rr_get_class(entry) != ARES_CLASS_IN || ares_dns_rr_get_type(entry) != type ||
            !sameName(boundedName(ares_dns_rr_get_name(entry)), current)) {
            continue;
        }

        ResolvedAddress candidate;
        candidate.ipv6 = family_ != 0;

        if (candidate.ipv6) {
            const auto *value = ares_dns_rr_get_addr6(entry, ARES_RR_AAAA_ADDR);

            if (!value) {
                fail(TcpError::DNS_FAILURE, "dns-address6");

                return;
            }

            std::memcpy(candidate.bytes.data(), value, 16);
        } else {
            const auto *value = ares_dns_rr_get_addr(entry, ARES_RR_A_ADDR);

            if (!value) {
                fail(TcpError::DNS_FAILURE, "dns-address4");

                return;
            }

            std::memcpy(candidate.bytes.data(), value, 4);
        }

        bool duplicate = false;

        for (std::size_t j = 0; j < addressCount_; ++j) {
            duplicate |= addresses_[j].ipv6 == candidate.ipv6 && addresses_[j].bytes == candidate.bytes;
        }

        if (duplicate) {
            continue;
        }

        if (addressCount_ == addressLimit_) {
            fail(TcpError::RESOURCE_LIMIT, "dns-address-capacity");

            return;
        }

        addresses_[addressCount_++] = candidate;
    }
}

std::expected<void, TcpDiagnostic> DnsResolver::initialize(const TcpOptions &options) {
    addressLimit_ = options.limits.addresses;
    depthLimit_ = options.limits.cnameDepth;
    auto name = options.endpoint;

    if (!name.empty() && name.back() == '.') {
        name.remove_suffix(1);
    }

    std::copy(name.begin(), name.end(), name_.begin());
    name_[name.size()] = '.';
    std::array<in_addr, 4> servers{};

    for (std::size_t i = 0; i < options.dnsServers.size(); ++i) {
        std::memcpy(&servers[i], options.dnsServers[i].address.data(), 4);
    }

    ares_options config{};
    char dnsOnly[] = "b";
    config.flags = ARES_FLAG_NOSEARCH | ARES_FLAG_NOALIASES | ARES_FLAG_NO_DFLT_SVR;
    config.lookups = dnsOnly;
    config.timeout = static_cast<int>(options.dnsTimeout.count());
    config.tries = 1;
    config.qcache_max_ttl = 0;
    config.servers = servers.data();
    config.nservers = static_cast<int>(options.dnsServers.size());
    config.udp_port = options.dnsPort;
    config.tcp_port = options.dnsPort;
    config.server_failover_opts.retry_chance = 0;
    const int mask = ARES_OPT_FLAGS | ARES_OPT_LOOKUPS | ARES_OPT_TIMEOUTMS | ARES_OPT_TRIES | ARES_OPT_QUERY_CACHE |
                     ARES_OPT_SERVERS | ARES_OPT_UDP_PORT | ARES_OPT_TCP_PORT | ARES_OPT_SERVER_FAILOVER;
    const auto status = ares_init_options(&channel_, &config, mask);

    if (status != ARES_SUCCESS) {
        return std::unexpected(TcpDiagnostic{
            status == ARES_ENOMEM ? TcpError::ALLOCATION_FAILURE : TcpError::DNS_FAILURE, "dns-channel", status});
    }

    const auto table = functions();
    const auto bridge = ares_set_socket_functions_ex(channel_, &table, this);

    if (bridge != ARES_SUCCESS) {
        return std::unexpected(TcpDiagnostic{TcpError::DNS_FAILURE, "dns-bridge", bridge});
    }

    return {};
}

TcpPollStatus DnsResolver::progress(TcpWork &work) noexcept {
    if (failure_ || !channel_) {
        return TcpPollStatus::IDLE;
    }

    if (family_ == 2) {
        return TcpPollStatus::IDLE;
    }

    const auto required = workRequirement();

    if (work.dnsCalls < required.dnsCalls || work.dnsPackets < required.dnsPackets ||
        work.dnsBytes < required.dnsBytes || work.dnsUnits < required.dnsUnits) {
        return TcpPollStatus::YIELD;
    }

    work.dnsUnits -= required.dnsUnits;
    work_ = &work;
    nativeCalls_ = 0;
    readCalls_ = readPackets_ = readBytes_ = 0;
    ares_status_t status = ARES_SUCCESS;

    if (completed_) {
        // A completed family is not reused until idle cleanup has reclaimed every prior descriptor.
        status = ares_process_fds(channel_, nullptr, 0, ARES_PROCESS_FLAG_NONE);

        if (ares_queue_active_queries(channel_) != 0 || socketCount_ != 0) {
            fail(TcpError::DNS_FAILURE, "dns-idle-boundary");
        } else {
            completed_ = false;
            ++family_;

            if (family_ == 2 && addressCount_ == 0) {
                fail(TcpError::DNS_FAILURE, "dns-no-eligible-address", lastStatus_);
            }
        }
    } else if (!pending_) {
        if (ares_queue_active_queries(channel_) != 0) {
            fail(TcpError::DNS_FAILURE, "dns-query-overlap");
        } else {
            pending_ = true;
            const auto type = family_ == 0 ? ARES_REC_TYPE_A : ARES_REC_TYPE_AAAA;
            status = ares_query_dnsrec(channel_, name_.data(), ARES_CLASS_IN, type, completed, this, nullptr);
        }
    } else {
        std::array<ares_socket_t, MAX_FDS> handles;
        handles.fill(ARES_SOCKET_BAD);
        // This pinned legacy snapshot avoids allocation-owning socket-state notification queues during close.
#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        const auto bitmap = static_cast<std::uint32_t>(ares_getsock(channel_, handles.data(), MAX_FDS));
#if defined(_MSC_VER)
#    pragma warning(pop)
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

        for (std::size_t i = 0; i < socketCount_; ++i) {
            sockets_[i].read = sockets_[i].write = false;
        }

        for (std::size_t i = 0; i < MAX_FDS; ++i) {
            const bool read = (bitmap & (std::uint32_t{1} << i)) != 0;
            const bool write = (bitmap & (std::uint32_t{1} << (i + MAX_FDS))) != 0;

            if (!read && !write) {
                continue;
            }

            const auto found = std::find_if(sockets_.begin(), sockets_.begin() + socketCount_, [&](const Entry &entry) {
                return entry.socket == handles[i];
            });

            if (found == sockets_.begin() + socketCount_) {
                violation_ = true;
            } else {
                found->read = read;
                found->write = write;
            }
        }

        for (std::size_t i = 0; i < socketCount_; ++i) {
            sockets_[i].write |= sockets_[i].udpWrite;
        }

        const auto snapshot = sockets_;
        const auto count = socketCount_;
        std::array<ares_fd_events_t, MAX_FDS> events{};
        std::size_t eventCount = 0;

        if (count != 0 && nativeAttempt()) {
#ifdef _WIN32
            fd_set readSet;
            fd_set writeSet;
            FD_ZERO(&readSet);
            FD_ZERO(&writeSet);

            for (std::size_t i = 0; i < count; ++i) {
                if (snapshot[i].read) {
                    FD_SET(snapshot[i].socket, &readSet);
                }

                if (snapshot[i].write) {
                    FD_SET(snapshot[i].socket, &writeSet);
                }
            }

            timeval zero{};
            const auto ready = select(0, &readSet, &writeSet, nullptr, &zero);

            if (ready > 0) {
                for (std::size_t i = 0; i < count; ++i) {
                    const unsigned flags = (FD_ISSET(snapshot[i].socket, &readSet) ? ARES_FD_EVENT_READ : 0) |
                                           (FD_ISSET(snapshot[i].socket, &writeSet) ? ARES_FD_EVENT_WRITE : 0);

                    if (flags != 0) {
                        events[eventCount++] = {snapshot[i].socket, flags};
                    }
                }
            }

#else
            std::array<pollfd, MAX_FDS> descriptors{};

            for (std::size_t i = 0; i < count; ++i) {
                descriptors[i] = {
                    snapshot[i].socket,
                    static_cast<short>((snapshot[i].read ? POLLIN : 0) | (snapshot[i].write ? POLLOUT : 0)), 0};
            }

            const auto ready = ::poll(descriptors.data(), count, 0);

            if (ready > 0) {
                for (std::size_t i = 0; i < count; ++i) {
                    const unsigned flags =
                        (descriptors[i].revents & (POLLIN | POLLERR | POLLHUP) ? ARES_FD_EVENT_READ : 0) |
                        (descriptors[i].revents & POLLOUT ? ARES_FD_EVENT_WRITE : 0);

                    if (flags != 0) {
                        events[eventCount++] = {snapshot[i].socket, flags};
                    }
                }
            }

#endif
        }

        status = ares_process_fds(channel_, events.data(), eventCount, ARES_PROCESS_FLAG_NONE);
    }

    work_ = nullptr;

    if (status != ARES_SUCCESS) {
        fail(status == ARES_ENOMEM ? TcpError::ALLOCATION_FAILURE : TcpError::DNS_FAILURE, "dns-progress", status);
    }

    if (violation_ || ares_queue_active_queries(channel_) > 1) {
        fail(TcpError::RESOURCE_LIMIT, "dns-bridge-invariant");
    }

    return TcpPollStatus::PROGRESS;
}

void DnsResolver::close() noexcept {
    if (!channel_) {
        return;
    }

    if (callbackDepth_ != 0) {
        std::terminate();
    }

    closing_ = true;
    ares_destroy(channel_);
    channel_ = nullptr;
    pending_ = false;
    completed_ = false;

    if (socketCount_ != 0) {
        std::terminate();
    }
}

TcpWork DnsResolver::workRequirement() const noexcept {
    return {1, 0, 0, NATIVE_CALLS, READ_CALLS, READ_BYTES + DNS_SEND_BYTES, progressUnits()};
}

bool DnsResolver::immediate() const noexcept {
    return completed_ || !pending_;
}

bool DnsResolver::finished() const noexcept {
    return family_ == 2;
}

bool DnsResolver::pending() const noexcept {
    return pending_;
}

std::span<const ResolvedAddress> DnsResolver::addresses() const noexcept {
    return {addresses_.data(), addressCount_};
}

std::optional<TcpDiagnostic> DnsResolver::failure() const noexcept {
    return failure_;
}

std::size_t DnsResolver::envelopeBytes() const noexcept {
    return channel_ ? ENVELOPE_BYTES : 0;
}

} // namespace org::ttldtor::qdcxx::transport::detail
