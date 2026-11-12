// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/protocol/live_session.hpp"
#include "tape_event_bridge.hpp"

#include "field_time_binding.hpp"
#include "live_decode.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace org::ttldtor::qdcxx::protocol {
namespace {

/** Configured backing exhaustion, distinct from an allocator failure. */
struct LiveStorageLimit {};

/** Failing arena upstream which preserves configured-exhaustion classification. */
class LiveExhaustedResource final : public std::pmr::memory_resource {
    /** Refuse every upstream request without acquiring storage.
     * @param bytes Requested upstream bytes.
     * @param alignment Requested alignment.
     * @return No value; throws LiveStorageLimit.
     */
    void *do_allocate(std::size_t bytes, std::size_t alignment) override {
        static_cast<void>(bytes);
        static_cast<void>(alignment);
        throw LiveStorageLimit{};
    }

    /** No successful allocation exists to release.
     * @param pointer Unused pointer.
     * @param bytes Unused size.
     * @param alignment Unused alignment.
     */
    void do_deallocate(void *pointer, std::size_t bytes, std::size_t alignment) override {
        static_cast<void>(pointer);
        static_cast<void>(bytes);
        static_cast<void>(alignment);
    }

    /** Compare resource identity.
     * @param other Resource to compare.
     * @return True only for this resource instance.
     */
    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};

/** Single serialized domain's requested backing ledger; allocation overhead is excluded. */
class LiveStorage {
    std::size_t references_ = 1; ///< Session and independent owners retain this epoch identity.
    std::size_t resident_;       ///< Currently reserved requested backing and object bytes.
    std::size_t maximum_;        ///< Maximum simultaneous requested backing bytes.
    std::size_t peak_;           ///< Highest successfully reserved simultaneous byte count.
    std::size_t base_;           ///< Fixed conservative configuration reservation held by every epoch lease.
    std::size_t owners_ = 0;     ///< Distinct snapshot, event and pending-output owners.
    std::size_t maximumOwners_;  ///< Finite simultaneous owner count, including candidates.

    public:
    /** Initialize an epoch after its own fixed allocation has been admitted.
     * @param maximum Maximum simultaneous backing bytes, including this ledger.
     * @param base Fixed configuration backing reservation, excluding this ledger.
     * @param owners Maximum simultaneous candidate or published owners.
     */
    LiveStorage(std::size_t maximum, std::size_t base, std::size_t owners) noexcept
        : resident_(sizeof(LiveStorage) + base), maximum_(maximum), peak_(resident_), base_(base),
          maximumOwners_(owners) {
        assert(maximum >= resident_);
    }

    /** Retain this opaque epoch; all related accesses and releases must be serialized. */
    void retain() noexcept {
        assert(references_ != std::numeric_limits<std::size_t>::max());
        ++references_;
    }

    /** Release one owner, deleting this ledger after the last owner disappears. */
    void release() noexcept {
        assert(references_ != 0);

        if (--references_ == 0) {
            assert(resident_ == sizeof(LiveStorage) + base_ && owners_ == 0);
            delete this;
        }
    }

    /** Reserve one distinct candidate or published owner before allocating its object.
     * @return False without changing state when the finite owner limit is exhausted.
     */
    [[nodiscard]] bool reserveOwner() noexcept {
        if (owners_ == maximumOwners_) {
            return false;
        }

        ++owners_;

        return true;
    }

    /** Release a destroyed owner after its separately counted backing has been released. */
    void relinquishOwner() noexcept {
        assert(owners_ != 0);
        --owners_;
    }

    /** Reserve requested bytes before allocation, including old and candidate backing.
     * @param bytes Object or array allocation request size, without allocator bookkeeping.
     * @return True after admission; false leaves the ledger unchanged.
     */
    [[nodiscard]] bool reserve(std::size_t bytes) noexcept {
        if (bytes > maximum_ - resident_) {
            return false;
        }

        resident_ += bytes;

        if (resident_ > peak_) {
            peak_ = resident_;
        }

        return true;
    }

    /** Release destroyed or unsuccessfully allocated backing without refunding caller work.
     * @param bytes Previously reserved allocation request size.
     */
    void relinquish(std::size_t bytes) noexcept {
        assert(bytes <= resident_ - sizeof(LiveStorage));
        resident_ -= bytes;
    }

    /** Inspect requested bytes currently held by this epoch's owners.
     * @return Live requested byte count, including fixed metadata.
     */
    [[nodiscard]] std::size_t resident() const noexcept {
        return resident_;
    }

    /** Inspect the highest admitted simultaneous ownership.
     * @return Requested byte peak since epoch creation, never a process RSS measurement.
     */
    [[nodiscard]] std::size_t peak() const noexcept {
        return peak_;
    }
};

/** Intrusive serialized epoch lease without an additional allocator control block. */
class LiveStorageLease {
    LiveStorage *storage_ = nullptr; ///< Null only for an empty or moved-from lease.

    public:
    /** Retain one existing ledger.
     * @param storage Existing ledger; null creates an empty lease.
     */
    explicit LiveStorageLease(LiveStorage *storage = nullptr) noexcept : storage_(storage) {
        if (storage_) {
            storage_->retain();
        }
    }

    /** Retain the same ledger independently.
     * @param other Source lease; no backing allocation occurs.
     */
    LiveStorageLease(const LiveStorageLease &other) noexcept : LiveStorageLease(other.storage_) {
    }

    /** Transfer one existing reference without changing its count.
     * @param other Source, made empty.
     */
    LiveStorageLease(LiveStorageLease &&other) noexcept : storage_(other.storage_) {
        other.storage_ = nullptr;
    }

    /** Release an existing reference and retain the source.
     * @param other Lease to copy; self-assignment preserves ownership.
     * @return This lease.
     */
    LiveStorageLease &operator=(const LiveStorageLease &other) noexcept {
        if (this != &other) {
            LiveStorageLease next(other);
            swap(next);
        }

        return *this;
    }

    /** Release an existing reference and transfer the source.
     * @param other Lease to transfer; self-move preserves ownership.
     * @return This lease.
     */
    LiveStorageLease &operator=(LiveStorageLease &&other) noexcept {
        if (this != &other) {
            LiveStorageLease next(static_cast<LiveStorageLease &&>(other));
            swap(next);
        }

        return *this;
    }

    /** Release this lease after its separately accounted backing has been destroyed. */
    ~LiveStorageLease() {
        if (storage_) {
            storage_->release();
        }
    }

    /** Exchange identities without allocation.
     * @param other Lease to exchange.
     */
    void swap(LiveStorageLease &other) noexcept {
        auto *previous = storage_;
        storage_ = other.storage_;
        other.storage_ = previous;
    }

    /** Inspect this retained ledger.
     * @return Ledger pointer or null; lifetime is protected by this lease.
     */
    [[nodiscard]] LiveStorage *get() const noexcept {
        return storage_;
    }
};

/** Fixed requested arena backing; container reallocation never acquires upstream storage. */
class LiveArena {
    LiveStorageLease lease_;               ///< Outlives both owned backing and the resource using it.
    std::size_t bytes_;                    ///< Entire requested arena backing, including unused and abandoned slots.
    std::unique_ptr<std::byte[]> backing_; ///< Exactly bytes_ elements, admitted before construction.
    LiveExhaustedResource exhausted_;      ///< Rejects fallback allocation with a resource-limit classification.
    std::pmr::monotonic_buffer_resource resource_; ///< Fixed backing with a failing upstream resource.

    /** Allocate a complete admitted block, rolling back storage admission on allocation failure.
     * @param storage Shared serialized epoch ledger.
     * @param bytes Exact byte-array request.
     * @return Owned allocation; configured exhaustion and allocation failure remain distinguishable.
     */
    static std::unique_ptr<std::byte[]> allocate(LiveStorage &storage, std::size_t bytes) {
        if (!storage.reserve(bytes)) {
            throw LiveStorageLimit{};
        }

        try {
            return std::unique_ptr<std::byte[]>(new std::byte[bytes]);
        } catch (...) {
            storage.relinquish(bytes);
            throw;
        }
    }

    public:
    /** Reserve one fixed arena while old owners remain accounted.
     * @param storage Epoch ledger, retained by this arena.
     * @param bytes Full requested arena capacity; zero is finite.
     */
    LiveArena(LiveStorage &storage, std::size_t bytes)
        : lease_(&storage), bytes_(bytes), backing_(allocate(storage, bytes)),
          resource_(backing_.get(), bytes, &exhausted_) {
    }

    /** Release the arena after every container using resource() has been destroyed. */
    ~LiveArena() {
        resource_.release();
        backing_.reset();
        lease_.get()->relinquish(bytes_);
    }

    /** Borrow the arena's allocator; its objects must die before this arena.
     * @return Fixed-capacity resource with no external allocation fallback.
     */
    [[nodiscard]] std::pmr::memory_resource *resource() noexcept {
        return &resource_;
    }

    /** Inspect the complete requested block, including spare and abandoned monotonic slots.
     * @return Fixed arena capacity admitted against the epoch ledger.
     */
    [[nodiscard]] std::size_t capacityBytes() const noexcept {
        return bytes_;
    }
};

using Error = LiveError;
using Storage = LiveStorage;
constexpr std::size_t PREFIX_BYTES = 9;
constexpr std::int32_t HEARTBEAT = 0, DESCRIBE_PROTOCOL = 1, DESCRIBE_RECORDS = 2;
constexpr std::int32_t TICKER_DATA = 10, STREAM_DATA = 15, HISTORY_DATA = 20;
constexpr std::uint32_t TIME_BIT = 1, MARK_BIT = 2, DELTA_BIT = 4, LAG_BIT = 8;
constexpr std::uint32_t HEARTBEAT_MASK = TIME_BIT | MARK_BIT | DELTA_BIT | LAG_BIT;
constexpr std::size_t MAX_WIRE_COUNT = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::array<std::byte, 4> MAGIC{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};

/** Debit explicit turn work before executing it.
 * @param amount Requested units.
 * @param remaining Mutable remaining allowance; previous charges are never refunded.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        throw Error::WORK_LIMIT;
    }

    remaining -= amount;
}

/** Add finite accounting values without wrapping.
 * @param left First term.
 * @param right Second term.
 * @return Checked sum; RESOURCE_LIMIT on overflow.
 */
std::size_t sum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw Error::RESOURCE_LIMIT;
    }

    return left + right;
}

/** Multiply finite accounting values without wrapping.
 * @param left First factor.
 * @param right Second factor.
 * @return Checked product; RESOURCE_LIMIT on overflow.
 */
std::size_t product(std::size_t left, std::size_t right) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) {
        throw Error::RESOURCE_LIMIT;
    }

    return left * right;
}

/** Charge a complete exact-text comparison.
 * @param left First exact borrowed text.
 * @param right Second exact borrowed text.
 * @param work Mutable caller-turn work.
 * @return Whether all UTF-16 code units match.
 */
bool same(std::u16string_view left, std::u16string_view right, LiveWork &work) {
    debit(1, work.visits);
    debit(sum(left.size(), right.size()), work.textUnits);

    return left == right;
}

