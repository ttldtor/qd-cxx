// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"

#include "../protocol/local_event_owner.hpp"
#include "../transport/raw_byte_owner.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/short_string.hpp"
#include "qd/mux/demand.hpp"
#include "qd/mux/queue.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <asio/ip/address.hpp>
#include <bit>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <string>
#include <thread>

namespace org::ttldtor::qdcxx::api {
namespace {
using Clock = std::chrono::steady_clock;
using FieldType = core::SerialFieldType;
using namespace std::chrono_literals;

constexpr std::size_t MAX_PEERS = 8;
constexpr std::size_t MAX_PENDING_QUOTES = 1024;
constexpr std::size_t SYMBOL_UNITS = 256;
constexpr std::size_t CLIENT_KEYS = 32;
constexpr std::size_t CLIENT_PENDING_ITEMS = 32;
constexpr std::size_t HISTORY_POINTS_PER_KEY = 16;
constexpr std::size_t CLIENT_SYMBOL_UNITS = CLIENT_KEYS * SYMBOL_UNITS;
constexpr std::size_t INPUT_BYTES = 8192;
constexpr std::size_t MAX_FRAME_BYTES = 65536;
constexpr std::size_t FRAME_PREFIX_BYTES = 9;
constexpr std::size_t EVENT_BYTES = 8192;
constexpr std::size_t EVENT_OWNER_ALLOWANCE = 1024;
constexpr std::size_t SOURCE_EPOCH_BYTES = 4 * 1024 * 1024;
constexpr std::size_t REMOTE_EPOCH_BYTES = 20 * 1024 * 1024;
constexpr std::size_t REMOTE_EVENT_BYTES = 64 * 1024;
constexpr std::size_t REMOTE_DATA_RECORDS = 32;
constexpr std::size_t MIXED_OWNERS =
    MAX_PEERS * (CLIENT_KEYS * (HISTORY_POINTS_PER_KEY + 2) + CLIENT_PENDING_ITEMS + 1) + 12;
constexpr std::size_t MIXED_LOCAL_EPOCH_BYTES = MIXED_OWNERS * (EVENT_BYTES + EVENT_OWNER_ALLOWANCE);
constexpr std::size_t MIXED_REMOTE_EPOCH_BYTES = MIXED_OWNERS * (REMOTE_EVENT_BYTES + EVENT_OWNER_ALLOWANCE);
constexpr std::size_t QUOTE_FIELDS = 10;
constexpr std::size_t MAX_DATA_FIELDS = 16;
constexpr std::int32_t QUOTE_ID = 0;
constexpr std::int32_t SALE_ID = 1;
constexpr std::int32_t ORDER_ID = 2;
using Family = core::SubscriptionFamily;
using DataMessage = protocol::LiveDataMessage;
constexpr std::array FAMILIES{Family::TICKER, Family::STREAM, Family::HISTORY};
constexpr std::array DATA_MESSAGES{DataMessage::TICKER_DATA, DataMessage::STREAM_DATA, DataMessage::HISTORY_DATA};
constexpr std::array ADD_MESSAGES{protocol::SubscriptionMessage::TICKER_ADD, protocol::SubscriptionMessage::STREAM_ADD,
                                  protocol::SubscriptionMessage::HISTORY_ADD};
constexpr std::array REMOVE_MESSAGES{protocol::SubscriptionMessage::TICKER_REMOVE,
                                     protocol::SubscriptionMessage::STREAM_REMOVE,
                                     protocol::SubscriptionMessage::HISTORY_REMOVE};
constexpr std::array RECORD_IDS{QUOTE_ID, SALE_ID, ORDER_ID};
constexpr std::size_t FAMILY_COUNT = FAMILIES.size();
constexpr std::int32_t SEQUENCE_BITS = 22;
constexpr std::int32_t SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::chrono::milliseconds IO_CADENCE{2};
constexpr std::chrono::seconds HEARTBEAT_PERIOD{5};
constexpr std::chrono::seconds HANDSHAKE_TIMEOUT{10};
constexpr std::chrono::seconds WRITE_TIMEOUT{10};

constexpr std::array<std::u16string_view, QUOTE_FIELDS> QUOTE_FIELD_NAMES{
    u"Bid.Time",     u"Bid.Exchange", u"Bid.Price", u"Bid.Size", u"Ask.Time",
    u"Ask.Exchange", u"Ask.Price",    u"Ask.Size",  u"Sequence", u"TimeNanoPart"};
constexpr std::array<FieldType, QUOTE_FIELDS> QUOTE_FIELD_TYPES{
    FieldType::TIME_SECONDS, FieldType::UTF_CHAR,   FieldType::WIDE_DECIMAL, FieldType::WIDE_DECIMAL,
    FieldType::TIME_SECONDS, FieldType::UTF_CHAR,   FieldType::WIDE_DECIMAL, FieldType::WIDE_DECIMAL,
    FieldType::SEQUENCE,     FieldType::COMPACT_INT};

constexpr core::SchemaLimits SCHEMA_LIMITS{FAMILY_COUNT, 3 * MAX_DATA_FIELDS, 64, 64, 2048};
constexpr Diagnostic QUOTE_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-quote-shape"};
constexpr Diagnostic SALE_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-sale-shape"};
constexpr Diagnostic ORDER_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-order-shape"};
constexpr Diagnostic ALLOCATION{ErrorCode::ALLOCATION_FAILURE, "publisher-storage"};
constexpr Diagnostic CLOSED{ErrorCode::CLOSED, "publisher-closed"};
constexpr Diagnostic LOCAL_SOURCE_DISABLED{ErrorCode::INVALID_STATE, "publisher-remote-source"};
constexpr Diagnostic MIXED_DISABLED{ErrorCode::INVALID_STATE, "publisher-mixed-disabled"};

/** Copied input with bounded text and an exact ordered local field representation. */
struct SourceRow {
    std::array<char16_t, SYMBOL_UNITS> symbol;
    std::size_t length;
    std::array<std::int64_t, MAX_DATA_FIELDS> raw;
    std::size_t family;
    std::int32_t flags;
    std::optional<std::int64_t> removalTime;

