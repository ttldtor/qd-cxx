// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/event_registry.hpp"

#include "../events/order_index_coordinate.hpp"
#include "event_registry_defaults.hpp"
#include "event_selection.hpp"
#include "qd/events/analytic_order.hpp"
#include "qd/events/configuration.hpp"
#include "qd/events/derived_order.hpp"
#include "qd/events/market_maker.hpp"
#include "qd/events/order.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/events/spread_order.hpp"
#include "qd/events/summary.hpp"
#include "qd/events/text_events.hpp"
#include "qd/events/trade.hpp"
#include "qd/events/trade_eth.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::api {
namespace {
/** Bound and measure the actual shared-state allocation, including implementation-selected control metadata.
 * @tparam T Rebound element selected by allocate_shared for this fixed, non-overaligned state.
 */
template <typename T> struct StateAllocator {
    using value_type = T;   ///< Standard allocator element type.
    std::size_t *requested; ///< Construction-local counter, never inspected by deallocation or destruction.
    std::size_t limit;      ///< Complete shared allocation request ceiling after schema/container backing is reserved.

    /** Retain construction-local accounting through allocator rebinding.
     * @param requested Counter alive through allocation; it need not survive final shared-state retirement.
     * @param limit Maximum aggregate bytes this shared-state allocation may request.
     */
    StateAllocator(std::size_t &requested, std::size_t limit) noexcept : requested(&requested), limit(limit) {
    }

    /** Preserve complete finite accounting when allocate_shared selects its allocation element.
     * @tparam U Original allocator element.
     * @param other Construction allocator with the same active accounting domain.
     */
    template <typename U>
    StateAllocator(const StateAllocator<U> &other) noexcept : requested(other.requested), limit(other.limit) {
    }

    /** Refuse the complete allocation request before calling operator new.
     * @param count Number of rebound elements.
     * @return Exact allocated backing; resource refusal or bad_alloc propagates to the atomic factory boundary.
     */
    T *allocate(std::size_t count) {
        static_assert(alignof(T) <= alignof(std::max_align_t), "Registry shared state requires ordinary alignment");

        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw EventRegistryError::RESOURCE_LIMIT;
        }

        const auto bytes = count * sizeof(T);

        if (*requested > limit || bytes > limit - *requested) {
            throw EventRegistryError::RESOURCE_LIMIT;
        }

        auto *result = static_cast<T *>(::operator new(bytes));
        *requested += bytes;

        return result;
    }

    /** Release actual shared backing without reading the retired construction-local counter.
     * @param pointer Original allocation.
     * @param count Original element count, unnecessary for ordinary operator delete.
     */
    void deallocate(T *pointer, std::size_t count) noexcept {
        static_cast<void>(count);
        ::operator delete(pointer);
    }

    /** Compare interchangeable ordinary allocation ownership.
     * @tparam U Rebound allocator element.
     * @param other Other allocator, whose counter is irrelevant to deallocation.
     * @return True; every instance releases through ordinary operator delete.
     */
    template <typename U> bool operator==(const StateAllocator<U> &other) const noexcept {
        static_cast<void>(other);

        return true;
    }
};

/** Add finite logical counts without wrapping.
 * @param left Original amount.
 * @param right Additional amount in the same units.
 * @return Exact sum or registry resource refusal.
 */
std::size_t add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw EventRegistryError::RESOURCE_LIMIT;
    }

    return left + right;
}

/** Multiply finite logical counts without wrapping.
 * @param count Number of elements.
 * @param bytes Bytes in one element.
 * @return Exact product or registry resource refusal.
 */
std::size_t multiply(std::size_t count, std::size_t bytes) {
    if (bytes && count > std::numeric_limits<std::size_t>::max() / bytes) {
        throw EventRegistryError::RESOURCE_LIMIT;
    }

    return count * bytes;
}

/** Debit attempted work without refunds or internal replenishment.
 * @param remaining Caller-owned credits.
 * @param amount Complete conservative attempted charge.
 */
void debit(std::size_t &remaining, std::size_t amount) {
    if (amount > remaining) {
        throw EventRegistryError::WORK_LIMIT;
    }

    remaining -= amount;
}

