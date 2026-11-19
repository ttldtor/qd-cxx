// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"
#include "publisher_delivery.hpp"

#include "publication_queue.hpp"

#include "../mux/queue_replace.hpp"
#include "../mux/queue_schedule.hpp"
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
#include <atomic>
#include <bit>
#include <bitset>
#include <concepts>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>

namespace org::ttldtor::qdcxx::api {
namespace {
using Clock = std::chrono::steady_clock;
using FieldType = core::SerialFieldType;
using namespace std::chrono_literals;

constexpr std::size_t MAX_PEERS = 8;
constexpr std::size_t MAX_PENDING_QUOTES = 1024;
constexpr std::size_t MAX_STREAM_BATCH_RECORDS = PublisherOptions::MAX_STREAM_BATCH_RECORDS;
constexpr std::size_t SYMBOL_UNITS = 256;
constexpr std::size_t CLIENT_KEYS = 32;
// Java Feed expands Order into source-specific and derived Quote subscriptions.
constexpr std::size_t MAX_SUBSCRIPTION_WIRE_TUPLES = 256;
constexpr std::size_t CLIENT_PENDING_ITEMS = 32;
constexpr std::size_t HISTORY_POINTS_PER_KEY = 16;
constexpr std::size_t CLIENT_SYMBOL_UNITS = CLIENT_KEYS * SYMBOL_UNITS;
constexpr std::size_t INPUT_BYTES = 8192;
using detail::MAX_FRAME_BYTES;
using detail::MAX_STREAM_WINDOW_ROWS;
using detail::MINIMUM_WIRE_ROW_BYTES;
constexpr std::size_t FRAME_PREFIX_BYTES = 9;
constexpr std::size_t EVENT_BYTES = 8192;
constexpr std::size_t EVENT_OWNER_ALLOWANCE = 1024;
constexpr std::size_t RECORD_COPY_PADDING =
    (PublisherRecordLimits::MAX_FIELDS + 1) * (sizeof(char16_t) + 1) + alignof(core::FieldValueView);
constexpr std::size_t SOURCE_EPOCH_BYTES = 4 * 1024 * 1024;
constexpr std::size_t LOCAL_RECORD_EVENT_BYTES = 64 * 1024;
constexpr encoding::UtfStringLimits PUBLISHER_TEXT_LIMITS{8192, 1024};
constexpr std::size_t REMOTE_EPOCH_BYTES = 20 * 1024 * 1024;
constexpr std::size_t REMOTE_EVENT_BYTES = 64 * 1024;
constexpr std::size_t REMOTE_DATA_RECORDS = 32;
constexpr std::size_t MIXED_OWNERS =
    MAX_PEERS * (CLIENT_KEYS * (HISTORY_POINTS_PER_KEY + 2) + CLIENT_PENDING_ITEMS + 1) + 12;
constexpr std::size_t LOCAL_RECORD_EPOCH_BYTES = MIXED_OWNERS * (LOCAL_RECORD_EVENT_BYTES + EVENT_OWNER_ALLOWANCE);
constexpr std::size_t MIXED_LOCAL_EPOCH_BYTES = MIXED_OWNERS * (EVENT_BYTES + EVENT_OWNER_ALLOWANCE);
constexpr std::size_t MIXED_REMOTE_EPOCH_BYTES = MIXED_OWNERS * (REMOTE_EVENT_BYTES + EVENT_OWNER_ALLOWANCE);
constexpr std::size_t QUOTE_FIELDS = 10;
constexpr std::size_t MAX_DATA_FIELDS = 16;
constexpr std::size_t MAX_TYPED_ROW_LOGICAL_BYTES =
    MAX_DATA_FIELDS * sizeof(core::FieldValueView) + SYMBOL_UNITS * sizeof(char16_t);
constexpr std::int32_t QUOTE_ID = 0;
constexpr std::int32_t SALE_ID = 1;
constexpr std::int32_t ORDER_ID = 2;
using Family = core::SubscriptionFamily;
using DataMessage = protocol::LiveDataMessage;
using detail::DATA_MESSAGES;
constexpr std::array FAMILIES{Family::TICKER, Family::STREAM, Family::HISTORY};

constexpr std::array ADD_MESSAGES{protocol::SubscriptionMessage::TICKER_ADD, protocol::SubscriptionMessage::STREAM_ADD,
                                  protocol::SubscriptionMessage::HISTORY_ADD};
constexpr std::array REMOVE_MESSAGES{protocol::SubscriptionMessage::TICKER_REMOVE,
                                     protocol::SubscriptionMessage::STREAM_REMOVE,
                                     protocol::SubscriptionMessage::HISTORY_REMOVE};
constexpr std::array RECORD_IDS{QUOTE_ID, SALE_ID, ORDER_ID};
constexpr std::size_t FAMILY_COUNT = FAMILIES.size();
using detail::STREAM_FAMILY;
constexpr std::size_t CATALOG_RECORDS = 1024;
constexpr core::SchemaLimits CATALOG_LIMITS{CATALOG_RECORDS, 16384, 32768, 1024, 1048576};
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
constexpr Diagnostic RECORD_MODE{ErrorCode::INVALID_STATE, "publisher-record-mode"};
constexpr Diagnostic RECORD_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-record-shape"};
constexpr Diagnostic HISTORY_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-history-shape"};
constexpr Diagnostic RECORD_LIMIT{ErrorCode::RESOURCE_LIMIT, "publisher-record-limit"};
constexpr Diagnostic PENDING_RECORD_LIMIT{ErrorCode::RESOURCE_LIMIT, "publisher-pending-records"};
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

/** Requested component lengths of the exact pending record backing. */
struct RecordStorage {
    std::size_t textUnits;
    std::size_t charge;
};

/** Derive a finite view-index capacity from the independent egress and wire budgets. */
std::size_t deliveryWindowRows(const PublisherOptions &options) noexcept {
    if (!options.streamBytes)
        return options.streamBatchRecords;

    const auto &limits = *options.streamBytes;
    const auto candidates = (limits.targetPayloadBytes + MINIMUM_WIRE_ROW_BYTES - 1) / MINIMUM_WIRE_ROW_BYTES;
    // The immutable control block includes a complete MuxRecordView. The byte check uses its actual request.
    const auto detached = std::max<std::size_t>(1, limits.deliveryWindowBytes / sizeof(mux::MuxRecordView));

    return std::min(candidates, detached);
}

/** Bound each temporary source copy by its mailbox and by the room provable at a ready peer. */
std::size_t sourceWindowRows(const PublisherOptions &options) noexcept {
    if (!options.streamBytes)
        return options.streamBatchRecords;

    const auto minimumInput = options.catalog ? sizeof(char16_t) : sizeof(SourceRow);
    const auto available = std::max<std::size_t>(1, options.recordLimits.pendingBytes / minimumInput);

    return std::min({deliveryWindowRows(options), options.pendingQuotes, available});
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

std::size_t enabledFamilies(const PublisherOptions &options) noexcept {
    return options.catalog || options.mixed ? FAMILY_COUNT : 1;
}

bool exactSymbol(std::u16string_view symbol) noexcept {
    return !symbol.empty() && symbol.size() <= SYMBOL_UNITS && symbol.front() != u'*';
}

bool validHistoryMetadata(Family family, const PublisherHistoryMetadata &metadata) noexcept {
    using namespace events::indexed_event_flags;
    constexpr auto SUPPORTED_FLAGS =
        TX_PENDING | REMOVE_EVENT | SNAPSHOT_BEGIN | SNAPSHOT_END | SNAPSHOT_SNIP | SNAPSHOT_MODE;

    if (family != Family::HISTORY)
        return metadata.eventFlags == 0 && !metadata.removalIndex;

    return (metadata.eventFlags & ~SUPPORTED_FLAGS) == 0 &&
           ((metadata.eventFlags & REMOVE_EVENT) != 0) == metadata.removalIndex.has_value();
}

/** HISTORY reserves the largest signed index for a virtual snapshot boundary. */
bool reservedHistoryIndex(const core::IndexedTimeLayout &layout,
                          std::span<const core::FieldValueView> fields) noexcept {
    constexpr auto RESERVED = std::numeric_limits<std::int64_t>::max();

    if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&layout))
        return fields[pair->firstField].raw == highWord(RESERVED) && fields[pair->secondField].raw == lowWord(RESERVED);

    if (const auto *single = std::get_if<core::IndexedTimeSingleInt64>(&layout))
        return fields[single->field].raw == RESERVED;

    return false;
}

/** Check the applicable fields and size their backing without copying variable payload. */
std::expected<RecordStorage, Diagnostic> recordStorage(const core::LocalRecord &layout, std::u16string_view symbol,
                                                       std::span<const core::FieldValueView> fields, std::size_t limit,
                                                       bool removal) noexcept {
    if (fields.size() != (removal ? 0 : layout.fields.size()) || !exactSymbol(symbol))
        return std::unexpected(RECORD_SHAPE);

    if (fields.size() > PublisherRecordLimits::MAX_FIELDS)
        return std::unexpected(RECORD_LIMIT);

    const auto fieldBytes = fields.size() * sizeof(core::FieldValueView);

    if (fieldBytes > limit || symbol.size() > (limit - fieldBytes) / sizeof(char16_t))
        return std::unexpected(RECORD_LIMIT);

    RecordStorage storage{symbol.size(), fieldBytes + symbol.size() * sizeof(char16_t)};

    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto &field = fields[index];

        if (std::to_underlying(field.type) != layout.fields[index].serialTypeId || !core::validFieldValueView(field))
            return std::unexpected(RECORD_SHAPE);

        if (field.units) {
            const auto units = field.units->size();

            if (units > (limit - storage.charge) / sizeof(char16_t) ||
                (field.type == FieldType::STRING && units > PUBLISHER_TEXT_LIMITS.codeUnits))
                return std::unexpected(RECORD_LIMIT);

            storage.textUnits += units;
            storage.charge += units * sizeof(char16_t);
        }

        if (field.bytes) {
            const auto bytes = field.bytes->size();

            if (bytes > limit - storage.charge)
                return std::unexpected(RECORD_LIMIT);

            storage.charge += bytes;
        }
    }

    return storage;
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

protocol::LiveWork liveTurn(bool catalog = false) noexcept {
    if (catalog) {
        // Binding reserves configured lookup/name bounds for each effective peer description.
        // Cover a complete replacement plus the framing, cloning, and sorting around it.
        constexpr auto names = CATALOG_LIMITS.totalNameUnits + MAX_FRAME_BYTES;
        constexpr auto scans = CATALOG_LIMITS.fields + CATALOG_RECORDS * (CATALOG_RECORDS + 1);
        constexpr auto comparisons =
            CATALOG_LIMITS.fields * (CATALOG_LIMITS.lookups + 3) + CATALOG_RECORDS * CATALOG_RECORDS;

        return {128, 2 * comparisons, 2 * names * scans, 64 * 1024 * 1024};
    }

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

mux::QueueWork serverQueueTurn(std::size_t peers, bool remote, bool catalog, std::size_t families,
                               std::size_t records) noexcept {
    auto work = queueTurn();
    const auto keys = peers * CLIENT_KEYS;
    // A mapping rebuild prepays the full key-pair comparison bound in QueueWork, not MuxWork.
    work.textUnits += keys * keys * SYMBOL_UNITS;
    // MuxState reserves against both possible maps and all input rows before a command.
    constexpr std::size_t INTEREST_PASSES = 32;
    const auto interestVisits =
        INTEREST_PASSES * (1 + records + peers + 2 * keys + CLIENT_KEYS) * (1 + keys + CLIENT_KEYS);
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
    const auto epochBytes = !remote && catalog         ? LOCAL_RECORD_EPOCH_BYTES
                            : families == FAMILY_COUNT ? (remote ? MIXED_REMOTE_EPOCH_BYTES : MIXED_LOCAL_EPOCH_BYTES)
                                                       : (remote ? REMOTE_EPOCH_BYTES : SOURCE_EPOCH_BYTES);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    work.ticker.bytes =
        keys + 2 > (maximum - work.ticker.bytes) / epochBytes ? maximum : work.ticker.bytes + (keys + 2) * epochBytes;

    if (families > 1)
        work.stream.bytes = work.ticker.bytes;

    if (families == FAMILY_COUNT) {
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
    }

    return work;
}

protocol::LiveLimits liveLimits(bool mixed = false, bool catalog = false) noexcept {
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

