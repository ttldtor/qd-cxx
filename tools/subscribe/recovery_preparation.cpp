// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_preparation.hpp"
#include "recovery_file_input.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON
#    include "qd/schema/json_schema.hpp"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <concepts>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::size_t MEBIBYTE = 1024 * 1024; ///< Independent byte unit, never a work or UTF-16 unit.
constexpr core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Complete declarations, including aliases.
constexpr std::size_t SCHEMA_DOCUMENT_BYTES = MEBIBYTE;               ///< Explicit JSON file payload maximum.
constexpr std::size_t SCHEMA_JSON_DEPTH = 16;                         ///< Existing selected loader nesting profile.
constexpr std::size_t SCHEMA_JSON_PARSER_BYTES = 64 * MEBIBYTE;       ///< Independent fresh-parser requests.
constexpr std::size_t SCHEMA_JSON_WORK = 64 * MEBIBYTE;        ///< Logical parser work, not storage or instructions.
constexpr std::size_t SCHEMA_JSON_MODEL_BYTES = 64 * MEBIBYTE; ///< Independent logical staging/result bound.
constexpr std::size_t MAXIMUM_SYMBOLS = 16;                    ///< One local record's exact selectors per invocation.
constexpr std::size_t SYMBOL_UNITS = 128;                      ///< One selected or decoded record symbol.
constexpr std::size_t DESIRED_ENTRIES = 32; ///< Fixed current desired storage; copied replay banks use the same count.
constexpr std::size_t DESIRED_SYMBOL_UNITS = MAXIMUM_SYMBOLS * SYMBOL_UNITS; ///< Current exact selector text ceiling.
constexpr std::size_t DESIRED_PEAK_ENTRIES = 2 * DESIRED_ENTRIES;            ///< Old and candidate desired storage.
constexpr std::size_t DESIRED_PEAK_SYMBOL_UNITS = 2 * DESIRED_SYMBOL_UNITS;  ///< Old and candidate exact text.
constexpr std::size_t MAXIMUM_FRAME_RECORDS = 64;  ///< Actual emitted/processed records per admitted DATA frame.
constexpr std::size_t FRAME_BODY_BYTES = 8192;     ///< Existing controlled frame body capacity.
constexpr std::size_t FRAME_ENVELOPE_BYTES = 8201; ///< Existing frame body plus maximum envelope allowance.
constexpr std::size_t FIELD_TEXT_UNITS = 8192;     ///< One record's conservative aggregate text copy/equality maximum.
constexpr std::size_t FIELD_BYTE_UNITS = 16384;    ///< One record's conservative aggregate opaque payload maximum.
constexpr std::size_t CALLBACK_RECORDS = 4;        ///< Whole callback maximum; record targets never truncate it.
constexpr std::size_t ROUTE_RECORDS = 16;          ///< One queue route/cleanup quantum.
constexpr std::size_t QUEUED_RECORDS = 128;        ///< Per-client and aggregate detached-data slots.
constexpr std::size_t RECORD_ARENA_BYTES = 65536;  ///< Whole immutable per-record arena request ceiling.
constexpr std::size_t QUEUE_RECORD_BYTES = 16 * MEBIBYTE; ///< Detached records and arenas, excluding batch scopes.
constexpr std::size_t QUEUE_BATCH_BYTES = MEBIBYTE;       ///< Independently bounded composites and record-view arrays.
constexpr std::size_t QUEUE_RESIDENT_BYTES = 64 * MEBIBYTE; ///< Existing queue requested-backing ceiling.
constexpr std::size_t COLLECTOR_EVENT_BYTES = 2 * MEBIBYTE; ///< Shared retained full event State/arena scope.
constexpr std::size_t HISTORY_PARTITIONS = MAXIMUM_SYMBOLS; ///< Exact selected record/symbol partitions.
constexpr std::size_t HISTORY_POINTS = 256;        ///< Shared retained raw signed64 points, not a lifetime feed quota.
constexpr std::size_t COLLECTOR_VIEWS = 1;         ///< One frontend registration and collector-local desired owner.
constexpr std::size_t COLLECTOR_FRAMES = 8;        ///< Cache and per-view pending owner ceilings.
constexpr std::size_t HISTORY_PENDING_ITEMS = 128; ///< One view's detached update FIFO.
constexpr std::size_t COLLECTOR_RETRIEVAL_QUANTUM = 100; ///< Existing explicit retrieval scheduling profile.
constexpr std::size_t HISTORY_PARTITION_QUANTUM = 10000; ///< Existing explicit per-partition snapshot rotation.
constexpr std::size_t CONTROL_OPERATIONS = 100000;       ///< Fixed finite general owner/control operation allowance.
constexpr std::size_t CONTROL_VISITS = 100000000;        ///< Fixed finite general owner metadata/field allowance.
constexpr std::size_t CONTROL_TEXT_UNITS = 100000000;    ///< Fixed finite non-HISTORY text allowance.
constexpr std::size_t CONTROL_BYTES = 256 * MEBIBYTE;    ///< Fixed finite general logical copy/disposal work.
constexpr std::size_t WRITER_CONFIGURATION_BYTES = 4 * MEBIBYTE; ///< Retained writer configuration ceiling.
constexpr std::size_t EPOCH_LABEL_BYTES = MEBIBYTE;       ///< Separate fixed app presentation token-array ceiling.
constexpr std::size_t DNS_ENVELOPE_BYTES = 80 * MEBIBYTE; ///< Initial per-channel logical SDK reservation.
constexpr std::uint64_t DNS_PROGRESS_UNITS = 351280142190671838ULL; ///< Pinned complete DNS logical progress charge.
constexpr std::size_t DNS_TURN_PACKETS = 10000;        ///< Distinct resolver callback/datagram turn allowance.
constexpr std::size_t DNS_TURN_BYTES = 10000000;       ///< Distinct DNS logical byte work, not reserved storage.
constexpr std::size_t UTF16_COPY_PADDING = 8;          ///< Qualified fresh-copy SSO/terminator envelope per string.
constexpr std::size_t ASCII_COPY_PADDING = 16;         ///< Qualified fresh ASCII SSO/terminator envelope.
constexpr std::size_t SCHEMA_COPY_PEAK = 3;            ///< Endpoint, fresh composer and live-session schema coexist.
constexpr std::size_t ENDPOINT_TEXT_COPY_PEAK = 2;     ///< Temporary settings and immutable state text coexist.
constexpr std::size_t MAPPING_SELECTIONS = 32;         ///< Existing Quote/Profile request-selection allowance.
constexpr std::size_t MAPPING_KEYS = 64;               ///< Existing complete request-key allowance.
constexpr std::size_t MAPPING_TYPE_ALTERNATIVES = 128; ///< Covers Quote72/Profile108 declared exact-type alternatives.
constexpr std::size_t MAPPING_REQUEST_NAME_UNITS = 4096; ///< Additional fixed request-key text allowance.

