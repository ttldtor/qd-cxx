// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "subscription_state_access.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::core {

/** Fixed owner token retained independently of desired entries to prevent address-reuse ABA. */
struct SubscriptionMembership::Identity {
    mutable bool candidateReserved = false; ///< Accessed only by the serialized desired-state owner and candidate.
};

SubscriptionState::Candidate::Candidate(const SubscriptionState &owner) noexcept
    : identity_(owner.identity_), baseRevision_(owner.revision_) {
    identity_->candidateReserved = true;
}

SubscriptionState::Candidate::~Candidate() {
    if (identity_) {
        entries_.reset();
        symbols_.reset();
        identity_->candidateReserved = false;
    }
}

std::expected<SubscriptionChange, SubscriptionError>
detail::SubscriptionStateAccess::commitReplace(SubscriptionState &owner, PreparedReplace candidate) noexcept {
    return owner.publish(std::move(candidate));
}

std::span<const SubscriptionEntry> detail::SubscriptionStateAccess::entries(const SubscriptionState &owner,
                                                                            const PreparedReplace &candidate) noexcept {
    if (!owner.owns(candidate)) {
        return {};
    }

    return candidate.changed_ ? std::span(candidate.entries_.get(), candidate.size_) : owner.entries();
}

std::optional<std::uint64_t> detail::SubscriptionStateAccess::revision(const SubscriptionState &owner,
                                                                       const PreparedReplace &candidate) noexcept {
    if (!owner.owns(candidate)) {
        return {};
    }

    return candidate.baseRevision_ + candidate.changed_;
}

bool detail::SubscriptionStateAccess::isCurrent(const SubscriptionState &owner, const PreparedReplace &candidate,
                                                const SubscriptionMembership &membership) noexcept {
    return owner.isCurrentIn(entries(owner, candidate), membership);
}

/** Immutable schema facts retained without copying unrelated declaration storage. */
struct SubscriptionState::Record {
    RecordContext context;
    bool indexed = false;
};

namespace {

/** Internal unwinding carries only a fixed typed cause; it never escapes the command boundary. */
struct Rejected {
    SubscriptionError error;
};

/** Debit work before starting the corresponding bounded step.
 * @param remaining Remaining caller-owned turn credits.
 * @param amount Required work in the counter's units.
 */
void charge(std::size_t &remaining, std::size_t amount) {
    if (amount > remaining) {
        throw Rejected{SubscriptionError::WORK_LIMIT};
    }

    remaining -= amount;
}

/** Add logical backing capacity without wraparound.
 * @param current Capacity already accounted in this scope.
 * @param extra Additional requested units or slots.
 * @param maximum Finite maximum in the same units.
 * @return Sum, or a resource rejection before overflow.
 */
std::size_t addCapacity(std::size_t current, std::size_t extra, std::size_t maximum) {
    if (current > maximum || extra > maximum - current) {
        throw Rejected{SubscriptionError::RESOURCE_LIMIT};
    }

    return current + extra;
}

/** Compare exact keys while charging each visited key and each examined UTF-16 pair.
 * @param left First borrowed exact key.
 * @param right Second borrowed exact key.
 * @param work Remaining caller turn credits.
 * @return Whether provenance and every UTF-16 unit agree.
 */
bool equalKey(const SubscriptionKey &left, const SubscriptionKey &right, SubscriptionWork &work) {
    charge(work.keyVisits, 1);

    if (left.selector != right.selector || !left.context.matches(right.context) ||
        left.symbol.size() != right.symbol.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.symbol.size(); ++i) {
        charge(work.symbolUnits, 2);

        if (left.symbol[i] != right.symbol[i]) {
            return false;
        }
    }

    return true;
}

} // namespace

CapturedDeliveryProjections<SubscriptionMembership>
captureDeliveryProjections(const SubscriptionEntry &first, const SubscriptionEntry *second) noexcept {
    CapturedDeliveryProjections<SubscriptionMembership> result;
    result.physical = second ? SubscriptionMembership::anyOf(first.membership, second->membership) : first.membership;

    for (std::size_t i = 0; i < MAX_DELIVERY_PROJECTIONS; ++i) {
        const auto bit = DeliveryProjectionMask{1} << i;
        const bool a = (first.projectionMask & bit) != 0;
        const bool b = second && (second->projectionMask & bit) != 0;

        if (!a && !b) {
            continue;
        }

        result.mask |= bit;
        result.memberships[i] = a ? first.projectionMemberships[i] : second->projectionMemberships[i];

        if (a && b) {
            result.memberships[i] =
                SubscriptionMembership::anyOf(first.projectionMemberships[i], second->projectionMemberships[i]);
        }
    }

    return result;
}

