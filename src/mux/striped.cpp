// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/mux/striped.hpp"
#include "../core/shared_record_block.hpp"
#include "../protocol/stripe_event_access.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace org::ttldtor::qdcxx::mux {
namespace detail {
namespace backing = core::detail;
template <class T> using StripeAllocator = backing::RecordBlockAllocator<T>;
template <class T> using StripeVector = std::vector<T, StripeAllocator<T>>;
using StripeText = std::basic_string<char16_t, std::char_traits<char16_t>, StripeAllocator<char16_t>>;

/** Pure non-reused root provenance; producer tokens never retain the operational root. */
struct StripeOrigin {};

/** One existing producer-safe port retained by a complete logical client. */
struct StripePortEntry {
    std::size_t stripe;
    MuxHandle handle;
};

/** Pure client storage, whose final release reaches only existing detached producer ports. */
struct StripePort {
    std::atomic<std::size_t> publicReferences{0};
    std::size_t controlBytes = 0;
    backing::RecordByteReservation hintCharge;
    std::shared_ptr<const StripeOrigin> origin;
    std::optional<core::SymbolStripeFilter> stripeHint;
    StripeVector<StripePortEntry> children;

    StripePort(const backing::RecordByteBudget &ledger, std::shared_ptr<const StripeOrigin> owner,
               std::optional<core::SymbolStripeFilter> selected)
        : hintCharge(ledger.state(), selected ? selected->storageBytes() : 0), origin(std::move(owner)),
          stripeHint(std::move(selected)), children(StripeAllocator<StripePortEntry>{ledger.state()}) {
    }
};

/** One owned original command identity, shared by routed borrowed request descriptors. */
struct StripeRequest {
    core::RecordContext context;
    StripeText symbol;
    core::SubscriptionSelectorKind selector;
    std::optional<std::int64_t> historyBound;
    core::DeliveryProjectionMask projections;

    StripeRequest(const core::SubscriptionRequest &request, const backing::RecordByteBudget &ledger)
        : context(request.key.context), symbol(request.key.symbol, StripeAllocator<char16_t>{ledger.state()}),
          selector(request.key.selector), historyBound(request.historyBound), projections(request.projectionMask) {
    }

    core::SubscriptionRequest borrow() const noexcept {
        return {{context, {symbol.data(), symbol.size()}, selector}, historyBound, projections};
    }
};

/** One exact range in flat routed command/index storage. */
struct StripeRange {
    std::size_t first = 0;
    std::size_t count = 0;
};

/** Exactly one operation's prepaid root-owned input and current accepted-child continuation. */
struct StripePending {
    backing::RecordByteReservation sourceCharge;
    backing::RecordByteReservation sliceCharge;
    StripeVector<StripeRequest> requests;
    StripeVector<core::SubscriptionRequest> routed;
    StripeVector<core::SubscriptionKey> removals;
    StripeVector<StripeRange> ranges;
    StripeVector<std::size_t> indices;
    StripeVector<ClientLimits> clientLimits;
    std::shared_ptr<StripePort> client;
    std::shared_ptr<MuxConsumer> consumer;
    std::optional<protocol::LiveEvent> source;
    std::optional<protocol::LiveEvent> slice;
    std::optional<MuxCommandTicket> ticket;
    std::size_t cursor = 0;
    bool discardRequested = false;
    bool stageClosing = false;
    std::size_t sourceBytes = 0;
    std::size_t sliceBytes = 0;
    bool cancel = false;
    bool unclaimedClient = false;

    explicit StripePending(const backing::RecordByteBudget &ledger)
        : requests(StripeAllocator<StripeRequest>{ledger.state()}),
          routed(StripeAllocator<core::SubscriptionRequest>{ledger.state()}),
          removals(StripeAllocator<core::SubscriptionKey>{ledger.state()}),
          ranges(StripeAllocator<StripeRange>{ledger.state()}), indices(StripeAllocator<std::size_t>{ledger.state()}),
          clientLimits(StripeAllocator<ClientLimits>{ledger.state()}) {
    }

    /** Retire within the same byte domain without moving allocators out of the reusable root slot.
     * @param other Empty/current operation storage constructed from this root's exact ledger.
     */
    void swap(StripePending &other) noexcept {
        using std::swap;
        swap(sourceCharge, other.sourceCharge);
        swap(sliceCharge, other.sliceCharge);
        requests.swap(other.requests);
        routed.swap(other.routed);
        removals.swap(other.removals);
        ranges.swap(other.ranges);
        indices.swap(other.indices);
        clientLimits.swap(other.clientLimits);
        swap(client, other.client);
        swap(consumer, other.consumer);
        swap(source, other.source);
        swap(slice, other.slice);
        swap(ticket, other.ticket);
        swap(cursor, other.cursor);
        swap(discardRequested, other.discardRequested);
        swap(stageClosing, other.stageClosing);
        swap(sourceBytes, other.sourceBytes);
        swap(sliceBytes, other.sliceBytes);
        swap(cancel, other.cancel);
        swap(unclaimedClient, other.unclaimedClient);
    }
};

/** Actual child, with no duplicated collector or detached-queue implementation. */
struct StripeChild {
    MuxQueue queue;
    std::size_t accepted = 0;
    std::size_t staged = 0;
    bool saturated = false;
    unsigned stopSent = 0;

    explicit StripeChild(MuxQueue &&value) noexcept : queue(std::move(value)) {
    }
};

/** Sole serialized operational root; callback leases never escape to detached client data. */
struct StripeRoot {
    backing::RecordByteReservation metadataCharge;
    backing::RecordByteBudget ledger;
    core::LocalSchema schema;
    core::SymbolStriper striper;
    StripedLimits limits;
    core::SubscriptionFamily family;
    std::shared_ptr<const StripeOrigin> origin;
    StripeVector<StripeChild> children;
    StripeVector<StripeChildResult> results;
    StripePending pending;
    std::uint64_t operationId = 0;
    StripeOperation operation = StripeOperation::NONE;
    StripePhase phase = StripePhase::IDLE;
    std::optional<StripeFailure> failure;
    std::optional<StopMode> stop;
    std::size_t deliveryCursor = 0;
    std::size_t closeCursor = 0;
    std::size_t unsubmittedSourceRecords = 0;
    bool countsSaturated = false;
    bool executing = false;
    bool destroyed = false;
    bool closed = false;

    StripeRoot(backing::RecordByteBudget budget, core::LocalSchema catalog, core::SymbolStriper partition,
               StripedLimits bounds, core::SubscriptionFamily contract)
        : metadataCharge(budget.state(), catalog.storageBytes() - sizeof(core::LocalSchema) + partition.storageBytes()),
          ledger(std::move(budget)), schema(std::move(catalog)), striper(std::move(partition)), limits(bounds),
          family(contract), origin(std::allocate_shared<StripeOrigin>(StripeAllocator<StripeOrigin>{ledger.state()})),
          children(StripeAllocator<StripeChild>{ledger.state()}),
          results(StripeAllocator<StripeChildResult>{ledger.state()}), pending(ledger) {
    }
};
} // namespace detail

namespace {
using Root = detail::StripeRoot;
using Pending = detail::StripePending;
using Error = StripeError;
namespace backing = core::detail;

/** Debit actual explicit root credits before work.
 * @param amount Required units.
 * @param remaining Mutable remaining allowance.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        throw StripeFailure{Error::WORK_LIMIT};
    }

    remaining -= amount;
}

/** Checked root byte/count sum.
 * @param left First count.
 * @param right Additional count.
 * @return Exact sum or detached resource refusal.
 */
std::size_t sum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw StripeFailure{Error::RESOURCE_LIMIT};
    }

    return left + right;
}

