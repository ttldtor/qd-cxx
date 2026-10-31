// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/transport/tcp_runtime.hpp"

#include "deadline.hpp"
#include "dns_resolver.hpp"
#include "fixed_arena.hpp"
#include "runtime_bootstrap.hpp"

#include <asio/bind_allocator.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/version.hpp>

#ifndef _WIN32
#    include <cerrno>
#    include <poll.h>
#    include <sys/socket.h>
#endif

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <thread>
#include <utility>

static_assert(ASIO_VERSION == 103802);
#ifdef _WIN32
#    if !defined(_M_X64)
#        error The qualified Windows runtime requires an x64 target
#    endif
#    if !defined(ASIO_HAS_IOCP)
#        error The qualified Windows runtime requires IOCP
#    endif
#else
#    if !defined(__x86_64__)
#        error The qualified Linux runtime requires an x86-64 target
#    endif
#    if !defined(ASIO_HAS_EPOLL) || defined(ASIO_HAS_IO_URING_AS_DEFAULT)
#        error The qualified Linux runtime requires epoll
#    endif
#endif

namespace org::ttldtor::qdcxx::transport {
namespace {

using Clock = std::chrono::steady_clock;
using Socket = asio::ip::tcp::socket;
constexpr std::size_t MAX_ADDRESSES = 64;   ///< Finite selected DNS result and serial connection-attempt capacity.
constexpr std::size_t MAX_CNAME_DEPTH = 16; ///< Finite selected in-response alias depth.
constexpr std::size_t MAX_HOST_BYTES = 253; ///< Maximum selected ASCII DNS presentation name without final root dot.

/** Construct static numeric evidence without retaining caller or peer text.
 * @param error Typed category.
 * @param stage Static library-owned checkpoint.
 * @param nativeCode Optional native error.
 * @return Detached diagnostic value.
 */
TcpDiagnostic diagnostic(TcpError error, std::string_view stage, int nativeCode = 0) noexcept {
    return {error, stage, nativeCode};
}

/** Compare detached numeric/static causes so repeated retirement processing does not duplicate one failure.
 * @param left Earlier observed cause.
 * @param right Later observation.
 * @return True for the same complete diagnostic value.
 */
bool sameDiagnostic(const TcpDiagnostic &left, const TcpDiagnostic &right) noexcept {
    return left.error == right.error && left.stage == right.stage && left.nativeCode == right.nativeCode &&
           left.protocolError == right.protocolError;
}

/** Translate a nonterminal pure composition failure without changing transport state.
 * @param error Pure-session rejection.
 * @return Detached typed transport evidence.
 */
TcpDiagnostic protocolDiagnostic(protocol::LiveError error) noexcept {
    const auto category = error == protocol::LiveError::ALLOCATION_FAILURE ? TcpError::ALLOCATION_FAILURE
                          : error == protocol::LiveError::WORK_LIMIT       ? TcpError::WORK_LIMIT
                          : error == protocol::LiveError::RESOURCE_LIMIT   ? TcpError::RESOURCE_LIMIT
                                                                           : TcpError::PROTOCOL_FAILURE;

    return {category, "session-output", 0, error};
}

/** Validate positive millisecond durations before conversion or monotonic time arithmetic.
 * @param duration Explicit setting.
 * @return True when conversion and addition to the current monotonic time are representable.
 */
bool validDuration(std::chrono::milliseconds duration) noexcept {
    if (duration.count() <= 0 || duration.count() > INT_MAX) {
        return false;
    }

    return detail::deadlineAfter(Clock::now(), duration).has_value();
}

/** Validate the bounded endpoint presentation and optional per-socket setting before copying.
 * @param options Borrowed factory settings.
 * @return True when fixed endpoint/server storage and native option integers are sufficient.
 */
bool validEndpointSettings(const TcpOptions &options) noexcept {
    constexpr std::string_view FORBIDDEN_BYTES{"\0%/@[]", 6};

    return options.port != 0 && !options.endpoint.empty() && options.endpoint.size() <= MAX_HOST_BYTES + 1 &&
           options.endpoint.find_first_of(FORBIDDEN_BYTES) == std::string_view::npos &&
           options.dnsServers.size() <= 4 && options.socketSendBufferBytes <= INT_MAX;
}

/** Validate finite socket/arena capacities without allocating or examining a DNS server.
 * @param limits Explicit requested-capacity ceilings.
 * @return True for the selected representable bounded profile.
 */
bool validSocketLimits(const TcpLimits &limits) noexcept {
    return limits.inputBytes != 0 && limits.inputBytes <= INT_MAX && limits.writeBytes != 0 &&
           limits.writeBytes <= INT_MAX && limits.addresses != 0 && limits.addresses <= MAX_ADDRESSES &&
           limits.cnameDepth != 0 && limits.cnameDepth <= MAX_CNAME_DEPTH && limits.contextBytes != 0 &&
           limits.handlerBytes != 0;
}

/** Validate every independent monotonic deadline/cadence before conversion or activation.
 * @param options Borrowed factory settings.
 * @return True when all configured timing values are positive and representable.
 */
bool validTimingSettings(const TcpOptions &options) noexcept {
    return validDuration(options.dnsTimeout) && validDuration(options.connectTimeout) &&
           validDuration(options.readTimeout) && validDuration(options.writeTimeout) &&
           validDuration(options.pollInterval);
}

/** Checked addition for requested backing reservations.
 * @param total Accumulated capacity, modified only when representable.
 * @param amount Additional capacity.
 * @return True when the result fits size_t.
 */
bool addBytes(std::size_t &total, std::size_t amount) noexcept {
    if (amount > std::numeric_limits<std::size_t>::max() - total) {
        return false;
    }

    total += amount;

    return true;
}

/** Inspect the last native socket error without constructing dynamic diagnostic text.
 * @return Native error ordinal.
 */
int socketError() noexcept {
#ifdef _WIN32

    return WSAGetLastError();
#else

    return errno;
#endif
}

/** Identify a native nonblocking result that may be retried only by a later bounded poll action.
 * @param error Native error ordinal.
 * @return True for interrupted or not-yet-ready I/O.
 */
bool retryLater(int error) noexcept {
#ifdef _WIN32

    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
#else

    return error == EWOULDBLOCK || error == EAGAIN || error == EINPROGRESS || error == EINTR;
#endif
}

} // namespace

/** Stable run owner; no internal completion retains the movable public handle or application callables. */
struct TcpRuntime::Impl {
    /** Release the process borrow only after context/services/socket members have been destroyed. */
    struct BootstrapBorrow {
        std::atomic_size_t &count; ///< Stable application-owned borrower counter.

