// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile_reader.hpp"
#include "qd/transport/tls_runtime.hpp"

#include <chrono>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::ipf {

/** Detached source/HTTP/operation failures; diagnostic text never contains caller or peer text. */
enum class IpfTransferError {
    INVALID_OPTIONS,       ///< Finite limits, work, trust or timeout settings are incompatible.
    INVALID_SOURCE,        ///< Source syntax or the selected local/HTTP(S) form is invalid.
    UNSUPPORTED_SOURCE,    ///< Scheme, file authority or compression is unsupported.
    RESOURCE_LIMIT,        ///< A configured byte/count/backing bound refused admission.
    WORK_LIMIT,            ///< Cumulative finite progress/copy/parser work was exhausted.
    WRONG_THREAD,          ///< A checked facade operation ran outside its creating thread.
    INVALID_STATE,         ///< A recursive load or an unavailable facade operation was requested.
    ALLOCATION_FAILURE,    ///< Admitted requested storage could not be allocated.
    IO_ERROR,              ///< An owned local source could not be opened/read.
    TRANSPORT_FAILURE,     ///< The bounded DNS/native owner reported its first failure.
    TLS_FAILURE,           ///< Verified TLS preparation, handshake or retirement failed.
    HTTP_FAILURE,          ///< HTTP syntax/framing or a required clean message boundary failed.
    HTTP_STATUS,           ///< Final HTTP status is neither 200 nor an explicitly requested 304.
    REDIRECT_REJECTED,     ///< Redirect target, downgrade, origin/credential policy or count was refused.
    AUTHORIZATION_FAILURE, ///< Selected authorization was unavailable, invalid or stale.
    IPF_FAILURE,           ///< The existing IPF parser failed, preserving its whole owned prefix.
    DEADLINE,              ///< A cooperative monotonic admission deadline expired.
    CANCELLED              ///< Stop token, creator cancel or public-facade retirement cut off admission.
};

/** Static diagnostic with detached lower-layer evidence; no URL/token/exception formatting. */
struct IpfTransferDiagnostic {
    IpfTransferError error = IpfTransferError::INVALID_STATE; ///< First cause.
    std::string_view stage = "none";                          ///< Static library-owned checkpoint.
    transport::TcpDiagnostic transport{};                     ///< Optional native first cause.
    transport::TlsDiagnostic tls{};                           ///< Optional TLS first cause.
};

/** Independent transfer terminal category; NOT_MODIFIED is not a parsed empty snapshot. */
enum class IpfTransferStatus {
    SUCCESS,      ///< Clean source/message plus successful parser EOF under selected IPF policy.
    NOT_MODIFIED, ///< Explicit conditional request received 304; no new parser snapshot.
    CANCELLED,    ///< Request-only cancellation won before successful publication.
    FAILED        ///< A first non-cancellation cause prevented success.
};

/** Bounded interpretation of X-Live metadata, without retaining arbitrary peer header text. */
enum class IpfLiveSupport {
    ABSENT,   ///< No X-Live response field.
    PROVIDED, ///< Exact documented provided response value.
    OTHER     ///< Present with another bounded value; no live collector is implied.
};