SubscriptionKey allSymbols(RecordContext context) noexcept {
    return {std::move(context), u"", SubscriptionSelectorKind::ALL};
}

SubscriptionMembership SubscriptionMembership::anyOf(const SubscriptionMembership &first,
                                                     const SubscriptionMembership &second) noexcept {
    if (!first.identity_ || first.identity_ != second.identity_ || first.activation_ == 0 || second.activation_ == 0 ||
        first.alternateActivation_ != 0 || second.alternateActivation_ != 0) {
        return {};
    }

    auto result = first;

    if (first.activation_ != second.activation_) {
        result.alternateActivation_ = second.activation_;
    }

    return result;
}

SubscriptionSelectorPolicy SubscriptionState::selectorPolicy() const noexcept {
    return identity_ ? selectorPolicy_ : SubscriptionSelectorPolicy::EXACT_ONLY;
}

SubscriptionState::SubscriptionState() noexcept = default;
SubscriptionState::~SubscriptionState() = default;

SubscriptionState::SubscriptionState(SubscriptionState &&other) noexcept {
    *this = std::move(other);
}

SubscriptionState &SubscriptionState::operator=(SubscriptionState &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    records_ = std::move(other.records_);
    recordCount_ = std::exchange(other.recordCount_, 0);
    identity_ = std::move(other.identity_);
    family_ = other.family_;
    selectorPolicy_ = other.selectorPolicy_;
    limits_ = other.limits_;
    entries_ = std::move(other.entries_);
    symbols_ = std::move(other.symbols_);
    size_ = std::exchange(other.size_, 0);
    symbolUnits_ = std::exchange(other.symbolUnits_, 0);
    revision_ = std::exchange(other.revision_, 0);
    lastActivation_ = std::exchange(other.lastActivation_, 0);

    return *this;
}

std::expected<SubscriptionState, SubscriptionError> SubscriptionState::create(const LocalSchema &schema,
                                                                              SubscriptionFamily family,
                                                                              SubscriptionLimits limits,
                                                                              SubscriptionSelectorPolicy policy) {
    if (family != SubscriptionFamily::STREAM && family != SubscriptionFamily::TICKER &&
        family != SubscriptionFamily::HISTORY) {
        return std::unexpected(SubscriptionError::INVALID_STATE);
    }

    if ((policy != SubscriptionSelectorPolicy::EXACT_ONLY && policy != SubscriptionSelectorPolicy::CANONICAL_EXACT_V1 &&
         policy != SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) ||
        (policy == SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1 && family != SubscriptionFamily::STREAM)) {
        return std::unexpected(SubscriptionError::UNSUPPORTED_SELECTOR);
    }

    const auto count = schema.records().size();

    if (count > limits.schemaRecords || count > std::numeric_limits<std::size_t>::max() / sizeof(Record)) {
        return std::unexpected(SubscriptionError::RESOURCE_LIMIT);
    }

    try {
        SubscriptionState state;

        state.family_ = family;
        state.selectorPolicy_ = policy;
        state.limits_ = limits;
        state.identity_ = std::make_shared<SubscriptionMembership::Identity>();

        if (count != 0) {
            state.records_ = std::make_unique<Record[]>(count);
        }

        for (std::size_t i = 0; i < count; ++i) {
            state.records_[i] = {schema.recordContext(static_cast<std::int32_t>(i)),
                                 schema.records()[i].hasTime && schema.records()[i].indexedTimeLayout.has_value()};
        }

        state.recordCount_ = count;

        return state;
    } catch (const std::bad_alloc &) {
        return std::unexpected(SubscriptionError::ALLOCATION_FAILURE);
    }
}

