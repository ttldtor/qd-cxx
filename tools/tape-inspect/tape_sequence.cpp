// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tape_sequence.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
constexpr std::size_t COMPACT_PREFIX_BYTES = 9;      ///< Largest compact-long envelope prefix.
constexpr std::size_t HEX_BYTES_PER_NATIVE_BYTE = 2; ///< Safe path encoding never invokes a locale.
constexpr std::size_t FIXED_DIAGNOSTICS = 4;         ///< Primary plus data-close, time-close and flush causes.
constexpr std::size_t TERMINAL_RESOURCE_ROWS = 92;   ///< Fixed sequence inventory, independent of source count.

/** Unevaluated aggregate-field probe; never constructed or converted at runtime. */
struct LimitFieldProbe {
    template <typename T> operator T() const;
};

/** Detect aggregate arity so adding a nested limit requires updating the reservation inventory.
 * @tparam T Existing aggregate limit type, without inherited fields or arrays.
 * @tparam I Zero-based initializer placeholders.
 * @return Whether this exact number of direct aggregate initializers is accepted.
 */
template <typename T, std::size_t... I> constexpr bool acceptsLimitFields(std::index_sequence<I...>) noexcept {
    return requires { T{(static_cast<void>(I), LimitFieldProbe{})...}; };
}

/** Check an existing limit type's reviewed direct-field inventory at compile time.
 * @tparam T Existing aggregate limit type.
 * @tparam N Reviewed number of direct fields, including nested aggregate objects.
 * @return True only when N fields fit and a further field does not.
 */
template <typename T, std::size_t N> constexpr bool hasLimitFields() noexcept {
    return std::is_aggregate_v<T> && acceptsLimitFields<T>(std::make_index_sequence<N>{}) &&
           !acceptsLimitFields<T>(std::make_index_sequence<N + 1>{});
}

static_assert(hasLimitFields<protocol::TapeLimits, 9>());
static_assert(hasLimitFields<protocol::FrameLimits, 2>());
static_assert(hasLimitFields<protocol::TapeDescriptorLimits, 4>());
static_assert(hasLimitFields<protocol::ContextLimits, 3>());
static_assert(hasLimitFields<core::SchemaLimits, 5>());
static_assert(hasLimitFields<protocol::BindingLimits, 3>());
static_assert(hasLimitFields<protocol::StreamDataLimits, 17>());
static_assert(hasLimitFields<encoding::SymbolLimits, 3>());
static_assert(hasLimitFields<encoding::UtfStringLimits, 2>());
static_assert(hasLimitFields<encoding::CharArrayLimits, 2>());
static_assert(hasLimitFields<ContainerLimits, 13>());
static_assert(hasLimitFields<SidecarLimits, 6>());

/** Checked scalar reservation; the first cause remains sticky and no capacity is refunded. */
class Reservation {
    SequenceDiagnostic &diagnostic_;

    public:
    /** Retain only the fixed result diagnostic during synchronous validation.
     * @param diagnostic Owning result diagnostic, valid throughout this helper's lifetime.
     */
    explicit Reservation(SequenceDiagnostic &diagnostic) noexcept : diagnostic_(diagnostic) {
    }

    /** Admit an unsigned scalar before addition, keeping overflow distinct from a finite refusal.
     * @tparam T Unsigned counter type with numeric_limits, shared by used/count/limit.
     * @param used Previously reserved full capacity, changed only on success.
     * @param count Additional full declared capacity.
     * @param limit Explicit aggregate allowance.
     * @param component Static checkpoint literal; never an input string.
     */
    template <typename T> void add(T &used, T count, T limit, std::string_view component) noexcept {
        static_assert(std::is_unsigned_v<T>);

        if (diagnostic_.error != SequenceError::NONE) {
            return;
        }

        if (count > std::numeric_limits<T>::max() - used) {
            diagnostic_.error = SequenceError::UNREPRESENTABLE;
            diagnostic_.component = component;

            return;
        }

        if (used > limit || count > limit - used) {
            diagnostic_.error = SequenceError::RESOURCE_LIMIT;
            diagnostic_.component = component;

            return;
        }

        used += count;
    }
};

/** Reserve every nested parser field, including per-value and retained peak capacities.
 * @param sum Checked accumulated capacities.
 * @param part Literal capacities of this occurrence.
 * @param limit Explicit aggregate capacities.
 * @param reservation Sticky checked scalar operation.
 */