    [[nodiscard]] std::u16string_view text() const noexcept {
        return {symbol.data(), length};
    }
};

SourceRow sourceRow(std::size_t family, std::u16string_view symbol, std::span<const std::int64_t> raw,
                    std::int32_t flags = 0, std::optional<std::int64_t> removalTime = {}) {
    SourceRow row{};
    row.family = family;
    row.length = symbol.size();
    row.flags = flags;
    row.removalTime = removalTime;
    std::ranges::copy(symbol, row.symbol.begin());
    std::ranges::copy(raw, row.raw.begin());

    return row;
}

std::optional<std::size_t> familyIndex(Family family) noexcept {
    const auto found = std::ranges::find(FAMILIES, family);

    if (found == FAMILIES.end())
        return {};

    return static_cast<std::size_t>(found - FAMILIES.begin());
}

std::optional<std::size_t> familyIndex(protocol::SubscriptionMessage message) noexcept {
    for (std::size_t index = 0; index < FAMILY_COUNT; ++index) {
        if (ADD_MESSAGES[index] == message || REMOVE_MESSAGES[index] == message)
            return index;
    }

    return {};
}

std::int32_t lowWord(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

std::int32_t highWord(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(value) >> 32));
}

std::int64_t floorSeconds(std::int64_t milliseconds) noexcept {
    return milliseconds / MILLIS_PER_SECOND - (milliseconds % MILLIS_PER_SECOND < 0);
}

bool exactSymbol(std::u16string_view symbol) noexcept {
    return !symbol.empty() && symbol.size() <= SYMBOL_UNITS && symbol.front() != u'*';
}

std::expected<SourceRow, Diagnostic> quoteRow(const events::Quote &quote) {
    if (!exactSymbol(quote.symbol) || quote.eventTime != 0 || quote.businessNumericSnapshot || quote.sequence < 0 ||
        quote.sequence > SEQUENCE_MASK || quote.bidTimeMs % MILLIS_PER_SECOND || quote.askTimeMs % MILLIS_PER_SECOND) {
        return std::unexpected(QUOTE_SHAPE);
    }

    const auto bid = floorSeconds(quote.bidTimeMs);
    const auto ask = floorSeconds(quote.askTimeMs);

    if (!std::in_range<std::int32_t>(bid) || !std::in_range<std::int32_t>(ask))
        return std::unexpected(QUOTE_SHAPE);

    const auto whole = std::max(quote.bidTimeMs, quote.askTimeMs);

    if (quote.timeMs < whole || quote.timeMs >= whole + MILLIS_PER_SECOND)
        return std::unexpected(QUOTE_SHAPE);

    const auto millis = quote.timeMs - whole;
    const auto sequence =
        (static_cast<std::uint32_t>(millis) << SEQUENCE_BITS) | static_cast<std::uint32_t>(quote.sequence);
    const std::array<std::int64_t, QUOTE_FIELDS> raw{bid,
                                                     quote.bidExchangeCode,
                                                     quote.bidPrice.raw,
                                                     quote.bidSize.raw,
                                                     ask,
                                                     quote.askExchangeCode,
                                                     quote.askPrice.raw,
                                                     quote.askSize.raw,
                                                     std::bit_cast<std::int32_t>(sequence),
                                                     quote.timeNanoPart};

    return sourceRow(0, quote.symbol, raw);
}

std::expected<SourceRow, Diagnostic> saleRow(const events::TimeAndSale &sale) {
    const auto conditions = events::encodeShortString(sale.exchangeSaleConditions);

    if (!exactSymbol(sale.symbol) || !conditions || sale.eventTime || sale.eventFlags || sale.buyer || sale.seller ||
        sale.businessNumericSnapshot)
        return std::unexpected(SALE_SHAPE);

    const std::array<std::int64_t, 11> raw{
        highWord(sale.index), lowWord(sale.index), sale.exchangeCode, sale.tradeId, sale.price.raw,   sale.size.raw,
        sale.bidPrice.raw,    sale.askPrice.raw,   *conditions,       sale.flags,   sale.timeNanoPart};

    return sourceRow(1, sale.symbol, raw);
}

std::expected<SourceRow, Diagnostic> orderRow(const events::Order &order) {
    using namespace events::indexed_event_flags;
    constexpr auto FLAGS = TX_PENDING | REMOVE_EVENT | SNAPSHOT_BEGIN | SNAPSHOT_END | SNAPSHOT_SNIP | SNAPSHOT_MODE;
    const auto maker = events::encodeShortString(order.marketMaker);

    if (!exactSymbol(order.symbol) || !maker || highWord(order.index) != 0 || lowWord(order.index) < 0 ||
        ((order.eventFlags & (SNAPSHOT_END | SNAPSHOT_SNIP)) != 0 && order.index != 0) ||
        !std::in_range<std::int32_t>(order.count) || order.eventTime || order.actionTime || order.orderId ||
        order.auxOrderId || order.tradeId || order.tradePrice.raw || order.tradeSize.raw || order.executedSize.raw ||
        order.businessNumericSnapshot || (order.eventFlags & ~FLAGS))
        return std::unexpected(ORDER_SHAPE);

    // A WideDecimal rank of 128 stores the signed integer without scaling.
    constexpr std::int64_t WIDE_DECIMAL_UNIT = 256;
    constexpr std::int64_t WIDE_DECIMAL_INTEGER_RANK = 128;
    const std::array<std::int64_t, 9> raw{lowWord(order.index),
                                          highWord(order.timeSequence),
                                          lowWord(order.timeSequence),
                                          order.price.raw,
                                          order.size.raw,
                                          order.count * WIDE_DECIMAL_UNIT + WIDE_DECIMAL_INTEGER_RANK,
                                          order.flags,
                                          *maker,
                                          order.timeNanoPart};
    const auto removalTime = (order.eventFlags & REMOVE_EVENT) != 0 ? std::optional{order.index} : std::nullopt;

    return sourceRow(2, order.symbol, raw, order.eventFlags, removalTime);
}

protocol::LiveWork liveTurn() noexcept {
    return {128, 8 * 1024 * 1024, 8 * 1024 * 1024, 16 * 1024 * 1024};
}

mux::QueueWork queueTurn() noexcept {
    constexpr std::size_t VISITS = 4 * 1024 * 1024;
    constexpr std::size_t BYTES = 64 * 1024 * 1024;

    return {512,
            VISITS,
            VISITS,
            VISITS,
            BYTES,
            128,
            128,
            {512, VISITS, VISITS, BYTES},
            {512, VISITS, VISITS},
            {512, VISITS, VISITS, BYTES},
            {512, VISITS, VISITS, BYTES},
            {512, VISITS, VISITS, BYTES}};
}

mux::QueueWork serverQueueTurn(std::size_t peers, bool remote, bool mixed = false) noexcept {
    auto work = queueTurn();
    const auto keys = peers * CLIENT_KEYS;
    // MuxState reserves against both possible maps and all input rows before a command.
    constexpr std::size_t INTEREST_PASSES = 32;
    const auto interestVisits = INTEREST_PASSES * (2 + peers + 2 * keys + CLIENT_KEYS) * (1 + keys + CLIENT_KEYS);
    work.interest.visits += interestVisits;
    work.interest.symbolUnits += interestVisits * SYMBOL_UNITS;
    // Cover TICKER maintenance, staging and cache owner release for every supported configuration.
    constexpr std::size_t TICKER_QUEUE_PASSES = 8;
    constexpr std::size_t TICKER_MEMBERSHIP_PASSES = 4 * (core::MAX_DELIVERY_PROJECTIONS + 1);
    const auto cacheScan = (keys + 4) * (keys + 1) * (peers + 1) * TICKER_QUEUE_PASSES;
    const auto membershipScan = keys * peers * keys * TICKER_MEMBERSHIP_PASSES;
    work.ticker.visits += cacheScan + membershipScan + 2 * keys * (keys + 1) + 8 * (keys + 1);
    work.ticker.textUnits += keys * (2 * SYMBOL_UNITS) * ((peers + 1) * keys + keys);
    // Each retained owner conservatively charges the whole shared source epoch on release.
    const auto epochBytes = mixed ? (remote ? MIXED_REMOTE_EPOCH_BYTES : MIXED_LOCAL_EPOCH_BYTES)
                                  : (remote ? REMOTE_EPOCH_BYTES : SOURCE_EPOCH_BYTES);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    work.ticker.bytes =
        keys + 2 > (maximum - work.ticker.bytes) / epochBytes ? maximum : work.ticker.bytes + (keys + 2) * epochBytes;

    if (mixed) {
        const auto historyPoints = keys * HISTORY_POINTS_PER_KEY;
        const auto frames = historyPoints + peers * (CLIENT_PENDING_ITEMS + 1) + 2;
        const auto rows = remote ? REMOTE_DATA_RECORDS : 1;
        const auto rowScan = std::uint64_t{64} * (keys + std::uint64_t{historyPoints} * historyPoints +
                                                  std::uint64_t{historyPoints + 1} * peers *
                                                      (keys + CLIENT_PENDING_ITEMS + frames + historyPoints + 4) +
                                                  MAX_DATA_FIELDS + 8);
        work.history.visits += 32 * (keys + 1) * (historyPoints + peers * (keys + CLIENT_PENDING_ITEMS) + 1);
        work.history.visits +=
            static_cast<std::size_t>(std::min(rowScan * rows, std::uint64_t{maximum - work.history.visits}));
        work.history.textUnits += 16 * (keys + 1) * (peers + 1) * keys * SYMBOL_UNITS;
        work.history.bytes =
            frames > (maximum - work.history.bytes) / epochBytes ? maximum : work.history.bytes + frames * epochBytes;
        work.stream.bytes = work.ticker.bytes;
    }

    return work;
}

protocol::LiveLimits liveLimits(bool mixed = false) noexcept {
    protocol::LiveLimits result{};
    result.frame = {MAX_FRAME_BYTES, MAX_FRAME_BYTES + FRAME_PREFIX_BYTES};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {MAX_FRAME_BYTES, 128, 128, 4096, {8192, 1024}, 65536};
    result.context = {SCHEMA_LIMITS, 128, {4096, 1024, 65536}};
    result.data = {MAX_FRAME_BYTES, 128,   4096,  128,   4096,  MAX_FRAME_BYTES, {8192, SYMBOL_UNITS, SYMBOL_UNITS},
                   {8192, 1024},    65536, 65536, 65536, 65536, 65536,           65536,
                   65536,           65536, 128};
    result.residentBytes = 8 * 1024 * 1024;
    result.owners = 16;
    result.snapshotBytes = 1024 * 1024;
    result.scratchBytes = 256 * 1024;
    result.eventBytes = EVENT_BYTES;
    result.outputBytes = MAX_FRAME_BYTES;
    result.credentials = protocol::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;

    if (mixed) {
        result.events = protocol::DataEventPolicy::BINARY_CATALOG_V1;
        result.historySnapshot = true;
    }

