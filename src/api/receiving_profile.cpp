// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "receiving_profile.hpp"

#include "qd/events/market_event_symbols.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::size_t MEBIBYTE = 1024 * 1024; ///< Independent logical byte unit.
constexpr core::SchemaLimits SCHEMA_LIMITS{MAXIMUM_RECEIVING_RECORDS, 128, 256, 128, 16384};
///< Complete declarations and aliases; regional binding does not expand this ceiling.
constexpr std::size_t FRAME_RECORDS = 64;                 ///< Whole admitted DATA frame record ceiling.
constexpr std::size_t CALLBACK_RECORDS = 4;               ///< Whole callback projection ceiling.
constexpr std::size_t FIELD_TEXT_UNITS = 8192;            ///< Aggregate text copied from one complete record.
constexpr std::size_t FIELD_BYTE_UNITS = 16384;           ///< Aggregate opaque payload copied from one record.
constexpr std::size_t RECORD_ARENA_BYTES = 65536;         ///< Complete detached per-record arena request ceiling.
constexpr std::size_t QUEUED_RECORDS = 128;               ///< Independent detached-record slots per registration.
constexpr std::size_t RECORD_BYTES = 16 * MEBIBYTE;       ///< Per-registration detached backing ceiling.
constexpr std::size_t BATCH_BYTES = MEBIBYTE;             ///< Per-registration composite backing ceiling.
constexpr std::size_t CONTROL_OPERATIONS = 100000;        ///< Qualified finite one-client owner work seed.
constexpr std::size_t CONTROL_VISITS = 100000000;         ///< Qualified finite metadata/equality work seed.
constexpr std::size_t CONTROL_BYTES = 256 * MEBIBYTE;     ///< Qualified logical copy/disposal seed.
constexpr std::size_t DNS_ENVELOPE_BYTES = 80 * MEBIBYTE; ///< Separate documented DNS SDK envelope.
constexpr std::uint64_t DNS_PROGRESS_UNITS = 351280142190671838ULL; ///< Complete documented DNS logical charge.
constexpr std::size_t WRITER_CONFIGURATION_BYTES = 4 * MEBIBYTE;    ///< Retained complete writer reservation.
constexpr auto SELECTORS = core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1; ///< Exact ASCII selector domain.

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
 * @param registry Optional explicit registry, borrowed during selection.
 * @return Owning immutable snapshot or detached registry refusal.
 */
EventRegistry selectRegistry(const EventRegistry *registry) {
    if (registry) {
        if (!registry->valid()) {
            refuse(ErrorCode::INVALID_ARGUMENT, "profile-registry");
        }

        return *registry;
    }

    EventRegistryWork work;
    auto result = EventRegistry::builtIn({}, work);

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
    const auto family = static_cast<std::size_t>(profile.family == core::SubscriptionFamily::TICKER
                                                     ? transport::RecoveryFamily::TICKER
                                                     : transport::RecoveryFamily::STREAM);
    const auto &desired = profile.endpoint.settings().budgets.desired;
    // Recovery inspects the actual collector owner maxima, including each independent TICKER view.
    // The queue requires every view's desired scope to cover its aggregate interest reservation.
    const auto owners = profile.family == core::SubscriptionFamily::TICKER ? profile.tickerLimits.viewSlots : 1;
    limits.replay[family] = {multiply(owners, desired.entries), multiply(owners, desired.totalSymbolUnits)};
}

/** Prepare each existing mapping and exact whole-callback projection coordinates.
 * @param profile Complete preparation, modified before publication.
 * @param registry Immutable validated routes; complete schema identity is preserved by profile.schema.
 * @param symbolUnits Maximum owned exact symbol size.
 */
