// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "runtime_core.hpp"

#include "deadline.hpp"
#include "dns_resolver.hpp"
#include "fixed_arena.hpp"
#include "raw_byte_state.hpp"
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

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

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

/** Own all fallible runtime storage before transferring pure-session configuration.
 * @param process Borrowed process owner.
 * @param borrow Independent counted borrow, retained before fallible native members.
 * @param config Validated explicit settings.
 */
RuntimeCore::RuntimeCore(RuntimeBootstrap::State &process, BootstrapLease &&borrow, TcpOptions config)
    : bootstrap(&process), bootstrapBorrow(std::move(borrow)), options(config),
      contextArena(config.limits.contextBytes), handlerArena(config.limits.handlerBytes),
      context(std::allocator_arg, detail::ArenaAllocator<void>(contextArena), 1), socket(context),
      input(std::make_unique<std::byte[]>(config.limits.inputBytes)) {
    std::copy(config.endpoint.begin(), config.endpoint.end(), host.begin());
    std::copy(config.dnsServers.begin(), config.dnsServers.end(), servers.begin());
    options.endpoint = {host.data(), config.endpoint.size()};
    options.dnsServers = {servers.data(), config.dnsServers.size()};
}

/** Release the bootstrap borrow after every owned descriptor/handler is retired. */
RuntimeCore::~RuntimeCore() {
    retire();
}

