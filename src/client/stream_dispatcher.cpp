// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/client/stream_dispatcher.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <exception>
#include <limits>
#include <memory_resource>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::client {
namespace {
using Error = ClientError;
constexpr unsigned NO_STOP = 0, GRACEFUL_STOP = 1, ABORT_STOP = 2;
constexpr std::int32_t STREAM_DATA = 15;
constexpr std::size_t PUBLIC_CLOSED = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
constexpr std::size_t PUBLIC_COUNT = PUBLIC_CLOSED - 1;

/** Charge before work without refunding prior successful charges.
 * @param amount Conservative requested units.
 * @param remaining Mutable caller turn allowance.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        throw Error::WORK_LIMIT;
    }

    remaining -= amount;
}

/** Reject unrepresentable backing/count arithmetic.
 * @param left Existing count.
 * @param right Additional count.
 * @return Exact sum, or INVALID_CONFIGURATION.
 */
std::size_t sum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw Error::INVALID_CONFIGURATION;
    }

    return left + right;
}

/** Reject unrepresentable array request arithmetic.
 * @param count Requested object count.
 * @param bytes One object's size or a second fixed-grid dimension; zero yields zero.
 * @return Exact product, or INVALID_CONFIGURATION.
 */
std::size_t product(std::size_t count, std::size_t bytes) {
    if (bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes) {
        throw Error::INVALID_CONFIGURATION;
    }

    return count * bytes;
}

/** Retain fixed detached ownership without allowing reference-counter arithmetic to wrap.
 * All references correspond to distinct live owner objects; representability exhaustion cannot
 * create a valid additional noexcept C++ owner, and terminates before corrupting shared lifetime.
 * @param references Existing atomic owner count.
 */
void retainReference(std::atomic<std::size_t> &references) noexcept {
    auto current = references.load(std::memory_order_relaxed);

    for (;;) {
        if (current == std::numeric_limits<std::size_t>::max()) {
            std::terminate();
        }

        if (references.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) {
            return;
        }
    }
}

/** Advance diagnostic totals with explicit saturation.
 * @param value Counter to advance.
 * @param amount Known local increment.
 * @param saturated Shared diagnostic saturation flag.
 */
void accumulate(std::size_t &value, std::size_t amount, bool &saturated) noexcept {
    if (amount > std::numeric_limits<std::size_t>::max() - value) {
        value = std::numeric_limits<std::size_t>::max();
        saturated = true;
    } else {
        value += amount;
    }
}

/** Translate existing typed failures without losing the nested category in owner diagnostics.
 * @param error Existing collector rejection.
 * @return Corresponding upper-layer category.
 */
Error collectorFailure(collectors::StreamError error) noexcept {
    if (error == collectors::StreamError::WORK_LIMIT) {
        return Error::WORK_LIMIT;
    }

    if (error == collectors::StreamError::RESOURCE_LIMIT) {
        return Error::RESOURCE_LIMIT;
    }

    if (error == collectors::StreamError::ALLOCATION_FAILURE) {
        return Error::ALLOCATION_FAILURE;
    }

    return Error::COLLECTOR_FAILURE;
}
} // namespace

namespace detail {
/** Shared detached accounting/identity only; no pointer leads back to a dispatcher or callable. */
struct Budget {
    std::atomic<std::size_t> references{1}; ///< Fixed metadata owner count.
    std::atomic<std::size_t> batches{0};    ///< Whole live candidate/executing/caller-retained allocations.
    std::size_t fixedBytes;    ///< Conservative fixed reservation retained through the last detached identity/result.
    std::size_t batchBytes;    ///< Exact Batch object plus arena request per distinct batch.
    std::size_t batchCapacity; ///< Simultaneous owner ceiling after all independent byte/count limits.

    /** Capture checked immutable reservations.
     * @param fixed Full client fixed reservation, including this object.
     * @param bytes One whole detached batch allocation scope.
     * @param capacity Effective finite simultaneous batch bound.
     */
    Budget(std::size_t fixed, std::size_t bytes, std::size_t capacity) noexcept
        : fixedBytes(fixed), batchBytes(bytes), batchCapacity(capacity) {
    }
};

/** Release only detached metadata.
 * @param budget Existing independently retained budget, or null.
 */
void release(Budget *budget) noexcept {
    if (budget && budget->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete budget;
    }
}

/** Fixed metadata lease, destroyed after later members in each owner. */
struct BudgetLease {
    Budget *budget = nullptr;

    /** Retain an existing budget.
     * @param value Existing detached metadata; null creates an empty lease.
     */
    explicit BudgetLease(Budget *value = nullptr) noexcept : budget(value) {
        if (budget) {
            retainReference(budget->references);
        }
    }

    /** Transfer a lease without allocating.
     * @param other Source, made empty.
     */
    BudgetLease(BudgetLease &&other) noexcept : budget(std::exchange(other.budget, nullptr)) {
    }

    /** Release detached metadata on any thread. */
    ~BudgetLease() {
        release(budget);
    }
};

/** Reservation guard transferred into a batch before its arena construction. */
struct BatchReservation {
    Budget *budget;

    /** Reserve one whole owner with no transient overshoot.
     * @param value Detached capacity ledger.
     */
    explicit BatchReservation(Budget *value) : budget(value) {
        auto current = budget->batches.load(std::memory_order_relaxed);

        for (;;) {
            if (current == budget->batchCapacity) {
                throw Error::RESOURCE_LIMIT;
            }

            if (budget->batches.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel)) {
                break;
            }
        }

        retainReference(budget->references);
    }

    /** Transfer reservation through potentially failing object/arena construction.
     * @param other Source guard, made empty.
     */
    BatchReservation(BatchReservation &&other) noexcept : budget(std::exchange(other.budget, nullptr)) {
    }

    /** Return batch credits after every payload member has been released. */
    ~BatchReservation() {
        if (budget) {
            const auto previous = budget->batches.fetch_sub(1, std::memory_order_acq_rel);
            assert(previous != 0);
            release(budget);
        }
    }
};

/** Distinguish fixed arena exhaustion from an actual allocator failure. */
struct ArenaLimit {};

/** Monotonic arenas may never fall back to an unbounded global allocation. */
class ExhaustedResource final : public std::pmr::memory_resource {
    void *do_allocate(std::size_t bytes, std::size_t alignment) override {
        static_cast<void>(bytes);
        static_cast<void>(alignment);
        throw ArenaLimit{};
    }

    void do_deallocate(void *pointer, std::size_t bytes, std::size_t alignment) override {
        static_cast<void>(pointer);
        static_cast<void>(bytes);
        static_cast<void>(alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
        return this == &other;
    }
};

/** Complete immutable detached batch; all final destruction is producer-safe. */
struct Batch {
    BatchReservation reservation;                          ///< Released last, after payload and full arena.
    std::atomic<std::size_t> references{1};                ///< Immutable owning leases.
    std::unique_ptr<std::byte[]> backing;                  ///< Complete fixed requested arena.
    ExhaustedResource exhausted;                           ///< No fallback allocations.
    std::pmr::monotonic_buffer_resource arena;             ///< Sole value/text allocator.
    std::pmr::vector<BatchRecord> records;                 ///< Stable exact records.
    std::pmr::vector<protocol::LiveFieldValueView> fields; ///< Stable flattened typed field views.
    std::pmr::vector<std::int32_t> legacy;                 ///< Stable flattened legacy values.

