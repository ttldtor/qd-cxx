// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "source_cli.hpp"
#include "tape_sequence.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#    include <corecrt_startup.h>
#    include <cstdlib>
#endif

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
constexpr std::size_t MAX_RESERVATION_COPIES = 256;
constexpr std::size_t GLOBAL_OUTPUT_BYTES = 67108864;
constexpr std::size_t SOURCE_OUTPUT_BYTES = 16777216;
constexpr std::size_t SOURCE_PATH_OUTPUT_BYTES = 4194304;
constexpr std::size_t TERMINAL_BYTES = 65536;
/** Existing wire-CLI defaults, independent of whether the optional JSON loader is linked. */
constexpr core::SchemaLimits WIRE_SCHEMA_LIMITS{1024, 16384, 32768, 1024, 1048576};

/** Parse only unsigned decimal digits without narrowing, signs or ambient locale.
 * @tparam Char Native argument code-unit type, char or wchar_t.
 * @tparam T Unsigned integral destination capacity.
 * @param text Borrowed decimal operand, requiring at least one digit.
 * @param result Assigned only after the complete representable operand is accepted.
 * @return False for signs, non-digits, empty input or overflow.
 */
template <typename Char, typename T> bool parseCount(std::basic_string_view<Char> text, T &result) noexcept {
    if (text.empty()) {
        return false;
    }

    T value = 0;

    for (auto digit : text) {
        if (digit < '0' || digit > '9' || value > (std::numeric_limits<T>::max() - (digit - '0')) / 10) {
            return false;
        }

        value = value * 10 + (digit - '0');
    }

    result = value;

    return true;
}

/** Compare option tokens as ASCII; native path operands are never transcoded.
 * @tparam Char Native argument code-unit type, char or wchar_t.
 * @param value Borrowed native argument token.
 * @param expected Static ASCII option or value spelling.
 * @return True only for equal code units and length.
 */
template <typename Char> bool token(std::basic_string_view<Char> value, std::string_view expected) noexcept {
    return value.size() == expected.size() && std::equal(value.begin(), value.end(), expected.begin());
}

/** Resolve diagnostic bytes before normal argument validation, without copying names or allocating.
 * Only key positions are examined; an operand that spells an option remains an operand.
 * Invalid values grant no allowance. Duplicate malformed requests use the smallest valid explicit cap.
 * @tparam Char Native argument code-unit type, char or wchar_t.
 * @param argc Process argument count including the executable.
 * @param argv Borrowed process argument pairs, never retained.
 * @return Smallest valid explicit output cap, or the finite default if none is valid.
 */
template <typename Char> std::size_t diagnosticOutputLimit(int argc, const Char *const *argv) noexcept {
    std::size_t budget = GLOBAL_OUTPUT_BYTES;
    bool explicitBudget = false;

    if (argc <= 1) {
        return budget;
    }

    for (int index = 1; index < argc - 1; index += 2) {
        if (!token(std::basic_string_view<Char>(argv[index]), "--max-output")) {
            continue;
        }

        std::size_t candidate = 0;

        if (parseCount(std::basic_string_view<Char>(argv[index + 1]), candidate)) {
            budget = explicitBudget ? std::min(budget, candidate) : candidate;
            explicitBudget = true;
        }
    }

    return budget;
}

/** Fixed CLI configuration with literal finite defaults; no source names are retained. */
struct Configuration {
    Limits parser;                                  ///< Fully resolved parser, feed and per-source output limits.
    ContainerLimits data;                           ///< Independent data envelope capacities.
    ContainerLimits time;                           ///< Independent companion envelope capacities.
    SidecarLimits scanner;                          ///< Companion scanner capacities.
    TapeSequenceOptions sequence;                   ///< Sequence-wide ownership and aggregate configuration.
    std::size_t copies = 1;                         ///< Explicit aggregate capacity multiplier, 1..256.
    std::size_t globalOutput = GLOBAL_OUTPUT_BYTES; ///< One encoded stdout+stderr allowance, never multiplied.
    std::uint64_t physicalProbes = 2;               ///< Unmultiplied aggregate physical excess-probe bytes.
    std::uint64_t inflatedProbes = 2;               ///< Unmultiplied aggregate discarded inflated excess-probe bytes.
    ContainerKind dataKind = ContainerKind::NONE;   ///< Explicit shared data envelope.
    ContainerKind timeKind = ContainerKind::NONE;   ///< Explicit shared companion envelope.

