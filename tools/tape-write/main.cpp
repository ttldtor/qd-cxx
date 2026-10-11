// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "configuration.hpp"
#include "file_boundary.hpp"

#include "qd/encoding/compact.hpp"

#include <csignal>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <fcntl.h>
#    include <io.h>
#    include <windows.h>
#endif

namespace {
namespace tw = org::ttldtor::qdcxx::tape_write;
namespace protocol = org::ttldtor::qdcxx::protocol;
namespace core = org::ttldtor::qdcxx::core;
namespace encoding = org::ttldtor::qdcxx::encoding;
namespace schema = org::ttldtor::qdcxx::schema;

/** A process category plus the first content-free owning failure. */
struct Failure {
    int exit;                      ///< Documented nonzero process outcome.
    tw::FileDiagnostic diagnostic; ///< First static failure and checked cleanup result.
};

/** Map file storage errors separately from ordinary filesystem refusals.
 * @param diagnostic Static owning file error.
 * @return Native nonzero category.
 */
int fileExit(const tw::FileDiagnostic &diagnostic) {
    return diagnostic.code == "PATH_LIMIT" || diagnostic.code == "WORK_LIMIT" ||
                   diagnostic.code == "INPUT_BYTE_LIMIT" || diagnostic.code == "ALLOCATION_FAILED"
               ? 6
               : 3;
}

/** Emit a bounded LF failure without input names or contents.
 * @param failure Primary category and checked cleanup state.
 * @return Original nonzero category, including broken diagnostic output.
 */
int report(const Failure &failure) noexcept {
    try {
        const auto &d = failure.diagnostic;
        std::cerr << "tape-write-error\t1\t" << d.stage << '\t' << d.code << "\tpublished=" << (d.published ? 1 : 0)
                  << '\n';

        if (d.cleanupFailed) {
            // Hex native path bytes preserve exact bounded operator evidence without row injection.
            constexpr char HEX[] = "0123456789abcdef";
            const auto &native = d.ownedTemporary.native();
            std::cerr << "tape-write-cleanup\t1\towned-native-path-hex\t";
            const auto bytes = std::as_bytes(std::span(native.data(), native.size()));

            for (auto byte : bytes) {
                const auto value = std::to_integer<unsigned>(byte);
                std::cerr << HEX[value >> 4] << HEX[value & 15];
            }

            std::cerr << '\n';
        }

        std::cerr.flush();
    } catch (...) {
    }

    return failure.exit;
}

/** Construct a static process failure without untrusted text.
 * @param exit Process category.
 * @param stage Static operation checkpoint.
 * @param code Static error token.
 * @return Owning primary failure before output cleanup.
 */
Failure fail(int exit, std::string_view stage, std::string_view code) {
    return {exit, {stage, code, false, false, {}}};
}

/** Bound native byte/operation work even with single-byte positive transfers.
 * @param bytes Explicit maximum transferred bytes.
 * @return Derived finite policy or checked arithmetic refusal.
 */
std::expected<tw::FileLimits, Failure> physicalLimits(std::size_t bytes, bool input) {
    constexpr std::size_t OPERATION_ALLOWANCE = 68; ///< 64 exclusive name attempts plus completion operations.
    const std::size_t visits = input ? 4 : 2;       // Read growth copies versus output byte/call visits.

    if (bytes > (std::numeric_limits<std::size_t>::max() - OPERATION_ALLOWANCE) / visits) {
        return std::unexpected(fail(6, "configuration", "RESOURCE_LIMIT"));
    }

    return tw::FileLimits{bytes, bytes * visits + OPERATION_ALLOWANCE, 4096};
}

/** Translate native reader failure categories.
 * @param diagnostic First reader error.
 * @return Content-free process failure.
 */
Failure readerFailure(const protocol::TapeDiagnostic &diagnostic) {
    const auto error = diagnostic.error;
    const int exit = error == protocol::TapeError::UNSUPPORTED ? 4
                     : error == protocol::TapeError::RESOURCE_LIMIT || error == protocol::TapeError::ALLOCATION_FAILURE
                         ? 6
                         : 5;

    return fail(exit, diagnostic.stage,
                error == protocol::TapeError::UNSUPPORTED          ? "UNSUPPORTED"
                : error == protocol::TapeError::ALLOCATION_FAILURE ? "ALLOCATION_FAILURE"
                : exit == 6                                        ? "RESOURCE_LIMIT"
                                                                   : "MALFORMED");
}

/** Translate native writer failure categories.
 * @param diagnostic First writer error.
 * @return Content-free process failure.
 */
Failure writerFailure(const protocol::WriterDiagnostic &diagnostic) {
    const auto error = diagnostic.error;
    const int exit =
        error == protocol::WriterError::RESOURCE_LIMIT || error == protocol::WriterError::ALLOCATION_FAILURE ? 6 : 4;

    std::string_view code = "UNSUPPORTED";

    switch (error) {
    case protocol::WriterError::INVALID_LAYOUT:
        code = "INVALID_LAYOUT";
        break;
    case protocol::WriterError::INVALID_VALUE:
        code = "INVALID_VALUE";
        break;
    case protocol::WriterError::INVALID_STATE:
        code = "INVALID_STATE";
        break;
    case protocol::WriterError::RESOURCE_LIMIT:
        code = "RESOURCE_LIMIT";
        break;
    case protocol::WriterError::ALLOCATION_FAILURE:
        code = "ALLOCATION_FAILURE";
        break;
    case protocol::WriterError::NONE:
    case protocol::WriterError::UNSUPPORTED:
        break;
    }

    return fail(exit, diagnostic.stage, code);
}

/** Preserve stable strict refusal names.
 * @param diagnostic First complete-input admission error.
 * @return Bounded process category and static code.
 */
Failure admissionFailure(const protocol::AdmissionDiagnostic &diagnostic) {
    using protocol::AdmissionCode;
    std::string_view code = "NATIVE";

    switch (diagnostic.code) {
    case AdmissionCode::UNKNOWN_RECORD:
        code = "UNKNOWN_RECORD";
        break;
    case AdmissionCode::SKIPPED_FIELD:
        code = "SKIPPED_FIELD";
        break;
    case AdmissionCode::REPEATED_TARGET:
        code = "REPEATED_TARGET";
        break;
    case AdmissionCode::MISSING_TARGET:
        code = "MISSING_TARGET";
        break;
    case AdmissionCode::DUPLICATE_LAYOUT_IDENTITY:
        code = "DUPLICATE_LAYOUT_IDENTITY";
        break;
    case AdmissionCode::UNREPRESENTED_DESCRIPTOR:
        code = "UNREPRESENTED_DESCRIPTOR";
        break;
    case AdmissionCode::NATIVE:

        if (diagnostic.writer) {
            return writerFailure(*diagnostic.writer);
        }

        if (diagnostic.reader) {
            return readerFailure(*diagnostic.reader);
        }

        code = diagnostic.error == protocol::AdmissionError::RESOURCE_LIMIT       ? "RESOURCE_LIMIT"
               : diagnostic.error == protocol::AdmissionError::ALLOCATION_FAILURE ? "ALLOCATION_FAILURE"
                                                                                  : "NATIVE";
        break;
    }

    const int exit = diagnostic.error == protocol::AdmissionError::UNSUPPORTED ? 4
                     : diagnostic.error == protocol::AdmissionError::MALFORMED ? 5
                                                                               : 6;

    return fail(exit, diagnostic.stage, code);
}

/** Independent extra second-pass visits and temporary logical container storage. */
class PassBudget {
    std::size_t remaining_;
    std::size_t bytes_;

