// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "inspect.hpp"

#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/stream_data.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <istream>
#include <limits>
#include <new>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::inspect {
namespace {
constexpr core::SchemaLimits SCHEMA_LIMITS{1024, 65536, 65536, 1024, 1048576}; ///< Independent local schema bounds.
constexpr std::size_t MAX_PREFIX_BYTES = 9;  ///< Maximum compact-long length-prefix width.
constexpr std::int32_t DESCRIBE_RECORDS = 2; ///< Only initial description messages are supported.
constexpr std::int32_t STREAM_DATA = 15;     ///< Only ordinary stream data is supported.

/** Internal first-error transfer, preserving separate consumed/error positions. */
struct Failure {
    int code;                            ///< Contract exit code.
    std::string stage;                   ///< ASCII checkpoint, possibly with an existing enum ordinal.
    std::optional<std::size_t> consumed; ///< Payload-relative committed examination position.
    std::optional<std::size_t> error;    ///< Payload-relative primitive/checkpoint start.
};

/** Current envelope context, independent of owning frame storage. */
struct Position {
    std::optional<std::uint64_t> index;    ///< Current zero-based frame if known.
    std::optional<std::int32_t> type;      ///< Absent for heartbeat or incomplete header.
    std::optional<std::uint64_t> start;    ///< Inclusive envelope start if known.
    std::optional<std::uint64_t> payload;  ///< Actual payload start if known.
    std::optional<std::uint64_t> accepted; ///< Absolute framer accepted position on framing error only.
};

/** Format an integer independent of stream or process locale.
 * @tparam T Integral type.
 * @param value Integer to represent.
 * @return Canonical decimal ASCII.
 */
template <typename T> std::string decimal(T value) {
    std::array<char, 32> buffer{};
    auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);

    return {buffer.data(), result.ptr};
}

/** Format a known position or the unavailable sentinel.
 * @tparam T Integral position type.
 * @param value Optional integer.
 * @return Decimal or '-'.
 */
template <typename T> std::string cell(std::optional<T> value) {
    return value ? decimal(*value) : "-";
}

/** Compose a checked absolute position from a parser-relative checkpoint.
 * @param base Actual payload start, absent for unknown headers.
 * @param offset Payload-relative checkpoint.
 * @return Absolute position, or absent if either input is absent or addition overflows.
 */
std::optional<std::uint64_t> absolute(std::optional<std::uint64_t> base, std::optional<std::size_t> offset) {
    if (!base || !offset || *offset > std::numeric_limits<std::uint64_t>::max() - *base) {
        return {};
    }

    return *base + *offset;
}

/** Emit available failure context; failure of stderr cannot turn failure into success.
 * @param out Binary diagnostic destination.
 * @param failure First error and optional parser checkpoints.
 * @param position Known envelope context.
 */
void diagnose(std::ostream &out, const Failure &failure, const Position &position) noexcept {
    try {
        const auto consumed = position.accepted ? position.accepted : absolute(position.payload, failure.consumed);
        const auto row = "incomplete\t" + decimal(failure.code) + "\t" + failure.stage + "\t" + cell(position.index) +
                         "\t" + cell(position.type) + "\t" + cell(position.start) + "\t" + cell(position.payload) +
                         "\t" + cell(failure.consumed) + "\t" + cell(failure.error) + "\t" + cell(consumed) + "\t" +
                         cell(absolute(position.payload, failure.error)) + "\n";
        out.write(row.data(), static_cast<std::streamsize>(row.size()));
        out.flush();
    } catch (...) {
        // An unusable diagnostic destination must not escape the nonzero-exit path.
    }
}

/** Translate planner failures without changing wire validation.
 * @param error Existing binding category.
 * @return Unsupported=4, invalid=5, resource/allocation=6.
 */
int bindingExit(protocol::BindingError error) {
    switch (error) {
    case protocol::BindingError::INVALID_DESCRIPTION:
        return 5;
    case protocol::BindingError::UNSUPPORTED_PROFILE:
        return 4;
    default:
        return 6;
    }
}
} // namespace

bool validLimits(const Limits &limits) noexcept {
    return limits.chunkSize != 0 &&
           limits.chunkSize <= static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) &&
           limits.frame <= static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max()) &&
           limits.frame <= std::numeric_limits<std::size_t>::max() - MAX_PREFIX_BYTES;
}

std::expected<core::LocalSchema, core::SchemaError> oracleSchema() {
    constexpr auto TYPE = std::to_underlying(core::SerialFieldType::COMPACT_INT); ///< Exact ordinary field identity.
    const std::vector<core::LocalRecord> records{
        {0, u"S", false, {{TYPE}}, {{u"v", 0}, {u"S.v", 0}}},
        {1,
         u"R",
         false,
         {{TYPE}, {TYPE}, {TYPE}},
         {{u"y", 0}, {u"R.y", 0}, {u"x", 1}, {u"R.x", 1}, {u"xdot", 1}, {u"z", 2}, {u"R.z", 2}}},
        {2, u"E", false, {}, {}}};

    return core::LocalSchema::create(records, SCHEMA_LIMITS);
}