    /** Initialize the source-profile defaults without allocating or consulting ambient state. */
    Configuration() {
        parser.output = SOURCE_OUTPUT_BYTES;
        parser.tape.context.schema = WIRE_SCHEMA_LIMITS;
        parser.tape.data.fieldPayloadEncodedBytes = 65536;
        parser.tape.data.fieldPayloadBytes = 65536;
        parser.tape.data.payloadEncodedBytes = 16777216;
        parser.tape.data.payloadWorkBytes = 16777216;
        parser.tape.data.ownedPayloadBytes = 8388608;
        parser.tape.snapshotUnits = 16777216;
        parser.tape.data.ownedFieldTimeRecords = 1048576;
        sequence.pathEncodedBytes = SOURCE_PATH_OUTPUT_BYTES;
    }
};

/** A numeric option borrows a member of one synchronous configuration. */
struct NumericOption {
    std::string_view name;      ///< Static ASCII option spelling.
    std::size_t *value;         ///< Borrowed mutable member of the synchronous configuration.
    bool companionOnly = false; ///< Whether use requires LONG or TEXT.
};

/** Return every existing parser option supported by binary discovery plus source ownership bounds.
 * @param config Mutable synchronous configuration whose lifetime exceeds the returned array.
 * @return Fixed owning option array, with pointers borrowing config members.
 */
auto numericOptions(Configuration &config) {
    auto &limits = config.parser;
    auto &t = limits.tape;
    auto &sequence = config.sequence;

    return std::array{NumericOption{"--chunk-size", &limits.chunkSize},
                      NumericOption{"--max-output", &config.globalOutput},
                      NumericOption{"--max-source-output", &limits.output},
                      NumericOption{"--max-event-units", &limits.eventUnits},
                      NumericOption{"--max-schema-records", &t.context.schema.records},
                      NumericOption{"--max-schema-fields", &t.context.schema.fields},
                      NumericOption{"--max-schema-lookups", &t.context.schema.lookups},
                      NumericOption{"--max-schema-name-units", &t.context.schema.nameUnits},
                      NumericOption{"--max-schema-total-name-units", &t.context.schema.totalNameUnits},
                      NumericOption{"--max-input", &t.inputBytes},
                      NumericOption{"--max-frame", &t.frame.maximumBodyBytes},
                      NumericOption{"--max-frames", &t.frames},
                      NumericOption{"--max-descriptions", &t.context.descriptions},
                      NumericOption{"--max-actions", &t.context.bindings.fields},
                      NumericOption{"--max-name-units", &t.context.bindings.nameUnits},
                      NumericOption{"--max-total-name-units", &t.context.bindings.totalNameUnits},
                      NumericOption{"--max-records", &t.data.processedRecords},
                      NumericOption{"--max-fields", &t.data.wireFields},
                      NumericOption{"--field-string-bytes", &t.data.fieldString.encodedBytes},
                      NumericOption{"--field-string-units", &t.data.fieldString.codeUnits},
                      NumericOption{"--max-field-bytes", &t.data.fieldBytes},
                      NumericOption{"--max-field-units", &t.data.fieldUnits},
                      NumericOption{"--max-owned-field-units", &t.data.ownedFieldUnits},
                      NumericOption{"--max-values", &t.data.emittedValues},
                      NumericOption{"--max-symbol-units", &t.data.ownedSymbolUnits},
                      NumericOption{"--symbol-units", &t.data.symbol.codeUnits},
                      NumericOption{"--max-construction", &t.constructionUnits},
                      NumericOption{"--max-properties", &t.descriptor.properties},
                      NumericOption{"--max-messages", &t.descriptor.messages},
                      NumericOption{"--max-descriptor-units", &t.descriptor.textUnits},
                      NumericOption{"--max-text-units", &t.descriptor.text.codeUnits},
                      NumericOption{"--max-timing-events", &t.timingEvents},
                      NumericOption{"--field-payload-encoded-bytes", &t.data.fieldPayloadEncodedBytes},
                      NumericOption{"--field-payload-bytes", &t.data.fieldPayloadBytes},
                      NumericOption{"--max-payload-encoded-bytes", &t.data.payloadEncodedBytes},
                      NumericOption{"--max-payload-work-bytes", &t.data.payloadWorkBytes},
                      NumericOption{"--max-owned-payload-bytes", &t.data.ownedPayloadBytes},
                      NumericOption{"--max-owned-field-time-records", &t.data.ownedFieldTimeRecords},
                      NumericOption{"--max-snapshot-units", &t.snapshotUnits},
                      NumericOption{"--aggregate-reservation-copies", &config.copies},
                      NumericOption{"--max-sources", &sequence.occurrences},
                      NumericOption{"--max-path-units", &sequence.pathNativeUnits},
                      NumericOption{"--max-total-path-units", &sequence.totalPathNativeUnits},
                      NumericOption{"--max-path-output-bytes", &sequence.pathEncodedBytes},
                      NumericOption{"--time-line-bytes", &config.scanner.lineBytes, true},
                      NumericOption{"--time-retained-bytes", &config.scanner.retainedBytes, true}};
}