    public:
    /** Retain explicit finite allowances.
     * @param limits Independently reused admission work/scratch policy.
     */
    explicit PassBudget(const protocol::AdmissionLimits &limits)
        : remaining_(limits.workUnits), bytes_(limits.scratchBytes) {
    }

    /** Charge visits before examining elements.
     * @param count Additional visits.
     * @return True when the charge fits.
     */
    bool charge(std::size_t count) {
        if (count > remaining_) {
            return false;
        }

        remaining_ -= count;

        return true;
    }

    /** Guard proportional temporary container bytes before allocation.
     * @param count Element count.
     * @param size Logical element byte size.
     * @return True when multiplication and allowance fit.
     */
    bool storage(std::size_t count, std::size_t size) const {
        return size == 0 || count <= bytes_ / size;
    }
};

/** Scan validated description IDs without allocating any duplicate names.
 * @param payload Complete reader-validated type-2 payload.
 * @param limits Finite retained layout bounds.
 * @param budget Independent visits and temporary byte guard.
 * @return Ordered sender IDs or finite-work refusal.
 */
std::expected<std::vector<std::int32_t>, Failure> descriptionIds(std::span<const std::byte> payload,
                                                                 protocol::LayoutLimits limits, PassBudget &budget) {
    std::vector<std::int32_t> ids;
    const auto take = [&](std::span<const std::byte> &input, bool longValue) -> std::optional<std::int64_t> {
        const auto decodedLong = longValue ? encoding::readCompactLong(input) : encoding::ReadResult<std::int64_t>{};
        const auto decodedInt = longValue ? encoding::ReadResult<std::int32_t>{} : encoding::readCompactInt(input);
        const encoding::ReadResult<std::int64_t> value =
            longValue ? decodedLong
                      : encoding::ReadResult<std::int64_t>{decodedInt.value, decodedInt.consumed, decodedInt.status};

        if (value.status != encoding::Status::OK || !budget.charge(value.consumed)) {
            return {};
        }

        input = input.subspan(value.consumed);

        return value.value;
    };
    const auto skipText = [&](std::span<const std::byte> &input) {
        const auto length = take(input, true);

        if (!length || *length < 0 || static_cast<std::uint64_t>(*length) > input.size() ||
            !budget.charge(static_cast<std::size_t>(*length))) {
            return false;
        }

        input = input.subspan(static_cast<std::size_t>(*length));

        return true;
    };