std::expected<SubscriptionState::Candidate, SubscriptionError>
SubscriptionState::prepare(Command command, std::span<const SubscriptionRequest> requests,
                           std::span<const SubscriptionKey> keys, SubscriptionWork &work) {
    try {
        charge(work.commands, 1);

        if (!identity_ || identity_->candidateReserved) {
            throw Rejected{SubscriptionError::INVALID_STATE};
        }

        const auto inputSize = command == Command::REMOVE ? keys.size() : requests.size();

        if (inputSize > limits_.commandEntries) {
            throw Rejected{SubscriptionError::RESOURCE_LIMIT};
        }

        // Inputs may borrow the current arena and remain valid throughout preparation.
        for (std::size_t i = 0; i < inputSize; ++i) {
            const auto &key = command == Command::REMOVE ? keys[i] : requests[i].key;

            charge(work.keyVisits, 1);

            bool found = false;
            bool indexed = false;

            for (std::size_t record = 0; record < recordCount_; ++record) {
                charge(work.keyVisits, 1);

                if (key.context.matches(records_[record].context)) {
                    found = true;
                    indexed = records_[record].indexed;

                    break;
                }
            }

            if (!found) {
                throw Rejected{SubscriptionError::INVALID_CONTEXT};
            }

            if (key.selector != SubscriptionSelectorKind::EXACT && key.selector != SubscriptionSelectorKind::ALL) {
                throw Rejected{SubscriptionError::INVALID_SELECTOR};
            }

            if (key.selector == SubscriptionSelectorKind::ALL && !key.symbol.empty()) {
                throw Rejected{SubscriptionError::INVALID_SELECTOR};
            }

            if (key.selector == SubscriptionSelectorKind::ALL &&
                selectorPolicy_ != SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) {
                throw Rejected{SubscriptionError::UNSUPPORTED_SELECTOR};
            }

            if (key.symbol.data() == nullptr) {
                throw Rejected{SubscriptionError::INVALID_SELECTOR};
            }

            if (key.symbol.size() > limits_.symbolUnits) {
                throw Rejected{SubscriptionError::RESOURCE_LIMIT};
            }

            // Conservatively charge every input unit, including duplicates and reserved strings.
            charge(work.symbolUnits, key.symbol.size());

            constexpr char16_t RESERVED_PREFIX = u'*';

            if (!key.symbol.empty() && key.symbol.front() == RESERVED_PREFIX) {
                throw Rejected{SubscriptionError::INVALID_SELECTOR};
            }

            if (family_ == SubscriptionFamily::HISTORY && !indexed) {
                throw Rejected{SubscriptionError::INVALID_HISTORY};
            }

            if (command != Command::REMOVE) {
                if (!validDeliveryProjectionMask(requests[i].projectionMask)) {
                    throw Rejected{SubscriptionError::INVALID_SELECTOR};
                }

                const auto hasBound = requests[i].historyBound.has_value();

                if ((family_ == SubscriptionFamily::HISTORY && !hasBound) ||
                    (family_ != SubscriptionFamily::HISTORY && hasBound)) {
                    throw Rejected{SubscriptionError::INVALID_HISTORY};
                }
            }

            for (std::size_t previous = 0; previous < i; ++previous) {
                const auto &previousKey = command == Command::REMOVE ? keys[previous] : requests[previous].key;

                if (equalKey(key, previousKey, work) && command != Command::REMOVE &&
                    (requests[i].historyBound != requests[previous].historyBound ||
                     requests[i].projectionMask != requests[previous].projectionMask)) {
                    throw Rejected{SubscriptionError::CONTRADICTORY_INPUT};
                }
            }
        }

        /** Find an input key; duplicates have already been completely validated. */
        const auto findInput = [&](const SubscriptionKey &key) -> std::optional<std::size_t> {
            for (std::size_t i = 0; i < inputSize; ++i) {
                const auto &inputKey = command == Command::REMOVE ? keys[i] : requests[i].key;

                if (equalKey(key, inputKey, work)) {
                    return i;
                }
            }

            return std::nullopt;
        };

        /** Enumerate the complete candidate without allocating temporary maps or key copies. */
        const auto visitCandidate = [&](const auto &visit) {
            if (command == Command::CLEAR) {
                return;
            }

            for (std::size_t i = 0; i < size_; ++i) {
                charge(work.keyVisits, 1);

                const auto &entry = entries_[i];
                const auto input = findInput(entry.key);

                if ((command == Command::REMOVE && input) || (command == Command::REPLACE && !input)) {
                    continue;
                }

                const auto bound =
                    input && command != Command::REMOVE ? requests[*input].historyBound : entry.historyBound;

                const auto mask =
                    input && command != Command::REMOVE ? requests[*input].projectionMask : entry.projectionMask;
                visit(entry.key, bound, mask, &entry);
            }

            if (command == Command::REMOVE) {
                return;
            }

            for (std::size_t i = 0; i < requests.size(); ++i) {
                charge(work.keyVisits, 1);

                bool present = false;

                for (std::size_t previous = 0; previous < i && !present; ++previous) {
                    present = equalKey(requests[i].key, requests[previous].key, work);
                }

                for (std::size_t old = 0; old < size_ && !present; ++old) {
                    present = equalKey(requests[i].key, entries_[old].key, work);
                }

                if (!present) {
                    visit(requests[i].key, requests[i].historyBound, requests[i].projectionMask, nullptr);
                }
            }
        };

        std::size_t candidateSize = 0;
        std::size_t candidateUnits = 0;
        std::uint64_t activations = 0;
        bool changed = false;

        visitCandidate([&](const SubscriptionKey &key, std::optional<std::int64_t> bound, DeliveryProjectionMask mask,
                           const SubscriptionEntry *previous) {
            candidateSize = addCapacity(candidateSize, 1, limits_.entries);
            candidateUnits = addCapacity(candidateUnits, key.symbol.size(), limits_.totalSymbolUnits);

            const bool renewed = !previous || previous->historyBound != bound;
            const auto issue = [&] {
                if (activations == std::numeric_limits<std::uint64_t>::max()) {
                    throw Rejected{SubscriptionError::IDENTITY_EXHAUSTED};
                }

                ++activations;
            };

            if (renewed || (mask & ~previous->projectionMask)) {
                issue();
            }

            for (std::size_t i = 0; i < MAX_DELIVERY_PROJECTIONS; ++i) {
                const auto bit = DeliveryProjectionMask{1} << i;

                if ((mask & bit) && (renewed || !(previous->projectionMask & bit))) {
                    issue();
                }
            }

            changed = changed || renewed || previous->projectionMask != mask;
        });

        changed = changed || candidateSize != size_;

        if (!changed) {
            return Candidate(*this);
        }

        if (revision_ == std::numeric_limits<std::uint64_t>::max() ||
            activations > std::numeric_limits<std::uint64_t>::max() - lastActivation_) {
            throw Rejected{SubscriptionError::IDENTITY_EXHAUSTED};
        }

        (void)addCapacity(size_, candidateSize, limits_.peakEntries);
        (void)addCapacity(symbolUnits_, candidateUnits, limits_.peakSymbolUnits);

        if (candidateSize > std::numeric_limits<std::size_t>::max() / sizeof(SubscriptionEntry) ||
            candidateUnits > std::numeric_limits<std::size_t>::max() / sizeof(char16_t)) {
            throw Rejected{SubscriptionError::RESOURCE_LIMIT};
        }

        // Reserve initialization and disposal visits even if a later copy exhausts its turn.
        charge(work.keyVisits, candidateSize);
        charge(work.keyVisits, candidateSize);

        Candidate candidate(*this);

        if (candidateSize != 0) {
            candidate.entries_ = std::make_unique<SubscriptionEntry[]>(candidateSize);
        }

        if (candidateUnits != 0) {
            candidate.symbols_ = std::make_unique_for_overwrite<char16_t[]>(candidateUnits);
        }

        std::size_t entryOffset = 0;
        std::size_t symbolOffset = 0;
        auto lastActivation = lastActivation_;

        visitCandidate([&](const SubscriptionKey &key, std::optional<std::int64_t> bound, DeliveryProjectionMask mask,
                           const SubscriptionEntry *previous) {
            charge(work.symbolUnits, key.symbol.size());

            const auto *symbol = u"";

            if (!key.symbol.empty()) {
                std::copy_n(key.symbol.data(), key.symbol.size(), candidate.symbols_.get() + symbolOffset);
                symbol = candidate.symbols_.get() + symbolOffset;
            }

            auto &entry = candidate.entries_[entryOffset++];

            entry.key = {key.context, std::u16string_view(symbol, key.symbol.size()), key.selector};
            entry.historyBound = bound;
            entry.projectionMask = mask;

            if (previous && previous->historyBound == bound && !(mask & ~previous->projectionMask)) {
                entry.membership = previous->membership;
            } else {
                entry.membership.identity_ = identity_;
                entry.membership.activation_ = ++lastActivation;
            }

            for (std::size_t i = 0; i < MAX_DELIVERY_PROJECTIONS; ++i) {
                const auto bit = DeliveryProjectionMask{1} << i;

                if (!(mask & bit)) {
                    continue;
                }

                if (previous && previous->historyBound == bound && (previous->projectionMask & bit)) {
                    entry.projectionMemberships[i] = previous->projectionMemberships[i];
                } else {
                    entry.projectionMemberships[i].identity_ = identity_;
                    entry.projectionMemberships[i].activation_ = ++lastActivation;
                }
            }

            symbolOffset += key.symbol.size();
        });

        // Releasing old entry owners is also finite command work, reserved before publication.
        charge(work.keyVisits, size_);

        candidate.size_ = candidateSize;
        candidate.symbolUnits_ = candidateUnits;
        candidate.lastActivation_ = lastActivation;
        candidate.changed_ = true;

        return candidate;
    } catch (const Rejected &rejection) {
        return std::unexpected(rejection.error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(SubscriptionError::ALLOCATION_FAILURE);
    }
}

bool SubscriptionState::owns(const Candidate &candidate) const noexcept {
    return identity_ && candidate.identity_ == identity_ && candidate.baseRevision_ == revision_;
}

std::expected<SubscriptionChange, SubscriptionError> SubscriptionState::publish(Candidate &&candidate) noexcept {
    if (!owns(candidate)) {
        return std::unexpected(SubscriptionError::INVALID_STATE);
    }

    if (candidate.changed_) {
        entries_.swap(candidate.entries_);
        symbols_.swap(candidate.symbols_);
        size_ = candidate.size_;
        symbolUnits_ = candidate.symbolUnits_;
        lastActivation_ = candidate.lastActivation_;
        ++revision_;
    }

    return SubscriptionChange{revision_, candidate.changed_};
}

std::expected<SubscriptionChange, SubscriptionError>
SubscriptionState::apply(Command command, std::span<const SubscriptionRequest> requests,
                         std::span<const SubscriptionKey> keys, SubscriptionWork &work) {
    auto candidate = prepare(command, requests, keys, work);

    if (!candidate) {
        return std::unexpected(candidate.error());
    }

    return publish(std::move(*candidate));
}

std::expected<SubscriptionChange, SubscriptionError>
SubscriptionState::add(std::span<const SubscriptionRequest> requests, SubscriptionWork &work) {
    return apply(Command::ADD, requests, {}, work);
}

std::expected<SubscriptionChange, SubscriptionError> SubscriptionState::remove(std::span<const SubscriptionKey> keys,
                                                                               SubscriptionWork &work) {
    return apply(Command::REMOVE, {}, keys, work);
}

std::expected<SubscriptionChange, SubscriptionError>
SubscriptionState::replace(std::span<const SubscriptionRequest> requests, SubscriptionWork &work) {
    return apply(Command::REPLACE, requests, {}, work);
}

std::expected<SubscriptionChange, SubscriptionError> SubscriptionState::clear(SubscriptionWork &work) {
    return apply(Command::CLEAR, {}, {}, work);
}

std::span<const SubscriptionEntry> SubscriptionState::entries() const noexcept {
    return {entries_.get(), size_};
}

bool SubscriptionState::isCurrentIn(std::span<const SubscriptionEntry> selected,
                                    const SubscriptionMembership &membership) const noexcept {
    if (!identity_ || membership.identity_ != identity_ || membership.activation_ == 0) {
        return false;
    }

    for (const auto &entry : selected) {
        if (entry.membership.activation_ == membership.activation_ ||
            (membership.alternateActivation_ != 0 && entry.membership.activation_ == membership.alternateActivation_)) {
            return true;
        }

        for (const auto &projection : entry.projectionMemberships) {
            if (projection.activation_ == membership.activation_ ||
                (membership.alternateActivation_ != 0 && projection.activation_ == membership.alternateActivation_)) {
                return true;
            }
        }
    }

    return false;
}

bool SubscriptionState::isCurrent(const SubscriptionMembership &membership) const noexcept {
    return isCurrentIn(entries(), membership);
}

DeliveryProjectionMask SubscriptionState::currentProjectionMask(
    const CapturedDeliveryProjections<SubscriptionMembership> &captured) const noexcept {
    DeliveryProjectionMask result = 0;

    for (std::size_t i = 0; i < MAX_DELIVERY_PROJECTIONS; ++i) {
        const auto bit = DeliveryProjectionMask{1} << i;

        if ((captured.mask & bit) && isCurrent(captured.memberships[i])) {
            result |= bit;
        }
    }

    return result;
}

std::uint64_t SubscriptionState::revision() const noexcept {
    return revision_;
}

SubscriptionFamily SubscriptionState::family() const noexcept {
    return family_;
}

} // namespace org::ttldtor::qdcxx::core