/** Assign an explicitly selected envelope without filename guessing.
 * @tparam Char Native argument code-unit type, char or wchar_t.
 * @param text Borrowed explicit none, gzip or zip spelling.
 * @param kind Assigned on success only.
 * @return False for an unknown spelling.
 */
template <typename Char> bool parseContainer(std::basic_string_view<Char> text, ContainerKind &kind) noexcept {
    if (token(text, "none")) {
        kind = ContainerKind::NONE;
    } else if (token(text, "gzip")) {
        kind = ContainerKind::GZIP;
    } else if (token(text, "zip")) {
        kind = ContainerKind::ZIP;
    } else {
        return false;
    }

    return true;
}

/** Bounded pre-work error; path or arbitrary input text cannot reach this grammar.
 * @param diagnostic Borrowed diagnostic destination, flushed once.
 * @param code Authoritative nonzero process exit.
 * @param stage Static safe operation token.
 * @param reason Static safe cause token.
 * @param budget Maximum encoded bytes; no row is written if it cannot fit.
 * @return Original code even when diagnostic construction, write or flush fails.
 */
int fail(std::ostream &diagnostic, int code, std::string_view stage, std::string_view reason,
         std::size_t budget = TERMINAL_BYTES) noexcept {
    try {
        const SourceDiagnostic failure{code, stage, reason, {}, "sequence"};
        diagnostic << sourceDiagnostic(failure, false, std::min(budget, TERMINAL_BYTES));
        diagnostic.flush();
    } catch (...) {
    }

    return code;
}

/** Checked multiplication of a fully resolved capacity.
 * @tparam T Unsigned integral finite capacity.
 * @param value Capacity changed only when the product is representable.
 * @param copies Validated positive multiplier, at most 256.
 * @return False without mutation on overflow.
 */
template <typename T> bool multiply(T &value, std::size_t copies) noexcept {
    if (value > std::numeric_limits<T>::max() / copies) {
        return false;
    }

    value *= static_cast<T>(copies);

    return true;
}

/** Unevaluated probe for the finite scalar/nested-member inventory. */
struct LimitFieldProbe {
    /** Supply an unevaluated aggregate initializer probe.
     * @tparam T Target scalar or nested aggregate type.
     * @return No runtime value; this declaration is used only in requires expressions.
     */
    template <typename T> operator T() const;
};

/** Detect direct aggregate-field additions so the explicit multiplier cannot silently miss them.
 * @tparam T Aggregate finite-limit type.
 * @tparam I Direct-field initializer index pack.
 * @return Whether this exact aggregate initializer is well formed.
 */
template <typename T, std::size_t... I> constexpr bool acceptsFields(std::index_sequence<I...>) noexcept {
    return requires { T{(static_cast<void>(I), LimitFieldProbe{})...}; };
}

/** Require exactly the reviewed direct-field inventory for every nested finite limit type.
 * @tparam T Aggregate finite-limit type without inherited or array fields.
 * @tparam N Reviewed direct-field count.
 * @return True only when N fields fit and N+1 fields do not.
 */
template <typename T, std::size_t N> constexpr bool hasFields() noexcept {
    return std::is_aggregate_v<T> && acceptsFields<T>(std::make_index_sequence<N>{}) &&
           !acceptsFields<T>(std::make_index_sequence<N + 1>{});
}

static_assert(hasFields<Limits, 4>());
static_assert(hasFields<SequenceReservations, 9>());
static_assert(hasFields<protocol::TapeLimits, 9>());
static_assert(hasFields<protocol::FrameLimits, 2>());
static_assert(hasFields<protocol::TapeDescriptorLimits, 4>());
static_assert(hasFields<protocol::ContextLimits, 3>());
static_assert(hasFields<core::SchemaLimits, 5>());
static_assert(hasFields<protocol::BindingLimits, 3>());
static_assert(hasFields<protocol::StreamDataLimits, 17>());
static_assert(hasFields<encoding::SymbolLimits, 3>());
static_assert(hasFields<encoding::UtfStringLimits, 2>());
static_assert(hasFields<encoding::CharArrayLimits, 2>());
static_assert(hasFields<ContainerLimits, 13>());
static_assert(hasFields<SidecarLimits, 6>());

/** Multiply every declared parser capacity; output and excess probes remain separate.
 * @param config Fully resolved per-source configuration, receiving explicit aggregate allowances.
 * @return False on the first unrepresentable product, before any source names or I/O.
 */