struct Refusal {
    std::string_view message;
}; ///< Static local escape with no caller/peer text.

/** Reject locally without formatting an endpoint, path or arbitrary exception.
 * @param message Static diagnostic.
 */
[[noreturn]] void refuse(std::string_view message) {
    throw Refusal{message};
}

/** Add explicitly finite quantities without wrapping.
 * @param left Existing amount.
 * @param right Additional amount in the same units.
 * @return Exact sum, or static unsupported-capacity refusal.
 */
std::size_t add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left)
        refuse("recovery profile arithmetic exceeds this platform");

    return left + right;
}

/** Multiply a finite count and per-element quantity without wrapping.
 * @param count Finite count.
 * @param amount Per-element quantity.
 * @return Exact product or static unsupported-capacity refusal.
 */
std::size_t multiply(std::size_t count, std::size_t amount) {
    if (amount && count > std::numeric_limits<std::size_t>::max() / amount)
        refuse("recovery profile arithmetic exceeds this platform");

    return count * amount;
}

/** Admit fixed frontend bounds before file inspection or proportional schema work.
 * This checks application/profile shape, not another network/command-language parser.
 * @param input Already parsed complete arguments, borrowed unchanged.
 */
void validateApplicationProfile(const RecoveryConfiguration &input) {
    if (input.security == client::EndpointSecurity::TLS) {
#if !defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
        refuse("recovery TLS is unavailable in this build");
#endif
    } else if (input.security != client::EndpointSecurity::CREDENTIAL_FREE_PLAIN || input.tlsTrustPem ||
               input.tlsReference || input.authBasicTokenFile || input.authTimeout) {
        refuse("recovery plain application profile forbids TLS or credentials");
    }

    if (input.family != core::SubscriptionFamily::STREAM && input.family != core::SubscriptionFamily::TICKER &&
        input.family != core::SubscriptionFamily::HISTORY)
        refuse("unsupported recovery family");

    if (input.selectors != core::SubscriptionSelectorPolicy::EXACT_ONLY &&
        input.selectors != core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1)
        refuse("unsupported recovery selector profile");

    if (input.symbols.empty() || input.symbols.size() > MAXIMUM_SYMBOLS)
        refuse("recovery selector count exceeds the application profile");

    std::size_t units = 0;

    for (const auto &symbol : input.symbols) {
        if (symbol.empty() || symbol.size() > SYMBOL_UNITS || symbol.front() == u'*' ||
            !std::ranges::all_of(symbol, [](char16_t unit) {
                return unit >= 0x20 && unit <= 0x7e;
            }))
            refuse("recovery selector exceeds the application profile");

        units = add(units, symbol.size());
    }

    if (units > DESIRED_SYMBOL_UNITS || input.attempts == 0)
        refuse("recovery requested capacities exceed the application profile");

    const auto labels = add(static_cast<std::size_t>(input.attempts), 1);

    if (multiply(labels, sizeof(mux::QueueIdentity)) > EPOCH_LABEL_BYTES)
        refuse("recovery attempt count exceeds the presentation profile");

    if (input.family == core::SubscriptionFamily::HISTORY ? !input.historyFromRaw : input.historyFromRaw.has_value())
        refuse("recovery HISTORY bound conflicts with the selected family");

    if (input.recordId < 0 || input.duration.count() <= 0 || input.duration.count() > 600000 ||
        input.drain.count() <= 0 || input.drain.count() > 60000 ||
        (input.removeAfter && (input.removeAfter->count() <= 0 || *input.removeAfter >= input.duration)))
        refuse("recovery application interval or record selection is invalid");

#if !defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) || !ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON

    if (input.schemaJson)
        refuse("recovery native binary JSON schema support is disabled");

#endif
}

