// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/client/subscription_receiver.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::client {
namespace {
/** Add finite logical counts without wrapping.
 * @param left Existing count.
 * @param right Additional count.
 * @return Exact sum or local resource refusal.
 */
std::size_t checkedSum(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left)
        throw ReceiverError::RESOURCE_LIMIT;

    return left + right;
}

/** Multiply finite logical counts without wrapping.
 * @param count Element count.
 * @param size Element size.
 * @return Exact product or local resource refusal.
 */
std::size_t checkedProduct(std::size_t count, std::size_t size) {
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size)
        throw ReceiverError::RESOURCE_LIMIT;

    return count * size;
}

/** Debit explicit attempted work without replenishment or underflow.
 * @param remaining Caller-owned remaining credits.
 * @param count Conservative attempted work.
 */
void debit(std::size_t &remaining, std::size_t count) {
    if (count > remaining)
        throw ReceiverError::WORK_LIMIT;

    remaining -= count;
}

/** Conservatively prepay one existing mapper's independently bounded preparation.
 * @param limits Existing mapper's count, name-unit and comparison allowances.
 * @return Checked sum; it is work, not an allocator/RSS measurement.
 */
std::size_t preparationAllowance(events::MappingLimits limits) {
    std::size_t total = 0;

    for (const auto amount :
         std::array{limits.fields, limits.lookups, limits.selections, limits.keys, limits.typeAlternatives,
                    limits.nameUnits, limits.totalNameUnits, limits.comparisonWork})
        total = checkedSum(total, amount);

    return total;
}

/** Check supported explicit route settings without accepting unknown enum values.
 * @param binding Caller route under inspection.
 * @return Whether the mapper and timestamp policy are known.
 */
bool validBinding(const PreparedSubscriptionBinding &binding) noexcept {
    return (binding.kind == PreparedEventKind::QUOTE || binding.kind == PreparedEventKind::PROFILE ||
            binding.kind == PreparedEventKind::TRADE) &&
           (binding.eventTimePolicy == ReceiverEventTimePolicy::UNSPECIFIED ||
            binding.eventTimePolicy == ReceiverEventTimePolicy::FIELD_METADATA);
}
} // namespace

namespace detail {
/** One independently owned immutable mapper and its explicit projection bounds. */
struct ReceiverRoute {
    core::RecordContext context; ///< Retained exact origin/record identity, without declaration names.
    std::variant<events::PreparedQuote, events::PreparedProfile, events::PreparedTrade>
        mapping;                          ///< Existing owned immutable mapper.
    events::EventValueLimits valueLimits; ///< Per-result owned text bounds.
    ReceiverEventTimePolicy timePolicy;   ///< Explicit optional timestamp source.
    std::size_t shapeFields;              ///< Complete shape inspection allowance, including unselected extra fields.
    std::size_t selectionVisits; ///< Declared mapper selection allowance, including absent optional properties.
};

/** Finite immutable adapter backing; no desired map, handle or endpoint is retained. */
struct ReceiverPlan {
    std::vector<ReceiverRoute> routes; ///< Exact unique routes, bounded before allocation/preparation.
};

/** Delegate the existing whole-batch callback boundary without internally dispatching individual records. */
class SubscriptionReceiver final : public mux::MuxConsumer {
    ReceiverPlan plan_;                              ///< Immutable independently owned prepared routes.
    std::shared_ptr<SubscriptionConsumer> consumer_; ///< Executing application callable's strong lifetime guard.

    public:
    /** Adopt a complete candidate, without starting work.
     * @param plan Prepared bounded immutable routes.
     * @param consumer Application callable retained by the adapter.
     */
    SubscriptionReceiver(ReceiverPlan plan, std::shared_ptr<SubscriptionConsumer> consumer)
        : plan_(std::move(plan)), consumer_(std::move(consumer)) {
    }

    /** Preserve exactly one application invocation and the queue's existing exception boundary.
     * @param batch Underlying callback-only immutable backing.
     * @return Application's whole-batch action, without another user call after it returns.
     */
    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &batch) override {
        const SubscriptionBatch view(batch, plan_);

        return consumer_->onBatch(view);
    }
};
} // namespace detail

SubscriptionBatch::SubscriptionBatch(const mux::BorrowedMuxBatch &batch, const detail::ReceiverPlan &plan) noexcept
    : batch_(&batch), plan_(&plan) {
}

std::span<const mux::MuxRecordView> SubscriptionBatch::records() const noexcept {
    return batch_->records();
}

mux::OwnedMuxBatch SubscriptionBatch::retain() const noexcept {
    return batch_->retain();
}

