// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_url_reader.hpp"

#include "../transport/tls_byte_owner.hpp"
#include "live_transfer.hpp"
#include "transfer_control.hpp"
#include "transfer_file.hpp"
#include "transfer_native.hpp"
#include "transfer_response.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/url/parse.hpp>

#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>

namespace org::ttldtor::qdcxx::ipf {
namespace {

namespace td = transport::detail;
using Clock = std::chrono::steady_clock;

/** Preserve the first detached failure.
 * @param result Stable owning result.
 * @param diagnostic Static/numeric cause.
 */
void fail(IpfTransferResult &result, IpfTransferDiagnostic diagnostic) noexcept {
    if (!result.error) {
        result.error = diagnostic;
    }
}

/** Check the positive native duration domain.
 * @param value Explicit millisecond duration.
 * @return True in1..INT_MAX.
 */
bool validDuration(std::chrono::milliseconds value) noexcept {
    return value.count() > 0 && value.count() <= INT_MAX;
}

/** Allocator used only for the stable transfer host/control block, with no unbounded fallback.
 * @tparam T Standard allocate_shared's rebound storage type.
 */
template <class T> class TransferHostAllocator {
    public:
    using value_type = T;
    std::size_t maximum; ///< Whole pre-admitted allocation capacity, including the library control block.

    /** Retain the exact admitted cap.
     * @param limit Whole requested-byte maximum.
     */
    explicit TransferHostAllocator(std::size_t limit) noexcept : maximum(limit) {
    }

    /** Preserve the same cap through allocate_shared's internal rebind.
     * @tparam U Original storage type.
     * @param other Allocator with the same operation cap.
     */
    template <class U> TransferHostAllocator(const TransferHostAllocator<U> &other) noexcept : maximum(other.maximum) {
    }

    /** Allocate only within the already reserved complete host/control-block cap.
     * @param count Requested element count.
     * @return Standard allocated storage; exceeding the cap throws length_error before allocation.
     */
    T *allocate(std::size_t count) {
        if (count > maximum / sizeof(T)) {
            throw std::length_error("transfer-host-capacity");
        }

        return std::allocator<T>{}.allocate(count);
    }

    /** Release exactly the matching standard allocation.
     * @param memory Previously allocated storage.
     * @param count Original element count.
     */
    void deallocate(T *memory, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(memory, count);
    }

    /** Compare the exact immutable cap.
     * @tparam U Other rebound type.
     * @param other Allocator to compare.
     * @return True for interchangeable capped allocation domains.
     */
    template <class U> bool operator==(const TransferHostAllocator<U> &other) const noexcept {
        return maximum == other.maximum;
    }
};

inline constexpr std::size_t HOST_CONTROL_BYTES =
    256; ///< Explicit shared-control/allocator slack, guarded before allocation.

/** Validate before owned settings copies or SDK preparation.
 * @param options Borrowed settings.
 * @return Static refusal, empty for eligible settings.
 */
std::optional<IpfTransferDiagnostic> validateOptions(const IpfTransferOptions &options) noexcept {
    const auto &limits = options.limits;
    const auto &ipf = options.ipf.turnWork;

    if (limits.sourceBytes == 0 || limits.sourceBytes > detail::SOURCE_CAPACITY ||
        limits.locationBytes > detail::SOURCE_CAPACITY || limits.authorizationBytes == 0 ||
        limits.authorizationBytes > detail::SOURCE_CAPACITY || limits.headerBytes == 0 ||
        limits.headerBytes > UINT32_MAX || limits.trailerBytes < 2 || limits.trailerBytes > UINT32_MAX - 2 ||
        limits.inputBytes == 0 || limits.requestBytes == 0 || limits.headerFields == 0 ||
        options.dnsServers.size() > 4 || options.dnsPort == 0 || ipf.inputBytes == 0 || ipf.decodedUnits == 0 ||
        ipf.fields == 0 || ipf.records == 0 || !validDuration(options.overallTimeout) ||
        !validDuration(options.connectTimeout) || !validDuration(options.readTimeout) ||
        !validDuration(options.writeTimeout) || !validDuration(options.pollInterval) ||
        options.pollInterval > std::chrono::milliseconds(100) || !validDuration(options.handshakeTimeout) ||
        !validDuration(options.closeTimeout) || options.nativeLimits.inputBytes == 0 ||
        options.nativeLimits.writeBytes == 0 || options.trustPem.size() > options.tls.limits.trustBytes ||
        options.trustPem.size() > 524288 || !options.tls.dns.servers.empty()) {
        return IpfTransferDiagnostic{IpfTransferError::INVALID_OPTIONS, "transfer-options"};
    }

    std::size_t backing = sizeof(IpfTransferOptions) + 2 * sizeof(detail::TransferSource);

    for (const auto bytes : {limits.inputBytes, limits.requestBytes, limits.authorizationBytes, options.trustPem.size(),
                             options.dnsServers.size() * sizeof(transport::DnsServer)}) {
        if (!detail::charge(backing, bytes, limits.residentBytes)) {
            return IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "transfer-backing"};
        }
    }

    return std::nullopt;
}

/** Publish outcome after retirement without conflating HTTP/IPF evidence.
 * @param result Stable owning output.
 */
void publish(IpfTransferResult &result) noexcept {
    if (result.error) {
        result.status = result.error->error == IpfTransferError::CANCELLED ? IpfTransferStatus::CANCELLED
                                                                           : IpfTransferStatus::FAILED;
        result.metadataCommitted = false;
    } else if (result.status != IpfTransferStatus::NOT_MODIFIED) {
        result.status = IpfTransferStatus::SUCCESS;
        result.metadataCommitted = result.httpStatus == 200;
    }
}

} // namespace

struct InstrumentProfileReader::Impl {
    std::optional<transport::RuntimeBootstrap> ownedBootstrap; ///< Released after child and outer lease.
    std::optional<td::BootstrapLease> bootstrapLease;          ///< Stable borrow across idle gaps.
    IpfTransferOptions options;                                ///< Validated owned settings.
    transport::TlsWork tlsWork{}; ///< Reader-lifetime credits, never reset by redirects/loads.
    const std::thread::id owner = std::this_thread::get_id(); ///< SDK affinity.
    bool live = true;                                         ///< Public lifetime distinct from operation lease.
    bool busy = false;                                        ///< Serialized load guard.
    bool cancelRequested = false;                             ///< Creator-thread request-only cutoff.