        /** Retain the borrowed process lifetime before fallible host construction.
         * @param value Application-owned counter.
         */
        explicit BootstrapBorrow(std::atomic_size_t &value) noexcept : count(value) {
            count.fetch_add(1, std::memory_order_relaxed);
        }

        /** Release after every subsequent run member has been destroyed. */
        ~BootstrapBorrow() {
            count.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    RuntimeBootstrap::State *bootstrap;                 ///< Borrowed explicit application owner.
    BootstrapBorrow bootstrapBorrow;                    ///< Declared before SDK state so its release is last.
    std::thread::id owner = std::this_thread::get_id(); ///< Immutable serialized execution affinity.
    TcpOptions options; ///< Views are rebound to owned fixed arrays immediately after construction.
    std::array<char, MAX_HOST_BYTES + 2> host{}; ///< NUL-terminated endpoint/root-name storage.
    std::array<DnsServer, 4> servers{};          ///< Bounded explicit server copy.
    detail::FixedArena contextArena; ///< Retained service and descriptor storage, before the context in lifetime order.
    detail::FixedArena handlerArena; ///< Retained reusable associated-handler storage.
    asio::io_context context;        ///< Caller-pumped IOCP/epoll context; no resolver or timer service.
    Socket socket;                   ///< Sole owner of the TCP native descriptor.
    detail::DnsResolver dns;         ///< Stable independent channel/callback domain, retired before bootstrap release.
    std::unique_ptr<std::byte[]> input; ///< Whole counted input backing, never borrowed from the caller.
    std::optional<protocol::LiveSession>
        session; ///< One immutable pure epoch, installed only after fallible preparation.
    std::array<asio::ip::tcp::endpoint, MAX_ADDRESSES> addresses{}; ///< Bounded serial attempt list.
    std::size_t addressCount = 0;                                   ///< Number of initialized eligible endpoints.
    std::size_t addressIndex = 0;                                   ///< Next serial connection candidate.
    std::size_t inputBegin = 0; ///< Beginning of the exact unconsumed socket suffix.
    std::size_t inputEnd = 0;   ///< End of the exact unconsumed socket suffix.
    std::size_t frameBytes = 0; ///< Accepted but not yet admitted pure-session frame bytes.
    std::optional<protocol::LivePendingOutput>
        output;                               ///< Sole whole-envelope output owner, including consumed backing.
    TcpState runState = TcpState::CONFIGURED; ///< Serialized run lifecycle.
    TcpTerminal outcome;            ///< Reserved primary/cleanup/terminal metadata, independent of data saturation.
    bool closeRequested = false;    ///< Immediate ordinary-admission cutoff.
    bool closeApplied = false;      ///< Owned descriptor retirement has been applied once.
    bool terminalPublished = false; ///< Completion is observable only after all handlers return.
    bool connecting = false;        ///< Native nonblocking connect awaits bounded readiness inspection.
    bool namedEndpoint = false;     ///< Whether this immutable endpoint requires the selected DNS-only lookup.
    bool readPending = false;       ///< One submitted read/readiness operation owns its buffer.
    bool writePending = false;      ///< One submitted write/readiness operation owns its output suffix.
    bool readReady = false;         ///< One completed read/readiness result awaits owner processing.
    bool writeReady = false;        ///< One completed write/readiness result awaits owner processing.
    bool readErrorPending = false;  ///< A Windows error accompanying a completed prefix waits for prefix admission.
    asio::error_code readError;     ///< Static numeric completed input result.
    asio::error_code writeError;    ///< Static numeric completed output result.
    std::size_t readCount = 0;      ///< Windows completed input bytes, including raced retirement success.
    std::size_t writeCount = 0;     ///< Windows completed output bytes, including raced retirement success.
    std::size_t pending = 0;        ///< Submitted handlers that have not returned.
    std::size_t callbackDepth = 0;  ///< Executing internal handler guard.
    std::size_t adapterBytes = 0;   ///< Full retained adapter/arena/input reservations.
    std::size_t peakBytes = 0;      ///< Highest conservative simultaneous requested backing.
    Clock::time_point connectDeadline{}; ///< Whole serial connect sequence deadline.
    Clock::time_point dnsDeadline{};     ///< Whole serial two-family lookup deadline.
    Clock::time_point dnsTick{};         ///< Earliest next useful DNS readiness poll.
    Clock::time_point readDeadline{};    ///< Input inactivity deadline.
    Clock::time_point writeDeadline{};   ///< Pending output inactivity deadline.
    int lastConnectError = 0;            ///< Last serial native connect failure, retained without peer text.

    /** Own all fallible runtime storage before transferring pure-session configuration.
     * @param process Borrowed process owner.
     * @param config Validated explicit settings.
     */
    Impl(RuntimeBootstrap::State &process, TcpOptions config)
        : bootstrap(&process), bootstrapBorrow(process.borrowers), options(config),
          contextArena(config.limits.contextBytes), handlerArena(config.limits.handlerBytes),
          context(std::allocator_arg, detail::ArenaAllocator<void>(contextArena), 1), socket(context),
          input(std::make_unique<std::byte[]>(config.limits.inputBytes)) {
        std::copy(config.endpoint.begin(), config.endpoint.end(), host.begin());
        std::copy(config.dnsServers.begin(), config.dnsServers.end(), servers.begin());
        options.endpoint = {host.data(), config.endpoint.size()};
        options.dnsServers = {servers.data(), config.dnsServers.size()};
    }

    /** Release the bootstrap borrow after every owned descriptor/handler is retired. */
    ~Impl() {
        if (owner != std::this_thread::get_id()) {
            std::terminate();
        }

        closeRequested = true;
        applyClose();

        // Fallback safety is deliberately not a deadline guarantee. The explicit API exposes progress instead.
        while (pending != 0) {
            context.restart();
            context.poll_one();
        }

        finishClose();
    }

    /** Check immutable affinity before a checked public operation touches mutable state.
     * @return Empty on the owner, otherwise static WRONG_THREAD evidence.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> wrongThread() const noexcept {
        if (owner != std::this_thread::get_id()) {
            return diagnostic(TcpError::WRONG_THREAD, "owner-thread");
        }

        return std::nullopt;
    }

    /** Preserve the first failure in owner order without discarding an accompanying read prefix.
     * @param failure Static/numeric cause.
     */
    void recordFailure(TcpDiagnostic failure) noexcept {
        if (sameDiagnostic(outcome.primary, failure) || sameDiagnostic(outcome.secondary, failure)) {
            return;
        }

        if (outcome.primary.error == TcpError::NONE) {
            outcome.primary = failure;
        } else if (outcome.secondary.error == TcpError::NONE) {
            outcome.secondary = failure;
        }
    }

    /** Record a failure and apply the ordinary-admission cutoff without reentering SDK calls.
     * @param failure Static/numeric cause.
     */
    void fail(TcpDiagnostic failure) noexcept {
        recordFailure(failure);

        closeRequested = true;
    }

    /** Set a positive checked deadline; range exhaustion retires rather than wrapping time.
     * @param target Owned deadline to replace only on success.
     * @param interval Explicit positive configured duration.
     * @return True when monotonic addition is representable.
     */
    bool setDeadline(Clock::time_point &target, std::chrono::milliseconds interval) noexcept {
        const auto deadline = detail::deadlineAfter(Clock::now(), interval);

        if (!deadline) {
            fail(diagnostic(TcpError::RESOURCE_LIMIT, "monotonic-range"));

            return false;
        }

        target = *deadline;

        return true;
    }

    /** Preserve simultaneous SDK/session admission peaks before either owner scope releases storage. */
    void observePeak() noexcept {
        if (session) {
            peakBytes = (std::max)(peakBytes, adapterBytes + dns.envelopeBytes() + session->peakBytes());
        }
    }

    /** Apply an idempotent allocation-free control cutoff before further ordinary progress. */
    void applyClose() noexcept {
        if (closeApplied) {
            return;
        }

        closeApplied = true;
        runState = TcpState::CLOSING;
        connecting = false;
#ifdef _WIN32
        outcome.writeAcceptanceUnknown |= writePending;
#endif
        observePeak();
        dns.close();

        if (const auto error = dns.failure()) {
            fail(*error);
        }

        asio::error_code error;

        socket.close(error);

        if (error) {
            fail(diagnostic(TcpError::CLOSE_FAILURE, "socket-close", error.value()));
        }
    }

    /** Account a raced successful write before releasing its immutable original backing. */
    void accountWrite() noexcept {
#ifdef _WIN32

        if (writeReady && output) {
            // Each submission checked the representable cumulative offset before starting I/O.
            outcome.reportedWriteBytes += writeCount;
            const auto consumed = output->consume(writeCount);

            if (!consumed) {
                fail(protocolDiagnostic(consumed.error()));
            }
        }

#endif
        writeReady = false;
        writeCount = 0;
    }

    /** Publish completion only outside callbacks after all submitted handlers return. */
    void finishClose() noexcept {
        if (!closeApplied || pending != 0 || callbackDepth != 0 || terminalPublished) {
            return;
        }

        accountWrite();
        outcome.discardedInputBytes = inputEnd - inputBegin + frameBytes;
#ifdef _WIN32

        if (readReady) {
            outcome.discardedInputBytes += readCount;
        }

#endif
        outcome.pendingOutputBytes = output ? output->pendingBytes().size() : 0;
        output.reset();
        inputBegin = inputEnd = frameBytes = readCount = 0;
        readReady = false;
        writeReady = false;
        terminalPublished = true;
        runState = TcpState::CLOSED;
    }

    /** Record one complete internal handler without rearming I/O or invoking application code.
     * @param reading True for input, false for output.
     * @param error Native completion evidence.
     * @param bytes Completed Windows bytes; Linux readiness handlers supply zero.
     */
    void completed(bool reading, asio::error_code error, std::size_t bytes) noexcept {
        ++callbackDepth;

        if (reading) {
            readPending = false;
            readReady = true;
            readError = error;
            readCount = bytes;
        } else {
            writePending = false;
            writeReady = true;
            writeError = error;
            writeCount = bytes;
        }

#ifdef _WIN32

        if (!reading && error) {
            outcome.writeAcceptanceUnknown = true;
        }

#endif

        if (error && (error != asio::error::operation_aborted || !closeRequested) &&
            (!reading || error != asio::error::eof)) {
            recordFailure(diagnostic(reading ? TcpError::READ_FAILURE : TcpError::WRITE_FAILURE,
                                     reading ? "socket-read" : "socket-write", error.value()));

            if (!reading || bytes == 0 || closeRequested) {
                closeRequested = true;
            }
        }

        --callbackDepth;
        --pending;
    }

    /** Begin a single owned read/readiness operation; its handler never rearms itself. */
    void beginRead() {
        auto allocator = detail::ArenaAllocator<std::byte>(handlerArena);

#ifdef _WIN32
        socket.async_read_some(
            asio::buffer(input.get(), options.limits.inputBytes),
            asio::bind_allocator(allocator, [this](asio::error_code error, std::size_t bytes) noexcept {
                completed(true, error, bytes);
            }));
#else
        socket.async_wait(Socket::wait_read, asio::bind_allocator(allocator, [this](asio::error_code error) noexcept {
                              completed(true, error, 0);
                          }));
#endif
        readPending = true;
        ++pending;
    }

    /** Begin one owned short write/readiness operation after checking cumulative offset headroom. */
    void beginWrite() {
        const auto bytes = (std::min)(options.limits.writeBytes, output->pendingBytes().size());

        if (bytes > std::numeric_limits<std::uint64_t>::max() - outcome.reportedWriteBytes) {
            fail(diagnostic(TcpError::RESOURCE_LIMIT, "write-offset"));

            return;
        }

        auto allocator = detail::ArenaAllocator<std::byte>(handlerArena);

#ifdef _WIN32
        socket.async_write_some(
            asio::buffer(output->pendingBytes().data(), bytes),
            asio::bind_allocator(allocator, [this](asio::error_code error, std::size_t count) noexcept {
                completed(false, error, count);
            }));
#else
        socket.async_wait(Socket::wait_write, asio::bind_allocator(allocator, [this](asio::error_code error) noexcept {
                              completed(false, error, 0);
                          }));
#endif
        writePending = true;
        ++pending;
    }

    /** Retire a failed candidate immediately; the last failure cannot disappear behind a later user abort.
     * @param code Native error observed for this candidate.
     */
    void rejectCandidate(int code) noexcept {
        lastConnectError = code;

        if (addressIndex == addressCount) {
            fail(diagnostic(TcpError::CONNECT_FAILURE, "connect-addresses", lastConnectError));
        }

        asio::error_code error;

        socket.close(error);
        connecting = false;

        if (error) {
            fail(diagnostic(TcpError::CLOSE_FAILURE, "connect-candidate-close", error.value()));
        }
    }

    /** Inspect or start one serial native nonblocking connect, without Asio ConnectEx/reactor fallback. */
    void connectStep() {
        if (connecting) {
            const auto handle = socket.native_handle();
            int ready = 0;

#ifdef _WIN32
            fd_set writeSet;
            fd_set errorSet;
            FD_ZERO(&writeSet);
            FD_ZERO(&errorSet);
            FD_SET(handle, &writeSet);
            FD_SET(handle, &errorSet);
            timeval zero{};
            ready = select(0, nullptr, &writeSet, &errorSet, &zero);
#else
            pollfd entry{handle, POLLOUT, 0};
            ready = ::poll(&entry, 1, 0);
#endif

            if (ready == 0) {
                return;
            }

            if (ready < 0) {
                const auto error = socketError();

                if (retryLater(error)) {
                    return;
                }

                lastConnectError = error;
            } else {
                int error = 0;
#ifdef _WIN32
                int length = sizeof(error);
                const int status = getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &length);
#else
                socklen_t length = sizeof(error);
                const int status = getsockopt(handle, SOL_SOCKET, SO_ERROR, &error, &length);
#endif

                if (status == 0 && error == 0) {
                    if (Clock::now() >= connectDeadline) {
                        fail(diagnostic(TcpError::DEADLINE, "connect-completion-deadline"));

                        return;
                    }

                    connecting = false;
                    runState = TcpState::CONNECTED;
                    (void)setDeadline(readDeadline, options.readTimeout);

                    if (output) {
                        (void)setDeadline(writeDeadline, options.writeTimeout);
                    }

                    return;
                }

                lastConnectError = status == 0 ? error : socketError();

                if (status != 0 && retryLater(lastConnectError)) {
                    return;
                }
            }

            rejectCandidate(lastConnectError);

            return;
        }

        if (addressIndex == addressCount) {
            fail(diagnostic(TcpError::CONNECT_FAILURE, "connect-addresses", lastConnectError));

            return;
        }

        const auto endpoint = addresses[addressIndex++];
        asio::error_code error;

        socket.open(endpoint.protocol(), error);

        if (!error) {
            socket.native_non_blocking(true, error);
        }

        if (!error && options.socketSendBufferBytes != 0) {
            socket.set_option(asio::socket_base::send_buffer_size(static_cast<int>(options.socketSendBufferBytes)),
                              error);
        }

        if (error) {
            rejectCandidate(error.value());

            return;
        }

        const int status = ::connect(socket.native_handle(), endpoint.data(), static_cast<int>(endpoint.size()));

        if (status == 0) {
            if (Clock::now() >= connectDeadline) {
                fail(diagnostic(TcpError::DEADLINE, "connect-completion-deadline"));

                return;
            }

            runState = TcpState::CONNECTED;
            (void)setDeadline(readDeadline, options.readTimeout);

            if (output) {
                (void)setDeadline(writeDeadline, options.writeTimeout);
            }

            return;
        }

        lastConnectError = socketError();

        if (retryLater(lastConnectError)) {
            connecting = true;
        } else {
            rejectCandidate(lastConnectError);
        }
    }

