// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "receiving_profile.hpp"
#include "callback_execution.hpp"
#include "event_registry_defaults.hpp"

#include "../events/candle_receiving_limits.hpp"
#include "../events/derived_order_sources.hpp"
#include "../events/order_record_source.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "qd/events/time_helpers.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::size_t MEBIBYTE = 1024 * 1024; ///< Independent logical byte unit.
constexpr core::SchemaLimits BASE_SCHEMA_LIMITS{42, 284, 724, 128, 16384};
// Reserve exactly the two records, 20 fields and 25 lookups of the Candle/TradeHistory declarations.
constexpr core::SchemaLimits CANDLE_SCHEMA_LIMITS{44, 284 + 20, 724 + 25, 128, 16384};

/** Reserve the default NuamTrade record, its fields and aliases.
 * @param limits Existing complete schema allowances.
 * @return Allowances including the composite NuamTrade record.
 */
constexpr core::SchemaLimits withNuamTradeLimits(core::SchemaLimits limits) noexcept {
    constexpr std::size_t FIELDS = 18;
    constexpr std::size_t LOOKUPS = 28;
    ++limits.records;
    limits.fields += FIELDS;
    limits.lookups += LOOKUPS;

    return limits;
}

/** Reserve the standard OptionSale logical fields and aliases.
 * @param limits Existing complete schema allowances.
 * @return Allowances including OptionSale.
 */
constexpr core::SchemaLimits withOptionSaleLimits(core::SchemaLimits limits) noexcept {
    constexpr std::size_t FIELDS = 14;
    limits.fields += FIELDS;
    limits.lookups += FIELDS;

    return limits;
}

/** Reserve the exact logical field and alias totals of the standard TimeAndSale declarations.
 * @param limits Existing complete allowances, with enough record and name capacity.
 * @return Complete allowances for the previous profile plus TimeAndSale.
 */
constexpr core::SchemaLimits withTimeAndSaleLimits(core::SchemaLimits limits) noexcept {
    constexpr std::size_t FIELDS_PER_RECORD = 10;
    constexpr std::size_t LOOKUPS_PER_RECORD = 14;
    limits.fields += TIME_AND_SALE_DEFAULT_RECORDS * FIELDS_PER_RECORD;
    limits.lookups += TIME_AND_SALE_DEFAULT_RECORDS * LOOKUPS_PER_RECORD;

    return limits;
}

/** Reserve the composite Nuam sale declaration in a default schema. */
constexpr core::SchemaLimits withNuamTimeAndSaleLimits(core::SchemaLimits limits) noexcept {
    ++limits.records;
    limits.fields += 11;
    limits.lookups += 15;

    return limits;
}

constexpr std::size_t FRAME_RECORDS = 64; ///< Whole admitted DATA frame record ceiling.
constexpr std::size_t CALLBACK_RECORDS = CallbackSlotPool::PHYSICAL_RECORDS; ///< Whole callback projection ceiling.
constexpr std::size_t FIELD_TEXT_UNITS = 8192;             ///< Aggregate text copied from one complete record.
constexpr std::size_t FIELD_BYTE_UNITS = 16384;            ///< Aggregate opaque payload copied from one record.
constexpr std::size_t RECORD_ARENA_BYTES = 65536;          ///< Complete detached per-record arena request ceiling.
constexpr std::size_t SESSION_SNAPSHOT_BYTES = 128 * 1024; ///< Maximum backing for a Feed snapshot.
constexpr std::size_t RECORD_BYTES = 16 * MEBIBYTE;        ///< Per-registration detached backing ceiling.
constexpr std::size_t BATCH_BYTES = MEBIBYTE;              ///< Per-registration composite backing ceiling.
constexpr std::size_t CONTROL_OPERATIONS = 100000;         ///< Qualified finite one-client owner work seed.
constexpr std::size_t CONTROL_VISITS = 100000000;          ///< Qualified finite metadata/equality work seed.
constexpr std::size_t CONTROL_BYTES = 256 * MEBIBYTE;      ///< Qualified logical copy/disposal seed.
constexpr std::size_t DNS_ENVELOPE_BYTES = 80 * MEBIBYTE;  ///< Separate documented DNS SDK envelope.
constexpr std::uint64_t DNS_PROGRESS_UNITS = 351280142190671838ULL; ///< Complete documented DNS logical charge.
constexpr std::size_t WRITER_CONFIGURATION_BYTES = 4 * MEBIBYTE;    ///< Retained complete writer reservation.

/** Reject before activating any native state.
 * @param code Detached high-level category.
 * @param stage Static library-owned checkpoint.
 */
[[noreturn]] void refuse(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** Add finite quantities without wrapping.
 * @param left Original amount.
 * @param right Additional amount in the same units.
 * @return Exact sum or detached resource refusal.
 */
std::size_t add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-arithmetic");
    }

    return left + right;
}

/** Multiply finite capacities without wrapping.
 * @param count Number of independent reservations.
 * @param amount One reservation's amount.
 * @return Exact product or detached resource refusal.
 */
std::size_t multiply(std::size_t count, std::size_t amount) {
    if (amount && count > std::numeric_limits<std::size_t>::max() / amount) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-arithmetic");
    }

    return count * amount;
}

/** Select an immutable caller registry or prepare compatible defaults without native work.
 * @param options Immutable endpoint policy and optional explicit registry.
 * @return Owning immutable snapshot or detached registry refusal.
 */
EventRegistry selectRegistry(const EndpointOptions &options) {
    const auto *registry = options.eventRegistry ? &*options.eventRegistry : nullptr;

    if (registry) {
        if (!registry->valid()) {
            refuse(ErrorCode::INVALID_ARGUMENT, "profile-registry");
        }

        return *registry;
    }

    auto result = defaultReceivingRegistry(options.contract);

    if (!result) {
        const auto allocation = std::visit(
            [](auto error) {
                using Error = decltype(error);

                return error == Error::ALLOCATION_FAILURE;
            },
            result.error().cause);
        refuse(allocation ? ErrorCode::ALLOCATION_FAILURE : ErrorCode::UNSUPPORTED_PROFILE, "profile-registry");
    }

    return std::move(*result);
}

/** Bound distinct physical coordinates for any one logical selector before reserving desired storage.
 * @param registry Immutable schema-bound route catalog, already bounded to the receiving ceilings.
 * @param contract Exact route eligibility policy used by mapping preparation.
 * @return Checked coordinate-domain ceiling; this does not change the physical schema record count.
 */
std::size_t receivingCoordinateCapacity(const EventRegistry &registry, DeliveryContract contract) {
    struct Context {
        bool registered = false;
        bool eligible = false;
        bool nativeQuote = false;
        bool derivedQuote = false;
        char16_t exchange = 0;
    };

    std::array<Context, MAXIMUM_RECEIVING_RECORDS> contexts{};
    // This complete bounded scan is paid before desired storage allocation or native publication.
    auto visits =
        add(MAXIMUM_RECEIVING_BINDINGS, multiply(MAXIMUM_RECEIVING_RECORDS, add(MAXIMUM_RECEIVING_RECORDS, 1)));
    const auto visit = [&] {
        if (visits == 0) {
            refuse(ErrorCode::RESOURCE_LIMIT, "profile-coordinate-work");
        }

        --visits;
    };

    for (const auto &route : registry.registrations()) {
        visit();
        const auto id = static_cast<std::size_t>(route.localRecordId);
        auto &context = contexts[id];
        context.registered = true;

        if (!receivingRouteSupportsContract(route, contract)) {
            continue;
        }

        context.eligible = true;
        context.nativeQuote |= route.type == EventType::QUOTE && route.projection == events::EventProjection::NATIVE;
        context.derivedQuote |= route.projection == events::EventProjection::QUOTE_ORDERS;
        context.exchange = events::market_symbols::getExchangeCode(registry.schema().records()[id].name);
    }

    std::size_t registered = 0;
    std::size_t eligible = 0;
    std::size_t largestDoubleBucket = 0;

    for (const auto &context : contexts) {
        visit();
        registered = add(registered, context.registered);
        eligible = add(eligible, context.eligible);
        std::size_t bucket = 0;

        for (const auto &other : contexts) {
            visit();

            if (context.exchange && context.exchange == other.exchange && other.nativeQuote && other.derivedQuote) {
                bucket = add(bucket, 1);
            }
        }

        largestDoubleBucket = std::max(largestDoubleBucket, bucket);
    }

    return std::max(registered, add(eligible, largestDoubleBucket));
}

