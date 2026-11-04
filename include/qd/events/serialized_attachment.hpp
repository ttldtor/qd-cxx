// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/serial_field_type.hpp"
#include <any>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace org::ttldtor::qdcxx::events {
namespace detail {
/** Known standard borrowed result shapes cannot satisfy the owning codec contract.
 * @tparam T Candidate native result, after removing top-level cv qualification.
 */
template <class T> inline constexpr bool BORROWED_CODEC_VALUE = false;
/** Borrowed character sequences cannot own decoded storage.
 * @tparam T Character type.
 * @tparam Traits Character traits.
 */
template <class T, class Traits> inline constexpr bool BORROWED_CODEC_VALUE<std::basic_string_view<T, Traits>> = true;
/** Borrowed contiguous sequences cannot own decoded storage.
 * @tparam T Element type.
 * @tparam Extent Span extent.
 */
template <class T, std::size_t Extent> inline constexpr bool BORROWED_CODEC_VALUE<std::span<T, Extent>> = true;
/** A reference wrapper cannot own decoded storage.
 * @tparam T Referenced type.
 */
template <class T> inline constexpr bool BORROWED_CODEC_VALUE<std::reference_wrapper<T>> = true;
} // namespace detail
/** Local attachment/codec failures; no payload or application exception text escapes. */
enum class AttachmentError {
    INVALID_INPUT,          ///< Unsupported field identity, empty registration or moved-from registry.
    DUPLICATE_REGISTRATION, ///< A local key/format-version pair is already registered.
    UNKNOWN_CODEC,          ///< No registration has the explicitly requested local key.
    UNSUPPORTED_VERSION,    ///< The key exists but not at the requested format version.
    OUTPUT_TYPE_MISMATCH,   ///< Requested or returned native type differs from the registration.
    FIELD_TYPE_MISMATCH,    ///< Registration and attachment full field identities differ.
    MALFORMED_BODY,         ///< Codec rejects the format or does not consume the complete body.
    RESOURCE_LIMIT,         ///< Explicit count, byte or work bound was exhausted.
    ALLOCATION_FAILURE,     ///< Required owned storage could not be allocated.
    CODEC_EXCEPTION,        ///< Trusted callback threw an ordinary exception.
    CODEC_BUSY,             ///< Another invocation owns this registration; no wait, queue or retry is started.
    REENTRY                 ///< This thread reentered the same registration before its callback returned.
};

/** Exact owning nullable serialization; copying has independent byte storage. No decoding occurs here. */
class SerializedAttachment final {
    core::SerialFieldType kind_ = core::SerialFieldType::SERIAL_OBJECT;
    std::optional<std::vector<std::byte>> bytes_;

    public:
    /** Construct a null SERIAL_OBJECT value without allocation. */
    SerializedAttachment() = default;
    /** Copy the exact independent byte value; allocation failure leaves the source unchanged.
     * @param other Borrowed source; no views are retained.
     */
    SerializedAttachment(const SerializedAttachment &other) = default;
    /** Transfer ownership without allocating; moved-from values remain valid unspecified values.
     * @param other Source whose owned bytes are transferred.
     */
    SerializedAttachment(SerializedAttachment &&other) noexcept = default;
    /** Atomically replace this value; parameter copying completes before modification.
     * @param other Complete owning replacement, copied or moved by the caller.
     * @return This value with the replacement's identity and bytes.
     */
    SerializedAttachment &operator=(SerializedAttachment other) noexcept;
    /** Copy exact caller bytes only after identity/length admission.
     * @param kind Exact full BYTE_ARRAY, CUSTOM_OBJECT or SERIAL_OBJECT identity.
     * @param bytes Borrowed nullable body; null differs from an engaged zero-length span.
     * @param maximumBytes Finite body-byte limit, including zero.
     * @return Independent value or typed refusal; no source storage is retained.
     */
    [[nodiscard]] static std::expected<SerializedAttachment, AttachmentError>
    create(core::SerialFieldType kind, std::optional<std::span<const std::byte>> bytes, std::size_t maximumBytes);
    /** @return Exact retained full QD field identity. */
    [[nodiscard]] core::SerialFieldType wireKind() const noexcept;
    /** @return True only for absent body storage, independent of field identity. */
    [[nodiscard]] bool isNull() const noexcept;
    /** @return Nullable borrowed exact bytes, valid while this owner is alive and unchanged. */
    [[nodiscard]] std::optional<std::span<const std::byte>> bytes() const noexcept;
    /** Compare exact field identity, null engagement and every body byte.
     * @param other Independent value to compare.
     * @return Exact serialized-value equality, with no codec execution.
     */
    bool operator==(const SerializedAttachment &other) const = default;
};

/** Explicit synchronous decode allowances; zero is a real bound, with no ambient defaults. */
struct AttachmentDecodeLimits {
    std::size_t inputBytes;          ///< Complete body bytes admitted before callback execution.
    std::size_t outputBytes;         ///< Cumulative logical result storage requested through the codec context.
    std::size_t scratchBytes;        ///< Cumulative temporary storage requested through the codec context.
    std::size_t work;                ///< Codec-defined documented work units; no wall-clock guarantee.
    std::size_t diagnosticBytes = 0; ///< Owned diagnostic text allowance; fixed enum-only diagnostics charge zero.
};

/** Per-call monotonic charge context for trusted native codecs; never retained beyond the callback.
 * Codecs must charge before allocation/work. External allocations and non-returning code are not sandboxed.
 */