/** Read at most the fixed input cap plus one detection byte from an ordinary file.
 * @param pathBytes Explicit UTF-8 path bytes, never included in diagnostics.
 * @return Exact owned document bytes, including whitespace, without trimming or autodetection.
 */
[[maybe_unused]] std::string readSchemaDocument(std::string_view pathBytes) {
    auto document = readRecoveryFile(pathBytes, SCHEMA_DOCUMENT_BYTES);

    if (!document) {
        refuse(document.error());
    }

    return std::move(*document);
}

/** Load the selected complete declaration set without reducing it to the subscribed record.
 * @param input Explicit schema selection; caller has already rejected unsupported TLS/features.
 * @return Complete owned BINARY_FIELDS_V1 schema with all aliases and indexed metadata unchanged.
 */
core::LocalSchema prepareSchema(const RecoveryConfiguration &input) {
    if (input.schemaJson) {
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON) && ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON
        const auto document = readSchemaDocument(*input.schemaJson);
        schema::BinaryJsonSchemaLimits limits;
        limits.loading = {SCHEMA_DOCUMENT_BYTES, SCHEMA_JSON_DEPTH, SCHEMA_LIMITS};
        limits.parserBytes = SCHEMA_JSON_PARSER_BYTES;
        limits.workUnits = SCHEMA_JSON_WORK;
        limits.schemaModelBytes = SCHEMA_JSON_MODEL_BYTES;
        auto loaded = schema::loadBinaryJsonSchema(document, limits);

        if (!loaded)
            refuse("recovery native binary JSON schema was refused");

        return std::move(*loaded);
#else
        refuse("recovery native binary JSON schema support is disabled");
#endif
    }

    if (input.schema != SchemaProfile::QUOTE && input.schema != SchemaProfile::PROFILE)
        refuse("unsupported recovery builtin schema selection");

    auto builtin = input.schema == SchemaProfile::QUOTE ? events::defaultQuoteSchema(SCHEMA_LIMITS)
                                                        : events::defaultProfileSchema(SCHEMA_LIMITS);

    if (!builtin)
        refuse("recovery builtin schema preparation failed");

    auto binary = core::LocalSchema::create(builtin->records(), SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);

    if (!binary)
        refuse("recovery builtin binary schema preparation failed");

    return std::move(*binary);
}

/** Classify only route syntax already accepted/refused by the immutable endpoint boundary.
 * Numeric-looking invalid input remains numeric and is rejected by EndpointOptions::create.
 * @param host Explicit host spelling.
 * @return Whether this route needs the existing explicit DNS path.
 */
bool namedRoute(std::string_view host) {
    return host.find(':') == std::string_view::npos && !std::ranges::all_of(host, [](unsigned char byte) {
               return (byte >= '0' && byte <= '9') || byte == '.';
           });
}

/** Adapt the already selected bounded IPv4 resolver spelling to the public octet value.
 * No DNS parsing, lookup, discovery or socket/SDK initialization occurs.
 * @param text Explicit resolver address from the parsed frontend configuration.
 * @return Exact network-order octets or a static handoff refusal.
 */
transport::DnsServer dnsServer(std::string_view text) {
    transport::DnsServer result{};

    for (std::size_t index = 0; index < result.address.size(); ++index) {
        const auto dot = text.find('.');
        const auto token = text.substr(0, dot);
        unsigned value = 0;
        const auto converted = std::from_chars(token.data(), token.data() + token.size(), value);

        if (token.empty() || token.size() > 3 || (token.size() > 1 && token.front() == '0') ||
            converted.ec != std::errc{} || converted.ptr != token.data() + token.size() || value > 255 ||
            ((index + 1 == result.address.size()) != (dot == std::string_view::npos)))
            refuse("recovery explicit DNS resolver is invalid");

        result.address[index] = static_cast<std::uint8_t>(value);

        if (dot != std::string_view::npos)
            text.remove_prefix(dot + 1);
    }

    if (std::ranges::all_of(result.address, [](auto byte) {
            return byte == 0;
        }))
        refuse("recovery explicit DNS resolver is invalid");

    return result;
}

/** Add DNS memory and separate preparation/progress work before the endpoint snapshot is created.
 * @param settings Original mutable construction settings, not yet published.
 */
void fundDns(client::EndpointSettings &settings) {
    auto required = transport::TcpRuntime::dnsPreparationWorkRequirement(1);

    if (!required)
        refuse("recovery DNS preparation profile is unsupported");

    settings.budgets.tcp.residentBytes = add(settings.budgets.tcp.residentBytes, DNS_ENVELOPE_BYTES);
    auto &work = settings.budgets.protocolTurn;
    work.operations = add(work.operations, required->operations);
    work.visits = add(work.visits, required->visits);
    work.textUnits = add(work.textUnits, required->textUnits);
    work.bytes = add(work.bytes, required->bytes);
    auto &runtime = settings.budgets.runtimeTurn;
    runtime.dnsCalls = DNS_TURN_PACKETS;
    runtime.dnsPackets = DNS_TURN_PACKETS;
    runtime.dnsBytes = DNS_TURN_BYTES;
    runtime.dnsUnits = DNS_PROGRESS_UNITS;
}

/** Preserve complete fields and select existing aliases, with explicit indexed source words first.
 * @param schema Complete validated local schema, including unselected records and every alias.
 * @return Complete one-to-one outgoing declarations; sender IDs use the actual small local IDs.
 */