/** Finite whole-load bounds, in bytes/counts; zero grants no capacity. */
struct IpfTransferLimits {
    std::size_t sourceBytes = 8192;         ///< Initial source cap, at most the fixed 8192-byte backing.
    std::size_t locationBytes = 8192;       ///< Each redirect field cap, at most fixed source backing.
    std::size_t authorizationBytes = 8192;  ///< Whole Authorization value cap.
    std::size_t headerBytes = 65536;        ///< Status/header syntax bytes per HTTP message.
    std::size_t headerFields = 128;         ///< Header fields per message.
    std::size_t trailerBytes = 16384;       ///< Trailer syntax bytes per message.
    std::size_t trailerFields = 64;         ///< Trailer fields per message.
    std::size_t bodyBytes = 67108864;       ///< Cumulative admitted HTTP payload.
    std::size_t wireBytes = 83886080;       ///< Cumulative plaintext HTTP wire input, independently of TLS ciphertext.
    std::size_t framingBytes = 8388608;     ///< Cumulative non-payload HTTP syntax bytes.
    std::size_t chunkExtensionBytes = 4096; ///< One chunk extension field.
    std::size_t totalChunkExtensionBytes = 1048576; ///< Cumulative chunk extension text.
    std::size_t redirects = 5;                      ///< Followed redirects; no connection retry is performed.
    std::size_t informationalResponses = 8;         ///< Cumulative nonfinal responses.
    std::size_t attempts = 6;                       ///< Cumulative HTTP connections across redirects.
    std::size_t parserCalls = 1000000;              ///< HTTP parser invocations, including EOF and failures.
    std::size_t presentedBytes = 268435456;         ///< Cumulative HTTP/parser/copy byte visits.
    std::size_t cleanupTurns = 256;   ///< Native cleanup reserve partitioned from nativeWork, never renewed.
    std::size_t turns = 1000000;      ///< Internal bounded operation turns.
    std::size_t inputBytes = 131072;  ///< Fixed contiguous HTTP input window.
    std::size_t requestBytes = 32768; ///< Fixed whole-request output slot.
    std::size_t residentBytes = 16 * 1024 * 1024; ///< Transfer-owned backing allowance, excluding lower-layer bounds.
};

/** Finite native/HTTP progress configuration, copied before activation; no ambient resolver/trust. */
struct IpfTransferOptions {
    IpfReadOptions ipf{};       ///< Existing independent parser/output limits and positive turn quotas.
    IpfTransferLimits limits{}; ///< Source, framing, copy and transfer backing limits.
    std::vector<transport::DnsServer> dnsServers{}; ///< Explicit at most four numeric IPv4 DNS servers.
    std::uint16_t dnsPort = 53;                     ///< Explicit DNS server port.
    transport::TcpLimits nativeLimits{4096, 4096, 64, 16, 65536, 8192, 96 * 1024 * 1024}; ///< Native backing.
    std::chrono::milliseconds overallTimeout{300000}; ///< Whole-load cooperative deadline.
    std::chrono::milliseconds connectTimeout{30000};  ///< Whole DNS/connect admission interval.
    std::chrono::milliseconds readTimeout{60000};     ///< Native input inactivity interval.
    std::chrono::milliseconds writeTimeout{60000};    ///< Pending native-output inactivity interval.
    std::chrono::milliseconds pollInterval{1};        ///< Finite cancellation/progress cadence; no notifier thread.
    std::string trustPem{}; ///< Explicit ordinary PEM anchors for HTTPS, at most tls.limits.trustBytes.
    std::chrono::milliseconds handshakeTimeout{30000}; ///< Verified TLS handshake deadline.
    std::chrono::milliseconds closeTimeout{1000};      ///< TLS-close deadline.
    transport::TlsOptions tls{};        ///< Explicit qualified same-image host/shared/private reservations.
    transport::TlsWorkVector tlsWork{}; ///< Whole-reader TLS work, debited across every load/redirect, never reset.
    transport::TcpWork nativeWork{
        1000000, 1000000, 536870912, 1000000, 1000000, 134217728, (std::numeric_limits<std::uint64_t>::max)()};
    ///< Whole-load native work; DNS units are the existing conservative profile, never an unlimited sentinel.
    protocol::LiveWork byteWork{1000000, 1000000000, 1000000000, 64ULL * 1024 * 1024 * 1024};
    ///< Whole-load logical preparation/copy visits, including DNS setup; not allocated bytes, never refunded.
};

/** Provider rejection without secret-bearing text. */
enum class HttpAuthorizationError {
    UNAVAILABLE,       ///< No value is available.
    INVALID,           ///< The supplied value is invalid or exceeds its finite bound.
    ALLOCATION_FAILURE ///< Owned token creation could not allocate.
};

