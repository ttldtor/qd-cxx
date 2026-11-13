// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/mux/interest.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::mux {
namespace detail {
/** Fixed opaque identity, retained independently of client keys and schema declarations. */
struct Identity {};
} // namespace detail

namespace {
/** Debit a finite credit without refund or underflow.
 * @param credit Remaining units, decreased only when sufficient.
 * @param count Required units.
 * @return Whether the complete charge succeeded.
 */
bool charge(std::size_t &credit, std::size_t count) noexcept {
    if (count > credit) {
        return false;
    }

    credit -= count;

    return true;
}

/** Accumulate representable requested backing/work units.
 * @param total Running total.
 * @param count Number of equal units.
 * @param width Size of each unit.
 * @return Whether multiplication and addition were representable.
 */
bool accumulate(std::size_t &total, std::size_t count, std::size_t width = 1) noexcept {
    const auto maximum = std::numeric_limits<std::size_t>::max();

    if (width != 0 && count > (maximum - total) / width) {
        return false;
    }

    total += count * width;

    return true;
}

/** Compare validated exact keys without normalization or schema-name fallback.
 * @param left First key.
 * @param right Second key.
 * @return Exact provenance and UTF-16-unit equality.
 */
bool sameKey(const core::SubscriptionKey &left, const core::SubscriptionKey &right) noexcept {
    return left.selector == right.selector && left.context.matches(right.context) && left.symbol == right.symbol;
}
} // namespace

/** Complete fixed reservation; one bank is published and one is private scratch. */
struct MuxState::State {
    /** Captured fixed provenance and explicit indexed-layout fact. */
    struct Record {
        core::RecordContext context; ///< Owned opaque schema record provenance.
        bool indexed = false;        ///< Explicit supported indexed layout was present.
    };

    /** Current non-reused activation in one fixed client slot. */
    struct Client {
        std::uint64_t activation = 0; ///< Zero means inactive.
    };

    /** Constant-time contiguous desired-map location in one bank. */
    struct Range {
        std::size_t offset = 0; ///< First entry slot for this client.
        std::size_t count = 0;  ///< Number of desired entries for this client.
    };

    /** Complete candidate/published backing, including a non-null empty-symbol sentinel. */
    struct Bank {
        std::unique_ptr<Range[]> ranges;                        ///< One stored range per fixed client slot.
        std::unique_ptr<MuxEntry[]> entries;                    ///< Client entries, contiguous by client slot.
        std::unique_ptr<core::SubscriptionRequest[]> aggregate; ///< Exact union, borrowing this bank's text.
        std::unique_ptr<char16_t[]> symbols;                    ///< Packed copied client symbols.
        std::size_t entryCount = 0;                             ///< Used client-entry slots.
        std::size_t aggregateCount = 0;                         ///< Used distinct aggregate slots.
        std::size_t symbolCount = 0;                            ///< Used packed UTF-16 units.
        char16_t emptySymbol = 0;                               ///< Stable inline address for a zero-unit exact symbol.
    };

    core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY; ///< Immutable profile.
    MuxLimits limits{};                                                 ///< Immutable finite capacities.
    core::SubscriptionFamily family = core::SubscriptionFamily::STREAM; ///< Fixed owner contract.
    std::shared_ptr<const detail::Identity> identity;                   ///< Independent fixed owner identity.
    std::unique_ptr<Record[]> records; ///< Captured provenance, without schema declarations.
    std::size_t recordCount = 0;       ///< Captured metadata count.
    std::unique_ptr<Client[]> clients; ///< Fixed client activations.
    Bank banks[2];                     ///< Both complete banks remain reserved throughout owner lifetime.
    std::size_t published = 0;         ///< Current bank index.
    std::size_t storageBytes = 0;      ///< Complete cached requested backing and provenance reserve.
    std::size_t bankBytes = 0;         ///< Requested arrays in one bank, excluding inline fixed metadata.
    std::uint64_t revision = 0;        ///< Global effective mutation barrier.
    std::uint64_t lastClient = 0;      ///< Last issued client activation, never reused.
    std::uint64_t lastMembership = 0;  ///< Last issued key activation, never reused.
    std::uint64_t preparation = 0;     ///< Current inactive-bank write identity.