/** Object metadata destroyed after all later arena/container members. */
struct OwnerLedger {
    LiveStorageLease lease; ///< Retains the ledger through every later member's destruction.
    std::size_t bytes;      ///< Already admitted exact object-allocation request.

    /** Adopt the factory's pre-admitted object allocation.
     * @param storage Serialized epoch ledger.
     * @param count Already reserved object bytes.
     */
    OwnerLedger(Storage &storage, std::size_t count) noexcept : lease(&storage), bytes(count) {
    }

    /** Release object and owner credits after all payload members have been destroyed. */
    ~OwnerLedger() {
        lease.get()->relinquish(bytes);
        lease.get()->relinquishOwner();
    }
};

/** Allocate one counted owner before invoking its constructor.
 * @tparam T Owner whose first member adopts an OwnerLedger without throwing.
 * @tparam Args Remaining constructor argument types.
 * @param storage Serialized epoch ledger.
 * @param args Constructor arguments.
 * @return Complete owner; failure preserves previous owners.
 */
template <typename T, typename... Args> std::unique_ptr<T> makeOwner(Storage &storage, Args &&...args) {
    static_assert(alignof(T) <= alignof(std::max_align_t));

    if (!storage.reserveOwner()) {
        throw LiveStorageLimit{};
    }

    if (!storage.reserve(sizeof(T))) {
        storage.relinquishOwner();
        throw LiveStorageLimit{};
    }

    void *backing = nullptr;
    try {
        backing = ::operator new(sizeof(T));
    } catch (...) {
        storage.relinquish(sizeof(T));
        storage.relinquishOwner();
        throw;
    }
    try {
        return std::unique_ptr<T>(new (backing) T(storage, std::forward<Args>(args)...));
    } catch (...) {
        // The first noexcept ledger member releases object admission during unwinding.
        ::operator delete(backing);
        throw;
    }
}

/** Scoped pre-admitted temporary backing, without a distinct retained-owner slot. */
struct TemporaryReservation {
    Storage &storage;  ///< Ledger retained by the active session operation.
    std::size_t bytes; ///< Temporary allocation allowance.

    /** Reserve temporary backing while old owners remain charged.
     * @param ledger Serialized epoch ledger.
     * @param count Complete allocation allowance.
     */
    TemporaryReservation(Storage &ledger, std::size_t count) : storage(ledger), bytes(count) {
        if (!storage.reserve(count)) {
            throw LiveStorageLimit{};
        }
    }

    /** Release temporary ownership without refunding turn work. */
    ~TemporaryReservation() {
        storage.relinquish(bytes);
    }
};

/** Exact nullable property owned exclusively by an explicit arena. */
struct Property {
    std::optional<std::pmr::u16string> key;   ///< Exact nullable key.
    std::optional<std::pmr::u16string> value; ///< Exact nullable value.
};

/** Directional message with owned local property overrides. */
struct Message {
    std::int32_t id;                         ///< Numeric wire identity.
    std::optional<std::pmr::u16string> name; ///< Exact nullable wire name.
    std::pmr::vector<Property> properties;   ///< Local overrides, merged by exact nullable key.

    /** Adopt identity and the explicit property allocator.
     * @param value Numeric identity.
     * @param text Exact optional name.
     * @param resource Explicit arena resource.
     */
    Message(std::int32_t value, std::optional<std::pmr::u16string> text, std::pmr::memory_resource &resource)
        : id(value), name(std::move(text)), properties(&resource) {
    }
};

/** Complete or delta peer metadata, with no endpoint/socket state. */
struct Protocol {
    std::pmr::vector<Property> properties; ///< Top-level exact properties.
    std::pmr::vector<Message> send;        ///< Peer send declarations.
    std::pmr::vector<Message> receive;     ///< Peer receive declarations.
    bool received = false;                 ///< A complete valid descriptor has been admitted.

    /** Select the sole allocator for every container.
     * @param resource Explicit arena resource.
     */
    explicit Protocol(std::pmr::memory_resource &resource)
        : properties(&resource), send(&resource), receive(&resource) {
    }
};

/** Borrow exact nullable text without allocating.
 * @param text Owned optional text.
 * @return Exact nullable immutable view.
 */
std::optional<std::u16string_view> view(const std::optional<std::pmr::u16string> &text) noexcept {
    return text ? std::optional<std::u16string_view>{*text} : std::nullopt;
}

/** Clone exact nullable text into an explicitly owned arena.
 * @param text Borrowed nullable text.
 * @param resource Candidate arena resource.
 * @param work Mutable caller-turn credits.
 * @return Independent exact nullable text.
 */
std::optional<std::pmr::u16string> clone(std::optional<std::u16string_view> text, std::pmr::memory_resource &resource,
                                         LiveWork &work) {
    if (!text) {
        return {};
    }

    debit(text->size(), work.textUnits);

    return std::pmr::u16string(*text, &resource);
}

/** Compare nullable property keys through charged exact code units.
 * @param left First nullable key.
 * @param right Second nullable key.
 * @param work Mutable caller-turn credits.
 * @return Whether nullability and all code units match.
 */
bool sameKey(std::optional<std::u16string_view> left, std::optional<std::u16string_view> right, LiveWork &work) {
    debit(1, work.visits);

    return left.has_value() == right.has_value() && (!left || same(*left, *right, work));
}

/** Find effective property text without changing its nullable representation.
 * @param properties Immutable property list.
 * @param key Exact non-null lookup key.
 * @param work Mutable turn comparison credits.
 * @return Pointer to its nullable value, or null when absent.
 */
const std::optional<std::pmr::u16string> *property(const std::pmr::vector<Property> &properties,
                                                   std::u16string_view key, LiveWork &work) {
    for (const auto &entry : properties) {
        debit(1, work.visits);

        if (entry.key && same(*entry.key, key, work)) {
            return &entry.value;
        }
    }

    return nullptr;
}

/** Recognize exact known protocol message names.
 * @param id Numeric identity.
 * @return Exact known name, or empty for an unknown identity.
 */
std::u16string_view messageName(std::int32_t id) noexcept {
    switch (id) {
    case 0:
        return u"HEARTBEAT";
    case 1:
        return u"DESCRIBE_PROTOCOL";
    case 2:
        return u"DESCRIBE_RECORDS";
    case 4:
        return u"PART";
    case 5:
        return u"RAW_DATA";
    case 10:
        return u"TICKER_DATA";
    case 11:
        return u"TICKER_ADD_SUBSCRIPTION";
    case 12:
        return u"TICKER_REMOVE_SUBSCRIPTION";
    case 15:
        return u"STREAM_DATA";
    case 16:
        return u"STREAM_ADD_SUBSCRIPTION";
    case 17:
        return u"STREAM_REMOVE_SUBSCRIPTION";
    case 20:
        return u"HISTORY_DATA";
    case 21:
        return u"HISTORY_ADD_SUBSCRIPTION";
    case 22:
        return u"HISTORY_REMOVE_SUBSCRIPTION";
    default:
        return {};
    }
}

/** Resolve Java's name-only sentinel.
 * @param name Nullable exact peer name.
 * @return Known numeric identity or minus one.
 */
std::int32_t messageId(std::optional<std::u16string_view> name) noexcept {
    if (name) {
        for (const auto id : {0, 1, 2, 4, 5, 10, 11, 12, 15, 16, 17, 20, 21, 22}) {
            if (messageName(id) == *name) {
                return id;
            }
        }
    }

    return -1;
}

/** Match capability by both numeric identity and exact known name.
 * @param messages Effective directional declarations.
 * @param id Required identity.
 * @param work Mutable turn comparison credits.
 * @return True only for a matching known declaration.
 */
bool capability(const std::pmr::vector<Message> &messages, std::int32_t id, LiveWork &work) {
    for (const auto &message : messages) {
        debit(1, work.visits);

        if (message.id == id) {
            return message.name && same(*message.name, messageName(id), work);
        }
    }

    return false;
}

/** Recognize an exact hs comma token; unknown options remain informational.
 * @param options Exact nullable effective option value.
 * @return Whether an hs token occurs without normalization.
 */
bool hasHistorySnapshot(const std::optional<std::pmr::u16string> *options) noexcept {
    if (!options || !*options) {
        return false;
    }

    const std::u16string_view text(**options);
    std::size_t start = 0;

    while (start <= text.size()) {
        const auto end = text.find(u',', start);

        if (text.substr(start, end == text.npos ? text.size() - start : end - start) == u"hs") {
            return true;
        }

        if (end == text.npos) {
            break;
        }

        start = end + 1;
    }

    return false;
}

/** Merge exact properties by key; authorization secrets never enter published state.
 * @param target Candidate-owned properties.
 * @param incoming Complete borrowed delta.
 * @param resource Candidate arena.
 * @param work Mutable turn credits.
 */
void mergeProperties(std::pmr::vector<Property> &target, const std::pmr::vector<Property> &incoming,
                     std::pmr::memory_resource &resource, LiveWork &work) {
    debit(target.size(), work.visits);
    target.reserve(sum(target.size(), incoming.size()));

    for (const auto &entry : incoming) {
        debit(1, work.visits);

        if (entry.key && same(*entry.key, u"authorization", work)) {
            continue;
        }

        Property *destination = nullptr;

        for (auto &old : target) {
            if (sameKey(view(old.key), view(entry.key), work)) {
                destination = &old;
                break;
            }
        }

        auto value = clone(view(entry.value), resource, work);

        if (destination) {
            destination->value.reset();

            if (value) {
                destination->value.emplace(std::move(*value));
            }
        } else {
            target.push_back({clone(view(entry.key), resource, work), std::move(value)});
        }
    }
}

/** Merge directional declarations by ID, retaining the first name.
 * @param target Candidate-owned declarations.
 * @param incoming Complete borrowed delta.
 * @param resource Candidate arena.
 * @param work Mutable turn credits.
 */
void mergeMessages(std::pmr::vector<Message> &target, const std::pmr::vector<Message> &incoming,
                   std::pmr::memory_resource &resource, LiveWork &work) {
    debit(target.size(), work.visits);
    target.reserve(sum(target.size(), incoming.size()));

    for (const auto &entry : incoming) {
        Message *destination = nullptr;

        for (auto &old : target) {
            debit(1, work.visits);

            if (old.id == entry.id) {
                destination = &old;
                break;
            }
        }

        if (!destination) {
            target.emplace_back(entry.id, clone(view(entry.name), resource, work), resource);
            destination = &target.back();
        }

        mergeProperties(destination->properties, entry.properties, resource, work);
    }
}

/** Bounded DXP3 reader using the explicit scratch arena and shared primitives. */
class ProtocolParser {
    std::span<const std::byte> payload_;  ///< Borrowed complete payload.
    TapeDescriptorLimits limits_;         ///< Finite aggregate descriptor bounds.
    LiveWork &work_;                      ///< Mutable turn work.
    std::pmr::memory_resource &resource_; ///< Fixed scratch allocator.
    LiveDiagnostic &diagnostic_;          ///< Static/numeric checkpoints.
    std::size_t cursor_ = 0;              ///< Committed examination position.
    std::size_t units_ = 0;               ///< Successfully decoded text units.
    std::size_t properties_ = 0;          ///< Charged property pairs across all lists.
    std::size_t messages_ = 0;            ///< Charged directional messages.