    while (!payload.empty()) {
        const auto id = take(payload, false);

        if (!id || *id < 0 || *id >= std::numeric_limits<std::int32_t>::max() || !skipText(payload)) {
            return std::unexpected(fail(6, "second-description", "RESOURCE_LIMIT"));
        }

        const auto fields = take(payload, false);

        if (!fields || *fields < 0 || static_cast<std::uint64_t>(*fields) > limits.fields) {
            return std::unexpected(fail(6, "second-description", "RESOURCE_LIMIT"));
        }

        for (std::int64_t i = 0; i < *fields; ++i) {
            if (!skipText(payload) || !take(payload, false)) {
                return std::unexpected(fail(6, "second-description", "RESOURCE_LIMIT"));
            }
        }

        if (ids.size() >= limits.records || !budget.storage(ids.size() + 1, sizeof(std::int32_t)) ||
            !budget.charge(1)) {
            return std::unexpected(fail(6, "second-description", "RESOURCE_LIMIT"));
        }

        ids.push_back(static_cast<std::int32_t>(*id));
    }

    return ids;
}

/** Compose one fresh independently charged pass, stopping before writing failed-prefix records.
 * @param admitted Immutable complete preflight token.
 * @param limits Finite second-pass extra bounds.
 * @param writerLimits Independent composer budgets.
 * @param output Sole temporary owner receiving complete chunks.
 * @return Success only after both reader and writer finish; publication follows separately.
 */
std::expected<void, Failure> compose(const protocol::AdmittedTape &admitted, protocol::AdmissionLimits limits,
                                     protocol::WriterLimits writerLimits, tw::OutputFile &output) {
    auto writer = protocol::TapeWriter::create(admitted.layouts(), admitted.descriptor(), writerLimits);

    if (!writer) {
        return std::unexpected(writerFailure(writer.error()));
    }

    protocol::TapeReader reader(admitted.schema(), admitted.readerLimits(), protocol::TapeMode::RAW_MESSAGE_V1);
    PassBudget budget(limits);
    std::size_t offset = 0;