    /** Construct arena and empty allocator-aware containers after reservation.
     * @param lease Already reserved owner, transferred before fallible backing allocation.
     * @param bytes Full arena request.
     */
    Batch(BatchReservation lease, std::size_t bytes)
        : reservation(std::move(lease)), backing(new std::byte[bytes]), arena(backing.get(), bytes, &exhausted),
          records(&arena), fields(&arena), legacy(&arena) {
    }
};

/** Release only immutable detached backing/atomic credits.
 * @param batch Existing batch or null.
 */
void release(Batch *batch) noexcept {
    if (batch && batch->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete batch;
    }
}

/** Independently allocated result; completion leases never retain the command port. */
struct Completion {
    BudgetLease budget;                     ///< Retains fixed accounting only, never owner progress.
    std::atomic<std::size_t> references{1}; ///< Pool plus caller ticket leases.
    std::atomic<bool> ready{true};          ///< Release/acquire publication; false prevents pending-slot reuse.
    std::uint64_t id = 0; ///< Immutable through every external ticket lifetime, separate from value publication.
    CommandResult value;  ///< Written only before ready becomes true; immutable while any ticket survives.

    /** Retain one fixed detached accounting domain.
     * @param ledger Existing client metadata.
     */
    explicit Completion(Budget *ledger) noexcept : budget(ledger) {
    }
};

/** Release detached result metadata on any thread.
 * @param result Existing independently referenced result or null.
 */
void release(Completion *result) noexcept {
    if (result && result->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete result;
    }
}

/** Exact ordinary desired mutation kind, separate from reserved stop controls. */
enum class CommandKind { ADD, REMOVE, REPLACE, CLEAR };
/** Fixed slot lifecycle; producer never touches an owner-executing slot. */
enum class SlotState { FREE, QUEUED, EXECUTING };

/** Copied command payload; no protocol or callable ownership occurs here. */
struct CommandSlot {
    SlotState state = SlotState::FREE;
    CommandKind kind = CommandKind::CLEAR;
    std::size_t count = 0;
    Completion *completion = nullptr;
    std::unique_ptr<core::SubscriptionRequest[]> requests;
    std::unique_ptr<core::SubscriptionKey[]> keys;
    std::unique_ptr<char16_t[]> symbols;

    /** Release copied immutable contexts after application/cancellation, without user code. */
    void clear() noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            if (kind == CommandKind::REMOVE) {
                keys[i] = {};
            } else {
                requests[i] = {};
            }
        }

        count = 0;
        completion = nullptr;
        state = SlotState::FREE;
    }
};

/** Pure thread-safe command/control storage; cannot reach a callback or serialized owner. */
struct Port {
    BudgetLease budget;                     ///< Last member destroyed logically, after arrays/results.
    std::atomic<std::size_t> references{1}; ///< Owner plus public handles.
    std::atomic<std::size_t> publicOwnership{
        0};                                 ///< Count plus closed bit; fresh acquisition/last release share one CAS.
    std::atomic<unsigned> stop{NO_STOP};    ///< Allocation-free monotonic control escalation.
    std::atomic<bool> terminalReady{false}; ///< Immutable terminal release/acquire publication.
    std::atomic<bool> gapReady{false};      ///< Immutable first gap publication.
    DispatchTerminal terminal{};
    DispatchGap gap{}; ///< Reserved independent control slot, never consumed by data.
    std::mutex mutex;  ///< No user callable or protocol operation runs while held.
    ClientLimits limits;
    std::unique_ptr<CommandSlot[]> commands;
    std::unique_ptr<std::size_t[]> queue;
    std::unique_ptr<Completion *[]> results;
    std::size_t initializedResults = 0;
    std::size_t head = 0;
    std::size_t queued = 0;
    std::uint64_t lastId = 0;

    /** Prepare all fixed command/result backing before publication.
     * @param ledger Detached accounting identity.
     * @param bounds Checked immutable capacities.
     */
    Port(Budget *ledger, ClientLimits bounds) : budget(ledger), limits(bounds) {
        try {
            if (bounds.commandSlots != 0) {
                commands = std::make_unique<CommandSlot[]>(bounds.commandSlots);
                queue = std::make_unique<std::size_t[]>(bounds.commandSlots);
            }

            if (bounds.resultSlots != 0) {
                results = std::make_unique<Completion *[]>(bounds.resultSlots);
            }

            for (std::size_t i = 0; i < bounds.commandSlots; ++i) {
                if (bounds.commandEntries != 0) {
                    commands[i].requests = std::make_unique<core::SubscriptionRequest[]>(bounds.commandEntries);
                    commands[i].keys = std::make_unique<core::SubscriptionKey[]>(bounds.commandEntries);
                }

                if (bounds.commandSymbolUnits != 0) {
                    commands[i].symbols = std::make_unique<char16_t[]>(bounds.commandSymbolUnits);
                }
            }

            for (; initializedResults < bounds.resultSlots; ++initializedResults) {
                results[initializedResults] = new Completion(ledger);
            }
        } catch (...) {
            for (std::size_t i = 0; i < initializedResults; ++i) {
                release(results[i]);
            }

            throw;
        }
    }

    /** Release only producer-safe command/result/core metadata; owner resolved commands first. */
    ~Port() {
        for (std::size_t i = 0; i < initializedResults; ++i) {
            release(results[i]);
        }
    }

    /** Request idempotent stop without allocation or ordinary queue dependence.
     * @param requested GRACEFUL_STOP or ABORT_STOP.
     */
    void request(unsigned requested) noexcept {
        if (terminalReady.load(std::memory_order_acquire)) {
            return;
        }

        publicOwnership.fetch_or(PUBLIC_CLOSED, std::memory_order_acq_rel);
        auto current = stop.load(std::memory_order_relaxed);

        while (current < requested &&
               !stop.compare_exchange_weak(current, requested, std::memory_order_release, std::memory_order_relaxed)) {
        }
    }

    /** Inspect the same admission/retirement bit used by fresh public acquisition.
     * @return True once any stop or last-public-release retirement has linearized.
     */
    [[nodiscard]] bool isClosed() const noexcept {
        return (publicOwnership.load(std::memory_order_acquire) & PUBLIC_CLOSED) != 0;
    }