/** Copy every complete field using its first explicit wire alias.
 * @param schema Complete selected declarations, including unregistered generic records and extra fields.
 * @return Complete one-to-one outgoing description blueprint.
 */
std::vector<protocol::OutgoingRecord> prepareBlueprint(const core::LocalSchema &schema) {
    std::vector<protocol::OutgoingRecord> result;
    result.reserve(schema.records().size());

    for (const auto &record : schema.records()) {
        protocol::OutgoingRecord outgoing{record.id, record.id, record.name, {}};
        outgoing.fields.reserve(record.fields.size());

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });

            if (lookup == record.lookups.end()) {
                refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-wire-alias");
            }

            outgoing.fields.push_back(
                {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        result.push_back(std::move(outgoing));
    }

    return result;
}

/** Create an explicit finite multi-registration work seed from the existing qualified one-client profile.
 * Normal turns and drain use this same seed, with no retry-time top-up.
 * @param scale Simultaneous registration/selector reservation multiplier.
 * @param interests Complete finite authoritative interest capacities.
 * @param recordCount Actual retained schema record count used by MuxState queries.
 * @param routeRecords Maximum records in one atomic STREAM routing chunk.
 * @return Independent finite queue/component work coordinates.
 */
mux::QueueWork queueWork(std::size_t scale, const mux::MuxLimits &interests, std::size_t recordCount,
                         std::size_t routeRecords) {
    const auto operations = multiply(scale, CONTROL_OPERATIONS);
    const auto visits = multiply(scale, CONTROL_VISITS);
    const auto bytes = multiply(scale, CONTROL_BYTES);
    // MuxState::reserve(1) prepays every exact select before inspecting current demand. An atomic
    // STREAM chunk selects each offered record for every client and cannot commit a partial fanout.
    constexpr std::size_t QUERY_WORK_PASSES = 32;
    constexpr std::size_t QUERY_FIXED_TERMS = 2; // One base visit plus the one query input.
    const auto outer = add(add(add(add(QUERY_FIXED_TERMS, interests.clients), recordCount), interests.entries),
                           interests.aggregateEntries);
    const auto inner = add(interests.entries, QUERY_FIXED_TERMS);
    const auto queryVisits = multiply(QUERY_WORK_PASSES, multiply(outer, inner));
    const auto routeQueries = multiply(routeRecords, interests.clients);
    const auto routeVisits = multiply(routeQueries, queryVisits);
    const auto routeText = multiply(routeVisits, interests.symbolUnits);

    return {operations,
            visits,
            visits,
            visits,
            bytes,
            multiply(scale, 10000),
            multiply(scale, 10000),
            {operations, add(visits, routeVisits), add(visits, routeText), bytes},
            {operations, visits, visits},
            {operations, visits, visits, bytes},
            {operations, visits, visits, bytes},
            {operations, visits, visits, bytes}};
}

/** Fund a whole maximum DATA frame and conservative collector maintenance from actual finite capacities.
 * The source stage prepays its quadratic point scans and independent view/frame scans before mutation.
 * These credits are conservative logical work, not measured instructions or a latency guarantee.
 * @param profile Complete finite collector and session reservations, modified before publication.
 * @param maximumFields Largest complete retained record shape, including unregistered fields.
 */
void fundHistoryWork(ReceivingProfile &profile, std::size_t maximumFields) {
    const auto &limits = profile.historyLimits;
    const auto frames = add(add(limits.cacheFrames, multiply(limits.viewSlots, limits.pendingFrames)), 1);
    const auto perView =
        add(add(limits.viewSubscriptions.entries, limits.pendingItems), add(add(frames, limits.points), 4));
    const auto viewScan = multiply(add(limits.points, 1), multiply(limits.viewSlots, perView));
    constexpr std::size_t STAGE_SCAN_PASSES = 64;
    const auto rowVisits =
        multiply(STAGE_SCAN_PASSES, add(add(limits.partitions, multiply(limits.points, limits.points)),
                                        add(viewScan, add(maximumFields, 8))));
    const auto viewSlots =
        multiply(limits.viewSlots, add(add(limits.pendingItems, limits.viewSubscriptions.entries), maximumFields));
    const auto dimensions = add(add(add(limits.partitions, limits.points), add(frames, limits.viewSlots)),
                                add(add(viewSlots, profile.schema.records().size()), 8));
    // Covers fixed clone, cleanup, selection and source-owner accounting scans, including zero dimensions.
    constexpr std::size_t MAINTENANCE_SCAN_PASSES = 128;
    const auto maintenanceVisits = multiply(MAINTENANCE_SCAN_PASSES, multiply(dimensions, dimensions));
    auto &work = profile.queueWork.history;
    work.visits = add(work.visits, add(multiply(FRAME_RECORDS, rowVisits), maintenanceVisits));
    constexpr std::size_t TEXT_SCAN_PASSES = 8;
    const auto maintenanceText = multiply(
        TEXT_SCAN_PASSES,
        multiply(add(limits.partitions, 1),
                 multiply(add(limits.viewSlots, 1), add(limits.viewSubscriptions.totalSymbolUnits,
                                                        multiply(limits.partitions, limits.symbolUnitsPerPartition)))));
    const auto inputText = multiply(
        TEXT_SCAN_PASSES,
        multiply(FRAME_RECORDS, multiply(add(limits.partitions, 1), multiply(2, limits.symbolUnitsPerPartition))));
    work.textUnits = add(work.textUnits, add(maintenanceText, inputText));
    work.bytes =
        add(work.bytes, add(multiply(2, limits.residentBytes), profile.endpoint.settings().budgets.session.eventBytes));
}

/** Fund the existing complete snapshot binding scans from this profile's finite schema/descriptor bounds.
 * A burst shares one seed; this is prepayment before parsing, never a retry-time refill.
 * @param settings Complete candidate settings, not yet published.
 * @param records Actual retained local record cardinality used by LiveSnapshot::complete.
 */
void fundContextBindingText(client::EndpointSettings &settings, std::size_t records) {
    const auto &session = settings.budgets.session;
    const auto fields = std::min(
        {session.descriptions.fieldsPerDescription, session.descriptions.totalFields, session.context.bindings.fields});
    const auto perDescription = multiply(
        add(session.descriptions.totalNameUnits, session.context.schema.totalNameUnits), add(add(fields, records), 1));
    const auto wholeFrame = multiply(session.context.descriptions, perDescription);
    settings.budgets.protocolTurn.textUnits =
        add(settings.budgets.protocolTurn.textUnits, multiply(RECEIVING_OWNER_POLLS_PER_TURN, wholeFrame));
}

/** Reserve actual complete schema/blueprint copies and three replay banks.
 * @param profile Complete preparation, modified before publication.
 */
void prepareCopies(ReceivingProfile &profile) {
    transport::RecoverySchemaCapacity one;
    one.records = profile.schema.records().size();

    for (const auto &record : profile.schema.records()) {
        one.fields = add(one.fields, record.fields.size());
        one.lookups = add(one.lookups, record.lookups.size());
        one.nameUnits = add(one.nameUnits, add(record.name.size(), 8));

        for (const auto &lookup : record.lookups) {
            one.nameUnits = add(one.nameUnits, add(lookup.name.size(), 8));
        }
    }

    auto &limits = profile.recoveryOptions.limits;
    limits.schemaCopies = {multiply(3, one.records), multiply(3, one.fields), multiply(3, one.lookups),
                           multiply(3, one.nameUnits)};
    limits.blueprintCopy.records = profile.blueprint.size();
    limits.adoptedBlueprint.records = profile.blueprint.capacity();

    for (const auto &record : profile.blueprint) {
        limits.blueprintCopy.fields = add(limits.blueprintCopy.fields, record.fields.size());
        limits.adoptedBlueprint.fields = add(limits.adoptedBlueprint.fields, record.fields.capacity());
        limits.blueprintCopy.nameUnits = add(limits.blueprintCopy.nameUnits, add(record.wireName.size(), 8));
        limits.adoptedBlueprint.nameUnits = add(limits.adoptedBlueprint.nameUnits, add(record.wireName.capacity(), 1));

        for (const auto &field : record.fields) {
            limits.blueprintCopy.nameUnits = add(limits.blueprintCopy.nameUnits, add(field.wireName.size(), 8));
            limits.adoptedBlueprint.nameUnits =
                add(limits.adoptedBlueprint.nameUnits, add(field.wireName.capacity(), 1));
        }
    }

    const auto &host = profile.endpoint.settings().host;
    limits.endpointCopyBytes = multiply(2, std::max(add(host.capacity(), 1), add(host.size(), 16)));
    limits.writerConfigurationBytes = WRITER_CONFIGURATION_BYTES;
    const auto family = static_cast<std::size_t>(
        profile.family == core::SubscriptionFamily::TICKER    ? transport::RecoveryFamily::TICKER
        : profile.family == core::SubscriptionFamily::HISTORY ? transport::RecoveryFamily::HISTORY
                                                              : transport::RecoveryFamily::STREAM);
    const auto &desired = profile.endpoint.settings().budgets.desired;
    // Recovery inspects the actual collector owner maxima, including each independent TICKER view.
    // The queue requires every view's desired scope to cover its aggregate interest reservation.
    const auto owners = profile.family == core::SubscriptionFamily::TICKER    ? profile.tickerLimits.viewSlots
                        : profile.family == core::SubscriptionFamily::HISTORY ? profile.historyLimits.viewSlots
                                                                              : 1;
    limits.replay[family] = {multiply(owners, desired.entries), multiply(owners, desired.totalSymbolUnits)};
}