    return result;
}

Diagnostic nativeDiagnostic(transport::TcpListenerDiagnostic diagnostic) noexcept {
    auto code = ErrorCode::TRANSPORT_FAILURE;

    if (diagnostic.error == transport::TcpListenerError::INVALID_CONFIGURATION)
        code = ErrorCode::INVALID_ARGUMENT;
    else if (diagnostic.error == transport::TcpListenerError::RESOURCE_LIMIT)
        code = ErrorCode::RESOURCE_LIMIT;
    else if (diagnostic.error == transport::TcpListenerError::ALLOCATION_FAILURE)
        code = ErrorCode::ALLOCATION_FAILURE;

    return {code, diagnostic.stage, diagnostic.nativeCode, static_cast<std::uint32_t>(diagnostic.error)};
}

Diagnostic queueDiagnostic(mux::QueueFailure diagnostic, std::string_view stage) noexcept {
    auto code = ErrorCode::INVALID_STATE;

    if (diagnostic.error == mux::QueueError::RESOURCE_LIMIT)
        code = ErrorCode::RESOURCE_LIMIT;
    else if (diagnostic.error == mux::QueueError::ALLOCATION_FAILURE)
        code = ErrorCode::ALLOCATION_FAILURE;
    else if (diagnostic.error == mux::QueueError::WORK_LIMIT)
        code = ErrorCode::WORK_LIMIT;

    return {code, stage, 0, static_cast<std::uint32_t>(diagnostic.error)};
}

Diagnostic protocolDiagnostic(protocol::LiveError error, std::string_view stage) noexcept {
    auto code = ErrorCode::PROTOCOL_FAILURE;

    if (error == protocol::LiveError::RESOURCE_LIMIT)
        code = ErrorCode::RESOURCE_LIMIT;
    else if (error == protocol::LiveError::ALLOCATION_FAILURE)
        code = ErrorCode::ALLOCATION_FAILURE;
    else if (error == protocol::LiveError::WORK_LIMIT)
        code = ErrorCode::WORK_LIMIT;

    return {code, stage, 0, static_cast<std::uint32_t>(error)};
}

core::LocalSchema publisherSchema(bool mixed) {
    core::LocalRecord record{QUOTE_ID, u"Quote", false, {}, {}};

    for (std::size_t i = 0; i < QUOTE_FIELD_NAMES.size(); ++i) {
        record.fields.push_back({std::to_underlying(QUOTE_FIELD_TYPES[i])});
        record.lookups.push_back({std::u16string(QUOTE_FIELD_NAMES[i]), i});
    }

    std::vector records{std::move(record)};

    if (mixed) {
        auto sale = events::defaultTimeAndSaleSchema(SCHEMA_LIMITS);
        auto order = events::defaultOrderSchema(SCHEMA_LIMITS);

        if (!sale || !order)
            throw Error({ErrorCode::ALLOCATION_FAILURE, "publisher-indexed-schema"});

        records.push_back(sale->records().front());
        records.push_back(order->records().front());

        for (std::size_t i = 1; i < records.size(); ++i) {
            auto &indexed = records[i];
            indexed.id = RECORD_IDS[i];
            indexed.lookups.push_back({u"TimeNanoPart", indexed.fields.size()});
            indexed.fields.push_back({std::to_underlying(FieldType::COMPACT_INT)});
        }
    }

    auto schema = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema)
        throw Error({schema.error() == core::SchemaError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                             : ErrorCode::INVALID_STATE,
                     "publisher-schema", 0, static_cast<std::uint32_t>(schema.error())});

    return std::move(*schema);
}

protocol::SubscriptionWriter publisherWriter(const core::LocalSchema &schema) {
    std::vector<protocol::OutgoingRecord> outgoing;

    for (const auto &record : schema.records()) {
        auto &layout = outgoing.emplace_back(record.id, record.id, record.name, std::vector<protocol::OutgoingField>{});

        for (std::size_t i = 0; i < record.fields.size(); ++i) {
            const auto key = std::ranges::find(record.lookups, i, &core::FieldLookup::fieldId);

            if (key == record.lookups.end())
                throw Error({ErrorCode::INVALID_STATE, "publisher-writer-field"});

            layout.fields.push_back({key->name, record.fields[i].serialTypeId, static_cast<std::int32_t>(i)});
        }
    }

    protocol::SubscriptionWriterWork writerWork{64, 65536, 65536, 256 * 1024};
    auto writer = protocol::SubscriptionWriter::create(schema, outgoing,
                                                       {SCHEMA_LIMITS, FAMILY_COUNT, 3 * MAX_DATA_FIELDS, 64, 2048},
                                                       {MAX_FRAME_BYTES,
                                                        MAX_FRAME_BYTES + FRAME_PREFIX_BYTES,
                                                        128,
                                                        SYMBOL_UNITS,
                                                        {8192, 1024},
                                                        256 * 1024,
                                                        256 * 1024,
                                                        4},
                                                       writerWork);

    if (!writer)
        throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-writer", 0, static_cast<std::uint32_t>(writer.error())});

    return std::move(*writer);
}

protocol::LiveSession serverSession(const core::LocalSchema &schema, protocol::LiveWork &work) {
    auto writer = publisherWriter(schema);
    auto local = schema;
    auto session = protocol::LiveSession::createServer(
        std::move(local), std::move(writer), liveLimits(schema.records().size() > 1),
        {MAX_FRAME_BYTES, FAMILY_COUNT, 128, CLIENT_KEYS, SYMBOL_UNITS, CLIENT_SYMBOL_UNITS}, work);

    if (!session)
        throw Error(protocolDiagnostic(session.error(), "publisher-server-session"));

    return std::move(*session);
}

constexpr mux::ClientLimits CLIENT_LIMITS{
    CLIENT_PENDING_ITEMS, 512 * 1024,         8, 128 * 1024, 1, 32, SYMBOL_UNITS, 0, 0, 4096, 2, 4,
    CLIENT_KEYS,          CLIENT_SYMBOL_UNITS};

mux::MuxQueue publisherQueue(const core::LocalSchema &schema, std::size_t peers, bool remote, Family family) {
    const auto allKeys = peers * CLIENT_KEYS;
    const auto allUnits = peers * CLIENT_LIMITS.commandSymbolUnits;
    const auto records = schema.records().size();
    const core::SubscriptionLimits desired{records,     allKeys,      SYMBOL_UNITS, allUnits,
                                           allKeys * 2, allUnits * 2, allKeys};
    const mux::MuxLimits interests{records,      peers,    allKeys,     allKeys,
                                   SYMBOL_UNITS, allUnits, CLIENT_KEYS, 8 * 1024 * 1024};
    const mux::QueueLimits limits{peers,
                                  remote ? REMOTE_DATA_RECORDS : 1,
                                  1,
                                  1,
                                  8,
                                  peers * CLIENT_LIMITS.dataItems,
                                  peers * CLIENT_LIMITS.recordBytes,
                                  peers * CLIENT_LIMITS.batchOwners,
                                  peers * CLIENT_LIMITS.batchBytes,
                                  peers * CLIENT_LIMITS.commandSlots,
                                  peers * CLIENT_LIMITS.resultSlots,
                                  peers * 2,
                                  32 * 1024 * 1024};
    const auto cacheFrames = family == Family::HISTORY ? allKeys * HISTORY_POINTS_PER_KEY + 1 : allKeys + 1;
    const auto eventBytes = (remote ? REMOTE_EVENT_BYTES : EVENT_BYTES) + EVENT_OWNER_ALLOWANCE;
    const auto cacheBytes = cacheFrames * eventBytes;
    const collectors::TickerLimits ticker{peers,  allKeys, cacheFrames, cacheBytes, cacheBytes + 32 * 1024 * 1024,
                                          desired};
    auto work = queueTurn();
    auto queue = [&] {
        switch (family) {
        case Family::TICKER:
            return mux::MuxQueue::createTicker(schema, interests, ticker, {false, 100}, limits, work);
        case Family::STREAM:
            return mux::MuxQueue::createStream(
                schema, interests, desired,
                {cacheFrames, cacheFrames * REMOTE_DATA_RECORDS, cacheBytes, cacheBytes + 32 * 1024 * 1024}, limits,
                work);
        case Family::HISTORY:
            return mux::MuxQueue::createHistory(
                schema, interests,
                {allKeys, allKeys * HISTORY_POINTS_PER_KEY, SYMBOL_UNITS, cacheFrames, cacheBytes, peers,
                 CLIENT_LIMITS.dataItems, CLIENT_LIMITS.dataItems + 1, (CLIENT_LIMITS.dataItems + 1) * eventBytes,
                 cacheBytes + peers * (CLIENT_LIMITS.dataItems + 1) * eventBytes + 32 * 1024 * 1024, desired},
                {false, 100, 10000}, limits, work);
        }

        return std::expected<mux::MuxQueue, mux::QueueFailure>{
            std::unexpected(mux::QueueFailure{mux::QueueError::INVALID_CONFIGURATION, {}})};
    }();

    if (!queue)
        throw Error(queueDiagnostic(queue.error(), "publisher-collector"));

    return std::move(*queue);
}