    /** Read a shared compact primitive and commit exact examination.
     * @param wide Whether to retain compact-long rather than projected compact-int width.
     * @return Signed value; malformed input throws a static category.
     */
    std::int64_t number(bool wide) {
        diagnostic_.consumed = cursor_;
        const auto bytes = payload_.subspan(cursor_);

        if (wide) {
            const auto result = encoding::readCompactLong(bytes);
            cursor_ += result.consumed;
            diagnostic_.consumed = cursor_;

            if (result.status != encoding::Status::OK) {
                throw Error::MALFORMED;
            }

            return result.value;
        }

        const auto result = encoding::readCompactInt(bytes);
        cursor_ += result.consumed;
        diagnostic_.consumed = cursor_;

        if (result.status != encoding::Status::OK) {
            throw Error::MALFORMED;
        }

        return result.value;
    }

    /** Admit a finite count before allocating or iterating.
     * @param wide Whether the wire count uses compact-long.
     * @param used Already admitted aggregate entries.
     * @param maximum Aggregate finite bound.
     * @return Bounded nonnegative count, charged before subsequent work.
     */
    std::size_t count(bool wide, std::size_t &used, std::size_t maximum) {
        const auto value = number(wide);

        if (value < 0 || value > static_cast<std::int64_t>(MAX_WIRE_COUNT)) {
            throw Error::MALFORMED;
        }

        const auto result = static_cast<std::size_t>(value);

        if (result > maximum - used) {
            throw Error::RESOURCE_LIMIT;
        }

        used += result;
        debit(result, work_.visits);

        return result;
    }

    /** Read nullable text through the shared allocator-aware UTFString primitive.
     * @return Exact owning scratch text; primitive failures retain charged work.
     */
    std::optional<std::pmr::u16string> text() {
        diagnostic_.consumed = cursor_;
        const auto allowance = std::min(limits_.text.codeUnits, limits_.textUnits - units_);
        debit(std::min(allowance, payload_.size() - cursor_), work_.textUnits);
        auto decoded =
            encoding::readUtfString(payload_.subspan(cursor_), {limits_.text.bodyBytes, allowance}, resource_);
        cursor_ += decoded.consumed;
        diagnostic_.consumed = cursor_;

        if (decoded.status != encoding::Status::OK) {
            throw decoded.status == encoding::Status::RESOURCE_LIMIT ? Error::RESOURCE_LIMIT : Error::MALFORMED;
        }

        units_ += decoded.value ? decoded.value->size() : 0;

        return std::move(decoded.value);
    }

    /** Parse complete local property pairs into scratch-owned storage.
     * @param target Selected property list.
     */
    void properties(std::pmr::vector<Property> &target) {
        const auto size = count(true, properties_, limits_.properties);
        target.reserve(size);

        for (std::size_t index = 0; index < size; ++index) {
            auto key = text();
            auto value = text();
            target.push_back({std::move(key), std::move(value)});
        }
    }

    /** Parse one directional list without deriving capability from ID alone.
     * @param target Selected scratch-owned list.
     */
    void messages(std::pmr::vector<Message> &target) {
        const auto size = count(false, messages_, limits_.messages);
        target.reserve(size);

        for (std::size_t index = 0; index < size; ++index) {
            const auto rawId = static_cast<std::int32_t>(number(false));
            auto name = text();

            if (rawId == -1) {
                constexpr std::size_t KNOWN_IDENTITIES = 14;
                debit(KNOWN_IDENTITIES, work_.visits);
                debit(product(name ? name->size() : 0, KNOWN_IDENTITIES), work_.textUnits);
            }

            const auto id = rawId == -1 ? messageId(view(name)) : rawId;
            target.emplace_back(id, std::move(name), resource_);
            properties(target.back().properties);
        }
    }

    /** Consume one standard optional peer EndpointId without publishing routing identity.
     * The null JVMId read context uses ordinary nullable byte-array framing, not reference IDs.
     * UID bytes stay inside the bounded frame; nullable name uses the existing scratch/text limits.
     */
    void endpoint() {
        diagnostic_.stage = "descriptor-endpoint";
        constexpr std::size_t ENDPOINT_PRIMITIVE_VISITS = 3;
        debit(ENDPOINT_PRIMITIVE_VISITS, work_.visits);
        const auto length = number(true);

        if (length < -1 || length > static_cast<std::int64_t>(MAX_WIRE_COUNT)) {
            throw Error::MALFORMED;
        }

        if (length == -1) {
            return;
        }

        const auto bytes = static_cast<std::size_t>(length);

        if (bytes > payload_.size() - cursor_) {
            throw Error::MALFORMED;
        }

        debit(bytes, work_.visits);
        cursor_ += bytes;
        diagnostic_.consumed = cursor_;
        static_cast<void>(text());
        static_cast<void>(number(true));
    }

    public:
    /** Bind complete input and explicitly owned scratch allocation.
     * @param payload Complete DXP3 body, excluding frame/type.
     * @param limits Finite descriptor bounds.
     * @param work Mutable turn credits.
     * @param resource Fixed scratch allocator.
     * @param diagnostic Static/numeric parsing checkpoints.
     */
    ProtocolParser(std::span<const std::byte> payload, TapeDescriptorLimits limits, LiveWork &work,
                   std::pmr::memory_resource &resource, LiveDiagnostic &diagnostic)
        : payload_(payload), limits_(limits), work_(work), resource_(resource), diagnostic_(diagnostic) {
    }

    /** Parse one whole delta and at most one standard optional EndpointId.
     * @return Complete scratch-owned delta, never partial published state.
     */
    Protocol parse() {
        diagnostic_.stage = "protocol";
        debit(payload_.size(), work_.bytes);

        for (const auto byte : MAGIC) {
            if (cursor_ == payload_.size() || payload_[cursor_++] != byte) {
                throw Error::MALFORMED;
            }
        }

        Protocol result(resource_);
        properties(result.properties);
        messages(result.send);
        messages(result.receive);

        if (cursor_ != payload_.size()) {
            endpoint();
            diagnostic_.stage = "protocol";
        }

        if (cursor_ != payload_.size()) {
            diagnostic_.stage = "descriptor-extension";
            diagnostic_.consumed = cursor_;
            throw Error::UNSUPPORTED;
        }

        result.received = true;

        return result;
    }
};

/** Validate explicitly excluded live timing/type semantics.
 * @param protocol Whole effective candidate; opaque informational properties remain accepted.
 * @param work Mutable turn comparison credits.
 */
void validatePeer(const Protocol &protocol, LiveWork &work) {
    const auto *time = property(protocol.properties, u"time", work);

    if (time && *time && !(**time).empty() && !same(**time, u"NONE", work) && !same(**time, u"none", work)) {
        throw Error::UNSUPPORTED;
    }

    const auto *type = property(protocol.properties, u"type", work);

    if (type && *type && !(**type).empty() && !same(**type, u"qtp", work)) {
        throw Error::UNSUPPORTED;
    }
}

/** Map a description failure without publishing its completed prefix.
 * @param status Ordinary parser outcome.
 * @return Static live category.
 */
Error descriptionError(DescriptionStatus status) noexcept {
    if (status == DescriptionStatus::RESOURCE_LIMIT) {
        return Error::RESOURCE_LIMIT;
    }

    if (status == DescriptionStatus::ALLOCATION_FAILURE) {
        return Error::ALLOCATION_FAILURE;
    }

    return Error::MALFORMED;
}

/** Map a binding failure without replacing old state.
 * @param error Ordinary exact-type planner failure.
 * @return Static live category.
 */
Error bindingError(BindingError error) noexcept {
    if (error == BindingError::RESOURCE_LIMIT) {
        return Error::RESOURCE_LIMIT;
    }

    if (error == BindingError::ALLOCATION_FAILURE) {
        return Error::ALLOCATION_FAILURE;
    }

    if (error == BindingError::UNSUPPORTED_PROFILE) {
        return Error::UNSUPPORTED;
    }

    return Error::MALFORMED;
}

/** Map a complete-data failure without exposing its completed prefix.
 * @param status Ordinary decoder outcome.
 * @return Static live category.
 */
Error dataError(StreamDataStatus status) noexcept {
    if (status == StreamDataStatus::RESOURCE_LIMIT) {
        return Error::RESOURCE_LIMIT;
    }

    if (status == StreamDataStatus::ALLOCATION_FAILURE) {
        return Error::ALLOCATION_FAILURE;
    }

    if (status == StreamDataStatus::UNSUPPORTED_PROFILE) {
        return Error::UNSUPPORTED;
    }

    return Error::MALFORMED;
}

/** Map a composer rejection without poisoning the session.
 * @param error Existing composer failure.
 * @return Static live category.
 */
Error writerError(SubscriptionWriterError error) noexcept {
    if (error == SubscriptionWriterError::RESOURCE_LIMIT) {
        return Error::RESOURCE_LIMIT;
    }

    if (error == SubscriptionWriterError::WORK_LIMIT) {
        return Error::WORK_LIMIT;
    }

    if (error == SubscriptionWriterError::ALLOCATION_FAILURE) {
        return Error::ALLOCATION_FAILURE;
    }

    if (error == SubscriptionWriterError::INVALID_STATE) {
        return Error::INVALID_STATE;
    }

    return Error::INVALID_CONFIGURATION;
}

/** Reserve bounded parse work before shared description loops execute.
 * @param limits Finite per-batch allowances.
 * @param payload Complete payload bytes.
 * @param work Mutable turn credits.
 */
void chargeDescriptions(DescriptionLimits limits, std::size_t payload, LiveWork &work) {
    debit(sum(limits.descriptions, limits.totalFields), work.visits);
    debit(product(limits.totalNameUnits, 2), work.textUnits);
    debit(payload, work.bytes);
}

/** Reserve complete data loops, symbol copies and sparse lookups before decoding.
 * @param limits Finite per-frame allowances.
 * @param descriptions Effective lookup length.
 * @param payload Complete payload bytes.
 * @param work Mutable turn credits.
 */
void chargeData(StreamDataLimits limits, std::size_t descriptions, std::size_t payload, LiveWork &work) {
    const auto lookups = product(limits.processedRecords, sum(descriptions, 1));
    debit(sum(sum(lookups, limits.wireFields), sum(limits.emittedValues, limits.emittedRecords)), work.visits);
    const auto symbols = product(product(limits.processedRecords, limits.symbol.codeUnits), 3);
    debit(sum(symbols, product(limits.fieldUnits, 2)), work.textUnits);
    debit(product(payload, 2), work.bytes);
}
} // namespace

