// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/protocol/tape_admission.hpp"

#include "qd/encoding/utf_char.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {
constexpr std::int32_t DESCRIBE_RECORDS = 2;           ///< Complete raw description message.
constexpr std::uint32_t SUPPLEMENTARY_POINT = 0x10000; ///< UTFString points at this boundary own two units.
constexpr char16_t HIGH_SURROGATE_FIRST = 0xd800;      ///< Start of paired high UTF-16 surrogates.
constexpr char16_t LOW_SURROGATE_FIRST = 0xdc00;       ///< Start of paired low UTF-16 surrogates.
constexpr char16_t SURROGATE_LAST = 0xdfff;            ///< Inclusive final low surrogate.

/** Internal nonallocating first-failure exit. */
struct Failure {};

/** Checked logical storage shape; excludes vector capacity and allocator metadata. */
struct Storage {
    std::size_t bytes = 0; ///< Simultaneously owned logical element bytes, including UTF-16 text.
    std::size_t units = 0; ///< Simultaneously owned UTF-16 units.
};

/** Scoped complete-input validator; no references escape its result. */
class AdmissionBuilder {
    const core::LocalSchema &schema_;
    AdmissionLimits limits_;
    AdmissionDiagnostic diagnostic_;
    std::vector<std::byte> bytes_;
    std::vector<OutgoingRecord> records_;
    OutgoingDescriptor descriptor_;
    Storage inventory_;
    Storage descriptorStorage_;

    /** Raise a bounded first failure.
     * @param error Terminal category.
     * @param stage Static checkpoint.
     * @param code Stable strict reason or native cause.
     */
    [[noreturn]] void reject(AdmissionError error, std::string_view stage, AdmissionCode code = AdmissionCode::NATIVE) {
        diagnostic_.error = error;
        diagnostic_.stage = stage;
        diagnostic_.code = code;
        throw Failure{};
    }

    /** Add without overflowing logical size arithmetic.
     * @param left Existing amount.
     * @param right Additional amount.
     * @return Representable sum or a resource failure.
     */
    std::size_t add(std::size_t left, std::size_t right) {
        if (right > std::numeric_limits<std::size_t>::max() - left) {
            reject(AdmissionError::RESOURCE_LIMIT, "size-arithmetic");
        }

        return left + right;
    }

    /** Multiply a logical element count before allocation.
     * @param count Logical count.
     * @param width Element bytes.
     * @return Representable product or a resource failure.
     */
    std::size_t multiply(std::size_t count, std::size_t width) {
        if (width != 0 && count > std::numeric_limits<std::size_t>::max() / width) {
            reject(AdmissionError::RESOURCE_LIMIT, "size-arithmetic");
        }

        return count * width;
    }

    /** Combine simultaneous owning storage.
     * @param left Existing storage.
     * @param right Additional storage.
     * @return Checked combined storage.
     */
    Storage combine(Storage left, Storage right) {
        return {add(left.bytes, right.bytes), add(left.units, right.units)};
    }

    /** Account exact units and their logical bytes.
     * @param units UTF-16 logical units.
     * @return Text storage excluding the string object itself.
     */
    Storage textStorage(std::size_t units) {
        return {multiply(units, sizeof(char16_t)), units};
    }

    /** Check simultaneous extra storage before the corresponding allocation/copy.
     * @param temporary Extra temporary beyond retained source, descriptor and inventory.
     */
    void scratch(Storage temporary = {}) {
        auto total = combine({bytes_.size(), 0}, combine(descriptorStorage_, combine(inventory_, temporary)));

        if (total.bytes > limits_.scratchBytes) {
            reject(AdmissionError::RESOURCE_LIMIT, "scratch-bytes");
        }

        if (total.units > limits_.scratchUnits) {
            reject(AdmissionError::RESOURCE_LIMIT, "scratch-units");
        }

        diagnostic_.counters.peakScratchBytes = std::max(diagnostic_.counters.peakScratchBytes, total.bytes);
        diagnostic_.counters.peakScratchUnits = std::max(diagnostic_.counters.peakScratchUnits, total.units);
    }

    /** Reserve nonrefunding work before visiting/copying elements.
     * @param units Logical visits or source bytes.
     */
    void work(std::size_t units) {
        if (units > limits_.workUnits - diagnostic_.counters.workUnits) {
            reject(AdmissionError::RESOURCE_LIMIT, "admission-work");
        }

        diagnostic_.counters.workUnits += units;
    }