/** Prepay the existing immutable schema copy's count and name-unit inspection.
 * @param schema Validated schema, whose capacities are checked independently.
 * @param work Remaining caller-owned credits.
 */
void chargeSchema(const core::LocalSchema &schema, EventRegistryWork &work) {
    debit(work.visits, schema.records().size());

    for (const auto &record : schema.records()) {
        debit(work.visits, add(add(record.fields.size(), record.lookups.size()), record.name.size()));

        for (const auto &lookup : record.lookups) {
            debit(work.visits, lookup.name.size());
        }
    }
}

/** Conservatively prepay one existing mapper's independent declared allowances.
 * @param limits Finite mapper counts, text units and comparison allowance.
 * @return Checked complete work allowance.
 */
std::size_t mappingWork(events::MappingLimits limits) {
    std::size_t result = 0;

    for (const auto amount :
         std::array{limits.fields, limits.lookups, limits.selections, limits.keys, limits.typeAlternatives,
                    limits.nameUnits, limits.totalNameUnits, limits.comparisonWork}) {
        result = add(result, amount);
    }

    return result;
}

/** Reject unknown enums rather than treating them as a fallback mapper.
 * @param registration Caller-supplied route.
 * @return Whether the projection and timestamp policy are known.
 */
bool validRegistration(const EventRegistration &registration) noexcept {
    return (registration.type == EventType::QUOTE || registration.type == EventType::PROFILE ||
            registration.type == EventType::TRADE || registration.type == EventType::TRADE_ETH ||
            registration.type == EventType::SUMMARY || registration.type == EventType::TEXT_MESSAGE ||
            registration.type == EventType::TEXT_CONFIGURATION || registration.type == EventType::MESSAGE ||
            registration.type == EventType::CONFIGURATION || registration.type == EventType::MARKET_MAKER ||
            detail::isOrderSourceEvent(registration.type)) &&
           (registration.eventTimePolicy == EventTimePolicy::UNSPECIFIED ||
            registration.eventTimePolicy == EventTimePolicy::FIELD_METADATA) &&
           (registration.projection == events::EventProjection::NATIVE ||
            (registration.type == EventType::ORDER &&
             (registration.projection == events::EventProjection::QUOTE_ORDERS ||
              registration.projection == events::EventProjection::MARKET_MAKER_ORDERS)));
}

/** Construct the fixed nine native declarations once, without preparing duplicate registry routes.
 * @param limits Complete schema and registry bounds.
 * @param work Remaining attempted preparation work.
 * @return Independently owned validated complete native schema or atomic refusal.
 */
std::expected<core::LocalSchema, EventRegistryFailure> defaultNativeSchema(
    EventRegistryLimits limits,
    EventRegistryWork
        &work) { // Prepay construction of all nine fixed input catalogs and their aliases before allocating them.
    constexpr std::size_t DEFAULT_CONSTRUCTION_WORK = 4096;
    debit(work.visits, DEFAULT_CONSTRUCTION_WORK);

    auto quote = events::defaultQuoteSchema(limits.schema);
    auto profile = events::defaultProfileSchema(limits.schema);
    auto trade = events::defaultTradeSchema(limits.schema);
    auto tradeETH = events::defaultTradeETHSchema(limits.schema);
    auto summary = events::defaultSummarySchema(limits.schema);
    auto textMessage = events::defaultTextMessageSchema(limits.schema);
    auto textConfiguration = events::defaultTextConfigurationSchema(limits.schema);
    auto message = events::defaultMessageSchema(limits.schema);
    auto configuration = events::defaultConfigurationSchema(limits.schema);

    if (!message || !configuration) {
        return std::unexpected(EventRegistryFailure{!message ? message.error() : configuration.error(), {}});
    }

    if (!quote || !profile) {
        return std::unexpected(EventRegistryFailure{!quote ? quote.error() : profile.error(), {}});
    }

    if (!trade)
        return std::unexpected(EventRegistryFailure{trade.error(), {}});

    if (!tradeETH) {
        return std::unexpected(EventRegistryFailure{tradeETH.error(), {}});
    }

    if (!summary) {
        return std::unexpected(EventRegistryFailure{summary.error(), {}});
    }

    if (!textMessage || !textConfiguration) {
        return std::unexpected(
            EventRegistryFailure{!textMessage ? textMessage.error() : textConfiguration.error(), {}});
    }

    std::array records{quote->records()[0],
                       profile->records()[0],
                       trade->records()[0],
                       tradeETH->records()[0],
                       summary->records()[0],
                       textMessage->records()[0],
                       textConfiguration->records()[0],
                       message->records()[0],
                       configuration->records()[0]};

    for (std::size_t index = 0; index < records.size(); ++index) {
        records[index].id = static_cast<std::int32_t>(index);
    }

    auto combined = core::LocalSchema::create(records, limits.schema, core::LocalProfile::BINARY_FIELDS_V1);

    if (!combined) {
        return std::unexpected(EventRegistryFailure{combined.error(), {}});
    }

    return std::move(*combined);
}
} // namespace