std::vector<protocol::OutgoingRecord> prepareBlueprint(const core::LocalSchema &schema) {
    std::vector<protocol::OutgoingRecord> result;
    result.reserve(schema.records().size());

    for (const auto &record : schema.records()) {
        protocol::OutgoingRecord outgoing{record.id, record.id, record.name, {}};
        outgoing.fields.reserve(record.fields.size());
        std::vector<std::size_t> order;
        order.reserve(record.fields.size());

        if (record.indexedTimeLayout) {
            std::visit(
                [&](const auto &layout) {
                    using Layout = std::decay_t<decltype(layout)>;

                    if constexpr (std::same_as<Layout, core::IndexedTimePairInt32>) {
                        order.push_back(layout.firstField);
                        order.push_back(layout.secondField);
                    } else
                        order.push_back(layout.field);
                },
                *record.indexedTimeLayout);
        }

        for (std::size_t field = 0; field < record.fields.size(); ++field)
            if (std::ranges::find(order, field) == order.end())
                order.push_back(field);

        for (const auto field : order) {
            const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });

            if (lookup == record.lookups.end())
                refuse("recovery outgoing field alias is absent");

            outgoing.fields.push_back(
                {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        result.push_back(std::move(outgoing));
    }

    return result;
}

/** Derive whole-frame HISTORY work from the actual fixed capacities and the pinned source charges.
 * @param profile Complete schema/settings and fixed collector/client bounds.
 * @return Checked logical stage envelope, without inspecting future payloads or invoking a collector.
 */
RecoveryHistoryEnvelope historyEnvelope(const PreparedRecovery &profile) {
    const auto &limits = profile.historyLimits;
    const auto &data = profile.endpoint.settings().budgets.session.data;
    const auto views = limits.viewSlots;
    const auto points = limits.points;
    const auto partitions = limits.partitions;
    const auto entries = limits.viewSubscriptions.entries;
    const auto pending = multiply(views, limits.pendingItems);
    const auto viewEntries = multiply(views, entries);
    std::size_t maxFields = 0;

    for (const auto &record : profile.schema.records())
        maxFields = std::max(maxFields, record.fields.size());

    RecoveryHistoryEnvelope result;
    result.maximumFrameRecords = data.emittedRecords;
    result.frameSlots = add(add(limits.cacheFrames, multiply(views, limits.pendingFrames)), 1);
    const auto rowViews =
        multiply(add(points, 1),
                 multiply(views, add(add(entries, limits.pendingItems), add(add(result.frameSlots, points), 4))));
    result.rowVisits = multiply(64, add(add(partitions, multiply(points, points)), add(rowViews, add(maxFields, 8))));
    auto fixed = add(8, multiply(12, add(add(partitions, points), add(pending, viewEntries))));
    fixed = add(fixed, multiply(12, add(result.frameSlots, views)));
    fixed = add(fixed, multiply(8, multiply(partitions, multiply(views, add(entries, 1)))));
    fixed = add(fixed, multiply(add(views, 2), add(add(result.frameSlots, points), add(pending, 4))));
    fixed = add(fixed, multiply(16, multiply(partitions, points)));
    fixed = add(fixed, multiply(16, multiply(partitions, pending)));
    fixed = add(fixed, multiply(12, multiply(views, maxFields)));
    fixed = add(fixed, multiply(16, multiply(profile.schema.records().size(), add(viewEntries, 1))));
    result.fixedVisits = fixed;
    const auto records = result.maximumFrameRecords;
    const auto equalityVisits = multiply(records, add(1, multiply(3, maxFields)));
    const auto canonicalVisits =
        profile.recoveryOptions.selectors == core::SubscriptionSelectorPolicy::EXACT_ONLY ? 0 : add(records, 1);
    result.stageVisits = add(add(fixed, multiply(records, result.rowVisits)), add(equalityVisits, canonicalVisits));
    const auto maintenanceText =
        multiply(8, multiply(add(partitions, 1),
                             multiply(add(views, 1), add(limits.viewSubscriptions.totalSymbolUnits,
                                                         multiply(partitions, limits.symbolUnitsPerPartition)))));
    const auto structuralText = multiply(
        8, multiply(records, multiply(add(partitions, 1), add(data.symbol.codeUnits, limits.symbolUnitsPerPartition))));
    const auto equalityText = multiply(records, multiply(2, data.ownedFieldUnits));
    const auto canonicalText =
        profile.recoveryOptions.selectors == core::SubscriptionSelectorPolicy::EXACT_ONLY ? 0 : multiply(records, 7);
    result.stageTextUnits = add(add(maintenanceText, structuralText), add(equalityText, canonicalText));
    const auto releaseBytes =
        multiply(add(result.frameSlots, 1), profile.endpoint.settings().budgets.session.residentBytes);
    result.stageBytes = add(releaseBytes, multiply(records, multiply(2, data.ownedPayloadBytes)));

    return result;
}

/** Supply the named fixed non-HISTORY queue work seed used by normal and drain turns.
 * @return Original public coordinate types with finite independent allowances.
 */
mux::QueueWork baseQueueWork() {
    return {CONTROL_OPERATIONS,
            CONTROL_VISITS,
            CONTROL_VISITS,
            CONTROL_TEXT_UNITS,
            CONTROL_BYTES,
            10000,
            10000,
            {CONTROL_OPERATIONS, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES},
            {CONTROL_OPERATIONS, CONTROL_VISITS, CONTROL_TEXT_UNITS},
            {CONTROL_OPERATIONS, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES},
            {CONTROL_OPERATIONS, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES},
            {CONTROL_OPERATIONS, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES}};
}

/** Fund one callback's exact public receiver projection precharge and optional mapping plan.
 * @param profile Complete selected schema/profile, changed only during preparation.
 * @param input Explicit output type and record selection.
 */
void prepareReceiver(PreparedRecovery &profile, const RecoveryConfiguration &input) {
    if (input.output == OutputProfile::GENERIC) {
        profile.receiverLimits = {0, 0, 0};
        profile.receiverWork = {16, 0};

        return;
    }

    if (input.output != OutputProfile::QUOTE && input.output != OutputProfile::PROFILE)
        refuse("unsupported recovery output profile");

    const auto comparisons =
        add(multiply(multiply(SCHEMA_LIMITS.lookups, MAPPING_KEYS), add(SCHEMA_LIMITS.nameUnits, 1)),
            multiply(MAPPING_SELECTIONS, MAPPING_TYPE_ALTERNATIVES));
    const events::MappingLimits mapping{SCHEMA_LIMITS.fields,
                                        SCHEMA_LIMITS.lookups,
                                        MAPPING_SELECTIONS,
                                        MAPPING_KEYS,
                                        MAPPING_TYPE_ALTERNATIVES,
                                        SCHEMA_LIMITS.nameUnits,
                                        add(SCHEMA_LIMITS.totalNameUnits, MAPPING_REQUEST_NAME_UNITS),
                                        comparisons};
    const auto kind =
        input.output == OutputProfile::QUOTE ? client::PreparedEventKind::QUOTE : client::PreparedEventKind::PROFILE;

    if (kind == client::PreparedEventKind::QUOTE) {
        if (!events::PreparedQuote::prepare(profile.schema, input.recordId, mapping))
            refuse("recovery selected record does not support the explicit Quote mapper");
    } else if (!events::PreparedProfile::prepare(profile.schema, input.recordId, mapping)) {
        refuse("recovery selected record does not support the explicit Profile mapper");
    }

    const events::EventValueLimits values{SYMBOL_UNITS, FIELD_TEXT_UNITS,
                                          add(SYMBOL_UNITS, multiply(2, FIELD_TEXT_UNITS))};
    profile.receiverBindings.push_back(
        {input.recordId, kind, mapping, values, client::ReceiverEventTimePolicy::FIELD_METADATA});
    const auto fields = profile.schema.records()[static_cast<std::size_t>(input.recordId)].fields.size();
    profile.receiverLimits = {1, fields, MAPPING_SELECTIONS};
    std::size_t preparation = 0;

    for (const auto amount :
         std::array{mapping.fields, mapping.lookups, mapping.selections, mapping.keys, mapping.typeAlternatives,
                    mapping.nameUnits, mapping.totalNameUnits, mapping.comparisonWork})
        preparation = add(preparation, amount);

    profile.receiverWork = {16, preparation};
    const auto units = kind == client::PreparedEventKind::QUOTE ? values.symbolUnits : values.totalUnits;
    const auto textValues = kind == client::PreparedEventKind::QUOTE ? std::size_t{1} : std::size_t{3};
    const auto bytes = add(sizeof(client::SubscriptionProjection), multiply(add(units, textValues), sizeof(char16_t)));
    profile.projectionWork = {multiply(CALLBACK_RECORDS, add(fields, add(MAPPING_SELECTIONS, 3))),
                              multiply(CALLBACK_RECORDS, units), multiply(CALLBACK_RECORDS, bytes)};
}

/** Configure finite copy envelopes from complete actual public declarations/capacities.
 * @param profile App-owned complete schema/blueprint/settings and family.
 */
void prepareRecoveryCopies(PreparedRecovery &profile) {
    transport::RecoverySchemaCapacity one;
    one.records = profile.schema.records().size();

    for (const auto &record : profile.schema.records()) {
        one.fields = add(one.fields, record.fields.size());
        one.lookups = add(one.lookups, record.lookups.size());
        one.nameUnits = add(one.nameUnits, add(record.name.size(), UTF16_COPY_PADDING));

        for (const auto &lookup : record.lookups)
            one.nameUnits = add(one.nameUnits, add(lookup.name.size(), UTF16_COPY_PADDING));
    }

    auto &limits = profile.recoveryOptions.limits;
    limits.schemaCopies = {multiply(SCHEMA_COPY_PEAK, one.records), multiply(SCHEMA_COPY_PEAK, one.fields),
                           multiply(SCHEMA_COPY_PEAK, one.lookups), multiply(SCHEMA_COPY_PEAK, one.nameUnits)};
    limits.blueprintCopy.records = profile.blueprint.size();
    limits.adoptedBlueprint.records = profile.blueprint.capacity();

    for (const auto &record : profile.blueprint) {
        limits.blueprintCopy.fields = add(limits.blueprintCopy.fields, record.fields.size());
        limits.adoptedBlueprint.fields = add(limits.adoptedBlueprint.fields, record.fields.capacity());
        limits.blueprintCopy.nameUnits =
            add(limits.blueprintCopy.nameUnits, add(record.wireName.size(), UTF16_COPY_PADDING));
        limits.adoptedBlueprint.nameUnits = add(limits.adoptedBlueprint.nameUnits, add(record.wireName.capacity(), 1));

        for (const auto &field : record.fields) {
            limits.blueprintCopy.nameUnits =
                add(limits.blueprintCopy.nameUnits, add(field.wireName.size(), UTF16_COPY_PADDING));
            limits.adoptedBlueprint.nameUnits =
                add(limits.adoptedBlueprint.nameUnits, add(field.wireName.capacity(), 1));
        }
    }

    const auto &settings = profile.endpoint.settings();
    const auto copiedTextBytes = [](const std::string &text) {
        return std::max(add(text.capacity(), 1), add(text.size(), ASCII_COPY_PADDING));
    };
    auto endpointTextBytes = copiedTextBytes(settings.host);

    if (settings.tls) {
        endpointTextBytes = add(endpointTextBytes, copiedTextBytes(settings.tls->referenceIdentity));
        endpointTextBytes = add(endpointTextBytes, copiedTextBytes(settings.tls->trustPem));
    }

    limits.endpointCopyBytes = multiply(ENDPOINT_TEXT_COPY_PEAK, endpointTextBytes);
    limits.writerConfigurationBytes = WRITER_CONFIGURATION_BYTES;
    const auto family = profile.family == core::SubscriptionFamily::TICKER
                            ? transport::RecoveryFamily::TICKER
                            : (profile.family == core::SubscriptionFamily::STREAM ? transport::RecoveryFamily::STREAM
                                                                                  : transport::RecoveryFamily::HISTORY);
    limits.replay[static_cast<std::size_t>(family)] = {DESIRED_ENTRIES, DESIRED_PEAK_SYMBOL_UNITS};
}

/** Validate complete outgoing descriptions and whole initial/removal frames without native work.
 * This temporary composer retains no output after preparation and submits no authoritative desire.
 * @param profile Complete fixed profile.
 * @param input Borrowed initial exact symbols and optional raw HISTORY bound.
 */
void preflightWriter(const PreparedRecovery &profile, const RecoveryConfiguration &input) {
    const auto &budgets = profile.endpoint.settings().budgets;
    auto work = profile.preparationWork.writer;
    auto writer = protocol::SubscriptionWriter::create(profile.schema, profile.blueprint, budgets.layout,
                                                       budgets.writer, work, input.selectors);

    if (!writer || writer->storageBytes() > WRITER_CONFIGURATION_BYTES)
        refuse("recovery complete outgoing layout exceeds the writer profile");

    std::vector<std::int32_t> ids;
    ids.reserve(profile.blueprint.size());

    for (const auto &record : profile.blueprint)
        ids.push_back(record.senderId);

    if (!writer->describe(ids, work))
        refuse("recovery complete descriptions exceed the frame profile");

    std::vector<core::SubscriptionRequest> requests;
    requests.reserve(input.symbols.size());

    for (const auto &symbol : input.symbols)
        requests.push_back({{profile.selectedContext, symbol}, input.historyFromRaw});

    const auto addMessage =
        input.family == core::SubscriptionFamily::STREAM
            ? protocol::SubscriptionMessage::STREAM_ADD
            : (input.family == core::SubscriptionFamily::TICKER ? protocol::SubscriptionMessage::TICKER_ADD
                                                                : protocol::SubscriptionMessage::HISTORY_ADD);

    if (!writer->compose(addMessage, requests, work))
        refuse("recovery initial subscription exceeds the frame profile");

    if (input.removeAfter) {
        for (auto &request : requests)
            request.historyBound.reset();

        const auto removeMessage =
            input.family == core::SubscriptionFamily::STREAM
                ? protocol::SubscriptionMessage::STREAM_REMOVE
                : (input.family == core::SubscriptionFamily::TICKER ? protocol::SubscriptionMessage::TICKER_REMOVE
                                                                    : protocol::SubscriptionMessage::HISTORY_REMOVE);

        if (!writer->compose(removeMessage, requests, work))
            refuse("recovery removal exceeds the frame profile");
    }
}
} // namespace

