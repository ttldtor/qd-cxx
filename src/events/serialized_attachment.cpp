// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/serialized_attachment.hpp"

#include <atomic>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/** Count actual registry-owned allocation requests during construction, including rebound control blocks.
 * @tparam T Standard allocator element; registry storage is not over-aligned.
 * Allocation alone reads the construction-local counter; deallocation never dereferences it.
 */
template <class T> struct RegistryAllocator {
    using value_type = T;
    std::size_t *requested;
    std::size_t limit;

    RegistryAllocator(std::size_t &requested, std::size_t limit) noexcept : requested(&requested), limit(limit) {
    }

    template <class U>
    RegistryAllocator(const RegistryAllocator<U> &other) noexcept : requested(other.requested), limit(other.limit) {
    }

    T *allocate(std::size_t count) {
        static_assert(alignof(T) <= alignof(std::max_align_t));

        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw AttachmentError::RESOURCE_LIMIT;
        }

        const auto bytes = count * sizeof(T);

        if (*requested > limit || bytes > limit - *requested) {
            throw AttachmentError::RESOURCE_LIMIT;
        }

        auto *result = static_cast<T *>(::operator new(bytes));
        *requested += bytes;

        return result;
    }

    void deallocate(T *pointer, std::size_t count) noexcept {
        static_cast<void>(count);
        ::operator delete(pointer);
    }

    template <class U> bool operator==(const RegistryAllocator<U> &other) const noexcept {
        static_cast<void>(other);

        return true;
    }
};

bool payloadKind(core::SerialFieldType kind) noexcept {
    return kind == core::SerialFieldType::BYTE_ARRAY || kind == core::SerialFieldType::CUSTOM_OBJECT ||
           kind == core::SerialFieldType::SERIAL_OBJECT;
}

struct CodecEntry {
    std::basic_string<char, std::char_traits<char>, RegistryAllocator<char>> key;
    std::uint32_t formatVersion;
    core::SerialFieldType wireKind;
    std::type_index outputType;
    decltype(AttachmentCodecRegistration::decode) decode;
    mutable std::atomic_flag busy = ATOMIC_FLAG_INIT;

    CodecEntry(const AttachmentCodecRegistration &value, RegistryAllocator<char> allocator)
        : key(value.key.begin(), value.key.end(), allocator), formatVersion(value.formatVersion),
          wireKind(value.wireKind), outputType(value.outputType), decode(value.decode) {
    }
};

struct ActiveCodec {
    const CodecEntry *entry;
    ActiveCodec *previous;
};

thread_local ActiveCodec *activeCodec = nullptr;
constexpr std::byte EMPTY_BODY_MARKER{}; ///< Non-null callback address for engaged-empty input; never wire data.

class ActiveGuard {
    ActiveCodec frame_;

    public:
    explicit ActiveGuard(const CodecEntry *entry) : frame_{entry, activeCodec} {
        activeCodec = &frame_;
    }

    ~ActiveGuard() {
        activeCodec = frame_.previous;
        frame_.entry->busy.clear(std::memory_order_release);
    }
};
} // namespace

SerializedAttachment &SerializedAttachment::operator=(SerializedAttachment other) noexcept {
    std::swap(kind_, other.kind_);
    bytes_.swap(other.bytes_);

    return *this;
}