/** Complete shared immutable storage; sharing never retains caller containers or callbacks. */
struct EventRegistry::State {
    core::LocalSchema schema; ///< Complete independently owned declarations, retaining validated provenance.
    std::vector<EventRegistration> registrations;          ///< Caller order, with unique exact local record routes.
    std::vector<std::optional<std::size_t>> versionFields; ///< Exact prepared version selections aligned with routes.
    std::size_t storageBytes; ///< Complete schema/container/shared-allocation request backing.

    /** Adopt complete candidate storage after all route checks succeeded.
     * @param schema Independently owned bounded schema copy.
     * @param registrations Independently owned bounded route declarations.
     * @param versionFields Optional selected version positions aligned with routes.
     * @param storageBytes Checked logical retained backing.
     */
    State(core::LocalSchema schema, std::vector<EventRegistration> registrations,
          std::vector<std::optional<std::size_t>> versionFields, std::size_t storageBytes)
        : schema(std::move(schema)), registrations(std::move(registrations)), versionFields(std::move(versionFields)),
          storageBytes(storageBytes) {
    }
};

EventRegistry::EventRegistry(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {
}

std::expected<EventRegistry, EventRegistryFailure>
EventRegistry::create(const core::LocalSchema &schema, std::span<const EventRegistration> registrations,
                      EventRegistryLimits limits, EventRegistryWork &work) {
    std::optional<std::size_t> index;

    try {
        debit(work.visits, 1);

        if (!schema.identity().valid()) {
            return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, {}});
        }

        if (registrations.size() > limits.registrations || schema.storageBytes() > limits.storageBytes) {
            return std::unexpected(EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT, {}});
        }

        const auto minimumBytes =
            add(add(sizeof(State), schema.storageBytes() - sizeof(core::LocalSchema)),
                multiply(registrations.size(), sizeof(EventRegistration) + sizeof(std::optional<std::size_t>)));

        if (minimumBytes > limits.storageBytes) {
            return std::unexpected(EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT, {}});
        }

        chargeSchema(schema, work);

        auto ownedSchema = schema.boundedCopy(limits.schema);

        if (!ownedSchema) {
            return std::unexpected(EventRegistryFailure{ownedSchema.error(), {}});
        }

        std::vector<EventRegistration> routes;
        routes.reserve(registrations.size());
        std::vector<std::optional<std::size_t>> versionFields;
        versionFields.reserve(registrations.size());

        for (std::size_t position = 0; position < registrations.size(); ++position) {
            index = position;
            debit(work.visits, 1);

            const auto &registration = registrations[position];
            std::optional<std::size_t> versionField;

            if (!validRegistration(registration) || registration.localRecordId < 0 ||
                static_cast<std::size_t>(registration.localRecordId) >= ownedSchema->records().size()) {
                return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, index});
            }

            for (const auto &previous : routes) {
                debit(work.visits, 1);

                if (previous.localRecordId == registration.localRecordId) {
                    const bool native = registration.projection == events::EventProjection::NATIVE;
                    const bool previousNative = previous.projection == events::EventProjection::NATIVE;

                    if (native == previousNative) {
                        return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, index});
                    }

                    const auto nativeType = native ? registration.type : previous.type;
                    const auto derived = native ? previous.projection : registration.projection;

                    if ((derived == events::EventProjection::QUOTE_ORDERS && nativeType != EventType::QUOTE) ||
                        (derived == events::EventProjection::MARKET_MAKER_ORDERS &&
                         nativeType != EventType::MARKET_MAKER)) {
                        return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, index});
                    }
                }
            }

            debit(work.preparation, mappingWork(registration.mappingLimits));

            if (registration.projection != events::EventProjection::NATIVE) {
                const auto kind = registration.projection == events::EventProjection::QUOTE_ORDERS
                                      ? events::DerivedOrderKind::QUOTE
                                      : events::DerivedOrderKind::MARKET_MAKER;
                const auto mapping = events::PreparedDerivedOrder::prepare(*ownedSchema, registration.localRecordId,
                                                                           kind, registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::MARKET_MAKER) {
                const auto mapping = events::PreparedMarketMaker::prepare(*ownedSchema, registration.localRecordId,
                                                                          registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::QUOTE) {
                const auto mapping = events::PreparedQuote::prepare(*ownedSchema, registration.localRecordId,
                                                                    registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::TRADE) {
                const auto mapping = events::PreparedTrade::prepare(*ownedSchema, registration.localRecordId,
                                                                    registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::TRADE_ETH) {
                const auto mapping = events::PreparedTradeETH::prepare(*ownedSchema, registration.localRecordId,
                                                                       registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::ORDER) {
                const auto mapping = events::PreparedOrder::prepare(*ownedSchema, registration.localRecordId,
                                                                    registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::SPREAD_ORDER) {
                const auto mapping = events::PreparedSpreadOrder::prepare(*ownedSchema, registration.localRecordId,
                                                                          registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::ANALYTIC_ORDER) {
                const auto mapping = events::PreparedAnalyticOrder::prepare(*ownedSchema, registration.localRecordId,
                                                                            registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::SUMMARY) {
                const auto mapping = events::PreparedSummary::prepare(*ownedSchema, registration.localRecordId,
                                                                      registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::MESSAGE) {
                const auto mapping = events::PreparedMessage::prepare(*ownedSchema, registration.localRecordId,
                                                                      registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::CONFIGURATION) {
                const auto mapping = events::PreparedConfiguration::prepare(*ownedSchema, registration.localRecordId,
                                                                            registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }

                versionField = mapping->versionField();
            } else if (registration.type == EventType::TEXT_MESSAGE) {
                const auto mapping = events::PreparedTextMessage::prepare(*ownedSchema, registration.localRecordId,
                                                                          registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            } else if (registration.type == EventType::TEXT_CONFIGURATION) {
                const auto mapping = events::PreparedTextConfiguration::prepare(
                    *ownedSchema, registration.localRecordId, registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }

                versionField = mapping->versionField();
            } else {
                const auto mapping = events::PreparedProfile::prepare(*ownedSchema, registration.localRecordId,
                                                                      registration.mappingLimits);

                if (!mapping) {
                    return std::unexpected(EventRegistryFailure{mapping.error(), index});
                }
            }

            if (registration.projection == events::EventProjection::NATIVE &&
                detail::isOrderSourceEvent(registration.type)) {
                const auto &record = ownedSchema->records()[static_cast<std::size_t>(registration.localRecordId)];

                if (!events::detail::hasOrderIndexCoordinate(record)) {
                    return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, index});
                }
            }

            routes.push_back(registration);
            versionFields.push_back(versionField);
        }

        index.reset();

        // The rebound allocator counts State and its control block, including State's inline schema object.
        const auto backingBytes = add(ownedSchema->storageBytes() - sizeof(core::LocalSchema),
                                      add(multiply(routes.capacity(), sizeof(EventRegistration)),
                                          multiply(versionFields.capacity(), sizeof(std::optional<std::size_t>))));

        if (backingBytes > limits.storageBytes) {
            return std::unexpected(EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT, {}});
        }

        std::size_t sharedBytes = 0;
        auto state =
            std::allocate_shared<State>(StateAllocator<State>{sharedBytes, limits.storageBytes - backingBytes},
                                        std::move(*ownedSchema), std::move(routes), std::move(versionFields), 0);
        state->storageBytes = add(backingBytes, sharedBytes);

        return EventRegistry(std::move(state));
    } catch (EventRegistryError error) {
        return std::unexpected(EventRegistryFailure{error, index});
    } catch (const std::bad_alloc &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, index});
    } catch (const std::length_error &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, index});
    }
}

std::expected<EventRegistry, EventRegistryFailure> EventRegistry::builtIn(EventRegistryLimits limits,
                                                                          EventRegistryWork &work) {
    try {
        auto combined = defaultNativeSchema(limits, work);

        if (!combined) {
            return std::unexpected(combined.error());
        }

        const std::array registrations{EventRegistration{EventType::QUOTE, 0},
                                       EventRegistration{EventType::PROFILE, 1},
                                       EventRegistration{EventType::TRADE, 2},
                                       EventRegistration{EventType::TRADE_ETH, 3},
                                       EventRegistration{EventType::SUMMARY, 4},
                                       EventRegistration{EventType::TEXT_MESSAGE, 5},
                                       EventRegistration{EventType::TEXT_CONFIGURATION, 6},
                                       EventRegistration{EventType::MESSAGE, 7},
                                       EventRegistration{EventType::CONFIGURATION, 8}};

        return create(*combined, registrations, limits, work);
    } catch (EventRegistryError error) {
        return std::unexpected(EventRegistryFailure{error, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    } catch (const std::length_error &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    }
}

bool EventRegistry::valid() const noexcept {
    return bool(state_);
}

const core::LocalSchema &EventRegistry::schema() const noexcept {
    return state_->schema;
}

std::span<const EventRegistration> EventRegistry::registrations() const noexcept {
    return state_->registrations;
}

std::optional<std::size_t> EventRegistry::versionField(std::int32_t localRecordId) const noexcept {
    for (std::size_t index = 0; index < state_->registrations.size(); ++index) {
        if (state_->registrations[index].localRecordId == localRecordId) {
            return state_->versionFields[index];
        }
    }

    return std::nullopt;
}

std::size_t EventRegistry::storageBytes() const noexcept {
    return state_ ? state_->storageBytes : 0;
}

namespace detail {
namespace {
/** Build an explicit derived route while retaining the ordinary finite mapper defaults.
 * @param id Exact physical local record ID.
 * @param family Verified physical-to-business delegate family.
 * @return Complete ordinary Order route declaration.
 */
EventRegistration derivedRegistration(std::int32_t id, events::EventProjection family) {
    EventRegistration result{EventType::ORDER, id};
    result.projection = family;

    return result;
}
} // namespace

std::expected<EventRegistry, EventRegistryFailure> defaultTickerEventRegistry(EventRegistryLimits limits,
                                                                              EventRegistryWork &work) {
    try {
        // Fixed catalogs are prepaid before their first candidate allocation.
        constexpr std::size_t REGIONAL_CONSTRUCTION_WORK = 16384;
        debit(work.visits, REGIONAL_CONSTRUCTION_WORK);
        auto base = defaultNativeSchema(limits, work);

        if (!base) {
            return std::unexpected(base.error());
        }

        std::vector<core::LocalRecord> records(base->records().begin(), base->records().end());
        std::vector<EventRegistration> routes{{EventType::QUOTE, 0},
                                              {EventType::PROFILE, 1},
                                              {EventType::TRADE, 2},
                                              {EventType::TRADE_ETH, 3},
                                              {EventType::SUMMARY, 4},
                                              {EventType::TEXT_MESSAGE, 5},
                                              {EventType::TEXT_CONFIGURATION, 6},
                                              {EventType::MESSAGE, 7},
                                              {EventType::CONFIGURATION, 8}};
        constexpr std::size_t DEFAULT_PHYSICAL_RECORDS = 35;
        constexpr std::size_t DEFAULT_ROUTES = 62;
        records.reserve(DEFAULT_PHYSICAL_RECORDS);
        routes.reserve(DEFAULT_ROUTES);
        routes.push_back(derivedRegistration(0, events::EventProjection::QUOTE_ORDERS));
        // Regional production records omit the two exchange fields; the record suffix supplies that fact.
        const auto composite = records[0];

        for (char16_t exchange = u'A'; exchange <= u'Z'; ++exchange) {
            core::LocalRecord regional{
                static_cast<std::int32_t>(records.size()), u"Quote&" + std::u16string(1, exchange), false, {}, {}};

            for (std::size_t field = 0; field < composite.fields.size(); ++field) {
                constexpr std::size_t BID_EXCHANGE_FIELD = 1, ASK_EXCHANGE_FIELD = 5;

                if (field == BID_EXCHANGE_FIELD || field == ASK_EXCHANGE_FIELD) {
                    continue;
                }

                const auto target = regional.fields.size();
                regional.fields.push_back(composite.fields[field]);

                for (const auto &lookup : composite.lookups) {
                    if (lookup.fieldId != field) {
                        continue;
                    }

                    auto name = lookup.name;

                    if (name.starts_with(u"Quote.")) {
                        name = regional.name + name.substr(5);
                    }

                    regional.lookups.push_back({std::move(name), target});
                }
            }

            const auto id = regional.id;
            records.push_back(std::move(regional));
            routes.push_back({EventType::QUOTE, id});
            routes.push_back(derivedRegistration(id, events::EventProjection::QUOTE_ORDERS));
        }

        auto schema = core::LocalSchema::create(records, limits.schema, core::LocalProfile::BINARY_FIELDS_V1);

        if (!schema) {
            return std::unexpected(EventRegistryFailure{schema.error(), {}});
        }

        return EventRegistry::create(*schema, routes, limits, work);
    } catch (EventRegistryError error) {
        return std::unexpected(EventRegistryFailure{error, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    } catch (const std::length_error &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    }
}

std::expected<EventRegistry, EventRegistryFailure> defaultHistoryEventRegistry(EventRegistryLimits limits,
                                                                               EventRegistryWork &work) {
    try {
        constexpr std::size_t DEFAULT_INDEXED_CONSTRUCTION_WORK = 2048;
        debit(work.visits, DEFAULT_INDEXED_CONSTRUCTION_WORK);
        auto order = events::defaultOrderSchema(limits.schema);

        if (!order) {
            return std::unexpected(EventRegistryFailure{order.error(), {}});
        }

        auto maker = events::defaultMarketMakerSchema(limits.schema);

        if (!maker) {
            return std::unexpected(EventRegistryFailure{maker.error(), {}});
        }

        auto spread = events::defaultSpreadOrderSchema(limits.schema);

        if (!spread) {
            return std::unexpected(EventRegistryFailure{spread.error(), {}});
        }

        auto analytic = events::defaultAnalyticOrderSchema(limits.schema);

        if (!analytic) {
            return std::unexpected(EventRegistryFailure{analytic.error(), {}});
        }

        std::array records{order->records()[0], maker->records()[0], spread->records()[0], analytic->records()[0]};

        for (std::size_t index = 0; index < records.size(); ++index) {
            records[index].id = static_cast<std::int32_t>(index);
        }

        auto schema = core::LocalSchema::create(records, limits.schema, core::LocalProfile::BINARY_FIELDS_V1);

        if (!schema) {
            return std::unexpected(EventRegistryFailure{schema.error(), {}});
        }

        const std::array routes{EventRegistration{EventType::ORDER, 0}, EventRegistration{EventType::MARKET_MAKER, 1},
                                derivedRegistration(1, events::EventProjection::MARKET_MAKER_ORDERS),
                                EventRegistration{EventType::SPREAD_ORDER, 2},
                                EventRegistration{EventType::ANALYTIC_ORDER, 3}};

        return EventRegistry::create(*schema, routes, limits, work);
    } catch (EventRegistryError error) {
        return std::unexpected(EventRegistryFailure{error, {}});
    } catch (const std::bad_alloc &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    } catch (const std::length_error &) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::ALLOCATION_FAILURE, {}});
    }
}
} // namespace detail
} // namespace org::ttldtor::qdcxx::api