std::expected<PreparedRecovery, std::string_view> prepareRecovery(const RecoveryConfiguration &input) {
    try {
        validateApplicationProfile(input);
        auto schema = prepareSchema(input);
        const auto context = schema.recordContext(input.recordId);

        if (!context.valid())
            refuse("recovery selected local record does not exist");

        if (input.family == core::SubscriptionFamily::HISTORY &&
            !schema.records()[static_cast<std::size_t>(input.recordId)].indexedTimeLayout)
            refuse("recovery HISTORY requires explicit indexed record metadata");

        auto configured = client::controlledStreamSettings(input.duration);

        if (!configured)
            refuse("recovery controlled endpoint profile is unavailable");

        auto &settings = *configured;
        settings.host = input.host;
        settings.port = input.port;
        settings.retry = {input.attempts, input.retryDelay};
        settings.timeouts = {input.dnsTimeout, input.connectTimeout, input.readTimeout, input.writeTimeout,
                             input.pollInterval};
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
        std::optional<PreparedTlsInput> tlsInput;

        if (input.security == client::EndpointSecurity::TLS) {
            auto preparedTls = prepareRecoveryTlsInput(input);

            if (!preparedTls) {
                refuse(preparedTls.error());
            }

            tlsInput.emplace(std::move(*preparedTls));
            settings.security = client::EndpointSecurity::TLS;
            settings.tls.emplace(std::move(tlsInput->endpoint));
        }

#endif
        auto &budgets = settings.budgets;
        budgets.schema = SCHEMA_LIMITS;
        budgets.layout = {SCHEMA_LIMITS, SCHEMA_LIMITS.records, SCHEMA_LIMITS.fields, SCHEMA_LIMITS.nameUnits,
                          SCHEMA_LIMITS.totalNameUnits};
        budgets.session.context.schema = SCHEMA_LIMITS;
        budgets.session.frame = {FRAME_BODY_BYTES, FRAME_ENVELOPE_BYTES};
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)

        if (tlsInput) {
            constexpr std::size_t TLS_CONTROL_BYTES = 64 * 1024; ///< Covers the bounded Basic token's3*N+64 envelope.
            budgets.session.outputBytes = TLS_CONTROL_BYTES;
            budgets.session.protocol.text.codeUnits = 4096;
            budgets.session.protocol.text.bodyBytes = TLS_CONTROL_BYTES;
            budgets.session.frame = {TLS_CONTROL_BYTES, TLS_CONTROL_BYTES + (FRAME_ENVELOPE_BYTES - FRAME_BODY_BYTES)};
            budgets.writer.frameBytes = TLS_CONTROL_BYTES;
            budgets.writer.operationBytes = TLS_CONTROL_BYTES + (FRAME_ENVELOPE_BYTES - FRAME_BODY_BYTES);
        }

#endif
        budgets.session.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
        budgets.session.fieldTime = protocol::FieldTimePolicy::READ;
        budgets.session.historySnapshot = input.family == core::SubscriptionFamily::HISTORY;
        budgets.session.data.processedRecords = MAXIMUM_FRAME_RECORDS;
        budgets.session.data.emittedRecords = MAXIMUM_FRAME_RECORDS;
        budgets.session.data.symbol.codeUnits = budgets.session.data.symbol.stateUnits = SYMBOL_UNITS;
        budgets.session.data.ownedFieldUnits = FIELD_TEXT_UNITS;
        budgets.session.data.ownedPayloadBytes = FIELD_BYTE_UNITS;
        budgets.desired = {SCHEMA_LIMITS.records, DESIRED_ENTRIES,           SYMBOL_UNITS,   DESIRED_SYMBOL_UNITS,
                           DESIRED_PEAK_ENTRIES,  DESIRED_PEAK_SYMBOL_UNITS, DESIRED_ENTRIES};
        const bool named = namedRoute(input.host);

        if (named) {
            if (!input.dnsServer)
                refuse("recovery named route requires an explicit IPv4 DNS server");

            fundDns(settings);
        }

        auto endpoint = client::EndpointOptions::create(schema, settings);

        if (!endpoint)
            refuse("recovery immutable endpoint profile was refused");

        PreparedRecovery result(std::move(schema), std::move(*endpoint));
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)

        if (tlsInput) {
            result.tlsLimits = tlsInput->limits;
        }

