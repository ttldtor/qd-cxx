// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "shared_record_payload.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <utility>

namespace org::ttldtor::qdcxx::core::detail {

/** Admission errors for the private schema-bound storage candidate. */
enum class RecordBlockError {
    WRONG_CONTEXT,     ///< A row does not originate from the binding schema.
    INVALID_RECORD,    ///< A field count/type/value disagrees with its local declaration.
    INVALID_RANGE,     ///< A selected range is outside the immutable block.
    CAPACITY_EXCEEDED, ///< This request cannot fit even in an empty byte domain.
    WOULD_BLOCK,       ///< Outstanding reservations temporarily prevent this request.
    SIZE_OVERFLOW,     ///< Required capacity is not representable by size_t.
    ALLOCATION_FAILURE ///< Allocation failed without publishing a partial owner.
};

/** Detached byte domain; counters and final releases require no operational owner. */
struct RecordByteState {
    const std::size_t capacity;
    std::atomic<std::size_t> used{0};

    explicit RecordByteState(std::size_t bytes) : capacity(bytes) {
    }

    void reserve(std::size_t bytes) {
        if (bytes > capacity) {
            throw RecordBlockError::CAPACITY_EXCEEDED;
        }

        auto current = used.load(std::memory_order_relaxed);

        do {
            if (bytes > capacity - current) {
                throw RecordBlockError::WOULD_BLOCK;
            }
        } while (!used.compare_exchange_weak(current, current + bytes, std::memory_order_relaxed));
    }
};

/** Move-only reservation; copies of storage owners do not duplicate physical charges. */
class RecordByteReservation {
    std::shared_ptr<RecordByteState> state_;
    std::size_t bytes_ = 0;

    public:
    RecordByteReservation() = default;

    RecordByteReservation(std::shared_ptr<RecordByteState> state, std::size_t bytes)
        : state_(std::move(state)), bytes_(bytes) {
        state_->reserve(bytes_);
    }

    /** Adopt part of one already successful aggregate reservation without a second atomic update. */
    static RecordByteReservation adopt(std::shared_ptr<RecordByteState> state, std::size_t bytes) noexcept {
        RecordByteReservation result;
        result.state_ = std::move(state);
        result.bytes_ = bytes;

        return result;
    }

    RecordByteReservation(const RecordByteReservation &) = delete;
    RecordByteReservation &operator=(const RecordByteReservation &) = delete;

    RecordByteReservation(RecordByteReservation &&other) noexcept
        : state_(std::move(other.state_)), bytes_(std::exchange(other.bytes_, 0)) {
    }

    RecordByteReservation &operator=(RecordByteReservation &&other) noexcept {
        if (this != &other) {
            RecordByteReservation old(std::move(*this));
            state_ = std::move(other.state_);
            bytes_ = std::exchange(other.bytes_, 0);
        }

        return *this;
    }

    ~RecordByteReservation() {
        if (state_) {
            state_->used.fetch_sub(bytes_, std::memory_order_relaxed);
        }
    }
};

/** Allocation-request measurement; excludes allocator bookkeeping and process RSS. */
struct RecordAllocationMeasurement {
    std::size_t bytes;
};

/** Charge shared controls until actual allocator deallocation, including outstanding weak owners. */
template <class T> struct RecordBlockAllocator {
    using value_type = T;
    std::shared_ptr<RecordByteState> state;
    bool measuring = false;
    bool prepaid = false; ///< Factory already reserved this control together with its backing in one CAS.

    RecordBlockAllocator() = default;

    RecordBlockAllocator(std::shared_ptr<RecordByteState> source, bool measure = false, bool reserved = false)
        : state(std::move(source)), measuring(measure), prepaid(reserved) {
    }

    template <class U>
    RecordBlockAllocator(const RecordBlockAllocator<U> &other)
        : state(other.state), measuring(other.measuring), prepaid(other.prepaid) {
    }

    T *allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length{};
        }

        const auto bytes = count * sizeof(T);

