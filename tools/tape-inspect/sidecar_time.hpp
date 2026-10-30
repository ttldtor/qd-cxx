// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Explicit companion label; both labels use the finite shared producer grammar. */
enum class SidecarFormat {
    LONG, ///< Decimal-millisecond producer label; uses the shared finite parser.
    TEXT  ///< Calendar producer label; uses the same finite parser.
};
/** Scanner state only; END does not authenticate container or owner completion. */
enum class SidecarStatus {
    NEED_INPUT, ///< No complete mark; supply more logical bytes or declare EOF.
    MARK,       ///< Exactly one independent owning mark is present.
    END,        ///< Caller-declared logical EOF was syntactically finalized.
    FAILED      ///< First terminal error; no more input work is permitted.
};
/** Sticky scanner failures, distinct from source I/O, integrity and physical close failures. */
enum class SidecarError {
    NONE,                ///< No scanner error.
    INVALID_ARGUMENT,    ///< Invalid configuration or input after declared EOF.
    INPUT_LIMIT,         ///< Logical companion byte allowance exhausted.
    LINE_LIMIT,          ///< One line body exceeds its finite capacity.
    LINE_COUNT_LIMIT,    ///< Attempted line count exceeds its allowance.
    MARK_LIMIT,          ///< Owning mark count exceeds its allowance.
    WORK_LIMIT,          ///< Cumulative logical examination/parse work is exhausted.
    RETAINED_LIMIT,      ///< Configured scratch cannot fit retained storage allowance.
    RESOURCE_LIMIT,      ///< A required counter or operation is unrepresentable.
    ALLOCATION,          ///< Admitted scratch allocation failed.
    MALFORMED_TIME,      ///< Timestamp does not match the finite absolute grammar.
    MALFORMED_POSITION,  ///< Position is malformed or outside signed64.
    UNSUPPORTED_TIME,    ///< Relative/negative numeric time would require an ambient clock.
    POSITION_AFTER_DATA, ///< A nonnegative mark exceeds final logical data length.
    NOT_FINISHED         ///< Data closure was requested before syntax END.
};

/** Finite per-scanner limits; every zero is literal, never an unlimited/default sentinel. */
struct SidecarLimits {
    std::uint64_t inputBytes = 4194304; ///< Consumed logical companion bytes, including terminators.
    std::size_t lineBytes = 128;        ///< Maximum unterminated line body, including ignored lines.
    std::uint64_t lines = 262144;       ///< Attempted lines, including empty/ignored/malformed lines.
    std::uint64_t marks = 262144;       ///< Returned owning marks, with no refund after use.
    std::uint64_t workBytes = 8388608;  ///< Input examination plus admitted parse-character work.
    std::size_t retainedBytes = 128;    ///< Configured line scratch capacity; fixed object state is excluded.
};

/** One owning mark in source order; signed positions must not be narrowed before comparison. */
struct SidecarMark {
    std::int64_t timestampMs = 0; ///< Exact absolute milliseconds, before Java cursor saturation.
    std::int64_t position = 0;    ///< Logical inflated data position; negative marks apply immediately.
    std::uint64_t ordinal = 0;    ///< Zero-based returned-mark ordinal, independent of data records.
    /** Determine whether the mark is reached without signed/unsigned subtraction.
     * @param logicalPosition Accepted inflated data bytes in the current segment.
     * @return True for a negative position or a nonnegative position at/before the supplied position.
     */
    [[nodiscard]] bool reached(std::uint64_t logicalPosition) const noexcept;
    /** Project exact absolute milliseconds to Java's saturated packed cursor time.
     * @return Saturated signed32 seconds and floor-mod1000 milliseconds, with other sequence bits zero.
     */
    [[nodiscard]] std::int64_t packed() const noexcept;
};

/** Owning scalar counters; physical compressed coordinates belong to the separate container. */
struct SidecarCounters {
    std::uint64_t inputBytes = 0;      ///< Bytes consumed from submitted spans, including CR/LF.
    std::uint64_t workBytes = 0;       ///< Irreversible input/parse work admitted before examination.
    std::uint64_t lines = 0;           ///< Charged attempted complete or final unterminated lines.
    std::uint64_t marks = 0;           ///< Owning marks returned so far.
    std::uint64_t maximumPosition = 0; ///< Maximum nonnegative mark position; negative marks do not increase it.
};