    /** Count source schema copy elements and text, before an admission-owned schema duplication.
     * @return Exact logical schema declarations excluding fixed identity/control-block metadata.
     */
    Storage schemaStorage() {
        Storage result;

        for (const auto &record : schema_.records()) {
            work(1);
            result = combine(result, {sizeof(core::LocalRecord), 0});
            result = combine(result, textStorage(record.name.size()));
            result.bytes = add(result.bytes, multiply(record.fields.size(), sizeof(core::LocalField)));
            result.bytes = add(result.bytes, multiply(record.lookups.size(), sizeof(core::FieldLookup)));

            for (const auto &key : record.lookups) {
                work(1);
                result = combine(result, textStorage(key.name.size()));
            }
        }

        return result;
    }

    /** Read compact grammar during the allocation-free description census.
     * @param payload Complete reader-validated description payload.
     * @param position Cursor advanced by the primitive.
     * @param wide Whether the UTFString compact-long length is required.
     * @return Signed primitive value.
     */
    std::int64_t number(std::span<const std::byte> payload, std::size_t &position, bool wide = false) {
        const auto input = payload.subspan(position);
        encoding::ReadResult<std::int64_t> value;

        if (wide) {
            value = encoding::readCompactLong(input);
        } else {
            const auto narrow = encoding::readCompactInt(input);
            value = {narrow.value, narrow.consumed, narrow.status};
        }

        work(add(value.consumed, 1));
        position += value.consumed;

        if (value.status != encoding::Status::OK) {
            reject(AdmissionError::MALFORMED, "description-census");
        }

        return value.value;
    }

    /** Count exact UTFString decoded units without allocating a string.
     * @param payload Complete description bytes.
     * @param position Cursor advanced over length and body.
     * @return Actual decoded unit count.
     */
    std::size_t nameUnits(std::span<const std::byte> payload, std::size_t &position) {
        auto remaining = number(payload, position, true);

        if (remaining < 0) {
            reject(AdmissionError::MALFORMED, "description-census");
        }

        std::size_t units = 0;

        while (remaining > 0) {
            const auto character = encoding::readUtfChar(payload.subspan(position));
            work(add(character.consumed, 1));
            position += character.consumed;

            if (character.status != encoding::Status::OK) {
                reject(AdmissionError::MALFORMED, "description-census");
            }

            units = add(units, static_cast<std::uint32_t>(character.value) >= SUPPLEMENTARY_POINT ? 2 : 1);
            remaining -= static_cast<std::int64_t>(character.consumed);
        }

        if (remaining != 0) {
            reject(AdmissionError::MALFORMED, "description-census");
        }

        return units;
    }

    /** Census exact reparse storage before its first allocation.
     * @param payload Complete reader-validated description bytes.
     * @return Raw owning description logical shape.
     */
    Storage rawStorage(std::span<const std::byte> payload) {
        Storage result;
        std::size_t position = 0;

        while (position < payload.size()) {
            static_cast<void>(number(payload, position));
            work(1); // Reserve the raw record element publication during the separate owning parse.
            result = combine(result, {sizeof(WireRecordDescription), 0});
            result = combine(result, textStorage(nameUnits(payload, position)));
            const auto count = number(payload, position);

            if (count < 0) {
                reject(AdmissionError::MALFORMED, "description-census");
            }

            work(static_cast<std::size_t>(count)); // Reserve each raw field element copy before owning parsing.

            result.bytes = add(result.bytes, multiply(static_cast<std::size_t>(count), sizeof(WireFieldDescription)));

            for (std::int64_t i = 0; i < count; ++i) {
                result = combine(result, textStorage(nameUnits(payload, position)));
                static_cast<void>(number(payload, position));
            }
        }

        scratch(result);

        return result;
    }

    /** Reserve the existing binding's finite schema/lookup/name traversal before invoking it.
     * @param description Complete source description.
     */
    void bindingWork(const WireRecordDescription &description) {
        work(add(1, description.name.size()));

        for (const auto &field : description.fields) {
            work(add(1, field.name.size()));
        }

        // Charge every possible compared unit before the existing planner's bounded search.
        for (const auto &record : schema_.records()) {
            work(add(1, add(record.name.size(), description.name.size())));

            if (record.name != description.name) {
                continue;
            }

            for (const auto &field : description.fields) {
                for (const auto &key : record.lookups) {
                    work(add(1, add(key.name.size(), field.name.size())));

                    if (key.name == field.name) {
                        break;
                    }
                }
            }

            break;
        }

        work(description.fields.size());
    }