void reserveTape(protocol::TapeLimits &sum, const protocol::TapeLimits &part, const protocol::TapeLimits &limit,
                 Reservation &reservation) noexcept {
#define QDCXX_RESERVE_TAPE(field) reservation.add(sum.field, part.field, limit.field, "tape." #field)
    QDCXX_RESERVE_TAPE(frame.maximumBodyBytes);
    QDCXX_RESERVE_TAPE(frame.maximumPendingBytes);
    QDCXX_RESERVE_TAPE(inputBytes);
    QDCXX_RESERVE_TAPE(frames);
    QDCXX_RESERVE_TAPE(descriptor.properties);
    QDCXX_RESERVE_TAPE(descriptor.messages);
    QDCXX_RESERVE_TAPE(descriptor.text.bodyBytes);
    QDCXX_RESERVE_TAPE(descriptor.text.codeUnits);
    QDCXX_RESERVE_TAPE(descriptor.textUnits);
    QDCXX_RESERVE_TAPE(context.schema.records);
    QDCXX_RESERVE_TAPE(context.schema.fields);
    QDCXX_RESERVE_TAPE(context.schema.lookups);
    QDCXX_RESERVE_TAPE(context.schema.nameUnits);
    QDCXX_RESERVE_TAPE(context.schema.totalNameUnits);
    QDCXX_RESERVE_TAPE(context.descriptions);
    QDCXX_RESERVE_TAPE(context.bindings.fields);
    QDCXX_RESERVE_TAPE(context.bindings.nameUnits);
    QDCXX_RESERVE_TAPE(context.bindings.totalNameUnits);
    QDCXX_RESERVE_TAPE(constructionUnits);
    QDCXX_RESERVE_TAPE(data.payloadBytes);
    QDCXX_RESERVE_TAPE(data.processedRecords);
    QDCXX_RESERVE_TAPE(data.wireFields);
    QDCXX_RESERVE_TAPE(data.emittedRecords);
    QDCXX_RESERVE_TAPE(data.emittedValues);
    QDCXX_RESERVE_TAPE(data.ownedSymbolUnits);
    QDCXX_RESERVE_TAPE(data.symbol.inputBytes);
    QDCXX_RESERVE_TAPE(data.symbol.codeUnits);
    QDCXX_RESERVE_TAPE(data.symbol.stateUnits);
    QDCXX_RESERVE_TAPE(data.fieldString.encodedBytes);
    QDCXX_RESERVE_TAPE(data.fieldString.codeUnits);
    QDCXX_RESERVE_TAPE(data.fieldBytes);
    QDCXX_RESERVE_TAPE(data.fieldUnits);
    QDCXX_RESERVE_TAPE(data.ownedFieldUnits);
    QDCXX_RESERVE_TAPE(data.fieldPayloadEncodedBytes);
    QDCXX_RESERVE_TAPE(data.fieldPayloadBytes);
    QDCXX_RESERVE_TAPE(data.payloadEncodedBytes);
    QDCXX_RESERVE_TAPE(data.payloadWorkBytes);
    QDCXX_RESERVE_TAPE(data.ownedPayloadBytes);
    QDCXX_RESERVE_TAPE(data.ownedFieldTimeRecords);
    QDCXX_RESERVE_TAPE(timingEvents);
    QDCXX_RESERVE_TAPE(snapshotUnits);
#undef QDCXX_RESERVE_TAPE
}

/** Reserve the entire envelope vector for either selected source of a pair.
 * @param sum Checked accumulated capacities for this source role.
 * @param part Literal capacities, including NONE containers.
 * @param limit Explicit aggregate capacities for this role.
 * @param reservation Sticky checked scalar operation.
 */
void reserveContainer(ContainerLimits &sum, const ContainerLimits &part, const ContainerLimits &limit,
                      Reservation &reservation) noexcept {
#define QDCXX_RESERVE_CONTAINER(field) reservation.add(sum.field, part.field, limit.field, "container." #field)
    QDCXX_RESERVE_CONTAINER(compressedBytes);
    QDCXX_RESERVE_CONTAINER(inflatedBytes);
    QDCXX_RESERVE_CONTAINER(expansionRatio);
    QDCXX_RESERVE_CONTAINER(ratioSlack);
    QDCXX_RESERVE_CONTAINER(gzipMembers);
    QDCXX_RESERVE_CONTAINER(zipEntries);
    QDCXX_RESERVE_CONTAINER(headerBytes);
    QDCXX_RESERVE_CONTAINER(totalHeaderBytes);
    QDCXX_RESERVE_CONTAINER(workBytes);
    QDCXX_RESERVE_CONTAINER(inflateCalls);
    QDCXX_RESERVE_CONTAINER(retainedBytes);
    QDCXX_RESERVE_CONTAINER(inflaterBytes);
    QDCXX_RESERVE_CONTAINER(chunkBytes);
#undef QDCXX_RESERVE_CONTAINER
}

/** Reserve the scanner only when an explicit companion is selected.
 * @param sum Checked accumulated scanner capacities.
 * @param part Literal scanner capacities of this occurrence.
 * @param limit Explicit aggregate scanner capacities.
 * @param reservation Sticky checked scalar operation.
 */