void report(std::ostream &diagnostic, int code, std::string_view stage) noexcept {
    try {
        diagnose(diagnostic, {code, std::string(stage), {}, {}}, {});
    } catch (...) {
        // Preserve the caller's nonzero exit even if diagnostic construction fails.
    }
}

int inspect(std::istream &input, std::ostream &output, std::ostream &diagnostic, const core::LocalSchema &schema,
            Limits limits, OutputFormat format, DisplayProfile profile) {
    const Formatter formatter(format, profile);
    Position position;
    std::string_view operation = "allocation";
    try {
        if (!validLimits(limits)) {
            throw Failure{2, "usage", {}, {}};
        }

        auto remaining = limits;
        protocol::ContextLimits contextLimits{
            SCHEMA_LIMITS, limits.descriptions, {limits.actions, limits.nameUnits, limits.totalNameUnits}};
        auto validation = protocol::StreamDataContext::create(schema, {}, contextLimits);

        if (!validation) {
            throw Failure{
                bindingExit(validation.error()), "binding." + decimal(std::to_underlying(validation.error())), {}, {}};
        }

        std::optional<protocol::StreamDataContext> context;
        std::vector<protocol::WireRecordDescription> descriptions;
        protocol::FrameReader reader({limits.frame, limits.frame + MAX_PREFIX_BYTES});
        std::vector<char> chunk(limits.chunkSize);
        std::size_t frameCount = 0;
        std::size_t emitted = 0;
        bool described = false;

        /** Write one complete logical row after its output-budget check.
         * @param row Owned complete encoded row; throws on budget or physical output failure.
         */
        const auto emit = [&](const std::string &row) {
            if (row.size() > remaining.output) {
                throw Failure{6, "write", {}, {}};
            }

            operation = "write";
            // Segment unusually large rows safely even when streamsize is narrower than size_t.
            std::size_t offset = 0;

            while (offset < row.size()) {
                const auto count = std::min<std::size_t>(
                    row.size() - offset, static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()));
                output.write(row.data() + offset, static_cast<std::streamsize>(count));

                if (!output) {
                    throw Failure{3, "write", {}, {}};
                }

                offset += count;
            }

            remaining.output -= row.size();
        };
        /** Flush so failures are never reported as successful completion. */
        const auto flush = [&] {
            operation = "flush";
            output.flush();

            if (!output) {
                throw Failure{3, "flush", {}, {}};
            }
        };
        /** Consume a delivered envelope once, retaining only initial descriptions and one result.
         * @param frame Borrowed owning envelope with actual stream boundaries.
         */
        const auto process = [&](const protocol::Frame &frame) {
            position = {frameCount, frame.type, frame.startOffset, {}, {}};

            if (frame.kind == protocol::FrameKind::MESSAGE) {
                if (frame.payload.size() > frame.endOffset - frame.startOffset) {
                    throw Failure{6, "allocation", {}, {}};
                }

                position.payload = frame.endOffset - frame.payload.size();
            }

            if (frameCount == limits.frames) {
                throw Failure{6, "frame-limit", {}, {}};
            }

            ++frameCount;
            emit(formatter.frame(frame, frameCount - 1, remaining.output));

            if (frame.kind == protocol::FrameKind::EMPTY_HEARTBEAT) {
                return;
            }

            if (*frame.type == DESCRIBE_RECORDS) {
                if (context) {
                    throw Failure{4, "sequence", {}, {}};
                }

                auto parsed = protocol::parseDescribeRecords(frame.payload, {limits.frame,
                                                                             remaining.descriptions,
                                                                             remaining.actions,
                                                                             remaining.actions,
                                                                             {limits.frame, limits.nameUnits},
                                                                             remaining.totalNameUnits});

                if (parsed.status != protocol::DescriptionStatus::OK) {
                    const auto code = parsed.status == protocol::DescriptionStatus::RESOURCE_LIMIT ||
                                              parsed.status == protocol::DescriptionStatus::ALLOCATION_FAILURE
                                          ? 6
                                          : 5;
                    throw Failure{code, "description." + decimal(std::to_underlying(parsed.stage)), parsed.consumed,
                                  parsed.errorOffset};
                }

                for (auto &description : parsed.descriptions) {
                    if (std::any_of(descriptions.begin(), descriptions.end(), [&](const auto &prior) {
                            return prior.senderId == description.senderId;
                        })) {
                        throw Failure{5, "duplicate-id", parsed.consumed, parsed.consumed};
                    }

                    auto bound = protocol::bindRecord(description, schema,
                                                      {remaining.actions, limits.nameUnits, remaining.totalNameUnits});

                    if (!bound) {
                        throw Failure{bindingExit(bound.error()),
                                      "binding." + decimal(std::to_underlying(bound.error())), parsed.consumed,
                                      parsed.consumed};
                    }

                    --remaining.descriptions;
                    remaining.actions -= description.fields.size();
                    remaining.totalNameUnits -= description.name.size();

                    for (const auto &field : description.fields) {
                        remaining.totalNameUnits -= field.name.size();
                    }

                    descriptions.push_back(std::move(description));
                }

                described = true;

                return;
            }

            if (*frame.type != STREAM_DATA) {
                throw Failure{4, "type", {}, {}};
            }

            if (!described) {
                throw Failure{4, "sequence", {}, {}};
            }

            if (!context) {
                auto built = protocol::StreamDataContext::create(schema, descriptions, contextLimits);

                if (!built) {
                    throw Failure{bindingExit(built.error()), "binding." + decimal(std::to_underlying(built.error())),
                                  0, 0};
                }

                context.emplace(std::move(*built));
            }

            const auto records = std::min(limits.messageRecords, remaining.records);
            auto decoded = protocol::decodeStreamData(frame.payload, *context,
                                                      {limits.frame,
                                                       records,
                                                       std::min(limits.messageFields, remaining.fields),
                                                       records,
                                                       std::min(limits.messageValues, remaining.values),
                                                       std::min(limits.messageSymbolUnits, remaining.symbolUnits),
                                                       {limits.frame, limits.symbol, limits.symbol}});
            remaining.records -= decoded.processedRecords;
            remaining.fields -= decoded.wireFields;

            for (const auto &record : decoded.records) {
                remaining.values -= record.values.size();
                remaining.symbolUnits -= record.symbol.units.size();
                emit(formatter.record(record, schema.records()[static_cast<std::size_t>(record.localId)].name,
                                      *position.index, emitted, remaining.output));
                ++emitted;
            }

            if (decoded.status != protocol::StreamDataStatus::OK) {
                const auto code = decoded.status == protocol::StreamDataStatus::UNSUPPORTED_PROFILE ? 4
                                  : decoded.status == protocol::StreamDataStatus::RESOURCE_LIMIT ||
                                          decoded.status == protocol::StreamDataStatus::ALLOCATION_FAILURE
                                      ? 6
                                      : 5;
                throw Failure{code, "data." + decimal(std::to_underlying(decoded.stage)), decoded.consumed,
                              decoded.errorOffset};
            }
        };
        emit(formatter.header(remaining.output));
        bool eof = false;

        while (!eof) {
            operation = "read";

            if (remaining.input == 0) {
                const auto next = input.peek();

                if (input.bad() || (!input.eof() && input.fail())) {
                    throw Failure{3, "read", {}, {}};
                }

                if (next != std::char_traits<char>::eof()) {
                    throw Failure{6, "input-limit", {}, {}};
                }

                eof = true;
                break;
            }

            const auto requested = std::min(limits.chunkSize, remaining.input);
            input.read(chunk.data(), static_cast<std::streamsize>(requested));
            const auto count = static_cast<std::size_t>(input.gcount());

            if (input.bad() || (input.fail() && !input.eof())) {
                throw Failure{3, "read", {}, {}};
            }

            eof = input.eof();
            remaining.input -= count;
            auto bytes = std::as_bytes(std::span(chunk.data(), count));

            while (!bytes.empty()) {
                auto result = reader.feed(bytes);
                bytes = bytes.subspan(result.accepted);

                if (result.status == protocol::FrameStatus::FAILED) {
                    position = {frameCount, {}, reader.errorOffset(), {}, reader.totalAccepted()};
                    const auto code = reader.error() == protocol::FrameError::BODY_LIMIT ||
                                              reader.error() == protocol::FrameError::ALLOCATION_FAILURE ||
                                              reader.error() == protocol::FrameError::OFFSET_OVERFLOW
                                          ? 6
                                          : 5;
                    throw Failure{code, "framing." + decimal(std::to_underlying(reader.error())), {}, {}};
                }

                if (result.frame) {
                    process(*result.frame);
                } else if (!reader.pending().empty()) {
                    // A later I/O failure belongs to this incomplete envelope, not the previous delivery.
                    position = {frameCount, {}, reader.totalAccepted() - reader.pending().size(), {}, {}};
                }
            }
        }

        if (reader.finish() != protocol::FrameStatus::FINISHED) {
            position = {frameCount, {}, reader.errorOffset(), {}, reader.totalAccepted()};
            throw Failure{5, "framing." + decimal(std::to_underlying(reader.error())), {}, {}};
        }

        flush();
        emit(formatter.complete(reader.totalAccepted(), frameCount, emitted, limits.records - remaining.records,
                                limits.fields - remaining.fields, remaining.output));
        flush();

        return 0;
    } catch (const OutputLimit &) {
        diagnose(diagnostic, {6, "write", {}, {}}, position);

        return 6;
    } catch (const Failure &failure) {
        diagnose(diagnostic, failure, position);

        return failure.code;
    } catch (const std::bad_alloc &) {
        diagnose(diagnostic, {6, "allocation", {}, {}}, position);

        return 6;
    } catch (const std::length_error &) {
        diagnose(diagnostic, {6, "allocation", {}, {}}, position);

        return 6;
    } catch (const std::ios_base::failure &) {
        diagnose(diagnostic, {3, std::string(operation), {}, {}}, position);

        return 3;
    }
}
} // namespace org::ttldtor::qdcxx::inspect
