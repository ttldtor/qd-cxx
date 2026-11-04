#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace org::ttldtor::qdcxx::time::detail::provider {

/** Private provider error boundary; no exception crosses the SDK API. */
enum class Error { NONE, INVALID_ARGUMENT, MISSING_DATA, BAD_DATA, LIMIT, ALLOCATION, PROVIDER, NOT_READY };

/** Opaque immutable native zone reference; never appears in an installed public header. */
struct Zone {
    const void *native = nullptr;
};

/** Logical admission/model limits, not an exact native heap or RSS quota. */
struct Limits {
    std::size_t files = 11;
    std::size_t totalBytes = 1100000;
    std::size_t lineBytes = 1024;
    std::size_t lines = 35000;
    std::size_t records = 12000;
    std::size_t zones = 600;
    std::size_t links = 400;
    std::size_t rules = 10000;
    std::size_t contexts = 1;
    std::size_t generations = 1;
    std::size_t modelUnits = 32768;
};

/** Fixed retained logical model statistics after all zones are eagerly initialized. */
struct Stats {
    std::size_t bytes;
    std::size_t lines;
    std::size_t records;
    std::size_t zones;
    std::size_t links;
    std::size_t rules;
    std::size_t modelUnits;
};

/** Admit the exact profile from one explicit root and publish once after native validation.
 * @param root Explicit data directory; no current-directory/home/OS fallback.
 * @param limits Finite logical limits; all must admit the fixed profile.
 * @param stats Receives model statistics only after success.
 * @return Typed failure; failed initialization publishes no usable context.
 */
Error initialize(const std::filesystem::path &root, Limits limits, Stats &stats) noexcept;

/** Convert one proleptic local millisecond value with Calendar gap/later-overlap policy.
 * @param zone Explicit IANA ID or the named Java-compatible aliases/custom offsets qualified here.
 * @param localMs Local fields expressed as signed milliseconds since the civil epoch.
 * @param utcMs Receives the converted instant on success.
 * @return Typed failure; no provider references or external types escape.
 */
Error localToUtc(const char *zone, std::int64_t localMs, std::int64_t &utcMs) noexcept;

/** Return a copied UTC offset without exposing a native reference.
 * @param zone Explicit qualified zone identity.
 * @param utcMs Signed Unix milliseconds.
 * @param seconds Receives the offset on success.
 * @return Typed failure, including a missing published context.
 */
Error offsetAt(const char *zone, std::int64_t utcMs, std::int64_t &seconds) noexcept;

/** @param canonical Explicit mapped IANA identity from the tracked compatibility profile.
 * @param zone Receives a stable native reference after successful lookup.
 * @return Typed refusal; this operation is completed for all named identities before public publication.
 */
Error resolve(const char *canonical, Zone &zone) noexcept;

/** @param zone Ready resolved native reference.
 * @return Stable native canonical name; no allocation or discovery occurs.
 */
const char *canonicalName(Zone zone) noexcept;

/** @param zone Ready resolved native reference.
 * @param utcMs Explicit signed instant within the checked private native envelope.
 * @param offsetSeconds Receives actual UTC offset seconds.
 * @param saveSeconds Receives native SAVE seconds; negative SAVE requires separate Calendar conventions.
 * @return Complete copied results or typed refusal, without retained query growth.
 */
Error offsets(Zone zone, std::int64_t utcMs, std::int64_t &offsetSeconds, std::int64_t &saveSeconds) noexcept;

/** @param zone Ready resolved native reference.
 * @param localMs Proleptic local millisecond fields within the checked private envelope.
 * @param firstSeconds Receives the earlier interval's offset.
 * @param secondSeconds Receives the later interval's offset for gaps/overlaps, or first for unique times.
 * @param result Receives0 unique,1 nonexistent or2 ambiguous as scalar values.
 * @return Complete copied results or typed refusal.
 */
Error localOffsets(Zone zone, std::int64_t localMs, std::int64_t &firstSeconds, std::int64_t &secondSeconds,
                   int &result) noexcept;

/** @return Last checked UTC millisecond, leaving one native following-year search margin. */
std::int64_t maximumUtcMillis() noexcept;
/** @param utcMillis UTC instant after the legacy transition table, inside the checked envelope.
 * @param rawSeconds Fixed legacy standard base.
 * @param springUtc Receives last-Sunday-April02:00wall as UTC milliseconds.
 * @param autumnUtc Receives last-Sunday-October02:00wall as UTC milliseconds.
 * @return Whether both scalar boundaries were computed in the supported envelope.
 */
bool legacyBoundaries(std::int64_t utcMillis, std::int64_t rawSeconds, std::int64_t &springUtc,
                      std::int64_t &autumnUtc) noexcept;

} // namespace org::ttldtor::qdcxx::time::detail::provider