#endif
        result.selectedContext = context;
        result.family = input.family;
        result.namedRoute = named;
        result.epochLabelBytes = EPOCH_LABEL_BYTES;
        result.recoveryOptions.selectors = input.selectors;

        if (named) {
            result.recoveryOptions.dns.servers[0] = dnsServer(*input.dnsServer);
            result.recoveryOptions.dns.serverCount = 1;
            result.recoveryOptions.dns.port = input.dnsPort;

            if (input.dnsPort == 0)
                refuse("recovery DNS port is invalid");
        }

        const auto &effective = result.endpoint.settings().budgets;
        result.interests = {
            SCHEMA_LIMITS.records, 1,           DESIRED_ENTRIES, DESIRED_ENTRIES, SYMBOL_UNITS, DESIRED_SYMBOL_UNITS,
            DESIRED_ENTRIES,       4 * MEBIBYTE};
        result.queueLimits = {1,
                              MAXIMUM_FRAME_RECORDS,
                              ROUTE_RECORDS,
                              CALLBACK_RECORDS,
                              ROUTE_RECORDS,
                              QUEUED_RECORDS,
                              QUEUE_RECORD_BYTES,
                              8,
                              QUEUE_BATCH_BYTES,
                              4,
                              8,
                              2,
                              QUEUE_RESIDENT_BYTES};
        result.clientLimits = {QUEUED_RECORDS,
                               QUEUE_RECORD_BYTES,
                               8,
                               QUEUE_BATCH_BYTES,
                               CALLBACK_RECORDS,
                               SCHEMA_LIMITS.fields,
                               SYMBOL_UNITS,
                               FIELD_TEXT_UNITS,
                               FIELD_BYTE_UNITS,
                               RECORD_ARENA_BYTES,
                               4,
                               8,
                               DESIRED_ENTRIES,
                               DESIRED_PEAK_SYMBOL_UNITS};
        result.tickerLimits = {COLLECTOR_VIEWS,       MAXIMUM_SYMBOLS, COLLECTOR_FRAMES,
                               COLLECTOR_EVENT_BYTES, 8 * MEBIBYTE,    effective.desired};
        result.tickerOptions = {false, COLLECTOR_RETRIEVAL_QUANTUM, input.selectors};
        result.historyLimits = {HISTORY_PARTITIONS,    HISTORY_POINTS,  SYMBOL_UNITS,          COLLECTOR_FRAMES,
                                COLLECTOR_EVENT_BYTES, COLLECTOR_VIEWS, HISTORY_PENDING_ITEMS, COLLECTOR_FRAMES,
                                COLLECTOR_EVENT_BYTES, 16 * MEBIBYTE,   effective.desired};
        result.historyOptions = {false, COLLECTOR_RETRIEVAL_QUANTUM, HISTORY_PARTITION_QUANTUM, input.selectors};
        result.blueprint = prepareBlueprint(result.schema);
        prepareRecoveryCopies(result);
        prepareReceiver(result, input);
        result.queueWork = baseQueueWork();
        result.historyObservationWork = {};

        if (input.family == core::SubscriptionFamily::HISTORY) {
            result.historyEnvelope = historyEnvelope(result);
            const auto &envelope = result.historyEnvelope;
            // One stage plus front/pop work for a complete route quantum and bounded owner-control phases.
            const auto historyActions = add(multiply(2, ROUTE_RECORDS), 6);
            auto &history = result.queueWork.history;
            history.visits =
                std::max(history.visits, add(envelope.stageVisits, multiply(historyActions, envelope.fixedVisits)));
            history.textUnits = std::max(history.textUnits, multiply(historyActions, envelope.stageTextUnits));
            history.bytes = std::max(history.bytes, multiply(historyActions, envelope.stageBytes));
            auto &observation = result.historyObservationWork;
            observation.operations = MAXIMUM_SYMBOLS;
            observation.visits = multiply(MAXIMUM_SYMBOLS, add(multiply(5, DESIRED_ENTRIES), QUEUED_RECORDS));
            observation.textUnits =
                multiply(MAXIMUM_SYMBOLS, multiply(add(multiply(3, DESIRED_ENTRIES), QUEUED_RECORDS), SYMBOL_UNITS));
            observation.interest = {MAXIMUM_SYMBOLS, multiply(MAXIMUM_SYMBOLS, add(multiply(2, DESIRED_ENTRIES), 1)), 0,
                                    0};
            observation.history = {
                MAXIMUM_SYMBOLS, multiply(MAXIMUM_SYMBOLS, envelope.fixedVisits),
                multiply(MAXIMUM_SYMBOLS, multiply(add(HISTORY_PARTITIONS, 1), multiply(2, SYMBOL_UNITS))), 0};
        }

        result.submitWork = {16, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES};
        const transport::RecoveryWork recovery{10000, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES};
        const protocol::SubscriptionWriterWork writer{10000, CONTROL_VISITS, CONTROL_TEXT_UNITS, CONTROL_BYTES};
        result.preparationWork = {recovery, writer, {result.queueWork, result.queueWork, result.queueWork}};
        const auto &runtime = effective.runtimeTurn;
        result.turnWork = {recovery,
                           writer,
                           {runtime.steps, runtime.handlers, runtime.socketBytes, runtime.dnsCalls, runtime.dnsPackets,
                            runtime.dnsBytes, runtime.dnsUnits},
                           effective.protocolTurn,
                           {result.queueWork, result.queueWork, result.queueWork}};
        preflightWriter(result, input);

        return result;
    } catch (const Refusal &failure) {
        return std::unexpected(failure.message);
    } catch (const std::bad_alloc &) {
        return std::unexpected("recovery application preparation allocation failed");
    } catch (const std::length_error &) {
        return std::unexpected("recovery application preparation capacity failed");
    } catch (...) {
        return std::unexpected("recovery application preparation failed");
    }
}