    /** Validate the outgoing/schema name policy while charging every examined unit.
     * @param name Complete exact wire record/field name.
     */
    void validName(std::u16string_view name) {
        if (name.empty()) {
            reject(AdmissionError::MALFORMED, "layout-name");
        }

        for (std::size_t i = 0; i < name.size(); ++i) {
            work(1);

            if (name[i] >= HIGH_SURROGATE_FIRST && name[i] < LOW_SURROGATE_FIRST) {
                if (++i == name.size()) {
                    reject(AdmissionError::MALFORMED, "layout-name");
                }

                work(1);

                if (name[i] < LOW_SURROGATE_FIRST || name[i] > SURROGATE_LAST) {
                    reject(AdmissionError::MALFORMED, "layout-name");
                }
            } else if (name[i] >= LOW_SURROGATE_FIRST && name[i] <= SURROGATE_LAST) {
                reject(AdmissionError::MALFORMED, "layout-name");
            }
        }
    }

    /** Strictly validate one complete record before moving its names into inventory.
     * @param raw Owning wire declaration; names transfer only after all strict checks.
     * @param rawShape Simultaneous remaining raw description storage.
     */
    void record(WireRecordDescription &raw, Storage &rawShape) {
        bindingWork(raw);
        const Storage bindingShape{add(sizeof(RecordBinding), multiply(raw.fields.size(), sizeof(FieldBinding))), 0};
        scratch(combine(rawShape, bindingShape));
        auto binding = bindRecord(raw, schema_,
                                  {limits_.layouts.fields, limits_.layouts.nameUnits, limits_.layouts.totalNameUnits});

        if (!binding) {
            diagnostic_.binding = binding.error();
            reject(binding.error() == BindingError::RESOURCE_LIMIT        ? AdmissionError::RESOURCE_LIMIT
                   : binding.error() == BindingError::ALLOCATION_FAILURE  ? AdmissionError::ALLOCATION_FAILURE
                   : binding.error() == BindingError::UNSUPPORTED_PROFILE ? AdmissionError::UNSUPPORTED
                                                                          : AdmissionError::MALFORMED,
                   "binding");
        }

        validName(raw.name);

        for (const auto &field : raw.fields) {
            validName(field.name);
        }

        if (!binding->localRecordId) {
            reject(AdmissionError::UNSUPPORTED, "unknown-record", AdmissionCode::UNKNOWN_RECORD);
        }

        const auto &local = schema_.records()[static_cast<std::size_t>(*binding->localRecordId)];

        for (std::size_t i = 0; i < binding->fields.size(); ++i) {
            diagnostic_.field = i;
            work(1);

            if (!binding->fields[i].targetFieldId) {
                reject(AdmissionError::UNSUPPORTED, "skipped-field", AdmissionCode::SKIPPED_FIELD);
            }

            for (std::size_t previous = 0; previous < i; ++previous) {
                work(1);

                if (binding->fields[previous].targetFieldId == binding->fields[i].targetFieldId) {
                    reject(AdmissionError::UNSUPPORTED, "repeated-target", AdmissionCode::REPEATED_TARGET);
                }
            }
        }

        diagnostic_.field.reset();

        if (binding->fields.size() != local.fields.size()) {
            reject(AdmissionError::UNSUPPORTED, "missing-target", AdmissionCode::MISSING_TARGET);
        }

        for (const auto &existing : records_) {
            work(add(1, add(existing.wireName.size(), raw.name.size())));

            if (existing.senderId == raw.senderId || existing.localRecordId == *binding->localRecordId ||
                existing.wireName == raw.name) {
                reject(AdmissionError::MALFORMED, "duplicate-layout-identity",
                       AdmissionCode::DUPLICATE_LAYOUT_IDENTITY);
            }
        }

        if (raw.senderId == std::numeric_limits<std::int32_t>::max()) {
            diagnostic_.writer = WriterDiagnostic{WriterError::INVALID_LAYOUT, WriterOperation::LAYOUT, "record-id",
                                                  diagnostic_.record, std::nullopt};
            reject(AdmissionError::UNSUPPORTED, "record-id");
        }

        if (records_.size() >= limits_.layouts.records) {
            reject(AdmissionError::RESOURCE_LIMIT, "layout-records");
        }

        std::size_t existingFields = 0;

        for (const auto &existing : records_) {
            work(1);
            existingFields = add(existingFields, existing.fields.size());
        }

        if (raw.fields.size() > limits_.layouts.fields - std::min(existingFields, limits_.layouts.fields)) {
            reject(AdmissionError::RESOURCE_LIMIT, "layout-fields");
        }

        std::size_t units = raw.name.size();

        for (const auto &field : raw.fields) {
            work(1);
            units = add(units, field.name.size());
        }

        if (units > limits_.layouts.totalNameUnits - std::min(inventory_.units, limits_.layouts.totalNameUnits)) {
            reject(AdmissionError::RESOURCE_LIMIT, "layout-name-units");
        }

        const auto addedObjects = add(sizeof(OutgoingRecord), multiply(raw.fields.size(), sizeof(OutgoingField)));
        scratch(combine(rawShape, combine(bindingShape, {addedObjects, 0})));
        work(add(1, raw.fields.size()));
        OutgoingRecord outgoing{raw.senderId, *binding->localRecordId, std::move(raw.name), {}};
        outgoing.fields.reserve(raw.fields.size());

        for (std::size_t i = 0; i < raw.fields.size(); ++i) {
            outgoing.fields.push_back({std::move(raw.fields[i].name), raw.fields[i].serialTypeId,
                                       static_cast<std::int32_t>(*binding->fields[i].targetFieldId)});
        }

        inventory_ = combine(inventory_, combine({addedObjects, 0}, textStorage(units)));
        rawShape.units -= units;
        rawShape.bytes -= multiply(units, sizeof(char16_t));
        records_.push_back(std::move(outgoing));
    }

