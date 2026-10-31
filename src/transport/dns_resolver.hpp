// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/transport/tcp_runtime.hpp"

#include <ares.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace org::ttldtor::qdcxx::transport::detail {

/** Fixed copied DNS result; no SDK record, name or socket lifetime escapes the resolver. */
struct ResolvedAddress {
    std::array<std::uint8_t, 16> bytes{}; ///< Network-order bytes; IPv4 uses the first four.
    bool ipv6 = false;                    ///< Whether all sixteen bytes form an IPv6 address.
};

/** Internal native-send seam; the public runtime always uses the default single system call.
 * A non-null context is borrowed through resolver destruction and is used only by finite bridge tests.
 */
using DnsSendAttempt = ares_ssize_t (*)(ares_socket_t, const void *, std::size_t, int, const sockaddr *, ares_socklen_t,
                                        void *) noexcept;

/** Internal finite native-connect seam; the public runtime always selects the single system call. */
using DnsConnectAttempt = int (*)(ares_socket_t, const sockaddr *, ares_socklen_t, void *) noexcept;

/** One stable caller-driven channel, one query and finite bridge/copy state; never moved after initialization. */
class DnsResolver {
    static constexpr std::size_t MAX_FDS = 16;       ///< Fixed socket/readiness registry and close-control ceiling.
    static constexpr std::size_t MAX_ADDRESSES = 64; ///< Fixed result/deduplication ceiling.
    static constexpr std::size_t MAX_NAME = 255;     ///< Plain selected presentation name including optional root dot.
    static constexpr std::size_t MAX_PRESENTATION = 1024; ///< Pinned parser's escaped name backing ceiling.
    static constexpr std::size_t MAX_CNAME_DEPTH = 16;    ///< Fixed loop-guard/name-copy ceiling.
    static constexpr std::size_t MAX_DNS_WIRE = 65535;    ///< Whole pinned DNS wire/parser ceiling.
    static constexpr std::size_t READ_CALLS = 2;          ///< Additional per-SDK-entry native receive-attempt ceiling.
    static constexpr std::size_t READ_BYTES = READ_CALLS * MAX_DNS_WIRE; ///< Complete UDP datagrams remain intact.
    static constexpr std::size_t DNS_SEND_BYTES = 9 * 8192; ///< Conservative simultaneous connection-output allowance.
    static constexpr std::size_t NATIVE_CALLS = 128; ///< Shared finite ordinary bridge/native-readiness allowance.

    /** One c-ares-owned descriptor; this adapter never gives it to Asio ownership. */
    struct Entry {
        ares_socket_t socket = ARES_SOCKET_BAD; ///< Native handle, closed only by the SDK bridge.
        bool tcp = false;                       ///< Whether preserving partial stream framing is required.
        bool read = false;                      ///< Current SDK read-readiness interest.
        bool write = false;                     ///< Current SDK write-readiness interest.
        bool udpWrite = false; ///< Bridge-observed UDP would-block state, absent from the legacy SDK snapshot.
    };

    ares_channel_t *channel_ = nullptr;                      ///< Sole SDK channel, destroyed outside its callbacks.
    std::array<Entry, MAX_FDS> sockets_{};                   ///< Fixed bridge and readiness state.
    std::size_t socketCount_ = 0;                            ///< Live descriptor cardinality.
    std::array<char, MAX_NAME + 1> name_{};                  ///< Owned absolute original query name.
    std::array<ResolvedAddress, MAX_ADDRESSES> addresses_{}; ///< Copied eligible addresses across both families.
    std::size_t addressCount_ = 0;                           ///< Number of copied distinct eligible addresses.
    std::size_t addressLimit_ = 0;                           ///< Configured result cap.
    std::size_t depthLimit_ = 0;                             ///< Configured in-response CNAME depth cap.
    unsigned family_ = 0;                                    ///< Zero=A, one=AAAA, two=finished.
    bool pending_ = false;                                   ///< One active query may retain this callback context.
    bool completed_ = false;        ///< One synchronous callback result awaits the next public progress action.
    bool closing_ = false;          ///< Expected destruction callbacks cannot replace the recorded cause.
    bool violation_ = false;        ///< Bridge invariant/capacity/native-work violation requires retirement.
    std::size_t callbackDepth_ = 0; ///< C callback lifetime guard; channel destruction is outside callbacks.
    TcpWork *work_ = nullptr;       ///< Borrowed only during one synchronous SDK/native progress entry.
    std::size_t nativeCalls_ = 0;   ///< Independent per-entry ceiling, even when the caller supplies excess credits.
    std::size_t readCalls_ = 0;     ///< Native receive attempts in this entry.
    std::size_t readPackets_ = 0;   ///< Successful receives in this entry.
    std::size_t readBytes_ = 0;     ///< Received bytes in this entry.
    int lastStatus_ = ARES_ENODATA; ///< Most recent family result, without peer text.
    std::optional<TcpDiagnostic> failure_;       ///< First bounded static error evidence.
    DnsSendAttempt sendAttempt_ = nullptr;       ///< Optional finite internal test seam; null selects native sendto.
    void *sendContext_ = nullptr;                ///< Borrowed test-seam context, absent from the public runtime path.
    DnsConnectAttempt connectAttempt_ = nullptr; ///< Optional finite interruption seam; null selects native connect.
    void *connectContext_ = nullptr;             ///< Borrowed finite test context, absent from the public runtime path.