class AttachmentDecodeContext final {
    AttachmentDecodeLimits remaining_;
    bool exhausted_ = false;
    bool charge(std::size_t &remaining, std::size_t amount) noexcept;

    public:
    /** @param limits Independent finite per-call allowances, copied into this context. */
    explicit AttachmentDecodeContext(AttachmentDecodeLimits limits) noexcept;
    /** @param bytes Additional logical result bytes, including owned dynamic data and terminators.
     * @return Admission; a refusal permanently marks this call resource-limited.
     */
    [[nodiscard]] bool chargeOutput(std::size_t bytes) noexcept;
    /** @param bytes Additional logical temporary bytes; charges are not refunded on release.
     * @return Admission; a refusal permanently marks this call resource-limited.
     */
    [[nodiscard]] bool chargeScratch(std::size_t bytes) noexcept;
    /** @param units Additional codec-defined work units, charged before execution.
     * @return Admission; a refusal permanently marks this call resource-limited.
     */
    [[nodiscard]] bool chargeWork(std::size_t units) noexcept;
    /** @return Whether any charge in this call was refused. */
    [[nodiscard]] bool exhausted() const noexcept;
};

/** A successful codec candidate. Its native value must own all data independently of this call. */
struct AttachmentCodecOutput {
    std::any value;       ///< Owning copyable native value of the registration's exact output type.
    std::size_t consumed; ///< Complete body bytes consumed; a suffix is rejected by the registry.
};

/** One explicit application-owned local codec; keys/versions are never written to QD wire. */
struct AttachmentCodecRegistration {
    std::string key;                ///< Stable application-agreed nonempty local format key.
    std::uint32_t formatVersion;    ///< Explicit local format version, unrelated to event versions.
    core::SerialFieldType wireKind; ///< Exact full payload field identity accepted by this codec.
    std::type_index outputType;     ///< Exact typeid(T) for the independent owning native result.
    std::function<std::expected<AttachmentCodecOutput, AttachmentError>(std::span<const std::byte>,
                                                                        AttachmentDecodeContext &)>
        decode;                         ///< Synchronous trusted callback; registration owns its captures.
    std::size_t retainedStateBytes = 0; ///< Declared dynamic capture/state bytes, excluding this wrapper and key.
};

/** Finite immutable registration-snapshot bounds, excluding allocator bookkeeping and caller inputs. */
struct AttachmentCodecRegistryLimits {
    std::size_t registrations; ///< Maximum registration count, including zero.
    std::size_t keyBytes;      ///< Maximum bytes in a single local key.
    std::size_t storageBytes;  ///< Conservative wrapper/key/shared/declared-capture allocation-request bytes.
};

/** Immutable owning codec snapshot. Copies share registration locks/callback state without allocation.
 * Concurrent const decode is supported. Registration admission never waits: contention returns CODEC_BUSY.
 * Admitted callbacks are serialized; same-thread reentry fails before callback admission.
 * Callback input/context are borrowed only for the synchronous call. No background tasks or JVM are started.
 */
class AttachmentCodecRegistry final {
    struct State;
    std::shared_ptr<const State> state_;
    explicit AttachmentCodecRegistry(std::shared_ptr<const State> state) noexcept;
    [[nodiscard]] std::expected<std::optional<std::any>, AttachmentError>
    decodeValue(const SerializedAttachment &attachment, std::string_view key, std::uint32_t version,
                std::type_index outputType, AttachmentDecodeLimits limits) const;

    public:
    /** Atomically copy a complete registration list; callback captures must own their referenced state.
     * @param registrations Borrowed complete list; duplicate key/version pairs are refused.
     * @param limits Finite count, key and retained-state limits, including zero.
     * @return Immutable independent snapshot or typed refusal; no callback is invoked.
     */
    [[nodiscard]] static std::expected<AttachmentCodecRegistry, AttachmentError>
    create(std::span<const AttachmentCodecRegistration> registrations, AttachmentCodecRegistryLimits limits);

    /** Explicitly decode one attachment without changing its bytes or any feed/version state.
     * @tparam T Copyable owning native result; raw pointers, references and void are excluded.
     * @param attachment Borrowed immutable exact serialized value, unchanged on every outcome.
     * @param key Explicit local application format key; no automatic sniffing or fallback.
     * @param version Explicit codec format version, unrelated to Configuration version.
     * @param limits Independent finite body/output/scratch/work allowances.
     * @return Owning optional native value, absent for validated null input, or typed failure.
     * Ordinary callback/result-construction exceptions are contained. Diagnostics are fixed enum values only.
     */
    template <class T>
    [[nodiscard]] std::expected<std::optional<T>, AttachmentError> decode(const SerializedAttachment &attachment,
                                                                          std::string_view key, std::uint32_t version,
                                                                          AttachmentDecodeLimits limits) const {
        static_assert(std::is_copy_constructible_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> &&
                          !std::is_void_v<T> && !detail::BORROWED_CODEC_VALUE<std::remove_cv_t<T>>,
                      "Attachment codec results must be copyable owning values, not pointers, references or views");
        try {
            auto result = decodeValue(attachment, key, version, typeid(T), limits);

            if (!result) {
                return std::unexpected(result.error());
            }

            if (!*result) {
                return std::optional<T>{};
            }

            return std::optional<T>{std::any_cast<T>(std::move(**result))};
        } catch (const std::bad_alloc &) {
            return std::unexpected(AttachmentError::ALLOCATION_FAILURE);
        } catch (...) {
            return std::unexpected(AttachmentError::CODEC_EXCEPTION);
        }
    }
};
} // namespace org::ttldtor::qdcxx::events