    /** Process one completed input action; Linux executes exactly one native receive attempt. */
    void readStep() noexcept {
        readReady = false;

#ifdef _WIN32
        const auto bytes = readCount;
        readCount = 0;

        if (bytes != 0) {
            inputEnd = bytes;
            (void)setDeadline(readDeadline, options.readTimeout);
            readErrorPending = static_cast<bool>(readError);

            // Preserve an error accompanying a completed prefix; admit that prefix first.
            return;
        }

        if (readError == asio::error::eof) {
            peerEnd();
        } else if (readError) {
            fail(diagnostic(TcpError::READ_FAILURE, "socket-read", readError.value()));
        }

#else

        if (readError) {
            fail(diagnostic(TcpError::READ_FAILURE, "socket-read-wait", readError.value()));

            return;
        }

        const auto bytes = ::recv(socket.native_handle(), input.get(), options.limits.inputBytes, MSG_DONTWAIT);

        if (bytes > 0) {
            inputEnd = static_cast<std::size_t>(bytes);
            (void)setDeadline(readDeadline, options.readTimeout);
        } else if (bytes == 0) {
            peerEnd();
        } else {
            const auto error = socketError();

            if (!retryLater(error)) {
                fail(diagnostic(TcpError::READ_FAILURE, "socket-read", error));
            }
        }

#endif
    }