    /** Copy validated settings and lifetime work shape.
     * @param input Caller settings, already validated.
     */
    explicit Impl(const IpfTransferOptions &input) : options(input) {
        options.tls.dns = {};

        if (!options.trustPem.empty()) {
            const auto requirements = transport::tlsRequirements(options.tls.limits, options.tls.host);

            if (requirements) {
                tlsWork.shape = requirements->shape;
                tlsWork.remaining = options.tlsWork;
            }
        }
    }

    /** Release after every operation lease, on the SDK creator. */
    ~Impl() {
        if (owner != std::this_thread::get_id() || busy) {
            std::terminate();
        }
    }

    /** Stable synchronous operation, independent of facade address/lifetime. */
    template <class ProfilePipe = detail::TransferProfiles> struct Operation {
        std::shared_ptr<Impl> host;         ///< Strong lease across user code/drain.
        std::stop_token stop;               ///< Thread-safe request observation only.
        IpfTransferResult result;           ///< Detached prefix and first cause.
        detail::TransferWork work;          ///< Whole-load HTTP counters.
        ProfilePipe profiles;               ///< Existing incremental IPF parser.
        transport::TcpWork cleanupWork{};   ///< Original native grant partition reserved only for retirement.
        transport::TcpWork nativeWork;      ///< Whole-load native credits.
        protocol::LiveWork byteWork;        ///< Whole-load copy/preparation credits.
        std::unique_ptr<std::byte[]> input; ///< Fixed contiguous staging.
        std::unique_ptr<char[]> output;     ///< Whole immutable request staging.
        std::unique_ptr<char[]> secret;     ///< Fixed provider destination.
        std::optional<HttpAuthorization> authorization;      ///< Independent secret, no full URL owner.
        std::shared_ptr<HttpAuthorizationProvider> provider; ///< Stable callback lease.
        std::optional<std::chrono::sys_seconds> condition;   ///< Explicit conditional metadata.
        std::unique_ptr<td::RawByteOwner> plain;             ///< Sole plain native child.
        std::unique_ptr<td::TlsByteOwner> tls;               ///< Sole verified TLS child.
        Clock::time_point deadline;                          ///< Whole-load deadline.
        std::size_t turns = 0;                               ///< Cumulative turns.
        std::size_t attempts = 0;                            ///< Cumulative connections.
        std::size_t inputSize = 0;                           ///< Initialized HTTP staging.
        bool nativeFailure = false;                          ///< Retained native first cause.
        bool authAllowed = true;                             ///< Cut permanently on cross-origin redirect.

        /** Prepare bounded storage and request snapshot.
         * @param state Stable host lease.
         * @param request Checked explicit request.
         * @param token Safe request-only cancellation.
         */
        Operation(std::shared_ptr<Impl> state, const IpfUrlRequest &request, std::stop_token token)
            : host(std::move(state)), stop(token), profiles(host->options.ipf, result, work, host->options.limits),
              nativeWork(host->options.nativeWork), byteWork(host->options.byteWork),
              input(std::make_unique<std::byte[]>(host->options.limits.inputBytes)),
              output(std::make_unique<char[]>(host->options.limits.requestBytes)),
              secret(std::make_unique<char[]>(host->options.limits.authorizationBytes)),
              authorization(request.authorization), provider(request.provider), condition(request.ifModifiedSince),
              deadline(Clock::now() + host->options.overallTimeout) {
            cleanupWork.steps = (std::min)(nativeWork.steps, host->options.limits.cleanupTurns);
            cleanupWork.handlers = (std::min)(nativeWork.handlers, host->options.limits.cleanupTurns);
            nativeWork.steps -= cleanupWork.steps;
            nativeWork.handlers -= cleanupWork.handlers;
            result.nativeRetired = true;
        }

        /** Drain children before callback/secret/input/bootstrap backing retires. */
        ~Operation() {
            retire(false);
        }

        /** Deliver the single staged item outside parser/native frames.
         * @return True after sink admission.
         */
        bool publishItems() {
            if constexpr (std::is_same_v<ProfilePipe, detail::StreamingProfiles>) {
                if (!profiles.publish()) {
                    fail(result, profiles.consumer().refusal().value_or(
                                     IpfTransferDiagnostic{IpfTransferError::CANCELLED, "connection-item-cutoff"}));

                    return false;
                }
            }

            return true;
        }

        /** Observe cutoff and debit one finite internal turn.
         * @return True for permitted ordinary work.
         */
        bool checkpoint() {
            if constexpr (std::is_same_v<ProfilePipe, detail::StreamingProfiles>) {
                if (!profiles.consumer().checkpoint()) {
                    fail(result, profiles.consumer().refusal().value_or(
                                     IpfTransferDiagnostic{IpfTransferError::CANCELLED, "connection-cutoff"}));

                    return false;
                }
            }

            if (detail::transferCancelled(host->live, host->cancelRequested, stop)) {
                fail(result, {IpfTransferError::CANCELLED, "transfer-cancelled"});

                return false;
            }

            if (Clock::now() >= deadline) {
                fail(result, {IpfTransferError::DEADLINE, "transfer-deadline"});

                return false;
            }

            if (!detail::charge(turns, 1, host->options.limits.turns)) {
                fail(result, {IpfTransferError::WORK_LIMIT, "transfer-turns"});

                return false;
            }

            return true;
        }

        /** Borrow the one current native operation.
         * @return Stable child, or null before/after native ownership.
         */
        td::RawByteOwner *bytes() noexcept {
            return tls ? &tls->bytes() : plain.get();
        }

        /** Observe first lower-layer cause without SDK calls.
         * @return False after a native/TLS failure.
         */
        bool observeNative() {
            if (tls) {
                const auto diagnostic = tls->diagnostic();

                if (diagnostic.error != transport::TlsError::NONE) {
                    fail(result, {IpfTransferError::TLS_FAILURE, "transfer-tls", diagnostic.transport, diagnostic});
                    nativeFailure = true;
                }
            }

            if (const auto child = bytes()) {
                const auto diagnostic = child->core().outcome.primary;

                if (diagnostic.error != transport::TcpError::NONE) {
                    fail(result, {IpfTransferError::TRANSPORT_FAILURE, "transfer-native", diagnostic});
                    nativeFailure = true;
                }
            }

            return !nativeFailure;
        }

        /** Perform one bounded B action without replenishing cumulative grants.
         * @return True for an admitted owner action; nativeFailure separately retains its first cause.
         */
        bool poll() {
            if (tls) {
                const auto progress = tls->poll(nativeWork, byteWork, host->tlsWork);

                if (!progress) {
                    fail(result,
                         {progress.error().error == transport::TlsError::WORK_LIMIT ? IpfTransferError::WORK_LIMIT
                                                                                    : IpfTransferError::TLS_FAILURE,
                          "transfer-tls-poll", progress.error().transport, progress.error()});

                    return false;
                }

                if (progress->status == transport::TcpPollStatus::YIELD) {
                    (void)observeNative();

                    if (bytes()->core().closeRequested) {
                        return true;
                    }

                    fail(result, {IpfTransferError::WORK_LIMIT, "transfer-progress-work"});

                    return false;
                }

                if (progress->status == transport::TcpPollStatus::IDLE) {
                    std::this_thread::sleep_for(host->options.pollInterval);
                }
            } else {
                const auto progress = plain->poll(nativeWork, byteWork);

                if (!progress) {
                    fail(result, {progress.error().error == transport::TcpError::WORK_LIMIT
                                      ? IpfTransferError::WORK_LIMIT
                                      : IpfTransferError::TRANSPORT_FAILURE,
                                  "transfer-poll", progress.error()});

                    return false;
                }

                if (progress->status == transport::TcpPollStatus::YIELD) {
                    (void)observeNative();

                    if (bytes()->core().closeRequested) {
                        return true;
                    }

                    fail(result, {IpfTransferError::WORK_LIMIT, "transfer-progress-work"});

                    return false;
                }

                if (progress->status == transport::TcpPollStatus::IDLE) {
                    std::this_thread::sleep_for(host->options.pollInterval);
                }
            }

            if (bytes()->core().state() == transport::TcpState::CONNECTING) {
                std::this_thread::sleep_for(host->options.pollInterval);
            }

            (void)observeNative();

            return true;
        }

        /** Complete orderly TLS when allowed, then perform existing B native fallback retirement.
         * @param orderly True after a complete HTTP message without earlier failure.
         */
        void retire(bool orderly) noexcept {
            if (!bytes()) {
                return;
            }

            bool closing = false;

            if (tls && orderly && !result.error) {
                while (bytes()->core().state() != transport::TcpState::CLOSED) {
                    if (!checkpoint()) {
                        break;
                    }

                    if (!closing && bytes()->pendingOutput() == 0) {
                        const auto close = tls->requestTlsClose();

                        if (close) {
                            closing = true;
                        } else if (close.error().error != transport::TlsError::INVALID_STATE) {
                            fail(result, {IpfTransferError::TLS_FAILURE, "transfer-tls-close", close.error().transport,
                                          close.error()});

                            break;
                        }
                    }

                    if (!poll() || nativeFailure) {
                        break;
                    }
                }
            }

            if (tls) {
                (void)tls->requestClose();
            } else {
                (void)plain->requestClose();
            }

            for (std::size_t turn = 0; bytes()->core().state() != transport::TcpState::CLOSED &&
                                       turn < host->options.limits.cleanupTurns && cleanupWork.steps != 0;
                 ++turn) {
                if (tls) {
                    const auto progress = tls->poll(cleanupWork, byteWork, host->tlsWork);

                    if (!progress) {
                        fail(result, {IpfTransferError::TLS_FAILURE, "transfer-cleanup", progress.error().transport,
                                      progress.error()});

                        break;
                    }

                    if (progress->status == transport::TcpPollStatus::IDLE) {
                        std::this_thread::sleep_for(host->options.pollInterval);
                    }
                } else {
                    const auto progress = plain->poll(cleanupWork, byteWork);

                    if (!progress) {
                        fail(result, {IpfTransferError::TRANSPORT_FAILURE, "transfer-cleanup", progress.error()});

                        break;
                    }

                    if (progress->status == transport::TcpPollStatus::IDLE) {
                        std::this_thread::sleep_for(host->options.pollInterval);
                    }
                }

                (void)observeNative();
            }

            if (bytes()->core().state() != transport::TcpState::CLOSED) {
                fail(result, {IpfTransferError::WORK_LIMIT, "transfer-cleanup-work"});
            }

            // B owns its original fallback drain/disposal reserve. Exhaustion never creates new credits.
            (void)observeNative();
            tls.reset();
            plain.reset();
            result.nativeRetired = true;
        }

        /** Prepare and activate the sole current transport.
         * @param source Stable sanitized route/reference.
         * @return True after native connection and, for HTTPS, verified identity.
         */
        bool connect(const detail::TransferSource &source) {
            if (!checkpoint() || !detail::charge(attempts, 1, host->options.limits.attempts)) {
                if (!result.error) {
                    fail(result, {IpfTransferError::RESOURCE_LIMIT, "http-attempts"});
                }

                return false;
            }

            nativeFailure = false;
            inputSize = 0;
            const auto &config = host->options;
            transport::TcpOptions native{source.hostname(),  source.port,           config.dnsServers,
                                         config.dnsPort,     config.connectTimeout, config.connectTimeout,
                                         config.readTimeout, config.writeTimeout,   config.pollInterval,
                                         config.nativeLimits};
            const td::RawByteLimits windows{config.nativeLimits.inputBytes, config.limits.requestBytes};

            if (source.secure) {
                if (config.trustPem.empty()) {
                    fail(result, {IpfTransferError::INVALID_OPTIONS, "https-explicit-trust"});

                    return false;
                }

                client::EndpointTls settings;
                settings.referenceIdentity.assign(source.hostname());
                settings.trustPem = config.trustPem;
                settings.handshakeTimeout = config.handshakeTimeout;
                settings.closeTimeout = config.closeTimeout;
                auto made = td::TlsByteOwner::create(*host->bootstrapLease, native, windows, settings, config.tls,
                                                     byteWork, host->tlsWork);

                if (!made) {
                    fail(result, {IpfTransferError::TLS_FAILURE, "https-create", made.error().transport, made.error()});

                    return false;
                }

                tls = std::move(*made);
                result.nativeRetired = false;
                const auto started = tls->start();

                if (!started) {
                    fail(result,
                         {IpfTransferError::TLS_FAILURE, "https-start", started.error().transport, started.error()});

                    return false;
                }
            } else {
                auto made = td::RawByteOwner::create(*host->bootstrapLease, native, windows, byteWork);

                if (!made) {
                    fail(result, {IpfTransferError::TRANSPORT_FAILURE, "http-create", made.error()});

                    return false;
                }

                plain = std::move(*made);
                result.nativeRetired = false;
                const auto started = plain->start();

                if (!started) {
                    fail(result, {IpfTransferError::TRANSPORT_FAILURE, "http-start", started.error()});

                    return false;
                }
            }

            while (checkpoint()) {
                const auto &core = bytes()->core();

                if (core.state() == transport::TcpState::CONNECTED &&
                    (!source.secure || (core.engine && core.engine->verified()))) {
                    return true;
                }

                if (core.state() == transport::TcpState::CLOSED || !poll() || nativeFailure) {
                    break;
                }
            }

            (void)observeNative();

            if (!result.error) {
                fail(result, {IpfTransferError::TRANSPORT_FAILURE, "connection-incomplete"});
            }

            return false;
        }

        /** Invoke only a verified still-live intended provider, outside owner entry.
         * @param source Current verified origin.
         * @return Stable owned secret view, or empty for anonymous/refusal.
         */
        std::string_view obtainAuthorization(const detail::TransferSource &source) {
            if (!authAllowed) {
                return {};
            }

            if (!provider) {
                return authorization ? authorization->value() : std::string_view{};
            }

            if (!source.secure || !bytes()->core().engine || !bytes()->core().engine->verified()) {
                fail(result, {IpfTransferError::AUTHORIZATION_FAILURE, "authorization-verification"});

                return {};
            }

            std::fill_n(secret.get(), host->options.limits.authorizationBytes, '\0');
            std::expected<std::size_t, HttpAuthorizationError> supplied =
                std::unexpected(HttpAuthorizationError::UNAVAILABLE);

            try {
                supplied = provider->authorization({"https", source.hostname(), source.port, attempts},
                                                   {secret.get(), host->options.limits.authorizationBytes});
            } catch (...) {
                supplied = std::unexpected(HttpAuthorizationError::UNAVAILABLE);
            }

            // Only stable operation state is touched after user code; cutoff wins before publication.
            if (!checkpoint()) {
                return {};
            }

            if (!supplied || *supplied > host->options.limits.authorizationBytes ||
                !detail::validAuthorization({secret.get(), supplied ? *supplied : 0},
                                            host->options.limits.authorizationBytes)) {
                fail(result, {IpfTransferError::AUTHORIZATION_FAILURE, "authorization-provider"});

                return {};
            }

            return {secret.get(), *supplied};
        }

        /** Publish exactly one copied immutable request after final lifetime checks.
         * @param source Current sanitized address.
         * @return True for whole output admission.
         */
        bool sendRequest(const detail::TransferSource &source) {
            if (!checkpoint()) {
                return false;
            }

            const auto token = obtainAuthorization(source);

            if (result.error) {
                return false;
            }

            const auto parsed = boost::urls::parse_uri(source.address());
            std::size_t size = 0;
            const auto append = [&](std::string_view part) {
                if (part.size() > host->options.limits.requestBytes - size) {
                    return false;
                }

                std::memcpy(output.get() + size, part.data(), part.size());
                size += part.size();

                return true;
            };
            const auto target = parsed->encoded_target();
            const auto authority = parsed->encoded_authority();
            const auto date = condition ? detail::formatHttpDate(*condition) : std::string{};

            if (!append("GET ") || !append({target.data(), target.size()}) || !append(" HTTP/1.1\r\nHost: ") ||
                !append({authority.data(), authority.size()}) ||
                !append(std::is_same_v<ProfilePipe, detail::StreamingProfiles>
                            ? "\r\nConnection: close\r\nX-Live: yes\r\nAccept-Encoding: identity\r\n"
                            : "\r\nConnection: close\r\nX-Live: no\r\nAccept-Encoding: identity\r\n") ||
                (!date.empty() && (!append("If-Modified-Since: ") || !append(date) || !append("\r\n"))) ||
                (!token.empty() && (!append("Authorization: ") || !append(token) || !append("\r\n"))) ||
                !append("\r\n") || !detail::charge(work.presented, size, host->options.limits.presentedBytes)) {
                fail(result, {IpfTransferError::RESOURCE_LIMIT, "http-request-capacity"});

                return false;
            }

            if (!checkpoint()) {
                return false;
            }

            const auto request = std::as_bytes(std::span(output.get(), size));

            if (tls) {
                const auto sent = tls->write(request, byteWork);

                if (!sent) {
                    fail(result,
                         {IpfTransferError::TLS_FAILURE, "https-request", sent.error().transport, sent.error()});
                }
            } else {
                const auto sent = plain->write(request, byteWork);

                if (!sent) {
                    fail(result, {IpfTransferError::TRANSPORT_FAILURE, "http-request", sent.error()});
                }
            }

            return !result.error;
        }

        /** Parse current responses using one bounded contiguous window.
         * @return Owned bounded Location for redirect, empty for final/error.
         */
        std::optional<std::string> receive() {
            using Response = detail::BasicTransferResponse<ProfilePipe>;
            std::optional<Response> response;
            bool headersObserved = false;
            response.emplace(host->options.limits, work, profiles, result);
            bool needMore = true;
            bool eofDelivered = false;

            while (checkpoint()) {
                if constexpr (std::is_same_v<ProfilePipe, detail::StreamingProfiles>) {
                    if (profiles.hasOwnedInput()) {
                        auto parserWork = host->options.ipf.turnWork;
                        parserWork.records = (std::min)(parserWork.records, std::size_t{1});
                        const auto before = parserWork;
                        const auto drained = profiles.drain(parserWork);
                        const auto staged = profiles.staged();
                        const auto progressed = parserWork.inputBytes != before.inputBytes ||
                                                parserWork.decodedUnits != before.decodedUnits ||
                                                parserWork.fields != before.fields ||
                                                parserWork.records != before.records;

                        if (!publishItems() || profiles.failed()) {
                            break;
                        }

                        if (progressed || staged) {
                            continue;
                        }

                        if (!drained) {
                            fail(result, {IpfTransferError::WORK_LIMIT, "connection-owned-parser-progress"});
                            break;
                        }
                    }
                }

                auto available = bytes()->input();

                if (!available) {
                    fail(result, {IpfTransferError::TRANSPORT_FAILURE, "http-input", available.error()});

                    break;
                }

                bool cachedCodecInput = false;

                if (nativeFailure && tls && available->empty()) {
                    *available = detail::transferVerifiedInput(bytes()->core());
                    cachedCodecInput = !available->empty();
                }

                bool added = false;

                if (!available->empty() && (needMore || inputSize < host->options.limits.inputBytes)) {
                    const auto count = (std::min)(available->size(), host->options.limits.inputBytes - inputSize);

                    if (count == 0 || !detail::charge(result.wireBytes, count, host->options.limits.wireBytes) ||
                        !detail::charge(work.presented, count, host->options.limits.presentedBytes)) {
                        fail(result, {IpfTransferError::RESOURCE_LIMIT, "http-input-capacity"});

                        break;
                    }

                    std::memcpy(input.get() + inputSize, available->data(), count);
                    inputSize += count;

                    if (cachedCodecInput) {
                        bytes()->core().engine->consumePlaintext(count);
                    } else if (tls) {
                        (void)tls->consumeInput(count);
                    } else {
                        (void)plain->consumeInput(count);
                    }

                    added = true;
                }

                if (inputSize != 0 && (!needMore || added)) {
                    if (!detail::charge(work.calls, 1, host->options.limits.parserCalls) ||
                        !detail::charge(work.presented, inputSize, host->options.limits.presentedBytes)) {
                        fail(result, {IpfTransferError::WORK_LIMIT, "http-parser-work"});

                        break;
                    }

                    response->ipfWork = host->options.ipf.turnWork;
                    const auto beforeBody = work.body;
                    boost::system::error_code error;
                    const auto consumed = response->put(boost::asio::buffer(input.get(), inputSize), error);
                    const auto body = work.body - beforeBody;

                    if (consumed < body ||
                        !detail::charge(work.framing, consumed - body, host->options.limits.framingBytes) ||
                        !detail::charge(work.presented, inputSize - consumed, host->options.limits.presentedBytes)) {
                        fail(result, {IpfTransferError::WORK_LIMIT, "http-framing-work"});

                        break;
                    }

                    std::memmove(input.get(), input.get() + consumed, inputSize - consumed);
                    inputSize -= consumed;
                    needMore = error == boost::beast::http::error::need_more;

                    if (!publishItems()) {
                        break;
                    }

                    if constexpr (std::is_same_v<ProfilePipe, detail::StreamingProfiles>) {
                        if (!headersObserved && response->is_header_done() && response->status == 200) {
                            headersObserved = true;
                            const auto modified =
                                result.lastModified
                                    ? std::optional(
                                          std::chrono::time_point_cast<std::chrono::milliseconds>(*result.lastModified))
                                    : std::nullopt;
                            const auto action = profiles.consumer().headers(result, modified);

                            if (action == connection_detail::HeaderAction::SKIP) {
                                profiles.skip();
                                result.status = IpfTransferStatus::NOT_MODIFIED;

                                return std::nullopt;
                            }

                            if (action == connection_detail::HeaderAction::REJECT || !checkpoint()) {
                                fail(result, {IpfTransferError::CANCELLED, "connection-header-cutoff"});
                                break;
                            }
                        }
                    }

                    if (error && !needMore) {
                        if (!result.error) {
                            fail(result, {IpfTransferError::HTTP_FAILURE, "http-parse"});
                        }

                        break;
                    }

                    if (response->is_done()) {
                        if (response->status >= 100 && response->status < 200) {
                            if (!detail::charge(work.informationals, 1, host->options.limits.informationalResponses)) {
                                fail(result, {IpfTransferError::RESOURCE_LIMIT, "http-informational-count"});

                                break;
                            }

                            headersObserved = false;
                            response.reset();
                            response.emplace(host->options.limits, work, profiles, result);
                            needMore = false;

                            continue;
                        }

                        break;
                    }

                    if (result.error && !nativeFailure) {
                        break;
                    }

                    if (consumed != 0 || !needMore) {
                        continue;
                    }
                }

                const auto &core = bytes()->core();

                if (nativeFailure) {
                    if (detail::transferHasOwnedInput(core)) {
                        const auto drained = detail::drainTransferPlainInput(*plain, nativeWork, byteWork);

                        if (!drained || drained->status != transport::TcpPollStatus::PROGRESS) {
                            break;
                        }

                        continue;
                    }

                    // No new read/output/provider is admitted after this error-owned prefix drains.
                    break;
                }

                if (core.state() == transport::TcpState::CLOSED || core.outcome.peerEnded) {
                    if (!eofDelivered && response->got_some()) {
                        if (!detail::charge(work.calls, 1, host->options.limits.parserCalls)) {
                            fail(result, {IpfTransferError::WORK_LIMIT, "http-eof-work"});

                            break;
                        }

                        eofDelivered = true;
                        boost::system::error_code error;
                        response->put_eof(error);

                        if (error || inputSize != 0) {
                            fail(result, {IpfTransferError::HTTP_FAILURE, "http-eof"});
                        }
                    } else if (!response->is_done()) {
                        fail(result, {IpfTransferError::HTTP_FAILURE, "http-empty-eof"});
                    }

                    break;
                }

                if (!poll()) {
                    break;
                }
            }

            if (!response->is_done()) {
                return std::nullopt;
            }

            result.httpFinished = true;

            if (result.error) {
                return std::nullopt;
            }

            if (inputSize != 0) {
                fail(result, {IpfTransferError::HTTP_FAILURE, "http-extra-input"});

                return std::nullopt;
            }

            if (response->status == 301 || response->status == 302 || response->status == 303 ||
                response->status == 307 || response->status == 308) {
                if (response->location().empty()) {
                    fail(result, {IpfTransferError::REDIRECT_REJECTED, "http-missing-location"});

                    return std::nullopt;
                }

                return std::string(response->location());
            }

            if (response->status == 304 && condition) {
                result.status = IpfTransferStatus::NOT_MODIFIED;

                return std::nullopt;
            }

            if (response->status != 200) {
                fail(result, {IpfTransferError::HTTP_STATUS, "http-status"});

                return std::nullopt;
            }

            while (checkpoint()) {
                auto parserWork = host->options.ipf.turnWork;

                const auto finished = profiles.finish(parserWork);

                if (!publishItems() || finished) {
                    break;
                }
            }

            return std::nullopt;
        }

        /** Drive an owned local file through the same finite IPF collector.
         * @param source Stable decoded local UTF-8 path.
         */
        void readLocal(const detail::TransferSource &source) {
            auto file = detail::TransferFile::open(source.address());

            if (!file) {
                fail(result, file.error());

                return;
            }

            if constexpr (std::is_same_v<ProfilePipe, detail::StreamingProfiles>) {
                const auto modified = file->modified();

                if (modified) {
                    result.lastModified = std::chrono::floor<std::chrono::seconds>(*modified);
                }

                const auto action = profiles.consumer().headers(result, modified);

                if (action == connection_detail::HeaderAction::SKIP) {
                    profiles.skip();
                    result.status = IpfTransferStatus::NOT_MODIFIED;

                    return;
                }

                if (action == connection_detail::HeaderAction::REJECT || !checkpoint()) {
                    return;
                }
            }

            while (checkpoint()) {
                const auto read =
                    file->read(std::span(input.get(), (std::min)(host->options.limits.inputBytes, std::size_t(4096))));

                if (!read) {
                    fail(result, read.error());

                    break;
                }

                const auto count = *read;

                if (!detail::charge(result.wireBytes, count, host->options.limits.wireBytes) ||
                    !detail::charge(work.presented, count, host->options.limits.presentedBytes)) {
                    fail(result, {IpfTransferError::RESOURCE_LIMIT, "source-input-limit"});

                    break;
                }

                std::size_t consumed = 0;

                while (consumed < count && checkpoint()) {
                    auto parserWork = host->options.ipf.turnWork;
                    consumed += profiles.feed(std::span(input.get(), count).subspan(consumed), parserWork);

                    if (!publishItems()) {
                        break;
                    }

                    if (result.error) {
                        break;
                    }
                }

                if (result.error) {
                    break;
                }

                if (count == 0) {
                    while (checkpoint()) {
                        auto parserWork = host->options.ipf.turnWork;

                        const auto finished = profiles.finish(parserWork);

                        if (!publishItems() || finished) {
                            break;
                        }
                    }

                    break;
                }
            }

            result.nativeRetired = true;
        }

        /** Execute the source and bounded redirects before final retirement.
         * @param source Borrowed initial source, consumed before application code.
         */
        void run(std::string_view source) {
            if (!checkpoint()) {
                return;
            }

            auto current = detail::parseTransferSource(source, host->options.limits, !authorization && !provider);

            if (!current) {
                fail(result, current.error());

                return;
            }

            if (!authorization && !provider) {
                authorization = std::move(current->userinfo);
                current->userinfo.reset();
            }

            if (current->local) {
                if (authorization || provider || condition) {
                    fail(result, {IpfTransferError::INVALID_OPTIONS, "local-http-options"});

                    return;
                }

                readLocal(*current);

                return;
            }

            if (!current->secure && (authorization || provider)) {
                fail(result, {IpfTransferError::AUTHORIZATION_FAILURE, "plaintext-authorization"});

                return;
            }

            while (checkpoint()) {
                if (!connect(*current) || !sendRequest(*current)) {
                    break;
                }

                const auto location = receive();
                retire(result.httpFinished && !result.error);

                if (result.error || !location) {
                    break;
                }

                if (!detail::charge(result.redirects, 1, host->options.limits.redirects)) {
                    fail(result, {IpfTransferError::REDIRECT_REJECTED, "redirect-count"});

                    break;
                }

                auto next = detail::resolveTransferSource(*current, *location, host->options.limits);

                if (!next) {
                    fail(result, next.error());

                    break;
                }

                if (!detail::sameOrigin(*current, *next)) {
                    authAllowed = false;
                    authorization.reset();
                    provider.reset();
                    condition.reset();
                }

                current = std::move(next);
                result.httpFinished = false;
            }

            retire(false);
        }
    };