/** Checked root array request.
 * @param count Requested items.
 * @param bytes Bytes per item.
 * @return Exact bytes or resource refusal.
 */
std::size_t product(std::size_t count, std::size_t bytes) {
    if (bytes && count > std::numeric_limits<std::size_t>::max() / bytes) {
        throw StripeFailure{Error::RESOURCE_LIMIT};
    }

    return count * bytes;
}

/** Measure one operation from its own immutable capacities, never from a racing shared-ledger delta.
 * @param pending Unpublished owned candidate.
 * @return Conservative complete current operation backing, including retained client metadata and source overlap.
 */
std::size_t operationBacking(const Pending &pending) {
    auto bytes = sizeof(Pending);
    bytes = sum(bytes, product(pending.requests.capacity(), sizeof(detail::StripeRequest)));
    bytes = sum(bytes, product(pending.routed.capacity(), sizeof(core::SubscriptionRequest)));
    bytes = sum(bytes, product(pending.removals.capacity(), sizeof(core::SubscriptionKey)));
    bytes = sum(bytes, product(pending.ranges.capacity(), sizeof(detail::StripeRange)));
    bytes = sum(bytes, product(pending.indices.capacity(), sizeof(std::size_t)));
    bytes = sum(bytes, product(pending.clientLimits.capacity(), sizeof(ClientLimits)));
    bytes = sum(bytes, sum(pending.sourceBytes, pending.sliceBytes));

    for (const auto &request : pending.requests) {
        bytes = sum(bytes, product(sum(request.symbol.capacity(), 1), sizeof(char16_t)));
    }

    if (pending.client) {
        bytes = sum(bytes, pending.client->controlBytes);
        bytes = sum(bytes, product(pending.client->children.capacity(), sizeof(detail::StripePortEntry)));
        bytes = sum(bytes, pending.client->stripeHint ? pending.client->stripeHint->storageBytes() : 0);
    }

    return bytes;
}

/** Preserve cumulative known observations without wrap.
 * @param value Counter to update.
 * @param amount Actual observed additional count.
 * @param saturated Sticky overflow evidence.
 */
void accumulate(std::size_t &value, std::size_t amount, bool &saturated) noexcept {
    if (amount > std::numeric_limits<std::size_t>::max() - value) {
        value = std::numeric_limits<std::size_t>::max();
        saturated = true;
    } else {
        value += amount;
    }
}

/** Map the actual child failure without discarding its typed source cause.
 * @param cause Existing queue failure.
 * @param stripe Actual child index.
 * @return Detached composition refusal.
 */
StripeFailure childFailure(QueueFailure cause, std::size_t stripe) {
    return {cause.error == QueueError::WORK_LIMIT ? Error::WORK_LIMIT : Error::CHILD_FAILURE, stripe, cause};
}

/** Hold serialized execution through callbacks, including public-handle destruction.
 * @param root Existing stable root operation lease.
 */
class OperationGuard {
    Root &root_;

    public:
    explicit OperationGuard(Root &root) : root_(root) {
        if (root.executing) {
            throw StripeFailure{Error::REENTRANT};
        }

        root.executing = true;
    }

    ~OperationGuard() {
        root_.executing = false;
    }
};

/** Validate an idle root before preparing another operation.
 * @param root Serialized root.
 * @param work Caller root credits.
 */
void beginCheck(Root &root, StripedWork &work) {
    debit(1, work.operations);
    debit(sum(sizeof(Pending), product(root.results.size(), sizeof(StripeChildResult))), work.bytes);
    debit(root.results.size(), work.visits);

    if (root.stop || root.destroyed || root.closed) {
        throw StripeFailure{Error::CLOSED};
    }

    if (root.phase == StripePhase::RUNNING || root.phase == StripePhase::FAILED ||
        root.phase == StripePhase::CANCELLING || root.pending.unclaimedClient) {
        throw StripeFailure{Error::BUSY};
    }

    if (root.limits.operationBytes < sizeof(Pending)) {
        throw StripeFailure{Error::RESOURCE_LIMIT};
    }

    if (root.operationId == std::numeric_limits<std::uint64_t>::max()) {
        throw StripeFailure{Error::IDENTITY_EXHAUSTED};
    }
}

/** Locate a complete producer port belonging to one actual stripe.
 * @param port Existing complete logical client.
 * @param stripe Physical partition index.
 * @return Borrowed existing handle, or null for a disabled stripe.
 */
const MuxHandle *clientHandle(const detail::StripePort &port, std::size_t stripe) noexcept {
    for (const auto &entry : port.children) {
        if (entry.stripe == stripe) {
            return &entry.handle;
        }
    }

    return nullptr;
}

/** Validate complete logical client provenance without retaining an operational owner.
 * @param root Existing serialized root.
 * @param port Detached producer state supplied by a public token.
 */
void validateClient(const Root &root, const std::shared_ptr<detail::StripePort> &port) {
    if (!port || port->origin != root.origin) {
        throw StripeFailure{Error::INVALID_CLIENT};
    }
}

/** Query a pure partition with explicit root symbol examination credits.
 * @param root Existing immutable partition.
 * @param symbol Exact physical text.
 * @param work Caller remaining root credits.
 * @return Selected stripe or original partition refusal.
 */
std::size_t stripeIndex(const Root &root, std::u16string_view symbol, StripedWork &work) {
    debit(symbol.size(), work.textUnits);
    auto selected = root.striper.stripeIndex(symbol);

    if (!selected) {
        throw StripeFailure{Error::INVALID_SOURCE, {}, {}, {}, {}, selected.error()};
    }

    return *selected;
}

/** Route exact/All demand to candidate children; the stripe hint is not the independent hard filter.
 * @param root Existing immutable partition.
 * @param request Owned original command.
 * @param stripe Actual child index.
 * @param work Caller root credits.
 * @return Whether this child must receive the exact request.
 */
bool routedTo(const Root &root, const detail::StripeRequest &request, std::size_t stripe, StripedWork &work) {
    debit(1, work.visits);

    if (request.selector == core::SubscriptionSelectorKind::ALL) {
        return true;
    }

    const std::u16string_view symbol(request.symbol.data(), request.symbol.size());

    return stripeIndex(root, symbol, work) == stripe;
}

/** Preserve exactly the accepted operation after all preparation can no longer fail.
 * @param root Existing idle root.
 * @param operation Selected operation kind.
 * @param candidate Completely prepared owned input.
 * @param required Optional enabled-client mask; null means every child.
 * @return Accepted non-reused operation identity.
 */
std::uint64_t publish(Root &root, StripeOperation operation, Pending &&candidate,
                      const detail::StripePort *required = nullptr) noexcept {
    root.pending = std::move(candidate);
    root.operation = operation;
    root.phase = StripePhase::RUNNING;
    root.failure.reset();
    ++root.operationId;

    std::size_t enabled = 0;

    for (std::size_t stripe = 0; stripe < root.results.size(); ++stripe) {
        root.results[stripe] = {};
        root.results[stripe].stripe = stripe;
        root.results[stripe].required = !required;

        if (required && enabled < required->children.size() && required->children[enabled].stripe == stripe) {
            root.results[stripe].required = true;
            ++enabled;
        }

        if (operation == StripeOperation::STAGE) {
            root.results[stripe].sourceRecords = root.pending.ranges[stripe].count;
        }
    }

    return root.operationId;
}