    /** Reparse every description and admit a whole inventory update.
     * @param frame Successfully read owning description frame.
     */
    void descriptions(const Frame &frame) {
        auto shape = rawStorage(frame.payload);
        work(add(frame.payload.size(), shape.units));
        auto parsed =
            parseDescribeRecords(frame.payload, {limits_.reader.frame.maximumBodyBytes,
                                                 limits_.layouts.records,
                                                 limits_.layouts.fields,
                                                 limits_.layouts.fields,
                                                 {limits_.reader.frame.maximumBodyBytes, limits_.layouts.nameUnits},
                                                 limits_.layouts.totalNameUnits});

        if (parsed.status != DescriptionStatus::OK) {
            diagnostic_.description = parsed.status;
            reject(parsed.status == DescriptionStatus::RESOURCE_LIMIT       ? AdmissionError::RESOURCE_LIMIT
                   : parsed.status == DescriptionStatus::ALLOCATION_FAILURE ? AdmissionError::ALLOCATION_FAILURE
                                                                            : AdmissionError::MALFORMED,
                   "description");
        }

        const auto nextCount = add(records_.size(), parsed.descriptions.size());

        if (nextCount > limits_.layouts.records) {
            reject(AdmissionError::RESOURCE_LIMIT, "layout-records");
        }

        if (nextCount > records_.capacity()) {
            // Old and new vector element storage coexist during growth; strings transfer ownership.
            scratch(combine(shape, {multiply(nextCount, sizeof(OutgoingRecord)), 0}));
            work(records_.size());
            records_.reserve(nextCount);
        }

        for (std::size_t i = 0; i < parsed.descriptions.size(); ++i) {
            diagnostic_.record = i;
            record(parsed.descriptions[i], shape);
        }

        diagnostic_.record.reset();
        diagnostic_.field.reset();
    }

    /** Project accepted metadata exactly, checking storage before text copying.
     * @param source Owning accepted reader descriptor.
     */
    void descriptor(const TapeDescriptor &source) {
        if (!source.time || source.explicitTimeNone || source.explicitStreamSend) {
            reject(AdmissionError::UNSUPPORTED, "descriptor-projection", AdmissionCode::UNREPRESENTED_DESCRIPTOR);
        }

        auto units = source.time->size();

        if (source.version) {
            units = add(units, source.version->size());
        }

        if (source.options) {
            units = add(units, source.options->size());
        }

        work(units);
        descriptorStorage_ = textStorage(units);
        scratch();
        descriptor_ = {source.version, source.options, *source.time, source.explicitRawSend};
    }