std::expected<SerializedAttachment, AttachmentError>
SerializedAttachment::create(core::SerialFieldType kind, std::optional<std::span<const std::byte>> bytes,
                             std::size_t maximumBytes) {
    if (!payloadKind(kind)) {
        return std::unexpected(AttachmentError::INVALID_INPUT);
    }

    if (bytes && bytes->size() > maximumBytes) {
        return std::unexpected(AttachmentError::RESOURCE_LIMIT);
    }

    try {
        SerializedAttachment result;
        result.kind_ = kind;

        if (bytes) {
            result.bytes_.emplace(bytes->begin(), bytes->end());
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(AttachmentError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(AttachmentError::ALLOCATION_FAILURE);
    }
}

core::SerialFieldType SerializedAttachment::wireKind() const noexcept {
    return kind_;
}

bool SerializedAttachment::isNull() const noexcept {
    return !bytes_;
}

std::optional<std::span<const std::byte>> SerializedAttachment::bytes() const noexcept {
    if (!bytes_) {
        return std::nullopt;
    }

    return std::span<const std::byte>(*bytes_);
}

AttachmentDecodeContext::AttachmentDecodeContext(AttachmentDecodeLimits limits) noexcept : remaining_(limits) {
}

bool AttachmentDecodeContext::charge(std::size_t &remaining, std::size_t amount) noexcept {
    if (amount > remaining) {
        remaining = 0;
        exhausted_ = true;

        return false;
    }

    remaining -= amount;

    return true;
}

bool AttachmentDecodeContext::chargeOutput(std::size_t bytes) noexcept {
    return charge(remaining_.outputBytes, bytes);
}

bool AttachmentDecodeContext::chargeScratch(std::size_t bytes) noexcept {
    return charge(remaining_.scratchBytes, bytes);
}

bool AttachmentDecodeContext::chargeWork(std::size_t units) noexcept {
    return charge(remaining_.work, units);
}

bool AttachmentDecodeContext::exhausted() const noexcept {
    return exhausted_;
}

struct AttachmentCodecRegistry::State {
    std::vector<std::shared_ptr<CodecEntry>, RegistryAllocator<std::shared_ptr<CodecEntry>>> entries;

    State(std::size_t &requested, std::size_t limit)
        : entries(RegistryAllocator<std::shared_ptr<CodecEntry>>{requested, limit}) {
    }
};

AttachmentCodecRegistry::AttachmentCodecRegistry(std::shared_ptr<const State> state) noexcept
    : state_(std::move(state)) {
}

std::expected<AttachmentCodecRegistry, AttachmentError>
AttachmentCodecRegistry::create(std::span<const AttachmentCodecRegistration> registrations,
                                AttachmentCodecRegistryLimits limits) {
    if (registrations.size() > limits.registrations) {
        return std::unexpected(AttachmentError::RESOURCE_LIMIT);
    }

    std::size_t storage = 0;
    const auto charge = [&](std::size_t bytes) {
        if (storage > limits.storageBytes || bytes > limits.storageBytes - storage) {
            return false;
        }

        storage += bytes;

        return true;
    };

    for (std::size_t i = 0; i < registrations.size(); ++i) {
        const auto &entry = registrations[i];

        if (entry.key.empty() || !payloadKind(entry.wireKind) || entry.outputType == typeid(void) || !entry.decode) {
            return std::unexpected(AttachmentError::INVALID_INPUT);
        }

        if (entry.key.size() > limits.keyBytes || !charge(entry.retainedStateBytes)) {
            return std::unexpected(AttachmentError::RESOURCE_LIMIT);
        }

        for (std::size_t j = 0; j < i; ++j) {
            if (registrations[j].key == entry.key && registrations[j].formatVersion == entry.formatVersion) {
                return std::unexpected(AttachmentError::DUPLICATE_REGISTRATION);
            }
        }
    }

    if (storage > limits.storageBytes) {
        return std::unexpected(AttachmentError::RESOURCE_LIMIT);
    }

    try {
        auto state = std::allocate_shared<State>(RegistryAllocator<State>{storage, limits.storageBytes}, storage,
                                                 limits.storageBytes);
        state->entries.reserve(registrations.size());

        for (const auto &entry : registrations) {
            state->entries.push_back(
                std::allocate_shared<CodecEntry>(RegistryAllocator<CodecEntry>{storage, limits.storageBytes}, entry,
                                                 RegistryAllocator<char>{storage, limits.storageBytes}));
        }

        return AttachmentCodecRegistry{std::move(state)};
    } catch (AttachmentError error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(AttachmentError::ALLOCATION_FAILURE);
    } catch (...) {
        return std::unexpected(AttachmentError::CODEC_EXCEPTION);
    }
}

std::expected<std::optional<std::any>, AttachmentError>
AttachmentCodecRegistry::decodeValue(const SerializedAttachment &attachment, std::string_view key,
                                     std::uint32_t version, std::type_index outputType,
                                     AttachmentDecodeLimits limits) const {
    const auto state = state_;

    if (!state) {
        return std::unexpected(AttachmentError::INVALID_INPUT);
    }

    bool knownKey = false;

    for (const auto &entry : state->entries) {
        const auto &registration = *entry;

        if (std::string_view(registration.key) != key) {
            continue;
        }

        knownKey = true;

        if (registration.formatVersion != version) {
            continue;
        }

        if (registration.outputType != outputType) {
            return std::unexpected(AttachmentError::OUTPUT_TYPE_MISMATCH);
        }

        if (registration.wireKind != attachment.wireKind()) {
            return std::unexpected(AttachmentError::FIELD_TYPE_MISMATCH);
        }

        if (attachment.isNull()) {
            return std::optional<std::any>{};
        }

        const auto bytes = *attachment.bytes();

        if (bytes.size() > limits.inputBytes) {
            return std::unexpected(AttachmentError::RESOURCE_LIMIT);
        }

        for (auto *active = activeCodec; active; active = active->previous) {
            if (active->entry == entry.get()) {
                return std::unexpected(AttachmentError::REENTRY);
            }
        }

        try {
            // Nonblocking admission prevents A->B / B->A callback lock cycles across threads.
            if (entry->busy.test_and_set(std::memory_order_acquire)) {
                return std::unexpected(AttachmentError::CODEC_BUSY);
            }

            ActiveGuard guard(entry.get());
            AttachmentDecodeContext context(limits);
            const auto input = bytes.empty() ? std::span<const std::byte>(&EMPTY_BODY_MARKER, 0) : bytes;
            auto result = registration.decode(input, context);

            if (context.exhausted()) {
                return std::unexpected(AttachmentError::RESOURCE_LIMIT);
            }

            if (!result) {
                return std::unexpected(result.error());
            }

            if (result->consumed != bytes.size()) {
                return std::unexpected(AttachmentError::MALFORMED_BODY);
            }

            if (result->value.type() != outputType) {
                return std::unexpected(AttachmentError::OUTPUT_TYPE_MISMATCH);
            }

            return std::optional<std::any>{std::move(result->value)};
        } catch (const std::bad_alloc &) {
            return std::unexpected(AttachmentError::ALLOCATION_FAILURE);
        } catch (...) {
            return std::unexpected(AttachmentError::CODEC_EXCEPTION);
        }
    }

    return std::unexpected(knownKey ? AttachmentError::UNSUPPORTED_VERSION : AttachmentError::UNKNOWN_CODEC);
}
} // namespace org::ttldtor::qdcxx::events