    /** Set native would-block so SDK retry loops yield without a new native attempt. */
    static void wouldBlock() noexcept;
    /** Admit one ordinary native attempt before executing it.
     * @return True when the active caller turn has a remaining shared call credit.
     */
    bool nativeAttempt() noexcept;
    /** Record a finite bridge/selection failure without reentering the channel.
     * @param error Static category.
     * @param stage Static checkpoint.
     * @param code Optional native/SDK ordinal.
     */
    void fail(TcpError error, std::string_view stage, int code = 0) noexcept;
    /** Copy only terminal canonical-owner addresses through a finite in-response CNAME chain.
     * @param record Borrowed SDK response, valid only during its completion callback.
     */
    void copyResult(const ares_dns_record_t *record) noexcept;
    /** Sum the checked conservative parser/allocation/copy/control work vector.
     * @return Sufficient logical primitive units for one whole SDK entry, never a latency estimate.
     */
    static std::uint64_t progressUnits() noexcept;
    /** Build the bridge table; the SDK copies its callback function pointers.
     * @return Nonblocking per-channel native socket functions.
     */
    ares_socket_functions_ex functions() const noexcept;
    /** Capture one borrowed terminal response without channel mutation or application callbacks.
     * @param argument Stable resolver.
     * @param status SDK outcome.
     * @param timeouts Finite SDK retry count, unused by this profile.
     * @param record Borrowed parsed response; never retained or freed by this callback.
     */
    static void completed(void *argument, ares_status_t status, std::size_t timeouts,
                          const ares_dns_record_t *record) noexcept;
    /** Open and configure one nonblocking native descriptor, debiting every ordinary native attempt.
     * @param domain Address family selected by the pinned SDK.
     * @param type Datagram or stream socket kind.
     * @param protocol Native protocol ordinal.
     * @param argument Stable resolver.
     * @return SDK-owned native handle, or ARES_SOCKET_BAD without an unowned descriptor.
     */
    static ares_socket_t openSocket(int domain, int type, int protocol, void *argument) noexcept;
    /** Close one tracked descriptor using the independently reserved finite control allowance.
     * @param socket SDK-owned native handle.
     * @param argument Stable resolver.
     * @return Native close result; no ordinary quota can prevent release.
     */
    static int closeSocket(ares_socket_t socket, void *argument) noexcept;
    /** Apply one supported socket-buffer option after call admission.
     * @param socket SDK-owned native descriptor.
     * @param option Selected SDK option.
     * @param value Borrowed option bytes.
     * @param size Exact option byte count.
     * @param argument Stable resolver.
     * @return Native status or would-block without another native attempt.
     */
    static int setOption(ares_socket_t socket, ares_socket_opt_t option, const void *value, ares_socklen_t size,
                         void *argument) noexcept;
    /** Attempt one native connect; shared quota bounds the SDK's interrupted-connect retry loop.
     * @param socket SDK-owned handle.
     * @param address Borrowed numeric server sockaddr.
     * @param size Exact sockaddr bytes.
     * @param flags Only zero is supported.
     * @param argument Stable resolver.
     * @return Native result or non-EINTR would-block before quota-exhausted retries.
     */
    static int connectSocket(ares_socket_t socket, const sockaddr *address, ares_socklen_t size, unsigned flags,
                             void *argument) noexcept;
    /** Receive one bounded complete datagram or partial TCP chunk without retrying native errors.
     * @param socket Tracked descriptor.
     * @param buffer SDK-owned destination backing.
     * @param length Destination capacity.
     * @param flags Native receive flags.
     * @param address Optional source address.
     * @param addressLength Optional source-address capacity/result.
     * @param argument Stable resolver.
     * @return Native received bytes or would-block before any exhausted quota consumes data.
     */
    static ares_ssize_t receive(ares_socket_t socket, void *buffer, std::size_t length, int flags, sockaddr *address,
                                ares_socklen_t *addressLength, void *argument) noexcept;
    /** Send one bounded SDK output suffix without retrying a native error.
     * @param socket Tracked descriptor.
     * @param buffer SDK-owned immutable output suffix.
     * @param length Requested bytes, debited conservatively before native submission.
     * @param flags Native send flags.
     * @param address Optional explicit numeric destination.
     * @param addressLength Destination sockaddr bytes.
     * @param argument Stable resolver.
     * @return Native accepted bytes or would-block before another attempt.
     */
    static ares_ssize_t send(ares_socket_t socket, const void *buffer, std::size_t length, int flags,
                             const sockaddr *address, ares_socklen_t addressLength, void *argument) noexcept;
    /** Inspect the local native address after call admission.
     * @param socket Tracked descriptor.
     * @param address Caller-provided bounded sockaddr storage.
     * @param length Capacity/result bytes.
     * @param argument Stable resolver.
     * @return Native result or would-block without another attempt.
     */
    static int localAddress(ares_socket_t socket, sockaddr *address, ares_socklen_t *length, void *argument) noexcept;