    if (catalog) {
        result.context = {CATALOG_LIMITS, CATALOG_RECORDS, {16384, 1024, 1048576}};
        result.descriptions = {MAX_FRAME_BYTES, CATALOG_RECORDS, 256, 16384, {8192, 1024}, MAX_FRAME_BYTES};
        result.snapshotBytes = 8 * 1024 * 1024;
        result.scratchBytes = 2 * 1024 * 1024;
        result.residentBytes = 32 * 1024 * 1024;
    }

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

using CatalogNames = std::span<const std::vector<std::u16string>>;

core::LocalSchema createSchema(PublisherOptions options) {
    if (!options.catalog)
        return publisherSchema(options.mixed);

    const auto &source = options.catalog->schema;

    if (source.profile() != core::LocalProfile::BINARY_FIELDS_V1 || source.records().empty())
        throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-catalog-profile"});

    auto schema = source.boundedCopy(CATALOG_LIMITS);

    if (!schema) {
        const auto code = schema.error() == core::SchemaError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                                  : ErrorCode::RESOURCE_LIMIT;
        throw Error({code, "publisher-catalog-schema"});
    }

    return std::move(*schema);
}

std::vector<std::int32_t> recordIds(const core::LocalSchema &schema) {
    std::vector<std::int32_t> ids;
    ids.reserve(schema.records().size());

    for (const auto &record : schema.records())
        ids.push_back(record.id);

    return ids;
}

std::vector<std::vector<std::u16string>> catalogNames(PublisherOptions options, const core::LocalSchema &schema) {
    if (!options.catalog)
        return {};

    const auto names = options.catalog->wireFieldNames;

    if (names.size() != schema.records().size())
        throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-catalog-records"});

    std::size_t remaining = CATALOG_LIMITS.totalNameUnits;

    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i].size() != schema.records()[i].fields.size())
            throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-catalog-fields"});

        for (const auto &name : names[i]) {
            if (name.size() > CATALOG_LIMITS.nameUnits || name.size() > remaining)
                throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-catalog-names"});

            remaining -= name.size();
        }
    }

    return {names.begin(), names.end()};
}

/** Local HISTORY must be able to encode removals and synthetic snapshot boundaries. */
std::vector<std::uint8_t> localHistoryRecords(const core::LocalSchema &schema, CatalogNames names) {
    std::vector<std::uint8_t> supported(schema.records().size());

    for (const auto &record : schema.records()) {
        if (!record.indexedTimeLayout || record.fields.size() > PublisherRecordLimits::MAX_FIELDS)
            continue;

        const auto first = std::ranges::find_if(record.fields, [](const auto &field) {
            return field.serialTypeId != std::to_underlying(FieldType::VOID_FIELD);
        });

        if (first != record.fields.end()) {
            const auto field = static_cast<std::size_t>(first - record.fields.begin());
            supported[record.id] = !names[record.id][field].starts_with(u"Event");
        }
    }

    return supported;
}

protocol::SubscriptionWriter
publisherWriter(const core::LocalSchema &schema, CatalogNames names,
                core::SubscriptionSelectorPolicy policy = core::SubscriptionSelectorPolicy::EXACT_ONLY,
                std::size_t streamWindowRows = 0) {
    std::vector<protocol::OutgoingRecord> outgoing;
    outgoing.reserve(schema.records().size());

    for (const auto &record : schema.records()) {
        auto &layout = outgoing.emplace_back(record.id, record.id, record.name, std::vector<protocol::OutgoingField>{});

        const auto fieldNames =
            names.empty() ? std::span<const std::u16string>{} : std::span<const std::u16string>(names[record.id]);
        layout.fields.reserve(record.fields.size());

        for (std::size_t i = 0; i < record.fields.size(); ++i) {
            const auto key = std::ranges::find_if(record.lookups, [&](const auto &lookup) {
                return lookup.fieldId == i && (names.empty() || fieldNames[i] == lookup.name);
            });

            if (key == record.lookups.end())
                throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-writer-field"});

            layout.fields.push_back({key->name, record.fields[i].serialTypeId, static_cast<std::int32_t>(i)});
        }
    }

    const auto catalog = !names.empty();
    const protocol::LayoutLimits layouts =
        catalog ? protocol::LayoutLimits{CATALOG_LIMITS, CATALOG_RECORDS, 16384, 1024, 1048576}
                : protocol::LayoutLimits{SCHEMA_LIMITS, FAMILY_COUNT, 3 * MAX_DATA_FIELDS, 64, 2048};
    protocol::SubscriptionWriterWork writerWork{64, 8 * 1024 * 1024, 8 * 1024 * 1024, 256 * 1024};
    auto writer =
        protocol::SubscriptionWriter::create(schema, outgoing, layouts,
                                             {MAX_FRAME_BYTES, MAX_FRAME_BYTES + FRAME_PREFIX_BYTES,
                                              std::max(streamWindowRows, catalog ? CATALOG_RECORDS : 128), SYMBOL_UNITS,
                                              PUBLISHER_TEXT_LIMITS, 256 * 1024, 256 * 1024, 4, catalog},
                                             writerWork, policy);

    if (!writer)
        throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-writer", 0, static_cast<std::uint32_t>(writer.error())});

    return std::move(*writer);
}

protocol::LiveSession serverSession(const core::LocalSchema &schema, CatalogNames names, protocol::LiveWork &work,
                                    std::size_t streamWindowRows) {
    auto writer = publisherWriter(schema, names, core::SubscriptionSelectorPolicy::EXACT_ONLY, streamWindowRows);
    auto local = schema;
    const bool catalog = !names.empty();
    auto session = protocol::LiveSession::createServer(
        std::move(local), std::move(writer), liveLimits(catalog || schema.records().size() > 1, catalog),
        {MAX_FRAME_BYTES, catalog ? CATALOG_RECORDS : FAMILY_COUNT, catalog ? CATALOG_RECORDS : 128,
         MAX_SUBSCRIPTION_WIRE_TUPLES, SYMBOL_UNITS, MAX_SUBSCRIPTION_WIRE_TUPLES * SYMBOL_UNITS},
        work);

    if (!session)
        throw Error(protocolDiagnostic(session.error(), "publisher-server-session"));

    return std::move(*session);
}

constexpr mux::ClientLimits CLIENT_LIMITS{
    CLIENT_PENDING_ITEMS, 512 * 1024,         8, 128 * 1024, 1, 32, SYMBOL_UNITS, 0, 0, 4096, 2, 4,
    CLIENT_KEYS,          CLIENT_SYMBOL_UNITS};

/** Account for explicit detached copies of all admitted field views and variable bytes. */
mux::ClientLimits recordClientLimits(std::size_t rowBytes) {
    auto result = CLIENT_LIMITS;
    // A terminator and one byte of alignment may precede or follow each copied UTF-16 value.
    result.recordFields = PublisherRecordLimits::MAX_FIELDS;
    result.recordFieldUnits = rowBytes / sizeof(char16_t);
    result.recordByteUnits = rowBytes;
    // HISTORY may materialize the full default layout from a removal containing only the symbol and index.
    constexpr auto MAX_DEFAULT_ROW =
        PublisherRecordLimits::MAX_FIELDS * sizeof(core::FieldValueView) + SYMBOL_UNITS * sizeof(char16_t);
    result.recordArenaBytes = std::max(rowBytes, MAX_DEFAULT_ROW) + RECORD_COPY_PADDING;

    result.recordBytes = result.dataItems * (result.recordArenaBytes + EVENT_OWNER_ALLOWANCE);

    return result;
}

mux::ClientLimits publisherClientLimits(const PublisherOptions &options, bool byteStream = false) {
    auto limits =
        options.catalog && !options.source ? recordClientLimits(options.recordLimits.rowBytes) : CLIENT_LIMITS;

    if (!byteStream)
        return limits;

    const auto window = deliveryWindowRows(options);
    limits.batchRecordBytes = options.streamBytes->deliveryWindowBytes;
    // One retained delivery and one additional ready window can coexist for a healthy peer.
    limits.recordBytes = 2 * std::max(limits.batchRecordBytes, limits.recordArenaBytes + EVENT_OWNER_ALLOWANCE);
    limits.dataItems = 2 * window;
    limits.batchBytes =
        std::max(limits.batchBytes,
                 EVENT_OWNER_ALLOWANCE + window * (sizeof(mux::MuxRecordView) + sizeof(std::shared_ptr<const void>)));
    limits.compactRecordArena = true;

    return limits;
}

/** Checked product calculated before allocating publication storage.
 * @param count Number of equal requests.
 * @param bytes Requested bytes per item.
 * @return Exact product; throws SIZE_OVERFLOW before allocation if unrepresentable.
 */
std::size_t publicationProduct(std::size_t count, std::size_t bytes) {
    if (bytes && count > std::numeric_limits<std::size_t>::max() / bytes) {
        throw core::detail::RecordBlockError::SIZE_OVERFLOW;
    }

    return count * bytes;
}

/** Byte limits shared by collector/Mux construction and the conservative physical domain. */
struct PublisherQueueCapacity {
    std::size_t cacheFrames;        ///< Existing collector frame/owner slots.
    std::size_t cacheBytes;         ///< Existing collector retained-frame quota.
    std::size_t queueResidentBytes; ///< Actual configured Mux context/generation capacity.
    std::size_t retainedBytes;      ///< Conservative simultaneous collector and detached consumer capacity sum.
};

/** Derive the same finite capacities used by real collector and queue construction.
 * @param peers Configured active/retiring peer slots.
 * @param remote Select the existing remote event capacity.
 * @param family Collector contract.
 * @param catalog Select the existing catalog event capacity.
 * @param limits Existing per-client capacities.
 * @param source Existing service quantum for this contract.
 * @return Shared setup values and conservative retained-capacity bound, with checked byte arithmetic.
 */
PublisherQueueCapacity publisherQueueCapacity(std::size_t peers, bool remote, Family family, bool catalog,
                                              const mux::ClientLimits &limits, std::size_t source) {
    using core::detail::recordByteSum;
    constexpr std::size_t QUEUE_RESIDENT_ALLOWANCE = 32 * 1024 * 1024;
    const auto keys = publicationProduct(peers, CLIENT_KEYS);
    const auto frames =
        recordByteSum(family == Family::HISTORY ? publicationProduct(keys, HISTORY_POINTS_PER_KEY) : keys, 1);
    const auto event = recordByteSum(remote    ? REMOTE_EVENT_BYTES
                                     : catalog ? LOCAL_RECORD_EVENT_BYTES
                                               : EVENT_BYTES,
                                     EVENT_OWNER_ALLOWANCE);
    const auto cache = publicationProduct(publicationProduct(frames, event), source);
    const auto client = publicationProduct(peers, recordByteSum(limits.recordBytes, limits.batchBytes));
    const auto resident = recordByteSum(QUEUE_RESIDENT_ALLOWANCE, limits.compactRecordArena ? client : 0);
    // Include both collector retained/cache limits, HISTORY cursor/output owners, and detached Mux consumers.
    const auto history = publicationProduct(publicationProduct(peers, recordByteSum(limits.dataItems, 1)), event);
    const auto collector = recordByteSum(recordByteSum(publicationProduct(2, cache), publicationProduct(2, history)),
                                         QUEUE_RESIDENT_ALLOWANCE);

    return {frames, cache, resident, recordByteSum(recordByteSum(collector, resident), client)};
}

/** Translate detached capacity and allocation refusals.
 * @param error Private validation/reservation failure.
 * @return Stable public diagnostic distinguishing permanently oversized and occupied capacity.
 */
Diagnostic blockDiagnostic(core::detail::RecordBlockError error) noexcept {
    using enum core::detail::RecordBlockError;

    if (error == ALLOCATION_FAILURE) {
        return ALLOCATION;
    }

    if (error == CAPACITY_EXCEEDED || error == SIZE_OVERFLOW) {
        return {ErrorCode::RESOURCE_LIMIT, "publisher-publication-capacity"};
    }

    if (error == WOULD_BLOCK) {
        return PENDING_RECORD_LIMIT;
    }

    return RECORD_SHAPE;
}

/** @return Actual allocator request for one publication node and shared control. */
std::size_t publicationNodeBytes() {
    return core::detail::recordControlBytes<detail::Publication>(
        core::detail::RecordByteReservation{}, core::detail::RecordByteReservation{},
        std::shared_ptr<const core::detail::BoundRecordBatch>{}, std::size_t{});
}

/** Bound a valid publication without scanning its borrowed rows.
 * @param rows Complete requested row count.
 * @param logicalRowBytes Maximum validated field descriptors, symbol and variable payload for one row.
 * @return Conservative node/block allocation request; throws SIZE_OVERFLOW when the bound is unrepresentable.
 */
std::size_t publicationRequestBound(std::size_t rows, std::size_t logicalRowBytes) {
    using core::detail::recordByteSum;
    constexpr auto PADDING = alignof(std::max_align_t) - 1;
    const auto perRow = recordByteSum(sizeof(core::detail::SharedRecordPayload) +
                                          sizeof(core::detail::BoundRecordBatch::Metadata) + PADDING,
                                      logicalRowBytes);
    const auto control = core::detail::recordControlBytes<core::detail::BoundRecordBatch>(
        core::detail::RecordByteReservation{}, std::shared_ptr<const core::detail::RecordBlockBinding>{},
        core::detail::RecordBlockAccounting{}, std::size_t{});

    return recordByteSum(publicationProduct(rows, perRow),
                         recordByteSum(PADDING, recordByteSum(control, publicationNodeBytes())));
}

