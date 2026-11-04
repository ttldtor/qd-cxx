// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/ipf/instrument_profile_url_reader.hpp"

#include <array>

namespace org::ttldtor::qdcxx::ipf::detail {

inline constexpr std::size_t SOURCE_CAPACITY = 8192; ///< Fixed address storage; never contains initial userinfo.

/** Stable sanitized address or decoded local path, with no view into caller storage. */
struct TransferSource {
    std::array<char, SOURCE_CAPACITY> bytes{};   ///< Fixed sanitized URI or local UTF-8 path.
    std::size_t size = 0;                        ///< Initialized address/path bytes.
    std::array<char, 256> host{};                ///< Canonical route/reference host without brackets.
    std::size_t hostSize = 0;                    ///< Canonical host byte count.
    std::uint16_t port = 0;                      ///< Effective origin port.
    bool secure = false;                         ///< Selected HTTPS transport.
    bool local = false;                          ///< Local binary-file dispatch.
    std::optional<HttpAuthorization> userinfo{}; ///< Extracted independent secret, never part of bytes.
    /** Borrow initialized sanitized source storage.
     * @return View valid until this value changes or retires; query remains unloggable.
     */
    [[nodiscard]] std::string_view address() const noexcept;
    /** Borrow canonical origin host.
     * @return View valid while this owning value remains unchanged.
     */
    [[nodiscard]] std::string_view hostname() const noexcept;
};

/** Classify and copy only a sanitized source or local path before I/O.
 * @param source Borrowed initial text, never retained.
 * @param limits Finite source/secret bounds.
 * @param extractUserinfo False when an explicit selected credential overrides URL userinfo.
 * @return Owning checked source or static failure.
 */
[[nodiscard]] std::expected<TransferSource, IpfTransferDiagnostic>
parseTransferSource(std::string_view source, const IpfTransferLimits &limits, bool extractUserinfo);

/** Bound simultaneous fixed source/resolve owners, including non-elided by-value handoffs.
 * Covers four complete source-result slots, four encoded AddressBuilders, parsed-view backing
 * and the separately retained Location copy. Native local-file path workspace is charged separately.
 * @return Conservative requested logical backing in bytes, independent of allocator bookkeeping/RSS.
 */
[[nodiscard]] std::size_t transferSourceWorkingBytes() noexcept;
/** Resolve one HTTP(S)-only redirect using encoded RFC3986 components and fixed backing.
 * @param base Owning sanitized HTTP(S) base.
 * @param location Borrowed bounded Location value.
 * @param limits Finite resolved source/Location bounds.
 * @return Owning validated destination; percent-encoded dots/slashes are not decoded.
 */
[[nodiscard]] std::expected<TransferSource, IpfTransferDiagnostic>
resolveTransferSource(const TransferSource &base, std::string_view location, const IpfTransferLimits &limits);

/** Compare canonical scheme/host/effective-port origins, independently of DNS route.
 * @param left First sanitized origin.
 * @param right Second sanitized origin.
 * @return True exactly for the same allowed origin tuple.
 */
[[nodiscard]] bool sameOrigin(const TransferSource &left, const TransferSource &right) noexcept;

/** Validate a whole HTTP Authorization field without allocation.
 * @param value Whole scheme-plus-credential bytes.
 * @param maximum Maximum admitted value bytes.
 * @return True for nonempty token scheme and noncontrol nonempty credential.
 */
[[nodiscard]] bool validAuthorization(std::string_view value, std::size_t maximum) noexcept;

/** Parse the three bounded HTTP-date forms without locale or ambient timezone access.
 * @param value Borrowed HTTP date; obsolete two-digit years use the current UTC fifty-year rule.
 * @return UTC timestamp, or empty for invalid/unsupported syntax.
 */
[[nodiscard]] std::optional<std::chrono::sys_seconds> parseHttpDate(std::string_view value) noexcept;

/** Check the supported HTTP timestamp range without allocation.
 * @param value Explicit UTC timestamp.
 * @return True in supported years1601..9999.
 */
[[nodiscard]] bool httpDateRepresentable(std::chrono::sys_seconds value) noexcept;
/** Serialize explicit UTC conditional metadata without locale/properties.
 * @param value Valid representable UTC date.
 * @return Fixed-format owned IMF-fixdate, empty for an unsupported year.
 */
[[nodiscard]] std::string formatHttpDate(std::chrono::sys_seconds value);

} // namespace org::ttldtor::qdcxx::ipf::detail
