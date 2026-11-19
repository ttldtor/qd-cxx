// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/serial_field_type.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

namespace org::ttldtor::qdcxx::core::detail {

/** One immutable row view within a batch; every alias retains the complete batch allocation. */
class SharedRecordPayload {
    struct Measurement {
        std::size_t bytes;
    };

    template <class T> struct Allocator {
        using value_type = T;
        bool measuring = false;
        Allocator() = default;

        explicit Allocator(bool measure) : measuring(measure) {
        }

        template <class U> Allocator(const Allocator<U> &other) : measuring(other.measuring) {
        }

        T *allocate(std::size_t count) {
            if (measuring)
                throw Measurement{count * sizeof(T)};

            return std::allocator<T>{}.allocate(count);
        }

        void deallocate(T *pointer, std::size_t count) noexcept {
            std::allocator<T>{}.deallocate(pointer, count);
        }

        template <class U> bool operator==(const Allocator<U> &other) const noexcept {
            return measuring == other.measuring;
        }
    };

    static std::size_t add(std::size_t left, std::size_t right) {
        if (right > std::numeric_limits<std::size_t>::max() - left)
            throw std::bad_array_new_length{};

        return left + right;
    }

    static std::size_t multiply(std::size_t count, std::size_t bytes) {
        if (bytes && count > std::numeric_limits<std::size_t>::max() / bytes)
            throw std::bad_array_new_length{};

        return count * bytes;
    }

