// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::api {
/** Finite independent subscription capacities. Each attached feed applies its own runtime limits as well. */
struct IndependentSubscriptionLimits {
    std::size_t maximumAttachments = 4;   ///< Active and callback-retiring attachment generations together.
    std::size_t maximumSelectors = 16;    ///< Raw selector arguments and retained identities per replacement.
    std::size_t maximumSymbolUnits = 128; ///< UTF-16 units retained per selector.
};

/** Own one typed listener and desired set independently of any receiving endpoint.
 * One native registration is used per attached Feed identity. Different feeds deliver independently and
 * may call the listener concurrently. Event spans are borrowed for the duration of each call.
 * @tparam Event Owning native event type supported by Feed::createSubscription.
 */
template <class Event> class IndependentSubscription final {
    public:
    using Listener = typename Subscription<Event>::Listener; ///< Listener shared by all attachments.
    using AttachmentId = std::uint64_t;                      ///< Identity local to this subscription, never reused.
    using AttachmentListener = std::function<void(AttachmentId, std::span<const Event>)>;
    ///< Event batches tagged with the exact delivering attachment.
    using ClearListener = std::move_only_function<void(AttachmentId) const noexcept>;

    ///< Invalidate one attachment's view without publishing market data.

    /** Synchronous refusal applying an accepted desired set to one attachment. */
    struct AttachmentFailure {
        AttachmentId id;       ///< Attachment returned by attach().
        Diagnostic diagnostic; ///< Refusal from this attachment's producer.
    };

    private:
    struct Handlers {
        Listener untagged;
        AttachmentListener tagged;
        ClearListener clear;

        void deliver(AttachmentId id, std::span<const Event> events) const {
            if (tagged) {
                tagged(id, events);
            } else {
                untagged(events);
            }
        }
    };

    /** Bridge native listener selection without retaining the logical subscription or its endpoint. */
    struct DeliveryGate {
        DeliveryGate(AttachmentId id, std::shared_ptr<const Handlers> handlers)
            : id(id), handlers(std::move(handlers)) {
        }

        std::mutex mutex;
        bool active = false;
        bool clearPending = false;
        std::size_t selected = 0;
        const AttachmentId id;
        const std::shared_ptr<const Handlers> handlers;

        void deliver(std::span<const Event> events) {
            {
                std::lock_guard lock(mutex);

                if (!active) {
                    return;
                }

                ++selected;
            }

            struct Completion {
                DeliveryGate &gate;

                ~Completion() {
                    gate.finishDelivery();
                }
            } completion{*this};

            handlers->deliver(id, events);
        }

        void disable() noexcept {
            std::lock_guard lock(mutex);
            active = false;
        }

        void requestClear() noexcept {
            {
                std::lock_guard lock(mutex);

                if (selected != 0) {
                    clearPending = true;

                    return;
                }
            }

            handlers->clear(id);
        }

        void finishDelivery() noexcept {
            {
                std::lock_guard lock(mutex);
                --selected;

                if (selected != 0 || !clearPending) {
                    return;
                }

                clearPending = false;
            }

            handlers->clear(id);
        }
    };

    struct Attachment {
        AttachmentId id;
        std::weak_ptr<const void> feed;
        std::weak_ptr<DeliveryGate> gate;
        std::shared_ptr<Subscription<Event>> native;
        std::shared_ptr<DeliveryGate> activeGate;

        [[nodiscard]] bool matches(const std::shared_ptr<detail::EndpointPort> &port) const noexcept {
            return native && !feed.owner_before(port) && !port.owner_before(feed);
        }
    };

    const IndependentSubscriptionLimits limits_;
    mutable std::mutex mutex_;
    std::shared_ptr<const Handlers> handlers_;
    std::vector<SubscriptionSelector> selectors_;
    std::vector<Attachment> attachments_;
    AttachmentId nextId_ = 1;

    void requireOpen() const {
        if (!handlers_) {
            throw Error({ErrorCode::CLOSED, "independent-subscription-closed"});
        }
    }

    explicit IndependentSubscription(Handlers handlers, IndependentSubscriptionLimits limits) : limits_(limits) {
        if ((!handlers.untagged && (!handlers.tagged || !handlers.clear)) || !limits_.maximumAttachments ||
            !limits_.maximumSelectors || !limits_.maximumSymbolUnits) {
            throw Error({ErrorCode::INVALID_ARGUMENT, "independent-subscription-options"});
        }

        if (limits_.maximumAttachments > attachments_.max_size() || limits_.maximumSelectors > selectors_.max_size()) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-options"});
        }

        attachments_.reserve(limits_.maximumAttachments);
        handlers_ = std::make_shared<const Handlers>(std::move(handlers));
    }

    /** Remove one gate before native retirement or invoking any application code. */
    std::shared_ptr<DeliveryGate> detachGate(const Feed &feed, bool clear) {
        std::shared_ptr<Subscription<Event>> native;
        std::shared_ptr<DeliveryGate> gate;
        {
            std::lock_guard lock(mutex_);
            const auto attachment = std::ranges::find_if(attachments_, [&](const Attachment &entry) {
                return entry.matches(feed.port_);
            });

            if (attachment == attachments_.end()) {
                return {};
            }

            if (clear && !handlers_->clear) {
                throw Error({ErrorCode::UNSUPPORTED_PROFILE, "independent-subscription-clear-listener"});
            }

            gate = std::move(attachment->activeGate);
            gate->disable();
            native = std::move(attachment->native);
        }
        native->close();

        return gate;
    }

    public:
    /** Construct an unattached subscription with an empty desired set.
     * @param listener Nonempty owning callback. It may be called concurrently by distinct feeds.
     * @param limits Positive capacities for retained selectors and active or retiring attachments.
     * @throws Error Invalid arguments or logical capacity refused.
     * @throws std::bad_alloc Owned storage could not be allocated.
     */
    explicit IndependentSubscription(Listener listener, IndependentSubscriptionLimits limits = {})
        : IndependentSubscription(Handlers{std::move(listener), {}, {}}, limits) {
    }

    /** Construct an unattached subscription with attachment-scoped data and invalidation callbacks.
     * @param listener Nonempty owning callback receiving the exact attachment ID with each data batch.
     * @param clear Nonempty owning nonthrowing callback to invalidate the view keyed by its attachment ID.
     * Different attachments can invoke these callbacks concurrently.
     * @param limits Positive capacities for retained selectors and active or retiring attachments.
     * @return New subscription with an empty desired set.
     * @throws Error Invalid arguments or logical capacity refused.
     * @throws std::bad_alloc Owned storage could not be allocated.
     */
    [[nodiscard]] static IndependentSubscription withAttachmentListener(AttachmentListener listener,
                                                                        ClearListener clear,
                                                                        IndependentSubscriptionLimits limits = {}) {
        return IndependentSubscription(Handlers{{}, std::move(listener), std::move(clear)}, limits);
    }

    /** Fence new event selection and request retirement of all attachments. */
    ~IndependentSubscription() {
        close();
    }

    /** Create one native registration with the current desired set.
     * Repeated attachment to the same Feed returns the same active identity. A later attachment after detach
     * obtains a new identity. Native delivery selected during initialization waits for successful publication.
     * @param feed Feed producer borrowed during this call.
     * @return Stable identity of the current attachment.
     * @throws Error Closed, capacity refused, or the feed rejected admission or the current selectors.
     * @throws std::bad_alloc Owned storage could not be allocated.
     */
    [[nodiscard]] AttachmentId attach(Feed &feed) {
        std::shared_ptr<Subscription<Event>> native;
        std::shared_ptr<DeliveryGate> gate;
        std::lock_guard lock(mutex_);
        requireOpen();
        const auto matches = [&](const Attachment &attachment) {
            return attachment.matches(feed.port_);
        };
        const auto existing = std::ranges::find_if(attachments_, matches);

        if (existing != attachments_.end()) {
            return existing->id;
        }

        std::erase_if(attachments_, [](const Attachment &attachment) {
            return !attachment.native && attachment.gate.expired();
        });

        if (attachments_.size() == limits_.maximumAttachments ||
            nextId_ == (std::numeric_limits<AttachmentId>::max)()) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-attachments"});
        }

        const auto id = nextId_;
        gate = std::make_shared<DeliveryGate>(id, handlers_);
        std::lock_guard deliveryLock(gate->mutex);
        ++nextId_;
        auto &attachment = attachments_.emplace_back(Attachment{id, feed.port_, gate, {}, {}});
        native = feed.template createSubscription<Event>();
        // A native registration may await retirement before the runtime has started its worker.
        // Only an actual invocation extends the gate's lifetime after detachment.
        native->addEventListener([gate = attachment.gate](std::span<const Event> events) {
            if (const auto selected = gate.lock()) {
                selected->deliver(events);
            }
        });
        native->setAttachedSelectors(selectors_);
        attachment.native = std::move(native);
        attachment.activeGate = gate;
        gate->active = true;

        return id;
    }

    /** Fence selection on one feed and request retirement of its native registration.
     * A listener invocation already selected may finish. Other attachments and desired selectors remain active.
     * @param feed Feed identity borrowed during this call.
     * @return Whether an active attachment was removed.
     */
    bool detach(const Feed &feed) noexcept {
        return detachGate(feed, false) != nullptr;
    }

    /** Detach one feed and invalidate the view associated with its retired attachment ID.
     * The clear listener runs once after all data callbacks already selected for that attachment return.
     * With no selected callback it runs inline. Otherwise the last selected callback invokes it on exit,
     * including after reentrant detachment or a data callback exception. This operation does not wait.
     * A concurrent close or reattachment does not cancel a clear already accepted.
     * @param feed Feed identity borrowed during this call.
     * @return Whether an active attachment was removed and its clear accepted.
     * @throws Error An active value-only subscription has no attachment-scoped clear listener.
     */
    bool detachAndClear(const Feed &feed) {
        if (const auto gate = detachGate(feed, true)) {
            gate->requestClear();

            return true;
        }

        return false;
    }

    /** Accept a bounded local desired replacement and submit it separately to each attached feed.
     * Equal selector identities coalesce, with the final occurrence retaining its effective attributes.
     * A feed can refuse synchronously and keep its previous desired set. Repeating the call retries admission.
     * Successful submission is local to each producer. Remote application and asynchronous failures are separate.
     * @param selectors Borrowed arguments copied before local acceptance.
     * @return Synchronous refusals identified by attachment. An empty result means every current feed accepted.
     * @throws Error Closed or logical capacity refused before local acceptance.
     * @throws std::bad_alloc Owned storage could not be allocated before local acceptance.
     */
    [[nodiscard]] std::vector<AttachmentFailure> setSelectors(std::span<const SubscriptionSelector> selectors) {
        std::lock_guard lock(mutex_);
        requireOpen();

        if (selectors.size() > limits_.maximumSelectors) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-selector-arguments"});
        }

        std::vector<SubscriptionSelector> desired;
        desired.reserve(selectors.size());

        for (const auto &selector : selectors) {
            if (selector.eventSymbol().size() > limits_.maximumSymbolUnits) {
                throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-symbol-units"});
            }

            const auto existing = std::ranges::find(desired, selector);

            if (existing == desired.end()) {
                desired.push_back(selector);
            } else {
                *existing = selector;
            }
        }

        std::vector<AttachmentFailure> failures;
        failures.reserve(attachments_.size());
        selectors_.swap(desired);

        for (const auto &attachment : attachments_) {
            if (!attachment.native) {
                continue;
            }

            try {
                attachment.native->setAttachedSelectors(selectors_);
            } catch (const Error &error) {
                failures.push_back({attachment.id, error.diagnostic()});
            } catch (const std::bad_alloc &) {
                failures.push_back({attachment.id, {ErrorCode::ALLOCATION_FAILURE, "api-allocation"}});
            }
        }

        return failures;
    }

    /** @return Owned copy of the current local desired selectors, empty after close. */
    [[nodiscard]] std::vector<SubscriptionSelector> getSelectors() const {
        std::lock_guard lock(mutex_);

        return selectors_;
    }

    /** Inspect the current native diagnostic for one attachment.
     * @param feed Feed identity borrowed during this call.
     * @return Diagnostic snapshot for an attached feed, or nullopt if detached.
     */
    [[nodiscard]] std::optional<Diagnostic> getAttachmentDiagnostic(const Feed &feed) const noexcept {
        std::lock_guard lock(mutex_);
        const auto attachment = std::ranges::find_if(attachments_, [&](const Attachment &entry) {
            return entry.matches(feed.port_);
        });

        if (attachment == attachments_.end()) {
            return std::nullopt;
        }

        return attachment->native->getDiagnostic();
    }

    /** Permanently close, fence new selection, and request native retirement without waiting for callbacks. */
    void close() noexcept {
        std::vector<Attachment> attachments;
        std::shared_ptr<const Handlers> handlers;
        {
            std::lock_guard lock(mutex_);

            for (const auto &attachment : attachments_) {
                if (attachment.activeGate) {
                    attachment.activeGate->disable();
                }
            }

            attachments.swap(attachments_);
            handlers.swap(handlers_);
            selectors_.clear();
        }

        for (const auto &attachment : attachments) {
            if (attachment.native) {
                attachment.native->close();
            }
        }
    }
};
} // namespace org::ttldtor::qdcxx::api