protocol::detail::LocalEventOwner localOwner(bool mixed) {
    auto work = liveTurn();
    protocol::LiveLimits limits{};
    limits.residentBytes = mixed ? MIXED_LOCAL_EPOCH_BYTES : SOURCE_EPOCH_BYTES;
    limits.owners = mixed ? MIXED_OWNERS : MAX_PEERS * CLIENT_KEYS + 4;
    limits.eventBytes = EVENT_BYTES;
    auto owner = protocol::detail::LocalEventOwner::create(limits, work);

    if (!owner)
        throw Error(protocolDiagnostic(owner.error(), "publisher-source"));

    return std::move(*owner);
}

/** The worker retains one callback batch while the corresponding wire frame is partially written. */
struct Delivery final : mux::MuxConsumer {
    mux::OwnedMuxBatch batch;

    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &view) override {
        batch = view.retain();

        return mux::BatchAction::PAUSE;
    }
};

// Front-loading control frames avoids consuming data queue reservations during negotiation.
enum class OutputKind { PROTOCOL, DESCRIPTION, HEARTBEAT, DATA };

struct Peer {
    transport::TcpConnectionId id;
    protocol::LiveSession session;
    std::array<std::shared_ptr<Delivery>, FAMILY_COUNT> delivery;
    std::array<mux::MuxHandle, FAMILY_COUNT> handles;
    std::optional<mux::MuxCommandTicket> command;
    std::optional<protocol::LivePendingOutput> output;
    Clock::time_point acceptedAt;
    Clock::time_point lastWrite;
    Clock::time_point heartbeatAt;
    OutputKind outputKind = OutputKind::PROTOCOL;
    std::size_t outputFamily = 0;
    std::size_t nextFamily = 0;
    bool sentProtocol = false;
    bool sentDescription = false;
    std::array<bool, FAMILY_COUNT> ready{};
    bool closing = false;
    bool socketClosed = false;
    std::array<std::byte, INPUT_BYTES> input;
    std::size_t inputStart = 0;
    std::size_t inputEnd = 0;
};

constexpr std::array<protocol::LiveMessageView, 6> SERVER_SEND{{{0, u"HEARTBEAT", {}},
                                                                {1, u"DESCRIBE_PROTOCOL", {}},
                                                                {2, u"DESCRIBE_RECORDS", {}},
                                                                {10, u"TICKER_DATA", {}},
                                                                {15, u"STREAM_DATA", {}},
                                                                {20, u"HISTORY_DATA", {}}}};
constexpr std::array<protocol::LiveMessageView, 9> SERVER_RECEIVE{{{0, u"HEARTBEAT", {}},
                                                                   {1, u"DESCRIBE_PROTOCOL", {}},
                                                                   {2, u"DESCRIBE_RECORDS", {}},
                                                                   {11, u"TICKER_ADD_SUBSCRIPTION", {}},
                                                                   {12, u"TICKER_REMOVE_SUBSCRIPTION", {}},
                                                                   {16, u"STREAM_ADD_SUBSCRIPTION", {}},
                                                                   {17, u"STREAM_REMOVE_SUBSCRIPTION", {}},
                                                                   {21, u"HISTORY_ADD_SUBSCRIPTION", {}},
                                                                   {22, u"HISTORY_REMOVE_SUBSCRIPTION", {}}}};
constexpr protocol::LivePropertyView HISTORY_OPTIONS[]{{u"opt", u"hs"}};

std::expected<protocol::LivePendingOutput, protocol::LiveError> composeDelivery(Peer &peer, std::size_t family,
                                                                                protocol::LiveWork &work) {
    const auto &record = peer.delivery[family]->batch.records().front();

    if (record.typedValues.size() > MAX_DATA_FIELDS)
        return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);

    const bool removed = (record.eventFlags & events::indexed_event_flags::REMOVE_EVENT) != 0;
    const protocol::LiveDataRecordView outgoing{RECORD_IDS[family], record.symbol,
                                                removed ? std::span<const core::FieldValueView>{} : record.typedValues,
                                                record.eventFlags, removed ? record.rawTime : std::nullopt};

    return peer.session.publishData(DATA_MESSAGES[family], std::span(&outgoing, 1), work);
}

} // namespace

namespace detail {
/** Detached bounded producer state. It contains no worker or native owner. */
struct PublisherPort {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<SourceRow> pending;
    std::array<std::vector<std::u16string>, FAMILY_COUNT> demand;
    const bool remote;
    const bool mixed;
    PublisherSourceStatus source;
    PublisherStatistics statistics;
    std::size_t head = 0;
    std::size_t size = 0;
    std::uint16_t boundPort = 0;
    bool started = false;
    bool closed = false;
    Diagnostic diagnostic;

    PublisherPort(std::size_t count, bool remote, bool mixed)
        : pending(remote ? 0 : count), remote(remote), mixed(mixed) {
        if (!mixed) {
            for (std::size_t family = 1; family < FAMILY_COUNT; ++family) {
                auto &stats = statistics.families[family];
                stats.ingressFrames.availability = core::StatisticAvailability::UNSUPPORTED;
                stats.ingressRecords.availability = core::StatisticAvailability::UNSUPPORTED;
                stats.sentFrames.availability = core::StatisticAvailability::UNSUPPORTED;
            }
        }
    }
};
} // namespace detail

namespace {
constexpr protocol::LivePropertyView SOURCE_PROPERTIES[]{{u"type", u"qtp"}};
constexpr protocol::LivePropertyView MIXED_SOURCE_PROPERTIES[]{{u"type", u"qtp"}, {u"opt", u"hs"}};

Diagnostic sourceDiagnostic(transport::TcpDiagnostic diagnostic) noexcept {
    return {ErrorCode::TRANSPORT_FAILURE, diagnostic.stage, diagnostic.nativeCode,
            static_cast<std::uint32_t>(diagnostic.error)};
}

bool hasCapability(std::span<const protocol::LiveMessageView> messages, const protocol::LiveMessageView &expected) {
    return std::ranges::any_of(messages, [&](const auto &message) {
        return message.id == expected.id && message.name == expected.name;
    });
}

using QueueSet = std::array<std::optional<mux::MuxQueue>, FAMILY_COUNT>;
using DemandSet = std::array<std::optional<mux::DemandTracker>, FAMILY_COUNT>;
using WorkSet = std::array<mux::QueueWork, FAMILY_COUNT>;

/** One upstream connection. Its raw output slot copies and owns the whole frame through the local write barrier. */
class SourceConnection {
    std::array<core::RecordContext, FAMILY_COUNT> contexts_;
    protocol::LiveSession session_;
    std::unique_ptr<transport::detail::RawByteOwner> bytes_;
    Clock::time_point handshakeDeadline_;
    Clock::time_point heartbeatAt_;
    std::optional<std::pair<std::size_t, std::uint64_t>> pendingDemand_;
    std::size_t familyCount_;
    std::size_t nextFamily_ = 0;
    bool described_ = false;
    bool ready_ = false;
    std::optional<protocol::LiveEvent> pendingEvent_;

    static protocol::LiveSession newSession(const core::LocalSchema &schema, protocol::LiveWork &work) {
        auto writer = publisherWriter(schema);
        const bool mixed = schema.records().size() > 1;
        auto limits = liveLimits(mixed);
        limits.residentBytes = mixed ? MIXED_REMOTE_EPOCH_BYTES : REMOTE_EPOCH_BYTES;
        limits.eventBytes = REMOTE_EVENT_BYTES;
        limits.owners = mixed ? MIXED_OWNERS : MAX_PEERS * CLIENT_KEYS + 12;
        limits.data.emittedRecords = REMOTE_DATA_RECORDS;
        auto local = schema;
        auto session = protocol::LiveSession::create(std::move(local), std::move(writer), limits, work);

        if (!session)
            throw Error(protocolDiagnostic(session.error(), "publisher-upstream-session"));

        return std::move(*session);
    }