void RuntimeCore::retire() noexcept {
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

ConnectionStatistics RuntimeCore::statistics() const noexcept {
    auto result = ioStatistics;
    result.writeAcceptanceUnknown = outcome.writeAcceptanceUnknown;

#ifdef _WIN32

    // Completion precedes owner cursor accounting. Include that same actual prefix without mutating output;
    // accountWrite later folds it into the totals and clears writeReady, so neither cut counts it twice.
    if (writeReady) {
        result.writtenBytes.add(writeCount);

        if (!engine && output && writeCount != 0 && writeCount == output->pendingBytes().size()) {
            result.writtenMessages.add(1);
            result.writtenSubscriptionRecords.add(output->subscriptionRecords());
        }
    }

#endif

    if (!engine && !session.raw()) {
        const auto inputStatistics = session->inputStatistics();
        result.readMessages = inputStatistics.messages;
        result.readDataRecords = inputStatistics.dataRecords;
    } else {
        result.readMessages.availability = core::StatisticAvailability::UNSUPPORTED;
        result.readDataRecords.availability = core::StatisticAvailability::UNSUPPORTED;
        result.writtenMessages.availability = core::StatisticAvailability::UNSUPPORTED;
        result.writtenSubscriptionRecords.availability = core::StatisticAvailability::UNSUPPORTED;
    }

    return result;
}

/** Check immutable affinity before a checked public operation touches mutable state.
 * @return Empty on the owner, otherwise static WRONG_THREAD evidence.
 */
[[nodiscard]] std::optional<TcpDiagnostic> RuntimeCore::wrongThread() const noexcept {
    if (owner != std::this_thread::get_id()) {
        return diagnostic(TcpError::WRONG_THREAD, "owner-thread");
    }

    return std::nullopt;
}

/** Preserve the first failure in owner order without discarding an accompanying read prefix.
 * @param failure Static/numeric cause.
 */
void RuntimeCore::recordFailure(TcpDiagnostic failure) noexcept {
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
void RuntimeCore::fail(TcpDiagnostic failure) noexcept {
    recordFailure(failure);

    closeRequested = true;
}

/** Set a positive checked deadline; range exhaustion retires rather than wrapping time.
 * @param target Owned deadline to replace only on success.
 * @param interval Explicit positive configured duration.
 * @return True when monotonic addition is representable.
 */
bool RuntimeCore::setDeadline(Clock::time_point &target, std::chrono::milliseconds interval) noexcept {
    const auto deadline = detail::deadlineAfter(Clock::now(), interval);

    if (!deadline) {
        fail(diagnostic(TcpError::RESOURCE_LIMIT, "monotonic-range"));

        return false;
    }

    target = *deadline;

    return true;
}

/** Preserve simultaneous SDK/session admission peaks before either owner scope releases storage. */
void RuntimeCore::observePeak() noexcept {
    if (session) {
        peakBytes = (std::max)(peakBytes, adapterBytes + dns.envelopeBytes() + session->peakBytes());
    } else if (session.raw()) {
        peakBytes = (std::max)(peakBytes, adapterBytes + dns.envelopeBytes());
    }
}

/** Apply an idempotent allocation-free control cutoff before further ordinary progress. */
void RuntimeCore::applyClose() noexcept {
    if (closeApplied) {
        return;
    }

    closeApplied = true;

    if (engine) {
        engine->abort();
    }

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
void RuntimeCore::accountWrite() noexcept {
#ifdef _WIN32

    if (writeReady && (output || engine || session.raw())) {
        // Each submission checked the representable cumulative offset before starting I/O.
        outcome.reportedWriteBytes += writeCount;
        ioStatistics.writtenBytes.add(writeCount);
        (void)consumeWire(writeCount);
    }

#endif
    writeReady = false;
    writeCount = 0;
}

/** Publish completion only outside callbacks after all submitted handlers return. */
void RuntimeCore::finishClose() noexcept {
    if (!closeApplied || pending != 0 || callbackDepth != 0 || terminalPublished) {
        return;
    }

    accountWrite();

    if (ioStatistics.establishedConnections.value != 0) {
        ioStatistics.closedConnections.add(1);
        ioStatistics.activeConnections = 0;
        ioStatistics.sourceEpoch.reset();
    }

    outcome.discardedInputBytes = inputEnd - inputBegin + (engine ? 0 : frameBytes);
#ifdef _WIN32

    if (readReady) {
        outcome.discardedInputBytes += readCount;
    }

#endif
    outcome.pendingOutputBytes = wireBytes().size();
    pendingApplicationAtClose = applicationBytes().size();
    output.reset();

    if (auto *raw = session.raw()) {
        raw->retireOutput();
    }

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
void RuntimeCore::completed(bool reading, asio::error_code error, std::size_t bytes) noexcept {
    ++callbackDepth;

    if (reading) {
#ifdef _WIN32
        ioStatistics.readBytes.add(bytes);
#endif
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

    bool expectedPauseRead = false;
#ifdef _WIN32
    // ERROR_CONNECTION_ABORTED is returned by an owned closesocket cancellation on Windows.
    expectedPauseRead = reading && pauseCancellation && closeRequested &&
                        error == asio::error_code(ERROR_CONNECTION_ABORTED, asio::error::get_system_category());
#endif

    if (error && !expectedPauseRead && (error != asio::error::operation_aborted || !closeRequested) &&
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

/** Inspect the exact immutable native-output suffix for either selected byte path.
 * @return Engine ciphertext or unchanged plain application output.
 */
std::span<const std::byte> RuntimeCore::wireBytes() const noexcept {
    if (engine) {
        return engine->ciphertext();
    }

    return applicationBytes();
}

std::span<const std::byte> RuntimeCore::applicationBytes() const noexcept {
    if (const auto *raw = session.raw()) {
        return raw->output();
    }

    return output ? output->pendingBytes() : std::span<const std::byte>{};
}

bool RuntimeCore::consumeApplication(std::size_t bytes) noexcept {
    if (auto *raw = session.raw()) {
        if (!raw->consumeOutput(bytes)) {
            fail(diagnostic(TcpError::RESOURCE_LIMIT, "raw-output-count"));

            return false;
        }

        return true;
    }

    const auto consumed = output->consume(bytes);

    if (!consumed) {
        fail(protocolDiagnostic(consumed.error()));

        return false;
    }

    if (!engine && bytes != 0 && output->pendingBytes().empty()) {
        ioStatistics.writtenMessages.add(1);
        ioStatistics.writtenSubscriptionRecords.add(output->subscriptionRecords());
    }

    return true;
}

/** Account native acceptance without advancing an application span on a codec WANT result.
 * @param bytes Native accepted prefix of wireBytes().
 * @return True for an eligible prefix, otherwise records the consistency failure.
 */
bool RuntimeCore::consumeWire(std::size_t bytes) noexcept {
    if (engine) {
        if (!engine->consumeCiphertext(bytes)) {
            fail(diagnostic(TcpError::RESOURCE_LIMIT, "engine-wire-count"));

            return false;
        }

        return true;
    }

    return consumeApplication(bytes);
}

/** Begin a single owned read/readiness operation; its handler never rearms itself. */
void RuntimeCore::beginRead() {
    auto allocator = detail::ArenaAllocator<std::byte>(handlerArena);

#ifdef _WIN32
    socket.async_read_some(asio::buffer(input.get(), options.limits.inputBytes),
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
void RuntimeCore::beginWrite() {
    const auto bytes = (std::min)(options.limits.writeBytes, wireBytes().size());

    if (bytes > std::numeric_limits<std::uint64_t>::max() - outcome.reportedWriteBytes) {
        fail(diagnostic(TcpError::RESOURCE_LIMIT, "write-offset"));

        return;
    }

    auto allocator = detail::ArenaAllocator<std::byte>(handlerArena);

#ifdef _WIN32
    socket.async_write_some(asio::buffer(wireBytes().data(), bytes),
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
void RuntimeCore::rejectCandidate(int code) noexcept {
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
void RuntimeCore::connectStep() {
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
                ioStatistics.establishedConnections.add(1);
                ioStatistics.activeConnections = 1;
                ioStatistics.sourceEpoch = 1;
                (void)setDeadline(readDeadline, options.readTimeout);

                if (output || !applicationBytes().empty()) {
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
        socket.set_option(asio::socket_base::send_buffer_size(static_cast<int>(options.socketSendBufferBytes)), error);
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
        ioStatistics.establishedConnections.add(1);
        ioStatistics.activeConnections = 1;
        ioStatistics.sourceEpoch = 1;
        (void)setDeadline(readDeadline, options.readTimeout);

        if (output || !applicationBytes().empty()) {
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
void RuntimeCore::readStep() noexcept {
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
        ioStatistics.readBytes.add(static_cast<std::uint64_t>(bytes));
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

/** End the selected input path; raw EOF is byte evidence, while QTP still requires complete frames. */
void RuntimeCore::peerEnd() noexcept {
    if (engine) {
        if (const auto error = engine->nativeEnd()) {
            fail(*error);

            return;
        }
    }

    if (session.raw()) {
        outcome.peerEnded = true;
        closeRequested = true;

        return;
    }

    if (session->finish() == protocol::LiveStatus::CLOSED) {
        outcome.peerEnded = true;
        closeRequested = true;
    } else {
        const auto error = session->diagnostic().error;

        fail({TcpError::PROTOCOL_FAILURE, "session-eof", 0, error});
    }
}

/** Process one completed output action; Linux executes exactly one native send attempt. */
void RuntimeCore::writeStep() noexcept {
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

    const auto bytes = (std::min)(options.limits.writeBytes, wireBytes().size());
    const auto written = ::send(socket.native_handle(), wireBytes().data(), bytes, MSG_DONTWAIT | MSG_NOSIGNAL);

    if (written > 0) {
        progressed = true;
        const auto count = static_cast<std::size_t>(written);
        outcome.reportedWriteBytes += count;
        ioStatistics.writtenBytes.add(count);
        (void)consumeWire(count);
    } else if (written < 0) {
        const auto error = socketError();

        if (!retryLater(error)) {
            fail(diagnostic(TcpError::WRITE_FAILURE, "socket-write", error));
        }
    }

#endif

    if (wireBytes().empty()) {
        if (!engine) {
            output.reset();
        }
    } else if (progressed) {
        (void)setDeadline(writeDeadline, options.writeTimeout);
    }
}

/** Inspect the earliest useful caller wake; no Asio timer service or helper thread is created.
 * @return Monotonic hint, empty for inactive/terminal runs.
 */
[[nodiscard]] std::optional<Clock::time_point> RuntimeCore::wake() const noexcept {
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
        if (readReady || writeReady || inputBegin != inputEnd ||
            ((output || !applicationBytes().empty()) && !writePending) || !readPending) {
            return Clock::now();
        }

        const auto deadline =
            (output || !applicationBytes().empty()) ? (std::min)(readDeadline, writeDeadline) : readDeadline;

        const auto cadence = detail::deadlineAfter(Clock::now(), options.pollInterval);

        return cadence ? (std::min)(deadline, *cadence) : deadline;
    }

    return std::nullopt;
}

/** Reject new output before any pure-session mutation when admission or the sole slot is unavailable.
 * @return Empty on admission, otherwise static rejection evidence.
 */
[[nodiscard]] std::optional<TcpDiagnostic> RuntimeCore::outputRejection() const noexcept {
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
RuntimeCore::adoptOutput(std::expected<protocol::LivePendingOutput, protocol::LiveError> value) {
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

std::expected<std::unique_ptr<RuntimeCore>, TcpDiagnostic>
RuntimeCore::create(RuntimeBootstrap::State *bootstrap, core::LocalSchema &&schema,
                    protocol::SubscriptionWriter &&composer, protocol::LiveLimits sessionLimits, TcpOptions options,
                    protocol::LiveWork &work, std::unique_ptr<ByteEngine> codec) {
    if (work.operations == 0) {
        return std::unexpected(diagnostic(TcpError::WORK_LIMIT, "runtime-create-attempt"));
    }

    --work.operations;
    const auto &limits = options.limits;

    if (!bootstrap || !bootstrap->dnsInitialized ||
        sessionLimits.credentials != (codec ? protocol::LiveCredentialPolicy::VERIFIED_TLS_BASIC
                                            : protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN) ||
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

    std::size_t backing = sizeof(RuntimeCore) + sizeof(RuntimeBootstrap::State);

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

    auto borrow = BootstrapLease::acquire(bootstrap);

    if (!borrow) {
        return std::unexpected(borrow.error());
    }

    try {
        auto impl = std::make_unique<RuntimeCore>(*bootstrap, std::move(*borrow), options);
        impl->engine = std::move(codec);
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

        return impl;
    } catch (const std::bad_alloc &) {
        return std::unexpected(diagnostic(TcpError::ALLOCATION_FAILURE, "runtime-storage"));
    } catch (const asio::system_error &error) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "runtime-host", error.code().value()));
    }
}

std::expected<std::unique_ptr<RuntimeCore>, TcpDiagnostic>
RuntimeCore::createRaw(RuntimeBootstrap::State *bootstrap, RawByteState &raw, std::size_t externalBytes,
                       TcpOptions options, protocol::LiveWork &work, std::unique_ptr<ByteEngine> codec) {
    if (work.operations == 0) {
        return std::unexpected(diagnostic(TcpError::WORK_LIMIT, "runtime-create-attempt"));
    }

    --work.operations;
    const auto &limits = options.limits;

    if (!bootstrap || !bootstrap->dnsInitialized || !validEndpointSettings(options) || !validSocketLimits(limits) ||
        !validTimingSettings(options)) {
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

    std::size_t backing = sizeof(RuntimeCore) + sizeof(RuntimeBootstrap::State);

    if (!addBytes(backing, limits.inputBytes) || !addBytes(backing, limits.contextBytes) ||
        !addBytes(backing, limits.handlerBytes)) {
        return std::unexpected(diagnostic(TcpError::RESOURCE_LIMIT, "runtime-reservation"));
    }

    std::size_t totalReservation = backing;

    if ((namedEndpoint && !addBytes(totalReservation, detail::DnsResolver::ENVELOPE_BYTES)) ||
        !addBytes(totalReservation, externalBytes) || totalReservation > limits.residentBytes) {
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

    auto borrow = BootstrapLease::acquire(bootstrap);

    if (!borrow) {
        return std::unexpected(borrow.error());
    }

    try {
        auto impl = std::make_unique<RuntimeCore>(*bootstrap, std::move(*borrow), options);
        impl->engine = std::move(codec);
        impl->adapterBytes = backing + externalBytes;
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

        impl->session.selectRaw(raw);
        impl->peakBytes = totalReservation;

        return impl;
    } catch (const std::bad_alloc &) {
        return std::unexpected(diagnostic(TcpError::ALLOCATION_FAILURE, "runtime-storage"));
    } catch (const asio::system_error &error) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "runtime-host", error.code().value()));
    }
}

std::expected<void, TcpDiagnostic> RuntimeCore::start() {
    if (const auto error = this->wrongThread()) {
        return std::unexpected(*error);
    }

    if (this->runState != TcpState::CONFIGURED || this->closeRequested) {
        return std::unexpected(diagnostic(TcpError::INVALID_STATE, "start-state"));
    }

    const auto now = Clock::now();

    if (this->namedEndpoint) {
        ioStatistics.connectionAttempts.add(1);
        ioStatistics.attemptIdentity = 1;
        this->runState = TcpState::RESOLVING;

        if (!this->setDeadline(this->dnsDeadline, this->options.dnsTimeout)) {
            return std::unexpected(this->outcome.primary);
        }

        this->dnsTick = now;
    } else {
        ioStatistics.connectionAttempts.add(1);
        ioStatistics.attemptIdentity = 1;
        this->runState = TcpState::CONNECTING;

        if (!this->setDeadline(this->connectDeadline, this->options.connectTimeout)) {
            return std::unexpected(this->outcome.primary);
        }
    }

    return {};
}

std::expected<protocol::LiveWork, TcpDiagnostic>
RuntimeCore::dnsPreparationWorkRequirement(std::size_t serverCount) noexcept {
    if (serverCount == 0 || serverCount > 4) {
        return std::unexpected(diagnostic(TcpError::INVALID_CONFIGURATION, "dns-preparation-servers"));
    }

    return detail::DnsResolver::preparationWork(serverCount);
}

/** Perform one connected codec/session/native action, keeping socket ownership unique.
 * @param work Mutable native caller-turn credits.
 * @param sessionWork Mutable pure-session caller-turn credits.
 * @return One bounded action/event/yield observation; no application callback is invoked.
 */
TcpPollResult RuntimeCore::pollEngine(TcpWork &work, protocol::LiveWork &sessionWork) {
    auto *raw = session.raw();
    const bool decodePaused =
        (!raw ? sessionWork.operations == 0 : sessionWork.operations == 0 || !raw->input().empty()) &&
        !engine->plaintext().empty();

    if (writeReady) {
#ifndef _WIN32
        const auto bytes = (std::min)(options.limits.writeBytes, wireBytes().size());

        if (work.socketBytes < bytes) {
            return {TcpPollStatus::YIELD, std::nullopt};
        }

        work.socketBytes -= bytes;
#endif
        writeStep();

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    if (readReady && !decodePaused && (!raw || engine->plaintext().empty())) {
#ifndef _WIN32

        if (work.socketBytes < options.limits.inputBytes) {
            return {TcpPollStatus::YIELD, std::nullopt};
        }

        work.socketBytes -= options.limits.inputBytes;
#endif
        readStep();

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    if (!engine->plaintext().empty()) {
        if (!engine->verified()) {
            fail(diagnostic(TcpError::INVALID_CONFIGURATION, "unverified-plaintext"));

            return {TcpPollStatus::PROGRESS, std::nullopt};
        }

        if (raw) {
            const auto count = (std::min)(engine->plaintext().size(), sessionWork.bytes);

            if (sessionWork.operations == 0 || count == 0 || !raw->input().empty()) {
                return {TcpPollStatus::YIELD, std::nullopt};
            }

            --sessionWork.operations;
            const auto accepted = raw->acceptInput(engine->plaintext().first(count));
            sessionWork.bytes -= accepted;
            engine->consumePlaintext(accepted);

            return {accepted == 0 ? TcpPollStatus::YIELD : TcpPollStatus::PROGRESS, std::nullopt};
        }

        if (sessionWork.operations != 0) {
            auto result = session->feed(engine->plaintext(), sessionWork);
            engine->consumePlaintext(result.accepted);
            frameBytes += result.accepted;
            observePeak();

            if (result.status == protocol::LiveStatus::FAILED) {
                fail({TcpError::PROTOCOL_FAILURE, "session-input", 0, session->diagnostic().error});
            }

            if (result.event) {
                frameBytes = 0;

                return {TcpPollStatus::EVENT, std::move(result.event)};
            }

            return {result.accepted == 0 ? TcpPollStatus::YIELD : TcpPollStatus::PROGRESS, std::nullopt};
        }
    }

    if (!decodePaused && engine->finished()) {
        if (raw || session->finish() == protocol::LiveStatus::CLOSED) {
            outcome.peerEnded = true;
            closeRequested = true;
        } else {
            fail({TcpError::PROTOCOL_FAILURE, "session-eof", 0, session->diagnostic().error});
        }

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    if (!wireBytes().empty() && !writePending) {
#ifdef _WIN32
        const auto bytes = (std::min)(options.limits.writeBytes, wireBytes().size());

        if (work.socketBytes < bytes) {
            return {TcpPollStatus::YIELD, std::nullopt};
        }

        work.socketBytes -= bytes;
#endif
        beginWrite();

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    if (decodePaused) {
        // Keep plaintext and any completed read intact; pending ciphertext still drains above.
        return {TcpPollStatus::YIELD, std::nullopt};
    }

    if (engine->needsStep(inputBegin != inputEnd, output.has_value() || (raw && !raw->output().empty()))) {
        const auto inputBytes = std::span<const std::byte>{input.get() + inputBegin, inputEnd - inputBegin};
        const auto outputBytes = applicationBytes();
        const auto step = engine->advance(inputBytes, outputBytes);

        if (step.inputConsumed > inputBytes.size() || step.outputConsumed > outputBytes.size()) {
            fail(diagnostic(TcpError::RESOURCE_LIMIT, "engine-prefix-count"));

            return {TcpPollStatus::PROGRESS, std::nullopt};
        }

        inputBegin += step.inputConsumed;

        if (inputBegin == inputEnd) {
            inputBegin = inputEnd = 0;
        }

        if (step.outputConsumed != 0) {
            if (consumeApplication(step.outputConsumed) && output && output->pendingBytes().empty()) {
                output.reset();
            }
        }

        if (step.failure) {
            fail(*step.failure);
        }

        if (!wireBytes().empty()) {
            (void)setDeadline(writeDeadline, options.writeTimeout);
        }

        return {step.progressed ? TcpPollStatus::PROGRESS : TcpPollStatus::IDLE, std::nullopt};
    }

    if (readErrorPending && inputBegin == inputEnd) {
        readErrorPending = false;

        if (readError == asio::error::eof) {
            peerEnd();
        } else {
            fail(diagnostic(TcpError::READ_FAILURE, "socket-read", readError.value()));
        }

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    if (inputBegin == inputEnd && !readPending) {
#ifdef _WIN32

        if (work.socketBytes < options.limits.inputBytes) {
            return {TcpPollStatus::YIELD, std::nullopt};
        }

        work.socketBytes -= options.limits.inputBytes;
#endif
        beginRead();

        return {TcpPollStatus::PROGRESS, std::nullopt};
    }

    return {pending != 0 && work.handlers == 0 ? TcpPollStatus::YIELD : TcpPollStatus::IDLE, std::nullopt};
}

std::expected<TcpPollResult, TcpDiagnostic> RuntimeCore::poll(TcpWork &work, protocol::LiveWork &sessionWork) {
    auto &state = *this;

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
                                   (now >= state.readDeadline || ((state.output || !state.applicationBytes().empty() ||
                                                                   !state.wireBytes().empty()) &&
                                                                  now >= state.writeDeadline))))) {
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

        if (state.engine) {
            return state.pollEngine(work, sessionWork);
        }

        if (state.writeReady) {
#ifndef _WIN32
            const auto bytes = (std::min)(state.options.limits.writeBytes, state.wireBytes().size());

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
        } else if (state.inputBegin != state.inputEnd && sessionWork.operations != 0) {
            if (auto *raw = state.session.raw()) {
                const auto count = (std::min)(state.inputEnd - state.inputBegin, sessionWork.bytes);

                if (count == 0 || !raw->input().empty()) {
                    return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
                }

                --sessionWork.operations;
                const auto accepted = raw->acceptInput({state.input.get() + state.inputBegin, count});
                sessionWork.bytes -= accepted;
                state.inputBegin += accepted;

                if (state.inputBegin == state.inputEnd) {
                    state.inputBegin = state.inputEnd = 0;
                }

                return TcpPollResult{accepted == 0 ? TcpPollStatus::YIELD : TcpPollStatus::PROGRESS, std::nullopt};
            }

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
        } else if (state.readErrorPending && state.inputBegin == state.inputEnd) {
            state.readErrorPending = false;

            if (state.readError == asio::error::eof) {
                state.peerEnd();
            } else {
                state.fail(diagnostic(TcpError::READ_FAILURE, "socket-read", state.readError.value()));
            }
        } else if ((state.output || !state.applicationBytes().empty()) && !state.writePending) {
#ifdef _WIN32
            const auto bytes = (std::min)(state.options.limits.writeBytes, state.wireBytes().size());

            if (work.socketBytes < bytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= bytes;
#endif
            state.beginWrite();
        } else if (state.inputBegin == state.inputEnd && !state.readPending &&
                   (!state.session.raw() || state.session.raw()->input().empty())) {
#ifdef _WIN32

            if (work.socketBytes < state.options.limits.inputBytes) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

            work.socketBytes -= state.options.limits.inputBytes;
#endif
            state.beginRead();
        } else {
            if (state.inputBegin != state.inputEnd) {
                return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
            }

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

std::expected<void, TcpDiagnostic> RuntimeCore::composeProtocol(protocol::LiveProtocolView descriptor,
                                                                protocol::LiveWork &work) {
    if (const auto error = this->outputRejection()) {
        return std::unexpected(*error);
    }

    return this->adoptOutput(this->session->composeProtocol(descriptor, work));
}

std::expected<void, TcpDiagnostic> RuntimeCore::composeHeartbeat(protocol::LiveHeartbeat heartbeat,
                                                                 protocol::LiveWork &work) {
    if (const auto error = this->outputRejection()) {
        return std::unexpected(*error);
    }

    return this->adoptOutput(this->session->composeHeartbeat(heartbeat, work));
}

std::expected<void, TcpDiagnostic> RuntimeCore::describe(std::span<const std::int32_t> senderIds,
                                                         protocol::LiveWork &work) {
    if (const auto error = this->outputRejection()) {
        return std::unexpected(*error);
    }

    return this->adoptOutput(this->session->describe(senderIds, work));
}

std::expected<void, TcpDiagnostic> RuntimeCore::subscribe(protocol::SubscriptionMessage message,
                                                          std::span<const core::SubscriptionRequest> requests,
                                                          protocol::LiveWork &work) {
    if (const auto error = this->outputRejection()) {
        return std::unexpected(*error);
    }

    return this->adoptOutput(this->session->subscribe(message, requests, work));
}

std::expected<void, TcpDiagnostic> RuntimeCore::requestPause() noexcept {
    if (const auto error = wrongThread()) {
        return std::unexpected(*error);
    }

    // Never reclassify a failure/close already observed before this reusable plain intent.
    if (!closeRequested && !terminalPublished) {
        pauseCancellation = true;
    }

    return requestClose();
}

std::expected<void, TcpDiagnostic> RuntimeCore::requestClose() noexcept {
    if (const auto error = this->wrongThread()) {
        return std::unexpected(*error);
    }

    if (!this->terminalPublished) {
        this->outcome.aborted = true;
        this->closeRequested = true;
        this->applyClose();
    }

    return {};
}

TcpState RuntimeCore::state() const noexcept {
    return this->runState;
}

std::optional<TcpTerminal> RuntimeCore::terminal() const noexcept {
    return this->terminalPublished ? std::optional(this->outcome) : std::nullopt;
}

protocol::LiveSnapshot RuntimeCore::peer() const noexcept {
    return this->session->peer();
}

protocol::LiveAuthentication RuntimeCore::authentication() const noexcept {
    return this->session->authentication();
}

TcpResources RuntimeCore::resources() noexcept {
    const auto current = this->session ? this->session->residentBytes() : 0;
    const auto dns = this->dns.envelopeBytes();
    this->observePeak();

    return {this->adapterBytes,
            dns,
            current,
            this->adapterBytes + dns + current,
            this->peakBytes,
            this->pending + (this->dns.pending() ? 1 : 0) + (this->connecting ? 1 : 0)};
}

std::optional<Clock::time_point> RuntimeCore::nextWake() const noexcept {
    return this->wake();
}

TcpWork RuntimeCore::dnsProgressWorkRequirement() const noexcept {
    return this->namedEndpoint ? this->dns.workRequirement() : TcpWork{};
}

} // namespace org::ttldtor::qdcxx::transport::detail