    /** End pure input explicitly; an incomplete frame is a failure, never successful EOF. */
    void peerEnd() noexcept {
        if (session->finish() == protocol::LiveStatus::CLOSED) {
            outcome.peerEnded = true;
            closeRequested = true;
        } else {
            const auto error = session->diagnostic().error;

            fail({TcpError::PROTOCOL_FAILURE, "session-eof", 0, error});
        }
    }

    /** Process one completed output action; Linux executes exactly one native send attempt. */
    void writeStep() noexcept {
        bool progressed = false;

#ifdef _WIN32
        progressed = writeCount != 0;
        accountWrite();

        if (writeError) {
            outcome.writeAcceptanceUnknown = true;
            fail(diagnostic(TcpError::WRITE_FAILURE, "socket-write", writeError.value()));
        }

#else
        writeReady = false;

        if (writeError) {
            fail(diagnostic(TcpError::WRITE_FAILURE, "socket-write-wait", writeError.value()));

            return;
        }

        const auto bytes = (std::min)(options.limits.writeBytes, output->pendingBytes().size());
        const auto written =
            ::send(socket.native_handle(), output->pendingBytes().data(), bytes, MSG_DONTWAIT | MSG_NOSIGNAL);

        if (written > 0) {
            progressed = true;
            const auto count = static_cast<std::size_t>(written);
            outcome.reportedWriteBytes += count;
            const auto consumed = output->consume(count);

            if (!consumed) {
                fail(protocolDiagnostic(consumed.error()));
            }
        } else if (written < 0) {
            const auto error = socketError();

            if (!retryLater(error)) {
                fail(diagnostic(TcpError::WRITE_FAILURE, "socket-write", error));
            }
        }

#endif

        if (output->pendingBytes().empty()) {
            output.reset();
        } else if (progressed) {
            (void)setDeadline(writeDeadline, options.writeTimeout);
        }
    }