    /** Validate one client using only fixed metadata.
     * @param client Opaque token, possibly foreign or retired.
     * @return Whether owner identity, slot and activation all match.
     */
    bool valid(const MuxClient &client) const noexcept {
        return client.identity_ == identity && client.activation_ != 0 && client.slot_ < limits.clients &&
               clients[client.slot_].activation == client.activation_;
    }

    /** Reserve a conservative whole-operation bound before scanning or touching scratch.
     * The pass factor covers independent validation, duplicate, old-map, candidate, union and cleanup
     * passes. Eight complete State-plus-bank byte passes conservatively cover array assignments,
     * repeated range writes, inline counter resets, local tokens and final publication metadata.
     * Configured capacities intentionally overcharge short/no-op operations. Every nested
     * scan is bounded by (metadata + inputs) times (entries + inputs); text comparisons scan at most
     * symbolUnits each. Byte work includes complete scratch reset and old-bank retirement.
     * @param inputCount Borrowed input slots, already bounded by commandEntries.
     * @param work Remaining caller credits; successful earlier charges are not refunded.
     * @param rewrite Whether complete candidate backing may be rewritten.
     * @return Success or typed representability/work rejection.
     */
    std::expected<void, MuxError> reserve(std::size_t inputCount, MuxWork &work, bool rewrite) const noexcept {
        constexpr std::size_t WORK_PASSES = 32;
        constexpr std::size_t REWRITE_PASSES = 8;
        std::size_t outer = 1;
        std::size_t inner = 1;
        std::size_t visits = 0;
        std::size_t text = 0;
        std::size_t bytes = 0;

        if (!accumulate(outer, limits.clients) || !accumulate(outer, recordCount) ||
            !accumulate(outer, limits.entries) || !accumulate(outer, limits.aggregateEntries) ||
            !accumulate(outer, inputCount) || !accumulate(inner, limits.entries) || !accumulate(inner, inputCount) ||
            !accumulate(visits, outer, inner) || visits > std::numeric_limits<std::size_t>::max() / WORK_PASSES) {
            return std::unexpected(MuxError::RESOURCE_LIMIT);
        }

        visits *= WORK_PASSES;

        std::size_t rewriteBytes = sizeof(State);

        if (!accumulate(text, visits, limits.symbolUnits) ||
            (rewrite && (!accumulate(rewriteBytes, bankBytes) || !accumulate(bytes, rewriteBytes, REWRITE_PASSES)))) {
            return std::unexpected(MuxError::RESOURCE_LIMIT);
        }

        if (!charge(work.visits, visits) || !charge(work.symbolUnits, text) || !charge(work.bytes, bytes)) {
            return std::unexpected(MuxError::WORK_LIMIT);
        }

        return {};
    }

    /** Validate provenance, exact symbol and HISTORY metadata.
     * @param key Borrowed exact key.
     * @return First input rejection, or no error.
     */
    std::optional<MuxError> validateKey(const core::SubscriptionKey &key, bool concrete = false) const noexcept {
        bool found = false;
        bool indexed = false;

        for (std::size_t i = 0; i < recordCount; ++i) {
            if (records[i].context.matches(key.context)) {
                found = true;
                indexed = records[i].indexed;
                break;
            }
        }

        if (!found) {
            return MuxError::INVALID_CONTEXT;
        }

        if (key.selector != core::SubscriptionSelectorKind::EXACT &&
            key.selector != core::SubscriptionSelectorKind::ALL) {
            return MuxError::INVALID_SELECTOR;
        }

        if (key.selector == core::SubscriptionSelectorKind::ALL && (!key.symbol.empty() || concrete)) {
            return MuxError::INVALID_SELECTOR;
        }

        if (key.selector == core::SubscriptionSelectorKind::ALL &&
            policy != core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) {
            return MuxError::UNSUPPORTED_SELECTOR;
        }

        const bool canonicalData = concrete && policy == core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1;

        if (key.symbol.data() == nullptr) {
            return MuxError::INVALID_SELECTOR;
        }

        if (!canonicalData && key.symbol.size() > limits.symbolUnits) {
            return MuxError::RESOURCE_LIMIT;
        }

        if (!canonicalData && !key.symbol.empty() && key.symbol.front() == u'*') {
            return MuxError::INVALID_SELECTOR;
        }

        if (family == core::SubscriptionFamily::HISTORY && !indexed) {
            return MuxError::INVALID_HISTORY;
        }

        return {};
    }