void prepareMappings(ReceivingProfile &profile, const EventRegistry &registry, std::size_t symbolUnits) {
    const events::EventValueLimits values{symbolUnits, FIELD_TEXT_UNITS, add(symbolUnits, 2 * FIELD_TEXT_UNITS)};
    profile.bindings.reserve(registry.registrations().size());

    if (profile.bindings.capacity() > SCHEMA_LIMITS.records)
        refuse(ErrorCode::RESOURCE_LIMIT, "profile-mapping-storage");

    for (std::size_t kind = 0; kind < RECEIVING_EVENT_KINDS; ++kind) {
        for (const auto &registration : registry.registrations()) {
            if (static_cast<std::size_t>(registration.type) != kind)
                continue;

            const auto index = profile.bindings.size();
            const auto &record = profile.schema.records()[static_cast<std::size_t>(registration.localRecordId)];
            profile.bindingExchanges[index] =
                registration.type == EventType::PROFILE ? 0 : events::market_symbols::getExchangeCode(record.name);
            const auto policy = registration.eventTimePolicy == EventTimePolicy::FIELD_METADATA
                                    ? client::ReceiverEventTimePolicy::FIELD_METADATA
                                    : client::ReceiverEventTimePolicy::UNSPECIFIED;
            profile.bindings.push_back({registration.localRecordId,
                                        static_cast<client::PreparedEventKind>(registration.type),
                                        registration.mappingLimits, values, policy});
        }
    }

    for (std::size_t mask = 1; mask <= RECEIVING_EVENT_SELECTIONS; ++mask) {
        auto &plan = profile.eventPlans[mask - 1];
        plan.work.visits = 16;
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
            const bool text = binding.kind == client::PreparedEventKind::PROFILE;
            const auto units = text ? values.totalUnits : values.symbolUnits;
            const auto strings = text ? std::size_t{3} : std::size_t{1};
            const auto bytes =
                add(sizeof(client::SubscriptionProjection), multiply(add(units, strings), sizeof(char16_t)));
            plan.bindingIndices[plan.bindingCount++] = index;
            plan.limits.bindings = plan.bindingCount;
            plan.limits.shapeFields = add(plan.limits.shapeFields, fields);
            plan.limits.selections = add(plan.limits.selections, mapping.selections);
            plan.work.preparationWork = add(plan.work.preparationWork, preparation);
            plan.work.visits = add(plan.work.visits, add(plan.bindingCount, 3));
            plan.projectionWork.visits =
                add(plan.projectionWork.visits, multiply(CALLBACK_RECORDS, add(fields, add(mapping.selections, 3))));
            plan.projectionWork.textUnits = add(plan.projectionWork.textUnits, multiply(CALLBACK_RECORDS, units));
            plan.projectionWork.bytes = add(plan.projectionWork.bytes, multiply(CALLBACK_RECORDS, bytes));
        }

        plan.projectionWork.visits = add(plan.projectionWork.visits, multiply(CALLBACK_RECORDS, plan.bindingCount));
    }
}
} // namespace

ReceivingProfile::ReceivingProfile(core::LocalSchema schema, client::EndpointOptions endpoint)
    : schema(std::move(schema)), endpoint(std::move(endpoint)) {
}

ReceivingProfile prepareReceivingProfile(const RuntimeOptions &runtime, const EndpointOptions &options,
                                         const std::string &host, std::uint16_t port) {
    auto selected = selectRegistry(options.eventRegistry ? &*options.eventRegistry : nullptr);
    auto copied = selected.schema().boundedCopy(SCHEMA_LIMITS);

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
    const auto maximumRoutes = std::max(std::size_t{1}, add(add(routes[0], routes[1]), routes[2]));
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
    budgets.session.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
    budgets.session.fieldTime = protocol::FieldTimePolicy::READ;
    budgets.session.data.processedRecords = budgets.session.data.emittedRecords = FRAME_RECORDS;
    budgets.session.data.wireFields = budgets.session.data.emittedValues = multiply(FRAME_RECORDS, maximumFields);
    budgets.session.data.symbol.codeUnits = budgets.session.data.symbol.stateUnits = runtime.maximumSymbolUnits;
    budgets.session.data.ownedFieldUnits = FIELD_TEXT_UNITS;
    budgets.session.data.ownedPayloadBytes = FIELD_BYTE_UNITS;
    budgets.desired = {SCHEMA_LIMITS.records,    entries, runtime.maximumSymbolUnits, symbolUnits, multiply(2, entries),
                       multiply(2, symbolUnits), entries};
    budgets.writer.operationEntries = std::max(budgets.writer.operationEntries, entries);

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

    auto endpoint = client::EndpointOptions::create(schema, settings);

    if (!endpoint) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-endpoint");
    }

    ReceivingProfile result(std::move(schema), std::move(*endpoint));
    result.family = options.contract == DeliveryContract::TICKER ? core::SubscriptionFamily::TICKER
                                                                 : core::SubscriptionFamily::STREAM;
    result.recoveryOptions.selectors = SELECTORS;
    result.recoveryOptions.dns.serverCount = runtime.dnsServers.size();
    result.recoveryOptions.dns.port = runtime.dnsPort;
    std::ranges::copy(runtime.dnsServers, result.recoveryOptions.dns.servers.begin());
    result.interests = {SCHEMA_LIMITS.records,      clients,     entries, entries,
                        runtime.maximumSymbolUnits, symbolUnits, entries, multiply(scale, 4 * MEBIBYTE)};
    result.clientLimits = {QUEUED_RECORDS,
                           RECORD_BYTES,
                           8,
                           BATCH_BYTES,
                           CALLBACK_RECORDS,
                           SCHEMA_LIMITS.fields,
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
                          CALLBACK_RECORDS,
                          16,
                          multiply(clients, QUEUED_RECORDS),
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
    result.blueprint = prepareBlueprint(result.schema);
    prepareCopies(result);
    prepareMappings(result, selected, runtime.maximumSymbolUnits);
    auto &routing = result.routing;
    routing.symbols = runtime.maximumSymbolsPerSubscription;
    const auto logicalUnits = multiply(routing.symbols, runtime.maximumSymbolUnits);
    routing.temporaryTextUnits = multiply(2, logicalUnits);
    // The port's retained logical set and the queue's owned command copies have their own existing bounds.
    const auto textBacking = multiply(add(routing.temporaryTextUnits, multiply(2, routing.symbols)), sizeof(char16_t));
    routing.temporaryBytes = add(add(multiply(routing.symbols, sizeof(ReceivingSymbol)),
                                     multiply(subscriptionEntries, sizeof(core::SubscriptionRequest))),
                                 textBacking);
    routing.retainedMetadataBytes =
        add(sizeof(result.bindingExchanges),
            add(sizeof(result.eventPlans),
                sizeof(std::array<client::PreparedSubscriptionBinding, MAXIMUM_RECEIVING_RECORDS>)));
    // Snapshot copy, suffix/representability scans, physical conversion and conservative retirement.
    constexpr std::size_t ROUTING_TEXT_PASSES = 11;
    routing.work = {0, add(multiply(routing.symbols, add(multiply(2, maximumRoutes), 8)), maximumRoutes),
                    multiply(ROUTING_TEXT_PASSES, logicalUnits),
                    add(multiply(2, routing.temporaryBytes), routing.retainedMetadataBytes)};
    result.queueWork =
        queueWork(scale, result.interests, result.schema.records().size(), result.queueLimits.routeRecords);
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

    if (!mask || mask > RECEIVING_EVENT_SELECTIONS)
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "subscription-event-type");

    return profile.eventPlans[mask - 1];
}