/** Prepare each existing mapping and exact whole-callback projection coordinates.
 * @param profile Complete preparation, modified before publication.
 * @param registry Immutable validated routes; complete schema identity is preserved by profile.schema.
 * @param symbolUnits Maximum owned exact symbol size.
 */
void prepareMappings(ReceivingProfile &profile, const EventRegistry &registry, std::size_t symbolUnits) {
    const events::EventValueLimits values{symbolUnits, FIELD_TEXT_UNITS, add(symbolUnits, 2 * FIELD_TEXT_UNITS)};
    const auto contract = profile.family == core::SubscriptionFamily::TICKER    ? DeliveryContract::TICKER
                          : profile.family == core::SubscriptionFamily::HISTORY ? DeliveryContract::HISTORY
                                                                                : DeliveryContract::STREAM;
    profile.candleRoutes = prepareReceivingCandleRoutes(registry, profile.schema, contract, symbolUnits);
    profile.bindings.reserve(registry.registrations().size());

    if (profile.bindings.capacity() > MAXIMUM_RECEIVING_BINDINGS)
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-mapping-storage");

    for (std::size_t kind = 0; kind < RECEIVING_EVENT_KINDS; ++kind) {
        for (const auto &registration : registry.registrations()) {
            if (static_cast<std::size_t>(registration.type) != kind ||
                !receivingRouteSupportsContract(registration, contract))
                continue;

            const auto index = profile.bindings.size();
            const auto &record = profile.schema.records()[static_cast<std::size_t>(registration.localRecordId)];

            if ((isOptionTimeSeriesEvent(registration.type) || registration.type == EventType::CANDLE ||
                 (registration.type == EventType::TIME_AND_SALE ||
                  registration.type == EventType::NUAM_TIME_AND_SALE)) &&
                profile.family == core::SubscriptionFamily::HISTORY) {
                if (!record.hasTime)
                    refuse(ErrorCode::UNSUPPORTED_PROFILE,
                           registration.type == EventType::GREEKS          ? "profile-greeks-time"
                           : registration.type == EventType::UNDERLYING    ? "profile-underlying-time"
                           : registration.type == EventType::THEO_PRICE    ? "profile-theo-price-time"
                           : registration.type == EventType::TIME_AND_SALE ? "profile-time-and-sale-time"
                                                                           : "profile-candle-time");

                profile.bindingTimeEncodings[index] = HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE;
            }

            profile.bindingExchanges[index] =
                (registration.type == EventType::PROFILE || registration.type == EventType::MARKET_MAKER ||
                 registration.type == EventType::SERIES || isOptionTimeSeriesEvent(registration.type) ||
                 registration.type == EventType::CANDLE || registration.type == EventType::OPTION_SALE ||
                 registration.type == EventType::TEXT_MESSAGE || registration.type == EventType::TEXT_CONFIGURATION ||
                 registration.type == EventType::MESSAGE || registration.type == EventType::CONFIGURATION ||
                 (usesOrderSource(registration.type) && registration.projection == events::EventProjection::NATIVE))
                    ? 0
                    : events::market_symbols::getExchangeCode(record.name);

            if (usesOrderSource(registration.type) && registration.projection == events::EventProjection::NATIVE) {
                auto mappingLimits = registration.mappingLimits;
                const auto source = events::detail::prepareRecordOrderSource(record.name, mappingLimits);

                if (!source) {
                    refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-order-source");
                }

                profile.bindingSources[index] = source->id();
            }

            const auto policy = registration.eventTimePolicy == EventTimePolicy::FIELD_METADATA
                                    ? client::ReceiverEventTimePolicy::FIELD_METADATA
                                    : client::ReceiverEventTimePolicy::UNSPECIFIED;

            if (const auto version = registry.versionField(registration.localRecordId)) {
                profile.versionBindings[profile.versionBindingCount++] = {
                    profile.schema.recordContext(registration.localRecordId), registration.localRecordId, *version};
            }

            if (registration.type == EventType::OPTION_SALE) {
                profile.snapshotRetransmitRecords[profile.snapshotRetransmitCount++] = registration.localRecordId;
            }

            profile.boundKinds |= 1U << kind;
            profile.bindings.push_back(
                {registration.localRecordId, static_cast<client::PreparedEventKind>(registration.type),
                 registration.mappingLimits, values, policy, FIELD_BYTE_UNITS, registration.projection});
        }
    }

    const auto planCount = std::size_t{1} << std::popcount(profile.boundKinds);
    profile.eventPlans.resize(planCount);

    if (profile.eventPlans.capacity() != planCount) {
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-selection-storage");
    }

    for (std::size_t denseMask = 0; denseMask < planCount; ++denseMask) {
        auto &plan = profile.eventPlans[denseMask];
        unsigned mask = 0;
        std::size_t denseBit = 1;

        for (unsigned kindBit = 1; kindBit <= profile.boundKinds; kindBit <<= 1) {
            if ((profile.boundKinds & kindBit) != 0) {
                if ((denseMask & denseBit) != 0) {
                    mask |= kindBit;
                }

                denseBit <<= 1;
            }
        }

        plan.work.visits = 16;
        plan.projectionWork.bytes = multiply(CALLBACK_RECORDS, sizeof(client::SubscriptionProjectionGroup));
        // One bounded stack copy is made before the existing receiver factory consumes its normal credits.
        plan.work.preparationWork = RECEIVING_BINDING_COPY_WORK;

        for (std::size_t index = 0; index < profile.bindings.size(); ++index) {
            const auto &binding = profile.bindings[index];

            if (!selectsEvent(static_cast<EventSelection>(mask), static_cast<EventType>(binding.kind)))
                continue;

            const auto mapping = binding.mappingLimits;
            const auto preparation =
                add(add(add(mapping.fields, mapping.lookups), add(mapping.selections, mapping.keys)),
                    add(add(mapping.typeAlternatives, mapping.nameUnits),
                        add(mapping.totalNameUnits, mapping.comparisonWork)));
            const auto fields = profile.schema.records()[static_cast<std::size_t>(binding.localId)].fields.size();
            const bool profileText = binding.kind == client::PreparedEventKind::PROFILE ||
                                     binding.kind == client::PreparedEventKind::OPTION_SALE;
            const bool candle = binding.kind == client::PreparedEventKind::CANDLE;
            const bool timeAndSale = binding.kind == client::PreparedEventKind::TIME_AND_SALE ||
                                     binding.kind == client::PreparedEventKind::NUAM_TIME_AND_SALE;
            const bool text = profileText || binding.kind == client::PreparedEventKind::TEXT_MESSAGE ||
                              binding.kind == client::PreparedEventKind::TEXT_CONFIGURATION || timeAndSale ||
                              isOrderSourceEvent(static_cast<EventType>(binding.kind));
            const auto units = candle ? multiply(values.symbolUnits, events::detail::CANDLE_SYMBOL_TEXT_PASSES)
                               : text ? values.totalUnits
                                      : values.symbolUnits;
            const auto strings = timeAndSale   ? std::size_t{4}
                                 : profileText ? std::size_t{3}
                                               : (text ? std::size_t{2} : std::size_t{1});
            const auto bytes =
                add(add(sizeof(client::SubscriptionProjection), multiply(add(units, strings), sizeof(char16_t))),
                    binding.kind == client::PreparedEventKind::MESSAGE ||
                            binding.kind == client::PreparedEventKind::CONFIGURATION
                        ? binding.attachmentBytes
                    : candle ? events::detail::CANDLE_SYMBOL_FIXED_BYTES
                             : 0);
            plan.bindingIndices[plan.bindingCount++] = index;
            plan.limits.bindings = plan.bindingCount;
            plan.limits.shapeFields = add(plan.limits.shapeFields, fields);
            plan.limits.selections = add(plan.limits.selections, mapping.selections);
            plan.work.preparationWork = add(plan.work.preparationWork, preparation);

            if (candle) {
                plan.work.preparationWork = add(plan.work.preparationWork, preparation);
                plan.work.visits = add(plan.work.visits, mapping.lookups);
            }

            plan.work.visits = add(plan.work.visits, add(plan.bindingCount, 3));

            if (binding.kind == client::PreparedEventKind::SPREAD_ORDER ||
                binding.kind == client::PreparedEventKind::ANALYTIC_ORDER ||
                binding.kind == client::PreparedEventKind::OTC_MARKETS_ORDER ||
                binding.kind == client::PreparedEventKind::SERIES ||
                binding.kind == client::PreparedEventKind::OPTION_SALE) {
                plan.work.visits = add(plan.work.visits, mapping.lookups);
            }

            constexpr std::size_t DERIVED_DELEGATES = core::MAX_DELIVERY_PROJECTIONS - 1;
            const auto outputs = multiply(
                CALLBACK_RECORDS, binding.projection == events::EventProjection::NATIVE ? 1 : DERIVED_DELEGATES);
            plan.projectionWork.visits =
                add(plan.projectionWork.visits, multiply(outputs, add(fields, add(mapping.selections, 3))));
            plan.projectionWork.textUnits = add(plan.projectionWork.textUnits, multiply(outputs, units));
            plan.projectionWork.bytes = add(plan.projectionWork.bytes, multiply(outputs, bytes));
        }

        plan.projectionWork.visits = add(plan.projectionWork.visits, multiply(CALLBACK_RECORDS, plan.bindingCount));
    }
}
} // namespace