/** @param measured Exact immutable block layout.
 * @return Complete per-publication INPUT request, excluding the once-reserved binding.
 */
std::size_t publicationRequestBytes(const core::detail::RecordBlockAccounting &measured) {
    return core::detail::recordByteSum(
        publicationNodeBytes(),
        core::detail::recordByteSum(measured.backingAllocationBytes, measured.controlAllocationBytes));
}

/** Measure the once-held immutable binding with the existing schema-capacity model.
 * @param options Validated local/remote schema configuration.
 * @return Modelled schema capacity plus actual binding shared-control request.
 */
std::size_t publicationBindingBytes(const PublisherOptions &options) {
    if (options.source) {
        return 0;
    }

    auto schema = createSchema(options);
    const auto storage = schema.storageBytes() - sizeof(core::LocalSchema);
    const auto control = core::detail::recordControlBytes<core::detail::RecordBlockBinding>(
        core::detail::RecordByteReservation{}, std::move(schema), core::detail::RecordSourceGeneration{}, std::size_t{},
        std::size_t{});

    return core::detail::recordByteSum(storage, control);
}

/** Preserve explicit byte mode and legacy typed row capacity using checked finite storage bounds.
 * @param options Validated publication and row configuration.
 * @return Complete INPUT capacity, including its once-held binding.
 */
std::size_t publicationInputCapacity(const PublisherOptions &options) {
    if (options.source) {
        return 0;
    }

    using core::detail::recordByteSum;

    if (options.catalog || options.streamBytes || options.source) {
        return options.recordLimits.pendingBytes;
    }

    // A single publication is the worst per-row layout: batching amortizes control/node and alignment costs.
    const auto single = publicationRequestBound(1, MAX_TYPED_ROW_LOGICAL_BYTES);

    return recordByteSum(publicationBindingBytes(options), publicationProduct(options.pendingQuotes, single));
}

/** Bound simultaneous input, selection, collector and detached Mux storage without an RSS claim.
 * @param options Existing configured input, service and consumer capacities.
 * @return Checked conservative physical-allocation capacity including immutable binding storage.
 */
std::size_t publicationPhysicalCapacity(const PublisherOptions &options) {
    if (options.source) {
        return 0;
    }

    using core::detail::recordByteSum;
    auto bytes = recordByteSum(publicationInputCapacity(options), publicationBindingBytes(options));
    const auto families = enabledFamilies(options);
    const auto catalog = options.catalog.has_value();
    const auto sourceRows = sourceWindowRows(options);
    const auto epoch = catalog                    ? LOCAL_RECORD_EPOCH_BYTES
                       : families == FAMILY_COUNT ? MIXED_LOCAL_EPOCH_BYTES
                                                  : SOURCE_EPOCH_BYTES;
    const auto event = catalog ? LOCAL_RECORD_EVENT_BYTES : EVENT_BYTES;
    bytes = recordByteSum(bytes, recordByteSum(epoch, publicationProduct(2 * (sourceRows - 1), event)));

    for (std::size_t family = 0; family < families; ++family) {
        const auto limits = publisherClientLimits(options, options.streamBytes && family == STREAM_FAMILY);
        const auto capacity =
            publisherQueueCapacity(options.listener.maxConnections, options.source.has_value(), FAMILIES[family],
                                   catalog, limits, family == STREAM_FAMILY ? sourceRows : 1);
        bytes = recordByteSum(bytes, capacity.retainedBytes);
    }

    return bytes;
}

mux::MuxQueue publisherQueue(const core::LocalSchema &schema, std::size_t peers, bool remote, Family family,
                             bool catalog, const mux::ClientLimits &clientLimits, std::size_t streamSourceRows,
                             std::size_t streamDeliveryRows, std::size_t bindingBytes = 0) {
    const auto allKeys = peers * CLIENT_KEYS;
    const auto allUnits = peers * clientLimits.commandSymbolUnits;
    const auto records = schema.records().size();
    const core::SubscriptionLimits desired{records,     allKeys,      SYMBOL_UNITS, allUnits,
                                           allKeys * 2, allUnits * 2, allKeys};
    const mux::MuxLimits interests{records,      peers,    allKeys,     allKeys,
                                   SYMBOL_UNITS, allUnits, CLIENT_KEYS, 8 * 1024 * 1024};
    const auto source = family == Family::STREAM ? streamSourceRows : 1;
    const auto batch = family == Family::STREAM ? streamDeliveryRows : 1;
    const auto capacity = publisherQueueCapacity(peers, remote, family, catalog, clientLimits, source);
    const mux::QueueLimits limits{peers,
                                  remote ? REMOTE_DATA_RECORDS : source,
                                  source,
                                  batch,
                                  8,
                                  peers * clientLimits.dataItems,
                                  peers * clientLimits.recordBytes,
                                  peers * clientLimits.batchOwners,
                                  peers * clientLimits.batchBytes,
                                  peers * clientLimits.commandSlots,
                                  peers * clientLimits.resultSlots,
                                  peers * 2,
                                  core::detail::recordByteSum(capacity.queueResidentBytes, bindingBytes)};
    const auto cacheFrames = capacity.cacheFrames;
    const auto cacheBytes = capacity.cacheBytes;
    const auto eventBytes = (remote    ? REMOTE_EVENT_BYTES
                             : catalog ? LOCAL_RECORD_EVENT_BYTES
                                       : EVENT_BYTES) +
                            EVENT_OWNER_ALLOWANCE;
    const collectors::TickerLimits ticker{peers,  allKeys, cacheFrames, cacheBytes, cacheBytes + 32 * 1024 * 1024,
                                          desired};
    auto work = queueTurn();
    auto queue = [&] {
        switch (family) {
        case Family::TICKER:
            return mux::MuxQueue::createTicker(schema, interests, ticker, {false, 100}, limits, work);
        case Family::STREAM:
            return mux::MuxQueue::createStream(schema, interests, desired,
                                               {cacheFrames, std::max(source, cacheFrames * REMOTE_DATA_RECORDS),
                                                cacheBytes, cacheBytes + 32 * 1024 * 1024},
                                               limits, work);
        case Family::HISTORY:
            return mux::MuxQueue::createHistory(
                schema, interests,
                {allKeys, allKeys * HISTORY_POINTS_PER_KEY, SYMBOL_UNITS, cacheFrames, cacheBytes, peers,
                 clientLimits.dataItems, clientLimits.dataItems + 1, (clientLimits.dataItems + 1) * eventBytes,
                 cacheBytes + peers * (clientLimits.dataItems + 1) * eventBytes + 32 * 1024 * 1024, desired},
                {.storeEverything = false,
                 .retrievalQuantum = 100,
                 .partitionSnapshotQuantum = 10000,
                 .reclaimInactive = catalog},
                limits, work);
        }

        return std::expected<mux::MuxQueue, mux::QueueFailure>{
            std::unexpected(mux::QueueFailure{mux::QueueError::INVALID_CONFIGURATION, {}})};
    }();

    if (!queue)
        throw Error(queueDiagnostic(queue.error(), "publisher-collector"));

    return std::move(*queue);
}

protocol::detail::LocalEventOwner localOwner(bool catalog, std::size_t families, std::size_t streamBatchRecords,
                                             std::shared_ptr<const core::detail::RecordBlockBinding> binding = {}) {
    auto work = liveTurn();
    protocol::LiveLimits limits{};
    limits.residentBytes = SOURCE_EPOCH_BYTES;
    limits.owners = MAX_PEERS * CLIENT_KEYS + 4;
    limits.eventBytes = EVENT_BYTES;

    if (catalog) {
        limits.residentBytes = LOCAL_RECORD_EPOCH_BYTES;
        limits.owners = MIXED_OWNERS;
        limits.eventBytes = LOCAL_RECORD_EVENT_BYTES;
    } else if (families == FAMILY_COUNT) {
        limits.residentBytes = MIXED_LOCAL_EPOCH_BYTES;
        limits.owners = MIXED_OWNERS;
    }

    // The worker and collector may each retain a bounded STREAM arena during routing.
    limits.residentBytes += 2 * (streamBatchRecords - 1) * limits.eventBytes;

    if (binding) {
        limits.residentBytes = core::detail::recordByteSum(limits.residentBytes, binding->retainedCapacityBytes());
    }

    auto owner = protocol::detail::LocalEventOwner::create(limits, work, std::move(binding));

    if (!owner)
        throw Error(protocolDiagnostic(owner.error(), "publisher-source"));

    return std::move(*owner);
}

using detail::Delivery;
using detail::DeliveryMask;
using detail::OutputKind;

using DemandPolicy = std::array<std::optional<FeedFilter>, FAMILY_COUNT>;

struct PeerDemandPolicy {
    transport::TcpPeerAddress address;
    DemandPolicy demand;
};

struct DemandPolicies {
    DemandPolicy defaults;
    std::vector<PeerDemandPolicy> peers;
};

DemandPolicy validatedDemandPolicy(const PublisherDemandPolicy &policy, bool allowDynamic = false) {
    const DemandPolicy result{policy.ticker, policy.stream, policy.history};

    if (std::ranges::any_of(result, [allowDynamic](const auto &filter) {
            return filter && (!filter->valid() || (!allowDynamic && filter->isDynamic()));
        }))
        throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-demand-filter"});

    return result;
}

DemandPolicies configuredDemandPolicies(const PublisherOptions &options) {
    if (options.peerDemand.size() > PublisherOptions::MAX_PEER_DEMAND_POLICIES)
        throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-demand-policy-count"});

    DemandPolicies result{validatedDemandPolicy(options.demand), {}};
    result.peers.reserve(options.peerDemand.size());

    for (const auto &peer : options.peerDemand) {
        constexpr std::size_t maximumNumericAddress = 45;

        if (peer.address.empty() || peer.address.size() > maximumNumericAddress ||
            peer.address.find_first_of(std::string_view("\0%", 2)) != std::string::npos)
            throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-demand-peer-address"});

        asio::error_code error;
        const auto address = asio::ip::make_address(peer.address, error);

        if (error)
            throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-demand-peer-address"});

        const auto bytes = address.is_v4() ? asio::ip::make_address_v6(asio::ip::v4_mapped, address.to_v4()).to_bytes()
                                           : address.to_v6().to_bytes();

        if (std::ranges::find(result.peers, bytes, &PeerDemandPolicy::address) != result.peers.end())
            throw Error({ErrorCode::INVALID_ARGUMENT, "publisher-duplicate-demand-peer"});

        result.peers.push_back({bytes, validatedDemandPolicy(peer.demand)});
    }

    return result;
}

using RequestedEntries = std::vector<protocol::IncomingSubscriptionEntry>;

struct PendingSubscription {
    mux::MuxCommandTicket ticket;
    std::size_t family;
    RequestedEntries requested;
};

struct HistoryContinuity {
    core::RecordContext context;
    std::u16string symbol;
    bool snapshot = false;
    bool transaction = false;
};

struct Peer {
    transport::TcpConnectionId id;
    protocol::LiveSession session;
    DemandPolicy demand;
    transport::TcpPeerAddress address;
    std::uint64_t policyRevision = 0;
    std::array<RequestedEntries, FAMILY_COUNT> requested;
    std::vector<HistoryContinuity> openHistory;
    std::array<std::shared_ptr<Delivery>, FAMILY_COUNT> delivery;
    std::array<mux::MuxHandle, FAMILY_COUNT> handles;
    std::optional<PendingSubscription> command;
    std::optional<protocol::LivePendingOutput> output;
    Clock::time_point acceptedAt;
    Clock::time_point lastWrite;
    Clock::time_point heartbeatAt;
    OutputKind outputKind = OutputKind::PROTOCOL;
    std::size_t outputFamily = 0;
    std::size_t outputEnd = 0;
    std::size_t outputRecordCount = 0;
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

/** Borrow the existing worker fields without changing their ownership or lifetime.
 * @param peer Serialized connection owner, alive through the immediate delivery operation.
 * @return Borrowed output references that must not outlive the peer or escape the worker turn.
 */
detail::DeliveryOutputView deliveryOutput(Peer &peer) noexcept {
    return {peer.output, peer.outputKind, peer.outputFamily, peer.outputEnd, peer.outputRecordCount};
}

} // namespace

namespace detail {
struct PublisherPeerIdentityAccess {
    static PublisherPeerId make(transport::TcpConnectionId value) noexcept {
        PublisherPeerId result;
        result.connection_ = value;

        return result;
    }

    static transport::TcpConnectionId get(PublisherPeerId value) noexcept {
        return value.connection_;
    }
};

struct PublisherPolicyCompletion {
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    std::optional<PublisherPolicyResult> result;

