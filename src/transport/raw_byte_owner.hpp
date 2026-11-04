// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "raw_byte_state.hpp"
#include "runtime_core.hpp"

namespace org::ttldtor::qdcxx::transport::detail {

/** Stable caller-driven byte operation; no method invokes application code or creates a thread.
 * The outer bootstrap lease survives idle/configured gaps. The independent core lease covers DNS
 * and every late native completion. Keep this owner on its creating serialized thread; destroy
 * only outside an active method, after future facade callbacks release their operation lease.
 */
class RawByteOwner {
    BootstrapLease bootstrap_;          ///< Outer borrow, released after the native child and all byte backing.
    RawByteState bytes_;                ///< Whole input/output backing, released after core/engine retirement.
    std::unique_ptr<RuntimeCore> core_; ///< Single existing DNS/socket/completion spine.
    std::thread::id owner_ = std::this_thread::get_id(); ///< Immutable serialized owner.
    bool busy_ = false;                                  ///< Owner-turn reentry guard; close remains request-only.
    bool closeRequested_ = false;                        ///< Immediate allocation-free ordinary-admission cutoff.

    /** Retain outer process lifetime and allocate checked finite byte windows.
     * @param bootstrap Outer stable counted borrow, transferred here.
     * @param limits Checked positive capacities.
     */
    RawByteOwner(BootstrapLease &&bootstrap, RawByteLimits limits);
    /** Check affinity and optional reentry without touching native/SDK state.
     * @param allowBusy True only for request-only close.
     * @return Detached refusal, or empty for an eligible call.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> rejection(bool allowBusy = false) const noexcept;

    /** Share factory checks/debits while selecting one stable source at the original borrow checkpoint.
     * Exactly one of facade/lease is supplied by the two private-owner entry points.
     * @param facade Movable application facade, used only during this factory call, or null.
     * @param lease Stable outer identity, or null for facade acquisition.
     * @param options Explicit finite native settings.
     * @param limits Whole owned byte windows.
     * @param work Mutable cumulative preparation credits.
     * @param engine Optional owned codec.
     * @return Prepared owner or the original ordered refusal; never activates I/O.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
    createImpl(RuntimeBootstrap *facade, const BootstrapLease *lease, TcpOptions options, RawByteLimits limits,
               protocol::LiveWork &work, std::unique_ptr<ByteEngine> engine);

    public:
    /** Drain native callbacks before byte backing, engine configuration or outer lease retirement. */
    ~RawByteOwner();
    RawByteOwner(const RawByteOwner &) = delete;
    RawByteOwner &operator=(const RawByteOwner &) = delete;
    /** Prepare one bounded unactivated operation without any QTP schema or session.
     * @param bootstrap Application-owned process lifetime, retained by independent outer/child borrows.
     * @param options Copied finite endpoint/native capacities and deadlines.
     * @param limits Whole application input/output capacities.
     * @param work Mutable preparation credits; admitted work is never refunded.
     * @param engine Optional privately prepared engine, whose enclosing owner funds every entry.
     * @return Stable operation or detached preparation failure; never activates network I/O.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
    create(RuntimeBootstrap &bootstrap, TcpOptions options, RawByteLimits limits, protocol::LiveWork &work,
           std::unique_ptr<ByteEngine> engine = {});
    /** Prepare from stable retained process identity, independent of any movable bootstrap facade.
     * @param bootstrap Outer counted identity; moved-to application owner survives through child drain.
     * @param options Copied finite native settings.
     * @param limits Whole application byte windows.
     * @param work Mutable cumulative preparation credits; original debit/error order is preserved.
     * @param engine Optional owned internal codec.
     * @return Prepared owner or existing ordered refusal, without SDK initialization or network I/O.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
    create(const BootstrapLease &bootstrap, TcpOptions options, RawByteLimits limits, protocol::LiveWork &work,
           std::unique_ptr<ByteEngine> engine = {});
    /** Activate the single native connection sequence.
     * @return Acceptance or detached affinity/state refusal.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> start();
    /** Perform one native/codec/copy action, without application callbacks.
     * @param work Mutable native-turn credits.
     * @param byteWork Mutable copy credits; each admitted input copy consumes one operation and copied bytes.
     * @return Bounded progress, EOF/error retirement or detached refusal; never HTTP completion.
     */
    [[nodiscard]] std::expected<TcpPollResult, TcpDiagnostic> poll(TcpWork &work, protocol::LiveWork &byteWork);
    /** Copy only already-owned native plain input after READ_FAILURE, without ordinary progress.
     * Never polls context, reads a socket, advances a codec or submits/accepts output. The existing
     * completed-read promotion/copy native-step charges and byte op/bytes are paid before mutation.
     * Pending native handlers and original immutable output remain retained for later close/drain.
     * @param work Remaining native-step credits; handlers/socket/DNS credits are never used here.
     * @param byteWork Remaining copy operation/byte credits; zero credits yield without mutation.
     * @return Bounded cached-prefix admission, yield/idle, or detached affinity/state refusal.
     */
    [[nodiscard]] std::expected<TcpPollResult, TcpDiagnostic> drainOwnedInput(TcpWork &work,
                                                                              protocol::LiveWork &byteWork);
    /** Copy and publish a whole finite output before any engine/native borrower sees it.
     * @param bytes Borrowed complete output, copied before return.
     * @param work Mutable copy credits, one operation and bytes.size() bytes on admission.
     * @return Whole local admission, or refusal with no output publication.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> write(std::span<const std::byte> bytes, protocol::LiveWork &work);
    /** Inspect stable application input, including a retained prefix after terminal retirement.
     * @return Immutable suffix until explicit consumeInput or owner retirement, or owner refusal.
     */
    [[nodiscard]] std::expected<std::span<const std::byte>, TcpDiagnostic> input() const noexcept;
    /** Explicitly release a processed application prefix.
     * @param bytes Consumed prefix of input().
     * @return Acceptance or refusal without mutation for an excessive count.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> consumeInput(std::size_t bytes) noexcept;
    /** Request only: no SDK, socket cancellation, buffer release or callback drain occurs inline.
     * Already published terminal evidence remains unchanged.
     * @return Acceptance, including during an active owner turn, or wrong-thread refusal.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> requestClose() noexcept;
    /** Inspect immutable output still unaccepted by engine/native transport on the owner.
     * @return Remaining application byte count, distinct from pending ciphertext.
     */
    [[nodiscard]] std::size_t pendingOutput() const noexcept;
    /** Borrow the stable private core for the enclosing TLS permit/deadline adapter.
     * @return Stable core reference, valid until this owner retires; serialized owner precondition.
     */
    [[nodiscard]] RuntimeCore &core() noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