struct LiveSnapshot::State {
    OwnerLedger ledger;         ///< Destroyed after all allocator-aware members and backing.
    LiveArena arena;            ///< Complete requested backing, including abandoned container growth.
    std::size_t references = 1; ///< Serialized intrusive handle count.
    std::uint64_t generation;   ///< Non-reused effective peer generation.
    Protocol protocol;          ///< Complete effective descriptor, without authorization secrets.
    std::pmr::vector<detail::PmrWireRecordDescription> descriptions; ///< Complete directional sender table.
    detail::PmrStreamDataContext context;                            ///< Immutable exact binding plans/defaults.
    std::pmr::vector<LivePropertyView> propertyViews;                ///< Stable top-level property views.
    std::pmr::vector<LivePropertyView> messageProperties;            ///< Flattened stable local-property views.
    std::pmr::vector<LiveMessageView> sendViews;                     ///< Stable directional send views.
    std::pmr::vector<LiveMessageView> receiveViews;                  ///< Stable directional receive views.
    std::pmr::vector<LiveFieldDescriptionView> fieldViews;           ///< Flattened stable physical-field views.
    std::pmr::vector<LiveRecordDescriptionView> descriptionViews;    ///< Stable effective description views.

    /** Initialize a complete candidate arena before variable metadata construction.
     * @param storage Ledger with pre-admitted object bytes.
     * @param bytes Exact arena capacity.
     * @param profile Immutable local exact-type policy.
     * @param value Candidate generation.
     */
    State(Storage &storage, std::size_t bytes, core::LocalProfile profile, std::uint64_t value)
        : ledger(storage, sizeof(State)), arena(storage, bytes), generation(value), protocol(*arena.resource()),
          descriptions(arena.resource()), context(profile, *arena.resource()), propertyViews(arena.resource()),
          messageProperties(arena.resource()), sendViews(arena.resource()), receiveViews(arena.resource()),
          fieldViews(arena.resource()), descriptionViews(arena.resource()) {
    }

    /** Clone complete old metadata without changing its owner.
     * @param old Immutable previous generation.
     * @param work Mutable turn credits.
     */
    void cloneFrom(const State &old, LiveWork &work) {
        mergeProperties(protocol.properties, old.protocol.properties, *arena.resource(), work);
        mergeMessages(protocol.send, old.protocol.send, *arena.resource(), work);
        mergeMessages(protocol.receive, old.protocol.receive, *arena.resource(), work);
        protocol.received = old.protocol.received;
        mergeDescriptions(old.descriptions, work);
    }

    /** Replace exact sender declarations in order using independent candidate ownership.
     * @param incoming Whole valid parsed batch or previous immutable table.
     * @param work Mutable turn credits.
     */
    void mergeDescriptions(const std::pmr::vector<detail::PmrWireRecordDescription> &incoming, LiveWork &work) {
        debit(descriptions.size(), work.visits);
        descriptions.reserve(sum(descriptions.size(), incoming.size()));

        for (const auto &record : incoming) {
            debit(1, work.visits);
            debit(record.name.size(), work.textUnits);
            detail::PmrWireRecordDescription next(record.senderId, std::pmr::u16string(record.name, arena.resource()),
                                                  *arena.resource());
            next.fields.reserve(record.fields.size());

            for (const auto &field : record.fields) {
                debit(1, work.visits);
                debit(field.name.size(), work.textUnits);
                next.fields.push_back({std::pmr::u16string(field.name, arena.resource()), field.serialTypeId});
            }

            auto found = descriptions.end();

            for (auto iter = descriptions.begin(); iter != descriptions.end(); ++iter) {
                debit(1, work.visits);

                if (iter->senderId == record.senderId) {
                    found = iter;
                    break;
                }
            }

            if (found == descriptions.end()) {
                descriptions.push_back(std::move(next));
            } else {
                *found = std::move(next);
            }
        }
    }

    /** Build complete shared-algorithm bindings and stable published views.
     * @param schema Immutable local schema.
     * @param limits Complete-generation finite bounds.
     * @param policy Explicit virtual-prefix recognition policy.
     * @param protocolLimits Complete effective descriptor bounds.
     * @param work Mutable turn credits.
     */
    void complete(const core::LocalSchema &schema, ContextLimits limits, FieldTimePolicy policy,
                  TapeDescriptorLimits protocolLimits, LiveWork &work) {
        if (descriptions.size() > limits.descriptions) {
            throw Error::RESOURCE_LIMIT;
        }

        std::size_t properties = protocol.properties.size();
        std::size_t units = 0;
        /** Check one complete effective nullable text value.
         * @param text Exact optional text.
         */
        const auto checkText = [&](const std::optional<std::pmr::u16string> &text) {
            if (!text) {
                return;
            }

            debit(text->size(), work.textUnits);

            if (text->size() > protocolLimits.text.codeUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            units = sum(units, text->size());

            if (units > protocolLimits.textUnits) {
                throw Error::RESOURCE_LIMIT;
            }
        };
        /** Check every complete property without flattening inheritance.
         * @param entries Candidate-owned exact properties.
         */
        const auto checkProperties = [&](const std::pmr::vector<Property> &entries) {
            for (const auto &entry : entries) {
                debit(1, work.visits);
                checkText(entry.key);
                checkText(entry.value);
            }
        };
        checkProperties(protocol.properties);

        for (const auto *messages : {&protocol.send, &protocol.receive}) {
            for (const auto &message : *messages) {
                debit(1, work.visits);
                properties = sum(properties, message.properties.size());
                checkText(message.name);
                checkProperties(message.properties);
            }
        }

        if (properties > protocolLimits.properties ||
            sum(protocol.send.size(), protocol.receive.size()) > protocolLimits.messages) {
            throw Error::RESOURCE_LIMIT;
        }

        validatePeer(protocol, work);
        const auto schemaLookups = limits.schema.lookups;
        const auto schemaNames = limits.schema.totalNameUnits;

        if (schema.records().size() > limits.schema.records) {
            throw Error::RESOURCE_LIMIT;
        }

        context.shapes.reserve(schema.records().size());
        context.plans.reserve(descriptions.size());

        for (const auto &record : schema.records()) {
            debit(1, work.visits);

            if (record.fields.size() > limits.schema.fields || record.lookups.size() > limits.schema.lookups ||
                record.name.size() > limits.schema.nameUnits || record.name.size() > limits.schema.totalNameUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            limits.schema.fields -= record.fields.size();
            limits.schema.lookups -= record.lookups.size();
            limits.schema.totalNameUnits -= record.name.size();

            for (const auto &lookup : record.lookups) {
                debit(1, work.visits);
                debit(lookup.name.size(), work.textUnits);

                if (lookup.name.size() > limits.schema.nameUnits || lookup.name.size() > limits.schema.totalNameUnits) {
                    throw Error::RESOURCE_LIMIT;
                }

                limits.schema.totalNameUnits -= lookup.name.size();
            }

            if (schema.profile() != core::LocalProfile::COMPACT_INT_V1) {
                context.shapes.emplace_back(*arena.resource());
                context.shapes.back().values.reserve(record.fields.size());

                for (const auto &field : record.fields) {
                    debit(1, work.visits);
                    context.shapes.back().values.emplace_back(static_cast<core::SerialFieldType>(field.serialTypeId),
                                                              0);
                }
            }
        }

        std::size_t totalFields = 0;

        for (const auto &description : descriptions) {
            debit(1, work.visits);
            const auto fields = description.fields.size();
            totalFields = sum(totalFields, fields);
            debit(sum(sum(schema.records().size(), product(fields, 3)), product(fields, schemaLookups)), work.visits);
            std::size_t names = description.name.size();

            for (const auto &field : description.fields) {
                debit(1, work.visits);
                names = sum(names, field.name.size());
            }

            debit(product(sum(names, schemaNames), sum(sum(fields, schema.records().size()), 1)), work.textUnits);
            const auto prefix = policy == FieldTimePolicy::READ ? detail::eventTimeFieldCount(description) : 0;
            auto binding = detail::bindPmrRecord(description, schema, limits.bindings, prefix, *arena.resource());

            if (!binding) {
                throw bindingError(binding.error());
            }

            limits.bindings.fields -= fields;
            limits.bindings.totalNameUnits -= names;
            detail::PmrStreamRecordPlan plan{std::move(*binding)};
            plan.eventTimeFieldCount = prefix;
            plan.removalPrefixFieldCount =
                policy == FieldTimePolicy::READ ? detail::removalPrefixFieldCount(description) : 0;

            if (plan.binding.localRecordId) {
                const auto &record = schema.records()[static_cast<std::size_t>(*plan.binding.localRecordId)];
                plan.context = schema.recordContext(record.id);
                plan.localFieldCount = record.fields.size();

                if (record.indexedTimeLayout) {
                    if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*record.indexedTimeLayout)) {
                        plan.indexedTimeLayout = ResolvedIndexedTimePairInt32{pair->firstField, pair->secondField};
                    } else if (const auto *low = std::get_if<core::IndexedTimeLowInt32>(&*record.indexedTimeLayout)) {
                        plan.indexedTimeLayout = ResolvedIndexedTimeLowInt32{low->field};
                    } else {
                        plan.indexedTimeLayout = ResolvedIndexedTimeSingleInt64{
                            std::get<core::IndexedTimeSingleInt64>(*record.indexedTimeLayout).field};
                    }
                }
            }

            context.plans.push_back(std::move(plan));
        }

        propertyViews.reserve(protocol.properties.size());

        for (const auto &entry : protocol.properties) {
            debit(1, work.visits);
            propertyViews.push_back({view(entry.key), view(entry.value)});
        }

        messageProperties.reserve(properties - protocol.properties.size());
        /** Build stable directional views after reserving all flattened property slots.
         * @param source Owned candidate message declarations.
         * @param target Stable published directional views.
         */
        const auto publishMessages = [&](const std::pmr::vector<Message> &source,
                                         std::pmr::vector<LiveMessageView> &target) {
            target.reserve(source.size());

            for (const auto &message : source) {
                debit(1, work.visits);
                const auto start = messageProperties.size();

                for (const auto &entry : message.properties) {
                    debit(1, work.visits);
                    messageProperties.push_back({view(entry.key), view(entry.value)});
                }

                target.push_back({message.id, view(message.name),
                                  std::span(messageProperties).subspan(start, message.properties.size())});
            }
        };
        publishMessages(protocol.send, sendViews);
        publishMessages(protocol.receive, receiveViews);
        fieldViews.reserve(totalFields);
        descriptionViews.reserve(descriptions.size());

        for (const auto &description : descriptions) {
            debit(1, work.visits);
            const auto start = fieldViews.size();

            for (const auto &field : description.fields) {
                debit(1, work.visits);
                fieldViews.push_back({field.name, field.serialTypeId});
            }

            descriptionViews.push_back({description.senderId, description.name,
                                        std::span(fieldViews).subspan(start, description.fields.size())});
        }
    }
};

struct LiveEvent::State {
    OwnerLedger ledger;                          ///< Released after every later member.
    LiveArena arena;                             ///< Full fixed data/view backing.
    LiveSnapshot snapshot;                       ///< Independently retained immutable peer generation.
    LiveEventKind kind;                          ///< Whole-envelope kind.
    std::optional<std::int32_t> messageType;     ///< Original message type, absent only for an empty envelope.
    std::optional<LiveHeartbeat> heartbeat;      ///< Selected explicit type0 fields.
    detail::PmrStreamDataResult decoded;         ///< Complete decoded owner, unpublished on failure.
    std::pmr::vector<LiveFieldValueView> fields; ///< Flattened stable value views.
    std::pmr::vector<LiveRecordView> records;    ///< Stable whole-frame local record views.