    void complete(PublisherPolicyResult outcome) noexcept {
        const std::lock_guard lock(mutex);
        result = outcome;
        changed.notify_all();
    }
};

struct PublisherPolicyCommand {
    transport::TcpConnectionId id;
    DemandPolicy demand;
    std::shared_ptr<PublisherPolicyCompletion> completion;
    Clock::time_point deadline;
};

/** Detached bounded producer state. Immutable binding and credits contain no native/runtime owner. */
struct PublisherPort {
    std::mutex mutex;
    std::condition_variable changed;
    PublicationQueue publications;
    const PublisherRecordLimits recordLimits;
    core::detail::RecordByteBudget physical;
    core::detail::RecordByteBudget input;
    core::detail::RecordByteBudget recordCount;
    core::detail::RecordByteBudget committedCount;
    std::shared_ptr<const core::detail::RecordBlockBinding> binding;
    core::detail::RecordByteReservation bindingInput;
    std::vector<std::uint8_t> localHistory;
    std::array<std::vector<PublisherDemand>, FAMILY_COUNT> demand;
    std::array<PublisherPeer, MAX_PEERS> peers;
    std::size_t peerCount = 0;
    std::optional<PublisherPolicyCommand> policyCommand;
    std::atomic<bool> policyPending{false};
    const bool remote;
    const bool localRecords;
    const std::size_t families;
    const std::size_t streamBatchRecords;
    const bool byteStream;
    PublisherSourceStatus source;
    PublisherStatistics statistics;
    std::uint16_t boundPort = 0;
    bool started = false;
    bool closed = false;
    Diagnostic diagnostic;

    explicit PublisherPort(const PublisherOptions &options)
        : recordLimits(options.recordLimits), physical(publicationPhysicalCapacity(options)),
          input(publicationInputCapacity(options)), recordCount(options.pendingQuotes),
          committedCount(options.pendingQuotes), remote(options.source.has_value()),
          localRecords(!options.source && options.catalog), families(enabledFamilies(options)),
          streamBatchRecords(options.streamBatchRecords), byteStream(options.streamBytes.has_value()) {
        if (!remote) {
            auto schema = createSchema(options);

            if (localRecords) {
                localHistory = localHistoryRecords(schema, catalogNames(options, schema));
            }

            auto prepared = core::detail::RecordBlockBinding::create(std::move(schema), {}, physical);

            if (!prepared) {
                throw Error(blockDiagnostic(prepared.error()));
            }

            binding = std::move(*prepared);
            bindingInput = core::detail::RecordByteReservation(input.state(), binding->retainedCapacityBytes());
        }

        for (std::size_t family = families; family < FAMILY_COUNT; ++family) {
            auto &stats = statistics.families[family];
            stats.ingressFrames.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.ingressRecords.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.sentFrames.availability = core::StatisticAvailability::UNSUPPORTED;
        }
    }
};
} // namespace detail

namespace {
bool awaitPublishedDemand(detail::PublisherPort &port, Family family, std::u16string_view recordName,
                          std::u16string_view symbol, bool subscribed, Clock::time_point deadline) {
    const auto index = familyIndex(family);

    if (!index || *index >= port.families)
        return false;

    std::unique_lock lock(port.mutex);
    const auto &demand = port.demand[*index];

    return port.changed.wait_until(lock, deadline, [&] {
        if (port.closed)
            return true;

        return std::ranges::any_of(demand, [&](const auto &entry) {
                   return entry.symbol == symbol && (recordName.empty() || entry.recordName == recordName);
               }) == subscribed;
    }) && !port.closed;
}

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

bool supportsRecord(const core::LocalSchema &schema, std::size_t family, core::RecordContext context, bool catalog,
                    std::span<const std::uint8_t> localHistory = {}) noexcept {
    const auto id = schema.recordId(context);

    if (!id)
        return false;

    if (!catalog)
        return *id == RECORD_IDS[family];

    return FAMILIES[family] != Family::HISTORY ||
           (schema.records()[*id].hasTime && (localHistory.empty() || localHistory[*id] != 0));
}

/** One upstream connection. Its raw output slot copies and owns the whole frame through the local write barrier. */
class SourceConnection {
    const core::LocalSchema &schema_;
    const std::span<const std::int32_t> recordIds_;
    const bool catalog_;
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

    static protocol::LiveSession newSession(const core::LocalSchema &schema, CatalogNames names,
                                            protocol::LiveWork &work) {
        auto writer = publisherWriter(schema, names, core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
        const bool mixed = !names.empty() || schema.records().size() > 1;
        auto limits = liveLimits(mixed, !names.empty());
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
                    return !supportsRecord(schema_, family, record.context, catalog_);
                }))
                throw Error({ErrorCode::PROTOCOL_FAILURE, "publisher-upstream-record-family"});