    while (offset < admitted.bytes().size()) {
        auto event = reader.feed(admitted.bytes().subspan(offset));
        offset += event.accepted;

        if (event.status == protocol::TapeStatus::FAILED) {
            return std::unexpected(readerFailure(reader.diagnostic()));
        }

        if (!event.frame) {
            return std::unexpected(fail(5, "second-pass", "INCOMPLETE_FRAME"));
        }

        std::expected<protocol::WriterChunk, protocol::WriterDiagnostic> chunk =
            std::unexpected(protocol::WriterDiagnostic{});
        const auto &frame = *event.frame;

        if (frame.kind == protocol::FrameKind::EMPTY_HEARTBEAT) {
            chunk = writer->marker();
        } else if (frame.type == 1) {
            chunk = writer->begin();
        } else if (frame.type == 2) {
            auto ids = descriptionIds(frame.payload, limits.layouts, budget);

            if (!ids) {
                return std::unexpected(ids.error());
            }

            chunk = writer->describe(*ids);
        } else if (frame.type == 0) {
            if (!event.heartbeat) {
                return std::unexpected(fail(5, "second-pass", "MISSING_HEARTBEAT"));
            }

            chunk = writer->heartbeat(*event.heartbeat);
        } else {
            if (!budget.storage(event.records.size(), sizeof(protocol::OutgoingRecordView))) {
                return std::unexpected(fail(6, "second-records", "RESOURCE_LIMIT"));
            }

            std::size_t legacyValues = 0;

            for (const auto &record : event.records) {
                if (record.values.size() > limits.reader.data.emittedValues - legacyValues) {
                    return std::unexpected(fail(6, "second-values", "RESOURCE_LIMIT"));
                }

                legacyValues += record.values.size();
            }

            // Guard both simultaneously owned temporary containers before either allocation.
            const auto viewBytes = event.records.size() * sizeof(protocol::OutgoingRecordView);

            if (viewBytes > limits.scratchBytes ||
                legacyValues > (limits.scratchBytes - viewBytes) / sizeof(core::FieldValue) ||
                !budget.charge(event.records.size()) || !budget.charge(legacyValues)) {
                return std::unexpected(fail(6, "second-values", "RESOURCE_LIMIT"));
            }

            std::vector<core::FieldValue> legacy;
            legacy.reserve(legacyValues);
            std::vector<protocol::OutgoingRecordView> views;
            views.reserve(event.records.size());

            for (const auto &record : event.records) {
                const protocol::OutgoingRecord *layout = nullptr;

                for (const auto &candidate : admitted.layouts().records()) {
                    if (!budget.charge(1)) {
                        return std::unexpected(fail(6, "second-identity", "RESOURCE_LIMIT"));
                    }

                    if (candidate.localRecordId == record.localId) {
                        layout = &candidate;
                        break;
                    }
                }

                if (layout == nullptr) {
                    return std::unexpected(fail(5, "second-identity", "UNKNOWN_RECORD"));
                }

                auto values = std::span<const core::FieldValue>(record.typedValues);

                if (admitted.schema().profile() == core::LocalProfile::COMPACT_INT_V1) {
                    const auto start = legacy.size();

                    for (auto value : record.values) {
                        legacy.push_back({core::SerialFieldType::COMPACT_INT, value, {}});
                    }

                    values = std::span<const core::FieldValue>(legacy).subspan(start, record.values.size());
                }

                views.push_back({layout->senderId, record.symbol.units, values, record.symbol.eventFlags});
            }

            chunk = writer->data(views);
        }

        if (!chunk) {
            return std::unexpected(writerFailure(chunk.error()));
        }

        const auto written = output.write(chunk->bytes);

        if (!written) {
            return std::unexpected(Failure{fileExit(written.error()), written.error()});
        }
    }

    if (reader.finish() != protocol::TapeStatus::FINISHED) {
        return std::unexpected(readerFailure(reader.diagnostic()));
    }

    const auto finished = writer->finish();

    if (!finished) {
        return std::unexpected(writerFailure(finished.error()));
    }