    /** Linearize a fresh public acquisition or a copy of an already existing handle.
     * @param fresh Fresh owner acquisition must reject the closed/retired bit; existing copies remain queryable.
     * @return True after retaining one public/port reference; false only for a refused fresh acquisition.
     */
    [[nodiscard]] bool acquirePublic(bool fresh) noexcept {
        auto current = publicOwnership.load(std::memory_order_acquire);

        for (;;) {
            if (fresh && (current & PUBLIC_CLOSED) != 0) {
                return false;
            }

            const auto count = current & PUBLIC_COUNT;

            if ((!fresh && count == 0) || count == PUBLIC_COUNT) {
                std::terminate();
            }

            if (publicOwnership.compare_exchange_strong(current, current + 1, std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                retainReference(references);

                return true;
            }
        }
    }
};

/** Release pure port metadata only; a live owner keeps its independent reference.
 * @param port Existing independently referenced port or null.
 */
void release(Port *port) noexcept {
    if (port && port->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete port;
    }
}

/** Release one public ownership reference and request abort on the last public release.
 * @param port Existing public port reference or null.
 */
void releasePublic(Port *port) noexcept {
    if (port) {
        auto current = port->publicOwnership.load(std::memory_order_acquire);
        std::size_t count = 0;

        for (;;) {
            count = current & PUBLIC_COUNT;
            assert(count != 0);
            const auto next = count == 1 ? PUBLIC_CLOSED : current - 1;

            if (port->publicOwnership.compare_exchange_strong(current, next, std::memory_order_acq_rel,
                                                              std::memory_order_acquire)) {
                break;
            }
        }

        if (count == 1) {
            port->request(ABORT_STOP);
        }

        release(port);
    }
}
} // namespace detail

CommandTicket::CommandTicket(detail::Completion *state) noexcept : state_(state) {
}

CommandTicket::CommandTicket(const CommandTicket &other) noexcept : state_(other.state_) {
    if (state_) {
        retainReference(state_->references);
    }
}

CommandTicket::CommandTicket(CommandTicket &&other) noexcept : state_(std::exchange(other.state_, nullptr)) {
}

CommandTicket &CommandTicket::operator=(const CommandTicket &other) noexcept {
    if (this != &other) {
        auto *next = other.state_;

        if (next) {
            retainReference(next->references);
        }

        detail::release(state_);
        state_ = next;
    }

    return *this;
}

CommandTicket &CommandTicket::operator=(CommandTicket &&other) noexcept {
    if (this != &other) {
        detail::release(state_);
        state_ = std::exchange(other.state_, nullptr);
    }

    return *this;
}

CommandTicket::~CommandTicket() {
    detail::release(state_);
}

std::optional<CommandResult> CommandTicket::result() const noexcept {
    return state_ && state_->ready.load(std::memory_order_acquire) ? std::optional(state_->value) : std::nullopt;
}

std::uint64_t CommandTicket::id() const noexcept {
    return state_ ? state_->id : 0;
}

ClientIdentity::ClientIdentity(detail::Budget *budget, bool epoch) noexcept : budget_(budget), epoch_(epoch) {
    if (budget_) {
        retainReference(budget_->references);
    }
}

ClientIdentity::ClientIdentity(const ClientIdentity &other) noexcept : ClientIdentity(other.budget_, other.epoch_) {
}

ClientIdentity::ClientIdentity(ClientIdentity &&other) noexcept
    : budget_(std::exchange(other.budget_, nullptr)), epoch_(other.epoch_) {
}

ClientIdentity &ClientIdentity::operator=(const ClientIdentity &other) noexcept {
    if (this != &other) {
        auto *next = other.budget_;

        if (next) {
            retainReference(next->references);
        }

        detail::release(budget_);
        budget_ = next;
        epoch_ = other.epoch_;
    }

    return *this;
}

ClientIdentity &ClientIdentity::operator=(ClientIdentity &&other) noexcept {
    if (this != &other) {
        detail::release(budget_);
        budget_ = std::exchange(other.budget_, nullptr);
        epoch_ = other.epoch_;
    }

    return *this;
}

ClientIdentity::~ClientIdentity() {
    detail::release(budget_);
}

bool ClientIdentity::matches(const ClientIdentity &other) const noexcept {
    return budget_ && budget_ == other.budget_ && epoch_ == other.epoch_;
}

OwnedBatch::OwnedBatch(detail::Batch *state) noexcept : state_(state) {
}

OwnedBatch::OwnedBatch(const OwnedBatch &other) noexcept : state_(other.state_) {
    if (state_) {
        retainReference(state_->references);
    }
}

OwnedBatch::OwnedBatch(OwnedBatch &&other) noexcept : state_(std::exchange(other.state_, nullptr)) {
}

OwnedBatch &OwnedBatch::operator=(const OwnedBatch &other) noexcept {
    if (this != &other) {
        auto *next = other.state_;

        if (next) {
            retainReference(next->references);
        }

        detail::release(state_);
        state_ = next;
    }

    return *this;
}

OwnedBatch &OwnedBatch::operator=(OwnedBatch &&other) noexcept {
    if (this != &other) {
        detail::release(state_);
        state_ = std::exchange(other.state_, nullptr);
    }

    return *this;
}

OwnedBatch::~OwnedBatch() {
    detail::release(state_);
}

std::span<const BatchRecord> OwnedBatch::records() const noexcept {
    return state_ ? std::span<const BatchRecord>(state_->records) : std::span<const BatchRecord>{};
}

ClientIdentity OwnedBatch::runIdentity() const noexcept {
    return ClientIdentity(state_ ? state_->reservation.budget : nullptr, false);
}

ClientIdentity OwnedBatch::epochIdentity() const noexcept {
    return ClientIdentity(state_ ? state_->reservation.budget : nullptr, true);
}

BorrowedBatch::BorrowedBatch(detail::Batch *state) noexcept : state_(state) {
}

std::span<const BatchRecord> BorrowedBatch::records() const noexcept {
    return state_->records;
}

OwnedBatch BorrowedBatch::retain() const noexcept {
    retainReference(state_->references);

    return OwnedBatch(state_);
}

ClientHandle::ClientHandle(detail::Port *port) noexcept : port_(port) {
}

ClientHandle::ClientHandle(const ClientHandle &other) noexcept : port_(other.port_) {
    if (port_) {
        static_cast<void>(port_->acquirePublic(false));
    }
}

ClientHandle::ClientHandle(ClientHandle &&other) noexcept : port_(std::exchange(other.port_, nullptr)) {
}

ClientHandle &ClientHandle::operator=(const ClientHandle &other) noexcept {
    if (this != &other) {
        auto *next = other.port_;

        if (next) {
            static_cast<void>(next->acquirePublic(false));
        }

        detail::releasePublic(port_);
        port_ = next;
    }

    return *this;
}

ClientHandle &ClientHandle::operator=(ClientHandle &&other) noexcept {
    if (this != &other) {
        detail::releasePublic(port_);
        port_ = std::exchange(other.port_, nullptr);
    }

    return *this;
}

ClientHandle::~ClientHandle() {
    detail::releasePublic(port_);
}

