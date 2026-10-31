// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/collectors/stream.hpp"

#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::collectors {
namespace {
using Error = StreamError;
constexpr std::int32_t STREAM_DATA = 15;

/** Debit before work; failures do not refund earlier charges.
 * @param amount Required conservative units.
 * @param remaining Mutable remaining turn credits.
 */
void debit(std::size_t amount, std::size_t &remaining) {
    if (amount > remaining) {
        throw Error::WORK_LIMIT;
    }

    remaining -= amount;
}

/** Check requested backing arithmetic before allocation.
 * @param left First count.
 * @param right Additional count.
 * @return Exact sum or RESOURCE_LIMIT.
 */
std::size_t sum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw Error::RESOURCE_LIMIT;
    }

    return left + right;
}

/** Check array allocation arithmetic.
 * @param count Required slots.
 * @param bytes One slot's object size.
 * @return Exact requested backing or RESOURCE_LIMIT.
 */
std::size_t product(std::size_t count, std::size_t bytes) {
    if (count > std::numeric_limits<std::size_t>::max() / bytes) {
        throw Error::RESOURCE_LIMIT;
    }

    return count * bytes;
}

/** Advance a cumulative diagnostic without confusing overflow with an exact count.
 * @param counter Counter to update.
 * @param amount Known local count.
 * @param saturated Shared explicit diagnostic overflow indicator.
 */
void accumulate(std::size_t &counter, std::size_t amount, bool &saturated) noexcept {
    if (amount > std::numeric_limits<std::size_t>::max() - counter) {
        counter = std::numeric_limits<std::size_t>::max();
        saturated = true;
    } else {
        counter += amount;
    }
}
} // namespace

struct StreamCollector::Impl {
    /** One immutable frame and the selected items retaining it. */
    struct Frame {
        std::optional<protocol::LiveEvent> event; ///< Full owner, never a decoder prefix.
        std::size_t remaining = 0;                ///< Number of selected items still queued.
        std::size_t bytes = 0;                    ///< Unique event backing reserved in this collector scope.
    };

    /** Selection metadata; record contents remain in the owning immutable frame. */
    struct Item {
        const protocol::LiveRecordView *record = nullptr; ///< Stable event-arena address.
        core::SubscriptionMembership membership; ///< Admission activation, independent of symbol-view lifetime.
        std::uint64_t revision = 0;              ///< Whole-frame desired revision.
        std::size_t frame = 0;                   ///< Fixed frame ring index.
    };

    protocol::LiveIdentity epoch;    ///< Retained through drain; destruction follows all frame releases.
    core::SubscriptionState desired; ///< Unique mutable eligibility owner.
    StreamLimits limits;             ///< Immutable configured capacities.
    std::unique_ptr<Frame[]> frames; ///< Fixed initialized frame slots.
    std::unique_ptr<Item[]> items;   ///< Fixed initialized selected-record slots.
    std::size_t fixedBytes;          ///< Impl plus requested fixed array backing.
    std::size_t stagedBytes = 0;     ///< Unique whole-event backing currently retained.
    std::size_t frameHead = 0;       ///< First retained frame slot.
    std::size_t frameCount = 0;      ///< Retained whole frame owners.
    std::size_t itemHead = 0;        ///< First queued record slot.
    std::size_t itemCount = 0;       ///< Queued selected records, including pending withdrawals.
    bool pinned = false;             ///< First selected admission established the epoch.
    bool offered = false;            ///< front offered the current item after the last effective mutation.
    StreamCounts counts;             ///< Explicit cumulative observations.
    StreamDiagnostic diagnostic;     ///< Inline control evidence unaffected by ordinary queue saturation.

    /** Transfer only after every fallible factory step.
     * @param state Validated desired owner.
     * @param bounds Immutable capacities.
     * @param frameSlots Fully initialized frame ring.
     * @param itemSlots Fully initialized item ring.
     * @param backing Exact fixed requested bytes.
     */
    Impl(core::SubscriptionState &&state, StreamLimits bounds, std::unique_ptr<Frame[]> frameSlots,
         std::unique_ptr<Item[]> itemSlots, std::size_t backing) noexcept
        : desired(std::move(state)), limits(bounds), frames(std::move(frameSlots)), items(std::move(itemSlots)),
          fixedBytes(backing) {
    }