    return {};
}

/** Execute the explicit schema, complete preflight and checked publication order.
 * @param configuration Owned validated command-line policy.
 * @return Silent success or first bounded failure.
 */
int run(const tw::Configuration &configuration) {
#ifndef ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON

    if (!configuration.schemaPath.empty()) {
        return report(fail(4, "schema", "LOADER_DISABLED"));
    }

#endif
    auto limits = tw::admissionLimits(configuration);

    if (!limits) {
        return report(fail(6, "configuration", "RESOURCE_LIMIT"));
    }

    std::expected<core::LocalSchema, Failure> local = std::unexpected(fail(5, "schema", "INVALID_SCHEMA"));

    if (configuration.schemaPath.empty()) {
        auto built = tw::builtinSchema(configuration);

        if (!built) {
            return report(fail(built.error() == core::SchemaError::RESOURCE_LIMIT ||
                                       built.error() == core::SchemaError::ALLOCATION_FAILURE
                                   ? 6
                                   : 5,
                               "schema", "INVALID_SCHEMA"));
        }

        local.emplace(std::move(*built));
    } else {
#ifdef ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON
        auto schemaFileLimits = physicalLimits(configuration.schemaLimits.documentBytes, true);

        if (!schemaFileLimits) {
            return report(schemaFileLimits.error());
        }

        auto document = tw::readInput({configuration.schemaPath, {}}, *schemaFileLimits);

        if (!document) {
            return report({fileExit(document.error()), document.error()});
        }

        const auto text = std::string_view(reinterpret_cast<const char *>(document->data()), document->size());
        auto loaded = schema::loadJsonSchema(text, configuration.schemaLimits);

        if (!loaded) {
            const auto error = loaded.error();
            const int exit =
                error == schema::JsonSchemaError::RESOURCE_LIMIT || error == schema::JsonSchemaError::ALLOCATION_FAILURE
                    ? 6
                : error == schema::JsonSchemaError::INVALID_SCHEMA || error == schema::JsonSchemaError::MALFORMED_JSON
                    ? 5
                    : 4;

            return report(fail(exit, "schema", schema::jsonSchemaErrorName(error)));
        }

        local.emplace(std::move(*loaded));
#endif
    }

    const auto fileLimits = physicalLimits(limits->inputBytes, true);
    const auto outputLimits = physicalLimits(tw::writerLimits(configuration).outputBytes, false);

    if (!fileLimits || !outputLimits) {
        return report(!fileLimits ? fileLimits.error() : outputLimits.error());
    }

    const auto paths = tw::resolvePaths(configuration.input, configuration.output, *fileLimits);

    if (!paths) {
        return report({fileExit(paths.error()), paths.error()});
    }

    auto admitted = [&]() -> std::expected<protocol::AdmittedTape, Failure> {
        auto input = tw::readInput(*paths, *fileLimits);

        if (!input) {
            return std::unexpected(Failure{fileExit(input.error()), input.error()});
        }

        auto result = protocol::admitTape(*local, *input, *limits);

        if (!result) {
            return std::unexpected(admissionFailure(result.error()));
        }

        return std::move(*result);
    }();

    if (!admitted) {
        return report(admitted.error());
    }

    if (!admitted->valid()) {
        return report(fail(5, "admission", "INVALID_TOKEN"));
    }

    auto output = tw::OutputFile::create(*paths, *outputLimits);

    if (!output) {
        return report({fileExit(output.error()), output.error()});
    }

    try {
        auto result = compose(*admitted, *limits, tw::writerLimits(configuration), *output);

        if (!result) {
            auto failure = std::move(result.error());
            failure.diagnostic = output->discard(std::move(failure.diagnostic));

            return report(failure);
        }
    } catch (const std::bad_alloc &) {
        return report({6, output->discard(fail(6, "second-pass", "ALLOCATION_FAILURE").diagnostic)});
    } catch (const std::length_error &) {
        return report({6, output->discard(fail(6, "second-pass", "RESOURCE_LIMIT").diagnostic)});
    } catch (const std::invalid_argument &) {
        return report({6, output->discard(fail(6, "second-pass", "RESOURCE_LIMIT").diagnostic)});
    }

    auto published = output->finishPublish();

    if (!published) {
        return report({fileExit(published.error()), published.error()});
    }

    return 0;
}
} // namespace