            if (!records.empty())
                pendingEvent_ = std::move(incoming.event);
        }
    }

    public:
    SourceConnection(const core::LocalSchema &schema, CatalogNames names, std::span<const std::int32_t> recordIds,
                     std::size_t familyCount, const transport::detail::BootstrapLease &bootstrap,
                     PublisherSourceOptions options, Clock::time_point now, protocol::LiveWork &work)
        : schema_(schema), recordIds_(recordIds), catalog_(!names.empty()), session_(newSession(schema, names, work)),
          bytes_(newTransport(bootstrap, options, work)), handshakeDeadline_(now + HANDSHAKE_TIMEOUT),
          heartbeatAt_(now + HEARTBEAT_PERIOD), familyCount_(familyCount) {
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
        auto work = liveTurn(catalog_);
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
            send(session_.describe(recordIds_, work), work);
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
    const std::vector<std::int32_t> recordIds_;
    const std::vector<std::vector<std::u16string>> catalogNames_;
    std::vector<std::uint8_t> localHistory_;
    std::optional<protocol::detail::LocalEventOwner> source_;
    const mux::ClientLimits clientLimits_;
    const mux::ClientLimits streamClientLimits_;
    const std::size_t streamSourceRows_;
    const std::size_t streamDeliveryRows_;
    const std::optional<PublisherStreamByteLimits> streamBytes_;
    std::vector<protocol::detail::LocalStreamRow> sourceViews_;
    std::vector<protocol::LiveDataRecordView> outgoingViews_;
    QueueSet queues_;
    transport::TcpListener listener_;
    std::vector<std::optional<Peer>> peers_;
    const DemandPolicies demandPolicies_;
    std::optional<protocol::LiveEvent> pendingSource_;

    detail::PublicationCursor cursor_;
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

    void validateCatalog() const {
        auto work = liveTurn(!catalogNames_.empty());
        auto session = serverSession(schema_, catalogNames_, work, streamDeliveryRows_);
        const auto description = session.describe(recordIds_, work);

        if (!description)
            throw Error(protocolDiagnostic(description.error(), "publisher-catalog-description"));
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
            const auto address = listener_.peerAddress(**accepted);

            if (!address)
                throw Error(nativeDiagnostic(address.error()));

            auto demand = demandPolicies_.defaults;
            const auto found = std::ranges::find(demandPolicies_.peers, *address, &PeerDemandPolicy::address);

            if (found != demandPolicies_.peers.end())
                demand = found->demand;

            auto liveWork = liveTurn(!catalogNames_.empty());
            auto session = serverSession(schema_, catalogNames_, liveWork, streamDeliveryRows_);
            const auto now = Clock::now();
            Peer peer{.id = **accepted,
                      .session = std::move(session),
                      .demand = std::move(demand),
                      .address = *address,
                      .acceptedAt = now,
                      .lastWrite = now,
                      .heartbeatAt = now + HEARTBEAT_PERIOD};
            peer.openHistory.reserve(CLIENT_KEYS);

            for (std::size_t family = 0; family < familyCount_; ++family) {
                auto work = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), !catalogNames_.empty(),
                                            familyCount_, recordIds_.size());
                peer.delivery[family] = std::make_shared<Delivery>();
                auto limits = family == STREAM_FAMILY ? streamClientLimits_ : clientLimits_;

                if (family == STREAM_FAMILY)
                    limits.batchRecords = streamDeliveryRows_;

                auto handle = queues_[family]->open(limits, peer.delivery[family], work);

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
                peer.delivery[family]->clear();
                (void)peer.handles[family].requestStop(mux::StopMode::ABORT);
            }

            peer.openHistory.clear();
        }

        if (!peer.socketClosed)
            peer.socketClosed = listener_.retire(peer.id).has_value();
    }

    static bool sameRequested(const protocol::IncomingSubscriptionEntry &left,
                              const protocol::IncomingSubscriptionEntry &right) noexcept {
        return left.context.matches(right.context) && left.symbol == right.symbol;
    }

    bool allowed(const DemandPolicy &policy, std::size_t family, const core::RecordContext &context,
                 std::u16string_view symbol) const {
        return detail::publisherRecordAllowed(policy[family], schema_, context, symbol);
    }

    bool finishSubscription(Peer &peer) noexcept {
        if (peer.command) {
            const auto applied = peer.command->ticket.result();

            if (!applied)
                return true;

            if (applied->status != mux::CommandStatus::APPLIED)
                return false;

            peer.requested[peer.command->family] = std::move(peer.command->requested);
            peer.command.reset();
        }

        return true;
    }

    bool receive(Peer &peer, protocol::LiveWork &work) {
        if (!finishSubscription(peer))
            return false;

        if (peer.command)
            return true;

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
            const auto catalog = !catalogNames_.empty();
            std::erase_if(result.subscription->entries, [&](const auto &entry) {
                return !supportsRecord(schema_, *family, entry.context, catalog, localHistory_);
            });

            RequestedEntries requested;
            const bool adding = result.subscription->message == ADD_MESSAGES[*family];
            const auto &removals = result.subscription->entries;

            try {
                requested = peer.requested[*family];

                if (adding) {
                    auto &entries = result.subscription->entries;

                    for (auto current = entries.begin(); current != entries.end(); ++current) {
                        const auto duplicate = std::find_if(entries.begin(), current, [&](const auto &earlier) {
                            return sameRequested(earlier, *current);
                        });

                        if (duplicate != current && duplicate->historyBound != current->historyBound)
                            return false;

                        const auto previous = std::ranges::find_if(requested, [&](const auto &entry) {
                            return sameRequested(entry, *current);
                        });

                        if (previous == requested.end()) {
                            if (requested.size() == CLIENT_KEYS)
                                return false;

                            requested.push_back(*current);
                        } else {
                            previous->historyBound = current->historyBound;
                        }
                    }

                    std::erase_if(entries, [&](const auto &entry) {
                        return !allowed(peer.demand, *family, entry.context, entry.symbol);
                    });
                } else {
                    std::erase_if(requested, [&](const auto &entry) {
                        return std::ranges::any_of(removals, [&](const auto &removal) {
                            return sameRequested(entry, removal);
                        });
                    });
                }
            } catch (const Error &) {
                return false;
            } catch (const std::bad_alloc &) {
                return false;
            }

            mux::QueueSubmitWork submit{8, 65536, 65536, 1024 * 1024};
            auto ticket =
                queues_[*family]->submitIncomingSubscription(peer.handles[*family], *result.subscription, submit);

            if (!ticket)
                return false;

            if (!adding && FAMILIES[*family] == Family::HISTORY) {
                std::erase_if(peer.openHistory, [&](const auto &entry) {
                    return std::ranges::any_of(removals, [&](const auto &removal) {
                        return removal.context.matches(entry.context) && removal.symbol == entry.symbol;
                    });
                });
            }

            peer.command.emplace(std::move(*ticket), *family, std::move(requested));
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
            const auto properties = familyCount_ == FAMILY_COUNT
                                        ? std::span<const protocol::LivePropertyView>(HISTORY_OPTIONS)
                                        : std::span<const protocol::LivePropertyView>{};
            output = peer.session.composeProtocol({properties, std::span(SERVER_SEND).first(3 + familyCount_),
                                                   std::span(SERVER_RECEIVE).first(3 + 2 * familyCount_)},
                                                  work);
        } else if (!peer.sentDescription && std::ranges::any_of(peer.ready, std::identity{})) {
            kind = OutputKind::DESCRIPTION;
            output = peer.session.describe(recordIds_, work);
        } else {
            if (peer.sentDescription) {
                for (std::size_t i = 0; i < familyCount_; ++i) {
                    const auto family = (peer.nextFamily + i) % familyCount_;
                    auto &delivery = *peer.delivery[family];
                    const auto records = delivery.batch.records();

                    if (!records.empty()) {
                        if (peer.demand[family]) {
                            try {
                                for (auto row = delivery.cursor; row < records.size(); ++row) {
                                    if (!delivery.suppressed[row] &&
                                        !allowed(peer.demand, family, records[row].context, records[row].symbol))
                                        delivery.suppressed.set(row);
                                }
                            } catch (const Error &) {
                                return false;
                            }
                        }

                        detail::releaseSuppressedPrefix(*peer.delivery[family], peer.handles[family]);

                        if (delivery.batch.records().empty())
                            continue;

                        kind = OutputKind::DATA;
                        peer.outputFamily = family;
                        peer.nextFamily = (family + 1) % familyCount_;

                        if (family == STREAM_FAMILY && streamBytes_) {
                            output = detail::composeDelivery(delivery, deliveryOutput(peer), peer.session, family,
                                                             schema_, streamBytes_, outgoingViews_, work);

                            if (!output)
                                statistics_.families[family].failedFrames.add(1);
                        } else {
                            output = detail::composeDelivery(delivery, deliveryOutput(peer), peer.session, family,
                                                             schema_, {}, outgoingViews_, work);
                        }

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

    bool trackHistory(Peer &peer, const mux::MuxRecordView &record) {
        using namespace events::indexed_event_flags;
        auto previous = std::ranges::find_if(peer.openHistory, [&](const auto &entry) {
            return entry.context.matches(record.context) && entry.symbol == record.symbol;
        });
        const auto flags = record.eventFlags;
        const bool hasPrevious = previous != peer.openHistory.end();
        const bool snapshot = (flags & (SNAPSHOT_END | SNAPSHOT_SNIP)) == 0 &&
                              ((flags & SNAPSHOT_BEGIN) != 0 || (hasPrevious && previous->snapshot));
        const bool transaction = (flags & TX_PENDING) != 0;

        if (hasPrevious) {
            if (snapshot || transaction) {
                previous->snapshot = snapshot;
                previous->transaction = transaction;
            } else {
                peer.openHistory.erase(previous);
            }
        } else if (snapshot || transaction) {
            if (peer.openHistory.size() == CLIENT_KEYS)
                return false;

            peer.openHistory.emplace_back(record.context, std::u16string(record.symbol), snapshot, transaction);
        }

        return true;
    }

    bool send(Peer &peer, Clock::time_point now, protocol::LiveWork &work, bool &streamAdvanced) {
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

        const auto frameBytes = peer.output->bytes().size();
        peer.output.reset();

        switch (peer.outputKind) {
        case OutputKind::PROTOCOL:
            peer.sentProtocol = true;
            break;
        case OutputKind::DESCRIPTION:
            peer.sentDescription = true;
            break;
        case OutputKind::DATA: {
            auto &stats = statistics_.families[peer.outputFamily];
            stats.sentFrames.add(1);
            stats.maxSentFrameRecords = std::max(stats.maxSentFrameRecords, peer.outputRecordCount);
            stats.maxSentFrameBytes = std::max(stats.maxSentFrameBytes, frameBytes);
            streamAdvanced |= streamBytes_ && peer.outputFamily == STREAM_FAMILY;

            if (FAMILIES[peer.outputFamily] == Family::HISTORY) {
                try {
                    if (!trackHistory(peer, peer.delivery[peer.outputFamily]->batch.records().front()))
                        return false;
                } catch (const std::bad_alloc &) {
                    return false;
                }
            }

            peer.delivery[peer.outputFamily]->cursor = peer.outputEnd;
            detail::releaseSuppressedPrefix(*peer.delivery[peer.outputFamily], peer.handles[peer.outputFamily]);
            break;
        }
        case OutputKind::HEARTBEAT:
            break;
        }

        peer.heartbeatAt = now + HEARTBEAT_PERIOD;

        return true;
    }

    bool progressPeers() {
        const auto now = Clock::now();
        bool streamAdvanced = false;

        for (auto &entry : peers_) {
            if (!entry)
                continue;

            auto &peer = *entry;
            auto work = liveTurn(!catalogNames_.empty());

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
                !receive(peer, work) || !send(peer, now, work, streamAdvanced)) {
                retire(peer);

                if (peer.socketClosed && std::ranges::all_of(handles, [](const auto &handle) {
                        return handle.terminal().has_value();
                    }))
                    entry.reset();
            }
        }

        return streamAdvanced;
    }

    std::vector<PublisherDemand> demandSnapshot(std::span<const core::SubscriptionRequest> entries) const {
        const auto records = schema_.records();
        std::vector<PublisherDemand> demand;
        demand.reserve(entries.size());

        for (const auto &entry : entries) {
            const auto id = schema_.recordId(entry.key.context);

            if (!id)
                throw Error({ErrorCode::INVALID_STATE, "publisher-demand-record"});

            demand.emplace_back(records[static_cast<std::size_t>(*id)].name, std::u16string(entry.key.symbol),
                                entry.historyBound);
        }

        return demand;
    }

    void publishDemand(detail::PublisherPort &port, std::size_t family) {
        const auto &queue = *queues_[family];
        const auto revision = queue.revision();

        if (publishedRevision_[family] == revision)
            return;

        auto demand = demandSnapshot(queue.aggregateEntries());
        {
            const std::lock_guard lock(port.mutex);
            port.demand[family] = std::move(demand);
        }

        publishedRevision_[family] = revision;
        port.changed.notify_all();
    }

    struct PreparedPolicy {
        const PublisherServer &server;
        Clock::time_point deadline;
        std::array<std::vector<PublisherDemand>, FAMILY_COUNT> demand;

        static std::expected<void, mux::QueueFailure> stage(void *argument, std::size_t family,
                                                            std::span<const core::SubscriptionRequest> aggregate) {
            auto &prepared = *static_cast<PreparedPolicy *>(argument);

            if (Clock::now() >= prepared.deadline)
                return std::unexpected(mux::QueueFailure{mux::QueueError::BUSY, {}});

            try {
                prepared.demand[family] = prepared.server.demandSnapshot(aggregate);
            } catch (const std::bad_alloc &) {
                return std::unexpected(mux::QueueFailure{mux::QueueError::ALLOCATION_FAILURE, {}});
            } catch (const Error &) {
                return std::unexpected(mux::QueueFailure{mux::QueueError::INVALID_STATE, {}});
            }

            return {};
        }
    };

    // Caller holds the port mutex, including when publishing a completed replacement.
    void snapshotPeers(detail::PublisherPort &port) const noexcept {
        port.peerCount = 0;

        for (const auto &peer : peers_) {
            if (peer && !peer->closing) {
                port.peers[port.peerCount++] = {detail::PublisherPeerIdentityAccess::make(peer->id), peer->address,
                                                peer->policyRevision};
            }
        }
    }

    void completePolicy(detail::PublisherPort &port, PublisherPolicyResult outcome) noexcept {
        port.policyCommand->completion->complete(outcome);
        port.policyCommand.reset();
        port.policyPending.store(false, std::memory_order_release);
        port.changed.notify_all();
    }

    void progressPolicy(detail::PublisherPort &port) {
        if (!port.policyPending.load(std::memory_order_acquire))
            return;

        auto input = [&] {
            const std::lock_guard lock(port.mutex);

            return port.closed ? std::optional<detail::PublisherPolicyCommand>{} : port.policyCommand;
        }();

        if (!input)
            return;

        auto found = std::ranges::find_if(peers_, [&](const auto &peer) {
            return peer && !peer->closing && peer->id == input->id;
        });
        const auto reject = [&](PublisherPolicyOutcome outcome, Diagnostic diagnostic) {
            const std::lock_guard lock(port.mutex);
            snapshotPeers(port);
            completePolicy(port, port.closed ? PublisherPolicyResult{PublisherPolicyOutcome::PEER_CLOSED, 0, CLOSED}
                                             : PublisherPolicyResult{outcome, 0, diagnostic});
        };

        if (found == peers_.end()) {
            reject(PublisherPolicyOutcome::PEER_CLOSED, CLOSED);

            return;
        }

        auto &peer = **found;

        if (!finishSubscription(peer)) {
            retire(peer);
            reject(PublisherPolicyOutcome::PEER_CLOSED, CLOSED);

            return;
        }

        if (Clock::now() >= input->deadline) {
            reject(PublisherPolicyOutcome::REJECTED, {ErrorCode::REQUEST_TIMED_OUT, "publisher-policy-timeout"});

            return;
        }

        if (peer.command)
            return;

        if (peer.policyRevision == std::numeric_limits<std::uint64_t>::max()) {
            reject(PublisherPolicyOutcome::REJECTED, {ErrorCode::RESOURCE_LIMIT, "publisher-policy-revision"});

            return;
        }

        std::array<DeliveryMask, FAMILY_COUNT> revoked{};
        std::array<std::array<core::SubscriptionRequest, CLIENT_KEYS>, FAMILY_COUNT> requests;
        std::array<mux::detail::QueueReplacement, FAMILY_COUNT> replacements{};
        WorkSet work{};
        std::size_t selectors = 0;

        try {
            for (std::size_t family = 0; family < familyCount_; ++family) {
                auto &lane = requests[family];
                std::size_t count = 0;

                for (const auto &entry : peer.requested[family]) {
                    if (allowed(input->demand, family, entry.context, entry.symbol))
                        lane[count++] = {{entry.context, entry.symbol}, entry.historyBound};
                }

                work[family] = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), !catalogNames_.empty(),
                                               familyCount_, recordIds_.size());
                replacements[family] = {&*queues_[family], &peer.handles[family], std::span(lane).first(count),
                                        &work[family]};
                selectors += count;
                detail::collectDeliveryRevocations(*peer.delivery[family], input->demand[family], schema_,
                                                   revoked[family]);
            }

            constexpr std::size_t history = 2;
            const auto losesHistory = std::ranges::any_of(peer.openHistory, [&](const auto &entry) {
                return !allowed(input->demand, history, entry.context, entry.symbol);
            });
            const auto losesPartialFrame = detail::revokesPartialOutput(
                deliveryOutput(peer), *peer.delivery[peer.outputFamily], peer.outputFamily, revoked[peer.outputFamily]);

            if (losesHistory || losesPartialFrame) {
                retire(peer);
                reject(PublisherPolicyOutcome::PEER_CLOSED, {ErrorCode::CLOSED, "publisher-policy-wire-boundary"});

                return;
            }
        } catch (const Error &failure) {
            reject(PublisherPolicyOutcome::REJECTED, failure.diagnostic());

            return;
        }

        const std::lock_guard lock(port.mutex);

        if (port.closed) {
            completePolicy(port, {PublisherPolicyOutcome::PEER_CLOSED, 0, CLOSED});

            return;
        }

        PreparedPolicy prepared{*this, input->deadline, {}};
        const auto revision = peer.policyRevision + 1;
        const auto replaced = mux::detail::QueueReplaceAccess::replace(
            std::span(replacements).first(familyCount_), selectors, revision, PreparedPolicy::stage, &prepared);

        if (!replaced) {
            if (replaced.error().error != mux::QueueError::BUSY) {
                completePolicy(
                    port, {PublisherPolicyOutcome::REJECTED, 0, queueDiagnostic(replaced.error(), "publisher-policy")});
            }

            return;
        }

        peer.demand = std::move(input->demand);
        peer.policyRevision = revision;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            detail::applyDeliveryRevocation(*peer.delivery[family], peer.handles[family], deliveryOutput(peer), family,
                                            revoked[family]);

            publishedRevision_[family] = queues_[family]->revision();
        }

        port.demand = std::move(prepared.demand);
        snapshotPeers(port);
        completePolicy(port, {PublisherPolicyOutcome::APPLIED, revision, {}});
    }

    /** Guard the next STREAM source window against detached record pressure.
     * @param allPeers Require headroom at every subscribed peer before collecting another group without socket
     * progress.
     * @return Whether the selected peer readiness condition allows source progress.
     */
    bool canAdvanceStream(bool allPeers = false) const noexcept {
        const auto &queue = *queues_[STREAM_FAMILY];

        if (mux::detail::QueueScheduleAccess::pendingRecords(queue) != 0)
            return false;

        bool blocked = false;

        for (const auto &peer : peers_) {
            if (!peer || peer->closing || peer->requested[STREAM_FAMILY].empty())
                continue;

            const auto resources = mux::detail::QueueScheduleAccess::client(queue, peer->handles[STREAM_FAMILY]);

            if (!resources || resources->state == mux::QueueClientState::TERMINAL)
                continue;

            const bool ready = resources->recordOwners <= streamDeliveryRows_ &&
                               resources->recordBytes <= streamClientLimits_.recordBytes / 2;

            if (allPeers && !ready)
                return false;

            if (!allPeers && ready)
                return true;

            blocked |= !ready;
        }

        return !blocked;
    }

    /** Fund a complete existing source window and its normal callback rotation. */
    void fundStreamWindow(mux::QueueWork &work) const noexcept {
        const auto copies = peers_.size() * (2 * streamDeliveryRows_ + streamSourceRows_);
        const auto fields = copies * streamClientLimits_.recordFields;
        const auto bytes = peers_.size() * streamClientLimits_.recordBytes;
        constexpr auto projectionPasses = 1 + core::MAX_DELIVERY_PROJECTIONS;
        work.interest.operations += copies * projectionPasses;
        work.interest.visits += copies * (recordIds_.size() + 2 * projectionPasses * CLIENT_KEYS);
        work.interest.symbolUnits += copies * (1 + 3 * CLIENT_KEYS * SYMBOL_UNITS);
        work.stream.textUnits += 4 * copies * CLIENT_KEYS * SYMBOL_UNITS;
        work.fields += 4 * fields;
        work.visits += fields + 4 * copies;
        work.textUnits += 3 * bytes / sizeof(char16_t);
        work.bytes += 5 * bytes + 2 * peers_.size() * streamClientLimits_.batchBytes;
    }

    /** One borrowed source selection; its immutable owner is shared without rebuilding row payloads. */
    struct LocalSourceSelection {
        std::shared_ptr<const core::detail::BoundRecordBatch> block; ///< Original or independently reduced backing.
        std::size_t first;                                           ///< First selected row in this backing.
        std::size_t count;  ///< Selected rows, bounded by the existing service quantum.
        std::size_t charge; ///< Payload and per-row retention allowance for the selected source event.
    };

    /** Select available rows without charging a later publication beyond the remaining window.
     * @param port Detached physical capacity for a genuinely smaller retained range.
     * @param publication Current worker-owned publication, never a mailbox borrow.
     * @param first First unconsumed row of this publication.
     * @param maximum Remaining rows in the existing service quantum.
     * @param remainingBytes Remaining source-window retention allowance, excluding its fixed event allowance.
     * @param empty Whether no row has been selected, allowing the existing single-row soft-window exception.
     * @return Ready selection, or no selection when a later publication must wait for the next window.
     */
    std::optional<LocalSourceSelection> selectLocalSource(detail::PublisherPort &port,
                                                          const detail::Publication &publication, std::size_t first,
                                                          std::size_t maximum, std::size_t remainingBytes,
                                                          bool empty) const {
        const auto &block = *publication.block;
        auto count = std::min(maximum, block.count - first);

        if (!streamBytes_ || publication.family != STREAM_FAMILY) {
            return LocalSourceSelection{publication.block, first, count, 0};
        }

        const auto perRow = RECORD_COPY_PADDING + EVENT_OWNER_ALLOWANCE;
        const auto retained = block.rows[0].residentBytes();
        const auto charge = core::detail::recordByteSum(retained, count * perRow);

        if (charge <= remainingBytes) {
            return LocalSourceSelection{publication.block, first, count, charge};
        }

        if (!empty) {
            if (retained >= remainingBytes || perRow > remainingBytes - retained) {
                return std::nullopt;
            }

            count = std::min(count, (remainingBytes - retained) / perRow);

            return LocalSourceSelection{publication.block, first, count,
                                        core::detail::recordByteSum(retained, count * perRow)};
        }

        struct Selection {
            const core::detail::BoundRecordBatch &block;
            std::size_t first;
            std::size_t count;

            std::size_t size() const noexcept {
                return count;
            }

            core::detail::RecordBlockInput operator[](std::size_t index) const {
                const auto row = first + index;
                const auto &metadata = block.metadata[row];

                return {block.binding->schema.recordContext(metadata.localId), block.rows[row].symbol(),
                        block.rows[row].fields(), metadata.source};
            }
        };

        const auto fits = [&](std::size_t rows) {
            const auto measured =
                core::detail::SharedRecordBlock::measure(*block.binding, Selection{block, first, rows});

            if (!measured) {
                throw Error(blockDiagnostic(measured.error()));
            }

            const auto bytes =
                core::detail::recordByteSum(measured->backingAllocationBytes, measured->controlAllocationBytes);

            return core::detail::recordByteSum(bytes, rows * perRow) <= remainingBytes;
        };
        std::size_t low = 1, high = count;

        while (low < high) {
            const auto middle = low + (high - low + 1) / 2;

            if (fits(middle)) {
                low = middle;
            } else {
                high = middle - 1;
            }
        }

        count = low;
        auto selected = detail::selectPublicationRange(publication.block, first, count, port.physical);

        if (!selected) {
            throw Error(blockDiagnostic(selected.error()));
        }

        const auto selectedCharge = core::detail::recordByteSum((*selected)->rows[0].residentBytes(), count * perRow);

        return LocalSourceSelection{std::move(*selected), 0, count, selectedCharge};
    }

    /** Append only already-ready ownership to a detached tail, without waiting to fill a quantum.
     * @param port Commit/close fence protecting the ready mailbox.
     * @param tail Worker-owned tail with no successor.
     */
    void appendReadyLocal(detail::PublisherPort &port, detail::Publication &tail) const {
        std::shared_ptr<detail::Publication> ready;
        {
            const std::lock_guard lock(port.mutex);

            if (!port.closed) {
                ready = port.publications.detach();
            }
        }
        tail.next = std::move(ready);
    }

    /** Keep each complete input reservation through every partial collector selection. */
    bool prepareLocal(detail::PublisherPort &port, bool streamOnly) {
        if (cursor_.applyingFamily) {
            if (mux::detail::QueueScheduleAccess::pendingRecords(*queues_[*cursor_.applyingFamily]) != 0) {
                return false;
            }

            cursor_.collected();
        }

        if (!cursor_.publication) {
            const std::lock_guard lock(port.mutex);

            if (port.closed) {
                return false;
            }

            cursor_.publication = port.publications.detach();
        }

        if (!cursor_.publication) {
            return false;
        }

        const auto family = cursor_.publication->family;

        if ((streamOnly && family != STREAM_FAMILY) ||
            (streamBytes_ && family == STREAM_FAMILY && !canAdvanceStream())) {
            return false;
        }

        const auto maximum = family == STREAM_FAMILY ? streamSourceRows_ : std::size_t{1};
        const auto window = streamBytes_ && family == STREAM_FAMILY ? streamClientLimits_.recordBytes / 2 : 0;
        auto charged = std::size_t{2 * EVENT_OWNER_ALLOWANCE};
        const auto select = [&](const detail::Publication &publication, std::size_t first, std::size_t remainingRows,
                                std::size_t offset) {
            const auto remainingBytes = charged < window ? window - charged : 0;
            auto selected = selectLocalSource(port, publication, first, remainingRows, remainingBytes, offset == 0);

            if (!selected) {
                return std::size_t{0};
            }

            for (std::size_t i = 0; i < selected->count; ++i) {
                const auto index = selected->first + i;
                const auto &payload = selected->block->rows[index];
                const auto &metadata = selected->block->metadata[index];
                sourceViews_[offset + i] = {metadata.localId,
                                            schema_.recordContext(metadata.localId),
                                            payload.symbol(),
                                            metadata.source.removalTime ? std::span<const core::FieldValueView>{}
                                                                        : payload.fields(),
                                            core::detail::SharedRecordPayload::row(selected->block, index),
                                            metadata.source.flags,
                                            metadata.source.removalTime};
            }

            charged = core::detail::recordByteSum(charged, selected->charge);

            return selected->count;
        };
        const auto count = detail::selectReadyPublicationRows(cursor_, maximum, select, [&](detail::Publication &tail) {
            appendReadyLocal(port, tail);
        });

        if (count == 0) {
            return false;
        }

        auto work = liveTurn(true);
        auto event = source_->stream(std::span(sourceViews_).first(count), work, true, DATA_MESSAGES[family]);

        for (std::size_t i = 0; i < count; ++i) {
            sourceViews_[i] = {};
        }

        if (!event) {
            throw Error(protocolDiagnostic(event.error(), "publisher-local-publication"));
        }

        cursor_.staged(count);
        pendingSource_ = std::move(*event);

        return true;
    }

    /** Admit the next local owner if its queue can accept it.
     * @param port Bounded producer mailbox.
     * @param work Remaining queue credits for this turn.
     * @param streamOnly Leave other families in FIFO order for normal progress.
     * @return Whole admitted record count, or zero if the source cannot advance.
     */
    std::size_t publishLocal(detail::PublisherPort &port, WorkSet &work, bool streamOnly = false) {
        if (!pendingSource_) {
            prepareLocal(port, streamOnly);

            if (!pendingSource_)
                return 0;
        }

        const auto message = static_cast<DataMessage>(*pendingSource_->messageType());
        const auto family = static_cast<std::size_t>(std::ranges::find(DATA_MESSAGES, message) - DATA_MESSAGES.begin());

        if (streamOnly && family != STREAM_FAMILY)
            return 0;

        const auto records = pendingSource_->records().size();
        const auto admitted = queues_[family]->admit(std::move(*pendingSource_), work[family]);

        if (!admitted) {
            if (admitted.error().error == mux::QueueError::BUSY ||
                admitted.error().error == mux::QueueError::WORK_LIMIT)
                return 0;

            throw Error(queueDiagnostic(admitted.error(), "publisher-admit"));
        }

        statistics_.families[family].ingressFrames.add(1);
        statistics_.families[family].ingressRecords.add(records);
        pendingSource_.reset();

        return records;
    }

    /** Route ready source groups before detaching the next byte-bounded client batch.
     * @param port Bounded producer mailbox; this method never waits for another admission.
     * @param work Shared finite turn credits, including the regular callback rotation.
     * @param callbackSteps Reserve at least this many attempts for the subsequent normal rotation.
     * @return Whether source admission or queue work advanced.
     */
    bool collectStream(detail::PublisherPort &port, WorkSet &work, std::size_t callbackSteps) {
        auto &queue = *queues_[STREAM_FAMILY];
        auto &credits = work[STREAM_FAMILY];
        auto remaining = streamDeliveryRows_;
        bool advanced = false;
        bool collectedGroup = mux::detail::QueueScheduleAccess::pendingRecords(queue) != 0;

        while (remaining && credits.operations > callbackSteps) {
            if (mux::detail::QueueScheduleAccess::pendingRecords(queue) == 0) {
                // Flush ready selections before collecting more for a peer that needs another socket turn.
                if (collectedGroup && !canAdvanceStream(true))
                    break;

                const auto admitted = publishLocal(port, work, true);

                if (!admitted &&
                    (!pendingSource_ || pendingSource_->messageType() != std::to_underlying(DataMessage::STREAM_DATA)))
                    break;

                remaining -= std::min(remaining, admitted);
                advanced |= admitted != 0;
                collectedGroup |= admitted != 0;
            }

            const auto moved = queue.progressWithoutCallbacks(credits);

            if (!moved) {
                if (moved.error().error == mux::QueueError::WORK_LIMIT)
                    break;

                throw Error(queueDiagnostic(moved.error(), "publisher-stream-collection"));
            }

            if (moved->kind == mux::QueueProgressKind::TERMINAL)
                break;

            advanced |= moved->kind != mux::QueueProgressKind::IDLE;
        }

        return advanced;
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

            if (!peer.openHistory.empty()) {
                retire(peer);
                continue;
            }

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
                    peer.delivery[family]->clear();
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

                auto liveWork = liveTurn(!catalogNames_.empty());
                upstream_.emplace(schema_, catalogNames_, recordIds_, familyCount_, *sourceBootstrap_, *sourceOptions_,
                                  now, liveWork);
            }

            upstream_->progress(queues_, sourceDemand_, now, work, demandWork, statistics_);
        } catch (const Error &failure) {
            if (sourceStatus_.connected || (upstream_ && upstream_->ready())) {
                fenceSource();

                for (std::size_t family = 0; family < familyCount_; ++family) {
                    auto resetWork =
                        serverQueueTurn(peers_.size(), true, !catalogNames_.empty(), familyCount_, recordIds_.size());
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
    PublisherServer(transport::RuntimeBootstrap &bootstrap, const PublisherOptions &options, DemandPolicies demand,
                    std::shared_ptr<const core::detail::RecordBlockBinding> binding)
        : schema_(binding ? core::LocalSchema(binding->schema) : createSchema(options)), recordIds_(recordIds(schema_)),
          catalogNames_(catalogNames(options, schema_)), clientLimits_(publisherClientLimits(options)),
          streamClientLimits_(publisherClientLimits(options, options.streamBytes.has_value())),
          streamSourceRows_(sourceWindowRows(options)), streamDeliveryRows_(deliveryWindowRows(options)),
          streamBytes_(options.streamBytes), sourceViews_(options.source ? 0 : streamSourceRows_),
          outgoingViews_(streamDeliveryRows_), listener_(listen(bootstrap, options.listener)),
          peers_(options.listener.maxConnections), demandPolicies_(std::move(demand)),
          familyCount_(enabledFamilies(options)) {
        if (streamBytes_)
            statistics_.families[STREAM_FAMILY].failedFrames.availability = core::StatisticAvailability::AVAILABLE;

        if (options.catalog)
            validateCatalog();

        if (options.catalog && !options.source)
            localHistory_ = localHistoryRecords(schema_, catalogNames_);

        for (std::size_t family = familyCount_; family < FAMILY_COUNT; ++family) {
            auto &stats = statistics_.families[family];
            stats.ingressFrames.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.ingressRecords.availability = core::StatisticAvailability::UNSUPPORTED;
            stats.sentFrames.availability = core::StatisticAvailability::UNSUPPORTED;
        }

        for (std::size_t family = 0; family < familyCount_; ++family)
            queues_[family].emplace(
                publisherQueue(schema_, peers_.size(), options.source.has_value(), FAMILIES[family],
                               !catalogNames_.empty(), family == STREAM_FAMILY ? streamClientLimits_ : clientLimits_,
                               streamSourceRows_, streamDeliveryRows_, binding ? binding->retainedCapacityBytes() : 0));

        if (!options.source) {
            for (std::size_t family = 0; family < familyCount_; ++family) {
                const auto bound = mux::detail::QueueScheduleAccess::bindPublicationSchema(
                    *queues_[family], binding, binding->retainedCapacityBytes());

                if (!bound) {
                    throw Error(queueDiagnostic(bound.error(), "publisher-fixed-binding"));
                }
            }

            source_.emplace(localOwner(!catalogNames_.empty(), familyCount_, streamSourceRows_, binding));

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

    [[nodiscard]] std::span<const std::uint8_t> localHistory() const noexcept {
        return localHistory_;
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return listener_.port();
    }

    /** Run one bounded turn across sockets and queues.
     * @param port Shared publisher mailbox and status snapshots.
     * @return Whether typed local publication or routing advanced and merits another turn before polling.
     */
    bool progress(detail::PublisherPort &port) {
        acceptPeer();
        progressPolicy(port);
        bool advanced = progressPeers();
        WorkSet work{};
        constexpr std::size_t DEFAULT_QUEUE_ACTIONS = 16;
        const bool driveLocal = !port.remote;
        // A sequence of IDLE visits may end before the next runnable slot, or before source admission reopens.
        const auto idleRotation = mux::detail::QUEUE_PHASES * peers_.size();
        const auto queueActions = driveLocal ? std::max(DEFAULT_QUEUE_ACTIONS, idleRotation) : DEFAULT_QUEUE_ACTIONS;
        const auto idleLimit = port.localRecords && driveLocal ? idleRotation : queueActions;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto &queue = *queues_[family];
            auto &turn = work[family];
            turn = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), !catalogNames_.empty(), familyCount_,
                                   recordIds_.size());

            if (family == STREAM_FAMILY && streamBytes_) {
                fundStreamWindow(turn);
                advanced |= collectStream(port, work, queueActions);
            }

            std::size_t idleVisits = 0;

            for (std::size_t i = 0; i < queueActions; ++i) {
                const auto moved = queue.progress(turn);

                if (!moved) {
                    if (moved.error().error == mux::QueueError::WORK_LIMIT)
                        break;

                    throw Error(queueDiagnostic(moved.error(), "publisher-progress"));
                }

                if (moved->kind == mux::QueueProgressKind::IDLE) {
                    // Visit every configured slot in every phase before yielding an idle local queue.
                    if (++idleVisits == idleLimit)
                        break;
                } else {
                    idleVisits = 0;
                    advanced = true;
                }
            }

            if (const auto terminal = queue.terminal())
                throw Error({ErrorCode::INVALID_STATE, "publisher-queue-terminal", 0,
                             static_cast<std::uint32_t>(terminal->outcome)});

            publishDemand(port, family);
        }

        if (sourceOptions_)
            progressSource(port, work);
        else if (publishLocal(port, work))
            advanced = true;

        statistics_.activePeers = 0;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto &familyStats = statistics_.families[family];
            const auto &queue = *queues_[family];
            familyStats.desiredKeys = queues_[family]->aggregateEntries().size();
            familyStats.pendingSourceRecords = mux::detail::QueueScheduleAccess::pendingRecords(queue);
            familyStats.deliveryRecordOwners = mux::detail::QueueScheduleAccess::dataItems(queue);
            familyStats.queuedDeliveryRecords = 0;
            familyStats.retainedOutputRecords = 0;

            for (const auto &peer : peers_) {
                if (!peer)
                    continue;

                if (const auto client = mux::detail::QueueScheduleAccess::client(queue, peer->handles[family]))
                    familyStats.queuedDeliveryRecords += client->queuedRecords;

                familyStats.retainedOutputRecords += peer->delivery[family]->batch.records().size();
            }
        }

        for (const auto &peer : peers_)
            statistics_.activePeers += peer && !peer->closing;

        {
            const std::lock_guard lock(port.mutex);
            statistics_.pendingPublications = port.committedCount.usedBytes();
            port.statistics = statistics_;
            snapshotPeers(port);
        }

        // Peer output runs before queue routing. Give newly routed records a send turn without a timer wait.
        return driveLocal && advanced;
    }

    /** Poll socket readiness on the existing cadence when a typed publication cannot advance locally.
     * @param port Shared mailbox checked under its producer mutex before sleeping.
     */
    void waitForWork(detail::PublisherPort &port) const {
        std::unique_lock lock(port.mutex);

        // An admission can arrive after progress() releases the mailbox and before we enter the wait.
        // An already held source owner must make queue progress before another admission can be taken.
        const bool canApply = !cursor_.applyingFamily ||
                              mux::detail::QueueScheduleAccess::pendingRecords(*queues_[*cursor_.applyingFamily]) == 0;

        if (port.closed || (!port.remote && canApply && !pendingSource_ &&
                            detail::runnablePublicationHead(cursor_, port.publications, [&](std::size_t family) {
                                return !streamBytes_ || family != STREAM_FAMILY || canAdvanceStream();
                            }))) {
            return;
        }

        port.changed.wait_for(lock, IO_CADENCE);
    }

    void stop() {
        const auto stopped = listener_.shutdown();

        if (!stopped)
            throw Error(nativeDiagnostic(stopped.error()));
    }
};