    /** Map an offset without overflowing head plus offset.
     * @param offset Logical offset below records capacity.
     * @return Fixed record ring index; only called for a nonzero capacity.
     */
    [[nodiscard]] std::size_t index(std::size_t offset) const noexcept {
        const auto suffix = limits.records - itemHead;

        return offset < suffix ? itemHead + offset : offset - suffix;
    }

    /** Locate an exact current subscription; no work or allocation occurs here.
     * @param record Whole-frame local record.
     * @return Borrowed matching desired entry, or null for unsolicited data.
     */
    [[nodiscard]] const core::SubscriptionEntry *match(const protocol::LiveRecordView &record) const noexcept {
        for (const auto &entry : desired.entries()) {
            if (entry.key.context.matches(record.context) && entry.key.symbol == record.symbol) {
                return &entry;
            }
        }

        return nullptr;
    }

    /** Remove one front item after all possible failures and work charges.
     * Counts are updated by the caller according to delivery/withdrawal/discard intent.
     */
    void removeFront() noexcept {
        auto &item = items[itemHead];
        auto &frame = frames[item.frame];
        item = Item{};
        --itemCount;
        itemHead = itemHead + 1 == limits.records ? 0 : itemHead + 1;

        if (--frame.remaining == 0) {
            stagedBytes -= frame.bytes;
            frame.event.reset();
            frame.bytes = 0;
            --frameCount;
            frameHead = frameHead + 1 == limits.frames ? 0 : frameHead + 1;
        }
    }
};