std::expected<EventRegistry, EventRegistryFailure> defaultReceivingRegistry(DeliveryContract contract) {
    if (contract != DeliveryContract::TICKER && contract != DeliveryContract::STREAM &&
        contract != DeliveryContract::HISTORY) {
        return std::unexpected(EventRegistryFailure{EventRegistryError::INVALID_INPUT, {}});
    }

    EventRegistryWork work;
    EventRegistryLimits limits;
    limits.schema = contract == DeliveryContract::STREAM
                        ? withNuamTimeAndSaleLimits(withNuamTradeLimits(withTimeAndSaleLimits(BASE_SCHEMA_LIMITS)))
                    : contract == DeliveryContract::HISTORY
                        ? withNuamTimeAndSaleLimits(withOptionSaleLimits(withTimeAndSaleLimits(CANDLE_SCHEMA_LIMITS)))
                        : withNuamTradeLimits(CANDLE_SCHEMA_LIMITS);
    // Both appended physical Candle routes fit in the existing maximum registration capacity.
    limits.registrations = 74;
    constexpr auto defaultMapping = EventRegistration{EventType::QUOTE, 0}.mappingLimits;
    constexpr auto preparationPerRoute = defaultMapping.fields + defaultMapping.lookups + defaultMapping.selections +
                                         defaultMapping.keys + defaultMapping.typeAlternatives +
                                         defaultMapping.nameUnits + defaultMapping.totalNameUnits +
                                         defaultMapping.comparisonWork;
    work.preparation = limits.registrations * preparationPerRoute;

    return contract == DeliveryContract::HISTORY  ? defaultHistoryEventRegistry(limits, work)
           : contract == DeliveryContract::TICKER ? defaultTickerEventRegistry(limits, work)
                                                  : defaultStreamEventRegistry(limits, work);
}

bool receivingRouteSupportsContract(const EventRegistration &route, DeliveryContract contract) noexcept {
    if (route.projection == events::EventProjection::QUOTE_ORDERS) {
        return contract == DeliveryContract::TICKER;
    }

    if (route.projection == events::EventProjection::MARKET_MAKER_ORDERS || isOrderSourceEvent(route.type) ||
        (route.type == EventType::MARKET_MAKER || route.type == EventType::SERIES ||
         route.type == EventType::OPTION_SALE)) {
        return contract == DeliveryContract::HISTORY;
    }

    if (route.type == EventType::ORDER_IMBALANCE) {
        return contract == DeliveryContract::TICKER;
    }

    if (isOptionTimeSeriesEvent(route.type) || route.type == EventType::CANDLE) {
        return contract == DeliveryContract::TICKER || contract == DeliveryContract::HISTORY;
    }

    if (route.type == EventType::TIME_AND_SALE || route.type == EventType::NUAM_TIME_AND_SALE) {
        return contract == DeliveryContract::STREAM || contract == DeliveryContract::HISTORY;
    }

    if (contract == DeliveryContract::HISTORY) {
        return false;
    }

    if (route.type == EventType::MESSAGE || route.type == EventType::TEXT_MESSAGE) {
        return contract == DeliveryContract::STREAM;
    }

    return contract == DeliveryContract::TICKER || contract == DeliveryContract::STREAM;
}

std::optional<events::PreparedCandleRoutes> prepareReceivingCandleRoutes(const EventRegistry &registry,
                                                                         const core::LocalSchema &schema,
                                                                         DeliveryContract contract,
                                                                         std::size_t symbolUnits) {
    std::array<std::int32_t, MAXIMUM_RECEIVING_RECORDS> ids{};
    std::size_t count = 0;
    auto mapping = EventRegistration{EventType::CANDLE, 0}.mappingLimits;

    for (const auto &route : registry.registrations()) {
        if (route.type != EventType::CANDLE || !receivingRouteSupportsContract(route, contract))
            continue;

        if (count == ids.size())
            refuse(ErrorCode::RESOURCE_LIMIT, "profile-candle-records");

        if (contract == DeliveryContract::HISTORY &&
            !schema.records()[static_cast<std::size_t>(route.localRecordId)].hasTime)
            refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-candle-time");

        ids[count++] = route.localRecordId;
        const auto &limit = route.mappingLimits;
        mapping.fields = std::max(mapping.fields, limit.fields);
        mapping.lookups = std::max(mapping.lookups, limit.lookups);
        mapping.selections = std::max(mapping.selections, limit.selections);
        mapping.keys = std::max(mapping.keys, limit.keys);
        mapping.typeAlternatives = std::max(mapping.typeAlternatives, limit.typeAlternatives);
        mapping.nameUnits = std::max(mapping.nameUnits, limit.nameUnits);
        mapping.totalNameUnits = std::max(mapping.totalNameUnits, limit.totalNameUnits);
        mapping.comparisonWork = std::max(mapping.comparisonWork, limit.comparisonWork);
    }

    if (count == 0)
        return std::nullopt;

    auto prepared = events::PreparedCandleRoutes::prepare(
        schema, std::span(ids).first(count), {count, CANDLE_SCHEMA_LIMITS.nameUnits, symbolUnits, mapping});

    if (!prepared) {
        using RouteError = events::CandleRouteError;
        const auto &failure = prepared.error();
        refuse(failure.error == RouteError::ALLOCATION_FAILURE ||
                       failure.mappingError == events::MappingError::ALLOCATION_FAILURE
                   ? ErrorCode::ALLOCATION_FAILURE
               : failure.error == RouteError::RESOURCE_LIMIT ||
                       failure.mappingError == events::MappingError::RESOURCE_LIMIT
                   ? ErrorCode::RESOURCE_LIMIT
                   : ErrorCode::UNSUPPORTED_PROFILE,
               "profile-candle-routes");
    }

    return std::move(*prepared);
}