    /** Clear every scratch object after complete rewrite/cleanup work has been reserved.
     * @param bank Non-published bank whose fixed backing remains allocated.
     */
    void reset(Bank &bank) noexcept {
        for (std::size_t i = 0; i < limits.clients; ++i) {
            bank.ranges[i] = {};
        }

        for (std::size_t i = 0; i < limits.entries; ++i) {
            bank.entries[i] = {};
        }

        for (std::size_t i = 0; i < limits.aggregateEntries; ++i) {
            bank.aggregate[i] = {};
        }

        bank.entryCount = 0;
        bank.aggregateCount = 0;
        bank.symbolCount = 0;
    }

    /** Locate an exact key inside one client's fixed contiguous range.
     * @param bank Published or candidate bank.
     * @param range Explicit client range.
     * @param key Validated exact key.
     * @return Entry pointer, or null when absent.
     */
    const MuxEntry *find(const Bank &bank, Range range, const core::SubscriptionKey &key) const noexcept {
        for (std::size_t i = 0; i < range.count; ++i) {
            const auto &entry = bank.entries[range.offset + i];

            if (sameKey(entry.key, key)) {
                return &entry;
            }
        }

        return nullptr;
    }

    /** Append owned text, stable activation and exact aggregate demand to private scratch.
     * @param bank Non-published bank.
     * @param request Validated borrowed desired input.
     * @param previous Equal key in the same old client, or null.
     * @param clientActivation Non-reused active client identity.
     * @param nextMembership Private candidate activation counter, published only on commit.
     * @return Success or resource/identity rejection; published bank remains untouched.
     */
    std::expected<void, MuxError> append(Bank &bank, const core::SubscriptionRequest &request, const MuxEntry *previous,
                                         std::uint64_t clientActivation, std::uint64_t &nextMembership) noexcept {
        if (bank.entryCount == limits.entries ||
            request.key.symbol.size() > limits.totalSymbolUnits - bank.symbolCount) {
            return std::unexpected(MuxError::RESOURCE_LIMIT);
        }

        MuxMembership membership;

        if (previous && previous->historyBound == request.historyBound) {
            membership = previous->membership;
        } else {
            if (nextMembership == std::numeric_limits<std::uint64_t>::max()) {
                return std::unexpected(MuxError::IDENTITY_EXHAUSTED);
            }

            membership.identity_ = identity;
            membership.clientActivation_ = clientActivation;
            membership.activation_ = ++nextMembership;
        }

        auto *text = request.key.symbol.empty() ? &bank.emptySymbol : bank.symbols.get() + bank.symbolCount;
        std::copy(request.key.symbol.begin(), request.key.symbol.end(), text);
        bank.symbolCount += request.key.symbol.size();
        auto &entry = bank.entries[bank.entryCount++];
        entry = {{request.key.context, {text, request.key.symbol.size()}, request.key.selector},
                 request.historyBound,
                 std::move(membership),
                 request.projectionMask,
                 {}};

        for (std::size_t i = 0; i < core::MAX_DELIVERY_PROJECTIONS; ++i) {
            const auto bit = core::DeliveryProjectionMask{1} << i;

            if (!(request.projectionMask & bit)) {
                continue;
            }

            if (previous && previous->historyBound == request.historyBound && (previous->projectionMask & bit)) {
                entry.projectionMemberships[i] = previous->projectionMemberships[i];
            } else {
                if (nextMembership == std::numeric_limits<std::uint64_t>::max()) {
                    return std::unexpected(MuxError::IDENTITY_EXHAUSTED);
                }

                auto &projection = entry.projectionMemberships[i];
                projection.identity_ = identity;
                projection.clientActivation_ = clientActivation;
                projection.activation_ = ++nextMembership;
            }
        }

        for (std::size_t i = 0; i < bank.aggregateCount; ++i) {
            auto &aggregate = bank.aggregate[i];

            if (sameKey(aggregate.key, entry.key)) {
                if (family == core::SubscriptionFamily::HISTORY && *entry.historyBound < *aggregate.historyBound) {
                    aggregate.historyBound = entry.historyBound;
                }

                return {};
            }
        }

        if (bank.aggregateCount == limits.aggregateEntries) {
            return std::unexpected(MuxError::RESOURCE_LIMIT);
        }

        bank.aggregate[bank.aggregateCount++] = {entry.key, entry.historyBound};

        return {};
    }
};