void reserveScanner(SidecarLimits &sum, const SidecarLimits &part, const SidecarLimits &limit,
                    Reservation &reservation) noexcept {
#define QDCXX_RESERVE_SCANNER(field) reservation.add(sum.field, part.field, limit.field, "scanner." #field)
    QDCXX_RESERVE_SCANNER(inputBytes);
    QDCXX_RESERVE_SCANNER(lineBytes);
    QDCXX_RESERVE_SCANNER(lines);
    QDCXX_RESERVE_SCANNER(marks);
    QDCXX_RESERVE_SCANNER(workBytes);
    QDCXX_RESERVE_SCANNER(retainedBytes);
#undef QDCXX_RESERVE_SCANNER
}

/** Check existing envelope configuration without constructing an adapter or accessing a source.
 * @param kind Explicit selected envelope.
 * @param limits Literal per-instance configuration.
 * @return Whether the same representability/storage prerequisites used by the adapter hold.
 */
bool validContainer(ContainerKind kind, const ContainerLimits &limits) noexcept {
    if (kind != ContainerKind::NONE && kind != ContainerKind::GZIP && kind != ContainerKind::ZIP) {
        return false;
    }

    return limits.chunkBytes != 0 && limits.chunkBytes <= std::numeric_limits<unsigned int>::max() &&
           limits.chunkBytes <= static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) &&
           limits.chunkBytes <= limits.retainedBytes &&
           limits.compressedBytes != std::numeric_limits<std::uint64_t>::max() &&
           limits.inflatedBytes != std::numeric_limits<std::uint64_t>::max();
}

/** Validate exact effective parser capacities, without prepareLimits' derived increases.
 * @param limits Fully resolved literal limits supplied by the caller.
 * @return Whether feed IO and initial discovery/frame storage are representable and compatible.
 */
bool validParser(const Limits &limits) noexcept {
    const auto &frame = limits.tape.frame;

    if (limits.chunkSize == 0 ||
        limits.chunkSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
        frame.maximumBodyBytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        frame.maximumBodyBytes > std::numeric_limits<std::size_t>::max() - COMPACT_PREFIX_BYTES) {
        return false;
    }

    return frame.maximumPendingBytes >= frame.maximumBodyBytes + COMPACT_PREFIX_BYTES &&
           limits.tape.constructionUnits != 0 && limits.tape.snapshotUnits != 0;
}

/** Admit one native path without conversion, copying, normalization or filesystem access.
 * @param path Caller-owned native path, borrowed synchronously.
 * @param result Running fixed admission result.
 * @param options Explicit name/storage limits.
 * @param reservation Sticky checked scalar operation.
 */
void reservePath(const std::filesystem::path &path, SequenceAdmission &result, const TapeSequenceOptions &options,
                 Reservation &reservation) noexcept {
    const auto &native = path.native();

    if (native.empty() || native.find(std::filesystem::path::value_type{}) != native.npos) {
        result.diagnostic.error = SequenceError::INVALID_ARGUMENT;
        result.diagnostic.component = "path";

        return;
    }

    if (native.size() > options.pathNativeUnits) {
        result.diagnostic.error = SequenceError::RESOURCE_LIMIT;
        result.diagnostic.component = "path.nativeUnits";

        return;
    }

    constexpr auto NATIVE_BYTES = sizeof(std::filesystem::path::value_type);
    constexpr auto ENCODED_BYTES = NATIVE_BYTES * HEX_BYTES_PER_NATIVE_BYTE;
    const auto maximum = std::numeric_limits<std::size_t>::max();

    if (native.size() > maximum / ENCODED_BYTES || native.size() > maximum / NATIVE_BYTES - 1) {
        result.diagnostic.error = SequenceError::UNREPRESENTABLE;
        result.diagnostic.component = "path.bytes";

        return;
    }

    reservation.add(result.pathNativeUnits, native.size(), options.totalPathNativeUnits, "paths.nativeUnits");
    reservation.add(result.pathEncodedBytes, native.size() * ENCODED_BYTES, options.pathEncodedBytes,
                    "paths.encodedBytes");
    reservation.add(result.pathStorageBytes, (native.size() + 1) * NATIVE_BYTES, options.pathStorageBytes,
                    "paths.storageBytes");
}
} // namespace