    /** Compute the complete transfer-owned reservation before host/callback/native allocation.
     * IPF logical output/parser, native/TLS/process scope and allocator bookkeeping remain separate.
     * @param options Validated finite settings.
     * @return Exact declared reservation refusal, empty when all simultaneous owned capacities fit.
     */
    static std::optional<IpfTransferDiagnostic> validateBacking(const IpfTransferOptions &options,
                                                                std::size_t extra = 0) noexcept {
        std::size_t total = sizeof(Impl) + HOST_CONTROL_BYTES + sizeof(InstrumentProfileReader) + sizeof(Operation<>) +
                            sizeof(detail::TransferResponse) + detail::transferSourceWorkingBytes() +
                            detail::TRANSFER_FILE_PATH_BYTES + sizeof(detail::TransferFile);
        const auto inlineCapacity = std::string{}.capacity();

        for (const auto bytes :
             {options.limits.inputBytes, options.limits.requestBytes, options.limits.authorizationBytes,
              options.limits.authorizationBytes, options.trustPem.size(), options.limits.envelope.inflaterBytes,
              // Host options and temporary caller EndpointTls coexist with B's independently funded snapshot.
              options.trustPem.size(), sizeof(client::EndpointTls), sizeof(detail::TransferSource{}.host),
              std::size_t(2 * (inlineCapacity + 1)), std::size_t(4 * sizeof(transport::DnsServer)),
              std::size_t(4 * inlineCapacity + 128)}) {
            if (!detail::charge(total, bytes, options.limits.residentBytes)) {
                return IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "transfer-resident"};
            }
        }