    public:
    /** Prepare a scoped validator without allocation.
     * @param schema Borrowed immutable schema.
     * @param limits Explicit finite bounds.
     */
    AdmissionBuilder(const core::LocalSchema &schema, AdmissionLimits limits) : schema_(schema), limits_(limits) {
    }

    /** Validate complete bytes and construct final outgoing declarations.
     * @param input Complete borrowed source.
     * @return Owned declarations/source/header, or first failure.
     */
    std::expected<OutgoingLayouts, AdmissionDiagnostic> run(std::span<const std::byte> input) {
        try {
            if (input.size() > limits_.inputBytes) {
                reject(AdmissionError::RESOURCE_LIMIT, "input-bytes");
            }

            // Source copying is the only whole-input admission allocation; ownership transfers at commit.
            if (input.size() > limits_.scratchBytes) {
                reject(AdmissionError::RESOURCE_LIMIT, "scratch-bytes");
            }

            work(input.size());
            diagnostic_.counters.peakScratchBytes = input.size();
            bytes_.assign(input.begin(), input.end());
            TapeReader reader(schema_, limits_.reader, TapeMode::RAW_MESSAGE_V1);
            std::size_t position = 0;

            while (position < bytes_.size()) {
                auto result = reader.feed(std::span<const std::byte>(bytes_).subspan(position));
                position += result.accepted;

                if (result.status == TapeStatus::FAILED) {
                    diagnostic_.reader = reader.diagnostic();
                    diagnostic_.frameStart = reader.diagnostic().frameStart;
                    const auto error = reader.diagnostic().error;

                    // A present null option is explicitly outside the represented descriptor subset.
                    // Preserve the native malformed cause while returning the strict unsupported category.
                    if (error == TapeError::MALFORMED && reader.diagnostic().stage == "descriptor" &&
                        reader.diagnostic().rejection == TapeRejection::OPTIONS) {
                        reject(AdmissionError::UNSUPPORTED, "descriptor-projection",
                               AdmissionCode::UNREPRESENTED_DESCRIPTOR);
                    }

                    reject(error == TapeError::RESOURCE_LIMIT       ? AdmissionError::RESOURCE_LIMIT
                           : error == TapeError::ALLOCATION_FAILURE ? AdmissionError::ALLOCATION_FAILURE
                           : error == TapeError::UNSUPPORTED        ? AdmissionError::UNSUPPORTED
                                                                    : AdmissionError::MALFORMED,
                           "reader");
                }

                if (result.frame) {
                    diagnostic_.frameStart = result.frame->startOffset;
                    ++diagnostic_.counters.frames;

                    if (result.frame->type == DESCRIBE_RECORDS) {
                        descriptions(*result.frame);
                    } else if (diagnostic_.counters.frames == 1) {
                        descriptor(*reader.descriptor());
                    }
                }

                if (result.accepted == 0 && !result.frame) {
                    reject(AdmissionError::MALFORMED, "reader-progress");
                }
            }

            if (reader.finish() != TapeStatus::FINISHED) {
                diagnostic_.reader = reader.diagnostic();
                diagnostic_.frameStart = reader.diagnostic().frameStart;
                reject(AdmissionError::MALFORMED, "reader-finish");
            }

            // Final creation duplicates outgoing names and the validated local schema while old candidates live.
            // Reader-owned schema/framing/descriptions are independently bounded by TapeLimits, not extra scratch.
            const auto schemaShape = schemaStorage();
            scratch(combine(inventory_, schemaShape));
            work(add(inventory_.units, schemaShape.units));

            // The census visits names/containers; this reservation separately charges every copied element.
            for (const auto &record : schema_.records()) {
                work(add(1, add(record.fields.size(), record.lookups.size())));
            }

            // Outgoing construction searches explicit names/targets; reserve finite comparison visits first.
            for (const auto &record : records_) {
                work(add(1, record.fields.size()));

                for (const auto &local : schema_.records()) {
                    work(add(1, add(local.name.size(), record.wireName.size())));
                }

                for (const auto &field : record.fields) {
                    work(field.wireName.size());

                    for (const auto &otherField : record.fields) {
                        work(add(1, add(otherField.wireName.size(), field.wireName.size())));
                    }

                    for (const auto &key : schema_.records()[static_cast<std::size_t>(record.localRecordId)].lookups) {
                        work(add(1, add(key.name.size(), field.wireName.size())));
                    }
                }

                for (const auto &other : records_) {
                    work(add(1, add(other.wireName.size(), record.wireName.size())));
                }
            }

            auto layouts = OutgoingLayouts::create(schema_, records_, limits_.layouts);

            if (!layouts) {
                diagnostic_.writer = layouts.error();
                reject(layouts.error().error == WriterError::RESOURCE_LIMIT       ? AdmissionError::RESOURCE_LIMIT
                       : layouts.error().error == WriterError::ALLOCATION_FAILURE ? AdmissionError::ALLOCATION_FAILURE
                       : layouts.error().error == WriterError::UNSUPPORTED        ? AdmissionError::UNSUPPORTED
                                                                                  : AdmissionError::MALFORMED,
                       "layouts");
            }

            return std::move(*layouts);
        } catch (const Failure &) {
            return std::unexpected(diagnostic_);
        } catch (const std::bad_alloc &) {
            diagnostic_.error = AdmissionError::ALLOCATION_FAILURE;
            diagnostic_.stage = "allocation";

            return std::unexpected(diagnostic_);
        } catch (const std::length_error &) {
            diagnostic_.error = AdmissionError::ALLOCATION_FAILURE;
            diagnostic_.stage = "allocation";

            return std::unexpected(diagnostic_);
        } catch (const std::invalid_argument &) {
            diagnostic_.error = AdmissionError::RESOURCE_LIMIT;
            diagnostic_.stage = "reader-configuration";

            return std::unexpected(diagnostic_);
        }
    }