bool reserveCopies(Configuration &config) noexcept {
    auto &aggregate = config.sequence.aggregate;
    aggregate.tape = config.parser.tape;
    aggregate.dataContainer = config.data;
    aggregate.companionContainer = config.time;
    aggregate.scanner = config.scanner;
    aggregate.chunkBytes = config.parser.chunkSize;
    aggregate.eventUnits = config.parser.eventUnits;
    aggregate.outputBytes = config.globalOutput;
    aggregate.physicalProbes = config.physicalProbes;
    aggregate.inflatedProbes = config.inflatedProbes;
#define QDCXX_MULTIPLY(field)                                                                                          \
    if (!multiply(aggregate.field, config.copies)) {                                                                   \
        return false;                                                                                                  \
    }
    QDCXX_MULTIPLY(tape.frame.maximumBodyBytes);
    QDCXX_MULTIPLY(tape.frame.maximumPendingBytes);
    QDCXX_MULTIPLY(tape.inputBytes);
    QDCXX_MULTIPLY(tape.frames);
    QDCXX_MULTIPLY(tape.descriptor.properties);
    QDCXX_MULTIPLY(tape.descriptor.messages);
    QDCXX_MULTIPLY(tape.descriptor.text.bodyBytes);
    QDCXX_MULTIPLY(tape.descriptor.text.codeUnits);
    QDCXX_MULTIPLY(tape.descriptor.textUnits);
    QDCXX_MULTIPLY(tape.context.schema.records);
    QDCXX_MULTIPLY(tape.context.schema.fields);
    QDCXX_MULTIPLY(tape.context.schema.lookups);
    QDCXX_MULTIPLY(tape.context.schema.nameUnits);
    QDCXX_MULTIPLY(tape.context.schema.totalNameUnits);
    QDCXX_MULTIPLY(tape.context.descriptions);
    QDCXX_MULTIPLY(tape.context.bindings.fields);
    QDCXX_MULTIPLY(tape.context.bindings.nameUnits);
    QDCXX_MULTIPLY(tape.context.bindings.totalNameUnits);
    QDCXX_MULTIPLY(tape.constructionUnits);
    QDCXX_MULTIPLY(tape.data.payloadBytes);
    QDCXX_MULTIPLY(tape.data.processedRecords);
    QDCXX_MULTIPLY(tape.data.wireFields);
    QDCXX_MULTIPLY(tape.data.emittedRecords);
    QDCXX_MULTIPLY(tape.data.emittedValues);
    QDCXX_MULTIPLY(tape.data.ownedSymbolUnits);
    QDCXX_MULTIPLY(tape.data.symbol.inputBytes);
    QDCXX_MULTIPLY(tape.data.symbol.codeUnits);
    QDCXX_MULTIPLY(tape.data.symbol.stateUnits);
    QDCXX_MULTIPLY(tape.data.fieldString.encodedBytes);
    QDCXX_MULTIPLY(tape.data.fieldString.codeUnits);
    QDCXX_MULTIPLY(tape.data.fieldBytes);
    QDCXX_MULTIPLY(tape.data.fieldUnits);
    QDCXX_MULTIPLY(tape.data.ownedFieldUnits);
    QDCXX_MULTIPLY(tape.data.fieldPayloadEncodedBytes);
    QDCXX_MULTIPLY(tape.data.fieldPayloadBytes);
    QDCXX_MULTIPLY(tape.data.payloadEncodedBytes);
    QDCXX_MULTIPLY(tape.data.payloadWorkBytes);
    QDCXX_MULTIPLY(tape.data.ownedPayloadBytes);
    QDCXX_MULTIPLY(tape.data.ownedFieldTimeRecords);
    QDCXX_MULTIPLY(tape.timingEvents);
    QDCXX_MULTIPLY(tape.snapshotUnits);
    QDCXX_MULTIPLY(dataContainer.compressedBytes);
    QDCXX_MULTIPLY(dataContainer.inflatedBytes);
    QDCXX_MULTIPLY(dataContainer.expansionRatio);
    QDCXX_MULTIPLY(dataContainer.ratioSlack);
    QDCXX_MULTIPLY(dataContainer.gzipMembers);
    QDCXX_MULTIPLY(dataContainer.zipEntries);
    QDCXX_MULTIPLY(dataContainer.headerBytes);
    QDCXX_MULTIPLY(dataContainer.totalHeaderBytes);
    QDCXX_MULTIPLY(dataContainer.workBytes);
    QDCXX_MULTIPLY(dataContainer.inflateCalls);
    QDCXX_MULTIPLY(dataContainer.retainedBytes);
    QDCXX_MULTIPLY(dataContainer.inflaterBytes);
    QDCXX_MULTIPLY(dataContainer.chunkBytes);
    QDCXX_MULTIPLY(companionContainer.compressedBytes);
    QDCXX_MULTIPLY(companionContainer.inflatedBytes);
    QDCXX_MULTIPLY(companionContainer.expansionRatio);
    QDCXX_MULTIPLY(companionContainer.ratioSlack);
    QDCXX_MULTIPLY(companionContainer.gzipMembers);
    QDCXX_MULTIPLY(companionContainer.zipEntries);
    QDCXX_MULTIPLY(companionContainer.headerBytes);
    QDCXX_MULTIPLY(companionContainer.totalHeaderBytes);
    QDCXX_MULTIPLY(companionContainer.workBytes);
    QDCXX_MULTIPLY(companionContainer.inflateCalls);
    QDCXX_MULTIPLY(companionContainer.retainedBytes);
    QDCXX_MULTIPLY(companionContainer.inflaterBytes);
    QDCXX_MULTIPLY(companionContainer.chunkBytes);
    QDCXX_MULTIPLY(scanner.inputBytes);
    QDCXX_MULTIPLY(scanner.lineBytes);
    QDCXX_MULTIPLY(scanner.lines);
    QDCXX_MULTIPLY(scanner.marks);
    QDCXX_MULTIPLY(scanner.workBytes);
    QDCXX_MULTIPLY(scanner.retainedBytes);
    QDCXX_MULTIPLY(chunkBytes);
    QDCXX_MULTIPLY(eventUnits);
#undef QDCXX_MULTIPLY

    return true;
}