/** Validated owned HTTP value; no formatter or implicit logging conversion is provided. */
class HttpAuthorization {
    std::string value_; ///< Whole validated header value, independent of caller storage.
    /** Adopt a checked complete value.
     * @param value Owned complete header value.
     */
    explicit HttpAuthorization(std::string value);

    public:
    /** Construct UTF-8 Basic authorization without percent decoding.
     * @param user Exact UTF-8 user bytes; colon is forbidden.
     * @param password Exact UTF-8 password bytes, including colons.
     * @param maximum Maximum whole value bytes, checked before allocation.
     * @return Owned value or detached rejection.
     */
    [[nodiscard]] static std::expected<HttpAuthorization, HttpAuthorizationError>
    basic(std::string_view user, std::string_view password, std::size_t maximum = 8192);
    /** Construct a checked Bearer value.
     * @param token Nonempty HTTP credential bytes.
     * @param maximum Maximum whole value bytes.
     * @return Owned value or detached rejection.
     */
    [[nodiscard]] static std::expected<HttpAuthorization, HttpAuthorizationError> bearer(std::string_view token,
                                                                                         std::size_t maximum = 8192);
    /** Construct a custom checked scheme/value.
     * @param scheme Nonempty ASCII HTTP token.
     * @param value Nonempty field-value credential bytes without controls/CR/LF.
     * @param maximum Maximum whole value bytes, checked before copy.
     * @return Owned value or detached rejection.
     */
    [[nodiscard]] static std::expected<HttpAuthorization, HttpAuthorizationError>
    custom(std::string_view scheme, std::string_view value, std::size_t maximum = 8192);
    /** Borrow the secret only for explicit request serialization.
     * @return Value valid while this owning object remains unchanged/alive; never log it.
     */
    [[nodiscard]] std::string_view value() const noexcept;
};

/** Sanitized intended origin descriptor borrowed only during one authorization callback. */
struct HttpAuthorizationRequest {
    std::string_view scheme; ///< HTTP scheme, HTTPS for every secret-bearing invocation.
    std::string_view host;   ///< Canonical host without userinfo/path/query.
    std::uint16_t port;      ///< Effective numeric origin port.
    std::size_t attempt;     ///< Current monotonically admitted HTTP attempt.
};

/** Application-owned HTTP provider, separate from the existing QTP Basic provider.
 * Invocation is on the creator thread after verified HTTPS, outside native/owner locks.
 * It may cancel/delete/replace its facade; no destination/request borrow may escape the call.
 * Capture/destructor/callback work and external graphs remain embedding preconditions.
 */
class HttpAuthorizationProvider {
    public:
    /** Release on the thread dropping the last provider lease. */
    virtual ~HttpAuthorizationProvider() = default;
    /** Supply a complete scheme-plus-value into a fresh bounded destination.
     * @param request Sanitized currently verified origin, borrowed until return.
     * @param destination Secret output buffer, borrowed until return.
     * @return Written bytes in1..destination.size(), or detached rejection; exceptions are redacted.
     */
    [[nodiscard]] virtual std::expected<std::size_t, HttpAuthorizationError>
    authorization(HttpAuthorizationRequest request, std::span<char> destination) = 0;
};

/** Explicit one-load conditional/auth input; no properties or credential fallback are consulted. */
struct IpfUrlRequest {
    std::optional<std::chrono::sys_seconds> ifModifiedSince{}; ///< Explicit conditional HTTP date.
    std::optional<HttpAuthorization> authorization{};          ///< Owned token overriding initial URL userinfo.
    std::shared_ptr<HttpAuthorizationProvider> provider{}; ///< Explicit leased provider, mutually exclusive with token.
};