        if (extra != 0 && !detail::charge(total, extra, options.limits.residentBytes)) {
            return IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "connection-stream-backing"};
        }

        return std::nullopt;
    }

    /** Keep host state alive through user callbacks and all operation retirement.
     * @param state Strong self lease captured before user code.
     * @param source Initial borrowed source.
     * @param request Explicit bounded auth/conditional input.
     * @param stop Cross-thread request-only cancellation.
     * @return Owning prefix and detached first cause after retirement.
     */
    static IpfTransferResult load(std::shared_ptr<Impl> state, std::string_view source, const IpfUrlRequest &request,
                                  std::stop_token stop) {
        IpfTransferResult early;
        early.nativeRetired = true;

        if (state->owner != std::this_thread::get_id()) {
            fail(early, {IpfTransferError::WRONG_THREAD, "reader-owner"});

            return early;
        }

        if (!state->live || state->busy) {
            fail(early, {IpfTransferError::INVALID_STATE, "reader-busy"});

            return early;
        }

        if (stop.stop_requested()) {
            fail(early, {IpfTransferError::CANCELLED, "transfer-pre-cancelled"});
            publish(early);

            return early;
        }

        if ((request.provider && request.authorization) || (!request.provider && request.provider.use_count() != 0) ||
            (request.authorization &&
             !detail::validAuthorization(request.authorization->value(), state->options.limits.authorizationBytes)) ||
            (request.ifModifiedSince && !detail::httpDateRepresentable(*request.ifModifiedSince))) {
            fail(early, {IpfTransferError::INVALID_OPTIONS, "reader-request"});

            return early;
        }

        state->busy = true;
        state->cancelRequested = false;

        struct BusyReset {
            Impl &state; ///< Stable operation host retained by the caller lease.

            /** Release the synchronous admission guard after operation retirement. */
            ~BusyReset() {
                state.busy = false;
            }
        } reset{*state};

        try {
            Operation<> operation(state, request, stop);

            try {
                operation.run(source);

                // Failure drain consumes only the owned decoded cache, without source/SDK/provider progress.
                while (operation.profiles.hasOwnedInput() && operation.checkpoint()) {
                    auto parserWork = state->options.ipf.turnWork;
                    (void)operation.profiles.drain(parserWork);
                }
            } catch (const std::bad_alloc &) {
                fail(operation.result, {IpfTransferError::ALLOCATION_FAILURE, "transfer-allocation"});
            } catch (const std::length_error &) {
                fail(operation.result, {IpfTransferError::RESOURCE_LIMIT, "transfer-length"});
            } catch (const std::filesystem::filesystem_error &) {
                fail(operation.result, {IpfTransferError::IO_ERROR, "source-path"});
            }

            operation.retire(false);

            if (!operation.result.error) {
                (void)operation.checkpoint();
            }

            publish(operation.result);

            return std::move(operation.result);
        } catch (const std::bad_alloc &) {
            fail(early, {IpfTransferError::ALLOCATION_FAILURE, "transfer-preparation"});

            return early;
        }
    }
};