    static std::unique_ptr<transport::detail::RawByteOwner>
    newTransport(const transport::detail::BootstrapLease &bootstrap, PublisherSourceOptions options,
                 protocol::LiveWork &work) {
        constexpr transport::TcpLimits LIMITS{INPUT_BYTES, INPUT_BYTES, 1, 1, 65536, 8192, 8 * 1024 * 1024};
        const transport::TcpOptions tcp{
            options.address,      options.port,  {},         53,    HANDSHAKE_TIMEOUT, HANDSHAKE_TIMEOUT,
            HEARTBEAT_PERIOD * 6, WRITE_TIMEOUT, IO_CADENCE, LIMITS};
        auto owner = transport::detail::RawByteOwner::create(bootstrap, tcp,
                                                             {INPUT_BYTES, MAX_FRAME_BYTES + FRAME_PREFIX_BYTES}, work);

        if (!owner)
            throw Error(sourceDiagnostic(owner.error()));

        return std::move(*owner);
    }

    void send(std::expected<protocol::LivePendingOutput, protocol::LiveError> output, protocol::LiveWork &work) {
        if (!output)
            throw Error(protocolDiagnostic(output.error(), "publisher-upstream-output"));

        const auto written = bytes_->write(output->bytes(), work);

        if (!written)
            throw Error(sourceDiagnostic(written.error()));
    }

    void receive(protocol::LiveWork &work) {
        const auto input = bytes_->input();

        if (!input)
            throw Error(sourceDiagnostic(input.error()));

        if (input->empty())
            return;

        auto incoming = session_.feed(*input, work);
        const auto consumed = bytes_->consumeInput(incoming.accepted);

        if (!consumed)
            throw Error(sourceDiagnostic(consumed.error()));

        if (incoming.status == protocol::LiveStatus::FAILED || !incoming.accepted)
            throw Error(protocolDiagnostic(session_.diagnostic().error, session_.diagnostic().stage));

        if (!incoming.event)
            return;

        if (incoming.event->kind() == protocol::LiveEventKind::PROTOCOL) {
            const auto peer = incoming.event->snapshot();
            const auto descriptor = peer.protocol();
            ready_ = session_.authentication() != protocol::LiveAuthentication::REQUIRED;

            for (std::size_t family = 0; ready_ && family < familyCount_; ++family) {
                const auto capability = 3 + family;
                ready_ = hasCapability(descriptor.send, SERVER_SEND[capability]) &&
                         hasCapability(descriptor.receive, SERVER_RECEIVE[3 + family * 2]) &&
                         hasCapability(descriptor.receive, SERVER_RECEIVE[4 + family * 2]);
            }

            ready_ = ready_ && (familyCount_ == 1 || session_.historySnapshotEnabled());

            return;
        }

        if (incoming.event->kind() == protocol::LiveEventKind::DATA) {
            const auto id = incoming.event->messageType();
            const auto messages = std::span(DATA_MESSAGES).first(familyCount_);
            const auto selected = id ? std::ranges::find(messages, static_cast<DataMessage>(*id)) : messages.end();

            if (!ready_ || selected == messages.end())
                throw Error({ErrorCode::PROTOCOL_FAILURE, "publisher-upstream-data"});

            const auto family = static_cast<std::size_t>(selected - messages.begin());
            const auto records = incoming.event->records();

            if (std::ranges::any_of(records, [&](const auto &record) {
                    return !record.context.matches(contexts_[family]);
                }))
                throw Error({ErrorCode::PROTOCOL_FAILURE, "publisher-upstream-record-family"});

            if (!records.empty())
                pendingEvent_ = std::move(incoming.event);
        }
    }

    public:
    SourceConnection(const core::LocalSchema &schema, const transport::detail::BootstrapLease &bootstrap,
                     PublisherSourceOptions options, Clock::time_point now, protocol::LiveWork &work)
        : session_(newSession(schema, work)), bytes_(newTransport(bootstrap, options, work)),
          handshakeDeadline_(now + HANDSHAKE_TIMEOUT), heartbeatAt_(now + HEARTBEAT_PERIOD),
          familyCount_(schema.records().size()) {
        for (std::size_t family = 0; family < familyCount_; ++family)
            contexts_[family] = schema.recordContext(RECORD_IDS[family]);

        const auto started = bytes_->start();

        if (!started)
            throw Error(sourceDiagnostic(started.error()));

        const auto properties = familyCount_ == 1
                                    ? std::span<const protocol::LivePropertyView>(SOURCE_PROPERTIES)
                                    : std::span<const protocol::LivePropertyView>(MIXED_SOURCE_PROPERTIES);
        send(session_.composeProtocol({properties, std::span(SERVER_RECEIVE).first(3 + 2 * familyCount_),
                                       std::span(SERVER_SEND).first(3 + familyCount_)},
                                      work),
             work);
    }

    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    void progress(QueueSet &queues, DemandSet &demand, Clock::time_point now, WorkSet &queueWork,
                  mux::DemandWork &demandWork, PublisherStatistics &statistics) {
        auto work = liveTurn();
        transport::TcpWork native{16, 16, INPUT_BYTES * 2, 0, 0, 0, 0};
        const auto polled = bytes_->poll(native, work);

        if (!polled)
            throw Error(sourceDiagnostic(polled.error()));

        if (const auto terminal = bytes_->core().terminal()) {
            if (terminal->primary.error != transport::TcpError::NONE)
                throw Error(sourceDiagnostic(terminal->primary));

            throw Error({ErrorCode::TRANSPORT_FAILURE, "publisher-upstream-peer-end"});
        }

        if (bytes_->core().state() == transport::TcpState::CLOSING)
            return;

        const auto wasReady = ready_;

        if (!pendingEvent_)
            receive(work);

        if (!ready_ && (wasReady || now >= handshakeDeadline_))
            throw Error({ErrorCode::PROTOCOL_FAILURE, "publisher-upstream-negotiation"});

        if (pendingEvent_) {
            const auto type = static_cast<DataMessage>(*pendingEvent_->messageType());
            const auto family =
                static_cast<std::size_t>(std::ranges::find(DATA_MESSAGES, type) - DATA_MESSAGES.begin());
            const auto records = pendingEvent_->records().size();
            const auto admitted = queues[family]->admit(std::move(*pendingEvent_), queueWork[family]);

            if (admitted) {
                statistics.families[family].ingressFrames.add(1);
                statistics.families[family].ingressRecords.add(records);
                pendingEvent_.reset();
            } else if (admitted.error().error != mux::QueueError::BUSY &&
                       admitted.error().error != mux::QueueError::WORK_LIMIT)
                throw Error(queueDiagnostic(admitted.error(), "publisher-upstream-admit"));
        }

        if (bytes_->pendingOutput())
            return;

        if (pendingDemand_) {
            const auto [family, sequence] = *pendingDemand_;

            if (!demand[family]->acknowledge(sequence))
                throw Error({ErrorCode::INVALID_STATE, "publisher-upstream-demand-ack"});

            pendingDemand_.reset();
        }

        if (ready_ && !described_) {
            send(session_.describe(std::span(RECORD_IDS).first(familyCount_), work), work);
            described_ = true;

            return;
        }

        if (ready_) {
            for (std::size_t i = 0; i < familyCount_; ++i) {
                const auto family = (nextFamily_ + i) % familyCount_;
                const auto next = demand[family]->next(queues[family]->aggregateEntries(), demandWork);

                if (!next)
                    throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-upstream-demand", 0,
                                 static_cast<std::uint32_t>(next.error())});

                if (*next) {
                    const auto message =
                        (*next)->action == mux::DemandAction::ADD ? ADD_MESSAGES[family] : REMOVE_MESSAGES[family];
                    send(session_.subscribe(message, std::span(&(*next)->request, 1), work), work);
                    pendingDemand_.emplace(family, (*next)->sequence);
                    nextFamily_ = (family + 1) % familyCount_;

                    return;
                }
            }
        }

        if (now >= heartbeatAt_) {
            send(session_.composeHeartbeat({false, {}, {}, {}, {}}, work), work);
            heartbeatAt_ = now + HEARTBEAT_PERIOD;
        }
    }
};

class PublisherServer {
    core::LocalSchema schema_;
    std::optional<protocol::detail::LocalEventOwner> source_;
    QueueSet queues_;
    transport::TcpListener listener_;
    std::vector<std::optional<Peer>> peers_;
    std::optional<protocol::LiveEvent> pendingSource_;
    std::string sourceAddress_;
    std::optional<PublisherSourceOptions> sourceOptions_;
    std::optional<transport::detail::BootstrapLease> sourceBootstrap_;
    DemandSet sourceDemand_;
    std::optional<SourceConnection> upstream_;
    Clock::time_point retryAt_{};
    bool resetPending_ = false;
    PublisherSourceStatus sourceStatus_;
    std::array<std::uint64_t, FAMILY_COUNT> publishedRevision_{};
    PublisherStatistics statistics_;
    const std::size_t familyCount_;