std::expected<CommandTicket, ClientError> ClientHandle::submit(detail::CommandKind kind,
                                                               std::span<const core::SubscriptionRequest> requests,
                                                               std::span<const core::SubscriptionKey> keys,
                                                               SubmitWork &work) const {
    try {
        debit(1, work.operations);

        if (!port_) {
            throw Error::INVALID_STATE;
        }

        const auto count = kind == detail::CommandKind::REMOVE ? keys.size() : requests.size();

        if (count > port_->limits.commandEntries) {
            throw Error::RESOURCE_LIMIT;
        }

        debit(port_->limits.commandSlots, work.visits);
        debit(port_->limits.resultSlots, work.visits);
        debit(count, work.visits);
        debit(count, work.visits);
        std::size_t textUnits = 0;

        for (std::size_t i = 0; i < count; ++i) {
            const auto symbol = kind == detail::CommandKind::REMOVE ? keys[i].symbol : requests[i].key.symbol;

            if ((!symbol.data() && !symbol.empty()) || symbol.size() > port_->limits.commandSymbolUnits - textUnits) {
                throw Error::RESOURCE_LIMIT;
            }

            debit(symbol.size(), work.textUnits);
            debit(symbol.size(), work.textUnits);
            textUnits += symbol.size();
        }

        std::lock_guard lock(port_->mutex);

        if (port_->isClosed()) {
            throw Error::CLOSING;
        }

        detail::CommandSlot *slot = nullptr;
        std::size_t slotIndex = 0;

        for (; slotIndex < port_->limits.commandSlots; ++slotIndex) {
            if (port_->commands[slotIndex].state == detail::SlotState::FREE) {
                slot = &port_->commands[slotIndex];
                break;
            }
        }

        if (!slot) {
            throw Error::COMMAND_LIMIT;
        }

        detail::Completion *result = nullptr;

        for (std::size_t i = 0; i < port_->limits.resultSlots; ++i) {
            auto *candidate = port_->results[i];

            if (candidate->ready.load(std::memory_order_acquire) &&
                candidate->references.load(std::memory_order_acquire) == 1) {
                result = candidate;
                break;
            }
        }

        if (!result) {
            throw Error::RESULT_LIMIT;
        }

        if (port_->lastId == std::numeric_limits<std::uint64_t>::max()) {
            throw Error::IDENTITY_EXHAUSTED;
        }

        slot->kind = kind;
        slot->count = count;
        std::size_t offset = 0;

        for (std::size_t i = 0; i < count; ++i) {
            const auto &key = kind == detail::CommandKind::REMOVE ? keys[i] : requests[i].key;
            std::u16string_view symbol;

            if (key.symbol.data()) {
                if (!key.symbol.empty()) {
                    std::copy(key.symbol.begin(), key.symbol.end(), slot->symbols.get() + offset);
                    symbol = {slot->symbols.get() + offset, key.symbol.size()};
                } else {
                    symbol = u"";
                }
            }

            if (kind == detail::CommandKind::REMOVE) {
                slot->keys[i] = {key.context, symbol, key.selector};
            } else {
                slot->requests[i] = {{key.context, symbol, key.selector}, requests[i].historyBound};
            }

            offset += key.symbol.size();
        }

        // The final gate is submission's linearization point relative to an atomic stop request.
        if (port_->isClosed()) {
            slot->clear();
            throw Error::CLOSING;
        }

        result->ready.store(false, std::memory_order_relaxed);
        result->id = ++port_->lastId;
        retainReference(result->references);
        slot->completion = result;
        slot->state = detail::SlotState::QUEUED;
        const auto suffix = port_->limits.commandSlots - port_->head;
        const auto tail = port_->queued < suffix ? port_->head + port_->queued : port_->queued - suffix;
        port_->queue[tail] = slotIndex;
        ++port_->queued;

        return CommandTicket(result);
    } catch (Error error) {
        return std::unexpected(error);
    }
}

std::expected<CommandTicket, ClientError> ClientHandle::add(std::span<const core::SubscriptionRequest> requests,
                                                            SubmitWork &work) const {
    return submit(detail::CommandKind::ADD, requests, {}, work);
}

std::expected<CommandTicket, ClientError> ClientHandle::remove(std::span<const core::SubscriptionKey> keys,
                                                               SubmitWork &work) const {
    return submit(detail::CommandKind::REMOVE, {}, keys, work);
}

std::expected<CommandTicket, ClientError> ClientHandle::replace(std::span<const core::SubscriptionRequest> requests,
                                                                SubmitWork &work) const {
    return submit(detail::CommandKind::REPLACE, requests, {}, work);
}

std::expected<CommandTicket, ClientError> ClientHandle::clear(SubmitWork &work) const {
    return submit(detail::CommandKind::CLEAR, {}, {}, work);
}