        if (measuring) {
            throw RecordAllocationMeasurement{bytes};
        }

        if (state && !prepaid) {
            state->reserve(bytes);
        }

        try {
            return std::allocator<T>{}.allocate(count);
        } catch (...) {
            if (state) {
                state->used.fetch_sub(bytes, std::memory_order_relaxed);
            }

            throw;
        }
    }

    void deallocate(T *pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);

        if (state) {
            state->used.fetch_sub(count * sizeof(T), std::memory_order_relaxed);
        }
    }

    template <class U> bool operator==(const RecordBlockAllocator<U> &other) const noexcept {
        return state == other.state && measuring == other.measuring && prepaid == other.prepaid;
    }
};

inline std::atomic<std::size_t> recordControlMeasurementCount{
    0}; ///< Cold layout probes; no memory is allocated, not a runtime allocation count.

/** Obtain the actual allocate_shared request shape without constructing an object.
 * @tparam T Shared object type.
 * @tparam Args Constructor argument types.
 * @param args Constructor shape; not consumed because the measuring allocator always throws.
 * @return Allocator-request bytes, including the implementation's shared control.
 */
template <class T, class... Args> std::size_t recordControlBytes(Args &&...args) {
    // For this fixed allocator/T/constructor signature, control layout is independent of argument values.
    // The measuring allocator throws before construction, so forwarded inputs are never consumed.
    static const auto bytes = [&] {
        try {
            (void)std::allocate_shared<T>(RecordBlockAllocator<T>{{}, true}, std::forward<Args>(args)...);
        } catch (const RecordAllocationMeasurement &measurement) {
            recordControlMeasurementCount.fetch_add(1, std::memory_order_relaxed);

            return measurement.bytes;
        }

        throw std::bad_alloc{};
    }();

    return bytes;
}

/** Checked sum used before any data allocation or publication.
 * @param left Existing byte count.
 * @param right Additional bytes.
 * @return Representable sum; otherwise throws SIZE_OVERFLOW.
 */
inline std::size_t recordByteSum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw RecordBlockError::SIZE_OVERFLOW;
    }

    return left + right;
}

/** Explicit pure byte-capacity owner, reusable by independent physical or consumer domains. */
class RecordByteBudget {
    std::shared_ptr<RecordByteState> state_;

    public:
    /** Allocate one detached counter control; its request is reported separately from capacity.
     * @param bytes Maximum simultaneous requested/modelled reservations in this domain.
     */
    explicit RecordByteBudget(std::size_t bytes)
        : state_(std::allocate_shared<RecordByteState>(RecordBlockAllocator<RecordByteState>{}, bytes)) {
    }

    [[nodiscard]] const std::shared_ptr<RecordByteState> &state() const noexcept {
        return state_;
    }

    [[nodiscard]] std::size_t usedBytes() const noexcept {
        return state_->used.load(std::memory_order_relaxed);
    }

    /** Measure the one byte-domain control request, outside its configured data capacity.
     * @return Allocator-request bytes for the counter object and shared control.
     */
    [[nodiscard]] static std::size_t metadataAllocationBytes() {
        return recordControlBytes<RecordByteState>(0);
    }
};

/** Caller-supplied source labels; no global uniqueness or authentication is implied. */
struct RecordSourceGeneration {
    std::uint64_t origin = 0;
    std::uint64_t generation = 0;
    bool operator==(const RecordSourceGeneration &) const = default;
};

/** Exact physical virtual-field words, without protocol/session ownership. */
struct RecordFieldTime {
    std::int64_t seconds = 0;
    std::optional<std::int64_t> sequence;
    std::uint64_t sourceOrdinal = 0;
    bool operator==(const RecordFieldTime &) const = default;
};

/** Fixed source metadata; projection/collector state remains outside immutable input blocks. */
struct RecordBlockMetadata {
    std::int32_t senderId = 0;
    std::int32_t cipher = 0;
    std::int32_t flags = 0;
    std::optional<std::int64_t> removalTime;
    std::optional<RecordFieldTime> fieldTime;
    std::optional<std::int64_t> tapeMessageTimeMs;
    bool operator==(const RecordBlockMetadata &) const = default;
};