/** First fixed owning diagnostic; stage refers only to static string literals. */
struct SidecarDiagnostic {
    SidecarError error = SidecarError::NONE; ///< Sticky first error.
    std::uint64_t line = 0;                  ///< Completed/attempted line checkpoint; zero before the first line.
    std::size_t column = 0;                  ///< Zero-based current character/checkpoint within that line.
    std::uint64_t offset = 0;        ///< Logical companion consumed-byte checkpoint, separate from data position.
    std::string_view stage = "none"; ///< Static checkpoint name, never borrowed input.
};

/** One synchronous scanner result; caller resubmits every unaccepted byte before reading again. */
struct SidecarFeed {
    std::size_t accepted = 0;                         ///< Consumed prefix of this call only.
    SidecarStatus status = SidecarStatus::NEED_INPUT; ///< MARK carries exactly one owning value.
    std::optional<SidecarMark> mark;                  ///< Independent value surviving scanner/input destruction.
};

/** Bounded synchronous scanner with no source pointers, callbacks, clock, locale or I/O.
 * Owns lazy line scratch only. The caller owns one pending mark and source/container lifetimes.
 * END and finish prove syntax/position only; paired owner close/flush/integrity remain separate.
 */
class SidecarTime {
    SidecarLimits limits_;             ///< Fixed per-instance bounds, copied without allocation.
    SidecarCounters counters_;         ///< Irreversible work and publication charges.
    SidecarDiagnostic diagnostic_;     ///< First fixed owning failure.
    std::vector<char> line_;           ///< Lazy configured-size scratch; never stores caller views.
    std::size_t lineSize_ = 0;         ///< Active body bytes within admitted scratch.
    std::optional<std::size_t> colon_; ///< Last colon position, tracked during initial examination.
    bool afterCr_ = false;             ///< Skip one following LF without inventing an extra line.
    bool closing_ = false;             ///< Logical EOF declared; further feed calls are forbidden.
    bool ended_ = false;               ///< Syntax END returned after any final owning mark.
    bool finished_ = false;            ///< First successful data EOF length has been sealed.
    /** Preserve the first failure without allocation.
     * @param error Typed cause.
     * @param stage Static checkpoint name.
     * @param column Current zero-based line checkpoint.
     */
    void fail(SidecarError error, std::string_view stage, std::size_t column = 0) noexcept;
    /** Admit cumulative parse work without overflow.
     * @param count Requested work units.
     * @return True when debited; false with a sticky error otherwise.
     */
    bool chargeWork(std::uint64_t count) noexcept;
    /** Complete one line without retaining any returned mark.
     * @return MARK, NEED_INPUT for ignored lines, or sticky FAILED.
     */
    SidecarFeed completeLine() noexcept;

    public:
    /** Construct without allocation or I/O, validating all configuration first.
     * @param format Explicit finite label; invalid enum stores INVALID_ARGUMENT immediately.
     * @param limits Finite instance bounds; zero capacities remain valid.
     */
    SidecarTime(SidecarFormat format, SidecarLimits limits = {}) noexcept;
    /** Consume at most through one owning mark; retains no caller span.
     * @param input Borrowed logical companion bytes for this call only; empty input performs no work.
     * @return Consumed prefix and one mark, NEED_INPUT, or sticky FAILED.
     */
    [[nodiscard]] SidecarFeed feed(std::span<const std::byte> input) noexcept;
    /** Finalize a caller-confirmed logical EOF without reading a source.
     * @return A final unterminated mark once, END on later calls, or sticky FAILED.
     */
    [[nodiscard]] SidecarFeed endInput() noexcept;
    /** Seal the first data EOF after END, validating every nonnegative mark position.
     * @param dataLogicalBytes Actual inflated data length for this segment; later arguments are ignored.
     * @return NONE after successful sealing; sticky NOT_FINISHED before END, or another first failure.
     */
    [[nodiscard]] SidecarError finish(std::uint64_t dataLogicalBytes) noexcept;
    /** Inspect owned counters without input work.
     * @return Independent scalar snapshot.
     */
    [[nodiscard]] SidecarCounters counters() const noexcept;
    /** Inspect the first error without input work.
     * @return Fixed owning diagnostic with only a static stage view.
     */
    [[nodiscard]] SidecarDiagnostic diagnostic() const noexcept;
};
} // namespace org::ttldtor::qdcxx::tape_inspect