std::expected<mux::MuxQueue, mux::QueueFailure>
createSelectedQueue(const PreparedRecovery &profile, const RecoveryConfiguration &input, mux::QueueWork &work) {
    if (profile.family != input.family || profile.recoveryOptions.selectors != input.selectors ||
        !profile.selectedContext.matches(profile.schema.recordContext(input.recordId)))
        return std::unexpected(mux::QueueFailure{mux::QueueError::INVALID_CONFIGURATION, {}});

    switch (profile.family) {
    case core::SubscriptionFamily::STREAM:
        return mux::MuxQueue::createStream(
            profile.schema, profile.interests, profile.endpoint.settings().budgets.desired,
            profile.endpoint.settings().budgets.collector, profile.queueLimits, work, input.selectors);
    case core::SubscriptionFamily::TICKER:
        return mux::MuxQueue::createTicker(profile.schema, profile.interests, profile.tickerLimits,
                                           profile.tickerOptions, profile.queueLimits, work);
    case core::SubscriptionFamily::HISTORY:
        return mux::MuxQueue::createHistory(profile.schema, profile.interests, profile.historyLimits,
                                            profile.historyOptions, profile.queueLimits, work);
    }

    return std::unexpected(mux::QueueFailure{mux::QueueError::INVALID_CONFIGURATION, {}});
}

mux::QueueWork freshQueueWork(const PreparedRecovery &profile) {
    return profile.queueWork;
}

transport::RecoveryTurnWork freshRecoveryTurn(const PreparedRecovery &profile) {
    return profile.turnWork;
}

mux::QueueWork freshHistoryObservationWork(const PreparedRecovery &profile) {
    return profile.historyObservationWork;
}

} // namespace org::ttldtor::qdcxx::connect