    /** Inspect the earliest useful caller wake; no Asio timer service or helper thread is created.
     * @return Monotonic hint, empty for inactive/terminal runs.
     */
    [[nodiscard]] std::optional<Clock::time_point> wake() const noexcept {
        if (terminalPublished || runState == TcpState::CONFIGURED) {
            return std::nullopt;
        }

        if (closeRequested) {
            return Clock::now();
        }

        if (runState == TcpState::RESOLVING) {
            return dns.immediate() ? Clock::now() : (std::min)(dnsDeadline, dnsTick);
        }

        if (runState == TcpState::CONNECTING) {
            const auto cadence = detail::deadlineAfter(Clock::now(), options.pollInterval);

            return cadence ? (std::min)(connectDeadline, *cadence) : connectDeadline;
        }

        if (runState == TcpState::CONNECTED) {
            if (readReady || writeReady || inputBegin != inputEnd || (output && !writePending) || !readPending) {
                return Clock::now();
            }

            const auto deadline = output ? (std::min)(readDeadline, writeDeadline) : readDeadline;

            const auto cadence = detail::deadlineAfter(Clock::now(), options.pollInterval);

            return cadence ? (std::min)(deadline, *cadence) : deadline;
        }

        return std::nullopt;
    }

    /** Reject new output before any pure-session mutation when admission or the sole slot is unavailable.
     * @return Empty on admission, otherwise static rejection evidence.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> outputRejection() const noexcept {
        if (const auto error = wrongThread()) {
            return error;
        }

        if (closeRequested || outcome.primary.error != TcpError::NONE || runState == TcpState::CLOSED) {
            return diagnostic(TcpError::INVALID_STATE, "output-closed");
        }

        if (output) {
            return diagnostic(TcpError::RESOURCE_LIMIT, "output-slot");
        }

        return std::nullopt;
    }

    /** Adopt a completely composed immutable output without another allocation.
     * @param value Whole pure-session result.
     * @return Queue admission or detached pure-session failure.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic>
    adoptOutput(std::expected<protocol::LivePendingOutput, protocol::LiveError> value) {
        observePeak();

        if (!value) {
            return std::unexpected(protocolDiagnostic(value.error()));
        }

        output.emplace(std::move(*value));

        if (!setDeadline(writeDeadline, options.writeTimeout)) {
            return std::unexpected(outcome.primary);
        }

        return {};
    }
};

TcpRuntime::TcpRuntime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TcpRuntime::TcpRuntime(TcpRuntime &&other) noexcept = default;
TcpRuntime &TcpRuntime::operator=(TcpRuntime &&other) noexcept = default;
TcpRuntime::~TcpRuntime() = default;

std::expected<TcpRuntime, TcpDiagnostic> TcpRuntime::create(RuntimeBootstrap &bootstrap, core::LocalSchema &&schema,
                                                            protocol::SubscriptionWriter &&composer,
                                                            protocol::LiveLimits sessionLimits, TcpOptions options,
                                                            protocol::LiveWork &work) {
    if (work.operations == 0) {
        return std::unexpected(diagnostic(TcpError::WORK_LIMIT, "runtime-create-attempt"));
    }

    --work.operations;
    const auto &limits = options.limits;

    if (!bootstrap.state_ || !bootstrap.state_->dnsInitialized ||
        sessionLimits.credentials != protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN ||
        !validEndpointSettings(options) || !validSocketLimits(limits) || !validTimingSettings(options)) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "runtime-configuration"));
    }

    std::array<char, MAX_HOST_BYTES + 2> name{};
    std::copy(options.endpoint.begin(), options.endpoint.end(), name.begin());
    asio::error_code addressError;
    const auto address = asio::ip::make_address(name.data(), addressError);

    const bool namedEndpoint = static_cast<bool>(addressError);

    if (namedEndpoint) {
        const auto zeroServer = std::ranges::any_of(options.dnsServers, [](const DnsServer &server) {
            return std::ranges::all_of(server.address, [](std::uint8_t octet) {
                return octet == 0;
            });
        });

        if (!detail::DnsResolver::validName(options.endpoint) || options.dnsServers.empty() || options.dnsPort == 0 ||
            zeroServer) {
            return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "dns-configuration"));
        }
    }

    std::size_t backing = sizeof(Impl) + sizeof(RuntimeBootstrap::State);

    if (!addBytes(backing, limits.inputBytes) || !addBytes(backing, limits.contextBytes) ||
        !addBytes(backing, limits.handlerBytes)) {
        return std::unexpected(diagnostic(TcpError::RESOURCE_LIMIT, "runtime-reservation"));
    }

    std::size_t totalReservation = backing;

    if ((namedEndpoint && !addBytes(totalReservation, detail::DnsResolver::ENVELOPE_BYTES)) ||
        sessionLimits.residentBytes > limits.residentBytes ||
        totalReservation > limits.residentBytes - sessionLimits.residentBytes) {
        return std::unexpected(diagnostic(TcpError::RESOURCE_LIMIT, "runtime-reservation"));
    }

    constexpr auto PREPARATION_VISITS = 2 * detail::FixedArena::MAX_BLOCKS * detail::FixedArena::MAX_BLOCKS;
    constexpr std::size_t PREPARATION_BYTE_PASSES =
        3; // Zeroed backing, SDK initialization and fixed configuration copy.

    const auto dnsPreparation =
        namedEndpoint ? detail::DnsResolver::preparationWork(options.dnsServers.size()) : protocol::LiveWork{};
    std::size_t preparationBytes = backing;

    if (preparationBytes > std::numeric_limits<std::size_t>::max() / PREPARATION_BYTE_PASSES) {
        return std::unexpected(diagnostic(TcpError::WORK_LIMIT, "runtime-preparation-work"));
    }

    preparationBytes *= PREPARATION_BYTE_PASSES;

    if (!addBytes(preparationBytes, dnsPreparation.bytes) || work.bytes < preparationBytes ||
        work.visits < PREPARATION_VISITS + dnsPreparation.visits) {
        return std::unexpected(diagnostic(TcpError::WORK_LIMIT, "runtime-preparation-work"));
    }

    work.bytes -= preparationBytes;
    work.visits -= PREPARATION_VISITS + dnsPreparation.visits;

    try {
        auto impl = std::make_unique<Impl>(*bootstrap.state_, options);
        impl->adapterBytes = backing;
        impl->namedEndpoint = namedEndpoint;

        if (namedEndpoint) {
            auto initialized = impl->dns.initialize(impl->options);

            if (!initialized) {
                return std::unexpected(initialized.error());
            }
        } else {
            impl->addresses[0] = {address, options.port};
            impl->addressCount = 1;
        }

        auto session = protocol::LiveSession::create(std::move(schema), std::move(composer), sessionLimits, work);

        if (!session) {
            return std::unexpected(protocolDiagnostic(session.error()));
        }

        impl->session.emplace(std::move(*session));
        impl->peakBytes = totalReservation + impl->session->peakBytes();

        return TcpRuntime(std::move(impl));
    } catch (const std::bad_alloc &) {
        return std::unexpected(diagnostic(TcpError::ALLOCATION_FAILURE, "runtime-storage"));
    } catch (const asio::system_error &error) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "runtime-host", error.code().value()));
    }
}

std::expected<void, TcpDiagnostic> TcpRuntime::start() {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->wrongThread()) {
        return std::unexpected(*error);
    }

    if (impl_->runState != TcpState::CONFIGURED || impl_->closeRequested) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "start-state"));
    }

    const auto now = Clock::now();

    if (impl_->namedEndpoint) {
        impl_->runState = TcpState::RESOLVING;

        if (!impl_->setDeadline(impl_->dnsDeadline, impl_->options.dnsTimeout)) {
            return std::unexpected(impl_->outcome.primary);
        }

        impl_->dnsTick = now;
    } else {
        impl_->runState = TcpState::CONNECTING;

        if (!impl_->setDeadline(impl_->connectDeadline, impl_->options.connectTimeout)) {
            return std::unexpected(impl_->outcome.primary);
        }
    }

    return {};
}

std::expected<protocol::LiveWork, TcpDiagnostic>
TcpRuntime::dnsPreparationWorkRequirement(std::size_t serverCount) noexcept {
    if (serverCount == 0 || serverCount > 4) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "dns-preparation-servers"));
    }

    return detail::DnsResolver::preparationWork(serverCount);
}

std::expected<TcpPollResult, TcpDiagnostic> TcpRuntime::poll(TcpWork &work, protocol::LiveWork &sessionWork) {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    auto &state = *impl_;

    if (const auto error = state.wrongThread()) {
        return std::unexpected(*error);
    }

    if (state.terminalPublished) {
        return TcpPollResult{TcpPollStatus::CLOSED, std::nullopt};
    }

    const auto now = Clock::now();

    if (!state.closeRequested && ((state.runState == TcpState::RESOLVING && now >= state.dnsDeadline) ||
                                  (state.runState == TcpState::CONNECTING && now >= state.connectDeadline) ||
                                  (state.runState == TcpState::CONNECTED &&
                                   (now >= state.readDeadline || (state.output && now >= state.writeDeadline))))) {
        state.fail(diagnostic(TcpError::DEADLINE, "operation-deadline"));
    }

    if (state.closeRequested) {
        state.applyClose();
        state.finishClose();

        if (state.terminalPublished) {
            return TcpPollResult{TcpPollStatus::CLOSED, std::nullopt};
        }
    }

    if (work.steps == 0) {
        return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
    }

    --work.steps;

    try {
        if (state.pending != 0 && work.handlers != 0) {
            --work.handlers;
            state.context.restart();

            if (state.context.poll_one() != 0) {
                state.finishClose();

                return TcpPollResult{state.terminalPublished ? TcpPollStatus::CLOSED : TcpPollStatus::PROGRESS,
                                     std::nullopt};
            }
        }

        if (state.closeRequested) {
            return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
        }

        if (state.runState == TcpState::CONNECTING) {
            state.connectStep();

            return TcpPollResult{TcpPollStatus::PROGRESS, std::nullopt};
        }

        if (state.runState == TcpState::RESOLVING) {
            if (!state.dns.immediate() && now < state.dnsTick) {
                return TcpPollResult{TcpPollStatus::IDLE, std::nullopt};
            }

            const auto progress = state.dns.progress(work);

            if (const auto error = state.dns.failure()) {
                state.fail(*error);
            } else if (state.dns.finished()) {
                if (Clock::now() >= state.dnsDeadline) {
                    state.fail(diagnostic(TcpError::DEADLINE, "dns-completion-deadline"));

                    return TcpPollResult{TcpPollStatus::PROGRESS, std::nullopt};
                }

                for (const auto &resolved : state.dns.addresses()) {
                    if (resolved.ipv6) {
                        state.addresses[state.addressCount++] = {asio::ip::address_v6(resolved.bytes),
                                                                 state.options.port};
                    } else {
                        asio::ip::address_v4::bytes_type octets{};
                        std::copy_n(resolved.bytes.begin(), octets.size(), octets.begin());
                        state.addresses[state.addressCount++] = {asio::ip::address_v4(octets), state.options.port};
                    }
                }

                state.observePeak();
                state.dns.close();
                state.runState = TcpState::CONNECTING;
                (void)state.setDeadline(state.connectDeadline, state.options.connectTimeout);
            } else if (progress != TcpPollStatus::YIELD) {
                (void)state.setDeadline(state.dnsTick, state.options.pollInterval);
            }

            return TcpPollResult{progress, std::nullopt};
        }

        if (state.runState != TcpState::CONNECTED) {
            return TcpPollResult{TcpPollStatus::IDLE, std::nullopt};
        }

        if (state.writeReady) {
#ifndef _WIN32
            const auto bytes = (std::min)(state.options.limits.writeBytes, state.output->pendingBytes().size());

            if (work.socketBytes < bytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= bytes;
#endif
            state.writeStep();
        } else if (state.readReady) {
#ifndef _WIN32

            if (work.socketBytes < state.options.limits.inputBytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= state.options.limits.inputBytes;
#endif
            state.readStep();
        } else if (state.inputBegin != state.inputEnd) {
            auto result = state.session->feed({state.input.get() + state.inputBegin, state.inputEnd - state.inputBegin},
                                              sessionWork);
            state.inputBegin += result.accepted;
            state.frameBytes += result.accepted;

            if (state.inputBegin == state.inputEnd) {
                state.inputBegin = state.inputEnd = 0;
            }

            state.observePeak();

            if (result.status == protocol::LiveStatus::FAILED) {
                state.fail({TcpError::PROTOCOL_FAILURE, "session-input", 0, state.session->diagnostic().error});
            }

            if (result.event) {
                state.frameBytes = 0;

                return TcpPollResult{TcpPollStatus::EVENT, std::move(result.event)};
            }
        } else if (state.readErrorPending) {
            state.readErrorPending = false;

            if (state.readError == asio::error::eof) {
                state.peerEnd();
            } else {
                state.fail(diagnostic(TcpError::READ_FAILURE, "socket-read", state.readError.value()));
            }
        } else if (state.output && !state.writePending) {
#ifdef _WIN32
            const auto bytes = (std::min)(state.options.limits.writeBytes, state.output->pendingBytes().size());

            if (work.socketBytes < bytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= bytes;
#endif
            state.beginWrite();
        } else if (!state.readPending) {
#ifdef _WIN32

            if (work.socketBytes < state.options.limits.inputBytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= state.options.limits.inputBytes;
#endif
            state.beginRead();
        } else {
            return TcpPollResult{state.pending != 0 && work.handlers == 0 ? TcpPollStatus::YIELD : TcpPollStatus::IDLE,
                                 std::nullopt};
        }

        return TcpPollResult{TcpPollStatus::PROGRESS, std::nullopt};
    } catch (const std::bad_alloc &) {
        state.fail(diagnostic(TcpError::ALLOCATION_FAILURE, "operation-storage"));
    } catch (const asio::system_error &error) {
        state.fail(diagnostic(TcpError::READ_FAILURE, "operation-host", error.code().value()));
    }

    return TcpPollResult{TcpPollStatus::PROGRESS, std::nullopt};
}

std::expected<void, TcpDiagnostic> TcpRuntime::composeProtocol(protocol::LiveProtocolView descriptor,
                                                               protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->outputRejection()) {
        return std::unexpected(*error);
    }

    return impl_->adoptOutput(impl_->session->composeProtocol(descriptor, work));
}

std::expected<void, TcpDiagnostic> TcpRuntime::composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->outputRejection()) {
        return std::unexpected(*error);
    }

    return impl_->adoptOutput(impl_->session->composeHeartbeat(heartbeat, work));
}

std::expected<void, TcpDiagnostic> TcpRuntime::describe(std::span<const std::int32_t> senderIds,
                                                        protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->outputRejection()) {
        return std::unexpected(*error);
    }

    return impl_->adoptOutput(impl_->session->describe(senderIds, work));
}

std::expected<void, TcpDiagnostic> TcpRuntime::subscribe(protocol::SubscriptionMessage message,
                                                         std::span<const core::SubscriptionRequest> requests,
                                                         protocol::LiveWork &work) {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->outputRejection()) {
        return std::unexpected(*error);
    }

    return impl_->adoptOutput(impl_->session->subscribe(message, requests, work));
}

std::expected<void, TcpDiagnostic> TcpRuntime::requestClose() noexcept {
    if (!impl_) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "moved-runtime"));
    }

    if (const auto error = impl_->wrongThread()) {
        return std::unexpected(*error);
    }

    if (!impl_->terminalPublished) {
        impl_->outcome.aborted = true;
        impl_->closeRequested = true;
        impl_->applyClose();
    }

    return {};
}

TcpState TcpRuntime::state() const noexcept {
    return impl_ ? impl_->runState : TcpState::CLOSED;
}

std::optional<TcpTerminal> TcpRuntime::terminal() const noexcept {
    return impl_ && impl_->terminalPublished ? std::optional(impl_->outcome) : std::nullopt;
}

protocol::LiveSnapshot TcpRuntime::peer() const noexcept {
    return impl_ ? impl_->session->peer() : protocol::LiveSnapshot{};
}

protocol::LiveAuthentication TcpRuntime::authentication() const noexcept {
    return impl_ ? impl_->session->authentication() : protocol::LiveAuthentication::UNCHALLENGED;
}

TcpResources TcpRuntime::resources() const noexcept {
    if (!impl_) {
        return {};
    }

    const auto current = impl_->session->residentBytes();
    const auto dns = impl_->dns.envelopeBytes();
    impl_->observePeak();

    return {impl_->adapterBytes,
            dns,
            current,
            impl_->adapterBytes + dns + current,
            impl_->peakBytes,
            impl_->pending + (impl_->dns.pending() ? 1 : 0) + (impl_->connecting ? 1 : 0)};
}

std::optional<Clock::time_point> TcpRuntime::nextWake() const noexcept {
    return impl_ ? impl_->wake() : std::nullopt;
}

TcpWork TcpRuntime::dnsProgressWorkRequirement() const noexcept {
    return impl_ && impl_->namedEndpoint ? impl_->dns.workRequirement() : TcpWork{};
}

} // namespace org::ttldtor::qdcxx::transport