MuxState::MuxState(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {
}

MuxState::~MuxState() = default;
MuxState::MuxState(MuxState &&other) noexcept = default;

MuxState &MuxState::operator=(MuxState &&other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
    }

    return *this;
}

std::expected<MuxState, MuxError> MuxState::create(const core::LocalSchema &schema, core::SubscriptionFamily family,
                                                   MuxLimits limits, MuxWork &work,
                                                   core::SubscriptionSelectorPolicy policy) {
    if (!charge(work.operations, 1)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (family != core::SubscriptionFamily::STREAM && family != core::SubscriptionFamily::TICKER &&
        family != core::SubscriptionFamily::HISTORY) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    if ((policy != core::SubscriptionSelectorPolicy::EXACT_ONLY &&
         policy != core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1 &&
         policy != core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) ||
        (policy == core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1 &&
         family != core::SubscriptionFamily::STREAM)) {
        return std::unexpected(MuxError::UNSUPPORTED_SELECTOR);
    }

    if (!schema.sharesIdentityWith(schema)) {
        return std::unexpected(MuxError::INVALID_CONFIGURATION);
    }

    const auto records = schema.records();

    if (records.size() > limits.schemaRecords) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    std::size_t bankBytes = 0;
    std::size_t bytes = sizeof(MuxState) + sizeof(State) + sizeof(detail::Identity);
    std::size_t visits = 1;

    if (!accumulate(bankBytes, limits.clients, sizeof(State::Range)) ||
        !accumulate(bankBytes, limits.entries, sizeof(MuxEntry)) ||
        !accumulate(bankBytes, limits.aggregateEntries, sizeof(core::SubscriptionRequest)) ||
        !accumulate(bankBytes, limits.totalSymbolUnits, sizeof(char16_t)) || !accumulate(bytes, bankBytes, 2) ||
        !accumulate(bytes, records.size(), sizeof(State::Record)) ||
        !accumulate(bytes, limits.clients, sizeof(State::Client)) || !accumulate(bytes, schema.storageBytes()) ||
        !accumulate(visits, limits.clients, 3) || !accumulate(visits, limits.entries, 2) ||
        !accumulate(visits, limits.aggregateEntries, 2) || !accumulate(visits, records.size())) {
        return std::unexpected(MuxError::INVALID_CONFIGURATION);
    }

    if (bytes > limits.residentBytes) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    std::size_t creationBytes = 0;
    std::size_t creationVisits = 0;

    // Reserve initialization and possible failure disposal, then the separate record-capture fill.
    if (!accumulate(creationBytes, bytes, 2) || !accumulate(creationBytes, records.size(), sizeof(State::Record)) ||
        !accumulate(creationVisits, visits, 2) || !accumulate(creationVisits, records.size())) {
        return std::unexpected(MuxError::INVALID_CONFIGURATION);
    }

    if (!charge(work.visits, creationVisits) || !charge(work.bytes, creationBytes)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    try {
        auto state = std::make_unique<State>();
        state->limits = limits;
        state->policy = policy;
        state->family = family;
        state->recordCount = records.size();
        state->storageBytes = bytes;
        state->bankBytes = bankBytes;
        state->identity = std::make_shared<detail::Identity>();
        state->records = std::make_unique<State::Record[]>(records.size());
        state->clients = std::make_unique<State::Client[]>(limits.clients);

        for (std::size_t i = 0; i < records.size(); ++i) {
            state->records[i] = {schema.recordContext(static_cast<std::int32_t>(i)),
                                 records[i].hasTime && records[i].indexedTimeLayout.has_value()};
        }

        for (auto &bank : state->banks) {
            bank.ranges = std::make_unique<State::Range[]>(limits.clients);
            bank.entries = std::make_unique<MuxEntry[]>(limits.entries);
            bank.aggregate = std::make_unique<core::SubscriptionRequest[]>(limits.aggregateEntries);
            bank.symbols = std::make_unique<char16_t[]>(limits.totalSymbolUnits);
        }

        return MuxState(std::move(state));
    } catch (const std::bad_alloc &) {
        return std::unexpected(MuxError::ALLOCATION_FAILURE);
    }
}

std::expected<MuxClient, MuxError> MuxState::createClient(MuxWork &work) {
    if (!charge(work.operations, 1)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (!state_) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    auto &state = *state_;

    if (!charge(work.visits, state.limits.clients) ||
        !charge(work.bytes, sizeof(State::Client) + sizeof(MuxClient) + 2 * sizeof(std::uint64_t))) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    for (std::size_t i = 0; i < state.limits.clients; ++i) {
        if (state.clients[i].activation != 0) {
            continue;
        }

        if (state.lastClient == std::numeric_limits<std::uint64_t>::max() ||
            state.revision == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(MuxError::IDENTITY_EXHAUSTED);
        }

        MuxClient result;
        result.identity_ = state.identity;
        result.slot_ = i;
        result.activation_ = ++state.lastClient;
        state.clients[i].activation = result.activation_;
        ++state.revision;

        return result;
    }

    return std::unexpected(MuxError::RESOURCE_LIMIT);
}

std::expected<MuxState::PreparedChange, MuxError> MuxState::prepare(Command command, const MuxClient &client,
                                                                    std::span<const core::SubscriptionRequest> requests,
                                                                    std::span<const core::SubscriptionKey> keys,
                                                                    MuxWork &work) {
    if (!charge(work.operations, 1)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (!state_) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    auto &state = *state_;

    if (!state.valid(client)) {
        return std::unexpected(MuxError::INVALID_CLIENT);
    }

    const auto inputCount = command == Command::REMOVE ? keys.size() : requests.size();

    if (inputCount > state.limits.commandEntries) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    if (const auto reserved = state.reserve(inputCount, work, true); !reserved) {
        return std::unexpected(reserved.error());
    }

    for (std::size_t i = 0; i < inputCount; ++i) {
        const auto &key = command == Command::REMOVE ? keys[i] : requests[i].key;

        if (const auto error = state.validateKey(key)) {
            return std::unexpected(*error);
        }

        if (command != Command::REMOVE &&
            ((state.family == core::SubscriptionFamily::HISTORY) != requests[i].historyBound.has_value())) {
            return std::unexpected(MuxError::INVALID_HISTORY);
        }

        if (command != Command::REMOVE && !core::validDeliveryProjectionMask(requests[i].projectionMask)) {
            return std::unexpected(MuxError::INVALID_SELECTOR);
        }

        if (command != Command::REMOVE) {
            for (std::size_t j = 0; j < i; ++j) {
                if (sameKey(key, requests[j].key) && (requests[i].historyBound != requests[j].historyBound ||
                                                      requests[i].projectionMask != requests[j].projectionMask)) {
                    return std::unexpected(MuxError::CONTRADICTORY_INPUT);
                }
            }
        }
    }

    if (state.preparation == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(MuxError::IDENTITY_EXHAUSTED);
    }

    ++state.preparation;
    const auto &old = state.banks[state.published];
    auto &candidate = state.banks[1 - state.published];
    state.reset(candidate);
    auto nextMembership = state.lastMembership;

    for (std::size_t slot = 0; slot < state.limits.clients; ++slot) {
        auto &range = candidate.ranges[slot];
        range.offset = candidate.entryCount;
        const auto oldRange = old.ranges[slot];
        const bool selected = slot == client.slot_;

        if (!selected || command == Command::ADD || command == Command::REMOVE) {
            for (std::size_t i = 0; i < oldRange.count; ++i) {
                const auto &entry = old.entries[oldRange.offset + i];
                core::SubscriptionRequest desired{entry.key, entry.historyBound, entry.projectionMask};
                bool removed = false;

                if (selected) {
                    for (std::size_t j = 0; j < inputCount; ++j) {
                        const auto &key = command == Command::REMOVE ? keys[j] : requests[j].key;

                        if (sameKey(entry.key, key)) {
                            if (command == Command::REMOVE) {
                                removed = true;
                            } else {
                                desired = requests[j];
                            }

                            break;
                        }
                    }
                }

                if (!removed) {
                    if (auto result =
                            state.append(candidate, desired, &entry, state.clients[slot].activation, nextMembership);
                        !result) {
                        return std::unexpected(result.error());
                    }
                }
            }
        }

        if (selected && (command == Command::ADD || command == Command::REPLACE)) {
            for (std::size_t i = 0; i < requests.size(); ++i) {
                const auto *previous = state.find(old, oldRange, requests[i].key);

                if (command == Command::ADD && previous) {
                    continue;
                }

                bool duplicate = false;

                for (std::size_t j = 0; j < i; ++j) {
                    if (sameKey(requests[i].key, requests[j].key)) {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate) {
                    if (auto result =
                            state.append(candidate, requests[i], previous, client.activation_, nextMembership);
                        !result) {
                        return std::unexpected(result.error());
                    }
                }
            }
        }

        range.count = candidate.entryCount - range.offset;
    }

    const auto previousRange = old.ranges[client.slot_];
    const auto nextRange = candidate.ranges[client.slot_];
    bool changed = command == Command::REMOVE_CLIENT || previousRange.count != nextRange.count;

    for (std::size_t i = 0; !changed && i < previousRange.count; ++i) {
        const auto &entry = old.entries[previousRange.offset + i];
        const auto *next = state.find(candidate, nextRange, entry.key);
        changed = !next || next->historyBound != entry.historyBound || next->projectionMask != entry.projectionMask;
    }

    if (changed && state.revision == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(MuxError::IDENTITY_EXHAUSTED);
    }

    return PreparedChange{state.identity,
                          state.revision,
                          state.preparation,
                          nextMembership,
                          command == Command::REMOVE_CLIENT ? std::optional{client.slot_} : std::nullopt,
                          changed};
}

bool MuxState::current(const PreparedChange &candidate) const noexcept {
    return state_ && state_->identity == candidate.identity && state_->revision == candidate.baseRevision &&
           state_->preparation == candidate.preparation;
}

std::expected<std::span<const MuxEntry>, MuxError> MuxState::preparedEntries(const PreparedChange &candidate,
                                                                             const MuxClient &client) const noexcept {
    if (!current(candidate))
        return std::unexpected(MuxError::INVALID_STATE);

    if (!state_->valid(client))
        return std::unexpected(MuxError::INVALID_CLIENT);

    const auto &bank = state_->banks[candidate.changed ? 1 - state_->published : state_->published];
    const auto range = bank.ranges[client.slot_];

    return std::span<const MuxEntry>{bank.entries.get() + range.offset, range.count};
}

std::expected<std::span<const core::SubscriptionRequest>, MuxError>
MuxState::preparedAggregate(const PreparedChange &candidate) const noexcept {
    if (!current(candidate))
        return std::unexpected(MuxError::INVALID_STATE);

    const auto &bank = state_->banks[candidate.changed ? 1 - state_->published : state_->published];

    return std::span<const core::SubscriptionRequest>{bank.aggregate.get(), bank.aggregateCount};
}

std::expected<MuxChange, MuxError> MuxState::commit(const PreparedChange &candidate) noexcept {
    if (!current(candidate)) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    auto &state = *state_;

    if (!candidate.changed) {
        return MuxChange{state.revision, false};
    }

    // All fallible work is complete. Publication changes only fixed metadata and the bank index.
    state.published = 1 - state.published;
    state.lastMembership = candidate.membership;

    if (candidate.retiredSlot) {
        state.clients[*candidate.retiredSlot].activation = 0;
    }

    ++state.revision;

    return MuxChange{state.revision, true};
}

std::expected<MuxChange, MuxError> MuxState::apply(Command command, const MuxClient &client,
                                                   std::span<const core::SubscriptionRequest> requests,
                                                   std::span<const core::SubscriptionKey> keys, MuxWork &work) {
    const auto candidate = prepare(command, client, requests, keys, work);

    if (!candidate) {
        return std::unexpected(candidate.error());
    }

    return commit(*candidate);
}

std::expected<MuxChange, MuxError> MuxState::removeClient(const MuxClient &client, MuxWork &work) {
    return apply(Command::REMOVE_CLIENT, client, {}, {}, work);
}

std::expected<MuxChange, MuxError> MuxState::add(const MuxClient &client,
                                                 std::span<const core::SubscriptionRequest> requests, MuxWork &work) {
    return apply(Command::ADD, client, requests, {}, work);
}

std::expected<MuxChange, MuxError> MuxState::remove(const MuxClient &client,
                                                    std::span<const core::SubscriptionKey> keys, MuxWork &work) {
    return apply(Command::REMOVE, client, {}, keys, work);
}

std::expected<MuxChange, MuxError>
MuxState::replace(const MuxClient &client, std::span<const core::SubscriptionRequest> requests, MuxWork &work) {
    return apply(Command::REPLACE, client, requests, {}, work);
}

std::expected<MuxChange, MuxError> MuxState::clear(const MuxClient &client, MuxWork &work) {
    return apply(Command::CLEAR, client, {}, {}, work);
}

std::expected<std::span<const MuxEntry>, MuxError> MuxState::clientEntries(const MuxClient &client) const noexcept {
    if (!state_) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    if (!state_->valid(client)) {
        return std::unexpected(MuxError::INVALID_CLIENT);
    }

    const auto &bank = state_->banks[state_->published];
    const auto range = bank.ranges[client.slot_];

    if (range.count == 0) {
        return std::span<const MuxEntry>{};
    }

    return std::span<const MuxEntry>{bank.entries.get() + range.offset, range.count};
}

std::span<const core::SubscriptionRequest> MuxState::aggregateEntries() const noexcept {
    if (!state_ || state_->banks[state_->published].aggregateCount == 0) {
        return {};
    }

    const auto &bank = state_->banks[state_->published];

    return {bank.aggregate.get(), bank.aggregateCount};
}

std::expected<std::optional<MuxMembership>, MuxError> MuxState::select(const MuxClient &client,
                                                                       core::SubscriptionKey key,
                                                                       std::optional<std::int64_t> rawTime,
                                                                       MuxWork &work) const {
    if (!charge(work.operations, 1)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (!state_) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    if (!state_->valid(client)) {
        return std::unexpected(MuxError::INVALID_CLIENT);
    }

    if (auto result = state_->reserve(1, work, false); !result) {
        return std::unexpected(result.error());
    }

    if (const auto error = state_->validateKey(key, true)) {
        return std::unexpected(*error);
    }

    if ((state_->family == core::SubscriptionFamily::HISTORY) != rawTime.has_value()) {
        return std::unexpected(MuxError::INVALID_HISTORY);
    }

    const auto &bank = state_->banks[state_->published];

    if (state_->policy == core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) {
        const auto range = bank.ranges[client.slot_];
        std::size_t visits = 1;
        std::size_t units = 0;

        if (!accumulate(visits, range.count, 2) || !accumulate(units, range.count, key.symbol.size()) ||
            !accumulate(units, units)) {
            return std::unexpected(MuxError::RESOURCE_LIMIT);
        }

        if (!charge(work.visits, visits) || !charge(work.symbolUnits, units)) {
            return std::unexpected(MuxError::WORK_LIMIT);
        }

        MuxMembership selected;

        for (std::size_t i = 0; i < range.count; ++i) {
            const auto &candidate = bank.entries[range.offset + i];

            if (candidate.key.context.matches(key.context) &&
                (candidate.key.selector == core::SubscriptionSelectorKind::ALL || candidate.key.symbol == key.symbol)) {
                if (selected.activation_ == 0) {
                    selected = candidate.membership;
                } else {
                    selected.alternateActivation_ = candidate.membership.activation_;
                }
            }
        }

        return selected.activation_ == 0 ? std::optional<MuxMembership>{} : std::optional<MuxMembership>{selected};
    }

    const auto *entry = state_->find(bank, bank.ranges[client.slot_], key);

    if (!entry || (rawTime && *rawTime < *entry->historyBound)) {
        return std::optional<MuxMembership>{};
    }

    return std::optional<MuxMembership>{entry->membership};
}

std::expected<bool, MuxError> MuxState::isCurrent(const MuxClient &client, const MuxMembership &membership,
                                                  MuxWork &work) const {
    return isCurrentInBank(client, membership, work, false);
}

std::expected<bool, MuxError> MuxState::isCurrentInBank(const MuxClient &client, const MuxMembership &membership,
                                                        MuxWork &work, bool inactive) const {
    if (!charge(work.operations, 1)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (!state_) {
        return std::unexpected(MuxError::INVALID_STATE);
    }

    if (!state_->valid(client)) {
        return std::unexpected(MuxError::INVALID_CLIENT);
    }

    const auto &bank = state_->banks[inactive ? 1 - state_->published : state_->published];
    const auto range = bank.ranges[client.slot_];

    std::size_t visits = 1;

    if (!accumulate(visits, range.count)) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    if (membership.alternateActivation_ != 0 && !accumulate(visits, range.count)) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    if (!charge(work.visits, visits)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    if (membership.identity_ != state_->identity || membership.clientActivation_ != client.activation_ ||
        membership.activation_ == 0) {
        return false;
    }

    for (std::size_t i = 0; i < range.count; ++i) {
        if (bank.entries[range.offset + i].membership.activation_ == membership.activation_ ||
            (membership.alternateActivation_ != 0 &&
             bank.entries[range.offset + i].membership.activation_ == membership.alternateActivation_)) {
            return true;
        }

        for (const auto &projection : bank.entries[range.offset + i].projectionMemberships) {
            if (projection.activation_ == membership.activation_ ||
                (membership.alternateActivation_ != 0 && projection.activation_ == membership.alternateActivation_)) {
                return true;
            }
        }
    }

    return false;
}

std::expected<core::CapturedDeliveryProjections<MuxMembership>, MuxError>
MuxState::selectProjections(const MuxClient &client, core::SubscriptionKey key, std::optional<std::int64_t> rawTime,
                            MuxWork &work) const {
    const auto physical = select(client, key, rawTime, work);

    if (!physical) {
        return std::unexpected(physical.error());
    }

    core::CapturedDeliveryProjections<MuxMembership> result;

    if (!*physical) {
        return result;
    }

    result.physical = **physical;
    const auto &bank = state_->banks[state_->published];
    const auto range = bank.ranges[client.slot_];
    std::size_t visits = 1;
    std::size_t units = 0;

    if (!accumulate(visits, range.count, core::MAX_DELIVERY_PROJECTIONS + 2) ||
        !accumulate(units, range.count, key.symbol.size()) || !accumulate(units, units)) {
        return std::unexpected(MuxError::RESOURCE_LIMIT);
    }

    if (!charge(work.visits, visits) || !charge(work.symbolUnits, units)) {
        return std::unexpected(MuxError::WORK_LIMIT);
    }

    for (std::size_t n = 0; n < range.count; ++n) {
        const auto &entry = bank.entries[range.offset + n];

        if (!entry.key.context.matches(key.context) ||
            (entry.key.selector != core::SubscriptionSelectorKind::ALL && entry.key.symbol != key.symbol) ||
            (rawTime && *rawTime < *entry.historyBound)) {
            continue;
        }

        for (std::size_t i = 0; i < core::MAX_DELIVERY_PROJECTIONS; ++i) {
            const auto bit = core::DeliveryProjectionMask{1} << i;

            if (!(entry.projectionMask & bit)) {
                continue;
            }

            if (!(result.mask & bit)) {
                result.memberships[i] = entry.projectionMemberships[i];
            } else if (result.memberships[i].activation_ != entry.projectionMemberships[i].activation_) {
                result.memberships[i].alternateActivation_ = entry.projectionMemberships[i].activation_;
            }

            result.mask |= bit;
        }
    }

    return result;
}

std::expected<core::DeliveryProjectionMask, MuxError>
MuxState::currentProjectionMask(const MuxClient &client,
                                const core::CapturedDeliveryProjections<MuxMembership> &captured, MuxWork &work) const {
    return currentProjectionMaskInBank(client, captured, work, false);
}

std::expected<core::DeliveryProjectionMask, MuxError>
MuxState::currentProjectionMaskInBank(const MuxClient &client,
                                      const core::CapturedDeliveryProjections<MuxMembership> &captured, MuxWork &work,
                                      bool inactive) const {
    core::DeliveryProjectionMask result = 0;

    if (captured.mask == 0) {
        const auto owner = isCurrentInBank(client, {}, work, inactive);

        if (!owner) {
            return std::unexpected(owner.error());
        }

        return result;
    }

    for (std::size_t i = 0; i < core::MAX_DELIVERY_PROJECTIONS; ++i) {
        const auto bit = core::DeliveryProjectionMask{1} << i;

        if (!(captured.mask & bit)) {
            continue;
        }

        const auto current = isCurrentInBank(client, captured.memberships[i], work, inactive);

        if (!current) {
            return std::unexpected(current.error());
        }

        if (*current) {
            result |= bit;
        }
    }

    return result;
}

core::SubscriptionSelectorPolicy MuxState::selectorPolicy() const noexcept {
    return state_ ? state_->policy : core::SubscriptionSelectorPolicy::EXACT_ONLY;
}

std::uint64_t MuxState::revision() const noexcept {
    return state_ ? state_->revision : 0;
}

std::size_t MuxState::storageBytes() const noexcept {
    return state_ ? state_->storageBytes : 0;
}
} // namespace org::ttldtor::qdcxx::mux