SequenceAdmission admitSequence(std::span<const TapeSegmentSpec> segments,
                                const TapeSequenceOptions &options) noexcept {
    SequenceAdmission result;
    Reservation reservation(result.diagnostic);

    if (segments.empty()) {
        result.diagnostic = {SequenceError::EMPTY_SEQUENCE, 0, "occurrences"};

        return result;
    }

    if (options.time != SourceTimePolicy::NONE && options.time != SourceTimePolicy::MESSAGE &&
        options.time != SourceTimePolicy::FIELD && options.time != SourceTimePolicy::LONG &&
        options.time != SourceTimePolicy::TEXT) {
        result.diagnostic = {SequenceError::INVALID_ARGUMENT, 0, "time"};

        return result;
    }

    const auto terminalMinimum = terminalReserveWorstCase(TERMINAL_RESOURCE_ROWS, FIXED_DIAGNOSTICS);

    if (!terminalMinimum || options.terminalBytes < *terminalMinimum || segments.size() > options.occurrences ||
        options.diagnosticBytes < FIXED_DIAGNOSTICS * sizeof(SourceDiagnostic) ||
        options.terminalBytes > options.outputBytes) {
        result.diagnostic = {SequenceError::RESOURCE_LIMIT, 0, "sequence.configuration"};

        return result;
    }

    if (segments.size() > std::numeric_limits<std::size_t>::max() / sizeof(TapeSegmentSpec)) {
        result.diagnostic = {SequenceError::UNREPRESENTABLE, 0, "paths.objects"};

        return result;
    }

    reservation.add(result.pathStorageBytes, segments.size() * sizeof(TapeSegmentSpec), options.pathStorageBytes,
                    "paths.objects");

    const bool paired = options.time == SourceTimePolicy::LONG || options.time == SourceTimePolicy::TEXT;

    if (options.sourceHandles < (paired ? 2U : 1U)) {
        result.diagnostic = {SequenceError::RESOURCE_LIMIT, 0, "sourceHandles"};

        return result;
    }

    for (std::size_t ordinal = 0; ordinal < segments.size(); ++ordinal) {
        if (result.diagnostic.error != SequenceError::NONE) {
            break;
        }

        result.diagnostic.sourceOrdinal = ordinal;
        const auto &segment = segments[ordinal];

        if (segment.companion.has_value() != paired) {
            result.diagnostic = {SequenceError::UNSUPPORTED_SOURCE_PROFILE, ordinal, "companion.selection"};

            break;
        }

        if (!validParser(segment.limits) || !validContainer(segment.container, segment.containerLimits)) {
            result.diagnostic = {SequenceError::INVALID_ARGUMENT, ordinal, "data.configuration"};

            break;
        }

        if (!containerAvailable(segment.container)) {
            result.diagnostic = {SequenceError::UNSUPPORTED_CONTAINER, ordinal, "data.container"};

            break;
        }

        reservePath(segment.path, result, options, reservation);
        reserveTape(result.reserved.tape, segment.limits.tape, options.aggregate.tape, reservation);
        reserveContainer(result.reserved.dataContainer, segment.containerLimits, options.aggregate.dataContainer,
                         reservation);
        reservation.add(result.reserved.chunkBytes, segment.limits.chunkSize, options.aggregate.chunkBytes,
                        "chunkBytes");
        reservation.add(result.reserved.outputBytes, segment.limits.output, options.aggregate.outputBytes,
                        "outputBytes");
        reservation.add(result.reserved.eventUnits, segment.limits.eventUnits, options.aggregate.eventUnits,
                        "eventUnits");
        reservation.add(result.reserved.physicalProbes, std::uint64_t{1}, options.aggregate.physicalProbes,
                        "physicalProbes");
        reservation.add(result.reserved.inflatedProbes, std::uint64_t{1}, options.aggregate.inflatedProbes,
                        "inflatedProbes");

        if (result.diagnostic.error != SequenceError::NONE) {
            break;
        }

        if (segment.companion) {
            const auto &companion = *segment.companion;

            if (!validContainer(companion.container, companion.containerLimits) ||
                companion.scannerLimits.lineBytes >
                    static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) ||
                companion.scannerLimits.lineBytes > companion.scannerLimits.retainedBytes) {
                result.diagnostic = {SequenceError::INVALID_ARGUMENT, ordinal, "companion.configuration"};

                break;
            }

            if (!containerAvailable(companion.container)) {
                result.diagnostic = {SequenceError::UNSUPPORTED_CONTAINER, ordinal, "companion.container"};

                break;
            }

            reservePath(companion.path, result, options, reservation);
            reserveContainer(result.reserved.companionContainer, companion.containerLimits,
                             options.aggregate.companionContainer, reservation);
            reserveScanner(result.reserved.scanner, companion.scannerLimits, options.aggregate.scanner, reservation);
            reservation.add(result.reserved.physicalProbes, std::uint64_t{1}, options.aggregate.physicalProbes,
                            "physicalProbes");
            reservation.add(result.reserved.inflatedProbes, std::uint64_t{1}, options.aggregate.inflatedProbes,
                            "inflatedProbes");
        }
    }

    if (result.diagnostic.error == SequenceError::NONE &&
        result.reserved.outputBytes > options.outputBytes - options.terminalBytes) {
        result.diagnostic.error = SequenceError::RESOURCE_LIMIT;
        result.diagnostic.component = "output.terminalReservation";
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::tape_inspect