/** Parse an envelope/scanner counter with a fixed inventory and no dynamic option table.
 * @tparam Char Native argument code-unit type, char or wchar_t.
 * @param key Borrowed option spelling.
 * @param operand Borrowed unsigned decimal capacity.
 * @param config Synchronous configuration receiving the accepted scalar.
 * @param companionOption Set when a selected option requires LONG or TEXT.
 * @return False for an unknown option, invalid decimal or overflow.
 */
template <typename Char>
bool parseExtra(std::basic_string_view<Char> key, std::basic_string_view<Char> operand, Configuration &config,
                bool &companionOption) noexcept {
    if (token(key, "--max-physical-probes")) {
        return parseCount(operand, config.physicalProbes);
    }

    if (token(key, "--max-inflated-probes")) {
        return parseCount(operand, config.inflatedProbes);
    }

#define QDCXX_SCANNER_OPTION(name, field)                                                                              \
    if (token(key, name)) {                                                                                            \
        companionOption = true;                                                                                        \
        return parseCount(operand, config.scanner.field);                                                              \
    }
    QDCXX_SCANNER_OPTION("--time-input-bytes", inputBytes)
    QDCXX_SCANNER_OPTION("--time-lines", lines)
    QDCXX_SCANNER_OPTION("--time-marks", marks)
    QDCXX_SCANNER_OPTION("--time-work-bytes", workBytes)
#undef QDCXX_SCANNER_OPTION
    const bool time = key.size() >= 14 && token(key.substr(0, 14), "--time-source-");
    const bool data = key.size() >= 9 && token(key.substr(0, 9), "--source-");

    if (!time && !data) {
        return false;
    }

    companionOption = companionOption || time;
    const auto suffix = key.substr(time ? 14 : 9);
    auto &limits = time ? config.time : config.data;
#define QDCXX_CONTAINER_OPTION(name, field)                                                                            \
    if (token(suffix, name)) {                                                                                         \
        return parseCount(operand, limits.field);                                                                      \
    }
    QDCXX_CONTAINER_OPTION("compressed-bytes", compressedBytes)
    QDCXX_CONTAINER_OPTION("inflated-bytes", inflatedBytes)
    QDCXX_CONTAINER_OPTION("expansion-ratio", expansionRatio)
    QDCXX_CONTAINER_OPTION("ratio-slack", ratioSlack)
    QDCXX_CONTAINER_OPTION("gzip-members", gzipMembers)
    QDCXX_CONTAINER_OPTION("zip-entries", zipEntries)
    QDCXX_CONTAINER_OPTION("header-bytes", headerBytes)
    QDCXX_CONTAINER_OPTION("total-header-bytes", totalHeaderBytes)
    QDCXX_CONTAINER_OPTION("work-bytes", workBytes)
    QDCXX_CONTAINER_OPTION("inflate-calls", inflateCalls)
    QDCXX_CONTAINER_OPTION("retained-bytes", retainedBytes)
    QDCXX_CONTAINER_OPTION("inflater-bytes", inflaterBytes)
    QDCXX_CONTAINER_OPTION("chunk-bytes", chunkBytes)
#undef QDCXX_CONTAINER_OPTION

    return false;
}