    /** Transfer the sole source-byte allocation after complete success.
     * @return Owned exact bytes.
     */
    std::vector<std::byte> takeBytes() noexcept {
        return std::move(bytes_);
    }

    /** Transfer represented descriptor text after complete success.
     * @return Owned exact metadata.
     */
    OutgoingDescriptor takeDescriptor() noexcept {
        return std::move(descriptor_);
    }

    /** Inspect executed charges.
     * @return Actual cumulative/peak counters.
     */
    AdmissionCounters counters() const noexcept {
        return diagnostic_.counters;
    }
};
} // namespace

AdmittedTape::AdmittedTape(std::vector<std::byte> bytes, OutgoingLayouts layouts, OutgoingDescriptor descriptor,
                           AdmissionLimits limits, AdmissionCounters counters) noexcept
    : bytes_(std::move(bytes)), layouts_(std::move(layouts)), descriptor_(std::move(descriptor)), limits_(limits),
      counters_(counters) {
}

AdmittedTape::AdmittedTape(AdmittedTape &&other) noexcept
    : bytes_(std::move(other.bytes_)), layouts_(std::move(other.layouts_)), descriptor_(std::move(other.descriptor_)),
      limits_(other.limits_), counters_(other.counters_), active_(std::exchange(other.active_, false)) {
}

bool AdmittedTape::valid() const noexcept {
    return active_;
}

std::span<const std::byte> AdmittedTape::bytes() const noexcept {
    return bytes_;
}

const core::LocalSchema &AdmittedTape::schema() const noexcept {
    return layouts_.schema();
}

const OutgoingLayouts &AdmittedTape::layouts() const noexcept {
    return layouts_;
}

const OutgoingDescriptor &AdmittedTape::descriptor() const noexcept {
    return descriptor_;
}

TapeLimits AdmittedTape::readerLimits() const noexcept {
    return limits_.reader;
}

AdmissionCounters AdmittedTape::counters() const noexcept {
    return counters_;
}

std::expected<AdmittedTape, AdmissionDiagnostic> admitTape(const core::LocalSchema &schema,
                                                           std::span<const std::byte> input, AdmissionLimits limits) {
    AdmissionBuilder builder(schema, limits);
    auto layouts = builder.run(input);

    if (!layouts) {
        return std::unexpected(layouts.error());
    }

    return AdmittedTape(builder.takeBytes(), std::move(*layouts), builder.takeDescriptor(), limits, builder.counters());
}
} // namespace org::ttldtor::qdcxx::protocol