InstrumentProfileReader::InstrumentProfileReader(std::shared_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

InstrumentProfileReader::~InstrumentProfileReader() {
    if (impl_) {
        if (impl_->owner != std::this_thread::get_id()) {
            std::terminate();
        }

        impl_->live = false;
        impl_->cancelRequested = true;
    }
}

std::expected<std::unique_ptr<InstrumentProfileReader>, IpfTransferDiagnostic>
InstrumentProfileReader::create(const IpfTransferOptions &options) {
    if (const auto error = validateOptions(options)) {
        return std::unexpected(*error);
    }

    if (const auto error = Impl::validateBacking(options)) {
        return std::unexpected(*error);
    }

    auto bootstrap = transport::RuntimeBootstrap::create();

    if (!bootstrap) {
        return std::unexpected(
            IpfTransferDiagnostic{IpfTransferError::TRANSPORT_FAILURE, "reader-bootstrap", bootstrap.error()});
    }

    try {
        auto state =
            std::allocate_shared<Impl>(TransferHostAllocator<Impl>(sizeof(Impl) + HOST_CONTROL_BYTES), options);
        state->ownedBootstrap.emplace(std::move(*bootstrap));
        auto lease = td::BootstrapLease::acquireForRuntime(*state->ownedBootstrap);

        if (!lease) {
            return std::unexpected(
                IpfTransferDiagnostic{IpfTransferError::TRANSPORT_FAILURE, "reader-bootstrap-lease", lease.error()});
        }

        state->bootstrapLease.emplace(std::move(*lease));

        return std::unique_ptr<InstrumentProfileReader>(new InstrumentProfileReader(std::move(state)));
    } catch (const std::length_error &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "reader-host-capacity"});
    } catch (const std::bad_alloc &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::ALLOCATION_FAILURE, "reader-allocation"});
    }
}