    static transport::TcpListener listen(transport::RuntimeBootstrap &bootstrap,
                                         transport::TcpListenerOptions options) {
        auto listener = transport::TcpListener::create(bootstrap, options);

        if (!listener)
            throw Error(nativeDiagnostic(listener.error()));

        return std::move(*listener);
    }

    void acceptPeer() {
        auto vacant = std::ranges::find_if(peers_, [](const auto &entry) {
            return !entry;
        });

        if (vacant == peers_.end())
            return;

        const auto accepted = listener_.accept();

        if (!accepted)
            throw Error(nativeDiagnostic(accepted.error()));

        if (!*accepted)
            return;

        try {
            auto liveWork = liveTurn();
            auto session = serverSession(schema_, liveWork);
            const auto now = Clock::now();
            Peer peer{**accepted, std::move(session), {}, {}, {}, {}, now, now, now + HEARTBEAT_PERIOD};

            for (std::size_t family = 0; family < familyCount_; ++family) {
                auto work = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), familyCount_ > 1);
                peer.delivery[family] = std::make_shared<Delivery>();
                auto handle = queues_[family]->open(CLIENT_LIMITS, peer.delivery[family], work);

                if (!handle)
                    throw Error(queueDiagnostic(handle.error(), "publisher-open-client"));

                peer.handles[family] = std::move(*handle);
            }