/** Validate every path's native units and checked storage/encoded sums before creating path objects.
 * @tparam Char Native argument code-unit type, matching the current OS path representation.
 * @param argc Validated complete process argument count.
 * @param argv Borrowed process arguments; only input/time-input operands are examined.
 * @param config Explicit path and occurrence bounds.
 * @param sourceCount Number of selected data occurrences.
 * @return Zero on success, usage2 for empty/stdin names, or resource6 for finite/representation limits.
 */
template <typename Char>
int checkPaths(int argc, const Char *const *argv, const Configuration &config, std::size_t sourceCount) noexcept {
    std::size_t total = 0;
    std::size_t encoded = 0;
    std::size_t storage = 0;
    constexpr auto MAXIMUM = std::numeric_limits<std::size_t>::max();
    constexpr auto ENCODED_NATIVE_UNIT = sizeof(Char) * 2;

    if (sourceCount > config.sequence.occurrences || sourceCount > MAXIMUM / sizeof(TapeSegmentSpec)) {
        return 6;
    }

    storage = sourceCount * sizeof(TapeSegmentSpec);

    for (int i = 1; i < argc; i += 2) {
        const std::basic_string_view<Char> key(argv[i]);

        if (!token(key, "--input") && !token(key, "--time-input")) {
            continue;
        }

        const std::basic_string_view<Char> path(argv[i + 1]);

        if (path.empty() || token(path, "-")) {
            return 2;
        }

        if (path.size() > config.sequence.pathNativeUnits || path.size() > MAXIMUM - total ||
            path.size() > MAXIMUM / ENCODED_NATIVE_UNIT || path.size() > MAXIMUM / sizeof(Char) - 1) {
            return 6;
        }

        total += path.size();
        const auto nextEncoded = path.size() * ENCODED_NATIVE_UNIT;
        const auto nextStorage = (path.size() + 1) * sizeof(Char);

        if (nextEncoded > MAXIMUM - encoded || nextStorage > MAXIMUM - storage) {
            return 6;
        }

        encoded += nextEncoded;
        storage += nextStorage;
    }

    if (total > config.sequence.totalPathNativeUnits || encoded > config.sequence.pathEncodedBytes) {
        return 6;
    }

    // This bound derives from already checked actual argument storage, not filesystem normalization.
    return storage == 0 ? 6 : 0;
}

/** Execute only after complete option validation; no names are copied by the parsing pass.
 * @tparam Char Native argument code-unit type, matching the current OS path representation.
 * @param argc Process argument count including the executable.
 * @param argv Borrowed process arguments, never retained after this call.
 * @param output Borrowed output destination through final owning flush.
 * @param diagnostic Borrowed bounded terminal error destination.
 * @return Documented process exit; zero requires complete inspection and successful close/flush.
 */