std::optional<events::PreparedCandleRoutes> prepareReceivingCandleRoutes(const EndpointOptions &options,
                                                                         std::size_t symbolUnits) {
    const auto registry = selectRegistry(options);

    return prepareReceivingCandleRoutes(registry, registry.schema(), options.contract, symbolUnits);
}

events::CandlePhysicalRoute resolveReceivingCandle(const events::PreparedCandleRoutes &routes,
                                                   std::u16string_view symbol) {
    try {
        auto route = routes.resolve(events::CandleSymbol::valueOf(symbol));

        if (route)
            return std::move(*route);

        using RouteError = events::CandleRouteError;

        switch (route.error().error) {
        case RouteError::ALLOCATION_FAILURE:
            refuse(ErrorCode::ALLOCATION_FAILURE, "subscription-candle-route");
        case RouteError::RESOURCE_LIMIT:
            refuse(ErrorCode::RESOURCE_LIMIT, "subscription-candle-route");
        case RouteError::INVALID_INPUT:
            refuse(ErrorCode::INVALID_ARGUMENT, "subscription-candle-route");
        default:
            refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-candle-route");
        }
    } catch (const std::invalid_argument &) {
        refuse(ErrorCode::INVALID_ARGUMENT, "subscription-candle-route");
    } catch (const std::out_of_range &) {
        refuse(ErrorCode::INVALID_ARGUMENT, "subscription-candle-route");
    } catch (const std::length_error &) {
        refuse(ErrorCode::RESOURCE_LIMIT, "subscription-candle-route");
    } catch (const std::bad_alloc &) {
        refuse(ErrorCode::ALLOCATION_FAILURE, "subscription-candle-route");
    }
}

ReceivingProfile::ReceivingProfile(core::LocalSchema schema, ReceivingSettings endpoint)
    : schema(std::move(schema)), endpoint(std::move(endpoint)) {
}