            vacant->emplace(std::move(peer));
        } catch (...) {
            const auto result = listener_.retire(**accepted);

            if (!result)
                throw Error(nativeDiagnostic(result.error()));
        }
    }

    void retire(Peer &peer) noexcept {
        if (!peer.closing) {
            statistics_.retiredPeers.add(1);
            peer.closing = true;
            peer.output.reset();
            peer.command.reset();

            for (std::size_t family = 0; family < familyCount_; ++family) {
                peer.delivery[family]->batch = {};
                (void)peer.handles[family].requestStop(mux::StopMode::ABORT);
            }
        }

        if (!peer.socketClosed)
            peer.socketClosed = listener_.retire(peer.id).has_value();
    }

    bool receive(Peer &peer, protocol::LiveWork &work) {
        if (peer.command) {
            const auto applied = peer.command->result();

            if (!applied)
                return true;

            if (applied->status != mux::CommandStatus::APPLIED)
                return false;

            peer.command.reset();
        }

        if (peer.inputStart == peer.inputEnd) {
            const auto read = listener_.read(peer.id, peer.input);

            if (!read || read->status == transport::TcpIoStatus::PEER_EOF)
                return false;

            if (read->status == transport::TcpIoStatus::WOULD_BLOCK)
                return true;

            peer.inputStart = 0;
            peer.inputEnd = read->bytes;
        }

        const auto bytes = std::span(peer.input).subspan(peer.inputStart, peer.inputEnd - peer.inputStart);
        auto result = peer.session.feedServer(bytes, work);
        peer.inputStart += result.accepted;

        if (result.status == protocol::LiveStatus::FAILED || result.accepted == 0)
            return false;

        if (result.event) {
            if (result.event->kind() == protocol::LiveEventKind::DATA)
                return false;

            if (result.event->kind() == protocol::LiveEventKind::PROTOCOL) {
                const auto descriptor = result.event->snapshot();

                for (std::size_t family = 0; family < familyCount_; ++family)
                    peer.ready[family] = hasCapability(descriptor.protocol().receive, SERVER_SEND[3 + family]);
            }
        }

        if (result.subscription) {
            const auto family = familyIndex(result.subscription->message);

            if (!family || *family >= familyCount_ || !peer.ready[*family] ||
                (*family == 2 && !peer.session.historySnapshotEnabled()))
                return false;

            // The QTP record map covers every enabled family. A command cannot install a key from another family.
            const auto context = schema_.recordContext(RECORD_IDS[*family]);
            std::erase_if(result.subscription->entries, [&](const auto &entry) {
                return !entry.context.matches(context);
            });

            mux::QueueSubmitWork submit{8, 65536, 65536, 1024 * 1024};
            auto ticket =
                queues_[*family]->submitIncomingSubscription(peer.handles[*family], *result.subscription, submit);

            if (!ticket)
                return false;

            peer.command = std::move(*ticket);
        }

        return true;
    }

    bool enqueueOutput(Peer &peer, Clock::time_point now, protocol::LiveWork &work) {
        if (peer.output)
            return true;

        auto kind = OutputKind::HEARTBEAT;
        std::expected<protocol::LivePendingOutput, protocol::LiveError> output =
            std::unexpected(protocol::LiveError::NONE);

        if (!peer.sentProtocol) {
            kind = OutputKind::PROTOCOL;
            const auto properties = familyCount_ == 1 ? std::span<const protocol::LivePropertyView>{}
                                                      : std::span<const protocol::LivePropertyView>(HISTORY_OPTIONS);
            output = peer.session.composeProtocol({properties, std::span(SERVER_SEND).first(3 + familyCount_),
                                                   std::span(SERVER_RECEIVE).first(3 + 2 * familyCount_)},
                                                  work);
        } else if (!peer.sentDescription && std::ranges::any_of(peer.ready, std::identity{})) {
            kind = OutputKind::DESCRIPTION;
            output = peer.session.describe(std::span(RECORD_IDS).first(familyCount_), work);
        } else {
            if (peer.sentDescription) {
                for (std::size_t i = 0; i < familyCount_; ++i) {
                    const auto family = (peer.nextFamily + i) % familyCount_;

                    if (!peer.delivery[family]->batch.records().empty()) {
                        kind = OutputKind::DATA;
                        peer.outputFamily = family;
                        peer.nextFamily = (family + 1) % familyCount_;
                        output = composeDelivery(peer, family, work);
                        break;
                    }
                }
            }

            if (kind == OutputKind::HEARTBEAT) {
                if (now < peer.heartbeatAt)
                    return true;

                output = peer.session.composeHeartbeat({false, {}, {}, {}, {}}, work);
            }
        }

        if (!output)
            return false;

        peer.outputKind = kind;
        peer.output = std::move(*output);
        peer.lastWrite = now;

        return true;
    }

    bool send(Peer &peer, Clock::time_point now, protocol::LiveWork &work) {
        if (!enqueueOutput(peer, now, work))
            return false;

        if (!peer.output)
            return true;

        const auto written = listener_.write(peer.id, peer.output->pendingBytes());

        if (!written)
            return false;

        if (written->status == transport::TcpIoStatus::WOULD_BLOCK)
            return now - peer.lastWrite < WRITE_TIMEOUT;

        peer.lastWrite = now;

        if (!peer.output->consume(written->bytes))
            return false;

        if (!peer.output->pendingBytes().empty())
            return true;

        peer.output.reset();

        switch (peer.outputKind) {
        case OutputKind::PROTOCOL:
            peer.sentProtocol = true;
            break;
        case OutputKind::DESCRIPTION:
            peer.sentDescription = true;
            break;
        case OutputKind::DATA:
            statistics_.families[peer.outputFamily].sentFrames.add(1);
            peer.delivery[peer.outputFamily]->batch = {};
            (void)peer.handles[peer.outputFamily].requestResume();
            break;
        case OutputKind::HEARTBEAT:
            break;
        }

        peer.heartbeatAt = now + HEARTBEAT_PERIOD;

        return true;
    }

    void progressPeers() {
        const auto now = Clock::now();

        for (auto &entry : peers_) {
            if (!entry)
                continue;

            auto &peer = *entry;
            auto work = liveTurn();

            const auto handles = std::span(peer.handles).first(familyCount_);

            for (auto &handle : handles) {
                if (const auto notice = handle.resetNotice(); notice && !notice->globalAckPending)
                    (void)handle.acknowledgeReset(notice->sequence);
            }

            if (peer.closing ||
                std::ranges::any_of(handles,
                                    [](const auto &handle) {
                                        return handle.terminal().has_value();
                                    }) ||
                (!std::ranges::any_of(peer.ready, std::identity{}) && now - peer.acceptedAt >= HANDSHAKE_TIMEOUT) ||
                !receive(peer, work) || !send(peer, now, work)) {
                retire(peer);

                if (peer.socketClosed && std::ranges::all_of(handles, [](const auto &handle) {
                        return handle.terminal().has_value();
                    }))
                    entry.reset();
            }
        }
    }

    void publishDemand(detail::PublisherPort &port, std::size_t family) {
        const auto &queue = *queues_[family];
        const auto revision = queue.revision();

        if (publishedRevision_[family] == revision)
            return;

        const auto entries = queue.aggregateEntries();
        std::vector<std::u16string> symbols;
        symbols.reserve(entries.size());

        for (const auto &entry : entries)
            symbols.emplace_back(entry.key.symbol);

        {
            const std::lock_guard lock(port.mutex);
            port.demand[family] = std::move(symbols);
        }

        publishedRevision_[family] = revision;
        port.changed.notify_all();
    }

    void publishLocal(detail::PublisherPort &port, WorkSet &work) {
        if (!pendingSource_) {
            SourceRow row;
            {
                const std::lock_guard lock(port.mutex);

                if (port.size == 0 || port.closed)
                    return;

                row = port.pending[port.head];
                port.head = (port.head + 1) % port.pending.size();
                --port.size;
            }
            auto liveWork = liveTurn();
            const auto id = RECORD_IDS[row.family];
            const auto layout = schema_.records()[static_cast<std::size_t>(id)].fields;
            std::array<core::FieldValue, MAX_DATA_FIELDS> fields{};

            for (std::size_t i = 0; i < layout.size(); ++i)
                fields[i] = {static_cast<FieldType>(layout[i].serialTypeId), row.raw[i]};

            auto event =
                source_->record(DATA_MESSAGES[row.family], id, schema_.recordContext(id), row.text(),
                                std::span(fields).first(layout.size()), row.flags, row.removalTime, {}, liveWork);

            if (!event)
                throw Error(protocolDiagnostic(event.error(), "publisher-local-event"));

            pendingSource_ = std::move(*event);
        }

        const auto message = static_cast<DataMessage>(*pendingSource_->messageType());
        const auto family = static_cast<std::size_t>(std::ranges::find(DATA_MESSAGES, message) - DATA_MESSAGES.begin());
        const auto records = pendingSource_->records().size();
        const auto admitted = queues_[family]->admit(std::move(*pendingSource_), work[family]);

        if (admitted) {
            statistics_.families[family].ingressFrames.add(1);
            statistics_.families[family].ingressRecords.add(records);
            pendingSource_.reset();
        } else if (admitted.error().error != mux::QueueError::BUSY &&
                   admitted.error().error != mux::QueueError::WORK_LIMIT)
            throw Error(queueDiagnostic(admitted.error(), "publisher-admit"));
    }

    void publishSourceStatus(detail::PublisherPort &port) {
        {
            const std::lock_guard lock(port.mutex);
            port.source = sourceStatus_;
        }
        port.changed.notify_all();
    }

    void fenceSource() noexcept {
        for (auto &entry : peers_) {
            if (!entry || entry->closing)
                continue;

            auto &peer = *entry;

            if (peer.output && peer.outputKind == OutputKind::DATA) {
                if (peer.output->pendingBytes().size() != peer.output->bytes().size()) {
                    // The accepted prefix cannot be recalled or followed by a different envelope.
                    retire(peer);
                    continue;
                }

                peer.output.reset();
            }

            for (std::size_t family = 0; family < familyCount_; ++family) {
                if (!peer.delivery[family]->batch.records().empty()) {
                    peer.delivery[family]->batch = {};
                    (void)peer.handles[family].requestResume();
                }
            }
        }
    }

    void progressSource(detail::PublisherPort &port, WorkSet &work) {
        const auto now = Clock::now();
        const auto keys = peers_.size() * CLIENT_KEYS;
        // One validation and search pass over every pair in each of the two finite demand maps.
        const auto comparisons = 4 * familyCount_ * (keys + 1) * (keys + 1);
        mux::DemandWork demandWork{2 * familyCount_, comparisons, comparisons * SYMBOL_UNITS};

        if (resetPending_) {
            for (std::size_t family = 0; family < familyCount_; ++family) {
                const auto reset = queues_[family]->resetStatus();

                if (reset.failure)
                    throw Error(queueDiagnostic(*reset.failure, "publisher-upstream-reset"));

                if (reset.phase != mux::ResetPhase::NONE || !reset.globalAcknowledged)
                    return;
            }

            resetPending_ = false;
        }

        try {
            if (!upstream_) {
                if (now < retryAt_)
                    return;

                auto liveWork = liveTurn();
                upstream_.emplace(schema_, *sourceBootstrap_, *sourceOptions_, now, liveWork);
            }

            upstream_->progress(queues_, sourceDemand_, now, work, demandWork, statistics_);
        } catch (const Error &failure) {
            if (sourceStatus_.connected || (upstream_ && upstream_->ready())) {
                fenceSource();

                for (std::size_t family = 0; family < familyCount_; ++family) {
                    auto resetWork = serverQueueTurn(peers_.size(), true, familyCount_ > 1);
                    const auto reset = queues_[family]->resetSource(resetWork);

                    if (!reset)
                        throw Error(queueDiagnostic(reset.error(), "publisher-upstream-reset"));
                }

                resetPending_ = true;
            }

            upstream_.reset();

            for (std::size_t family = 0; family < familyCount_; ++family) {
                auto resetWork = mux::DemandWork{1, 2 * keys, 0};

                if (!sourceDemand_[family]->reset(resetWork))
                    throw Error({ErrorCode::INVALID_STATE, "publisher-upstream-demand-reset"});
            }

            retryAt_ = now + sourceOptions_->retryDelay;
            sourceStatus_.connected = false;
            sourceStatus_.diagnostic = failure.diagnostic();
            publishSourceStatus(port);

            return;
        }

        if (upstream_->ready() && !sourceStatus_.connected) {
            if (sourceStatus_.generation == std::numeric_limits<std::uint64_t>::max())
                throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-upstream-generation"});

            ++sourceStatus_.generation;
            sourceStatus_.connected = true;
            sourceStatus_.diagnostic = {};
            publishSourceStatus(port);
        }
    }

    public:
    PublisherServer(transport::RuntimeBootstrap &bootstrap, PublisherOptions options)
        : schema_(publisherSchema(options.mixed)), listener_(listen(bootstrap, options.listener)),
          peers_(options.listener.maxConnections), familyCount_(schema_.records().size()) {
        for (std::size_t family = familyCount_; family < FAMILY_COUNT; ++family) {
            auto &stats = statistics_.families[family];
            stats.ingressFrames.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.ingressRecords.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.sentFrames.availability = core::StatisticAvailability::UNSUPPORTED;
        }

        for (std::size_t family = 0; family < familyCount_; ++family)
            queues_[family].emplace(
                publisherQueue(schema_, peers_.size(), options.source.has_value(), FAMILIES[family]));

        if (!options.source) {
            source_.emplace(localOwner(options.mixed));

            return;
        }

        sourceAddress_ = options.source->address;
        sourceOptions_ = *options.source;
        sourceOptions_->address = sourceAddress_;
        auto lease = transport::detail::BootstrapLease::acquire(bootstrap);

        if (!lease)
            throw Error(sourceDiagnostic(lease.error()));

        sourceBootstrap_.emplace(std::move(*lease));

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto demand = mux::DemandTracker::create(
                FAMILIES[family], {peers_.size() * CLIENT_KEYS, peers_.size() * CLIENT_SYMBOL_UNITS});

            if (!demand)
                throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-upstream-demand-storage"});

            sourceDemand_[family].emplace(std::move(*demand));
        }
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return listener_.port();
    }

    void progress(detail::PublisherPort &port) {
        acceptPeer();
        progressPeers();
        WorkSet work{};
        constexpr std::size_t QUEUE_ACTIONS = 16;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto &queue = *queues_[family];
            auto &turn = work[family];
            turn = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), familyCount_ > 1);

            for (std::size_t i = 0; i < QUEUE_ACTIONS; ++i) {
                const auto moved = queue.progress(turn);

                if (!moved) {
                    if (moved.error().error == mux::QueueError::WORK_LIMIT)
                        break;

                    throw Error(queueDiagnostic(moved.error(), "publisher-progress"));
                }
            }

            if (const auto terminal = queue.terminal())
                throw Error({ErrorCode::INVALID_STATE, "publisher-queue-terminal", 0,
                             static_cast<std::uint32_t>(terminal->outcome)});

            publishDemand(port, family);
        }

        if (sourceOptions_)
            progressSource(port, work);
        else
            publishLocal(port, work);

        statistics_.activePeers = 0;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto &familyStats = statistics_.families[family];
            const auto resources = queues_[family]->resources();
            familyStats.desiredKeys = queues_[family]->aggregateEntries().size();
            familyStats.pendingSourceRecords = resources.pendingRecords;
            familyStats.deliveryRecordOwners = resources.dataItems;
            familyStats.queuedDeliveryRecords = 0;
            familyStats.retainedOutputRecords = 0;

            for (const auto &peer : peers_) {
                if (!peer)
                    continue;

                if (const auto client = queues_[family]->resources(peer->handles[family]))
                    familyStats.queuedDeliveryRecords += client->queuedRecords;

                familyStats.retainedOutputRecords += peer->delivery[family]->batch.records().size();
            }
        }

        for (const auto &peer : peers_)
            statistics_.activePeers += peer && !peer->closing;

        {
            const std::lock_guard lock(port.mutex);
            statistics_.pendingPublications = port.size;
            port.statistics = statistics_;
        }
    }

    void stop() {
        const auto stopped = listener_.shutdown();

        if (!stopped)
            throw Error(nativeDiagnostic(stopped.error()));
    }
};