    /** Initialize a counted event arena without user-code execution.
     * @param storage Serialized epoch ledger.
     * @param bytes Complete arena capacity.
     * @param peer Immutable peer-generation handle.
     * @param value Whole-frame kind.
     */
    State(Storage &storage, std::size_t bytes, LiveSnapshot peer, LiveEventKind value)
        : ledger(storage, sizeof(State)), arena(storage, bytes), snapshot(std::move(peer)), kind(value),
          decoded(*arena.resource()), fields(arena.resource()), records(arena.resource()) {
    }

    /** Build views only after the whole data decoder succeeds.
     * @param work Mutable turn credits.
     */
    void complete(LiveWork &work) {
        std::size_t count = 0;

        for (const auto &record : decoded.records) {
            debit(1, work.visits);
            count = sum(count, record.typedValues.size());
        }

        debit(sum(count, decoded.records.size()), work.visits);
        fields.reserve(count);
        records.reserve(decoded.records.size());

        for (const auto &record : decoded.records) {
            const auto start = fields.size();

            for (const auto &field : record.typedValues) {
                fields.push_back(
                    {field.type, field.raw, view(field.units),
                     field.bytes ? std::optional<std::span<const std::byte>>{*field.bytes} : std::nullopt});
            }

            records.push_back({record.localId, record.senderId, record.context, record.symbol.units,
                               record.symbol.cipher, record.symbol.eventFlags, record.values,
                               std::span(fields).subspan(start, record.typedValues.size()), record.removalTime,
                               record.fieldTime});
        }
    }
};

detail::TapeEventBridge::TapeEventBridge(LiveSnapshot epoch, std::size_t eventBytes) noexcept
    : epoch_(std::move(epoch)), eventBytes_(eventBytes) {
}

TapeReader detail::TapeEventBridge::makeFixedSchemaReader(const core::LocalSchema &schema, TapeLimits limits,
                                                          FieldTimePolicy fieldTime) {
    return TapeReader(schema, limits, TapeMode::BINARY_CATALOG_V1, fieldTime, TapeExternalTimePolicy::DISABLED,
                      TapeReader::SchemaProvenance::CONFIGURED);
}