std::expected<std::unique_ptr<InstrumentProfileReader>, IpfTransferDiagnostic>
InstrumentProfileReader::create(transport::RuntimeBootstrap &bootstrap, const IpfTransferOptions &options) {
    if (const auto error = validateOptions(options)) {
        return std::unexpected(*error);
    }

    if (const auto error = Impl::validateBacking(options)) {
        return std::unexpected(*error);
    }

    auto lease = td::BootstrapLease::acquireForRuntime(bootstrap);

    if (!lease) {
        return std::unexpected(
            IpfTransferDiagnostic{IpfTransferError::TRANSPORT_FAILURE, "reader-bootstrap-lease", lease.error()});
    }

    try {
        auto state =
            std::allocate_shared<Impl>(TransferHostAllocator<Impl>(sizeof(Impl) + HOST_CONTROL_BYTES), options);
        state->bootstrapLease.emplace(std::move(*lease));

        return std::unique_ptr<InstrumentProfileReader>(new InstrumentProfileReader(std::move(state)));
    } catch (const std::length_error &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "reader-host-capacity"});
    } catch (const std::bad_alloc &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::ALLOCATION_FAILURE, "reader-allocation"});
    }
}

IpfTransferResult InstrumentProfileReader::loadURL(std::string_view source, const IpfUrlRequest &request,
                                                   std::stop_token stopToken) {
    auto state = impl_;

    return Impl::load(std::move(state), source, request, stopToken);
}