ReceivingProfile prepareReceivingProfile(const RuntimeOptions &runtime, const EndpointOptions &options,
                                         const std::string &host, std::uint16_t port, bool tape) {
    auto selected = selectRegistry(options);
    const auto hasCandle = std::ranges::any_of(selected.registrations(), [&](const EventRegistration &route) {
        return route.type == EventType::CANDLE && receivingRouteSupportsContract(route, options.contract);
    });
    const auto hasTimeAndSale = std::ranges::any_of(selected.registrations(), [&](const EventRegistration &route) {
        return route.type == EventType::TIME_AND_SALE && receivingRouteSupportsContract(route, options.contract);
    });
    const auto hasOptionSale = std::ranges::any_of(selected.registrations(), [&](const EventRegistration &route) {
        return route.type == EventType::OPTION_SALE && receivingRouteSupportsContract(route, options.contract);
    });
    const auto hasNuamTrade = std::ranges::any_of(selected.registrations(), [&](const EventRegistration &route) {
        return route.type == EventType::NUAM_TRADE && receivingRouteSupportsContract(route, options.contract);
    });
    const auto nuamTimeAndSaleRoutes =
        std::ranges::count_if(selected.registrations(), [&](const EventRegistration &route) {
            return route.type == EventType::NUAM_TIME_AND_SALE &&
                   receivingRouteSupportsContract(route, options.contract);
        });
    auto schemaLimits = hasCandle ? CANDLE_SCHEMA_LIMITS : BASE_SCHEMA_LIMITS;

    if (nuamTimeAndSaleRoutes > 0) {
        for (const auto &route : selected.registrations()) {
            if (route.type != EventType::NUAM_TIME_AND_SALE || !receivingRouteSupportsContract(route, options.contract))
                continue;

            const auto &record = selected.schema().records()[static_cast<std::size_t>(route.localRecordId)];
            schemaLimits.records = std::min(MAXIMUM_RECEIVING_RECORDS, add(schemaLimits.records, 1));
            schemaLimits.fields = add(schemaLimits.fields, record.fields.size());
            schemaLimits.lookups = add(schemaLimits.lookups, record.lookups.size());
        }
    }

    if (hasNuamTrade) {
        for (const auto &route : selected.registrations()) {
            if (route.type != EventType::NUAM_TRADE || !receivingRouteSupportsContract(route, options.contract)) {
                continue;
            }

            const auto &record = selected.schema().records()[static_cast<std::size_t>(route.localRecordId)];
            schemaLimits.records = std::min(MAXIMUM_RECEIVING_RECORDS, add(schemaLimits.records, 1));
            schemaLimits.fields = add(schemaLimits.fields, record.fields.size());
            schemaLimits.lookups = add(schemaLimits.lookups, record.lookups.size());
        }
    }

    if (hasOptionSale) {
        schemaLimits = withOptionSaleLimits(schemaLimits);
    }

    if (hasTimeAndSale) {
        schemaLimits = withTimeAndSaleLimits(schemaLimits);
    }

    if (selected.schema().records().size() > MAXIMUM_RECEIVING_RECORDS) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-schema");
    }

    auto copied = selected.schema().boundedCopy(schemaLimits);

    if (!copied) {
        refuse(copied.error() == core::SchemaError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                       : ErrorCode::UNSUPPORTED_PROFILE,
               "profile-schema");
    }

    auto schema = std::move(*copied);
    std::array<std::size_t, RECEIVING_EVENT_KINDS> routes{};
    std::size_t maximumFields = 16;

    for (const auto &registration : selected.registrations()) {
        const auto kind = static_cast<std::size_t>(registration.type);
        routes[kind] = add(routes[kind], 1);
    }

    for (const auto &record : schema.records()) {
        maximumFields = std::max(maximumFields, record.fields.size());
    }

    // A mixed subscription may be created after connect; reserve the complete immutable registered selection.
    std::size_t routeCount = 0;

    for (const auto count : routes) {
        routeCount = add(routeCount, count);
    }

    if (routeCount > MAXIMUM_RECEIVING_BINDINGS) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-route-count");
    }

    const auto maximumRoutes = std::max(std::size_t{1}, receivingCoordinateCapacity(selected, options.contract));
    auto configured = client::controlledStreamSettings();

    if (!configured) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-settings");
    }

    auto settings = std::move(*configured);
    settings.host = host;
    settings.port = port;
    settings.timeouts = {options.dnsTimeout, options.connectTimeout, options.readTimeout, options.writeTimeout,
                         options.pollInterval};
    settings.retry = {options.maximumAttempts,
                      options.maximumAttempts == 1 ? std::chrono::milliseconds{0} : options.retryDelay};
    const auto clients = runtime.maximumSubscriptionsPerEndpoint;
    const auto subscriptionEntries = multiply(runtime.maximumSymbolsPerSubscription, maximumRoutes);
    const auto entries = multiply(clients, subscriptionEntries);
    const auto symbolUnits = multiply(entries, runtime.maximumSymbolUnits);
    const auto scale = multiply(clients, add(subscriptionEntries, 15) / 16);
    auto &budgets = settings.budgets;
    budgets.schema = schemaLimits;
    budgets.layout = {schemaLimits, schemaLimits.records, schemaLimits.fields, schemaLimits.nameUnits,
                      schemaLimits.totalNameUnits};
    budgets.session.context.schema = schemaLimits;
    budgets.session.context.descriptions = schemaLimits.records;
    budgets.session.snapshotBytes = SESSION_SNAPSHOT_BYTES;
    budgets.session.descriptions.descriptions = schemaLimits.records;
    budgets.session.descriptions.totalFields = schemaLimits.fields;

    if (hasTimeAndSale) {
        // A peer may describe nano-part, Buyer and Seller, plus both FIELD metadata values.
        constexpr std::size_t EXTRA_WIRE_FIELDS = 5;
        // Regional name and all 15 field names, using the longer declared spelling of each key.
        constexpr std::size_t MAXIMUM_WIRE_NAME_UNITS = 143;
        auto &descriptions = budgets.session.descriptions;
        auto &bindings = budgets.session.context.bindings;
        descriptions.totalFields = add(descriptions.totalFields, TIME_AND_SALE_DEFAULT_RECORDS * EXTRA_WIRE_FIELDS);
        descriptions.totalNameUnits =
            add(descriptions.totalNameUnits, TIME_AND_SALE_DEFAULT_RECORDS * MAXIMUM_WIRE_NAME_UNITS);
        bindings.fields = std::max(bindings.fields, descriptions.totalFields);
        bindings.totalNameUnits = std::max(bindings.totalNameUnits, descriptions.totalNameUnits);
    }

    if (nuamTimeAndSaleRoutes > 0) {
        auto &descriptions = budgets.session.descriptions;
        auto &bindings = budgets.session.context.bindings;
        constexpr std::size_t EXTRA_WIRE_FIELDS = 5;
        constexpr std::size_t EXTRA_WIRE_NAME_UNITS = 160;
        const auto routeCount = static_cast<std::size_t>(nuamTimeAndSaleRoutes);
        descriptions.totalFields = add(descriptions.totalFields, multiply(routeCount, EXTRA_WIRE_FIELDS));
        descriptions.totalNameUnits = add(descriptions.totalNameUnits, multiply(routeCount, EXTRA_WIRE_NAME_UNITS));
        bindings.fields = std::max(bindings.fields, descriptions.totalFields);
        bindings.totalNameUnits = std::max(bindings.totalNameUnits, descriptions.totalNameUnits);
    }

    budgets.session.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
    budgets.session.historySnapshot = options.contract == DeliveryContract::HISTORY;
    budgets.session.fieldTime = protocol::FieldTimePolicy::READ;
    budgets.session.data.processedRecords = budgets.session.data.emittedRecords = FRAME_RECORDS;
    budgets.session.data.wireFields = budgets.session.data.emittedValues = multiply(FRAME_RECORDS, maximumFields);
    budgets.session.data.symbol.codeUnits = budgets.session.data.symbol.stateUnits = runtime.maximumSymbolUnits;

    if (hasOptionSale) {
        constexpr std::size_t EXTRA_WIRE_FIELDS = 3;
        constexpr std::size_t EXTRA_WIRE_NAME_UNITS = 128;
        auto &descriptions = budgets.session.descriptions;
        auto &bindings = budgets.session.context.bindings;
        descriptions.totalFields = add(descriptions.totalFields, EXTRA_WIRE_FIELDS);
        descriptions.totalNameUnits = add(descriptions.totalNameUnits, EXTRA_WIRE_NAME_UNITS);
        bindings.fields = std::max(bindings.fields, descriptions.totalFields);
        bindings.totalNameUnits = std::max(bindings.totalNameUnits, descriptions.totalNameUnits);
    }

    budgets.session.data.ownedFieldUnits = FIELD_TEXT_UNITS;
    budgets.session.data.ownedPayloadBytes = FIELD_BYTE_UNITS;
    budgets.desired = {schemaLimits.records,     entries, runtime.maximumSymbolUnits, symbolUnits, multiply(2, entries),
                       multiply(2, symbolUnits), entries};
    budgets.writer.operationEntries = std::max(budgets.writer.operationEntries, entries);

    if (!tape) {
        // A complete named-route preparation and progress allowance is independent of queue reservations.
        const auto dns = transport::TcpRuntime::dnsPreparationWorkRequirement(runtime.dnsServers.size());

        if (!dns) {
            refuse(ErrorCode::DNS_CONFIGURATION, "profile-dns");
        }

        budgets.tcp.residentBytes = add(budgets.tcp.residentBytes, DNS_ENVELOPE_BYTES);
        budgets.protocolTurn.operations = add(budgets.protocolTurn.operations, dns->operations);
        budgets.protocolTurn.visits = add(budgets.protocolTurn.visits, dns->visits);
        budgets.protocolTurn.textUnits = add(budgets.protocolTurn.textUnits, dns->textUnits);
        budgets.protocolTurn.bytes = add(budgets.protocolTurn.bytes, dns->bytes);
        budgets.runtimeTurn.dnsCalls = budgets.runtimeTurn.dnsPackets = 10000;
        budgets.runtimeTurn.dnsBytes = 10000000;
        budgets.runtimeTurn.dnsUnits = DNS_PROGRESS_UNITS;
    }

    fundContextBindingText(settings, schema.records().size());
    std::optional<ReceivingSettings> endpoint;

    if (tape) {
        endpoint.emplace(settings);
    } else {
        auto validated = client::EndpointOptions::create(schema, settings);

        if (!validated) {
            refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-endpoint");
        }

        endpoint.emplace(std::move(*validated));
    }

    ReceivingProfile result(std::move(schema), std::move(*endpoint));
    result.physicalRouteCount = maximumRoutes;
    result.unitaryOrderSources = options.unitaryOrderSources;

    switch (options.contract) {
    case DeliveryContract::TICKER:
        result.family = core::SubscriptionFamily::TICKER;
        break;
    case DeliveryContract::STREAM:
        result.family = core::SubscriptionFamily::STREAM;
        break;
    case DeliveryContract::HISTORY:
        result.family = core::SubscriptionFamily::HISTORY;
        break;
    default:
        refuse(ErrorCode::INVALID_ARGUMENT, "profile-contract");
    }

    result.recoveryOptions.selectors = options.enableWildcard
                                           ? core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1
                                           : core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1;
    result.recoveryOptions.dns.serverCount = runtime.dnsServers.size();
    result.recoveryOptions.dns.port = runtime.dnsPort;
    std::ranges::copy(runtime.dnsServers, result.recoveryOptions.dns.servers.begin());
    result.interests = {schemaLimits.records,       clients,     entries, entries,
                        runtime.maximumSymbolUnits, symbolUnits, entries, multiply(scale, 4 * MEBIBYTE)};
    result.clientLimits = {options.pendingRecords,
                           RECORD_BYTES,
                           8,
                           BATCH_BYTES,
                           options.batchRecords,
                           schemaLimits.fields,
                           runtime.maximumSymbolUnits,
                           FIELD_TEXT_UNITS,
                           FIELD_BYTE_UNITS,
                           RECORD_ARENA_BYTES,
                           4,
                           8,
                           subscriptionEntries,
                           multiply(subscriptionEntries, add(runtime.maximumSymbolUnits, 1))};
    result.queueLimits = {clients,
                          FRAME_RECORDS,
                          16,
                          options.batchRecords,
                          16,
                          multiply(clients, options.pendingRecords),
                          multiply(clients, RECORD_BYTES),
                          multiply(clients, 8),
                          multiply(clients, BATCH_BYTES),
                          multiply(clients, 4),
                          multiply(clients, 8),
                          multiply(clients, 2),
                          multiply(scale, 64 * MEBIBYTE)};
    // One retained frame per exact cached key, plus an incoming candidate, covers fragmented updates.
    // Twice the event arena also covers its fixed owner/control backing in the qualified STL profiles.
    const auto tickerFrames = add(entries, 1);
    const auto tickerBytes = multiply(tickerFrames, multiply(2, budgets.session.eventBytes));
    result.tickerLimits = {
        clients, entries, tickerFrames, tickerBytes, add(tickerBytes, multiply(scale, 8 * MEBIBYTE)), budgets.desired};
    result.historyLimits = {entries,
                            options.history.maximumPoints,
                            runtime.maximumSymbolUnits,
                            options.history.maximumFrames,
                            options.history.maximumBytes,
                            clients,
                            options.history.pendingUpdatesPerSubscription,
                            options.history.maximumFrames,
                            options.history.pendingBytesPerSubscription,
                            options.history.maximumResidentBytes,
                            budgets.desired};

    if (result.family == core::SubscriptionFamily::HISTORY) {
        const auto retainedOwners =
            add(options.history.maximumBytes, multiply(clients, options.history.pendingBytesPerSubscription));

        if (retainedOwners > options.history.maximumResidentBytes) {
            refuse(ErrorCode::RESOURCE_LIMIT, "profile-history-resident-capacity");
        }

        static_cast<void>(add(options.pendingRecords,
                              add(options.history.maximumPoints, options.history.pendingUpdatesPerSubscription)));
    }

    if (!tape) {
        result.blueprint = prepareBlueprint(result.schema);
        prepareCopies(result);
    }

    prepareMappings(result, selected, runtime.maximumSymbolUnits);
    auto &routing = result.routing;
    routing.symbols = runtime.maximumSymbolsPerSubscription;
    const auto logicalUnits = multiply(routing.symbols, runtime.maximumSymbolUnits);
    constexpr std::size_t FRONTEND_SELECTOR_BANKS = 4;
    routing.frontendSelectorBytes =
        multiply(FRONTEND_SELECTOR_BANKS, add(multiply(routing.symbols, sizeof(SubscriptionSelector)),
                                              multiply(add(logicalUnits, routing.symbols), sizeof(char16_t))));
    const auto candlePasses = result.candleRoutes ? events::detail::CANDLE_SYMBOL_TEXT_PASSES : 0;
    routing.temporaryTextUnits = multiply(add(2, candlePasses), logicalUnits);
    // The port's retained logical set and the queue's owned command copies have their own existing bounds.
    const auto textBacking = multiply(add(routing.temporaryTextUnits, multiply(2, routing.symbols)), sizeof(char16_t));
    routing.temporaryBytes =
        add(add(multiply(routing.symbols, add(sizeof(ReceivingSymbol),
                                              result.candleRoutes ? events::detail::CANDLE_SYMBOL_FIXED_BYTES : 0)),
                multiply(subscriptionEntries, sizeof(core::SubscriptionRequest))),
            textBacking);
    routing.retainedMetadataBytes =
        add(add(add(add(sizeof(result.bindingExchanges), sizeof(result.bindingSources)),
                    sizeof(result.bindingTimeEncodings)),
                add(sizeof(result.versionBindings), sizeof(result.snapshotRetransmitRecords))),
            add(multiply(result.eventPlans.capacity(), sizeof(ReceivingEventPlan)),
                sizeof(std::array<client::PreparedSubscriptionBinding, MAXIMUM_RECEIVING_BINDINGS>)));

    if (result.candleRoutes) {
        // One retained descriptor per selected record at most, including its provenance.
        constexpr auto descriptorBytes = sizeof(events::CandlePhysicalRoute) +
                                         sizeof(std::optional<events::CandlePeriod>) +
                                         sizeof(std::optional<events::CandlePrice>);
        routing.retainedMetadataBytes =
            add(routing.retainedMetadataBytes, multiply(result.schema.records().size(), descriptorBytes));
    }

    // Snapshot copy, suffix/representability scans, physical conversion and conservative retirement.
    constexpr std::size_t ROUTING_TEXT_PASSES = 11;
    const auto routeScans = multiply(multiply(routing.symbols, selected.registrations().size()), subscriptionEntries);
    routing.work = {0,
                    add(routeScans, add(multiply(routing.symbols, add(multiply(2, maximumRoutes), 8)), maximumRoutes)),
                    multiply(add(ROUTING_TEXT_PASSES, candlePasses), logicalUnits),
                    add(multiply(2, routing.temporaryBytes), routing.retainedMetadataBytes)};
    // Every routed candidate may compare its key and complete text with every previous candidate.
    const auto comparisons = multiply(subscriptionEntries, subscriptionEntries);
    routing.work.visits = add(routing.work.visits, comparisons);
    routing.work.textUnits =
        add(routing.work.textUnits, multiply(multiply(2, runtime.maximumSymbolUnits), comparisons));
    result.queueWork =
        queueWork(scale, result.interests, result.schema.records().size(), result.queueLimits.routeRecords);

    if (result.family == core::SubscriptionFamily::HISTORY) {
        fundHistoryWork(result, maximumFields);
        const auto rowBytes = add(sizeof(mux::QueueRangeObservation),
                                  add(64, multiply(add(runtime.maximumSymbolUnits, 1), 4 * sizeof(char16_t))));
        const auto publicationBytes =
            multiply(2, add(sizeof(HistorySubscriptionSnapshot), multiply(subscriptionEntries, rowBytes)));

        if (publicationBytes > options.history.maximumObservationBytes) {
            refuse(ErrorCode::RESOURCE_LIMIT, "profile-history-observation-capacity");
        }

        result.historyObservationWork = result.queueWork;
    }

    result.submitWork = {16, add(multiply(scale, CONTROL_VISITS), multiply(clients, routing.work.visits)),
                         add(multiply(scale, CONTROL_VISITS), multiply(clients, routing.work.textUnits)),
                         add(multiply(scale, CONTROL_BYTES), multiply(clients, routing.work.bytes))};
    const transport::RecoveryWork recovery{10000, multiply(scale, CONTROL_VISITS), multiply(scale, CONTROL_VISITS),
                                           multiply(scale, CONTROL_BYTES)};
    const protocol::SubscriptionWriterWork writer{10000, multiply(scale, CONTROL_VISITS),
                                                  multiply(scale, CONTROL_VISITS), multiply(scale, CONTROL_BYTES)};
    result.preparationWork = {recovery, writer, {result.queueWork, result.queueWork, result.queueWork}};
    const auto &native = result.endpoint.settings().budgets.runtimeTurn;
    result.turnWork = {recovery,
                       writer,
                       {native.steps, native.handlers, native.socketBytes, native.dnsCalls, native.dnsPackets,
                        native.dnsBytes, native.dnsUnits},
                       result.endpoint.settings().budgets.protocolTurn,
                       {result.queueWork, result.queueWork, result.queueWork}};

    return result;
}