std::expected<detail::TapeEventBridge, LiveError> detail::TapeEventBridge::create(LiveLimits limits, LiveWork &work) {
    try {
        debit(1, work.operations);
        debit(sizeof(Storage) + sizeof(LiveSnapshot::State), work.bytes);

        if (limits.residentBytes < sizeof(Storage)) {
            return std::unexpected(Error::RESOURCE_LIMIT);
        }

        const auto release = [](Storage *value) {
            value->release();
        };
        std::unique_ptr<Storage, decltype(release)> storage(new Storage(limits.residentBytes, 0, limits.owners),
                                                            release);
        auto snapshot = makeOwner<LiveSnapshot::State>(*storage, 0, core::LocalProfile::BINARY_FIELDS_V1, 0);

        return TapeEventBridge(LiveSnapshot(snapshot.release()), limits.eventBytes);
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<LiveEvent, LiveError> detail::TapeEventBridge::copy(const TapeFeedResult &result,
                                                                  std::int32_t messageType, LiveWork &work) const {
    constexpr std::int32_t RAW_DATA = 5;

    if (!epoch_.state_ || result.status != TapeStatus::EVENT || !result.frame ||
        (messageType != TICKER_DATA && messageType != STREAM_DATA && messageType != HISTORY_DATA) ||
        (result.frame->type != RAW_DATA && result.frame->type != messageType)) {
        return std::unexpected(Error::INVALID_CONFIGURATION);
    }

    try {
        debit(1, work.operations);
        debit(result.records.size(), work.visits);
        debit(sum(sizeof(LiveEvent::State), eventBytes_), work.bytes);
        auto event =
            makeOwner<LiveEvent::State>(*epoch_.state_->ledger.lease.get(), eventBytes_, epoch_, LiveEventKind::DATA);
        event->messageType = messageType;
        auto &records = event->decoded.records;
        auto *resource = event->arena.resource();
        records.reserve(result.records.size());

        for (const auto &source : result.records) {
            debit(sum(source.typedValues.size(), source.values.size()), work.visits);
            debit(source.symbol.units.size(), work.textUnits);
            detail::PmrStreamDataRecord record{source.localId,
                                               encoding::PmrSymbolValue(*resource),
                                               std::pmr::vector<std::int32_t>(resource),
                                               std::pmr::vector<detail::PmrFieldValue>(resource),
                                               source.context,
                                               source.senderId,
                                               source.removalTime,
                                               source.fieldTime};
            record.symbol.units.assign(source.symbol.units);
            record.symbol.cipher = source.symbol.cipher;
            record.symbol.eventFlags = source.symbol.eventFlags;
            record.symbol.eventFlagsBytes = source.symbol.eventFlagsBytes;
            record.values.assign(source.values.begin(), source.values.end());
            record.typedValues.reserve(source.typedValues.size());

            for (const auto &field : source.typedValues) {
                auto &value = record.typedValues.emplace_back(field.type, field.raw);

                if (field.units) {
                    debit(field.units->size(), work.textUnits);
                    value.units.emplace(field.units->data(), field.units->size(), resource);
                }

                if (field.bytes) {
                    debit(field.bytes->size(), work.bytes);
                    value.bytes.emplace(field.bytes->begin(), field.bytes->end(), resource);
                }
            }

            records.push_back(std::move(record));
        }

        event->complete(work);

        return LiveEvent(std::move(event));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    }
}

struct LivePendingOutput::State {
    OwnerLedger ledger;                   ///< Retains epoch/object accounting until all backing is released.
    std::unique_ptr<std::byte[]> backing; ///< Fixed requested control allocation, when not a composer owner.
    std::optional<PendingSubscriptionOutput> subscription; ///< Existing exact subscription output.
    std::size_t capacity = 0;              ///< Globally charged full backing, including unused/consumed slots.
    std::size_t size = 0;                  ///< Exposed initialized envelope bytes.
    std::size_t consumed = 0;              ///< Local partial-write cursor, never peer acknowledgement.
    std::uint64_t subscriptionRecords = 0; ///< Exact composed ADD/REMOVE occurrences; zero for controls.

    /** Adopt pre-admitted output metadata.
     * @param storage Serialized epoch ledger.
     */
    explicit State(Storage &storage) noexcept : ledger(storage, sizeof(State)) {
    }

    /** Destroy backing before relinquishing global credits. */
    ~State() {
        subscription.reset();
        backing.reset();
        ledger.lease.get()->relinquish(capacity);
    }

    /** Borrow complete initialized envelope bytes.
     * @return Immutable bytes owned by this state.
     */
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return subscription ? subscription->bytes() : std::span<const std::byte>(backing.get(), size);
    }
};

namespace {
/** Canonical control composer writing only admitted output backing. */
class ControlWriter {
    std::span<std::byte> output_;       ///< Fixed admitted capacity.
    LiveWork &work_;                    ///< Mutable turn credits.
    encoding::UtfStringLimits text_;    ///< Finite per-string bounds.
    std::size_t cursor_ = PREFIX_BYTES; ///< Reserved length-prefix staging followed by body bytes.

    public:
    /** Begin one exact message in admitted storage.
     * @param output Whole backing, including prefix staging.
     * @param work Mutable turn credits.
     * @param text Finite UTFString bounds.
     * @param type Known nonnegative message identity.
     */
    ControlWriter(std::span<std::byte> output, LiveWork &work, encoding::UtfStringLimits text, std::int32_t type)
        : output_(output), work_(work), text_(text) {
        integer(type);
    }

    /** Append canonical compact-long preserving signed64 bits.
     * @param value Exact raw value.
     */
    void number(std::int64_t value) {
        std::array<std::byte, PREFIX_BYTES> encoded{};
        const auto result = encoding::writeCompactLong(value, encoded);
        raw(std::span(encoded).first(result.written));
    }

    /** Append canonical compact-int preserving its five-byte negative representation.
     * @param value Exact signed32 value.
     */
    void integer(std::int32_t value) {
        std::array<std::byte, PREFIX_BYTES> encoded{};
        const auto result = encoding::writeCompactInt(value, encoded);
        raw(std::span(encoded).first(result.written));
    }

    /** Copy already encoded fixed bytes.
     * @param bytes Borrowed initialized bytes.
     */
    void raw(std::span<const std::byte> bytes) {
        debit(bytes.size(), work_.bytes);

        if (bytes.size() > output_.size() - cursor_) {
            throw Error::RESOURCE_LIMIT;
        }

        std::copy(bytes.begin(), bytes.end(), output_.begin() + static_cast<std::ptrdiff_t>(cursor_));
        cursor_ += bytes.size();
    }

    /** Encode exact nullable UTFString through the shared canonical primitive.
     * @param value Borrowed exact text or Java null.
     */
    void text(std::optional<std::u16string_view> value) {
        debit(value ? product(value->size(), 2) : 0, work_.textUnits);
        debit(sum(PREFIX_BYTES, product(value ? value->size() : 0, 3)), work_.bytes);
        const auto result = encoding::writeUtfString(value, output_.subspan(cursor_), text_);

        if (result.status != encoding::Status::OK) {
            throw result.status == encoding::Status::RESOURCE_LIMIT ||
                    result.status == encoding::Status::INSUFFICIENT_OUTPUT
                ? Error::RESOURCE_LIMIT
                : Error::INVALID_CONFIGURATION;
        }

        cursor_ += result.written;
    }

    /** Encode complete nullable property pairs.
     * @param properties Borrowed bounded list.
     */
    void properties(std::span<const LivePropertyView> properties) {
        if (properties.size() > MAX_WIRE_COUNT) {
            throw Error::RESOURCE_LIMIT;
        }

        integer(static_cast<std::int32_t>(properties.size()));

        for (const auto &entry : properties) {
            debit(1, work_.visits);
            text(entry.key);
            text(entry.value);
        }
    }

    /** Encode one directional list.
     * @param messages Borrowed bounded declarations.
     */
    void messages(std::span<const LiveMessageView> messages) {
        if (messages.size() > MAX_WIRE_COUNT) {
            throw Error::RESOURCE_LIMIT;
        }

        integer(static_cast<std::int32_t>(messages.size()));

        for (const auto &message : messages) {
            debit(1, work_.visits);
            integer(message.id);
            text(message.name);
            properties(message.properties);
        }
    }

    /** Admit body size and compact prefix staging in place.
     * @param maximumBody Finite body-byte bound, including type.
     * @param maximumOutput Finite initialized envelope-byte bound.
     * @return Complete initialized size; requested capacity remains fully charged.
     */
    std::size_t finish(std::size_t maximumBody, std::size_t maximumOutput) {
        const auto body = cursor_ - PREFIX_BYTES;

        if (body > maximumBody || body > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
            throw Error::RESOURCE_LIMIT;
        }

        std::array<std::byte, PREFIX_BYTES> prefix{};
        const auto size = encoding::writeCompactLong(static_cast<std::int64_t>(body), prefix).written;
        const auto total = sum(body, size);

        if (total > maximumOutput) {
            throw Error::RESOURCE_LIMIT;
        }

        debit(total, work_.bytes);
        std::memmove(output_.data() + size, output_.data() + PREFIX_BYTES, body);
        std::copy_n(prefix.begin(), size, output_.begin());

        return total;
    }
};

/** Decode selected heartbeat fields with shared compact primitives and no timing effects.
 * @param payload Complete type0 body, excluding type.
 * @param work Mutable turn credits.
 * @return Exact field presence and raw bits, without reading a clock.
 */
LiveHeartbeat heartbeat(std::span<const std::byte> payload, LiveWork &work) {
    debit(payload.size(), work.bytes);
    LiveHeartbeat result;

    if (payload.empty()) {
        result.hasPayload = false;

        return result;
    }

    const auto content = encoding::readCompactInt(payload);

    if (content.status != encoding::Status::OK) {
        throw Error::MALFORMED;
    }

    const auto bits = static_cast<std::uint32_t>(content.value);

    if ((bits & ~HEARTBEAT_MASK) != 0) {
        throw Error::UNSUPPORTED;
    }

    std::size_t cursor = content.consumed;
    /** Read and commit one exact selected field primitive.
     * @param wide Whether this field retains compact-long width.
     * @return Exact signed value; malformed input throws before event publication.
     */
    const auto number = [&](bool wide) -> std::int64_t {
        if (wide) {
            const auto value = encoding::readCompactLong(payload.subspan(cursor));
            cursor += value.consumed;

            if (value.status != encoding::Status::OK) {
                throw Error::MALFORMED;
            }

            return value.value;
        }

        const auto value = encoding::readCompactInt(payload.subspan(cursor));
        cursor += value.consumed;

        if (value.status != encoding::Status::OK) {
            throw Error::MALFORMED;
        }

        return value.value;
    };

    if ((bits & TIME_BIT) != 0) {
        result.timeMillis = number(true);
    }

    if ((bits & MARK_BIT) != 0) {
        result.timeMark = static_cast<std::int32_t>(number(false));
    }

    if ((bits & DELTA_BIT) != 0) {
        result.deltaMark = static_cast<std::int32_t>(number(false));
    }

    if ((bits & LAG_BIT) != 0) {
        result.lagMark = static_cast<std::int32_t>(number(false));
    }

    if (cursor != payload.size()) {
        throw Error::MALFORMED;
    }

    return result;
}
} // namespace

struct LiveSession::Impl {
    LiveStorageLease lease;              ///< Destroyed last, retaining conservative fixed configuration accounting.
    LiveLimits limits;                   ///< Immutable finite bounds and profile choices.
    core::LocalSchema schema;            ///< Immutable local configuration/provenance.
    SubscriptionWriter composer;         ///< Fresh outgoing layouts and exact composer output credits.
    std::unique_ptr<FrameReader> framer; ///< Independent fixed framing state.
    LiveSnapshot peer;                   ///< Current complete immutable protocol/binding generation.
    LiveStatus status = LiveStatus::NEED_INPUT; ///< Input lifecycle.
    LiveDiagnostic diagnostic;                  ///< First terminal static/numeric evidence.
    LiveInputStatistics statistics;             ///< Serialized framing/commit totals, independent of event retention.
    LiveAuthentication authentication = LiveAuthentication::UNCHALLENGED; ///< Protocol-derived auth state.
    bool historySnapshot = false;                                         ///< Cached exact negotiated hs intersection.

    /** Commit noexcept ownership transfers after all prior allocations succeed.
     * @param storage Ledger retained by the first member.
     * @param bounds Immutable limits.
     * @param local Validated local schema.
     * @param writer Fresh matching composer.
     * @param frame Fully allocated independent framer.
     * @param snapshot Complete initial peer generation.
     */
    Impl(Storage &storage, LiveLimits bounds, core::LocalSchema &&local, SubscriptionWriter &&writer,
         std::unique_ptr<FrameReader> frame, LiveSnapshot snapshot) noexcept
        : lease(&storage), limits(bounds), schema(std::move(local)), composer(std::move(writer)),
          framer(std::move(frame)), peer(std::move(snapshot)) {
    }

    /** Record the first terminal input failure without retaining peer text.
     * @param error Static typed category.
     */
    void fail(Error error) noexcept {
        if (status != LiveStatus::FAILED) {
            diagnostic.error = error;
            diagnostic.accepted = framer->totalAccepted();
            status = LiveStatus::FAILED;
        }
    }

    /** Refuse failed or closed state before examining caller-owned inputs. */
    void requireOpen() const {
        if (status == LiveStatus::FAILED || status == LiveStatus::CLOSED) {
            throw Error::INVALID_STATE;
        }
    }

    /** Construct a full candidate without publishing it.
     * @param work Mutable turn credits.
     * @return Independently owned cloned candidate, before final bindings/views.
     */
    std::unique_ptr<LiveSnapshot::State> candidate(LiveWork &work) {
        const auto generation = peer.state_->generation;

        if (generation == std::numeric_limits<std::uint64_t>::max()) {
            throw Error::IDENTITY_EXHAUSTED;
        }

        auto next =
            makeOwner<LiveSnapshot::State>(*lease.get(), limits.snapshotBytes, schema.profile(), generation + 1);
        next->cloneFrom(*peer.state_, work);

        return next;
    }

    /** Admit one whole envelope before making any records or candidate state visible.
     * @param frame Complete owning frame, borrowed during dispatch.
     * @param work Mutable turn credits.
     * @return Complete immutable event; a failed frame exposes no decoder prefix.
     */
    LiveEvent dispatch(const Frame &frame, LiveWork &work) {
        diagnostic.frameStart = frame.startOffset;
        diagnostic.consumed = 0;
        diagnostic.detail = 0;
        diagnostic.stage = "message";
        auto kind = LiveEventKind::EMPTY_HEARTBEAT;
        std::optional<LiveHeartbeat> beat;
        std::unique_ptr<LiveSnapshot::State> next;

        if (frame.kind == FrameKind::EMPTY_HEARTBEAT) {
            kind = LiveEventKind::EMPTY_HEARTBEAT;
        } else if (frame.type == HEARTBEAT) {
            diagnostic.stage = "heartbeat";
            beat = heartbeat(frame.payload, work);
            kind = LiveEventKind::HEARTBEAT;
        } else if (frame.type == DESCRIBE_PROTOCOL) {
            LiveArena scratch(*lease.get(), limits.scratchBytes);
            auto incoming =
                ProtocolParser(frame.payload, limits.protocol, work, *scratch.resource(), diagnostic).parse();
            next = candidate(work);
            mergeProperties(next->protocol.properties, incoming.properties, *next->arena.resource(), work);
            mergeMessages(next->protocol.send, incoming.send, *next->arena.resource(), work);
            mergeMessages(next->protocol.receive, incoming.receive, *next->arena.resource(), work);
            next->protocol.received = true;
            next->complete(schema, limits.context, limits.fieldTime, limits.protocol, work);
            kind = LiveEventKind::PROTOCOL;
        } else if (frame.type == DESCRIBE_RECORDS) {
            diagnostic.stage = "descriptions";
            LiveArena scratch(*lease.get(), limits.scratchBytes);
            chargeDescriptions(limits.descriptions, frame.payload.size(), work);
            auto incoming = detail::parsePmrDescriptions(frame.payload, limits.descriptions, *scratch.resource());
            diagnostic.consumed = incoming.consumed;
            diagnostic.detail = static_cast<std::size_t>(incoming.stage);

            if (incoming.status != DescriptionStatus::OK) {
                throw descriptionError(incoming.status);
            }

            next = candidate(work);
            next->mergeDescriptions(incoming.descriptions, work);
            next->complete(schema, limits.context, limits.fieldTime, limits.protocol, work);
            kind = LiveEventKind::DESCRIPTIONS;
        } else if (frame.type == TICKER_DATA || frame.type == STREAM_DATA || frame.type == HISTORY_DATA) {
            debit(peer.state_->protocol.send.size(), work.visits);

            if (!peer.state_->protocol.received || !capability(peer.state_->protocol.send, *frame.type, work)) {
                throw Error::NOT_READY;
            }

            if (authentication == LiveAuthentication::REQUIRED ||
                authentication == LiveAuthentication::AUTHORIZATION_EMITTED) {
                throw Error::AUTHENTICATION_REQUIRED;
            }

            kind = LiveEventKind::DATA;
        } else {
            throw Error::UNSUPPORTED;
        }

        LiveSnapshot selected = next ? LiveSnapshot(next.release()) : peer;
        auto event = makeOwner<LiveEvent::State>(*lease.get(), limits.eventBytes, selected, kind);
        event->messageType = frame.type;
        event->heartbeat = beat;

        if (kind == LiveEventKind::DATA) {
            diagnostic.stage = "data";
            chargeData(limits.data, selected.state_->descriptions.size(), frame.payload.size(), work);
            event->decoded = detail::decodePmrStreamData(frame.payload, selected.state_->context, limits.data,
                                                         limits.events, *event->arena.resource());
            diagnostic.consumed = event->decoded.consumed;
            diagnostic.detail = static_cast<std::size_t>(event->decoded.stage);

            if (event->decoded.status != StreamDataStatus::OK) {
                throw dataError(event->decoded.status);
            }

            event->complete(work);
            statistics.dataRecords.add(event->decoded.processedRecords);
        }

        auto nextAuthentication = authentication;
        auto nextHistorySnapshot = historySnapshot;

        if (kind == LiveEventKind::PROTOCOL) {
            debit(product(selected.state_->protocol.properties.size(), 2), work.visits);
            const auto *auth = property(selected.state_->protocol.properties, u"authentication", work);

            if (auth && *auth) {
                nextAuthentication = (**auth).empty() ? LiveAuthentication::ACKNOWLEDGED : LiveAuthentication::REQUIRED;
            }

            const auto *options = property(selected.state_->protocol.properties, u"opt", work);
            debit(options && *options ? (**options).size() : 0, work.textUnits);
            nextHistorySnapshot = limits.historySnapshot && hasHistorySnapshot(options);
        }

        // Only type1 processes protocol authentication. A type2 binding change cannot acknowledge credentials.
        if (kind == LiveEventKind::PROTOCOL || kind == LiveEventKind::DESCRIPTIONS) {
            peer = selected;
            authentication = nextAuthentication;
            historySnapshot = nextHistorySnapshot;
        }

        return LiveEvent(std::move(event));
    }

    /** Reserve bounded control backing before any variable output allocation.
     * @return Output metadata plus uninitialized fixed backing and zero cursor.
     */
    std::unique_ptr<LivePendingOutput::State> controlOutput() {
        auto output = makeOwner<LivePendingOutput::State>(*lease.get());
        const auto capacity = sum(limits.outputBytes, PREFIX_BYTES);

        if (!lease.get()->reserve(capacity)) {
            throw LiveStorageLimit{};
        }

        output->capacity = capacity;
        output->backing.reset(new std::byte[capacity]);

        return output;
    }

    /** Compose through the existing writer after global maximum-operation backing reservation.
     * @param message Subscription identity, absent for descriptions.
     * @param requests Borrowed exact keys, unused for descriptions.
     * @param ids Borrowed outgoing IDs, unused for subscriptions.
     * @param work Mutable turn credits synchronized on every writer outcome.
     * @return Whole globally counted immutable output.
     */
    LivePendingOutput writerOutput(std::optional<SubscriptionMessage> message,
                                   std::span<const core::SubscriptionRequest> requests,
                                   std::span<const std::int32_t> ids, LiveWork &work) {
        auto output = makeOwner<LivePendingOutput::State>(*lease.get());
        const auto maximum = composer.maximumOutputBytes();

        if (!lease.get()->reserve(maximum)) {
            throw LiveStorageLimit{};
        }

        output->capacity = maximum;
        SubscriptionWriterWork credits{work.operations, work.visits, work.textUnits, work.bytes};
        auto result = message ? composer.compose(*message, requests, credits) : composer.describe(ids, credits);
        work.operations = credits.operations;
        work.visits = credits.visits;
        work.textUnits = credits.textUnits;
        work.bytes = credits.outputBytes;

        if (!result) {
            throw writerError(result.error());
        }

        const auto actual = result->bytes().size();

        if (actual > maximum) {
            throw Error::RESOURCE_LIMIT;
        }

        lease.get()->relinquish(maximum - actual);
        output->capacity = actual;
        output->size = actual;
        output->subscription.emplace(std::move(*result));
        output->subscriptionRecords = message ? requests.size() : 0;

        return LivePendingOutput(std::move(output));
    }
};

LiveIdentity::LiveIdentity(void *epoch, std::uint64_t generation) noexcept : epoch_(epoch), generation_(generation) {
    if (epoch_) {
        static_cast<Storage *>(epoch_)->retain();
    }
}

LiveIdentity::LiveIdentity(const LiveIdentity &other) noexcept : LiveIdentity(other.epoch_, other.generation_) {
}

LiveIdentity::LiveIdentity(LiveIdentity &&other) noexcept
    : epoch_(std::exchange(other.epoch_, nullptr)), generation_(std::exchange(other.generation_, 0)) {
}

LiveIdentity &LiveIdentity::operator=(const LiveIdentity &other) noexcept {
    if (this != &other) {
        LiveIdentity next(other);
        std::swap(epoch_, next.epoch_);
        std::swap(generation_, next.generation_);
    }

    return *this;
}

LiveIdentity &LiveIdentity::operator=(LiveIdentity &&other) noexcept {
    if (this != &other) {
        LiveIdentity next(std::move(other));
        std::swap(epoch_, next.epoch_);
        std::swap(generation_, next.generation_);
    }

    return *this;
}

LiveIdentity::~LiveIdentity() {
    if (epoch_) {
        static_cast<Storage *>(epoch_)->release();
    }
}

bool LiveIdentity::matches(const LiveIdentity &other) const noexcept {
    return epoch_ && epoch_ == other.epoch_ && generation_ == other.generation_;
}

bool LiveIdentity::sameEpoch(const LiveIdentity &other) const noexcept {
    return epoch_ && epoch_ == other.epoch_;
}

std::uint64_t LiveIdentity::generation() const noexcept {
    return generation_;
}

std::size_t LiveIdentity::releaseBytes() const noexcept {
    return epoch_ ? sizeof(Storage) : 0;
}

LiveSnapshot::LiveSnapshot(State *state) noexcept : state_(state) {
}

LiveSnapshot::LiveSnapshot(const LiveSnapshot &other) noexcept : state_(other.state_) {
    if (state_) {
        ++state_->references;
    }
}

LiveSnapshot::LiveSnapshot(LiveSnapshot &&other) noexcept : state_(std::exchange(other.state_, nullptr)) {
}

LiveSnapshot &LiveSnapshot::operator=(const LiveSnapshot &other) noexcept {
    if (this != &other) {
        LiveSnapshot next(other);
        std::swap(state_, next.state_);
    }

    return *this;
}

LiveSnapshot &LiveSnapshot::operator=(LiveSnapshot &&other) noexcept {
    if (this != &other) {
        LiveSnapshot next(std::move(other));
        std::swap(state_, next.state_);
    }

    return *this;
}

LiveSnapshot::~LiveSnapshot() {
    if (state_ && --state_->references == 0) {
        delete state_;
    }
}

LiveProtocolView LiveSnapshot::protocol() const noexcept {
    return state_ ? LiveProtocolView{state_->propertyViews, state_->sendViews, state_->receiveViews}
                  : LiveProtocolView{};
}

std::span<const LiveRecordDescriptionView> LiveSnapshot::descriptions() const noexcept {
    return state_ ? std::span<const LiveRecordDescriptionView>(state_->descriptionViews)
                  : std::span<const LiveRecordDescriptionView>{};
}

LiveIdentity LiveSnapshot::identity() const noexcept {
    return state_ ? LiveIdentity(state_->ledger.lease.get(), state_->generation) : LiveIdentity{};
}

LiveEvent::LiveEvent(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

LiveEvent::LiveEvent(LiveEvent &&other) noexcept = default;
LiveEvent &LiveEvent::operator=(LiveEvent &&other) noexcept = default;
LiveEvent::~LiveEvent() = default;

LiveEventKind LiveEvent::kind() const noexcept {
    return state_ ? state_->kind : LiveEventKind::EMPTY_HEARTBEAT;
}

std::optional<std::int32_t> LiveEvent::messageType() const noexcept {
    return state_ ? state_->messageType : std::nullopt;
}

std::span<const LiveRecordView> LiveEvent::records() const noexcept {
    return state_ ? std::span<const LiveRecordView>(state_->records) : std::span<const LiveRecordView>{};
}

std::optional<LiveHeartbeat> LiveEvent::heartbeat() const noexcept {
    return state_ ? state_->heartbeat : std::nullopt;
}

LiveSnapshot LiveEvent::snapshot() const noexcept {
    return state_ ? state_->snapshot : LiveSnapshot{};
}

std::size_t LiveEvent::residentBytes() const noexcept {
    // Both reservations coexist in one finite ledger, so their sum is representable.
    return state_ ? sizeof(State) + state_->arena.capacityBytes() : 0;
}

std::size_t LiveEvent::releaseBytes() const noexcept {
    return state_ ? state_->ledger.lease.get()->resident() : 0;
}

LivePendingOutput::LivePendingOutput(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

LivePendingOutput::LivePendingOutput(LivePendingOutput &&other) noexcept = default;
LivePendingOutput &LivePendingOutput::operator=(LivePendingOutput &&other) noexcept = default;
LivePendingOutput::~LivePendingOutput() = default;

std::span<const std::byte> LivePendingOutput::bytes() const noexcept {
    return state_ ? state_->bytes() : std::span<const std::byte>{};
}

std::uint64_t LivePendingOutput::subscriptionRecords() const noexcept {
    return state_ ? state_->subscriptionRecords : 0;
}

std::span<const std::byte> LivePendingOutput::pendingBytes() const noexcept {
    return state_ ? state_->bytes().subspan(state_->consumed) : std::span<const std::byte>{};
}

std::expected<void, LiveError> LivePendingOutput::consume(std::size_t count) noexcept {
    if (!state_ || count > state_->size - state_->consumed) {
        return std::unexpected(Error::INVALID_CONSUMPTION);
    }

    state_->consumed += count;

    return {};
}

LiveSession::LiveSession(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

LiveSession::LiveSession(LiveSession &&other) noexcept = default;
LiveSession &LiveSession::operator=(LiveSession &&other) noexcept = default;
LiveSession::~LiveSession() = default;

LiveInputStatistics LiveSession::inputStatistics() const noexcept {
    return impl_ ? impl_->statistics : LiveInputStatistics{};
}

std::expected<LiveSession, LiveError> LiveSession::create(core::LocalSchema &&schema, SubscriptionWriter &&composer,
                                                          LiveLimits limits, LiveWork &work) {
    try {
        debit(1, work.operations);
        debit(1, work.visits);

        if (!composer.matchesSchema(schema) || composer.retainedBytes() != 0) {
            return std::unexpected(Error::INVALID_CONFIGURATION);
        }

        if ((limits.credentials != LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN &&
             limits.credentials != LiveCredentialPolicy::VERIFIED_TLS_BASIC) ||
            (limits.fieldTime != FieldTimePolicy::DISABLED && limits.fieldTime != FieldTimePolicy::READ) ||
            (limits.events != DataEventPolicy::ZERO_FLAGS && limits.events != DataEventPolicy::BINARY_CATALOG_V1) ||
            ((limits.fieldTime == FieldTimePolicy::READ || limits.events == DataEventPolicy::BINARY_CATALOG_V1) &&
             schema.profile() != core::LocalProfile::BINARY_FIELDS_V1)) {
            return std::unexpected(Error::INVALID_CONFIGURATION);
        }

        if (limits.frame.maximumBodyBytes > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
            limits.frame.maximumBodyBytes > std::numeric_limits<std::size_t>::max() - PREFIX_BYTES ||
            limits.frame.maximumPendingBytes < limits.frame.maximumBodyBytes + PREFIX_BYTES) {
            return std::unexpected(Error::INVALID_CONFIGURATION);
        }

        const auto base =
            sum(sum(sizeof(Impl), sizeof(FrameReader)), sum(sum(schema.storageBytes(), composer.storageBytes()),
                                                            sum(limits.frame.maximumBodyBytes, PREFIX_BYTES)));

        if (sum(base, sizeof(Storage)) > limits.residentBytes) {
            return std::unexpected(Error::RESOURCE_LIMIT);
        }

        debit(schema.records().size(), work.visits);
        /** Release only the factory's initial root lease, after candidate owners die.
         * @param value Fresh serialized ledger.
         */
        const auto release = [](Storage *value) {
            value->release();
        };
        std::unique_ptr<Storage, decltype(release)> storage(new Storage(limits.residentBytes, base, limits.owners),
                                                            release);
        auto framer = std::make_unique<FrameReader>(limits.frame);
        auto snapshot = makeOwner<LiveSnapshot::State>(*storage, limits.snapshotBytes, schema.profile(), 0);
        snapshot->complete(schema, limits.context, limits.fieldTime, limits.protocol, work);
        LiveSnapshot initial(snapshot.release());
        static_assert(std::is_nothrow_move_constructible_v<core::LocalSchema>);
        static_assert(std::is_nothrow_move_constructible_v<SubscriptionWriter>);
        auto implementation = std::unique_ptr<Impl>(
            new Impl(*storage, limits, std::move(schema), std::move(composer), std::move(framer), std::move(initial)));

        return LiveSession(std::move(implementation));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

LiveFeedResult LiveSession::feed(std::span<const std::byte> input, LiveWork &work) {
    LiveFeedResult result;

    if (!impl_) {
        result.status = LiveStatus::FAILED;

        return result;
    }

    if (impl_->status == LiveStatus::FAILED || impl_->status == LiveStatus::CLOSED) {
        result.status = impl_->status;

        return result;
    }

    try {
        debit(1, work.operations);
        debit(std::min(input.size(), impl_->limits.frame.maximumPendingBytes), work.bytes);
        TemporaryReservation payload(*impl_->lease.get(), impl_->limits.frame.maximumBodyBytes);
        auto framed = impl_->framer->feed(input.first(std::min(input.size(), impl_->limits.frame.maximumPendingBytes)));
        result.accepted = framed.accepted;

        if (framed.status == FrameStatus::FAILED) {
            impl_->diagnostic.stage = "frame";
            impl_->diagnostic.frameStart = impl_->framer->errorOffset();
            impl_->diagnostic.detail = static_cast<std::size_t>(impl_->framer->error());
            impl_->fail(impl_->framer->error() == FrameError::ALLOCATION_FAILURE ? Error::ALLOCATION_FAILURE
                        : impl_->framer->error() == FrameError::BODY_LIMIT       ? Error::RESOURCE_LIMIT
                        : impl_->framer->error() == FrameError::OFFSET_OVERFLOW  ? Error::IDENTITY_EXHAUSTED
                                                                                 : Error::MALFORMED);
        } else if (framed.status == FrameStatus::FRAME_READY) {
            impl_->statistics.messages.add(1);
            result.event.emplace(impl_->dispatch(*framed.frame, work));
            result.status = LiveStatus::EVENT;

            return result;
        }
    } catch (Error error) {
        impl_->fail(error);
    } catch (const LiveStorageLimit &) {
        impl_->fail(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        impl_->fail(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        impl_->fail(Error::ALLOCATION_FAILURE);
    }
    result.status = impl_->status;

    return result;
}

LiveStatus LiveSession::finish() noexcept {
    if (!impl_) {
        return LiveStatus::FAILED;
    }

    if (impl_->status == LiveStatus::FAILED || impl_->status == LiveStatus::CLOSED) {
        return impl_->status;
    }

    const auto state = impl_->framer->finish();

    if (state == FrameStatus::FAILED || !impl_->peer.state_->protocol.received) {
        impl_->diagnostic.stage = "finish";
        impl_->diagnostic.frameStart = impl_->framer->errorOffset();
        impl_->fail(Error::MALFORMED);
    } else {
        impl_->status = LiveStatus::CLOSED;
    }

    return impl_->status;
}

namespace {
/** Refuse unsupported local advertisements and generic credential-bearing properties.
 * @param properties Borrowed explicit properties.
 * @param historySnapshot Whether hs is implemented locally.
 * @param work Mutable turn credits.
 */
void validateOutgoingProperties(std::span<const LivePropertyView> properties, bool historySnapshot, LiveWork &work) {
    for (const auto &entry : properties) {
        debit(1, work.visits);

        if (!entry.key) {
            continue;
        }

        if (same(*entry.key, u"authorization", work) || same(*entry.key, u"authentication", work)) {
            throw Error::CREDENTIALS_FORBIDDEN;
        }

        if (same(*entry.key, u"opt", work) && entry.value && !entry.value->empty() &&
            (!historySnapshot || !same(*entry.value, u"hs", work))) {
            throw Error::UNSUPPORTED;
        }

        if (same(*entry.key, u"time", work) && entry.value && !entry.value->empty() &&
            !same(*entry.value, u"NONE", work) && !same(*entry.value, u"none", work)) {
            throw Error::UNSUPPORTED;
        }

        if (same(*entry.key, u"type", work) && entry.value && !entry.value->empty() &&
            !same(*entry.value, u"qtp", work)) {
            throw Error::UNSUPPORTED;
        }
    }
}
} // namespace

std::expected<LivePendingOutput, LiveError> LiveSession::composeProtocol(LiveProtocolView descriptor, LiveWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            return std::unexpected(Error::INVALID_STATE);
        }

        impl_->requireOpen();
        std::size_t properties = descriptor.properties.size();
        std::size_t units = 0;
        /** Validate complete exact properties and charge aggregate units.
         * @param entries Borrowed local or top-level list.
         */
        const auto checkProperties = [&](std::span<const LivePropertyView> entries) {
            validateOutgoingProperties(entries, impl_->limits.historySnapshot, work);

            for (const auto &entry : entries) {
                units = sum(units, sum(entry.key ? entry.key->size() : 0, entry.value ? entry.value->size() : 0));
            }
        };
        checkProperties(descriptor.properties);

        if (sum(descriptor.send.size(), descriptor.receive.size()) > impl_->limits.protocol.messages) {
            throw Error::RESOURCE_LIMIT;
        }

        /** Refuse every unimplemented locally advertised message.
         * @param messages Directional borrowed declarations.
         * @param send Whether these are local send rather than receive capabilities.
         */
        const auto checkMessages = [&](std::span<const LiveMessageView> messages, bool send) {
            for (const auto &message : messages) {
                debit(1, work.visits);
                const auto known = messageName(message.id);
                const bool common =
                    message.id == HEARTBEAT || message.id == DESCRIBE_PROTOCOL || message.id == DESCRIBE_RECORDS;
                const bool supported = send ? common || message.id == 11 || message.id == 12 || message.id == 16 ||
                                                  message.id == 17 || message.id == 21 || message.id == 22
                                            : common || message.id == TICKER_DATA || message.id == STREAM_DATA ||
                                                  message.id == HISTORY_DATA;

                if (!supported || !message.name || !same(*message.name, known, work)) {
                    throw Error::UNSUPPORTED;
                }

                properties = sum(properties, message.properties.size());
                units = sum(units, message.name->size());
                checkProperties(message.properties);
            }
        };
        checkMessages(descriptor.send, true);
        checkMessages(descriptor.receive, false);

        if (properties > impl_->limits.protocol.properties || units > impl_->limits.protocol.textUnits) {
            throw Error::RESOURCE_LIMIT;
        }

        auto output = impl_->controlOutput();
        ControlWriter writer({output->backing.get(), output->capacity}, work, impl_->limits.protocol.text,
                             DESCRIBE_PROTOCOL);
        writer.raw(MAGIC);
        writer.properties(descriptor.properties);
        writer.messages(descriptor.send);
        writer.messages(descriptor.receive);
        output->size = writer.finish(impl_->limits.frame.maximumBodyBytes, impl_->limits.outputBytes);

        return LivePendingOutput(std::move(output));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<LivePendingOutput, LiveError> LiveSession::composeHeartbeat(LiveHeartbeat heartbeat, LiveWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            return std::unexpected(Error::INVALID_STATE);
        }

        impl_->requireOpen();

        if (!heartbeat.hasPayload &&
            (heartbeat.timeMillis || heartbeat.timeMark || heartbeat.deltaMark || heartbeat.lagMark)) {
            throw Error::INVALID_CONFIGURATION;
        }

        auto output = impl_->controlOutput();
        ControlWriter writer({output->backing.get(), output->capacity}, work, impl_->limits.protocol.text, HEARTBEAT);

        if (heartbeat.hasPayload) {
            const auto bits = (heartbeat.timeMillis ? TIME_BIT : 0U) | (heartbeat.timeMark ? MARK_BIT : 0U) |
                              (heartbeat.deltaMark ? DELTA_BIT : 0U) | (heartbeat.lagMark ? LAG_BIT : 0U);
            writer.integer(static_cast<std::int32_t>(bits));

            if (heartbeat.timeMillis) {
                writer.number(*heartbeat.timeMillis);
            }

            if (heartbeat.timeMark) {
                writer.integer(*heartbeat.timeMark);
            }

            if (heartbeat.deltaMark) {
                writer.integer(*heartbeat.deltaMark);
            }

            if (heartbeat.lagMark) {
                writer.integer(*heartbeat.lagMark);
            }
        }

        output->size = writer.finish(impl_->limits.frame.maximumBodyBytes, impl_->limits.outputBytes);

        return LivePendingOutput(std::move(output));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<LivePendingOutput, LiveError> LiveSession::authorize(std::u16string_view basicToken, LiveWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            return std::unexpected(Error::INVALID_STATE);
        }

        impl_->requireOpen();

        if (impl_->limits.credentials != LiveCredentialPolicy::VERIFIED_TLS_BASIC) {
            throw Error::CREDENTIALS_FORBIDDEN;
        }

        debit(basicToken.size(), work.textUnits);

        if (!basicToken.starts_with(u"Basic ") || basicToken.size() <= std::u16string_view(u"Basic ").size()) {
            throw Error::INVALID_CONFIGURATION;
        }

        const LivePropertyView property{u"authorization", basicToken};
        auto output = impl_->controlOutput();
        ControlWriter writer({output->backing.get(), output->capacity}, work, impl_->limits.protocol.text,
                             DESCRIBE_PROTOCOL);
        writer.raw(MAGIC);
        writer.properties(std::span(&property, 1));
        writer.messages({});
        writer.messages({});
        output->size = writer.finish(impl_->limits.frame.maximumBodyBytes, impl_->limits.outputBytes);
        impl_->authentication = LiveAuthentication::AUTHORIZATION_EMITTED;

        return LivePendingOutput(std::move(output));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<LivePendingOutput, LiveError> LiveSession::describe(std::span<const std::int32_t> senderIds,
                                                                  LiveWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            return std::unexpected(Error::INVALID_STATE);
        }

        impl_->requireOpen();

        return impl_->writerOutput({}, {}, senderIds, work);
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<LivePendingOutput, LiveError> LiveSession::subscribe(SubscriptionMessage message,
                                                                   std::span<const core::SubscriptionRequest> requests,
                                                                   LiveWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            return std::unexpected(Error::INVALID_STATE);
        }

        impl_->requireOpen();
        const auto id = static_cast<std::int32_t>(message);
        const auto add = id == 11 || id == 12 ? 11 : id == 16 || id == 17 ? 16 : id == 21 || id == 22 ? 21 : -1;

        if (add == -1) {
            throw Error::INVALID_CONFIGURATION;
        }

        debit(product(impl_->peer.state_->protocol.receive.size(), 2), work.visits);

        if (!impl_->peer.state_->protocol.received || !capability(impl_->peer.state_->protocol.receive, add, work) ||
            !capability(impl_->peer.state_->protocol.receive, id, work)) {
            throw Error::NOT_READY;
        }

        if (impl_->authentication == LiveAuthentication::REQUIRED ||
            impl_->authentication == LiveAuthentication::AUTHORIZATION_EMITTED) {
            throw Error::AUTHENTICATION_REQUIRED;
        }

        return impl_->writerOutput(message, requests, {}, work);
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const LiveStorageLimit &) {
        return std::unexpected(Error::RESOURCE_LIMIT);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

LiveSnapshot LiveSession::peer() const noexcept {
    return impl_ ? impl_->peer : LiveSnapshot{};
}

LiveAuthentication LiveSession::authentication() const noexcept {
    return impl_ ? impl_->authentication : LiveAuthentication::UNCHALLENGED;
}

bool LiveSession::historySnapshotEnabled() const noexcept {
    return impl_ && impl_->historySnapshot;
}

LiveDiagnostic LiveSession::diagnostic() const noexcept {
    return impl_ ? impl_->diagnostic : LiveDiagnostic{Error::INVALID_STATE};
}

std::size_t LiveSession::residentBytes() const noexcept {
    return impl_ ? impl_->lease.get()->resident() : 0;
}

std::size_t LiveSession::peakBytes() const noexcept {
    return impl_ ? impl_->lease.get()->peak() : 0;
}

} // namespace org::ttldtor::qdcxx::protocol