/** Complete schema snapshot created once at binding, independent of a network/runtime lifetime. */
struct RecordBlockBinding {
    RecordByteReservation schemaReservation;
    LocalSchema schema;
    const RecordSourceGeneration source;
    const std::size_t schemaStorageBytes; ///< LocalSchema capacity model; not measured allocator requests.
    const std::size_t controlAllocationBytes;

    RecordBlockBinding(RecordByteReservation reservation, LocalSchema &&catalog, RecordSourceGeneration generation,
                       std::size_t storageBytes, std::size_t controlBytes)
        : schemaReservation(std::move(reservation)), schema(std::move(catalog)), source(generation),
          schemaStorageBytes(storageBytes), controlAllocationBytes(controlBytes) {
    }

    /** Transfer an already owned schema once, preserving provenance; failure leaves it unchanged.
     * @param catalog Owned declarations to move only after all reservations and the control allocation succeed.
     * @param source Caller-supplied detached labels.
     * @param physical Separate physical-capacity domain, retained through final release.
     * @return Immutable binding or atomic admission/allocation failure.
     */
    static std::expected<std::shared_ptr<const RecordBlockBinding>, RecordBlockError>
    create(LocalSchema &&catalog, RecordSourceGeneration source, const RecordByteBudget &physical) {
        if (!catalog.identity().valid()) {
            return std::unexpected(RecordBlockError::WRONG_CONTEXT);
        }

        const auto storage = catalog.storageBytes();
        const auto control =
            recordControlBytes<RecordBlockBinding>(RecordByteReservation{}, std::move(catalog), source, storage, 0);

        try {
            if (recordByteSum(storage - sizeof(LocalSchema), control) > physical.state()->capacity) {
                return std::unexpected(RecordBlockError::CAPACITY_EXCEEDED);
            }

            // LocalSchema includes its fixed object, which already resides in the shared-control request.
            physical.state()->reserve(recordByteSum(storage - sizeof(LocalSchema), control));
            auto reservation = RecordByteReservation::adopt(physical.state(), storage - sizeof(LocalSchema));

            return std::allocate_shared<RecordBlockBinding>(
                RecordBlockAllocator<RecordBlockBinding>{physical.state(), false, true}, std::move(reservation),
                std::move(catalog), source, storage, control);
        } catch (RecordBlockError error) {
            return std::unexpected(error);
        } catch (const std::bad_alloc &) {
            return std::unexpected(RecordBlockError::ALLOCATION_FAILURE);
        }
    }

    [[nodiscard]] std::size_t retainedCapacityBytes() const {
        return recordByteSum(controlAllocationBytes, schemaStorageBytes - sizeof(LocalSchema));
    }
};

/** Borrowed exact input; context is validated against the immutable binding before allocation. */
struct RecordBlockInput {
    RecordContext context;
    std::u16string_view symbol;
    std::span<const FieldValueView> fields;
    RecordBlockMetadata metadata;
};

/** No counter claims to be allocator overhead or process RSS; schema uses its existing capacity model. */
struct RecordBlockAccounting {
    std::size_t payloadCopyBytes = 0;     ///< Symbol plus engaged UTF-16/opaque payload bytes actually copied.
    std::size_t fieldDescriptorBytes = 0; ///< Exact raw/tag/nullable descriptor bytes constructed.
    std::size_t rowDescriptorBytes = 0;   ///< Exact SharedRecordPayload row descriptors.
    std::size_t recordMetadataBytes = 0;  ///< Local identity plus complete source metadata, one entry per row.
    std::size_t paddingBytes = 0;         ///< Packed-storage alignment, not payload copies.
    std::size_t backingAllocationBytes = 0;
    std::size_t controlAllocationBytes = 0;
    std::size_t dataAllocationCount = 0; ///< Actual block backing plus shared control requests; excludes binding.
};