void runPublisher(detail::PublisherPort &port, transport::RuntimeBootstrap &bootstrap, PublisherOptions options,
                  std::stop_token stop) noexcept {
    Diagnostic diagnostic;

    try {
        PublisherServer server(bootstrap, options);
        {
            const std::lock_guard lock(port.mutex);
            port.boundPort = server.port();
            port.started = true;
        }
        port.changed.notify_all();

        while (!stop.stop_requested()) {
            server.progress(port);
            std::unique_lock lock(port.mutex);
            port.changed.wait_for(lock, IO_CADENCE);
        }

        server.stop();
    } catch (const Error &failure) {
        diagnostic = failure.diagnostic();
    } catch (const std::bad_alloc &) {
        diagnostic = ALLOCATION;
    } catch (...) {
        diagnostic = {ErrorCode::INVALID_STATE, "publisher-worker"};
    }
    {
        const std::lock_guard lock(port.mutex);
        port.closed = true;
        port.source.connected = false;
        port.diagnostic = diagnostic;
        std::vector<SourceRow>().swap(port.pending);

        for (auto &demand : port.demand)
            std::vector<std::u16string>().swap(demand);

        port.size = 0;
    }
    port.changed.notify_all();
}
} // namespace

struct PublisherEndpoint::Impl {
    std::shared_ptr<detail::PublisherPort> port;
    std::jthread worker;
};

Publisher::Publisher(std::shared_ptr<detail::PublisherPort> port) noexcept : port_(std::move(port)) {
}

namespace {
template <typename Event, typename Convert>
std::expected<void, Diagnostic> publishEvent(const std::shared_ptr<detail::PublisherPort> &port, const Event &event,
                                             Convert convert, bool indexed) {
    if (port && (port->remote || (indexed && !port->mixed))) {
        const std::lock_guard lock(port->mutex);

        return std::unexpected(port->closed ? CLOSED : (port->remote ? LOCAL_SOURCE_DISABLED : MIXED_DISABLED));
    }

    auto row = convert(event);

    if (!row)
        return std::unexpected(row.error());

    if (!port)
        return std::unexpected(CLOSED);

    {
        const std::lock_guard lock(port->mutex);

        if (port->closed)
            return std::unexpected(CLOSED);

        if (port->size == port->pending.size())
            return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-pending-quotes"});

        const auto index = (port->head + port->size) % port->pending.size();
        port->pending[index] = std::move(*row);
        ++port->size;
    }

    port->changed.notify_all();

    return {};
}
} // namespace

std::expected<void, Diagnostic> Publisher::publish(const events::Quote &quote) const {
    return publishEvent(port_, quote, quoteRow, false);
}

std::expected<void, Diagnostic> Publisher::publish(const events::TimeAndSale &sale) const {
    return publishEvent(port_, sale, saleRow, true);
}

std::expected<void, Diagnostic> Publisher::publish(const events::Order &order) const {
    return publishEvent(port_, order, orderRow, true);
}

PublisherEndpoint::PublisherEndpoint(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

std::expected<std::unique_ptr<PublisherEndpoint>, Diagnostic>
PublisherEndpoint::create(transport::RuntimeBootstrap &bootstrap, PublisherOptions options) {
    if (!options.listener.maxConnections || options.listener.maxConnections > MAX_PEERS || !options.pendingQuotes ||
        options.pendingQuotes > MAX_PENDING_QUOTES) {
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-limits"});
    }

    try {
        if (options.source) {
            const auto &source = *options.source;
            // Longest presentation is IPv6 with an embedded dotted IPv4 address.
            constexpr std::size_t MAX_NUMERIC_ADDRESS = 45;
            std::array<char, MAX_NUMERIC_ADDRESS + 1> address{};

            if (source.port == 0 || source.retryDelay < 1ms || source.retryDelay > 60s || source.address.empty() ||
                source.address.size() > MAX_NUMERIC_ADDRESS ||
                source.address.find_first_of(std::string_view("\0%", 2)) != std::string_view::npos)
                return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-source-options"});

            std::ranges::copy(source.address, address.begin());
            asio::error_code error;
            (void)asio::ip::make_address(address.data(), error);

            if (error)
                return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-source-address"});
        }

        auto impl = std::make_unique<Impl>();
        impl->port =
            std::make_shared<detail::PublisherPort>(options.pendingQuotes, options.source.has_value(), options.mixed);
        const auto port = impl->port;
        auto endpoint = std::unique_ptr<PublisherEndpoint>(new PublisherEndpoint(std::move(impl)));
        endpoint->impl_->worker = std::jthread([port, &bootstrap, options](std::stop_token stop) {
            runPublisher(*port, bootstrap, options, stop);
        });
        std::unique_lock lock(port->mutex);
        port->changed.wait(lock, [&] {
            return port->started || port->closed;
        });

        if (!port->started)
            return std::unexpected(port->diagnostic);

        return endpoint;
    } catch (const std::bad_alloc &) {
        return std::unexpected(ALLOCATION);
    } catch (const std::system_error &) {
        return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-worker-start"});
    }
}

PublisherEndpoint::~PublisherEndpoint() {
    closeAndAwaitTermination();
}

std::uint16_t PublisherEndpoint::port() const noexcept {
    const std::lock_guard lock(impl_->port->mutex);

    return impl_->port->boundPort;
}

Publisher PublisherEndpoint::getPublisher() const noexcept {
    return Publisher(impl_->port);
}

bool PublisherEndpoint::awaitDemand(std::u16string_view symbol, bool subscribed, Clock::time_point deadline) const {
    return awaitDemand(Family::TICKER, symbol, subscribed, deadline);
}

bool PublisherEndpoint::awaitDemand(Family family, std::u16string_view symbol, bool subscribed,
                                    Clock::time_point deadline) const {
    const auto index = familyIndex(family);
    auto &port = *impl_->port;

    if (!exactSymbol(symbol) || !index || (*index != 0 && !port.mixed))
        return false;

    std::unique_lock lock(port.mutex);
    const auto &demand = port.demand[*index];
    const auto matches = [&] {
        return (std::ranges::find(demand, symbol) != demand.end()) == subscribed;
    };

    return port.changed.wait_until(lock, deadline, [&] {
        return port.closed || matches();
    }) && !port.closed;
}

bool PublisherEndpoint::awaitSourceGeneration(std::uint64_t after, Clock::time_point deadline) const {
    auto &port = *impl_->port;

    if (!port.remote)
        return false;

    std::unique_lock lock(port.mutex);

    return port.changed.wait_until(lock, deadline, [&] {
        return port.closed || (port.source.connected && port.source.generation > after);
    }) && !port.closed;
}

PublisherSourceStatus PublisherEndpoint::getSourceStatus() const noexcept {
    const std::lock_guard lock(impl_->port->mutex);

    return impl_->port->source;
}

PublisherStatistics PublisherEndpoint::getStatistics() const noexcept {
    const std::lock_guard lock(impl_->port->mutex);
    auto snapshot = impl_->port->statistics;
    snapshot.pendingPublications = impl_->port->size;

    return snapshot;
}

void PublisherEndpoint::closeAndAwaitTermination() noexcept {
    auto &port = *impl_->port;
    {
        const std::lock_guard lock(port.mutex);
        port.closed = true;
    }

    impl_->worker.request_stop();
    port.changed.notify_all();

    if (impl_->worker.joinable())
        impl_->worker.join();
}

Diagnostic PublisherEndpoint::getDiagnostic() const noexcept {
    const std::lock_guard lock(impl_->port->mutex);

    return impl_->port->diagnostic;
}

} // namespace org::ttldtor::qdcxx::api