std::expected<void, ClientError> ClientHandle::requestStop(StopMode mode) const noexcept {
    if (!port_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (mode != StopMode::GRACEFUL && mode != StopMode::ABORT) {
        return std::unexpected(Error::INVALID_CONFIGURATION);
    }

    port_->request(mode == StopMode::ABORT ? ABORT_STOP : GRACEFUL_STOP);

    return {};
}

std::optional<DispatchTerminal> ClientHandle::terminal() const noexcept {
    return port_ && port_->terminalReady.load(std::memory_order_acquire) ? std::optional(port_->terminal)
                                                                         : std::nullopt;
}

std::optional<DispatchGap> ClientHandle::gap() const noexcept {
    return port_ && port_->gapReady.load(std::memory_order_acquire) ? std::optional(port_->gap) : std::nullopt;
}

namespace {
/** Bound one aggregate before increment, without allowing size arithmetic to wrap.
 * @param current Accumulated units.
 * @param extra Additional units.
 * @param maximum Explicit immutable limit.
 */
void bound(std::size_t &current, std::size_t extra, std::size_t maximum) {
    if (extra > maximum - current) {
        throw Error::RESOURCE_LIMIT;
    }

    current += extra;
}

/** Exact detached candidate dimensions discovered before any payload allocation. */
struct BatchSize {
    std::size_t typed = 0;
    std::size_t legacy = 0;
    std::size_t fields = 0;
    std::size_t symbols = 0;
    std::size_t units = 0;
    std::size_t bytes = 0;
};

/** Validate/count/cost the entire candidate, including its failure disposal, before copying.
 * @param offer Collector-owned source views, never acknowledged here.
 * @param limits Immutable detached limits.
 * @param budget Fixed detached reservation size.
 * @param work Mutable explicit owner credits.
 * @return Complete checked allocation dimensions.
 */
BatchSize measure(std::span<const collectors::StreamDelivery> offer, ClientLimits limits, const detail::Budget &budget,
                  DispatchWork &work) {
    BatchSize size;
    debit(offer.size(), work.records);
    debit(offer.size(), work.records);
    debit(budget.batchBytes, work.bytes);
    debit(budget.batchBytes, work.bytes);

    for (const auto &delivery : offer) {
        const auto &record = *delivery.record;
        bound(size.symbols, record.symbol.size(), limits.batchSymbolUnits);
        debit(record.symbol.size(), work.textUnits);
        debit(record.symbol.size(), work.textUnits);
        bound(size.fields, record.typedValues.size(), limits.batchFields);
        bound(size.fields, record.values.size(), limits.batchFields);
        size.typed += record.typedValues.size();
        size.legacy += record.values.size();
        debit(record.typedValues.size(), work.fields);
        debit(record.typedValues.size(), work.fields);
        debit(record.values.size(), work.fields);
        debit(record.values.size(), work.fields);

        for (const auto &field : record.typedValues) {
            if (field.units) {
                bound(size.units, field.units->size(), limits.batchFieldUnits);
                debit(field.units->size(), work.textUnits);
                debit(field.units->size(), work.textUnits);
            }

            if (field.bytes) {
                bound(size.bytes, field.bytes->size(), limits.batchByteUnits);
                debit(field.bytes->size(), work.bytes);
                debit(field.bytes->size(), work.bytes);
            }
        }
    }

    return size;
}

/** Copy exact non-null UTF-16 into a fixed arena.
 * @param arena Candidate resource with no fallback.
 * @param source Borrowed admitted text.
 * @return Stable exact text, with a non-null empty representation.
 */
std::u16string_view copyText(std::pmr::memory_resource &arena, std::u16string_view source) {
    if (source.empty()) {
        return u"";
    }

    auto *target = std::pmr::polymorphic_allocator<char16_t>(&arena).allocate(source.size());
    std::copy(source.begin(), source.end(), target);

    return {target, source.size()};
}

/** Copy exact opaque bytes into a fixed arena.
 * @param arena Candidate resource with no fallback.
 * @param source Borrowed admitted payload.
 * @return Stable exact bytes; engagement/nullability is maintained by the caller's optional.
 */
std::span<const std::byte> copyBytes(std::pmr::memory_resource &arena, std::span<const std::byte> source) {
    if (source.empty()) {
        return {};
    }

    auto *target = std::pmr::polymorphic_allocator<std::byte>(&arena).allocate(source.size());
    std::copy(source.begin(), source.end(), target);

    return {target, source.size()};
}

/** Construct a complete event-independent batch without touching collector eligibility/acknowledgement.
 * @param offer Prepared source metadata from the actual collector.
 * @param limits Immutable fixed arena limits.
 * @param budget Detached reservation domain.
 * @param size Complete preflighted dimensions.
 * @return Candidate owner, released on any failure before collector acknowledgement.
 */
std::unique_ptr<detail::Batch> copyBatch(std::span<const collectors::StreamDelivery> offer, ClientLimits limits,
                                         detail::Budget *budget, BatchSize size) {
    detail::BatchReservation reservation(budget);
    auto batch = std::make_unique<detail::Batch>(std::move(reservation), limits.batchArenaBytes);
    batch->records.reserve(offer.size());
    batch->fields.reserve(size.typed);
    batch->legacy.reserve(size.legacy);

    for (const auto &delivery : offer) {
        const auto &source = *delivery.record;
        const auto firstTyped = batch->fields.size();
        const auto firstLegacy = batch->legacy.size();

        for (const auto &field : source.typedValues) {
            protocol::LiveFieldValueView value{field.type, field.raw, {}, {}};

            if (field.units) {
                value.units = copyText(batch->arena, *field.units);
            }

            if (field.bytes) {
                value.bytes = copyBytes(batch->arena, *field.bytes);
            }

            batch->fields.push_back(value);
        }

        batch->legacy.insert(batch->legacy.end(), source.values.begin(), source.values.end());
        const auto fields = source.typedValues.empty()
                                ? std::span<const protocol::LiveFieldValueView>{}
                                : std::span<const protocol::LiveFieldValueView>(batch->fields.data() + firstTyped,
                                                                                source.typedValues.size());
        const auto legacy = source.values.empty() ? std::span<const std::int32_t>{}
                                                  : std::span<const std::int32_t>(batch->legacy.data() + firstLegacy,
                                                                                  source.values.size());
        batch->records.push_back({source.localId, source.senderId, source.context, delivery.membership,
                                  delivery.revision, delivery.bindingGeneration, copyText(batch->arena, source.symbol),
                                  source.cipher, source.eventFlags, legacy, fields, source.removalTime,
                                  source.fieldTime});
    }

    return batch;
}
} // namespace

struct StreamDispatcher::Impl {
    /** Accepted owner position; completed zero selection is never staged/counted again. */
    enum class Pending { NONE, UNSTAGED, RELEASE_ONLY };
    detail::BudgetLease budget;
    detail::Port *port;
    std::optional<collectors::StreamCollector> collector;
    std::unique_ptr<core::RecordContext[]> contexts;
    std::size_t contextCount;
    std::unique_ptr<collectors::StreamDelivery[]> offers;
    ClientLimits limits;
    core::SubscriptionLimits desiredLimits;
    collectors::StreamLimits collectorLimits;
    std::shared_ptr<Consumer> consumer;
    protocol::LiveIdentity epoch; ///< Serialized lease; never placed in detached output or the command port.
    std::optional<protocol::LiveEvent> pending;
    Pending pendingKind = Pending::NONE;
    bool contextValidated = false;
    bool pinned = false;
    bool active = false;
    bool abort = false;
    bool unknownEligibility = false;
    DispatchState state = DispatchState::OPEN;
    std::uint64_t revision = 0;
    DispatchCounts counts;
    std::optional<ClientError> primary;
    std::optional<collectors::StreamError> nested;

    /** Adopt all fully allocated state; caller callback ownership is copied only here.
     * @param ledger Detached accounting metadata.
     * @param commands Pure producer-safe port ownership.
     * @param stream Existing real collector owner.
     * @param schemaContexts Fixed source provenance.
     * @param records Context count.
     * @param metadata Fixed caller-owned collector batch-offer slots.
     * @param bounds Client capacities.
     * @param desiredBounds Existing core limits.
     * @param streamBounds Existing collector limits.
     * @param callback Caller-created shared callback owner.
     */
    Impl(detail::Budget *ledger, detail::Port *commands, collectors::StreamCollector stream,
         std::unique_ptr<core::RecordContext[]> schemaContexts, std::size_t records,
         std::unique_ptr<collectors::StreamDelivery[]> metadata, ClientLimits bounds,
         core::SubscriptionLimits desiredBounds, collectors::StreamLimits streamBounds,
         const std::shared_ptr<Consumer> &callback) noexcept
        : budget(ledger), port(commands), collector(std::move(stream)), contexts(std::move(schemaContexts)),
          contextCount(records), offers(std::move(metadata)), limits(bounds), desiredLimits(desiredBounds),
          collectorLimits(streamBounds), consumer(callback) {
    }

    /** Report only pending records that have not already been classified/staged.
     * @return Exact raw pending record count, without inventing eligibility.
     */
    [[nodiscard]] std::size_t pendingRecords() const noexcept {
        return pending && pendingKind == Pending::UNSTAGED ? pending->records().size() : 0;
    }

    /** Publish the reserved first gap observation once; producer readers see immutable data.
     * @param cause Static local failure/abort category.
     */
    void gap(ClientError cause) noexcept {
        if (!port->gapReady.load(std::memory_order_relaxed)) {
            const auto pendingCount = pendingRecords();
            port->gap = {cause, collector ? collector->queuedRecords() : 0, pendingCount, pendingCount != 0};
            port->gapReady.store(true, std::memory_order_release);
        }
    }

    /** Preserve the first owner failure and request local bounded abort, never callback retry.
     * @param error Static upper category.
     * @param collectorError Optional existing nested category.
     */
    void fail(ClientError error, std::optional<collectors::StreamError> collectorError = {}) noexcept {
        if (!primary) {
            primary = error;
            nested = collectorError;
        }

        gap(error);
        port->request(ABORT_STOP);
        abort = true;
        state = DispatchState::CLOSING;
    }

    /** Finish an already detached command slot outside user/core mutation locks.
     * @param slot Executing fixed slot.
     * @param result Immutable completion to publish after payload disposal.
     */
    void complete(detail::CommandSlot &slot, CommandResult result) noexcept {
        auto *completion = slot.completion;
        {
            std::lock_guard lock(port->mutex);
            slot.clear();
        }
        completion->value = result;
        completion->ready.store(true, std::memory_order_release);
        accumulate(counts.completedCommands, 1, counts.saturated);
    }

    /** Detach at most one queued command without retaining a mutable-container iterator.
     * @param work Explicit owner action/disposal credits, preflighted before queue mutation.
     * @return Executing stable fixed slot, or null when the queue is empty.
     */
    detail::CommandSlot *takeCommand(DispatchWork &work) {
        std::lock_guard lock(port->mutex);

        if (port->queued == 0) {
            return nullptr;
        }

        auto &slot = port->commands[port->queue[port->head]];
        debit(1, work.commands);
        debit(slot.count, work.records);
        slot.state = detail::SlotState::EXECUTING;
        --port->queued;
        port->head = port->head + 1 == limits.commandSlots ? 0 : port->head + 1;

        return &slot;
    }

    /** Apply or cancel exactly one copied command; ordinary rejection is nonterminal.
     * @param slot Stable executing slot, inaccessible to producer mutation.
     */
    void cancel(detail::CommandSlot &slot) noexcept {
        complete(slot, {slot.completion->id, CommandStatus::CANCELLED_CLOSING, revision, false, {}});
    }

    /** Apply the real collector-owned desired command without a port lock.
     * @param slot Stable executing slot.
     * @param work Existing desired mutation credits.
     */
    void apply(detail::CommandSlot &slot, DispatchWork &work) {
        std::expected<core::SubscriptionChange, core::SubscriptionError> result =
            std::unexpected(core::SubscriptionError::INVALID_STATE);
        const auto requests = std::span<const core::SubscriptionRequest>(slot.requests.get(), slot.count);
        const auto keys = std::span<const core::SubscriptionKey>(slot.keys.get(), slot.count);

        switch (slot.kind) {
        case detail::CommandKind::ADD:
            result = collector->add(requests, work.desired);
            break;
        case detail::CommandKind::REMOVE:
            result = collector->remove(keys, work.desired);
            break;
        case detail::CommandKind::REPLACE:
            result = collector->replace(requests, work.desired);
            break;
        case detail::CommandKind::CLEAR:
            result = collector->clear(work.desired);
            break;
        }

        if (result) {
            revision = result->revision;
            complete(slot, {slot.completion->id, CommandStatus::APPLIED, revision, result->changed, {}});
        } else {
            complete(slot, {slot.completion->id, CommandStatus::REJECTED, revision, false, result.error()});
        }
    }

    /** Release an accepted event only after full current epoch disposal allowance is available.
     * @param work Explicit owner release credits.
     * @param discard Whether unclassified pending records are intentionally aborted.
     */
    void releasePending(DispatchWork &work, bool discard) {
        debit(1, work.records);
        debit(pending->releaseBytes(), work.bytes);

        if (discard && pendingRecords() != 0) {
            unknownEligibility = true;
            accumulate(counts.discardedPendingRecords, pendingRecords(), counts.saturated);
        }

        pending.reset();
        pendingKind = Pending::NONE;
        contextValidated = false;
    }

    /** Stage the accepted whole event before every ordinary command/callback.
     * @param work Explicit context validation/collector/release credits.
     * @return Exact bounded progress category; failures retain the accepted owner for cleanup/retry.
     */
    ProgressKind stagePending(DispatchWork &work) {
        if (pendingKind == Pending::UNSTAGED) {
            if (!contextValidated) {
                for (const auto &record : pending->records()) {
                    debit(1, work.records);
                    bool found = false;

                    for (std::size_t i = 0; i < contextCount; ++i) {
                        debit(1, work.records);
                        found = contexts[i].matches(record.context) || found;
                    }

                    if (!found) {
                        fail(Error::INVALID_CONTEXT);

                        return ProgressKind::CLOSING;
                    }
                }

                contextValidated = true;
            }

            auto staged = collector->stage(std::move(*pending), work.collector);

            if (!staged) {
                if (staged.error() == collectors::StreamError::WORK_LIMIT) {
                    throw Error::WORK_LIMIT;
                }

                fail(collectorFailure(staged.error()), staged.error());

                return ProgressKind::CLOSING;
            }

            accumulate(counts.stagedRecords, staged->selected, counts.saturated);
            accumulate(counts.ignoredRecords, staged->ignored, counts.saturated);
            pendingKind = Pending::RELEASE_ONLY;

            if (staged->selected != 0) {
                pending.reset(); // A moved-from event has no protocol backing left to release.
                pendingKind = Pending::NONE;
                contextValidated = false;

                return ProgressKind::STAGED;
            }
        }

        releasePending(work, false);

        return ProgressKind::RELEASED;
    }

    /** Prepare/copy/acknowledge one whole batch before invoking its protected consumer.
     * @param work Explicit record/value/arena/callback/collector credits.
     * @return Actual callback or withdrawal progress, never a partial callback batch.
     */
    ProgressKind dispatch(DispatchWork &work) {
        auto prepared = collector->prepareBatch({offers.get(), limits.batchRecords}, work.collector);

        if (!prepared) {
            throw collectorFailure(prepared.error());
        }

        if (prepared->empty()) {
            auto acknowledged = collector->acknowledgeBatch(work.collector);

            if (!acknowledged) {
                throw collectorFailure(acknowledged.error());
            }

            accumulate(counts.withdrawnRecords, acknowledged->withdrawn, counts.saturated);

            return ProgressKind::RELEASED;
        }

        debit(1, work.callbacks);
        const auto size = measure(*prepared, limits, *budget.budget, work);
        auto candidate = copyBatch(*prepared, limits, budget.budget, size);
        auto acknowledged = collector->acknowledgeBatch(work.collector);

        if (!acknowledged) {
            throw collectorFailure(acknowledged.error());
        }

        accumulate(counts.acknowledgedRecords, acknowledged->records, counts.saturated);
        accumulate(counts.withdrawnRecords, acknowledged->withdrawn, counts.saturated);
        OwnedBatch owner(candidate.release());
        const BorrowedBatch borrowed(owner.state_);
        const auto protectedConsumer = consumer;
        accumulate(counts.startedBatches, 1, counts.saturated);
        accumulate(counts.startedRecords, borrowed.records().size(), counts.saturated);
        try {
            protectedConsumer->onBatch(borrowed);
            accumulate(counts.completedBatches, 1, counts.saturated);
            accumulate(counts.completedRecords, borrowed.records().size(), counts.saturated);
        } catch (...) {
            accumulate(counts.failedBatches, 1, counts.saturated);
            accumulate(counts.failedRecords, borrowed.records().size(), counts.saturated);
            fail(Error::CALLBACK_FAILURE);
        }

        return ProgressKind::CALLBACK;
    }

    /** Publish immutable terminal only after consumer/protocol ownership has retired.
     * @param fallback Whether explicit destructor fallback forces aborted outcome absent a prior failure.
     */
    void publishTerminal(bool fallback) noexcept {
        consumer.reset(); // Caller-created destructor/user state is released outside every port lock.
        epoch = {};
        abort = abort || fallback || port->stop.load(std::memory_order_acquire) == ABORT_STOP;
        port->terminal = {primary ? DispatchOutcome::FAILED
                          : abort ? DispatchOutcome::ABORTED
                                  : DispatchOutcome::DRAINED,
                          primary,
                          nested,
                          counts,
                          revision,
                          unknownEligibility};
        state = DispatchState::TERMINAL;
        port->publicOwnership.fetch_or(PUBLIC_CLOSED, std::memory_order_acq_rel);
        port->terminalReady.store(true, std::memory_order_release);
    }

    /** Release fixed collector arrays/core tokens after their full bounded destructor work is reserved.
     * @param work Explicit owner disposal credits.
     */
    void retireCollector(DispatchWork &work) {
        debit(collectorLimits.frames, work.records);
        debit(collectorLimits.records, work.records);
        debit(contextCount, work.records);
        debit(collector->desired()->entries().size(), work.records);
        debit(collector->residentBytes(), work.bytes);
        collector.reset();
    }

    /** Bounded uncredited owner-only fallback; no callback is ever invoked here. */
    void fallback() noexcept {
        assert(!active);

        if (state == DispatchState::TERMINAL) {
            return;
        }

        port->request(ABORT_STOP);

        if (pendingRecords() != 0) {
            unknownEligibility = true;
            accumulate(counts.discardedPendingRecords, pendingRecords(), counts.saturated);
        }

        if (collector) {
            accumulate(counts.discardedRecords, collector->queuedRecords(), counts.saturated);
        }

        gap(primary.value_or(Error::ABORTED));
        pending.reset();
        pendingKind = Pending::NONE;
        collector.reset();
        {
            std::lock_guard lock(port->mutex);

            for (std::size_t i = 0; i < limits.commandSlots; ++i) {
                auto &slot = port->commands[i];

                if (slot.state != detail::SlotState::FREE) {
                    auto *completion = slot.completion;
                    const auto id = completion->id;
                    slot.clear();
                    completion->value = {id, CommandStatus::CANCELLED_CLOSING, revision, false, {}};
                    completion->ready.store(true, std::memory_order_release);
                    accumulate(counts.completedCommands, 1, counts.saturated);
                }
            }

            port->queued = 0;
        }
        publishTerminal(true);
    }

    /** Fallback releases serialized owners before the final pure port/budget references. */
    ~Impl() {
        fallback();
        detail::release(port);
    }
};

StreamDispatcher::StreamDispatcher(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

StreamDispatcher::StreamDispatcher(StreamDispatcher &&other) noexcept {
    assert(!other.impl_ || !other.impl_->active);
    impl_ = std::move(other.impl_);
}

StreamDispatcher &StreamDispatcher::operator=(StreamDispatcher &&other) noexcept {
    assert(!impl_ || !impl_->active);
    assert(!other.impl_ || !other.impl_->active);

    if (this != &other) {
        impl_ = std::move(other.impl_);
    }

    return *this;
}

StreamDispatcher::~StreamDispatcher() = default;

std::expected<StreamDispatcher, ClientError>
StreamDispatcher::create(const core::LocalSchema &schema, core::SubscriptionLimits desiredLimits,
                         collectors::StreamLimits collectorLimits, ClientLimits limits,
                         const std::shared_ptr<Consumer> &consumer, DispatchWork &work) {
    try {
        debit(1, work.operations);

        if (!consumer || limits.batchRecords == 0) {
            throw Error::INVALID_CONFIGURATION;
        }

        const auto perCommand =
            sum(sizeof(detail::CommandSlot) + sizeof(std::size_t),
                sum(product(limits.commandEntries, sizeof(core::SubscriptionRequest) + sizeof(core::SubscriptionKey)),
                    product(limits.commandSymbolUnits, sizeof(char16_t))));
        auto fixed = sum(sizeof(Impl), sum(sizeof(detail::Port), sizeof(detail::Budget)));
        fixed = sum(fixed, product(limits.commandSlots, perCommand));
        fixed = sum(fixed, product(limits.resultSlots, sizeof(detail::Completion) + sizeof(detail::Completion *)));
        fixed = sum(fixed, product(schema.records().size(), sizeof(core::RecordContext)));
        fixed = sum(fixed, product(limits.batchRecords, sizeof(collectors::StreamDelivery)));
        const auto batchBytes = sum(sizeof(detail::Batch), limits.batchArenaBytes);

        if (fixed > limits.residentBytes) {
            throw Error::RESOURCE_LIMIT;
        }

        const auto capacity = std::min(
            {limits.batchOwners, limits.batchResidentBytes / batchBytes, (limits.residentBytes - fixed) / batchBytes});
        debit(fixed, work.bytes);
        debit(fixed, work.bytes);
        debit(schema.records().size(), work.records);
        debit(limits.commandSlots, work.records);
        debit(product(limits.commandSlots, limits.commandEntries), work.records);
        debit(product(limits.commandSlots, limits.commandEntries), work.records);
        debit(limits.resultSlots, work.records);
        debit(limits.batchRecords, work.records);
        auto desired = core::SubscriptionState::create(schema, core::SubscriptionFamily::STREAM, desiredLimits);

        if (!desired) {
            throw desired.error() == core::SubscriptionError::ALLOCATION_FAILURE ? Error::ALLOCATION_FAILURE
                                                                                 : Error::RESOURCE_LIMIT;
        }

        auto stream = collectors::StreamCollector::create(std::move(*desired), collectorLimits, work.collector);

        if (!stream) {
            throw collectorFailure(stream.error());
        }

        auto contexts = schema.records().empty() ? std::unique_ptr<core::RecordContext[]>{}
                                                 : std::make_unique<core::RecordContext[]>(schema.records().size());

        for (std::size_t i = 0; i < schema.records().size(); ++i) {
            contexts[i] = schema.recordContext(static_cast<std::int32_t>(i));
        }

        auto offers = std::make_unique<collectors::StreamDelivery[]>(limits.batchRecords);
        auto budget = std::unique_ptr<detail::Budget, void (*)(detail::Budget *)>(
            new detail::Budget(fixed, batchBytes, capacity), detail::release);
        auto port = std::unique_ptr<detail::Port, void (*)(detail::Port *)>(new detail::Port(budget.get(), limits),
                                                                            detail::release);
        auto impl = std::unique_ptr<Impl>(new Impl(budget.get(), port.get(), std::move(*stream), std::move(contexts),
                                                   schema.records().size(), std::move(offers), limits, desiredLimits,
                                                   collectorLimits, consumer));
        port.release();

        return StreamDispatcher(std::move(impl));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<ClientHandle, ClientError> StreamDispatcher::handle() const noexcept {
    if (!impl_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (!impl_->port->acquirePublic(true)) {
        return std::unexpected(Error::CLOSING);
    }

    return ClientHandle(impl_->port);
}

std::expected<void, ClientError> StreamDispatcher::admit(protocol::LiveEvent &&event) noexcept {
    if (!impl_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (impl_->active) {
        return std::unexpected(Error::REENTRANT);
    }

    if (impl_->port->isClosed()) {
        return std::unexpected(Error::CLOSING);
    }

    if (impl_->pending) {
        return std::unexpected(Error::BUSY);
    }

    const auto identity = event.snapshot().identity();

    if (!identity.sameEpoch(identity) ||
        (event.kind() == protocol::LiveEventKind::DATA && event.messageType() != STREAM_DATA)) {
        return std::unexpected(Error::INVALID_EVENT);
    }

    if (impl_->pinned && !impl_->epoch.sameEpoch(identity)) {
        return std::unexpected(Error::INVALID_EPOCH);
    }

    if (!impl_->pinned) {
        impl_->epoch = identity;
        impl_->pinned = true;
    }

    const auto data = event.kind() == protocol::LiveEventKind::DATA;
    accumulate(impl_->counts.acceptedFrames, 1, impl_->counts.saturated);
    accumulate(impl_->counts.acceptedRecords, event.records().size(), impl_->counts.saturated);
    impl_->pending.emplace(std::move(event));
    impl_->pendingKind = data ? Impl::Pending::UNSTAGED : Impl::Pending::RELEASE_ONLY;
    impl_->contextValidated = false;

    return {};
}

std::expected<DispatchProgress, ClientError> StreamDispatcher::progress(DispatchWork &work) {
    if (!impl_) {
        return std::unexpected(Error::INVALID_STATE);
    }

    if (impl_->active) {
        return std::unexpected(Error::REENTRANT);
    }

    struct Guard {
        bool &active;

        explicit Guard(bool &value) noexcept : active(value) {
            active = true;
        }

        ~Guard() {
            active = false;
        }
    } guard(impl_->active);

    try {
        debit(1, work.operations);

        if (impl_->state == DispatchState::TERMINAL) {
            return DispatchProgress{ProgressKind::TERMINAL, impl_->state};
        }

        const auto stop = impl_->port->stop.load(std::memory_order_acquire);

        if ((impl_->state == DispatchState::OPEN && stop != NO_STOP) || (!impl_->abort && stop == ABORT_STOP)) {
            impl_->state = DispatchState::CLOSING;
            impl_->abort = stop == ABORT_STOP;

            if (impl_->abort) {
                impl_->gap(impl_->primary.value_or(Error::ABORTED));
            }

            return DispatchProgress{ProgressKind::CLOSING, impl_->state};
        }

        if (impl_->pending) {
            if (impl_->abort) {
                impl_->releasePending(work, true);

                return DispatchProgress{ProgressKind::RELEASED, impl_->state};
            }

            const auto kind = impl_->stagePending(work);

            return DispatchProgress{kind, impl_->state};
        }

        if (auto *slot = impl_->takeCommand(work)) {
            if (impl_->state == DispatchState::CLOSING) {
                impl_->cancel(*slot);
            } else {
                impl_->apply(*slot, work);
            }

            return DispatchProgress{ProgressKind::COMMAND, impl_->state};
        }

        if (impl_->collector->queuedRecords() != 0) {
            if (impl_->abort) {
                auto discarded = impl_->collector->discard(work.collector);

                if (!discarded) {
                    throw collectorFailure(discarded.error());
                }

                accumulate(impl_->counts.discardedRecords, *discarded, impl_->counts.saturated);

                return DispatchProgress{ProgressKind::CLOSING, impl_->state};
            }

            const auto kind = impl_->dispatch(work);

            return DispatchProgress{kind, impl_->state};
        }

        if (impl_->state == DispatchState::CLOSING) {
            impl_->retireCollector(work);
            impl_->publishTerminal(false);

            return DispatchProgress{ProgressKind::TERMINAL, impl_->state};
        }

        return DispatchProgress{ProgressKind::IDLE, impl_->state};
    } catch (Error error) {
        if (error == Error::WORK_LIMIT) {
            return std::unexpected(error);
        }

        impl_->fail(error);

        return DispatchProgress{ProgressKind::CLOSING, impl_->state};
    } catch (const detail::ArenaLimit &) {
        impl_->fail(Error::RESOURCE_LIMIT);

        return DispatchProgress{ProgressKind::CLOSING, impl_->state};
    } catch (const std::bad_alloc &) {
        impl_->fail(Error::ALLOCATION_FAILURE);

        return DispatchProgress{ProgressKind::CLOSING, impl_->state};
    }
}

const core::SubscriptionState *StreamDispatcher::desired() const noexcept {
    return impl_ && impl_->collector ? impl_->collector->desired() : nullptr;
}

DispatchState StreamDispatcher::state() const noexcept {
    return impl_ ? impl_->state : DispatchState::TERMINAL;
}

std::optional<DispatchTerminal> StreamDispatcher::terminal() const noexcept {
    return impl_ && impl_->port->terminalReady.load(std::memory_order_acquire) ? std::optional(impl_->port->terminal)
                                                                               : std::nullopt;
}

ClientResources StreamDispatcher::resources() const noexcept {
    if (!impl_) {
        return {};
    }

    std::size_t queued = 0;
    std::size_t results = 0;
    {
        std::lock_guard lock(impl_->port->mutex);
        queued = impl_->port->queued;

        for (std::size_t i = 0; i < impl_->limits.resultSlots; ++i) {
            const auto *result = impl_->port->results[i];

            if (!result->ready.load(std::memory_order_acquire) ||
                result->references.load(std::memory_order_acquire) != 1) {
                ++results;
            }
        }
    }
    const auto *budget = impl_->budget.budget;
    const auto batches = budget->batches.load(std::memory_order_acquire);
    const auto staged = impl_->collector ? impl_->collector->stagedBytes() : 0;
    const auto fixed = impl_->collector ? impl_->collector->residentBytes() - staged : 0;
    const auto pending = impl_->pending ? impl_->pending->residentBytes() : 0;

    return {budget->fixedBytes,
            batches,
            batches * budget->batchBytes,
            queued,
            results,
            impl_->collector ? impl_->collector->queuedRecords() : 0,
            impl_->pendingRecords(),
            fixed,
            staged + pending,
            impl_->collector ? impl_->collector->desired()->entries().size() : 0};
}

DispatchCounts StreamDispatcher::counts() const noexcept {
    return impl_ ? impl_->counts : DispatchCounts{};
}

} // namespace org::ttldtor::qdcxx::client