void runPublisher(detail::PublisherPort &port, transport::RuntimeBootstrap &bootstrap, const PublisherOptions &options,
                  DemandPolicies demand, std::stop_token stop) noexcept {
    Diagnostic diagnostic;

    try {
        PublisherServer server(bootstrap, options, std::move(demand), port.binding);
        {
            const std::lock_guard lock(port.mutex);
            port.boundPort = server.port();
            port.started = true;
        }
        port.changed.notify_all();

        while (!stop.stop_requested()) {
            if (!server.progress(port))
                server.waitForWork(port);
        }

        server.stop();
    } catch (const Error &failure) {
        diagnostic = failure.diagnostic();
    } catch (const std::bad_alloc &) {
        diagnostic = ALLOCATION;
    } catch (...) {
        diagnostic = {ErrorCode::INVALID_STATE, "publisher-worker"};
    }
    std::shared_ptr<detail::Publication> abandoned;
    {
        const std::lock_guard lock(port.mutex);
        port.closed = true;
        abandoned = port.publications.detach();
        port.source.connected = false;
        port.peerCount = 0;
        port.diagnostic = diagnostic;

        for (auto &demand : port.demand)
            std::vector<PublisherDemand>().swap(demand);

        if (port.policyCommand) {
            port.policyCommand->completion->complete({PublisherPolicyOutcome::PEER_CLOSED, 0, CLOSED});
            port.policyCommand.reset();
        }

        port.policyPending.store(false, std::memory_order_release);
    }
    detail::discardPublications(std::move(abandoned));
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
/** Reserve a whole call's row quota without hiding permanent byte oversize behind temporary row occupation.
 * @tparam Rows Stable indexed borrowed records accepted by SharedRecordBlock::measure.
 * @param port Detached admission domain.
 * @param rows Complete requested rows, scanned only when byte fit cannot be proved cheaply.
 * @param knownLayout Existing exact measurement, when the caller already validated this layout.
 * @return Move-only reservation or permanent/temporary detached diagnostic; never reserves twice.
 */
template <class Rows>
std::expected<core::detail::RecordByteReservation, Diagnostic>
reservePublicationRows(const std::shared_ptr<detail::PublisherPort> &port, const Rows &rows,
                       const core::detail::RecordBlockAccounting *knownLayout = nullptr) {
    using namespace core::detail;
    const auto admission = port->recordCount.state()->tryReserve(rows.size());

    if (admission) {
        return RecordByteReservation::adopt(port->recordCount.state(), rows.size());
    }

    if (admission.error() != RecordBlockError::WOULD_BLOCK) {
        return std::unexpected(blockDiagnostic(admission.error()));
    }

    const auto available = port->input.state()->capacity - port->binding->retainedCapacityBytes();
    bool byteFitGuaranteed = false;
    try {
        const auto logicalRowBytes = port->localRecords ? port->recordLimits.rowBytes : MAX_TYPED_ROW_LOGICAL_BYTES;
        byteFitGuaranteed = publicationRequestBound(rows.size(), logicalRowBytes) <= available;
    } catch (RecordBlockError) {
        // An overflowing upper bound proves nothing about the actual borrowed request.
    }
    try {
        if (!byteFitGuaranteed) {
            auto measured = knownLayout ? std::expected<RecordBlockAccounting, RecordBlockError>(*knownLayout)
                                        : SharedRecordBlock::measure(*port->binding, rows);

            if (!measured) {
                return std::unexpected(blockDiagnostic(measured.error()));
            }

            if (publicationRequestBytes(*measured) > available) {
                return std::unexpected(blockDiagnostic(RecordBlockError::CAPACITY_EXCEEDED));
            }
        }

        return std::unexpected(port->localRecords ? PENDING_RECORD_LIMIT
                                                  : Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-pending-quotes"});
    } catch (RecordBlockError error) {
        return std::unexpected(blockDiagnostic(error));
    } catch (Diagnostic diagnostic) {
        return std::unexpected(diagnostic);
    } catch (const std::bad_alloc &) {
        return std::unexpected(ALLOCATION);
    }
}

/** Reserve the complete INPUT request before data allocation, then commit at the close fence.
 * @tparam Rows Stable indexed borrowed input exposing exact schema-bound records.
 * @param port Detached immutable binding and pure reservation domains, retained through this call.
 * @param family Collector contract shared by every row.
 * @param rows Input borrowed only until synchronous preparation finishes.
 * @param reservedRows Optional already-reserved bulk credit; absent preserves single-row validation order.
 * @return Atomic whole-call admission or validation, allocation, capacity or close diagnostic.
 */
template <class Rows>
std::expected<void, Diagnostic> publishPrepared(const std::shared_ptr<detail::PublisherPort> &port, std::size_t family,
                                                const Rows &rows,
                                                std::optional<core::detail::RecordByteReservation> reservedRows = {}) {
    using namespace core::detail;
    try {
        RecordByteReservation input;
        std::optional<Diagnostic> admissionFailure;
        const auto admit = [&](const RecordBlockAccounting &measured) -> std::expected<void, RecordBlockError> {
            const auto request = publicationRequestBytes(measured);
            const auto available = port->input.state()->capacity - port->binding->retainedCapacityBytes();

            if (request > available) {
                return std::unexpected(RecordBlockError::CAPACITY_EXCEEDED);
            }

            if (!reservedRows) {
                auto admission = reservePublicationRows(port, rows, &measured);

                if (!admission) {
                    // Preserve the caller's precise family-specific diagnostic without throwing an ordinary row miss.
                    admissionFailure = admission.error();

                    return std::unexpected(RecordBlockError::WOULD_BLOCK);
                }

                reservedRows.emplace(std::move(*admission));
            }

            input = RecordByteReservation(port->input.state(), request);

            return {};
        };
        auto block = SharedRecordBlock::copyWithAdmission(port->binding, rows, port->physical, admit,
                                                          RecordPayloadResidency::SEPARATE_BINDING);

        if (!block) {
            return std::unexpected(admissionFailure ? *admissionFailure : blockDiagnostic(block.error()));
        }

        auto publication = std::allocate_shared<detail::Publication>(
            RecordBlockAllocator<detail::Publication>{port->physical.state()}, std::move(input),
            std::move(*reservedRows), std::move(*block), family);
        {
            const std::lock_guard lock(port->mutex);

            if (port->closed) {
                return std::unexpected(CLOSED);
            }

            port->committedCount.state()->used.fetch_add(rows.size(), std::memory_order_relaxed);
            publication->committedRows = RecordByteReservation::adopt(port->committedCount.state(), rows.size());
            port->publications.commit(std::move(publication));
        }
        port->changed.notify_all();

        return {};
    } catch (RecordBlockError error) {
        return std::unexpected(blockDiagnostic(error));
    } catch (const std::bad_alloc &) {
        return std::unexpected(ALLOCATION);
    } catch (Diagnostic diagnostic) {
        return std::unexpected(diagnostic);
    }
}

/** Check closed under the commit fence; preparation uses only detached immutable state.
 * @param port Live detached producer state.
 * @return Whether close has fenced admission.
 */
bool publicationClosed(detail::PublisherPort &port) {
    const std::lock_guard lock(port.mutex);

    return port.closed;
}

/** Synchronous typed projection with one reusable borrowed descriptor set.
 * @tparam Event Existing supported typed event.
 * @tparam Convert Exact validated typed-to-raw projection.
 */
template <class Event, class Convert> struct TypedPublicationRows {
    const core::LocalSchema &schema; ///< Immutable schema borrowed from the detached port.
    std::span<const Event> events;   ///< Caller-owned input through this synchronous call.
    Convert convert;                 ///< Exact existing shape validator.
    mutable SourceRow current{};     ///< Stack-only current raw projection.
    mutable std::array<core::FieldValueView, MAX_DATA_FIELDS> fields{}; ///< Reused borrowed field descriptors.

    std::size_t size() const noexcept {
        return events.size();
    }

    core::detail::RecordBlockInput operator[](std::size_t index) const {
        auto row = convert(events[index]);

        if (!row) {
            throw row.error();
        }

        current = std::move(*row);
        const auto id = RECORD_IDS[current.family];
        const auto &layout = schema.records()[static_cast<std::size_t>(id)].fields;

        for (std::size_t field = 0; field < layout.size(); ++field) {
            fields[field] = {static_cast<FieldType>(layout[field].serialTypeId), current.raw[field], {}, {}};
        }

        core::detail::RecordBlockMetadata metadata;
        metadata.flags = current.flags;
        metadata.removalTime = current.removalTime;

        return {schema.recordContext(id), current.text(), std::span(fields).first(layout.size()), metadata};
    }
};

/** Validate the existing typed mode and prepare one whole call.
 * @tparam Event Supported typed event.
 * @tparam Convert Existing exact projection function.
 * @param port Detached producer state or empty closed handle.
 * @param events Caller-owned input, borrowed synchronously.
 * @param convert Existing shape validator/projection.
 * @param family Target collector index.
 * @param batch Apply the existing legacy batch-size contract.
 * @return Whole admission or detached diagnostic.
 */
template <class Event, class Convert>
std::expected<void, Diagnostic> publishEvents(const std::shared_ptr<detail::PublisherPort> &port,
                                              std::span<const Event> events, Convert convert, std::size_t family,
                                              bool batch) {
    if (!port || publicationClosed(*port)) {
        return std::unexpected(CLOSED);
    }

    if (port->remote || port->localRecords || (family != 0 && port->families != FAMILY_COUNT)) {
        return std::unexpected(port->remote         ? LOCAL_SOURCE_DISABLED
                               : port->localRecords ? RECORD_MODE
                                                    : MIXED_DISABLED);
    }

    if (batch && (events.empty() || (!port->byteStream && events.size() > port->streamBatchRecords))) {
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-batch"});
    }

    std::optional<core::detail::RecordByteReservation> admission;

    if (batch) {
        auto rows =
            reservePublicationRows(port, TypedPublicationRows<Event, Convert>{port->binding->schema, events, convert});

        if (!rows) {
            return std::unexpected(rows.error());
        }

        admission.emplace(std::move(*rows));
    }

    return publishPrepared(port, family, TypedPublicationRows<Event, Convert>{port->binding->schema, events, convert},
                           std::move(admission));
}

/** Indexed adapter over caller-owned catalog input; it allocates no staging descriptors. */
struct CatalogPublicationRows {
    std::span<const PublisherStreamRecordView> records; ///< Validated input borrowed through preparation.

    std::size_t size() const noexcept {
        return records.size();
    }

    core::detail::RecordBlockInput operator[](std::size_t index) const {
        const auto &row = records[index];

        return {row.context, row.symbol, row.fields, {}};
    }
};
} // namespace

std::expected<void, Diagnostic> Publisher::publish(const events::Quote &quote) const {
    return publishEvents(port_, std::span(&quote, 1), quoteRow, 0, false);
}

std::expected<void, Diagnostic> Publisher::publish(const events::TimeAndSale &sale) const {
    return publishEvents(port_, std::span(&sale, 1), saleRow, STREAM_FAMILY, false);
}

std::expected<void, Diagnostic> Publisher::publish(std::span<const events::TimeAndSale> sales) const {
    return publishEvents(port_, sales, saleRow, STREAM_FAMILY, true);
}

std::expected<void, Diagnostic> Publisher::publish(const events::Order &order) const {
    return publishEvents(port_, std::span(&order, 1), orderRow, 2, false);
}

std::expected<void, Diagnostic>
Publisher::publishStreamRecords(std::span<const PublisherStreamRecordView> records) const {
    if (!port_ || publicationClosed(*port_)) {
        return std::unexpected(CLOSED);
    }

    const auto &port = *port_;

    if (!port.localRecords) {
        return std::unexpected(port.remote ? LOCAL_SOURCE_DISABLED : RECORD_MODE);
    }

    if (records.empty() || (!port.byteStream && records.size() > port.streamBatchRecords)) {
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-batch"});
    }

    auto admission = reservePublicationRows(port_, CatalogPublicationRows{records});

    if (!admission) {
        return std::unexpected(admission.error());
    }

    for (const auto &row : records) {
        const auto id = port.binding->schema.recordId(row.context);

        if (!id) {
            return std::unexpected(RECORD_SHAPE);
        }

        const auto storage = recordStorage(port.binding->schema.records()[static_cast<std::size_t>(*id)], row.symbol,
                                           row.fields, port.recordLimits.rowBytes, false);

        if (!storage) {
            return std::unexpected(storage.error());
        }
    }

    return publishPrepared(port_, STREAM_FAMILY, CatalogPublicationRows{records}, std::optional(std::move(*admission)));
}

std::expected<void, Diagnostic> Publisher::publishRecord(Family family, const core::RecordContext &context,
                                                         std::u16string_view symbol,
                                                         std::span<const core::FieldValueView> fields) const {
    return publishRecord(family, context, symbol, fields, {});
}

std::expected<void, Diagnostic> Publisher::publishRecord(Family family, const core::RecordContext &context,
                                                         std::u16string_view symbol,
                                                         std::span<const core::FieldValueView> fields,
                                                         PublisherHistoryMetadata history) const {
    if (!port_ || publicationClosed(*port_)) {
        return std::unexpected(CLOSED);
    }

    const auto &port = *port_;

    if (!port.localRecords) {
        return std::unexpected(port.remote ? LOCAL_SOURCE_DISABLED : RECORD_MODE);
    }

    const auto lane = familyIndex(family);
    const auto id = port.binding->schema.recordId(context);

    if (!lane || !id) {
        return std::unexpected(RECORD_SHAPE);
    }

    if (!validHistoryMetadata(family, history) || (family == Family::HISTORY && port.localHistory[*id] == 0)) {
        return std::unexpected(HISTORY_SHAPE);
    }

    const auto &layout = port.binding->schema.records()[static_cast<std::size_t>(*id)];
    const auto storage =
        recordStorage(layout, symbol, fields, port.recordLimits.rowBytes, history.removalIndex.has_value());

    if (!storage) {
        return std::unexpected(storage.error());
    }

    if (family == Family::HISTORY && !history.removalIndex && reservedHistoryIndex(*layout.indexedTimeLayout, fields)) {
        return std::unexpected(HISTORY_SHAPE);
    }

    // Removal payload is absent at the public boundary; the immutable block retains its full typed shape.
    std::array<core::FieldValueView, PublisherRecordLimits::MAX_FIELDS> defaults{};

    if (history.removalIndex) {
        for (std::size_t i = 0; i < layout.fields.size(); ++i) {
            defaults[i].type = static_cast<FieldType>(layout.fields[i].serialTypeId);
        }

        fields = std::span(defaults).first(layout.fields.size());
    }

    core::detail::RecordBlockMetadata metadata;
    metadata.flags = history.eventFlags;
    metadata.removalTime = history.removalIndex;
    const std::array rows{core::detail::RecordBlockInput{context, symbol, fields, metadata}};

    return publishPrepared(port_, *lane, rows);
}

PublisherEndpoint::PublisherEndpoint(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

std::expected<std::unique_ptr<PublisherEndpoint>, Diagnostic>
PublisherEndpoint::create(transport::RuntimeBootstrap &bootstrap, PublisherOptions options) {
    if (!options.listener.maxConnections || options.listener.maxConnections > MAX_PEERS || !options.pendingQuotes ||
        options.pendingQuotes > MAX_PENDING_QUOTES) {
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-limits"});
    }

    if (options.catalog && options.mixed)
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-catalog-options"});

    if (!options.streamBytes &&
        (!options.streamBatchRecords || options.streamBatchRecords > MAX_STREAM_BATCH_RECORDS ||
         (options.streamBatchRecords > 1 && (options.source || (!options.mixed && !options.catalog)))))
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-batch"});

    const auto &limits = options.recordLimits;

    if (((options.catalog && !options.source) || options.streamBytes) &&
        (limits.rowBytes == 0 || limits.rowBytes > PublisherRecordLimits::MAX_ROW_BYTES || limits.pendingBytes == 0 ||
         limits.pendingBytes > PublisherRecordLimits::MAX_PENDING_BYTES))
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-record-limits"});

    if (const auto &wire = options.streamBytes;
        wire && (options.source || (!options.mixed && !options.catalog) || !wire->targetPayloadBytes ||
                 wire->targetPayloadBytes >= wire->maxFrameBytes || wire->maxFrameBytes > MAX_FRAME_BYTES ||
                 !wire->deliveryWindowBytes ||
                 wire->deliveryWindowBytes > PublisherStreamByteLimits::MAX_DELIVERY_WINDOW_BYTES))
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-bytes"});

    if (options.streamBytes && options.mixed &&
        (limits.rowBytes < sizeof(SourceRow) || limits.pendingBytes < sizeof(SourceRow)))
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-record-limits"});

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

        auto demand = configuredDemandPolicies(options);
        options.demand = {};
        std::vector<PublisherPeerDemandPolicy>().swap(options.peerDemand);
        auto impl = std::make_unique<Impl>();
        impl->port = std::make_shared<detail::PublisherPort>(options);
        const auto port = impl->port;
        auto endpoint = std::unique_ptr<PublisherEndpoint>(new PublisherEndpoint(std::move(impl)));
        endpoint->impl_->worker = std::jthread(
            [port, &bootstrap, options = std::move(options), demand = std::move(demand)](std::stop_token stop) mutable {
                runPublisher(*port, bootstrap, options, std::move(demand), stop);
            });
        std::unique_lock lock(port->mutex);
        port->changed.wait(lock, [&] {
            return port->started || port->closed;
        });

        if (!port->started)
            return std::unexpected(port->diagnostic);

        return endpoint;
    } catch (const Error &failure) {
        return std::unexpected(failure.diagnostic());
    } catch (const std::bad_alloc &) {
        return std::unexpected(ALLOCATION);
    } catch (core::detail::RecordBlockError error) {
        return std::unexpected(blockDiagnostic(error));
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
    if (!exactSymbol(symbol))
        return false;

    return awaitPublishedDemand(*impl_->port, family, {}, symbol, subscribed, deadline);
}

bool PublisherEndpoint::awaitDemand(Family family, std::u16string_view recordName, std::u16string_view symbol,
                                    bool subscribed, Clock::time_point deadline) const {
    if (recordName.empty() || !symbol.data() || (!symbol.empty() && !exactSymbol(symbol)))
        return false;

    return awaitPublishedDemand(*impl_->port, family, recordName, symbol, subscribed, deadline);
}

std::vector<PublisherDemand> PublisherEndpoint::getDemand(Family family) const {
    const auto index = familyIndex(family);
    auto &port = *impl_->port;

    if (!index)
        return {};

    const std::lock_guard lock(port.mutex);

    if (port.closed)
        return {};

    return port.demand[*index];
}

PublisherPolicyChange::PublisherPolicyChange(std::shared_ptr<detail::PublisherPolicyCompletion> completion) noexcept
    : completion_(std::move(completion)) {
}

std::optional<PublisherPolicyResult> PublisherPolicyChange::waitUntil(Clock::time_point deadline) const {
    if (!completion_)
        return std::nullopt;

    std::unique_lock lock(completion_->mutex);
    (void)completion_->changed.wait_until(lock, deadline, [&] {
        return completion_->result.has_value();
    });

    return completion_->result;
}

std::vector<PublisherPeer> PublisherEndpoint::getPeers() const {
    auto &port = *impl_->port;
    const std::lock_guard lock(port.mutex);

    if (port.closed)
        return {};

    return {port.peers.begin(), port.peers.begin() + static_cast<std::ptrdiff_t>(port.peerCount)};
}

std::expected<PublisherPolicyChange, Diagnostic> PublisherEndpoint::replacePeerDemand(PublisherPeerId id,
                                                                                      PublisherDemandPolicy policy) {
    try {
        auto demand = validatedDemandPolicy(policy, true);
        auto completion = std::make_shared<detail::PublisherPolicyCompletion>();
        auto &port = *impl_->port;
        const std::lock_guard lock(port.mutex);

        if (port.closed)
            return std::unexpected(CLOSED);

        if (port.policyCommand)
            return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-policy-pending"});

        constexpr auto policyTimeout = 5s;
        port.policyCommand.emplace(detail::PublisherPeerIdentityAccess::get(id), std::move(demand), completion,
                                   Clock::now() + policyTimeout);
        port.policyPending.store(true, std::memory_order_release);
        port.changed.notify_all();

        return PublisherPolicyChange(std::move(completion));
    } catch (const Error &failure) {
        return std::unexpected(failure.diagnostic());
    } catch (const std::bad_alloc &) {
        return std::unexpected(ALLOCATION);
    }
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
    snapshot.pendingPublications = impl_->port->committedCount.usedBytes();

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
