// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "tls_policy.hpp"

// Project client enums must be parsed before Asio introduces the Windows CALLBACK macro.
#include "runtime_core.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <memory>

namespace org::ttldtor::qdcxx::transport::detail {

/** Selected private TLS codec with fixed independent ciphertext/plaintext windows.
 * Native operations retain Core plus this stable owner. No application callback runs here. The
 * wrapper grants copied per-entry/initial permits before Core dispatch; no caller budget is retained.
 * Destruction is owner-only after completion drain, funded by the wrapper's copied disposal reserve.
 */
class TlsEngine final : public ByteEngine {
    std::unique_ptr<TlsPolicy> policy_; ///< Stable private store/provider/context, released after SSL/BIO.
    SSL *ssl_ = nullptr;                ///< Sole SSL state; owns both references to the internal BIO endpoint.
    BIO *peer_ = nullptr;               ///< Separate external BIO endpoint, owned until native completion drain.
    std::array<std::byte, TLS_RECORD_CAPACITY> cipher_{};   ///< Stable wire-write window, never changed in flight.
    std::array<std::byte, TLS_PLAINTEXT_CAPACITY> plain_{}; ///< Stable verified input window for pure-session feed.
    std::size_t cipherBegin_ = 0;                           ///< Credited wire prefix.
    std::size_t cipherEnd_ = 0;                             ///< End of immutable queued ciphertext.
    std::size_t plainBegin_ = 0;                            ///< Admitted plaintext prefix.
    std::size_t plainEnd_ = 0;                              ///< End of immutable decoded plaintext.
    std::size_t pendingWire_ = 0; ///< Cached external-BIO readable ciphertext, queried only inside entries.
    bool bufferedRead_ = false;   ///< Cached incoming/internal plaintext opportunity after a successful SDK step.
    const std::byte *pendingWrite_ = nullptr; ///< Exact SSL_write pointer retained across WANT.
    std::size_t pendingWriteSize_ = 0;        ///< Exact SSL_write length retained across WANT.
    std::uint64_t acceptedPlain_ = 0; ///< Checked cumulative application acceptance, distinct from socket bytes.
    TlsDiagnostic failure_{};         ///< First detached static/numeric engine failure.
    bool initialFunded_ = false;      ///< Once-only copied H permit, never reset on WANT/failure.
    bool entryFunded_ = false;        ///< One synchronously granted E entry; no budget pointer is retained.
    bool closeFunded_ = false;        ///< Current entry also includes the close-control coordinate.
    bool started_ = false;            ///< Initial SSL handshake entry occurred.
    bool verified_ = false;         ///< Current initial attempt verified both chain and configured reference identity.
    bool aborted_ = false;          ///< Reserved request-only cutoff; no further SDK entry is legal.
    bool wantRead_ = false;         ///< Last classification requires fresh encrypted input.
    bool wantWrite_ = false;        ///< Last classification requires ciphertext backpressure relief.
    bool closeRequested_ = false;   ///< Orderly TLS close requested outside application callbacks.
    bool peerClosed_ = false;       ///< Cached actual SSL_RECEIVED_SHUTDOWN.
    bool shutdownComplete_ = false; ///< SSL_shutdown returned1; ciphertext still needs independent drain.

    /** Construct only empty stable codec backing; policy/SSL are installed before publication. */
    TlsEngine() = default;
    /** Preserve the first engine cause without allocating or formatting text.
     * @param failure Static/numeric detached cause.
     */
    void fail(TlsDiagnostic failure) noexcept;
    /** Refresh cached BIO/shutdown/read opportunities after I/O classification, never before get_error.
     * @return True unless a finite BIO accounting query is inconsistent.
     */
    [[nodiscard]] bool refresh() noexcept;
    /** Copy one finite available BIO prefix into the immutable native-write window.
     * @return True when bytes were copied; false for no pending bytes or a recorded failure.
     */
    [[nodiscard]] bool drainCiphertext() noexcept;
    /** Classify one already immediately sampled SSL_get_error result.
     * @param classification Same-owner immediate classification, with no intervening SDK call.
     * @param stage Static checkpoint.
     */
    void classify(int classification, std::string_view stage) noexcept;
    /** Prepare DNS SAN/SNI or numeric IP SAN identity independently of the network route.
     * @param settings Owned immutable TLS settings, borrowed during setup only.
     * @return Empty on checked setup, otherwise static identity/policy rejection.
     */
    [[nodiscard]] std::expected<void, TlsDiagnostic> configureIdentity(const client::EndpointTls &settings) noexcept;