bool copyReceivingBindings(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                           std::span<client::PreparedSubscriptionBinding> output, client::ReceiverWork &work) noexcept {
    const auto visits = plan.bindingCount <= MAXIMUM_RECEIVING_RECORDS ? 2 * plan.bindingCount : 0;

    if (output.size() != MAXIMUM_RECEIVING_RECORDS || plan.bindingCount > MAXIMUM_RECEIVING_RECORDS ||
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

    if (registry->schema().records().size() > SCHEMA_LIMITS.records) {
        refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-schema");
    }

    const auto exchange = events::market_symbols::getExchangeCode(symbol);

    for (const auto &route : registry->registrations()) {
        if (route.type == EventType::PROFILE || !selectsEvent(selection, route.type)) {
            continue;
        }

        const auto &record = registry->schema().records()[static_cast<std::size_t>(route.localRecordId)];

        if (record.name.size() > SCHEMA_LIMITS.nameUnits) {
            refuse(ErrorCode::UNSUPPORTED_PROFILE, "profile-schema");
        }

        if (exchange && events::market_symbols::getExchangeCode(record.name) == exchange) {
            refuse(ErrorCode::INVALID_ARGUMENT, "subscription-symbol-representation");
        }
    }
}

void prepareReceivingSymbols(const ReceivingProfile &profile, const ReceivingEventPlan &plan,
                             std::span<ReceivingSymbol> symbols) {
    for (auto &symbol : symbols) {
        symbol.regional = {};
        // Retire any previous physical backing before constructing its replacement, including helper reuse.
        std::u16string{}.swap(symbol.physical);
        symbol.exchange = events::market_symbols::getExchangeCode(symbol.logical);

        for (std::size_t offset = 0; offset < plan.bindingCount; ++offset) {
            const auto index = plan.bindingIndices[offset];
            const auto exchange = profile.bindingExchanges[index];

            if (exchange && exchange == symbol.exchange) {
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

bool receivingSymbolMatches(const ReceivingProfile &profile, std::size_t bindingIndex,
                            const ReceivingSymbol &symbol) noexcept {
    const auto kind = static_cast<std::size_t>(profile.bindings[bindingIndex].kind);
    const auto exchange = profile.bindingExchanges[bindingIndex];

    return exchange ? exchange == symbol.exchange : !symbol.regional[kind];
}

std::expected<mux::MuxQueue, mux::QueueFailure> createReceivingQueue(const ReceivingProfile &profile,
                                                                     mux::QueueWork &work) {
    if (profile.family == core::SubscriptionFamily::TICKER) {
        return mux::MuxQueue::createTicker(profile.schema, profile.interests, profile.tickerLimits,
                                           {false, 100, SELECTORS}, profile.queueLimits, work);
    }

    return mux::MuxQueue::createStream(profile.schema, profile.interests, profile.endpoint.settings().budgets.desired,
                                       profile.endpoint.settings().budgets.collector, profile.queueLimits, work,
                                       SELECTORS);
}
} // namespace org::ttldtor::qdcxx::api::detail