StreamCollector::StreamCollector(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

StreamCollector::StreamCollector(StreamCollector &&other) noexcept = default;
StreamCollector &StreamCollector::operator=(StreamCollector &&other) noexcept = default;
StreamCollector::~StreamCollector() = default;

std::expected<StreamCollector, StreamError> StreamCollector::create(core::SubscriptionState &&desired,
                                                                    StreamLimits limits, StreamWork &work) {
    try {
        debit(1, work.operations);

        if (desired.family() != core::SubscriptionFamily::STREAM) {
            throw Error::INVALID_FAMILY;
        }

        debit(desired.entries().size(), work.visits);
        core::SubscriptionWork validation{1, desired.entries().size(), 0};

        if (!desired.add({}, validation)) {
            throw Error::INVALID_FAMILY;
        }

        const auto bytes = sum(sizeof(Impl), sum(product(limits.frames, sizeof(Impl::Frame)),
                                                 product(limits.records, sizeof(Impl::Item))));

        if (bytes > limits.residentBytes) {
            throw Error::RESOURCE_LIMIT;
        }

        debit(limits.frames, work.visits);
        debit(limits.records, work.visits);
        debit(bytes, work.bytes);
        std::unique_ptr<Impl::Frame[]> frames;
        std::unique_ptr<Impl::Item[]> items;

        if (limits.frames != 0) {
            frames = std::make_unique<Impl::Frame[]>(limits.frames);
        }

        if (limits.records != 0) {
            items = std::make_unique<Impl::Item[]>(limits.records);
        }

        auto impl =
            std::unique_ptr<Impl>(new Impl(std::move(desired), limits, std::move(frames), std::move(items), bytes));

        return StreamCollector(std::move(impl));
    } catch (Error error) {
        return std::unexpected(error);
    } catch (const std::bad_alloc &) {
        return std::unexpected(Error::ALLOCATION_FAILURE);
    }
}

std::expected<core::SubscriptionChange, core::SubscriptionError>
StreamCollector::add(std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &work) {
    if (!impl_ || impl_->diagnostic.error) {
        return std::unexpected(core::SubscriptionError::INVALID_STATE);
    }

    const auto result = impl_->desired.add(requests, work);

    if (result && result->changed) {
        impl_->offered = false;
    }

    return result;
}

std::expected<core::SubscriptionChange, core::SubscriptionError>
StreamCollector::remove(std::span<const core::SubscriptionKey> keys, core::SubscriptionWork &work) {
    if (!impl_) {
        return std::unexpected(core::SubscriptionError::INVALID_STATE);
    }

    const auto result = impl_->desired.remove(keys, work);

    if (result && result->changed) {
        impl_->offered = false;
    }

    return result;
}

std::expected<core::SubscriptionChange, core::SubscriptionError>
StreamCollector::replace(std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &work) {
    if (!impl_ || impl_->diagnostic.error) {
        return std::unexpected(core::SubscriptionError::INVALID_STATE);
    }

    const auto result = impl_->desired.replace(requests, work);

    if (result && result->changed) {
        impl_->offered = false;
    }

    return result;
}

std::expected<core::SubscriptionChange, core::SubscriptionError> StreamCollector::clear(core::SubscriptionWork &work) {
    if (!impl_) {
        return std::unexpected(core::SubscriptionError::INVALID_STATE);
    }

    const auto result = impl_->desired.clear(work);

    if (result && result->changed) {
        impl_->offered = false;
    }

    return result;
}

std::expected<StreamStage, StreamError> StreamCollector::stage(protocol::LiveEvent &&event, StreamWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_ || impl_->diagnostic.error) {
            throw Error::INVALID_STATE;
        }

        if (event.kind() != protocol::LiveEventKind::DATA || event.messageType() != STREAM_DATA) {
            throw Error::INVALID_EVENT;
        }

        const auto identity = event.snapshot().identity();

        if (impl_->pinned && !impl_->epoch.sameEpoch(identity)) {
            throw Error::INVALID_EPOCH;
        }

        std::size_t selected = 0;

        // Reserve both matching passes before writing any free ring slot or transferring ownership.
        for (const auto &record : event.records()) {
            debit(1, work.visits);
            bool requested = false;

            for (const auto &entry : impl_->desired.entries()) {
                debit(2, work.visits);
                debit(record.symbol.size(), work.textUnits);
                debit(entry.key.symbol.size(), work.textUnits);
                debit(record.symbol.size(), work.textUnits);
                debit(entry.key.symbol.size(), work.textUnits);

                if (entry.key.context.matches(record.context) && entry.key.symbol == record.symbol) {
                    requested = true;
                }
            }

            if (requested) {
                ++selected;
            }
        }

        const auto ignored = event.records().size() - selected;
        const auto revision = impl_->desired.revision();

        if (selected == 0) {
            accumulate(impl_->counts.ignored, ignored, impl_->counts.saturated);

            return StreamStage{0, ignored, revision};
        }

        const auto bytes = event.residentBytes();

        if (impl_->frameCount == impl_->limits.frames || selected > impl_->limits.records - impl_->itemCount ||
            bytes > impl_->limits.stagedBytes - impl_->stagedBytes ||
            bytes > impl_->limits.residentBytes - impl_->fixedBytes - impl_->stagedBytes) {
            impl_->diagnostic = {Error::RESOURCE_LIMIT, impl_->itemCount, selected};
            impl_->offered = false;

            throw Error::RESOURCE_LIMIT;
        }

        const auto frameSuffix = impl_->limits.frames - impl_->frameHead;
        const auto frameIndex =
            impl_->frameCount < frameSuffix ? impl_->frameHead + impl_->frameCount : impl_->frameCount - frameSuffix;
        std::size_t offset = impl_->itemCount;

        for (const auto &record : event.records()) {
            if (const auto *entry = impl_->match(record)) {
                auto &item = impl_->items[impl_->index(offset++)];
                item.record = &record;
                item.membership = entry->membership;
                item.revision = revision;
                item.frame = frameIndex;
            }
        }

        auto &frame = impl_->frames[frameIndex];
        frame.event.emplace(std::move(event));
        frame.remaining = selected;
        frame.bytes = bytes;
        impl_->itemCount += selected;
        ++impl_->frameCount;
        impl_->stagedBytes += bytes;
        impl_->offered = false;

        if (!impl_->pinned) {
            impl_->epoch = identity;
            impl_->pinned = true;
        }

        accumulate(impl_->counts.staged, selected, impl_->counts.saturated);
        accumulate(impl_->counts.ignored, ignored, impl_->counts.saturated);

        return StreamStage{selected, ignored, revision};
    } catch (Error error) {
        return std::unexpected(error);
    }
}

