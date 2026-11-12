// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/api/feed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
    std::size_t maximumAttachments = 4;   ///< Active attachments and still-selected deliveries together.
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

    /** Synchronous refusal applying an accepted desired set to one attachment. */
    struct AttachmentFailure {
        AttachmentId id;       ///< Attachment returned by attach().
        Diagnostic diagnostic; ///< Refusal from this attachment's producer.
    };

    private:
    /** Bridge native listener selection without retaining the logical subscription or its endpoint. */
    struct DeliveryGate {
        explicit DeliveryGate(std::shared_ptr<const Listener> listener) : listener(std::move(listener)) {
        }

        std::mutex mutex;
        bool active = false;
        const std::shared_ptr<const Listener> listener;

        void deliver(std::span<const Event> events) {
            {
                std::lock_guard lock(mutex);

                if (!active) {
                    return;
                }
            }

            (*listener)(events);
        }

        void disable() noexcept {
            std::lock_guard lock(mutex);
            active = false;
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
    std::shared_ptr<const Listener> listener_;
    std::vector<SubscriptionSelector> selectors_;
    std::vector<Attachment> attachments_;
    AttachmentId nextId_ = 1;

    void requireOpen() const {
        if (!listener_) {
            throw Error({ErrorCode::CLOSED, "independent-subscription-closed"});
        }
    }

    public:
    /** Construct an unattached subscription with an empty desired set.
     * @param listener Nonempty owning callback. It may be called concurrently by distinct feeds.
     * @param limits Positive capacities for retained selectors and active or retiring attachments.
     * @throws Error Invalid arguments or logical capacity refused.
     * @throws std::bad_alloc Owned storage could not be allocated.
     */
    explicit IndependentSubscription(Listener listener, IndependentSubscriptionLimits limits = {}) : limits_(limits) {
        if (!listener || !limits_.maximumAttachments || !limits_.maximumSelectors || !limits_.maximumSymbolUnits) {
            throw Error({ErrorCode::INVALID_ARGUMENT, "independent-subscription-options"});
        }

        if (limits_.maximumAttachments > attachments_.max_size() || limits_.maximumSelectors > selectors_.max_size()) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-options"});
        }

        attachments_.reserve(limits_.maximumAttachments);
        listener_ = std::make_shared<const Listener>(std::move(listener));
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

        if (attachments_.size() == limits_.maximumAttachments || nextId_ == std::numeric_limits<AttachmentId>::max()) {
            throw Error({ErrorCode::RESOURCE_LIMIT, "independent-subscription-attachments"});
        }

        gate = std::make_shared<DeliveryGate>(listener_);
        std::lock_guard deliveryLock(gate->mutex);
        const auto id = nextId_++;
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
        std::shared_ptr<Subscription<Event>> native;
        std::shared_ptr<DeliveryGate> gate;
        {
            std::lock_guard lock(mutex_);
            const auto attachment = std::ranges::find_if(attachments_, [&](const Attachment &entry) {
                return entry.matches(feed.port_);
            });

            if (attachment == attachments_.end()) {
                return false;
            }

            gate = std::move(attachment->activeGate);
            gate->disable();
            native = std::move(attachment->native);
        }
        native->close();

        return true;
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
        std::shared_ptr<const Listener> listener;
        {
            std::lock_guard lock(mutex_);

            for (const auto &attachment : attachments_) {
                if (attachment.activeGate) {
                    attachment.activeGate->disable();
                }
            }

            attachments.swap(attachments_);
            listener.swap(listener_);
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