/** Build one root and every independently configured actual child before exposing it.
 * @tparam Options One existing-family per-stripe configuration type.
 * @tparam Factory SDK-only child factory, invoked synchronously without user callbacks.
 * @param schema Complete immutable input provenance.
 * @param striper Owned immutable physical partition.
 * @param options Exact independent options for every stripe.
 * @param limits Root-only explicit grants.
 * @param work Actual root/child initialization credits.
 * @param family Fixed native family.
 * @param factory Actual existing MuxQueue factory.
 * @return Complete operational root or atomic refusal.
 */
template <class Options, class Factory>
std::expected<std::shared_ptr<Root>, StripeFailure>
createRoot(const core::LocalSchema &schema, core::SymbolStriper striper, std::span<const Options> options,
           StripedLimits limits, StripedWork &work, core::SubscriptionFamily family, Factory factory) {
    try {
        debit(1, work.operations);

        if (!schema.identity().valid() || !striper.valid() || options.size() != striper.stripeCount() ||
            options.empty()) {
            throw StripeFailure{Error::INVALID_CONFIGURATION};
        }

        debit(backing::RecordByteBudget::metadataAllocationBytes(), work.bytes);
        backing::RecordByteBudget ledger(limits.residentBytes);
        const auto rootControl =
            backing::recordControlBytes<Root>(ledger, schema, core::SymbolStriper(striper), limits, family);
        const auto originControl = backing::recordControlBytes<detail::StripeOrigin>();
        const auto fixed = sum(sum(rootControl, originControl),
                               product(options.size(), sizeof(detail::StripeChild) + sizeof(StripeChildResult)));
        const auto minimum = sum(fixed, sum(schema.storageBytes(), striper.storageBytes()));

        if (minimum > limits.residentBytes) {
            throw StripeFailure{Error::RESOURCE_LIMIT};
        }

        debit(minimum, work.bytes);
        debit(options.size(), work.visits);

        for (const auto &record : schema.records()) {
            debit(sum(1, sum(record.fields.size(), record.lookups.size())), work.visits);
        }

        auto root = std::allocate_shared<Root>(detail::StripeAllocator<Root>{ledger.state()}, ledger,
                                               core::LocalSchema(schema), std::move(striper), limits, family);
        root->children.reserve(options.size());
        root->results.resize(options.size());

        for (std::size_t stripe = 0; stripe < options.size(); ++stripe) {
            auto child = factory(options[stripe]);

            if (!child) {
                throw childFailure(child.error(), stripe);
            }

            root->children.emplace_back(std::move(*child));
            root->results[stripe].stripe = stripe;
        }

        return root;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (backing::RecordBlockError error) {
        return std::unexpected(StripeFailure{error == backing::RecordBlockError::ALLOCATION_FAILURE
                                                 ? Error::ALLOCATION_FAILURE
                                                 : Error::RESOURCE_LIMIT});
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

/** Inspect truthful current operation progress without advancing an owner.
 * @param root Stable root or null after a move.
 * @return Borrowed exact operation cut.
 */
StripedProgress progressOf(const Root *root) noexcept {
    if (!root) {
        return {0,   StripeOperation::NONE, StripePhase::IDLE, 0, 0, {}, StripeFailure{Error::INVALID_STATE}, false,
                true};
    }

    std::size_t completed = 0, required = 0;

    for (const auto &child : root->results) {
        required += child.required;
        completed += child.required && child.completed;
    }

    return {root->operationId,
            root->operation,
            root->phase,
            completed,
            required,
            root->destroyed ? std::span<const StripeChildResult>{} : std::span<const StripeChildResult>{root->results},
            root->failure,
            root->stop.has_value() || root->destroyed,
            root->closed};
}
} // namespace

StripeClient::StripeClient(std::shared_ptr<detail::StripePort> port) noexcept : port_(std::move(port)) {
    if (port_ &&
        port_->publicReferences.fetch_add(1, std::memory_order_relaxed) == std::numeric_limits<std::size_t>::max()) {
        std::terminate();
    }
}

StripeClient::StripeClient(const StripeClient &other) noexcept : StripeClient(other.port_) {
}

StripeClient::StripeClient(StripeClient &&other) noexcept = default;

StripeClient &StripeClient::operator=(const StripeClient &other) noexcept {
    if (this != &other) {
        StripeClient replacement(other);
        port_.swap(replacement.port_);
    }

    return *this;
}

StripeClient &StripeClient::operator=(StripeClient &&other) noexcept {
    if (this != &other) {
        StripeClient retired;
        retired.port_ = std::move(port_);
        port_ = std::move(other.port_);
    }

    return *this;
}

StripeClient::~StripeClient() {
    if (port_ && port_->publicReferences.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        requestStop(StopMode::ABORT);
    }
}

bool StripeClient::valid() const noexcept {
    return bool(port_);
}

std::size_t StripeClient::stripeCount() const noexcept {
    return port_ ? port_->children.size() : 0;
}

void StripeClient::requestStop(StopMode mode) const noexcept {
    if (port_) {
        for (const auto &entry : port_->children) {
            auto handle = entry.handle;
            (void)handle.requestStop(mode);
        }
    }
}

void StripeClient::requestResume() const noexcept {
    if (port_) {
        for (const auto &entry : port_->children) {
            auto handle = entry.handle;
            (void)handle.requestResume();
        }
    }
}

ControlAdmission StripeClient::acknowledgeReset(std::size_t stripe, std::uint64_t sequence) const noexcept {
    if (port_) {
        if (const auto *selected = clientHandle(*port_, stripe)) {
            auto handle = *selected;

            return handle.acknowledgeReset(sequence);
        }
    }

    return ControlAdmission::CLOSED;
}

std::optional<QueueResetNotice> StripeClient::resetNotice(std::size_t stripe) const {
    const auto *handle = port_ ? clientHandle(*port_, stripe) : nullptr;

    return handle ? handle->resetNotice() : std::nullopt;
}

std::optional<QueueTerminal> StripeClient::terminal(std::size_t stripe) const {
    const auto *handle = port_ ? clientHandle(*port_, stripe) : nullptr;

    return handle ? handle->terminal() : std::nullopt;
}

StripedCollector::StripedCollector(std::shared_ptr<detail::StripeRoot> state) noexcept : state_(std::move(state)) {
}

StripedCollector::StripedCollector(StripedCollector &&other) noexcept = default;

StripedCollector &StripedCollector::operator=(StripedCollector &&other) noexcept {
    if (this != &other) {
        auto old = std::move(state_);

        if (old) {
            old->destroyed = true;
            old->stop = StopMode::ABORT;
        }

        state_ = std::move(other.state_);
    }

    return *this;
}

StripedCollector::~StripedCollector() {
    auto old = std::move(state_);

    if (old) {
        old->destroyed = true;
        old->stop = StopMode::ABORT;
    }
}

std::expected<StripedCollector, StripeFailure>
StripedCollector::createStream(const core::LocalSchema &schema, core::SymbolStriper striper,
                               std::span<const StreamStripeOptions> options, StripedLimits limits, StripedWork &work) {
    auto state = createRoot(schema, std::move(striper), options, limits, work, core::SubscriptionFamily::STREAM,
                            [&](const StreamStripeOptions &option) {
                                return MuxQueue::createStream(schema, option.interests, option.desired, option.source,
                                                              option.queue, work.child, option.selectorPolicy);
                            });

    if (!state) {
        return std::unexpected(std::move(state.error()));
    }

    return StripedCollector(std::move(*state));
}

std::expected<StripedCollector, StripeFailure>
StripedCollector::createTicker(const core::LocalSchema &schema, core::SymbolStriper striper,
                               std::span<const TickerStripeOptions> options, StripedLimits limits, StripedWork &work) {
    auto state = createRoot(schema, std::move(striper), options, limits, work, core::SubscriptionFamily::TICKER,
                            [&](const TickerStripeOptions &option) {
                                return MuxQueue::createTicker(schema, option.interests, option.source, option.options,
                                                              option.queue, work.child);
                            });

    if (!state) {
        return std::unexpected(std::move(state.error()));
    }

    return StripedCollector(std::move(*state));
}

std::expected<StripedCollector, StripeFailure>
StripedCollector::createHistory(const core::LocalSchema &schema, core::SymbolStriper striper,
                                std::span<const HistoryStripeOptions> options, StripedLimits limits,
                                StripedWork &work) {
    auto state = createRoot(schema, std::move(striper), options, limits, work, core::SubscriptionFamily::HISTORY,
                            [&](const HistoryStripeOptions &option) {
                                return MuxQueue::createHistory(schema, option.interests, option.source, option.options,
                                                               option.queue, work.child);
                            });

    if (!state) {
        return std::unexpected(std::move(state.error()));
    }

    return StripedCollector(std::move(*state));
}

StripedProgress StripedCollector::progress() const noexcept {
    return progressOf(state_.get());
}

void StripedCollector::requestStop(StopMode mode) noexcept {
    if (state_ && (!state_->stop || mode == StopMode::ABORT)) {
        state_->stop = mode;
    }
}

std::optional<core::SubscriptionFamily> StripedCollector::family() const noexcept {
    return state_ ? std::optional{state_->family} : std::nullopt;
}

std::size_t StripedCollector::stripeCount() const noexcept {
    return state_ ? state_->children.size() : 0;
}

std::span<const core::SubscriptionRequest> StripedCollector::aggregateEntries(std::size_t stripe) const noexcept {
    return state_ && stripe < state_->children.size() ? state_->children[stripe].queue.aggregateEntries()
                                                      : std::span<const core::SubscriptionRequest>{};
}

std::expected<std::uint64_t, StripeFailure>
StripedCollector::beginOpen(std::span<const ClientLimits> limits, std::shared_ptr<MuxConsumer> consumer,
                            std::optional<core::SymbolStripeFilter> stripeHint, StripedWork &work) {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        beginCheck(*state, work);

        if (!consumer || limits.size() != state->children.size() || (stripeHint && !stripeHint->valid())) {
            throw StripeFailure{Error::INVALID_CONFIGURATION};
        }

        const auto control = backing::recordControlBytes<detail::StripePort>(state->ledger, state->origin, stripeHint);
        debit(sum(control, sum(product(limits.size(), sizeof(ClientLimits) + sizeof(detail::StripePortEntry)),
                               stripeHint ? stripeHint->storageBytes() : 0)),
              work.bytes);
        debit(limits.size(), work.visits);
        Pending candidate(state->ledger);
        candidate.clientLimits.assign(limits.begin(), limits.end());
        candidate.client =
            std::allocate_shared<detail::StripePort>(detail::StripeAllocator<detail::StripePort>{state->ledger.state()},
                                                     state->ledger, state->origin, std::move(stripeHint));
        candidate.client->controlBytes = control;
        candidate.client->children.reserve(limits.size());
        candidate.consumer = std::move(consumer);
        candidate.unclaimedClient = true;
        std::optional<core::StripeIntersection> overlap;

        if (candidate.client->stripeHint) {
            const auto selected = state->striper.intersectingStripes(*candidate.client->stripeHint);

            if (!selected) {
                throw StripeFailure{Error::INVALID_CONFIGURATION, {}, {}, {}, {}, selected.error()};
            }

            overlap = *selected;
        }

        for (std::size_t stripe = 0; stripe < limits.size(); ++stripe) {
            if (!overlap || (stripe >= overlap->first && stripe - overlap->first < overlap->count)) {
                candidate.client->children.push_back({stripe, {}});
            }
        }

        if (operationBacking(candidate) > state->limits.operationBytes) {
            throw StripeFailure{Error::RESOURCE_LIMIT};
        }

        const auto *enabled = candidate.client.get();
        const auto id = publish(*state, StripeOperation::OPEN, std::move(candidate), enabled);

        return id;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (backing::RecordBlockError) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::expected<StripeClient, StripeFailure> StripedCollector::takeClient() {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);

        if (state->operation != StripeOperation::OPEN || state->phase != StripePhase::COMPLETE ||
            !state->pending.unclaimedClient || !state->pending.client) {
            throw StripeFailure{Error::INVALID_OPERATION};
        }

        state->pending.unclaimedClient = false;

        return StripeClient(std::move(state->pending.client));
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    }
}

std::expected<std::uint64_t, StripeFailure>
StripedCollector::beginCommand(StripeOperation operation, const StripeClient &client,
                               std::span<const core::SubscriptionRequest> requests,
                               std::span<const core::SubscriptionKey> keys, StripedWork &work) {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        beginCheck(*state, work);
        validateClient(*state, client.port_);
        const auto count = operation == StripeOperation::REMOVE ? keys.size() : requests.size();
        debit(sum(product(count, sizeof(detail::StripeRequest)),
                  product(state->children.size(), sizeof(detail::StripeRange))),
              work.bytes);
        Pending candidate(state->ledger);
        candidate.client = client.port_;
        candidate.requests.reserve(count);
        candidate.ranges.resize(state->children.size());

        for (std::size_t index = 0; index < count; ++index) {
            const auto request = operation == StripeOperation::REMOVE
                                     ? core::SubscriptionRequest{keys[index], std::nullopt}
                                     : requests[index];
            debit(1, work.visits);
            debit(request.key.symbol.size(), work.textUnits);
            debit(product(sum(request.key.symbol.size(), 1), sizeof(char16_t)), work.bytes);

            if (request.key.symbol.data() == nullptr ||
                (request.key.selector != core::SubscriptionSelectorKind::EXACT &&
                 request.key.selector != core::SubscriptionSelectorKind::ALL)) {
                throw StripeFailure{Error::INVALID_CONFIGURATION};
            }

            candidate.requests.emplace_back(request, state->ledger);
        }

        std::size_t routed = 0;

        for (const auto &entry : candidate.client->children) {
            auto &range = candidate.ranges[entry.stripe];
            range.first = routed;

            for (const auto &request : candidate.requests) {
                if (routedTo(*state, request, entry.stripe, work)) {
                    ++range.count;
                    routed = sum(routed, 1);
                }
            }
        }

        debit(product(routed, operation == StripeOperation::REMOVE ? sizeof(core::SubscriptionKey)
                                                                   : sizeof(core::SubscriptionRequest)),
              work.bytes);

        if (operation == StripeOperation::REMOVE) {
            candidate.removals.reserve(routed);
        } else {
            candidate.routed.reserve(routed);
        }

        for (const auto &entry : candidate.client->children) {
            for (const auto &request : candidate.requests) {
                if (routedTo(*state, request, entry.stripe, work)) {
                    if (operation == StripeOperation::REMOVE) {
                        candidate.removals.push_back(request.borrow().key);
                    } else {
                        candidate.routed.push_back(request.borrow());
                    }
                }
            }
        }

        if (operationBacking(candidate) > state->limits.operationBytes) {
            throw StripeFailure{Error::RESOURCE_LIMIT};
        }

        const auto *enabled = candidate.client.get();

        return publish(*state, operation, std::move(candidate), enabled);
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (backing::RecordBlockError) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::expected<std::uint64_t, StripeFailure>
StripedCollector::beginAdd(const StripeClient &client, std::span<const core::SubscriptionRequest> requests,
                           StripedWork &work) {
    return beginCommand(StripeOperation::ADD, client, requests, {}, work);
}

std::expected<std::uint64_t, StripeFailure> StripedCollector::beginRemove(const StripeClient &client,
                                                                          std::span<const core::SubscriptionKey> keys,
                                                                          StripedWork &work) {
    return beginCommand(StripeOperation::REMOVE, client, {}, keys, work);
}

std::expected<std::uint64_t, StripeFailure>
StripedCollector::beginReplace(const StripeClient &client, std::span<const core::SubscriptionRequest> requests,
                               StripedWork &work) {
    return beginCommand(StripeOperation::REPLACE, client, requests, {}, work);
}

std::expected<std::uint64_t, StripeFailure> StripedCollector::beginClear(const StripeClient &client,
                                                                         StripedWork &work) {
    return beginCommand(StripeOperation::CLEAR, client, {}, {}, work);
}

std::expected<std::uint64_t, StripeFailure> StripedCollector::beginStage(protocol::LiveEvent &&event,
                                                                         StripedWork &work) {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        beginCheck(*state, work);
        const auto message = state->family == core::SubscriptionFamily::STREAM ? protocol::LiveDataMessage::STREAM_DATA
                             : state->family == core::SubscriptionFamily::TICKER
                                 ? protocol::LiveDataMessage::TICKER_DATA
                                 : protocol::LiveDataMessage::HISTORY_DATA;

        if (event.kind() != protocol::LiveEventKind::DATA ||
            event.messageType() != static_cast<std::int32_t>(message)) {
            throw StripeFailure{Error::INVALID_SOURCE};
        }

        debit(sum(product(state->children.size(), sizeof(detail::StripeRange)),
                  product(event.records().size(), sizeof(std::size_t))),
              work.bytes);
        Pending candidate(state->ledger);
        candidate.ranges.resize(state->children.size());
        candidate.indices.resize(event.records().size());

        for (const auto &record : event.records()) {
            debit(1, work.visits);

            if (!state->schema.recordId(record.context)) {
                throw StripeFailure{Error::INVALID_SOURCE};
            }

            ++candidate.ranges[stripeIndex(*state, record.symbol, work)].count;
        }

        std::size_t first = 0;

        for (auto &range : candidate.ranges) {
            range.first = first;
            first = sum(first, range.count);
            range.count = 0;
        }

        for (std::size_t index = 0; index < event.records().size(); ++index) {
            debit(1, work.visits);
            auto &range = candidate.ranges[stripeIndex(*state, event.records()[index].symbol, work)];
            candidate.indices[range.first + range.count++] = index;
        }

        const protocol::detail::StripeEventLimits sliceLimits{state->limits.sliceMetadataBytes,
                                                              state->limits.sliceCopiedPayloadBytes,
                                                              state->limits.sliceRetainedPayloadBytes};
        std::size_t largest = 0;

        for (std::size_t stripe = 0; stripe < candidate.ranges.size(); ++stripe) {
            const auto range = candidate.ranges[stripe];

            if (!range.count) {
                continue;
            }

            const auto measured = protocol::detail::StripeEventAccess::measure(
                event, std::span(candidate.indices).subspan(range.first, range.count), sliceLimits, work.event);

            if (!measured) {
                throw StripeFailure{measured.error() == protocol::LiveError::WORK_LIMIT ? Error::WORK_LIMIT
                                                                                        : Error::INVALID_SOURCE,
                                    stripe,
                                    {},
                                    {},
                                    measured.error()};
            }

            largest = std::max(largest, sum(measured->metadataBytes,
                                            sum(measured->copiedPayloadBytes, measured->retainedPayloadBytes)));
        }

        const auto overlap = sum(event.residentBytes(), largest);
        debit(overlap, work.bytes);
        candidate.sourceBytes = event.residentBytes();
        candidate.sliceBytes = largest;
        candidate.sourceCharge = backing::RecordByteReservation(state->ledger.state(), event.residentBytes());
        candidate.sliceCharge = backing::RecordByteReservation(state->ledger.state(), largest);

        if (operationBacking(candidate) > state->limits.operationBytes) {
            throw StripeFailure{Error::RESOURCE_LIMIT};
        }

        candidate.source.emplace(std::move(event));

        return publish(*state, StripeOperation::STAGE, std::move(candidate));
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (backing::RecordBlockError) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::expected<std::uint64_t, StripeFailure> StripedCollector::beginReset(StripedWork &work) {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        beginCheck(*state, work);
        Pending candidate(state->ledger);

        return publish(*state, StripeOperation::RESET, std::move(candidate));
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    }
}

std::expected<void, StripeFailure> StripedCollector::requestCancel(std::uint64_t operationId) noexcept {
    if (!state_) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    if (state_->operationId != operationId ||
        (state_->phase != StripePhase::RUNNING && state_->phase != StripePhase::FAILED &&
         state_->phase != StripePhase::CANCELLING)) {
        return std::unexpected(StripeFailure{Error::INVALID_OPERATION});
    }

    state_->pending.cancel = true;
    state_->phase = StripePhase::CANCELLING;

    return {};
}

namespace {
/** Release only the current operation input after funding its complete owned disposal.
 * @param root Stable serialized root.
 * @param cancelled Whether unsubmitted source rows are being excluded by explicit cutoff.
 * @param work Actual disposal credits.
 */
void finishOperation(Root &root, bool cancelled, StripedWork &work) {
    auto &pending = root.pending;
    debit(sum(root.children.size(), pending.requests.size()), work.visits);
    auto bytes = sum(product(pending.requests.capacity(), sizeof(detail::StripeRequest)),
                     sum(product(pending.routed.capacity(), sizeof(core::SubscriptionRequest)),
                         product(pending.removals.capacity(), sizeof(core::SubscriptionKey))));
    bytes = sum(bytes, sum(product(pending.indices.capacity(), sizeof(std::size_t)),
                           sum(product(pending.ranges.capacity(), sizeof(detail::StripeRange)),
                               product(pending.clientLimits.capacity(), sizeof(ClientLimits)))));

    for (const auto &request : pending.requests) {
        bytes = sum(bytes, product(sum(request.symbol.capacity(), 1), sizeof(char16_t)));
    }

    if (pending.source) {
        bytes = sum(bytes, pending.source->releaseBytes());
    }

    if (pending.slice) {
        bytes = sum(bytes, pending.slice->releaseBytes());
    }

    debit(bytes, work.bytes);

    if (cancelled && root.operation == StripeOperation::OPEN && pending.client) {
        for (const auto &entry : pending.client->children) {
            auto handle = entry.handle;
            (void)handle.requestStop(StopMode::ABORT);
        }
    }

    if (cancelled && root.operation == StripeOperation::STAGE) {
        for (const auto &child : root.results) {
            if (!child.submitted) {
                accumulate(root.unsubmittedSourceRecords, child.sourceRecords, root.countsSaturated);
            }
        }
    }

    Pending empty(root.ledger);

    if (!cancelled && root.operation == StripeOperation::OPEN) {
        empty.client = std::move(pending.client);
        empty.unclaimedClient = true;
    }

    // Swap buffers within the same ledger: moving the reusable slot's allocators would make later
    // vector assignment relocate request objects and invalidate routed SSO string views.
    // The retired empty object releases payloads before its source/slice reservations.
    pending.swap(empty);
    root.phase = cancelled ? StripePhase::CANCELLED : StripePhase::COMPLETE;

    if (!cancelled) {
        root.failure.reset();
    }
}

/** Remember a retryable or permanent real child refusal without reapplying completed children.
 * @param root Stable root.
 * @param stripe Actual current child.
 * @param failure Original detached failure.
 * @param retry Whether a later explicit turn can retry this same uncompleted child action.
 */
void operationFailure(Root &root, std::size_t stripe, StripeFailure failure, bool retry) {
    root.failure = failure;
    root.results[stripe].failure = std::move(failure);

    if (!retry && root.phase != StripePhase::CANCELLING) {
        root.phase = StripePhase::FAILED;
    }
}

/** Recognize existing producer pressure that can clear without changing the accepted root input.
 * @param error Original immediate command refusal.
 * @return Whether the current child can remain a continuing operation.
 */
bool retrySubmission(QueueError error) noexcept {
    return error == QueueError::WORK_LIMIT || error == QueueError::BUSY || error == QueueError::COMMAND_LIMIT ||
           error == QueueError::RESULT_LIMIT;
}

/** Drive one existing child action, preserving its error and callback cutoff semantics.
 * @param root Stable operation lease.
 * @param stripe Actual child.
 * @param work Actual owner credits.
 * @param callbacks Whether an already admitted consumer callback may start in this step.
 * @return Existing actual owner action or original nested failure.
 */
std::expected<QueueProgress, StripeFailure> childStep(Root &root, std::size_t stripe, StripedWork &work,
                                                      bool callbacks = true) {
    const auto result = callbacks ? root.children[stripe].queue.progress(work.child)
                                  : root.children[stripe].queue.progressWithoutCallbacks(work.child);

    if (!result) {
        return std::unexpected(childFailure(result.error(), stripe));
    }

    return *result;
}

/** Advance an already accepted command result, never submitting a replacement ticket.
 * @param root Stable root with current ticket.
 * @param stripe Current actual child.
 * @param work Actual continuing credits.
 * @param cancel Whether the root is resolving an accepted action after cutoff.
 */
void resolveTicket(Root &root, std::size_t stripe, StripedWork &work, bool cancel) {
    auto &pending = root.pending;
    auto result = pending.ticket->result();

    if (!result) {
        const auto stepped = childStep(root, stripe, work, !cancel);

        if (!stepped) {
            operationFailure(root, stripe, stepped.error(), stepped.error().error == Error::WORK_LIMIT);

            return;
        }

        if (root.destroyed) {
            return;
        }

        result = pending.ticket->result();
    }

    if (!result) {
        return;
    }

    auto &outcome = root.results[stripe];
    outcome.command = *result;
    pending.ticket.reset();

    if (result->status == CommandStatus::APPLIED) {
        outcome.completed = true;
        ++pending.cursor;
    } else if (!cancel) {
        operationFailure(root, stripe, StripeFailure{Error::CHILD_FAILURE, stripe}, false);
    }
}

/** Advance cancellation without claiming rollback or discarding a completed source stripe.
 * @param root Stable root; no later child may be submitted.
 * @param work Actual continuing credits.
 */
void cancelStep(Root &root, StripedWork &work) {
    auto &pending = root.pending;
    const auto stripe = pending.cursor;

    if (stripe < root.children.size() && pending.ticket) {
        resolveTicket(root, stripe, work, true);

        if (pending.ticket || root.destroyed) {
            return;
        }
    }

    if (root.operation == StripeOperation::STAGE && stripe < root.children.size() && root.results[stripe].submitted &&
        !root.results[stripe].completed) {
        auto &queue = root.children[stripe].queue;

        if (!pending.discardRequested) {
            queue.requestDiscardPendingSource();
            pending.discardRequested = true;

            return;
        }

        if (queue.resources().pendingRecords) {
            const auto stepped = childStep(root, stripe, work, false);

            if (!stepped) {
                operationFailure(root, stripe, stepped.error(), true);
            }

            return;
        }
    }

    if (root.operation == StripeOperation::RESET && stripe < root.children.size() && root.results[stripe].submitted &&
        !root.results[stripe].completed) {
        const auto status = root.children[stripe].queue.resetStatus();

        if (status.sourceCommitted && status.globalAcknowledged) {
            root.results[stripe].completed = true;
        } else if (status.phase != ResetPhase::FAILED && !root.children[stripe].queue.terminal()) {
            const auto stepped = childStep(root, stripe, work, false);

            if (!stepped) {
                operationFailure(root, stripe, stepped.error(), true);
            }

            return;
        }
    }

    finishOperation(root, true, work);
}
} // namespace

std::expected<StripedProgress, StripeFailure> StripedCollector::advance(StripedWork &work) {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        debit(sum(1, product(state->children.size(), 2)), work.visits);

        if (state->closed) {
            return progressOf(state.get());
        }

        if (state->stop) {
            const auto intent = *state->stop == StopMode::ABORT ? 2U : 1U;

            for (std::size_t stripe = 0; stripe < state->children.size(); ++stripe) {
                debit(1, work.visits);

                if (state->children[stripe].stopSent < intent) {
                    state->children[stripe].queue.requestStop(*state->stop);
                    state->children[stripe].stopSent = intent;

                    return progressOf(state.get());
                }
            }

            if (state->phase == StripePhase::RUNNING || state->phase == StripePhase::FAILED) {
                state->pending.cancel = true;
                state->phase = StripePhase::CANCELLING;
            }
        }

        if (state->phase == StripePhase::CANCELLING || state->pending.cancel) {
            cancelStep(*state, work);

            return progressOf(state.get());
        }

        if (state->stop) {
            const auto stripe = state->closeCursor;
            state->closeCursor = (stripe + 1) % state->children.size();
            const auto stepped = childStep(*state, stripe, work);

            if (!stepped && stepped.error().error != Error::WORK_LIMIT && !state->failure) {
                state->failure = stepped.error();
            }

            if (state->destroyed) {
                return progressOf(state.get());
            }

            debit(state->children.size(), work.visits);
            state->closed = std::ranges::all_of(state->children, [](const auto &child) {
                return child.queue.terminal().has_value();
            });

            if (!stepped && stepped.error().error == Error::WORK_LIMIT) {
                return std::unexpected(stepped.error());
            }

            return progressOf(state.get());
        }

        if (state->phase != StripePhase::RUNNING) {
            const auto stripe = state->deliveryCursor;
            state->deliveryCursor = (stripe + 1) % state->children.size();
            const auto stepped = childStep(*state, stripe, work);

            if (!stepped && state->phase != StripePhase::FAILED) {
                return std::unexpected(stepped.error());
            }

            return progressOf(state.get());
        }

        auto &pending = state->pending;

        while (pending.cursor < state->children.size() && !state->results[pending.cursor].required) {
            debit(1, work.visits);
            ++pending.cursor;
        }

        if (pending.cursor == state->children.size()) {
            finishOperation(*state, false, work);

            return progressOf(state.get());
        }

        const auto stripe = pending.cursor;
        auto &outcome = state->results[stripe];
        auto &child = state->children[stripe];
        state->failure.reset();
        outcome.failure.reset();
        accumulate(outcome.attempts, 1, child.saturated);

        if (state->operation == StripeOperation::OPEN) {
            auto opened = child.queue.open(pending.clientLimits[stripe], pending.consumer, work.child);

            if (!opened) {
                operationFailure(*state, stripe, childFailure(opened.error(), stripe),
                                 opened.error().error == QueueError::WORK_LIMIT);
            } else {
                const auto position =
                    std::ranges::find(pending.client->children, stripe, &detail::StripePortEntry::stripe);
                position->handle = std::move(*opened);
                outcome.submitted = true;
                outcome.completed = true;
                ++pending.cursor;
            }
        } else if (state->operation == StripeOperation::STAGE) {
            const auto range = pending.ranges[stripe];

            if (!range.count) {
                outcome.completed = true;
                ++pending.cursor;
            } else if (!outcome.submitted) {
                if (!pending.slice) {
                    const protocol::detail::StripeEventLimits bounds{state->limits.sliceMetadataBytes,
                                                                     state->limits.sliceCopiedPayloadBytes,
                                                                     state->limits.sliceRetainedPayloadBytes};
                    auto selected = protocol::detail::StripeEventAccess::select(
                        *pending.source, std::span(pending.indices).subspan(range.first, range.count), bounds,
                        work.event);

                    if (!selected) {
                        operationFailure(*state, stripe,
                                         StripeFailure{selected.error() == protocol::LiveError::WORK_LIMIT
                                                           ? Error::WORK_LIMIT
                                                           : Error::INVALID_SOURCE,
                                                       stripe,
                                                       {},
                                                       {},
                                                       selected.error()},
                                         selected.error() == protocol::LiveError::WORK_LIMIT);

                        return progressOf(state.get());
                    }

                    pending.slice.emplace(std::move(*selected));
                }

                auto admitted = child.queue.admit(std::move(*pending.slice), work.child);

                if (!admitted) {
                    if (admitted.error().error == QueueError::BUSY) {
                        const auto stepped = childStep(*state, stripe, work);

                        if (!stepped) {
                            operationFailure(*state, stripe, stepped.error(),
                                             stepped.error().error == Error::WORK_LIMIT);
                        }
                    } else {
                        operationFailure(*state, stripe, childFailure(admitted.error(), stripe),
                                         admitted.error().error == QueueError::WORK_LIMIT);
                    }
                } else {
                    outcome.submitted = true;
                    outcome.admission = *admitted;
                    outcome.admittedRecords = range.count;
                    accumulate(child.accepted, range.count, child.saturated);
                    pending.slice.reset();
                }
            } else if (pending.stageClosing) {
                if (const auto terminal = child.queue.terminal()) {
                    operationFailure(*state, stripe,
                                     terminal->primary ? childFailure(*terminal->primary, stripe)
                                                       : StripeFailure{Error::CLOSED, stripe},
                                     false);
                } else {
                    const auto stepped = childStep(*state, stripe, work, false);

                    if (!stepped) {
                        operationFailure(*state, stripe, stepped.error(), stepped.error().error == Error::WORK_LIMIT);
                    }
                }
            } else {
                const auto stepped = childStep(*state, stripe, work);

                if (!stepped) {
                    operationFailure(*state, stripe, stepped.error(), stepped.error().error == Error::WORK_LIMIT);
                } else if (stepped->state != QueueState::OPEN) {
                    // The existing queue publishes a source failure through continuing closure, not an immediate error.
                    pending.stageClosing = true;
                    state->failure = StripeFailure{Error::INVALID_SOURCE, stripe};
                } else if (stepped->kind == QueueProgressKind::STAGED || stepped->kind == QueueProgressKind::RELEASED) {
                    // A successful STREAM stage with no eligible rows reports RELEASED and must not be retried.
                    outcome.completed = true;
                    outcome.stagedRecords = range.count;
                    accumulate(child.staged, range.count, child.saturated);
                    ++pending.cursor;
                }
            }
        } else if (state->operation == StripeOperation::RESET) {
            if (!outcome.submitted) {
                const auto admitted = child.queue.resetSource(work.child);

                if (!admitted) {
                    operationFailure(*state, stripe, childFailure(admitted.error(), stripe),
                                     admitted.error().error == QueueError::WORK_LIMIT);
                } else {
                    outcome.submitted = true;
                    outcome.reset = *admitted;
                }
            } else {
                const auto status = child.queue.resetStatus();

                if (status.sourceCommitted && status.globalAcknowledged) {
                    outcome.completed = true;
                    ++pending.cursor;
                } else if (status.phase == ResetPhase::FAILED && status.failure) {
                    operationFailure(*state, stripe, childFailure(*status.failure, stripe), false);
                } else {
                    const auto stepped = childStep(*state, stripe, work, false);

                    if (!stepped) {
                        operationFailure(*state, stripe, stepped.error(), stepped.error().error == Error::WORK_LIMIT);
                    }
                }
            }
        } else if (!pending.ticket) {
            auto handle = *clientHandle(*pending.client, stripe);
            const auto range = pending.ranges[stripe];
            const auto submitted = [&]() -> std::expected<MuxCommandTicket, QueueError> {
                switch (state->operation) {
                case StripeOperation::ADD:
                    return handle.add(std::span(pending.routed).subspan(range.first, range.count), work.submit);
                case StripeOperation::REMOVE:
                    return handle.remove(std::span(pending.removals).subspan(range.first, range.count), work.submit);
                case StripeOperation::REPLACE:
                    return handle.replace(std::span(pending.routed).subspan(range.first, range.count), work.submit);
                case StripeOperation::CLEAR:
                    return handle.clear(work.submit);
                default:
                    return std::unexpected(QueueError::INVALID_CONFIGURATION);
                }
            }();

            if (!submitted) {
                const bool retry = retrySubmission(submitted.error());
                operationFailure(*state, stripe,
                                 StripeFailure{submitted.error() == QueueError::WORK_LIMIT ? Error::WORK_LIMIT
                                                                                           : Error::CHILD_FAILURE,
                                               stripe,
                                               {},
                                               submitted.error()},
                                 retry);

                if (retry && submitted.error() != QueueError::WORK_LIMIT) {
                    const auto stepped = childStep(*state, stripe, work);

                    if (!stepped) {
                        operationFailure(*state, stripe, stepped.error(), stepped.error().error == Error::WORK_LIMIT);
                    }
                }
            } else {
                pending.ticket.emplace(std::move(*submitted));
                outcome.submitted = true;
            }
        } else {
            resolveTicket(*state, stripe, work, false);
        }

        // The operation lease names this old root even if a callback replaced or destroyed its public handle.
        return progressOf(state.get());
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (backing::RecordBlockError) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::size_t StripedCollector::residentBytes() const noexcept {
    return state_ ? state_->ledger.usedBytes() : 0;
}

std::expected<std::vector<StripeObservation>, StripeFailure> StripedCollector::observations(StripedWork &work) const {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        debit(state->children.size(), work.visits);
        const auto bytes = product(state->children.size(), sizeof(StripeObservation));

        if (bytes > state->limits.operationBytes) {
            throw StripeFailure{Error::RESOURCE_LIMIT};
        }

        debit(bytes, work.bytes);
        std::vector<StripeObservation> result;
        result.reserve(state->children.size());

        for (std::size_t stripe = 0; stripe < state->children.size(); ++stripe) {
            const auto &child = state->children[stripe];
            result.push_back({stripe, child.queue.resources(), child.queue.capacity(), child.queue.state(),
                              child.queue.revision(), child.queue.aggregateEntries().size(), child.queue.resetStatus(),
                              child.queue.terminal(), child.accepted, child.staged, child.saturated});
        }

        return result;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::expected<StripedSummary, StripeFailure> StripedCollector::summary(StripedWork &work) const {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        debit(state->children.size(), work.visits);
        debit(sizeof(StripedSummary), work.bytes);
        StripedSummary result;
        result.operationId = state->operationId;
        result.stripes = state->children.size();
        result.unsubmittedSourceRecords = state->unsubmittedSourceRecords;
        result.countsSaturated = state->countsSaturated;
        result.hadGap = state->unsubmittedSourceRecords != 0;
        QueueCounts total;
        const std::array members{&QueueCounts::acceptedFrames,
                                 &QueueCounts::acceptedRecords,
                                 &QueueCounts::collectorAcknowledged,
                                 &QueueCounts::queueAdmitted,
                                 &QueueCounts::withdrawn,
                                 &QueueCounts::discarded,
                                 &QueueCounts::sourceRecordsDiscarded,
                                 &QueueCounts::callbackStartedBatches,
                                 &QueueCounts::callbackStartedRecords,
                                 &QueueCounts::callbackCompletedBatches,
                                 &QueueCounts::callbackCompletedRecords,
                                 &QueueCounts::callbackFailedBatches,
                                 &QueueCounts::callbackFailedRecords,
                                 &QueueCounts::commandsApplied,
                                 &QueueCounts::commandsRejected,
                                 &QueueCounts::commandsCancelled};

        for (const auto &child : state->children) {
            accumulate(result.desiredEntries, child.queue.aggregateEntries().size(), result.countsSaturated);
            accumulate(result.acceptedSourceRecords, child.accepted, result.countsSaturated);
            accumulate(result.stagedSourceRecords, child.staged, result.countsSaturated);
            result.countsSaturated = result.countsSaturated || child.saturated;
            const auto terminal = child.queue.terminal();

            if (terminal) {
                ++result.terminalStripes;
                debit(members.size(), work.visits);

                for (const auto member : members) {
                    accumulate(total.*member, terminal->counts.*member, total.saturated);
                }

                total.saturated = total.saturated || terminal->counts.saturated;
                result.hadGap = result.hadGap || terminal->hadGap;
                result.unknownEligibility = result.unknownEligibility || terminal->unknownEligibility;
            }
        }

        if (result.terminalStripes == result.stripes) {
            result.finalCounts = total;
            result.countsSaturated = result.countsSaturated || total.saturated;
        }

        return result;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    }
}

std::expected<std::vector<StripedCollector::RangeObservation>, StripeFailure>
StripedCollector::rangeSnapshot(const StripeClient &client, StripedWork &work) const {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        validateClient(*state, client.port_);

        if (state->family != core::SubscriptionFamily::HISTORY) {
            throw StripeFailure{Error::UNSUPPORTED_FAMILY};
        }

        std::size_t count = 0;
        std::size_t symbolBytes = 0;

        // No owner progress or callback occurs between these passes. Concurrent reserved stop
        // can change status flags, but not the applied key cardinality or owned key capacities.
        for (const auto &entry : client.port_->children) {
            debit(1, work.visits);
            auto rows = state->children[entry.stripe].queue.rangeSnapshot(entry.handle, work.child);

            if (!rows) {
                throw childFailure(rows.error(), entry.stripe);
            }

            debit(rows->size(), work.visits);
            count = sum(count, rows->size());

            for (const auto &row : *rows) {
                symbolBytes = sum(symbolBytes, product(sum(row.symbol.capacity(), 1), sizeof(char16_t)));
            }

            if (sum(product(count, sizeof(RangeObservation)), symbolBytes) > state->limits.operationBytes) {
                throw StripeFailure{Error::RESOURCE_LIMIT, entry.stripe};
            }
        }

        debit(sum(product(count, sizeof(RangeObservation)), symbolBytes), work.bytes);
        std::vector<RangeObservation> result;
        result.reserve(count);

        for (const auto &entry : client.port_->children) {
            debit(1, work.visits);
            auto rows = state->children[entry.stripe].queue.rangeSnapshot(entry.handle, work.child);

            if (!rows) {
                throw childFailure(rows.error(), entry.stripe);
            }

            debit(rows->size(), work.visits);

            if (rows->size() > count - result.size()) {
                throw StripeFailure{Error::RESOURCE_LIMIT, entry.stripe};
            }

            for (auto &row : *rows) {
                result.push_back({entry.stripe, std::move(row)});
            }
        }

        return result;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    } catch (const std::length_error &) {
        return std::unexpected(StripeFailure{Error::RESOURCE_LIMIT});
    }
}

std::expected<collectors::HistoryRetainedRange, StripeFailure>
StripedCollector::retainedHistoryRange(const StripeClient &client, const collectors::HistoryRetainedQuery &query,
                                       StripedWork &work) const {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        validateClient(*state, client.port_);

        if (state->family != core::SubscriptionFamily::HISTORY) {
            throw StripeFailure{Error::UNSUPPORTED_FAMILY};
        }

        const auto stripe = stripeIndex(*state, query.key.symbol, work);
        debit(client.port_->children.size(), work.visits);
        const auto *handle = clientHandle(*client.port_, stripe);

        if (!handle) {
            throw StripeFailure{Error::INVALID_CLIENT, stripe};
        }

        auto result = state->children[stripe].queue.retainedHistoryRange(*handle, query, work.child);

        if (!result) {
            throw childFailure(result.error(), stripe);
        }

        return std::move(*result);
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    } catch (const std::bad_alloc &) {
        return std::unexpected(StripeFailure{Error::ALLOCATION_FAILURE});
    }
}

std::expected<TickerLatestObservation, StripeFailure>
StripedCollector::visitTickerLatest(const StripeClient &client, core::SubscriptionKey key, StripedWork &work,
                                    void *context,
                                    void (*visitor)(void *, const collectors::TickerValue &) noexcept) const {
    auto state = state_;

    if (!state) {
        return std::unexpected(StripeFailure{Error::INVALID_STATE});
    }

    try {
        OperationGuard guard(*state);
        debit(1, work.operations);
        validateClient(*state, client.port_);

        if (state->family != core::SubscriptionFamily::TICKER) {
            throw StripeFailure{Error::UNSUPPORTED_FAMILY};
        }

        const auto stripe = stripeIndex(*state, key.symbol, work);
        debit(client.port_->children.size(), work.visits);
        const auto *handle = clientHandle(*client.port_, stripe);

        if (!handle) {
            return TickerLatestObservation::UNMAPPED;
        }

        const auto result = state->children[stripe].queue.visitTickerLatest(*handle, key, work.child, context, visitor);

        if (!result) {
            throw childFailure(result.error(), stripe);
        }

        return *result;
    } catch (StripeFailure failure) {
        return std::unexpected(std::move(failure));
    }
}
} // namespace org::ttldtor::qdcxx::mux