template <typename Char> int run(int argc, const Char *const *argv, std::ostream &output, std::ostream &diagnostic) {
    const auto diagnosticBudget = diagnosticOutputLimit(argc, argv);

    try {
        Configuration config;
        const auto numeric = numericOptions(config);
        const auto reject = [&](int code, std::string_view stage, std::string_view reason) {
            return fail(diagnostic, code, stage, reason, diagnosticBudget);
        };
        bool sourceProfile = false, wire = false, mode = false, timePolicy = false, companionOption = false;
        std::size_t dataCount = 0, timeCount = 0;

        for (int i = 1; i < argc; i += 2) {
            if (i + 1 == argc) {
                return reject(2, "usage", "invalid-arguments");
            }

            const std::basic_string_view<Char> key(argv[i]), operand(argv[i + 1]);
            const bool path = token(key, "--input") || token(key, "--time-input");

            if (!path) {
                for (int previous = 1; previous < i; previous += 2) {
                    if (key == std::basic_string_view<Char>(argv[previous])) {
                        return reject(2, "usage", "duplicate-option");
                    }
                }
            }

            bool valid = true;

            if (token(key, "--input")) {
                ++dataCount;
                valid = !operand.empty() && !token(operand, "-");
            } else if (token(key, "--time-input")) {
                ++timeCount;
                valid = !operand.empty() && !token(operand, "-");
                companionOption = true;
            } else if (token(key, "--source-profile")) {
                sourceProfile = token(operand, "self-contained-segments-v1");
                valid = sourceProfile;
            } else if (token(key, "--wire-schema")) {
                wire = token(operand, "binary-catalog-v1");
                valid = wire;
            } else if (token(key, "--tape-mode")) {
                mode = token(operand, "binary-catalog-v1");
                valid = mode;
            } else if (token(key, "--time-policy")) {
                timePolicy = true;

                if (token(operand, "none")) {
                    config.sequence.time = SourceTimePolicy::NONE;
                } else if (token(operand, "message")) {
                    config.sequence.time = SourceTimePolicy::MESSAGE;
                } else if (token(operand, "field")) {
                    config.sequence.time = SourceTimePolicy::FIELD;
                } else if (token(operand, "long")) {
                    config.sequence.time = SourceTimePolicy::LONG;
                } else if (token(operand, "text")) {
                    config.sequence.time = SourceTimePolicy::TEXT;
                } else {
                    valid = false;
                }
            } else if (token(key, "--container")) {
                valid = parseContainer(operand, config.dataKind);
            } else if (token(key, "--time-container")) {
                valid = parseContainer(operand, config.timeKind);
                companionOption = true;
            } else if (token(key, "--diagnostics-version")) {
                valid = token(operand, "3");
            } else if (token(key, "--output-format")) {
                valid = token(operand, "tsv") || token(operand, "human");
                config.sequence.format =
                    token(operand, "human") ? inspect::OutputFormat::HUMAN : inspect::OutputFormat::TSV;
            } else {
                bool found = false;

                for (const auto &option : numeric) {
                    if (token(key, option.name)) {
                        found = true;
                        companionOption = companionOption || option.companionOnly;
                        valid = parseCount(operand, *option.value);
                        break;
                    }
                }

                if (!found) {
                    valid = parseExtra(key, operand, config, companionOption);
                }
            }

            if (!valid) {
                return reject(2, "usage", "invalid-arguments");
            }
        }

        const bool paired =
            config.sequence.time == SourceTimePolicy::LONG || config.sequence.time == SourceTimePolicy::TEXT;

        if (!sourceProfile || !wire || !mode || !timePolicy || dataCount == 0 ||
            (paired ? timeCount != dataCount : companionOption) || config.copies == 0 ||
            config.copies > MAX_RESERVATION_COPIES) {
            return reject(2, "usage", "invalid-arguments");
        }

        const bool invalidChunk =
            config.parser.chunkSize == 0 || config.data.chunkBytes == 0 || (paired && config.time.chunkBytes == 0);

        if (invalidChunk) {
            return reject(2, "usage", "zero-chunk");
        }

        if (!containerAvailable(config.dataKind) || (paired && !containerAvailable(config.timeKind))) {
            return reject(4, "configuration", "compression-disabled");
        }

        // Positive per-source feed capacity makes a count beyond explicit copies inadmissible.
        if (dataCount > config.copies) {
            return reject(6, "admission", "configuration-refused");
        }

        const auto paths = checkPaths(argc, argv, config, dataCount);

        if (paths != 0) {
            return reject(paths, paths == 2 ? "usage" : "configuration", "path-limits");
        }

        if (!prepareLimits(config.parser) || !reserveCopies(config)) {
            return reject(6, "configuration", "unrepresentable");
        }

        config.sequence.outputBytes = config.globalOutput;
        config.sequence.terminalBytes = TERMINAL_BYTES;

        // The actual path storage sum was checked above. The owner rechecks the same exact objects.
        std::size_t pathStorage = dataCount * sizeof(TapeSegmentSpec);

        for (int i = 1; i < argc; i += 2) {
            const std::basic_string_view<Char> key(argv[i]);

            if (token(key, "--input") || token(key, "--time-input")) {
                pathStorage += (std::char_traits<Char>::length(argv[i + 1]) + 1) * sizeof(Char);
            }
        }

        config.sequence.pathStorageBytes = pathStorage;

        std::vector<TapeSegmentSpec> segments;

        segments.reserve(dataCount);

        for (int i = 1; i < argc; i += 2) {
            if (token(std::basic_string_view<Char>(argv[i]), "--input")) {
                TapeSegmentSpec segment;

                segment.path = std::filesystem::path(argv[i + 1]);
                segment.container = config.dataKind;
                segment.containerLimits = config.data;
                segment.limits = config.parser;
                segments.push_back(std::move(segment));
            }
        }

        std::size_t companion = 0;

        for (int i = 1; i < argc; i += 2) {
            if (token(std::basic_string_view<Char>(argv[i]), "--time-input")) {
                segments[companion++].companion =
                    TapeCompanionSpec{std::filesystem::path(argv[i + 1]), config.timeKind, config.time, config.scanner};
            }
        }

        const auto result = inspectSequence(segments, output, diagnostic, config.sequence);

        return result.status == SequenceStatus::COMPLETE  ? 0
               : result.status == SequenceStatus::STOPPED ? 7
               : result.primary.exitCode == 0             ? 3
                                                          : result.primary.exitCode;
    } catch (const std::bad_alloc &) {
        return fail(diagnostic, 6, "configuration", "allocation", diagnosticBudget);
    } catch (const std::length_error &) {
        return fail(diagnostic, 6, "configuration", "allocation", diagnosticBudget);
    } catch (...) {
        return fail(diagnostic, 3, "write", "io-error", diagnosticBudget);
    }
}
} // namespace