std::expected<std::optional<StreamDelivery>, StreamError> StreamCollector::front(StreamWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_ || impl_->diagnostic.error) {
            throw Error::INVALID_STATE;
        }

        std::size_t withdrawn = 0;

        for (; withdrawn < impl_->itemCount; ++withdrawn) {
            const auto &item = impl_->items[impl_->index(withdrawn)];
            debit(1, work.visits);
            debit(impl_->desired.entries().size(), work.visits);

            if (impl_->desired.isCurrent(item.membership)) {
                break;
            }

            const bool last =
                withdrawn + 1 == impl_->itemCount || impl_->items[impl_->index(withdrawn + 1)].frame != item.frame;

            if (last) {
                debit(1, work.visits);
                debit(impl_->frames[item.frame].event->releaseBytes(), work.bytes);
            }
        }

        for (std::size_t i = 0; i < withdrawn; ++i) {
            impl_->removeFront();
        }

        accumulate(impl_->counts.withdrawn, withdrawn, impl_->counts.saturated);
        impl_->offered = impl_->itemCount != 0;

        if (!impl_->offered) {
            return std::optional<StreamDelivery>{};
        }

        const auto &item = impl_->items[impl_->itemHead];

        return std::optional<StreamDelivery>{{item.record, item.revision}};
    } catch (Error error) {
        return std::unexpected(error);
    }
}

std::expected<void, StreamError> StreamCollector::pop(StreamWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_ || impl_->diagnostic.error) {
            throw Error::INVALID_STATE;
        }

        if (!impl_->offered) {
            throw Error::NO_OFFER;
        }

        debit(1, work.visits);
        const auto &frame = impl_->frames[impl_->items[impl_->itemHead].frame];

        if (frame.remaining == 1) {
            debit(1, work.visits);
            debit(frame.event->releaseBytes(), work.bytes);
        }

        impl_->removeFront();
        impl_->offered = false;
        accumulate(impl_->counts.delivered, 1, impl_->counts.saturated);

        return {};
    } catch (Error error) {
        return std::unexpected(error);
    }
}

std::expected<std::size_t, StreamError> StreamCollector::discard(StreamWork &work) {
    try {
        debit(1, work.operations);

        if (!impl_) {
            throw Error::INVALID_STATE;
        }

        debit(impl_->itemCount, work.visits);
        debit(impl_->frameCount, work.visits);

        for (std::size_t i = 0, slot = impl_->frameHead; i < impl_->frameCount; ++i) {
            debit(impl_->frames[slot].event->releaseBytes(), work.bytes);
            slot = slot + 1 == impl_->limits.frames ? 0 : slot + 1;
        }

        const auto count = impl_->itemCount;

        while (impl_->itemCount != 0) {
            impl_->removeFront();
        }

        impl_->offered = false;
        accumulate(impl_->counts.discarded, count, impl_->counts.saturated);

        return count;
    } catch (Error error) {
        return std::unexpected(error);
    }
}

const core::SubscriptionState *StreamCollector::desired() const noexcept {
    return impl_ ? &impl_->desired : nullptr;
}

std::size_t StreamCollector::queuedRecords() const noexcept {
    return impl_ ? impl_->itemCount : 0;
}

std::size_t StreamCollector::residentBytes() const noexcept {
    return impl_ ? impl_->fixedBytes + impl_->stagedBytes : 0;
}

std::size_t StreamCollector::stagedBytes() const noexcept {
    return impl_ ? impl_->stagedBytes : 0;
}

StreamCounts StreamCollector::counts() const noexcept {
    return impl_ ? impl_->counts : StreamCounts{};
}

StreamDiagnostic StreamCollector::diagnostic() const noexcept {
    return impl_ ? impl_->diagnostic : StreamDiagnostic{};
}

} // namespace org::ttldtor::qdcxx::collectors