    static std::size_t aligned(std::size_t offset) {
        constexpr auto ALIGNMENT = alignof(std::max_align_t);

        return add(offset, ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

    template <class Row> static std::size_t rowBytes(const Row &row) {
        auto bytes =
            add(multiply(row.fields.size(), sizeof(FieldValueView)), multiply(row.symbol.size(), sizeof(char16_t)));

        for (const auto &field : row.fields) {
            if (field.units)
                bytes = add(bytes, multiply(field.units->size(), sizeof(char16_t)));

            if (field.bytes)
                bytes = add(bytes, field.bytes->size());
        }

        return bytes;
    }

    public:
    struct Batch;

    private:
    Batch *batch_ = nullptr;
    const std::byte *data_ = nullptr;
    std::size_t symbolUnits_ = 0;
    std::size_t fields_ = 0;
    std::size_t bytes_ = 0;

    public:
    SharedRecordPayload() = default;

    SharedRecordPayload(Batch *batch, const std::byte *data, std::size_t symbolUnits, std::size_t fields,
                        std::size_t bytes) noexcept
        : batch_(batch), data_(data), symbolUnits_(symbolUnits), fields_(fields), bytes_(bytes) {
    }

    static std::size_t controlBytes();

    template <class Row> static std::size_t batchBytes(std::span<const Row> rows) {
        auto bytes = multiply(rows.size(), sizeof(SharedRecordPayload));

        for (const auto &row : rows)
            bytes = add(aligned(bytes), rowBytes(row));

        return add(bytes, controlBytes());
    }
    template <class Row> static std::shared_ptr<Batch> copyBatch(std::span<const Row> rows);

    /** Fill already allocated batch storage, shared by ordinary and schema-bound owners.
     * @tparam Rows Indexed borrowed sequence exposing size(), symbol and fields.
     * @param batch Exclusive candidate with enough backing for batchBytes(rows) minus controlBytes().
     * @param rows Immutable input retained only until this call returns.
     */
    template <class Rows> static void fillBatch(Batch &batch, const Rows &rows);
    static std::shared_ptr<const SharedRecordPayload>
    adopt(std::unique_ptr<std::byte[]> backing, std::size_t symbolUnits, std::size_t fields, std::size_t bytes);
    static std::shared_ptr<const SharedRecordPayload> row(const std::shared_ptr<Batch> &batch, std::size_t index);
    static std::shared_ptr<const SharedRecordPayload> row(const std::shared_ptr<const Batch> &batch, std::size_t index);

    [[nodiscard]] const std::byte *data() const noexcept {
        return data_;
    }

    [[nodiscard]] std::u16string_view symbol() const noexcept {
        return {reinterpret_cast<const char16_t *>(data_ + fields_ * sizeof(FieldValueView)), symbolUnits_};
    }

    [[nodiscard]] std::span<const FieldValueView> fields() const noexcept {
        return {reinterpret_cast<const FieldValueView *>(data_), fields_};
    }

    [[nodiscard]] std::size_t residentBytes() const noexcept;

    [[nodiscard]] std::size_t payloadBytes() const noexcept {
        return bytes_;
    }

    [[nodiscard]] const void *batchIdentity() const noexcept {
        return batch_;
    }

    /** @return Separately reserved immutable metadata identity, or null for ordinary whole-retained accounting. */
    [[nodiscard]] const void *fixedOwnerIdentity() const noexcept;
    /** @return Capacity charged once to a matching metadata owner instead of every payload. */
    [[nodiscard]] std::size_t fixedOwnerBytes() const noexcept;
    /** Worker-only weak cache; immutable bytes may be read and final aliases released on other threads. */
    [[nodiscard]] std::atomic<std::weak_ptr<void>> &queueAccounting() const noexcept;
};

/** Pure batch storage and worker-owned weak accounting cache, without protocol/session lifetime. */
struct SharedRecordPayload::Batch {
    std::unique_ptr<std::byte[]> backing;
    std::size_t bytes;
    SharedRecordPayload *rows = nullptr;
    SharedRecordPayload single;
    std::atomic<std::weak_ptr<void>> queueAccounting;
    const void *fixedOwnerIdentity = nullptr; ///< Non-owning identity kept alive by the schema-bound derived batch.
    std::size_t fixedOwnerBytes = 0;          ///< Capacity reserved once in an exact matching consumer metadata domain.

    explicit Batch(std::size_t size) : backing(std::make_unique_for_overwrite<std::byte[]>(size)), bytes(size) {
    }

    Batch(std::unique_ptr<std::byte[]> storage, std::size_t size) : backing(std::move(storage)), bytes(size) {
    }
};

inline std::size_t SharedRecordPayload::controlBytes() {
    static const auto bytes = [] {
        try {
            (void)std::allocate_shared<Batch>(Allocator<Batch>{true}, 0);
        } catch (const Measurement &measurement) {
            return measurement.bytes;
        }

        return std::size_t{0};
    }();

    return bytes;
}

inline std::size_t SharedRecordPayload::residentBytes() const noexcept {
    return batch_->bytes + controlBytes() - batch_->fixedOwnerBytes;
}

inline const void *SharedRecordPayload::fixedOwnerIdentity() const noexcept {
    return batch_->fixedOwnerIdentity;
}

inline std::size_t SharedRecordPayload::fixedOwnerBytes() const noexcept {
    return batch_->fixedOwnerBytes;
}

inline std::atomic<std::weak_ptr<void>> &SharedRecordPayload::queueAccounting() const noexcept {
    return batch_->queueAccounting;
}

inline std::shared_ptr<const SharedRecordPayload> SharedRecordPayload::adopt(std::unique_ptr<std::byte[]> backing,
                                                                             std::size_t symbolUnits,
                                                                             std::size_t fields, std::size_t bytes) {
    auto batch = std::allocate_shared<Batch>(Allocator<Batch>{}, std::move(backing), bytes);
    batch->single = {batch.get(), batch->backing.get(), symbolUnits, fields, bytes};

    return {batch, &batch->single};
}

inline std::shared_ptr<const SharedRecordPayload> SharedRecordPayload::row(const std::shared_ptr<Batch> &batch,
                                                                           std::size_t index) {
    return {batch, batch->rows + index};
}

inline std::shared_ptr<const SharedRecordPayload> SharedRecordPayload::row(const std::shared_ptr<const Batch> &batch,
                                                                           std::size_t index) {
    return {batch, batch->rows + index};
}

template <class Row>
std::shared_ptr<SharedRecordPayload::Batch> SharedRecordPayload::copyBatch(std::span<const Row> rows) {
    auto batch = std::allocate_shared<Batch>(Allocator<Batch>{}, batchBytes(rows) - controlBytes());
    fillBatch(*batch, rows);

    return batch;
}

template <class Rows> void SharedRecordPayload::fillBatch(Batch &storage, const Rows &rows) {
    static_assert(std::is_trivially_destructible_v<FieldValueView>);
    static_assert(std::is_trivially_destructible_v<SharedRecordPayload>);

    auto *batch = &storage;
    batch->rows = reinterpret_cast<SharedRecordPayload *>(batch->backing.get());
    auto offset = rows.size() * sizeof(SharedRecordPayload);

    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto &input = rows[index];
        offset = aligned(offset);
        auto *memory = batch->backing.get() + offset;
        auto *fields = reinterpret_cast<FieldValueView *>(memory);
        auto *text = reinterpret_cast<char16_t *>(memory + input.fields.size() * sizeof(FieldValueView));
        auto textUnits = input.symbol.size();

        for (const auto &field : input.fields)
            if (field.units)
                textUnits += field.units->size();

        auto *opaque = reinterpret_cast<std::byte *>(text + textUnits);
        text = std::copy(input.symbol.begin(), input.symbol.end(), text);

        for (std::size_t fieldIndex = 0; fieldIndex < input.fields.size(); ++fieldIndex) {
            const auto &source = input.fields[fieldIndex];
            auto &target = *std::construct_at(fields + fieldIndex, FieldValueView{source.type, source.raw, {}, {}});

            if (source.units) {
                target.units = std::u16string_view(text, source.units->size());

                if (!source.units->empty())
                    text = std::copy(source.units->begin(), source.units->end(), text);
            }

            if (source.bytes) {
                target.bytes = std::span<const std::byte>(opaque, source.bytes->size());

                if (!source.bytes->empty())
                    opaque = std::copy(source.bytes->begin(), source.bytes->end(), opaque);
            }
        }

        const auto bytes = rowBytes(input);
        std::construct_at(batch->rows + index, batch, memory, input.symbol.size(), input.fields.size(), bytes);
        offset += bytes;
    }
}

} // namespace org::ttldtor::qdcxx::core::detail