/** Payload accounting policy; complete block capacity/retains always include the binding. */
enum class RecordPayloadResidency {
    WHOLE_RETAINED,  ///< Each payload consumer pays for all capacity it can keep alive.
    SEPARATE_BINDING ///< Payload-only quota requires an exact, separately reserved binding owner in its domain.
};

/** Extended Batch; row aliases retain this actual derived shared-control object and binding. */
struct BoundRecordBatch : SharedRecordPayload::Batch {
    struct Metadata {
        std::int32_t localId;
        RecordBlockMetadata source;
    };

    RecordByteReservation backingReservation;
    std::shared_ptr<const RecordBlockBinding> binding;
    const RecordBlockAccounting accounting;
    Metadata *metadata = nullptr;
    const std::size_t count;
    RecordPayloadResidency residency =
        RecordPayloadResidency::WHOLE_RETAINED; ///< Payload-only mode requires a matching external fixed reservation.

    BoundRecordBatch(RecordByteReservation reservation, std::shared_ptr<const RecordBlockBinding> snapshot,
                     RecordBlockAccounting measured, std::size_t rows)
        : Batch(measured.backingAllocationBytes), backingReservation(std::move(reservation)),
          binding(std::move(snapshot)), accounting(measured), count(rows) {
    }

    ~BoundRecordBatch() {
        // Free data before releasing its capacity. The allocator keeps the control charge through weak owners.
        backing.reset();
    }
};

/** Move-only consumer admission; an independent retain takes an independent consumer reservation. */
class RetainedRecordBlock {
    RecordByteReservation reservation_;
    std::shared_ptr<const BoundRecordBatch> owner_;
    std::size_t first_ = 0;
    std::size_t count_ = 0;

    public:
    RetainedRecordBlock(RecordByteReservation reservation, std::shared_ptr<const BoundRecordBatch> owner,
                        std::size_t first, std::size_t count)
        : reservation_(std::move(reservation)), owner_(std::move(owner)), first_(first), count_(count) {
    }

    RetainedRecordBlock(const RetainedRecordBlock &) = delete;
    RetainedRecordBlock &operator=(const RetainedRecordBlock &) = delete;
    RetainedRecordBlock(RetainedRecordBlock &&) noexcept = default;

    RetainedRecordBlock &operator=(RetainedRecordBlock &&other) noexcept {
        if (this != &other) {
            RetainedRecordBlock old(std::move(*this));
            reservation_ = std::move(other.reservation_);
            owner_ = std::move(other.owner_);
            first_ = other.first_;
            count_ = other.count_;
        }

        return *this;
    }

    [[nodiscard]] const std::shared_ptr<const BoundRecordBatch> &owner() const noexcept {
        return owner_;
    }

    [[nodiscard]] std::size_t first() const noexcept {
        return first_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return count_;
    }
};

/** Schema-bound operations reuse existing immutable packed payloads; no public delivery API is introduced. */
class SharedRecordBlock {
    static std::size_t product(std::size_t count, std::size_t bytes) {
        if (bytes && count > std::numeric_limits<std::size_t>::max() / bytes) {
            throw RecordBlockError::SIZE_OVERFLOW;
        }

        return count * bytes;
    }