const ReceivingEventPlan &receivingEventPlan(const ReceivingProfile &profile, EventSelection selection) {
    const auto mask = static_cast<std::size_t>(selection);

    if (!mask || (mask & ~static_cast<std::size_t>(EventSelection::ALL))) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-event-type");
    }

    std::size_t denseMask = 0;
    std::size_t denseBit = 1;

    for (unsigned kindBit = 1; kindBit <= profile.boundKinds; kindBit <<= 1) {
        if ((profile.boundKinds & kindBit) != 0) {
            if ((mask & kindBit) != 0) {
                denseMask |= denseBit;
            }

            denseBit <<= 1;
        }
    }

    return profile.eventPlans[denseMask];
}

bool copyReceivingBindings(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                           std::span<client::PreparedSubscriptionBinding> output, client::ReceiverWork &work) noexcept {
    const auto visits = plan.bindingCount <= MAXIMUM_RECEIVING_BINDINGS ? 2 * plan.bindingCount : 0;

    if (output.size() != MAXIMUM_RECEIVING_BINDINGS || plan.bindingCount > MAXIMUM_RECEIVING_BINDINGS ||
        work.visits < visits || work.preparationWork < RECEIVING_BINDING_COPY_WORK)
        return false;

    for (std::size_t index = 0; index < plan.bindingCount; ++index) {
        if (plan.bindingIndices[index] >= profile.bindings.size())
            return false;
    }

    work.visits -= visits;
    work.preparationWork -= RECEIVING_BINDING_COPY_WORK;

    for (std::size_t index = 0; index < plan.bindingCount; ++index)
        output[index] = profile.bindings[plan.bindingIndices[index]];

    return true;
}

bool prepayReceivingRouting(const ReceivingProfile &profile, mux::QueueSubmitWork &work) noexcept {
    const auto &charge = profile.routing.work;

    if (charge.visits > work.visits || charge.textUnits > work.textUnits || charge.bytes > work.bytes) {
        return false;
    }

    work.visits -= charge.visits;
    work.textUnits -= charge.textUnits;
    work.bytes -= charge.bytes;

    return true;
}

bool regionalSymbolRepresentable(std::u16string_view symbol) noexcept {
    const auto attributes = events::market_symbols::hasAttributes(symbol);
    // hasAttributes establishes the recognized final opener; locate its position without owning text.
    const auto end = attributes ? symbol.rfind(u'{', symbol.size() - 2) : symbol.size();

    if (end < 2 || symbol[end - 2] != u'&') {
        return true;
    }

    const auto base = symbol.substr(0, end - 2);
    const auto repeatedSuffix = base.size() >= 2 && base[base.size() - 2] == u'&';

    return !repeatedSuffix && (attributes || !events::market_symbols::hasAttributes(base));
}

void validateReceivingSymbol(const EventRegistry *registry, EventSelection selection, std::u16string_view symbol) {
    if (!registry || selection == EventSelection::PROFILE || regionalSymbolRepresentable(symbol)) {
        return;
    }

    if (registry->schema().records().size() > MAXIMUM_RECEIVING_RECORDS) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-schema");
    }

    const auto exchange = events::market_symbols::getExchangeCode(symbol);

    for (const auto &route : registry->registrations()) {
        if (route.type == EventType::PROFILE || route.type == EventType::TEXT_MESSAGE ||
            route.type == EventType::TEXT_CONFIGURATION || route.type == EventType::MESSAGE ||
            route.type == EventType::CONFIGURATION || route.type == EventType::SERIES ||
            isOptionTimeSeriesEvent(route.type) || route.type == EventType::CANDLE ||
            route.type == EventType::OPTION_SALE || usesOrderSource(route.type) ||
            !selectsEvent(selection, route.type)) {
            continue;
        }

        const auto &record = registry->schema().records()[static_cast<std::size_t>(route.localRecordId)];

        if (record.name.size() > CANDLE_SCHEMA_LIMITS.nameUnits) {
            refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-schema");
        }

        if (exchange && events::market_symbols::getExchangeCode(record.name) == exchange) {
            refuse(ErrorCode::INVALID_ARGUMENT, "subscription-symbol-representation");
        }
    }
}