int runSourceCli(int argc, const char *const *argv, std::ostream &output, std::ostream &diagnostic) {
#ifdef _WIN32

    // Only this opt-in branch initializes wide arguments; existing narrow argv remains unchanged.
    if (_configure_wide_argv(_crt_argv_unexpanded_arguments) != 0 || __wargv == nullptr || __argc != argc) {
        return fail(diagnostic, 6, "configuration", "arguments-unavailable", diagnosticOutputLimit(argc, argv));
    }

    return run(argc, __wargv, output, diagnostic);
#else

    return run(argc, argv, output, diagnostic);
#endif
}

void sourceCliHelp(std::ostream &output) {
    Configuration config;
    output
        << "Owning opt-in: --source-profile self-contained-segments-v1 --wire-schema binary-catalog-v1\n"
           "  --tape-mode binary-catalog-v1 --time-policy none|message|field|long|text --input PATH [--input PATH "
           "...]\n"
           "  [--time-input PATH ...] [--container none|gzip|zip] [--time-container none|gzip|zip]\n"
           "Output9, diagnostics3; --diagnostics-version accepts only 3 in this profile. TSV default, HUMAN "
           "available.\n"
           "Both containers default none. No filename guessing, wildcard expansion, stdin, live input or network.\n"
           "LONG/TEXT require one explicit companion per input; all companion options otherwise fail usage2.\n"
           "Repeated input paths are separate ordered occurrences; other repeated options fail usage2.\n"
           "Finite unsigned decimals; zero remains zero except positive chunks and reservation copies 1..256.\n"
           "Copies default1 explicitly multiplies each nested reservation; two default-limit sources need copies 2.\n"
           "Global max-output is one stdout+stderr cap, never multiplied; terminal reserve 65536 bytes.\n"
           "Source defaults (bytes except schema/name/string/snapshot units and counts):\n";

    for (const auto &option : numericOptions(config)) {
        output << option.name << ' ' << *option.value << '\n';
    }

    output << "--max-physical-probes 2 (unmultiplied diagnostic excess bytes)\n"
              "--max-inflated-probes 2 (unmultiplied discarded logical excess bytes)\n"
              "--time-input-bytes 4194304\n--time-lines 262144\n--time-marks 262144\n--time-work-bytes 8388608\n";

    for (auto prefix : {"--source-", "--time-source-"}) {
#define QDCXX_HELP_CONTAINER(name, field) output << prefix << name << ' ' << config.data.field << '\n';
        QDCXX_HELP_CONTAINER("compressed-bytes", compressedBytes)
        QDCXX_HELP_CONTAINER("inflated-bytes", inflatedBytes)
        QDCXX_HELP_CONTAINER("expansion-ratio", expansionRatio)
        QDCXX_HELP_CONTAINER("ratio-slack", ratioSlack)
        QDCXX_HELP_CONTAINER("gzip-members", gzipMembers)
        QDCXX_HELP_CONTAINER("zip-entries", zipEntries)
        QDCXX_HELP_CONTAINER("header-bytes", headerBytes)
        QDCXX_HELP_CONTAINER("total-header-bytes", totalHeaderBytes)
        QDCXX_HELP_CONTAINER("work-bytes", workBytes)
        QDCXX_HELP_CONTAINER("inflate-calls", inflateCalls)
        QDCXX_HELP_CONTAINER("retained-bytes", retainedBytes)
        QDCXX_HELP_CONTAINER("inflater-bytes", inflaterBytes)
        QDCXX_HELP_CONTAINER("chunk-bytes", chunkBytes)
#undef QDCXX_HELP_CONTAINER
    }
    output
        << "Path limits count native code units; path-output counts encoded bytes. Source chunks are physical "
           "scratch;\n"
           "chunk-size is produced parser-span bytes. Ratio is dimensionless; other source values are bytes/counts.\n"
           "Derived before admission: pending=frame+9, payload/symbol-input=frame, symbol-state=symbol-units,\n"
           "emitted-records=max-records. No later limit changes. At most one pair/two physical handles.\n"
           "Opens explicit local files only; consumes/validates EOF, closes own handles, writes bounded LF rows.\n"
           "COMPLETE requires parser/scanner/container/close and both flushes plus exit0; a marker alone is "
           "provisional.\n"
           "Owning cancellation returns 7 STOPPED; no CLI cancellation flag or signal scheduler is installed.\n"
           "Compression OFF refuses gzip/zip with4 before source work. NONE requires no zlib/JSON/replay.\n";
}
} // namespace org::ttldtor::qdcxx::tape_inspect