    static std::size_t aligned(std::size_t bytes) {
        constexpr auto ALIGNMENT = alignof(std::max_align_t);

        return recordByteSum(bytes, ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    static std::size_t capacity(const BoundRecordBatch &block) {
        return recordByteSum(
            recordByteSum(block.accounting.backingAllocationBytes, block.accounting.controlAllocationBytes),
            block.binding->retainedCapacityBytes());
    }

    public:
    /** Calculate exact block requests and copy work without copying schema/payload or allocating.
     * @tparam Rows Stable indexed sequence of RecordBlockInput values.
     * @param binding Immutable complete declarations and source generation.
     * @param rows Borrowed input records; all contexts and exact field shapes must match.
     * @return Requested block layout or validation/arithmetic error.
     */
    template <class Rows>
    static std::expected<RecordBlockAccounting, RecordBlockError> measure(const RecordBlockBinding &binding,
                                                                          const Rows &rows) {
        try {
            RecordBlockAccounting result;
            result.rowDescriptorBytes = product(rows.size(), sizeof(SharedRecordPayload));
            auto offset = result.rowDescriptorBytes;

            for (std::size_t index = 0; index < rows.size(); ++index) {
                const auto &row = rows[index];
                const auto id = binding.schema.recordId(row.context);

                if (!id) {
                    return std::unexpected(RecordBlockError::WRONG_CONTEXT);
                }

                const auto &declaration = binding.schema.records()[static_cast<std::size_t>(*id)];

                if (row.fields.size() != declaration.fields.size()) {
                    return std::unexpected(RecordBlockError::INVALID_RECORD);
                }

                const auto next = aligned(offset);
                result.paddingBytes = recordByteSum(result.paddingBytes, next - offset);
                offset = next;
                const auto descriptors = product(row.fields.size(), sizeof(FieldValueView));
                result.fieldDescriptorBytes = recordByteSum(result.fieldDescriptorBytes, descriptors);
                offset = recordByteSum(offset, descriptors);
                auto copied = product(row.symbol.size(), sizeof(char16_t));

                for (std::size_t field = 0; field < row.fields.size(); ++field) {
                    const auto &value = row.fields[field];

                    if (static_cast<std::int32_t>(value.type) != declaration.fields[field].serialTypeId ||
                        !validFieldValueView(value)) {
                        return std::unexpected(RecordBlockError::INVALID_RECORD);
                    }

                    copied = recordByteSum(copied, value.units ? product(value.units->size(), sizeof(char16_t)) : 0);
                    copied = recordByteSum(copied, value.bytes ? value.bytes->size() : 0);
                }

                result.payloadCopyBytes = recordByteSum(result.payloadCopyBytes, copied);
                offset = recordByteSum(offset, copied);
            }

            const auto metadataOffset = aligned(offset);
            result.paddingBytes = recordByteSum(result.paddingBytes, metadataOffset - offset);
            result.recordMetadataBytes = product(rows.size(), sizeof(BoundRecordBatch::Metadata));
            result.backingAllocationBytes = recordByteSum(metadataOffset, result.recordMetadataBytes);
            result.controlAllocationBytes = recordControlBytes<BoundRecordBatch>(
                RecordByteReservation{}, std::shared_ptr<const RecordBlockBinding>{}, result, rows.size());
            result.dataAllocationCount = 2;

            return result;
        } catch (RecordBlockError error) {
            return std::unexpected(error);
        }
    }

    /** Copy once into the existing packed Batch, with full schema/generation and exact metadata.
     * @tparam Rows Stable indexed sequence of RecordBlockInput values.
     * @param binding Reused immutable binding; no declaration copy is performed per block.
     * @param rows Source valid through this synchronous call.
     * @param physical Physical domain; both allocation requests reserve before constructing storage.
     * @param residency Full retain by default; separate mode requires each payload consumer to own the binding
     * reservation.
     * @return Complete immutable block; rejection publishes no owner and restores all reservations.
     */
    template <class Rows>
    static std::expected<std::shared_ptr<const BoundRecordBatch>, RecordBlockError>
    copy(std::shared_ptr<const RecordBlockBinding> binding, const Rows &rows, const RecordByteBudget &physical,
         RecordPayloadResidency residency = RecordPayloadResidency::WHOLE_RETAINED) {
        if (!binding) {
            return std::unexpected(RecordBlockError::WRONG_CONTEXT);
        }

        const auto measured = measure(*binding, rows);

        if (!measured) {
            return std::unexpected(measured.error());
        }

        try {
            // Test the whole permanent request, not its two individually allocatable pieces.
            const auto requested = recordByteSum(measured->backingAllocationBytes, measured->controlAllocationBytes);

            if (requested > physical.state()->capacity) {
                return std::unexpected(RecordBlockError::CAPACITY_EXCEEDED);
            }

            const auto retained = recordByteSum(requested, binding->retainedCapacityBytes());
            physical.state()->reserve(requested);
            auto reservation = RecordByteReservation::adopt(physical.state(), measured->backingAllocationBytes);
            auto block = std::allocate_shared<BoundRecordBatch>(
                RecordBlockAllocator<BoundRecordBatch>{physical.state(), false, true}, std::move(reservation),
                std::move(binding), *measured, rows.size());
            static_assert(std::is_trivially_destructible_v<BoundRecordBatch::Metadata>);
            SharedRecordPayload::fillBatch(*block, rows);
            const auto metadataOffset = measured->backingAllocationBytes - measured->recordMetadataBytes;
            block->metadata = reinterpret_cast<BoundRecordBatch::Metadata *>(block->backing.get() + metadataOffset);

            for (std::size_t i = 0; i < rows.size(); ++i) {
                std::construct_at(block->metadata + i, *block->binding->schema.recordId(rows[i].context),
                                  rows[i].metadata);
            }

            // Existing collector/MUX accounting sees the complete retained capacity through the base row.
            block->bytes = retained - SharedRecordPayload::controlBytes();
            block->residency = residency;

            if (residency == RecordPayloadResidency::SEPARATE_BINDING) {
                block->fixedOwnerIdentity = block->binding.get();
                block->fixedOwnerBytes = block->binding->retainedCapacityBytes();
            }

            return block;
        } catch (RecordBlockError error) {
            return std::unexpected(error);
        } catch (const std::bad_alloc &) {
            return std::unexpected(RecordBlockError::ALLOCATION_FAILURE);
        }
    }

    /** Independently copy a contiguous selection without allocating a staging descriptor array.
     * @param source Borrowed immutable source kept alive throughout this synchronous call.
     * @param first First selected record.
     * @param count Number of selected records.
     * @param physical Capacity domain for the new block and control allocation.
     * @return Independent smaller backing with the same residency policy and binding, or atomic failure.
     */
    static std::expected<std::shared_ptr<const BoundRecordBatch>, RecordBlockError>
    copyRange(const BoundRecordBatch &source, std::size_t first, std::size_t count, const RecordByteBudget &physical) {
        if (first > source.count || count > source.count - first) {
            return std::unexpected(RecordBlockError::INVALID_RANGE);
        }

        struct Selection {
            const BoundRecordBatch &source;
            std::size_t first;
            std::size_t count;

            std::size_t size() const noexcept {
                return count;
            }

            RecordBlockInput operator[](std::size_t index) const {
                const auto row = first + index;
                const auto &payload = source.rows[row];
                const auto &metadata = source.metadata[row];

                return {source.binding->schema.recordContext(metadata.localId), payload.symbol(), payload.fields(),
                        metadata.source};
            }
        };

        return copy(source.binding, Selection{source, first, count}, physical, source.residency);
    }

    /** Retain a logical range, charging the actual whole block and associated binding to this consumer.
     * @param block Immutable admitted owner; aliases preserve the same allocation and perform no payload copies.
     * @param first First selected row.
     * @param count Number of rows, including zero.
     * @param consumer Independent consumer-capacity domain.
     * @return Move-only lease without allocating; failure leaves the original owner/borrow intact.
     */
    static std::expected<RetainedRecordBlock, RecordBlockError> retain(std::shared_ptr<const BoundRecordBatch> block,
                                                                       std::size_t first, std::size_t count,
                                                                       const RecordByteBudget &consumer) {
        if (!block || first > block->count || count > block->count - first) {
            return std::unexpected(RecordBlockError::INVALID_RANGE);
        }

        try {
            RecordByteReservation reservation(consumer.state(), capacity(*block));

            return RetainedRecordBlock(std::move(reservation), std::move(block), first, count);
        } catch (RecordBlockError error) {
            return std::unexpected(error);
        }
    }
};

} // namespace org::ttldtor::qdcxx::core::detail