std::expected<void, IpfTransferDiagnostic> InstrumentProfileReader::cancel() noexcept {
    const auto state = impl_;

    if (state->owner != std::this_thread::get_id()) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::WRONG_THREAD, "reader-owner"});
    }

    state->cancelRequested = true;

    return {};
}

namespace connection_detail {
/** Persistent worker-owned grants; every retry consumes the same native/TLS/HTTP/envelope domain. */
struct TransferHost::Impl {
    using Reader = InstrumentProfileReader::Impl;
    std::shared_ptr<Reader> reader;
    detail::TransferWork work{};
    std::size_t turns = 0;
    std::size_t attempts = 0;
    std::size_t wire = 0;
    std::size_t redirects = 0;

    /** Retain the worker-affine host.
     * @param value Stable prepared reader state.
     */
    explicit Impl(std::shared_ptr<Reader> value) : reader(std::move(value)) {
    }
};

TransferHost::TransferHost(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

TransferHost::~TransferHost() = default;

std::expected<void, IpfTransferDiagnostic> TransferHost::validate(const IpfTransferOptions &options) {
    if (const auto error = validateOptions(options)) {
        return std::unexpected(*error);
    }

    using Reader = InstrumentProfileReader::Impl;
    constexpr auto extraFixed = sizeof(Reader::Operation<detail::StreamingProfiles>) - sizeof(Reader::Operation<>) +
                                sizeof(Impl) + sizeof(TransferHost) + 2 * sizeof(InstrumentProfile);
    auto extra = extraFixed;
    // Parser-to-single-slot MSVC handoff and the staged slot can coexist; no zero-cost streaming claim.
    const auto units = options.ipf.limits.profile.totalUnits;

    if (units > (std::numeric_limits<std::size_t>::max() - extra) / (2 * sizeof(char16_t))) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "connection-profile-backing"});
    }

    extra += units * 2 * sizeof(char16_t);

    if (const auto error = Reader::validateBacking(options, extra)) {
        return std::unexpected(*error);
    }

    return {};
}

