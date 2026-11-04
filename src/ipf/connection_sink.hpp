// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "../transport/bootstrap_lease.hpp"
#include "qd/ipf/instrument_profile_url_reader.hpp"

namespace org::ttldtor::qdcxx::ipf::connection_detail {
/** Header-boundary action; skipping never implies consumed HTTP/container EOF. */
enum class HeaderAction { ACCEPT, SKIP, REJECT };

/** Private stable outer-driver sink; none of these calls run inside parser/native/zlib callbacks. */
struct StreamConsumer {
    /** Retire only the sink interface; concrete operation owns its captures and drain. */
    virtual ~StreamConsumer() = default;
    /** Observe final200 headers.
     * @param result Borrowed stable metadata; no source/credential text.
     * @param modified Opened-file or parsed HTTP UTC time, absent when unknown.
     * @return Processing, unchanged-date skip, or refusal.
     */
    virtual HeaderAction headers(const IpfTransferResult &result,
                                 std::optional<std::chrono::sys_time<std::chrono::milliseconds>> modified) = 0;
    /** Admit one owned-parser profile into bounded pending state.
     * @param profile Immutable value borrowed only until return.
     * @return True after local admission; false seals ordinary progress.
     */
    virtual bool profile(const InstrumentProfile &profile) = 0;
    /** Publish a validated control at an outer stable boundary.
     * @param kind FLUSH or COMPLETE.
     * @return True after ordered collector/state publication.
     */
    virtual bool control(IpfItemKind kind) = 0;
    /** Publish advisory state selections and observe connection cutoff.
     * @return True when ordinary source work remains permitted.
     */
    virtual bool checkpoint() = 0;
    /** Read the current outer refusal without allocating or invoking callbacks.
     * @return First local admission/deadline cause, absent for ordinary caller cancellation.
     */
    virtual std::optional<IpfTransferDiagnostic> refusal() const noexcept = 0;
};

/** Worker-affine persistent attempt host over the existing native/HTTP/decoder implementation. */
class TransferHost {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    /** Bind a worker-owned implementation.
     * @param impl Stable private host.
     */
    explicit TransferHost(std::unique_ptr<Impl> impl) noexcept;

    public:
    ~TransferHost();
    /** Validate normal plus live-operation backing without SDK activation.
     * @param options Explicit full lifetime grants.
     * @return Detached refusal, or success.
     */
    static std::expected<void, IpfTransferDiagnostic> validate(const IpfTransferOptions &options);
    /** Clone stable explicit process identity on the worker without init/cleanup.
     * @param bootstrap Main-owned stable outer lease.
     * @param options Explicit lifetime grants and settings.
     * @return Worker-affine host or detached failure.
     */
    static std::expected<std::unique_ptr<TransferHost>, IpfTransferDiagnostic>
    create(const transport::detail::BootstrapLease &bootstrap, const IpfTransferOptions &options);
    /** Execute one incremental attempt, with cumulative grants and no full output accumulation.
     * @param source Stable sanitized source.
     * @param request Stable explicit origin/auth/conditional snapshot.
     * @param stop Safe request-only lifetime cutoff.
     * @param consumer Stable outer-driver owner.
     * @return Detached attempt evidence after native drain; vector holds at most one unadmitted profile.
     */
    IpfTransferResult attempt(std::string_view source, const IpfUrlRequest &request, std::stop_token stop,
                              StreamConsumer &consumer);
};
} // namespace org::ttldtor::qdcxx::ipf::connection_detail