/** Normalize one complete admitted tape and publish create-only output.
 * @param argc Number of native arguments.
 * @param argv Separate named options and literal native paths.
 * @param nativeArguments Windows-only original UTF-16 vector, preserving literal path operands.
 * @return Documented process category, including checked help/diagnostic output.
 */
int runArguments(int argc, char **argv
#ifdef _WIN32
                 ,
                 wchar_t **nativeArguments
#endif
) {
#ifdef _WIN32

    if (_setmode(_fileno(stdout), _O_BINARY) == -1 || _setmode(_fileno(stderr), _O_BINARY) == -1) {
        return 3;
    }

#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        tw::Configuration configuration;
        const auto status = tw::parseArguments(argc, argv, configuration);

        if (status == tw::ArgumentStatus::HELP) {
            tw::writeHelp(std::cout);
            std::cout.flush();

            return std::cout ? 0 : 3;
        }

        if (status != tw::ArgumentStatus::RUN) {
            return report(fail(status == tw::ArgumentStatus::INVALID ? 2 : 4, "arguments",
                               status == tw::ArgumentStatus::INVALID ? "USAGE" : "UNSUPPORTED"));
        }

#ifdef _WIN32

        // Path operands retain exact native units, independent of option-token UTF-8 conversion.
        for (int i = 1; i < argc; i += 2) {
            const std::string_view key(argv[i]);

            if (key == "--input") {
                configuration.input = nativeArguments[i + 1];
            } else if (key == "--output") {
                configuration.output = nativeArguments[i + 1];
            } else if (key == "--schema") {
                configuration.schemaPath = nativeArguments[i + 1];
            }
        }

#endif

        return run(configuration);
    } catch (const std::bad_alloc &) {
        return report(fail(6, "process", "ALLOCATION_FAILURE"));
    } catch (const std::length_error &) {
        return report(fail(6, "process", "RESOURCE_LIMIT"));
    } catch (...) {
        return report(fail(3, "process", "IO_ERROR"));
    }
}

#ifdef _WIN32
/** Preserve Windows native Unicode process paths without the active ANSI code page.
 * @param argc Native argument count.
 * @param argv Native UTF-16 arguments owned by the process.
 * @return Documented process category after checked conversion.
 */
int wmain(int argc, wchar_t **argv) {
    try {
        std::vector<std::string> arguments;
        std::vector<char *> pointers;
        arguments.reserve(static_cast<std::size_t>(argc));
        pointers.reserve(static_cast<std::size_t>(argc));
        bool nextPathOperand = false;

        for (int i = 0; i < argc; ++i) {
            const bool pathOperand = nextPathOperand;
            nextPathOperand = false;

            if (i == 0 || pathOperand) {
                // The parser needs only nonemptiness; native operands replace placeholders after resolution.
                arguments.emplace_back(argv[i][0] == L'\0' ? "" : ".");

                continue;
            }

            const auto size =
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);

            if (size == 0) {
                return report(fail(2, "arguments", "USAGE"));
            }

            std::string argument(static_cast<std::size_t>(size), '\0');

            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, argument.data(), size, nullptr,
                                    nullptr) == 0) {
                return report(fail(2, "arguments", "USAGE"));
            }

            argument.pop_back();
            arguments.push_back(std::move(argument));

            if (i % 2 == 1) {
                const std::wstring_view key(argv[i]);
                nextPathOperand = key == L"--input" || key == L"--output" || key == L"--schema";
            }
        }

        for (auto &argument : arguments) {
            pointers.push_back(argument.data());
        }

        return runArguments(argc, pointers.data(), argv);
    } catch (const std::bad_alloc &) {
        return report(fail(6, "arguments", "ALLOCATION_FAILURE"));
    } catch (...) {
        return report(fail(2, "arguments", "USAGE"));
    }
}
#else
/** Enter the native byte-oriented POSIX argument boundary.
 * @param argc Native argument count.
 * @param argv Native arguments owned by the process.
 * @return Documented process category.
 */
int main(int argc, char **argv) {
    return runArguments(argc, argv);
}
#endif