void prepareReceivingSymbols(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                             std::span<ReceivingSymbol> symbols) {
    const auto candleSelected =
        profile.candleRoutes &&
        std::ranges::any_of(std::span(plan.bindingIndices).first(plan.bindingCount), [&](std::size_t index) {
            return profile.bindings[index].kind == client::PreparedEventKind::CANDLE;
        });

    for (auto &symbol : symbols) {
        symbol.regional = {};
        symbol.exchange = 0;
        // Retire previous physical backing before either Exact conversion or an unused-text All key.
        std::u16string{}.swap(symbol.physical);
        symbol.candle.reset();

        if (symbol.selector == core::SubscriptionSelectorKind::ALL) {
            continue;
        }

        if (candleSelected)
            symbol.candle = resolveReceivingCandle(*profile.candleRoutes, symbol.logical);

        symbol.exchange = events::market_symbols::getExchangeCode(symbol.logical);

        for (std::size_t offset = 0; offset < plan.bindingCount; ++offset) {
            const auto index = plan.bindingIndices[offset];
            const auto exchange = profile.bindingExchanges[index];

            if (exchange && exchange == symbol.exchange &&
                profile.bindings[index].projection == events::EventProjection::NATIVE) {
                const auto kind = static_cast<std::size_t>(profile.bindings[index].kind);
                symbol.regional[kind] = true;
            }
        }

        if (std::ranges::any_of(symbol.regional, [](bool value) {
                return value;
            })) {
            if (!regionalSymbolRepresentable(symbol.logical)) {
                refuse(ErrorCode::INVALID_ARGUMENT, "subscription-symbol-representation");
            }

            symbol.physical = *events::market_symbols::changeExchangeCode(symbol.logical, 0);
        }
    }
}

core::DeliveryProjectionMask receivingProjectionMask(const ReceivingProfile &profile, std::size_t bindingIndex,
                                                     const ReceivingSymbol &symbol) noexcept {
    const auto &binding = profile.bindings[bindingIndex];
    const auto exchange = profile.bindingExchanges[bindingIndex];

    if (binding.kind == client::PreparedEventKind::CANDLE) {
        return symbol.candle && symbol.candle->recordId == binding.localId &&
                       (!symbol.sourceId || *symbol.sourceId <= 0)
                   ? core::NATIVE_DELIVERY_PROJECTION
                   : 0;
    }

    if (binding.projection != events::EventProjection::NATIVE) {
        const bool maker = binding.projection == events::EventProjection::MARKET_MAKER_ORDERS;
        const auto sources = events::detail::derivedOrderSourceIds(
            maker ? events::DerivedOrderKind::MARKET_MAKER : events::DerivedOrderKind::QUOTE, exchange);

        // The SDK default matches the production non-unitary configuration: both separate sides.
        if (!symbol.sourceId || *symbol.sourceId < 0) {
            return profile.unitaryOrderSources ? events::detail::DERIVED_UNITARY_PAIR
                                               : events::detail::DERIVED_SEPARATE_PAIR;
        }

        if (*symbol.sourceId == sources.bid) {
            return events::detail::DERIVED_BID;
        }

        if (*symbol.sourceId == sources.ask) {
            return events::detail::DERIVED_ASK;
        }

        if (*symbol.sourceId == sources.unitary) {
            return events::detail::DERIVED_UNITARY_PAIR;
        }

        return 0;
    }

    if (usesOrderSource(static_cast<EventType>(binding.kind))) {
        const auto source = profile.bindingSources[bindingIndex];

        return source && (!symbol.sourceId || *symbol.sourceId < 0 || *symbol.sourceId == *source)
                   ? core::NATIVE_DELIVERY_PROJECTION
                   : 0;
    }

    if (binding.kind == client::PreparedEventKind::MARKET_MAKER || binding.kind == client::PreparedEventKind::SERIES ||
        isOptionTimeSeriesEvent(static_cast<EventType>(binding.kind)) ||
        binding.kind == client::PreparedEventKind::OPTION_SALE) {
        return !symbol.sourceId || *symbol.sourceId <= 0 ? core::NATIVE_DELIVERY_PROJECTION : 0;
    }

    if (binding.kind == client::PreparedEventKind::NUAM_TIME_AND_SALE && symbol.sourceId && *symbol.sourceId > 0)
        return 0;

    const auto kind = static_cast<std::size_t>(binding.kind);

    return symbol.selector == core::SubscriptionSelectorKind::ALL ||
                   (exchange ? exchange == symbol.exchange : !symbol.regional[kind])
               ? core::NATIVE_DELIVERY_PROJECTION
               : 0;
}

bool receivingSymbolMatches(const ReceivingProfile &profile, std::size_t bindingIndex,
                            const ReceivingSymbol &symbol) noexcept {
    return receivingProjectionMask(profile, bindingIndex, symbol) != 0;
}

const std::u16string &receivingPhysicalSymbol(const ReceivingProfile &profile, std::size_t bindingIndex,
                                              const ReceivingSymbol &symbol) noexcept {
    if (profile.bindings[bindingIndex].kind == client::PreparedEventKind::CANDLE && symbol.candle)
        return symbol.candle->symbol;

    return profile.bindingExchanges[bindingIndex] &&
                   profile.bindings[bindingIndex].projection == events::EventProjection::NATIVE
               ? symbol.physical
               : symbol.logical;
}

void appendReceivingDesiredRequest(std::vector<core::SubscriptionRequest> &requests, core::RecordContext context,
                                   std::u16string_view symbol, core::SubscriptionSelectorKind selector,
                                   core::SubscriptionFamily family, std::optional<std::int64_t> fromTime,
                                   HistoryRangeEncoding encoding, core::DeliveryProjectionMask projections) {
    std::optional<std::int64_t> bound;

    if (family == core::SubscriptionFamily::HISTORY) {
        bound = 0;

        if (fromTime) {
            if (encoding != HistoryRangeEncoding::UNIX_MILLISECONDS_SEQUENCE)
                refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-mapper");

            bound = *fromTime == std::numeric_limits<std::int64_t>::max() ? *fromTime
                                                                          : events::withTimeSequenceTime(0, *fromTime);
        }
    } else if (fromTime) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-contract");
    }

    const auto found = std::ranges::find_if(requests, [&](const auto &request) {
        return request.key.context.matches(context) && request.key.selector == selector && request.key.symbol == symbol;
    });

    if (found == requests.end()) {
        requests.push_back({{context, symbol, selector}, bound, projections});

        return;
    }

    if (bound && *bound < *found->historyBound)
        found->historyBound = bound;

    found->projectionMask |= projections;
}

std::expected<mux::MuxQueue, mux::QueueFailure> createReceivingQueue(const ReceivingProfile &profile,
                                                                     mux::QueueWork &work) {
    if (profile.family == core::SubscriptionFamily::TICKER) {
        return mux::MuxQueue::createTicker(profile.schema, profile.interests, profile.tickerLimits,
                                           {false, 100, profile.recoveryOptions.selectors,
                                            std::span(profile.versionBindings).first(profile.versionBindingCount)},
                                           profile.queueLimits, work);
    }

    if (profile.family == core::SubscriptionFamily::HISTORY) {
        return mux::MuxQueue::createHistory(
            profile.schema, profile.interests, profile.historyLimits,
            {false, 100, 10000, profile.recoveryOptions.selectors,
             std::span(profile.snapshotRetransmitRecords).first(profile.snapshotRetransmitCount)},
            profile.queueLimits, work);
    }

    return mux::MuxQueue::createStream(profile.schema, profile.interests, profile.endpoint.settings().budgets.desired,
                                       profile.endpoint.settings().budgets.collector, profile.queueLimits, work,
                                       profile.recoveryOptions.selectors);
}
} // namespace org::ttldtor::qdcxx::api::detail