    public:
    static constexpr std::size_t ENVELOPE_BYTES = 80 * 1024 * 1024; ///< Conditional source-derived SDK payload ceiling.

    /** Create only fixed inline state; initialize performs fallible SDK preparation.
     * @param sendAttempt Optional finite internal native-send seam; null selects the single system call.
     * @param sendContext Borrowed seam context, alive through resolver destruction; not exposed publicly.
     * @param connectAttempt Optional finite internal native-connect seam, absent from the public runtime path.
     * @param connectContext Borrowed interruption-seam context, alive through resolver destruction.
     */
    explicit DnsResolver(DnsSendAttempt sendAttempt = nullptr, void *sendContext = nullptr,
                         DnsConnectAttempt connectAttempt = nullptr, void *connectContext = nullptr) noexcept
        : sendAttempt_(sendAttempt), sendContext_(sendContext), connectAttempt_(connectAttempt),
          connectContext_(connectContext) {
    }

    DnsResolver(const DnsResolver &) = delete;
    DnsResolver &operator=(const DnsResolver &) = delete;
    /** Destroy the remaining channel outside every SDK callback. */
    ~DnsResolver();
    /** Validate the selected ordinary ASCII/A-label presentation syntax without OS/IDNA services.
     * @param name Borrowed finite name, optionally ending in a root dot.
     * @return True when every label is nonempty, at most63 bytes and has valid ASCII label syntax.
     */
    [[nodiscard]] static bool validName(std::string_view name) noexcept;
    /** Inspect conservative channel-initialization work before SDK allocation.
     * @param servers Explicit validated server count in1..4.
     * @return Preparation-only bytes/visits; protocol construction needs independent additional credits.
     */
    [[nodiscard]] static protocol::LiveWork preparationWork(std::size_t servers) noexcept;
    /** Prepare one exact qualified channel before any protocol-configuration ownership transfer.
     * @param options Explicit finite options; all retained values are copied into owned SDK/fixed storage.
     * @return Prepared channel or static failure; no DNS query/socket is started.
     */
    [[nodiscard]] std::expected<void, TcpDiagnostic> initialize(const TcpOptions &options);
    /** Perform one SDK entry or one deferred family-transition action; next queries never start in callbacks.
     * @param work Mutable caller-turn DNS credits; each native attempt is charged before invocation.
     * @return Progress or yield, with failure available separately; no application code is invoked.
     */
    [[nodiscard]] TcpPollStatus progress(TcpWork &work) noexcept;
    /** Retire this channel synchronously, retaining callback context through every destruction callback. */
    void close() noexcept;
    /** Inspect selected sufficient per-entry runtime/DNS credits, separate from session/latency bounds.
     * @return Conservative source/bridge/copy work allowance for one full SDK progress action.
     */
    [[nodiscard]] TcpWork workRequirement() const noexcept;
    /** Inspect whether another bounded metadata/family-submission step can proceed without a readiness tick.
     * @return True before first query, after a completion, or at the final family transition.
     */
    [[nodiscard]] bool immediate() const noexcept;
    /** Inspect whether both serial family results have been processed.
     * @return True after the final successful/empty family result is processed.
     */
    [[nodiscard]] bool finished() const noexcept;
    /** Inspect the one submitted query's callback lifetime.
     * @return True until its completion/destruction callback has returned.
     */
    [[nodiscard]] bool pending() const noexcept;
    /** Borrow copied eligible addresses, independent of any SDK response lifetime.
     * @return Fixed owned result prefix, valid through resolver destruction.
     */
    [[nodiscard]] std::span<const ResolvedAddress> addresses() const noexcept;
    /** Inspect first static failure without retaining response text.
     * @return Empty while usable, otherwise a retirement-required cause.
     */
    [[nodiscard]] std::optional<TcpDiagnostic> failure() const noexcept;
    /** Inspect the conservative SDK payload reservation while the channel still exists.
     * @return ENVELOPE_BYTES until actual channel destruction, otherwise zero.
     */
    [[nodiscard]] std::size_t envelopeBytes() const noexcept;
};

} // namespace org::ttldtor::qdcxx::transport::detail