std::expected<SubscriptionProjection, ReceiverProjectionFailure>
SubscriptionBatch::project(std::size_t index, ReceiverProjectionWork &work) const {
    try {
        debit(work.visits, 1);
        const auto input = records();

        if (index >= input.size())
            return std::unexpected(ReceiverProjectionFailure{ReceiverError::INVALID_INPUT});

        const auto &record = input[index];

        for (const auto &route : plan_->routes) {
            debit(work.visits, 1);

            if (!route.context.matches(record.context))
                continue;

            const bool quote = !std::holds_alternative<events::PreparedProfile>(route.mapping);
            const auto units = quote ? std::min(route.valueLimits.symbolUnits, route.valueLimits.totalUnits)
                                     : route.valueLimits.totalUnits;
            // Each owned string may need a terminator; this is logical copy work, not allocator overhead.
            const auto textValues = quote ? std::size_t{1} : std::size_t{3};
            const auto bytes = checkedSum(sizeof(SubscriptionProjection),
                                          checkedProduct(checkedSum(units, textValues), sizeof(char16_t)));
            debit(work.visits, checkedSum(route.shapeFields, checkedSum(route.selectionVisits, 1)));

            if (units > work.textUnits || bytes > work.bytes)
                return std::unexpected(ReceiverProjectionFailure{ReceiverError::WORK_LIMIT});

            work.textUnits -= units;
            work.bytes -= bytes;
            std::optional<std::int64_t> eventTime;

            if (route.timePolicy == ReceiverEventTimePolicy::FIELD_METADATA && record.source &&
                record.source->fieldTime)
                eventTime = record.source->fieldTime->eventTimeMs();

            const events::BorrowedRecordValueView values{record.context, record.typedValues};

            if (std::holds_alternative<events::PreparedTrade>(route.mapping)) {
                auto mapped = std::get<events::PreparedTrade>(route.mapping)
                                  .mapView(record.symbol, values, units, eventTime.value_or(0));

                if (!mapped)
                    return std::unexpected(ReceiverProjectionFailure{mapped.error()});

                return SubscriptionProjection{std::move(*mapped), eventTime};
            }

            if (quote) {
                auto mapped = std::get<events::PreparedQuote>(route.mapping)
                                  .mapView(record.symbol, values, units, eventTime.value_or(0));

                if (!mapped)
                    return std::unexpected(ReceiverProjectionFailure{mapped.error()});

                return SubscriptionProjection{std::move(*mapped), eventTime};
            }

            auto mapped = std::get<events::PreparedProfile>(route.mapping)
                              .mapView(record.symbol, values, route.valueLimits, eventTime.value_or(0));

            if (!mapped)
                return std::unexpected(ReceiverProjectionFailure{mapped.error()});

            return SubscriptionProjection{std::move(*mapped), eventTime};
        }

        return SubscriptionProjection{};
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverProjectionFailure{error});
    }
}

std::expected<mux::MuxHandle, ReceiverFailure>
openSubscriptionReceiver(mux::MuxQueue &queue, const core::LocalSchema &schema, core::SubscriptionFamily family,
                         core::SubscriptionSelectorPolicy policy, std::span<const PreparedSubscriptionBinding> bindings,
                         const std::shared_ptr<SubscriptionConsumer> &consumer, ReceiverLimits receiverLimits,
                         mux::ClientLimits clientLimits, ReceiverWork &receiverWork, mux::QueueWork &queueWork) {
    std::optional<std::size_t> bindingIndex;

    try {
        debit(receiverWork.visits, 1);

        if (!consumer)
            return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, {}});

        if (bindings.size() > receiverLimits.bindings)
            return std::unexpected(ReceiverFailure{ReceiverError::RESOURCE_LIMIT, {}});

        const auto compatible = queue.validateReceiverBinding(schema, family, policy, queueWork);

        if (!compatible)
            return std::unexpected(ReceiverFailure{compatible.error(), {}});

        for (std::size_t i = 0; i < bindings.size(); ++i) {
            bindingIndex = i;
            debit(receiverWork.visits, 1);
            const auto &binding = bindings[i];

            if (!validBinding(binding) || binding.localId < 0 ||
                static_cast<std::size_t>(binding.localId) >= schema.records().size())
                return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});

            for (std::size_t previous = 0; previous < i; ++previous) {
                debit(receiverWork.visits, 1);

                if (bindings[previous].localId == binding.localId)
                    return std::unexpected(ReceiverFailure{ReceiverError::INVALID_INPUT, i});
            }

            const auto fields = schema.records()[static_cast<std::size_t>(binding.localId)].fields.size();

            if (fields > receiverLimits.shapeFields || binding.mappingLimits.selections > receiverLimits.selections)
                return std::unexpected(ReceiverFailure{ReceiverError::RESOURCE_LIMIT, i});

            receiverLimits.shapeFields -= fields;
            receiverLimits.selections -= binding.mappingLimits.selections;
        }

        detail::ReceiverPlan plan;
        plan.routes.reserve(bindings.size());

        for (std::size_t i = 0; i < bindings.size(); ++i) {
            bindingIndex = i;
            debit(receiverWork.visits, 1);
            const auto &binding = bindings[i];
            debit(receiverWork.preparationWork, preparationAllowance(binding.mappingLimits));
            const auto fields = schema.records()[static_cast<std::size_t>(binding.localId)].fields.size();

            if (binding.kind == PreparedEventKind::QUOTE) {
                auto prepared = events::PreparedQuote::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else if (binding.kind == PreparedEventKind::TRADE) {
                auto prepared = events::PreparedTrade::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            } else {
                auto prepared = events::PreparedProfile::prepare(schema, binding.localId, binding.mappingLimits);

                if (!prepared)
                    return std::unexpected(ReceiverFailure{prepared.error(), i});

                plan.routes.push_back({schema.recordContext(binding.localId), std::move(*prepared), binding.valueLimits,
                                       binding.eventTimePolicy, fields, binding.mappingLimits.selections});
            }
        }

        bindingIndex.reset();
        auto adapter = std::make_shared<detail::SubscriptionReceiver>(std::move(plan), consumer);
        auto handle = queue.open(clientLimits, adapter, queueWork);

        if (!handle)
            return std::unexpected(ReceiverFailure{handle.error(), {}});

        return std::move(*handle);
    } catch (ReceiverError error) {
        return std::unexpected(ReceiverFailure{error, bindingIndex});
    } catch (const std::bad_alloc &) {
        return std::unexpected(ReceiverFailure{ReceiverError::ALLOCATION_FAILURE, bindingIndex});
    } catch (const std::length_error &) {
        return std::unexpected(ReceiverFailure{ReceiverError::ALLOCATION_FAILURE, bindingIndex});
    }
}
} // namespace org::ttldtor::qdcxx::client