/** Detached source/message/parser outcome, owning every complete accepted profile on any failure. */
struct IpfTransferResult {
    IpfTransferStatus status = IpfTransferStatus::FAILED; ///< Transfer outcome, independent of IPF marker.
    IpfReadResult ipf{};                          ///< Owned parser prefix and exact existing EOF/completion evidence.
    std::optional<IpfTransferDiagnostic> error{}; ///< First transfer/native/IPF cause.
    unsigned httpStatus = 0;                      ///< Final observed HTTP code; zero for local/pre-header failure.
    std::optional<std::chrono::sys_seconds> lastModified{}; ///< Bounded parsed candidate response timestamp.
    bool metadataCommitted = false; ///< True only for successful processed200;304 does not replace content.
    IpfLiveSupport live = IpfLiveSupport::ABSENT; ///< X-Live observation without live-stream behavior.
    bool httpFinished = false;                    ///< Valid HTTP framing boundary, never inferred from IPF COMPLETE.
    bool nativeRetired = false;                   ///< All owned native callbacks and buffers drained before return.
    std::size_t wireBytes = 0;                    ///< Admitted plaintext HTTP syntax/payload bytes.
    std::size_t redirects = 0;                    ///< Followed redirects.
};

/** Synchronous source reader over private bounded HTTP/native owners, without an application poll loop.
 * Owned creation is main/pre-thread; borrowed bootstrap outlives facade and active drain.
 * All facade methods/destruction use the creator thread. A stop_token may be requested elsewhere;
 * that thread performs no SDK work. Cancellation is checked between finite internal turns.
 * Provider deletion of this facade requests cutoff only; a stable operation lease keeps all state
 * until native retirement. No facade access follows callback entry. No hidden worker is created.
 */
class InstrumentProfileReader {
    struct Impl;
    std::shared_ptr<Impl> impl_; ///< Stable host/operation lifetime, independent of public liveness.
    /** Adopt fully prepared host state.
     * @param impl Owned stable state.
     */
    explicit InstrumentProfileReader(std::shared_ptr<Impl> impl) noexcept;

    public:
    /** Retire public liveness; active callback/load leases retain native/bootstrap state through drain. */
    ~InstrumentProfileReader();
    InstrumentProfileReader(const InstrumentProfileReader &) = delete;
    InstrumentProfileReader &operator=(const InstrumentProfileReader &) = delete;
    /** Create owned process lifetime on main before application threads.
     * @param options Explicit finite settings, copied only after validation.
     * @return Stable nonmovable facade or detached preparation failure.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<InstrumentProfileReader>, IpfTransferDiagnostic>
    create(const IpfTransferOptions &options = {});
    /** Borrow an explicit same-linked-instance host bootstrap on its creator thread.
     * @param bootstrap Main/pre-thread owner; its stable state, possibly in a moved-to facade, survives final drain.
     * @param options Explicit finite settings.
     * @return Stable facade or detached preparation failure, without global SDK initialization.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<InstrumentProfileReader>, IpfTransferDiagnostic>
    create(transport::RuntimeBootstrap &bootstrap, const IpfTransferOptions &options = {});
    /** Read one uncompressed bounded source, internally progressing and draining its operation.
     * @param source Caller source, borrowed only during bounded initial classification/extraction.
     * @param request Explicit conditional/auth input, snapshotted before application callbacks.
     * @param stopToken Safe request-only cancellation token; request_stop may run on another thread.
     * @return Detached owning prefix, separate HTTP/IPF evidence and first failure after native drain.
     */
    [[nodiscard]] IpfTransferResult loadURL(std::string_view source, const IpfUrlRequest &request = {},
                                            std::stop_token stopToken = {});
    /** Request creator-thread cancellation only; never join, allocate or call SDK code inline.
     * @return Acceptance or wrong-thread diagnostic; current native drain remains with loadURL.
     */
    [[nodiscard]] std::expected<void, IpfTransferDiagnostic> cancel() noexcept;
};

} // namespace org::ttldtor::qdcxx::ipf