    public:
    /** Retire SSL and external BIO before private context/provider ownership; no blocking shutdown. */
    ~TlsEngine() override;
    /** Prepare complete policy/BIO/SSL state before the common core consumes the composer.
     * @param settings Snapshot-owned explicit trust/reference/deadline/token values.
     * @param limits Checked immutable resource/input ceilings.
     * @return Stable unactivated codec or detached setup rejection with ordinary partial cleanup.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TlsEngine>, TlsDiagnostic>
    create(const client::EndpointTls &settings, const TlsResourceLimits &limits);
    /** Grant the once-only initial phase permit by value before any handshake SDK entry. */
    void fundInitial() noexcept;
    /** Grant one synchronous engine-entry permit, retaining no caller work pointer.
     * @param close True when the copied vector also funds one close-control step.
     */
    void fundEntry(bool close) noexcept;
    /** Expire an unused/used entry at the public operation boundary; no credits are refunded. */
    void endEntry() noexcept;
    /** Inspect stable immutable queued ciphertext.
     * @return Borrowed wire suffix, valid through its native completion.
     */
    [[nodiscard]] std::span<const std::byte> ciphertext() const noexcept override;
    /** Credit native wire acceptance without any SDK call.
     * @param bytes Accepted prefix of ciphertext().
     * @return True for an eligible count.
     */
    [[nodiscard]] bool consumeCiphertext(std::size_t bytes) noexcept override;
    /** Inspect stable verified plaintext.
     * @return Borrowed suffix, valid until its matching consume/retirement.
     */
    [[nodiscard]] std::span<const std::byte> plaintext() const noexcept override;
    /** Credit pure-session input admission without SDK calls.
     * @param bytes Eligible accepted prefix.
     */
    void consumePlaintext(std::size_t bytes) noexcept override;
    /** Inspect cached useful progress without entering the SDK.
     * @param hasInput New encrypted input is retained by Core.
     * @param hasOutput Immutable application output is retained by Core.
     * @return True for one useful admitted step.
     */
    [[nodiscard]] bool needsStep(bool hasInput, bool hasOutput) const noexcept override;
    /** Execute at most one SSL operation with exact WANT arguments and immediate classification.
     * @param input Borrowed finite encrypted suffix; at most17408 bytes are directly supplied.
     * @param output Borrowed immutable application suffix, retained by Core across WANT.
     * @return Eligible accepted prefixes, progress and detached common failure marker.
     */
    [[nodiscard]] ByteEngineStep advance(std::span<const std::byte> input,
                                         std::span<const std::byte> output) noexcept override;
    /** Inspect completed chain/reference verification.
     * @return True only for this nonresumed verified initial attempt.
     */
    [[nodiscard]] bool verified() const noexcept override;
    /** Inspect complete TLS and ciphertext drain.
     * @return True only after SSL_shutdown1 and every encoded wire suffix drains.
     */
    [[nodiscard]] bool finished() const noexcept override;
    /** Notify raw native EOF without SDK calls.
     * @return Static failure for unverified/truncated TLS; empty after actual peer close_notify.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> nativeEnd() noexcept override;
    /** Request allocation-free cutoff without inline SSL/BIO/native cleanup or span invalidation. */
    void abort() noexcept override;
    /** Request TLS close after the wrapper checks owner/busy/output eligibility; no SDK call. */
    void requestTlsClose() noexcept;
    /** Inspect whether subsequent SDK entry needs the close-control coordinate.
     * @return True when orderly close is requested or peer close_notify was observed.
     */
    [[nodiscard]] bool closing() const noexcept;
    /** Inspect an immutable outstanding application SSL_write retry.
     * @return True while exact arguments/content ownership must remain unchanged.
     */
    [[nodiscard]] bool pendingWrite() const noexcept;
    /** Inspect cached actual peer close_notify evidence.
     * @return True after SSL_RECEIVED_SHUTDOWN was observed inside a classified entry.
     */
    [[nodiscard]] bool peerClosed() const noexcept;
    /** Inspect conservative private TLS-data retention without entering the SDK.
     * @return True unless complete orderly shutdown and known output drain were established.
     */
    [[nodiscard]] bool dataPending() const noexcept;
    /** Inspect known external BIO plus native-window ciphertext.
     * @return Finite known suffix; dataPending also covers private SDK record/protocol state.
     */
    [[nodiscard]] std::size_t pendingCiphertext() const noexcept;
    /** Inspect cumulative SSL application acceptance.
     * @return Checked accepted plaintext bytes, never peer acknowledgement.
     */
    [[nodiscard]] std::uint64_t acceptedPlaintext() const noexcept;
    /** Inspect first detached engine failure.
     * @return Static/numeric evidence, NONE when no failure occurred.
     */
    [[nodiscard]] TlsDiagnostic diagnostic() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