std::expected<std::unique_ptr<TransferHost>, IpfTransferDiagnostic>
TransferHost::create(const transport::detail::BootstrapLease &bootstrap, const IpfTransferOptions &options) {
    const auto valid = validate(options);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    auto lease = td::BootstrapLease::acquire(bootstrap);

    if (!lease) {
        return std::unexpected(
            IpfTransferDiagnostic{IpfTransferError::TRANSPORT_FAILURE, "connection-bootstrap", lease.error()});
    }

    try {
        using Reader = InstrumentProfileReader::Impl;
        auto reader =
            std::allocate_shared<Reader>(TransferHostAllocator<Reader>(sizeof(Reader) + HOST_CONTROL_BYTES), options);
        reader->bootstrapLease.emplace(std::move(*lease));
        auto impl = std::make_unique<Impl>(std::move(reader));

        return std::unique_ptr<TransferHost>(new TransferHost(std::move(impl)));
    } catch (const std::bad_alloc &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::ALLOCATION_FAILURE, "connection-host"});
    } catch (const std::length_error &) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "connection-host-capacity"});
    }
}

IpfTransferResult TransferHost::attempt(std::string_view source, const IpfUrlRequest &request, std::stop_token stop,
                                        StreamConsumer &consumer) {
    auto state = impl_->reader;
    IpfTransferResult early;
    early.nativeRetired = true;

    if (state->owner != std::this_thread::get_id() || state->busy) {
        early.error = IpfTransferDiagnostic{IpfTransferError::WRONG_THREAD, "connection-attempt-owner"};

        return early;
    }

    state->busy = true;

    struct Reset {
        InstrumentProfileReader::Impl &state;

        ~Reset() {
            state.busy = false;
        }
    } reset{*state};

    try {
        InstrumentProfileReader::Impl::Operation<detail::StreamingProfiles> operation(state, request, stop);
        operation.profiles.bind(consumer);
        operation.work = impl_->work;
        operation.turns = impl_->turns;
        operation.attempts = impl_->attempts;
        operation.result.wireBytes = impl_->wire;
        operation.result.redirects = impl_->redirects;
        try {
            operation.run(source);

            while (operation.profiles.hasOwnedInput() && operation.checkpoint()) {
                auto work = state->options.ipf.turnWork;
                (void)operation.profiles.drain(work);

                if (!operation.publishItems()) {
                    break;
                }
            }
        } catch (const std::bad_alloc &) {
            fail(operation.result, {IpfTransferError::ALLOCATION_FAILURE, "connection-attempt-allocation"});
        } catch (const std::length_error &) {
            fail(operation.result, {IpfTransferError::RESOURCE_LIMIT, "connection-attempt-length"});
        }
        operation.retire(false);
        impl_->work = operation.work;
        impl_->turns = operation.turns;
        impl_->attempts = operation.attempts;
        impl_->wire = operation.result.wireBytes;
        impl_->redirects = operation.result.redirects;
        state->options.nativeWork = operation.nativeWork;
        state->options.byteWork = operation.byteWork;
        auto &limits = state->options.limits.envelope;
        const auto &used = operation.result.envelope;
        limits.inputBytes -= std::min(limits.inputBytes, used.inputBytes);
        limits.outputBytes -= std::min(limits.outputBytes, used.outputBytes);
        limits.calls -= std::min(limits.calls, used.calls);
        limits.workBytes -= std::min(limits.workBytes, used.workBytes);
        limits.entries -= std::min(limits.entries, used.entries);
        limits.metadataBytes -= std::min(limits.metadataBytes, used.metadataBytes);
        publish(operation.result);

        return std::move(operation.result);
    } catch (const std::bad_alloc &) {
        early.error = IpfTransferDiagnostic{IpfTransferError::ALLOCATION_FAILURE, "connection-attempt-prepare"};

        return early;
    }
}
} // namespace connection_detail

} // namespace org::ttldtor::qdcxx::ipf
