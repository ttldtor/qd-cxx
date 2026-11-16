// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"

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
#include <bit>
#include <bitset>
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
constexpr std::size_t MAX_STREAM_BATCH_RECORDS = PublisherOptions::MAX_STREAM_BATCH_RECORDS;
constexpr std::size_t SYMBOL_UNITS = 256;
constexpr std::size_t CLIENT_KEYS = 32;
// Java Feed expands Order into source-specific and derived Quote subscriptions.
constexpr std::size_t MAX_SUBSCRIPTION_WIRE_TUPLES = 256;
constexpr std::size_t CLIENT_PENDING_ITEMS = 32;
constexpr std::size_t HISTORY_POINTS_PER_KEY = 16;
constexpr std::size_t CLIENT_SYMBOL_UNITS = CLIENT_KEYS * SYMBOL_UNITS;
constexpr std::size_t INPUT_BYTES = 8192;
constexpr std::size_t MAX_FRAME_BYTES = 65536;
constexpr std::size_t FRAME_PREFIX_BYTES = 9;
constexpr std::size_t EVENT_BYTES = 8192;
constexpr std::size_t EVENT_OWNER_ALLOWANCE = 1024;
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
constexpr std::size_t STREAM_FAMILY = 1;
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

/** One pending local row. Every pointer in fields refers into this row's independent arrays. */
struct RecordRow {
    std::unique_ptr<char16_t[]> text;
    std::unique_ptr<std::byte[]> opaque;
    std::unique_ptr<core::FieldValueView[]> fields;
    std::size_t symbolSize = 0;
    std::size_t fieldCount = 0;
    std::size_t charge = 0;
    std::size_t family = 0;
    std::int32_t id = 0;
    PublisherHistoryMetadata history;
};

/** Requested lengths for one exact backing allocation of each pending array. */
struct RecordStorage {
    std::size_t textUnits;
    std::size_t opaqueBytes;
    std::size_t charge;
};

std::size_t streamWindow(const PublisherOptions &options) noexcept {
    if (!options.streamBytes)
        return options.streamBatchRecords;

    // Even a fieldless occurrence needs a symbol marker and a compact record ID.
    constexpr std::size_t minimumWireRecord = 2;
    const auto threshold = options.streamBytes->targetPayloadBytes;
    const auto candidates = (threshold + minimumWireRecord - 1) / minimumWireRecord;
    const auto minimumInput = options.catalog ? sizeof(char16_t) : sizeof(SourceRow);
    const auto available = std::max<std::size_t>(1, options.recordLimits.pendingBytes / minimumInput);

    return std::min({candidates, options.pendingQuotes, available});
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

    RecordStorage storage{symbol.size(), 0, fieldBytes + symbol.size() * sizeof(char16_t)};

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

            storage.opaqueBytes += bytes;
            storage.charge += bytes;
        }
    }

    return storage;
}

/** Copy validated borrowed data only after the caller has checked available FIFO and byte capacity. */
RecordRow copyRecordRow(std::size_t family, std::int32_t id, std::u16string_view symbol,
                        std::span<const core::FieldValueView> fields, RecordStorage storage,
                        PublisherHistoryMetadata history) {
    RecordRow row{std::make_unique_for_overwrite<char16_t[]>(storage.textUnits),
                  storage.opaqueBytes ? std::make_unique_for_overwrite<std::byte[]>(storage.opaqueBytes) : nullptr,
                  fields.empty() ? nullptr : std::make_unique<core::FieldValueView[]>(fields.size()),
                  symbol.size(),
                  fields.size(),
                  storage.charge,
                  family,
                  id,
                  history};
    auto *text = std::ranges::copy(symbol, row.text.get()).out;
    auto *opaque = row.opaque.get();

    for (std::size_t index = 0; index < fields.size(); ++index) {
        const auto &field = fields[index];
        auto &target = row.fields[index];
        target = {field.type, field.raw, {}, {}};

        if (field.units) {
            target.units.emplace();

            if (!field.units->empty()) {
                target.units.emplace(text, field.units->size());
                text = std::ranges::copy(*field.units, text).out;
            }
        }

        if (field.bytes) {
            target.bytes.emplace();

            if (!field.bytes->empty()) {
                target.bytes.emplace(opaque, field.bytes->size());
                opaque = std::ranges::copy(*field.bytes, opaque).out;
            }
        }
    }

    return row;
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
    constexpr auto MAX_TEXT_PADDING = (PublisherRecordLimits::MAX_FIELDS + 1) * (sizeof(char16_t) + 1);
    result.recordFields = PublisherRecordLimits::MAX_FIELDS;
    result.recordFieldUnits = rowBytes / sizeof(char16_t);
    result.recordByteUnits = rowBytes;
    // HISTORY may materialize the full default layout from a removal containing only the symbol and index.
    constexpr auto MAX_DEFAULT_ROW =
        PublisherRecordLimits::MAX_FIELDS * sizeof(core::FieldValueView) + SYMBOL_UNITS * sizeof(char16_t);
    result.recordArenaBytes = std::max(rowBytes, MAX_DEFAULT_ROW) + MAX_TEXT_PADDING + alignof(core::FieldValueView);

    result.recordBytes = result.dataItems * (result.recordArenaBytes + EVENT_OWNER_ALLOWANCE);

    return result;
}

mux::ClientLimits publisherClientLimits(const PublisherOptions &options, bool byteStream = false) {
    auto limits =
        options.catalog && !options.source ? recordClientLimits(options.recordLimits.rowBytes) : CLIENT_LIMITS;

    if (!byteStream)
        return limits;

    const auto window = streamWindow(options);
    constexpr auto copyOverhead = EVENT_OWNER_ALLOWANCE +
                                  (PublisherRecordLimits::MAX_FIELDS + 1) * (sizeof(char16_t) + 1) +
                                  alignof(core::FieldValueView);
    const auto payload = options.catalog
                             ? std::min(options.recordLimits.pendingBytes, window * options.recordLimits.rowBytes)
                             : window * limits.recordArenaBytes;
    // One retained delivery and one additional ready window can coexist for a healthy peer.
    limits.recordBytes = 2 * (payload + window * copyOverhead);
    limits.dataItems = 2 * window;
    limits.batchBytes =
        std::max(limits.batchBytes,
                 EVENT_OWNER_ALLOWANCE + window * (sizeof(mux::MuxRecordView) + sizeof(std::shared_ptr<const void>)));
    limits.compactRecordArena = true;

    return limits;
}

mux::MuxQueue publisherQueue(const core::LocalSchema &schema, std::size_t peers, bool remote, Family family,
                             bool catalog, const mux::ClientLimits &clientLimits, std::size_t streamBatchRecords) {
    const auto allKeys = peers * CLIENT_KEYS;
    const auto allUnits = peers * clientLimits.commandSymbolUnits;
    const auto records = schema.records().size();
    const core::SubscriptionLimits desired{records,     allKeys,      SYMBOL_UNITS, allUnits,
                                           allKeys * 2, allUnits * 2, allKeys};
    const mux::MuxLimits interests{records,      peers,    allKeys,     allKeys,
                                   SYMBOL_UNITS, allUnits, CLIENT_KEYS, 8 * 1024 * 1024};
    const auto batch = family == Family::STREAM ? streamBatchRecords : 1;
    const mux::QueueLimits limits{peers,
                                  remote ? REMOTE_DATA_RECORDS : batch,
                                  batch,
                                  batch,
                                  8,
                                  peers * clientLimits.dataItems,
                                  peers * clientLimits.recordBytes,
                                  peers * clientLimits.batchOwners,
                                  peers * clientLimits.batchBytes,
                                  peers * clientLimits.commandSlots,
                                  peers * clientLimits.resultSlots,
                                  peers * 2,
                                  clientLimits.compactRecordArena
                                      ? 32 * 1024 * 1024 + peers * (clientLimits.recordBytes + clientLimits.batchBytes)
                                      : 32 * 1024 * 1024};
    const auto cacheFrames = family == Family::HISTORY ? allKeys * HISTORY_POINTS_PER_KEY + 1 : allKeys + 1;
    const auto eventBytes = (remote    ? REMOTE_EVENT_BYTES
                             : catalog ? LOCAL_RECORD_EVENT_BYTES
                                       : EVENT_BYTES) +
                            EVENT_OWNER_ALLOWANCE;
    const auto cacheBytes = cacheFrames * eventBytes * batch;
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

protocol::detail::LocalEventOwner localOwner(bool catalog, std::size_t families, std::size_t streamBatchRecords) {
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

    auto owner = protocol::detail::LocalEventOwner::create(limits, work);

    if (!owner)
        throw Error(protocolDiagnostic(owner.error(), "publisher-source"));

    return std::move(*owner);
}

using DeliveryMask = std::bitset<MAX_PENDING_QUOTES>;

/** The worker retains one detached callback batch until every eligible row has completed writing. */
struct Delivery final : mux::MuxConsumer {
    mux::OwnedMuxBatch batch;
    std::size_t cursor = 0;
    DeliveryMask suppressed;

    mux::BatchAction onBatch(const mux::BorrowedMuxBatch &view) override {
        batch = view.retain();
        cursor = 0;
        suppressed.reset();

        return mux::BatchAction::PAUSE;
    }

    void clear() noexcept {
        batch = {};
        cursor = 0;
        suppressed.reset();
    }
};

// Front-loading control frames avoids consuming data queue reservations during negotiation.
enum class OutputKind { PROTOCOL, DESCRIPTION, HEARTBEAT, DATA };

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
    DeliveryMask outputRows;
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

std::expected<protocol::LivePendingOutput, protocol::LiveError>
composeDelivery(Peer &peer, std::size_t family, const core::LocalSchema &schema,
                const std::optional<PublisherStreamByteLimits> &streamBytes,
                std::span<protocol::LiveDataRecordView> outgoing, std::span<std::size_t> indices,
                protocol::LiveWork &work) {
    const auto &delivery = *peer.delivery[family];
    const auto records = delivery.batch.records();
    std::size_t count = 0;

    for (auto i = delivery.cursor; i < records.size(); ++i) {
        if (delivery.suppressed[i])
            continue;

        if (count == outgoing.size())
            break;

        const auto &record = records[i];
        const auto id = schema.recordId(record.context);

        if (!id)
            return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);

        const bool removed = (record.eventFlags & events::indexed_event_flags::REMOVE_EVENT) != 0;
        indices[count] = i;
        outgoing[count++] = {*id, record.symbol, removed ? std::span<const core::FieldValueView>{} : record.typedValues,
                             record.eventFlags, removed ? record.rawTime : std::nullopt};
    }

    const auto rememberRows = [&](std::size_t written) {
        peer.outputEnd = indices[written - 1] + 1;
        peer.outputRows.reset();

        for (std::size_t i = 0; i < written; ++i)
            peer.outputRows.set(indices[i]);
    };

    if (family == STREAM_FAMILY && streamBytes) {
        std::size_t written = 0;
        auto output = peer.session.publishStreamPrefix(outgoing.first(count), streamBytes->targetPayloadBytes,
                                                       streamBytes->maxFrameBytes, written, work);

        if (output)
            rememberRows(written);

        return output;
    }

    // Legacy count-based mode retries a shorter prefix when a frame exceeds its byte limit.
    while (count != 0) {
        auto output = peer.session.publishData(DATA_MESSAGES[family], outgoing.first(count), work);

        if (output) {
            rememberRows(count);

            return output;
        }

        if (output.error() != protocol::LiveError::RESOURCE_LIMIT || count == 1)
            return output;

        count /= 2;
    }

    return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);
}

void releaseSuppressedPrefix(Peer &peer, std::size_t family) noexcept {
    auto &delivery = *peer.delivery[family];
    const auto records = delivery.batch.records();

    while (delivery.cursor < records.size() && delivery.suppressed[delivery.cursor])
        ++delivery.cursor;

    if (!records.empty() && delivery.cursor == records.size()) {
        delivery.clear();
        (void)peer.handles[family].requestResume();
    }
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

/** Detached bounded producer state. It contains no worker or native owner. */
struct PublisherPort {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<SourceRow> pending;
    std::vector<RecordRow> pendingRecords;
    std::optional<core::LocalSchema> localCatalog;
    std::vector<std::uint8_t> localHistory;
    const PublisherRecordLimits recordLimits;
    std::size_t recordBytes = 0;
    std::size_t activeRecords = 0;
    std::array<std::vector<PublisherDemand>, FAMILY_COUNT> demand;
    std::array<PublisherPeer, MAX_PEERS> peers;
    std::size_t peerCount = 0;
    std::optional<PublisherPolicyCommand> policyCommand;
    const bool remote;
    const bool localRecords;
    const std::size_t families;
    const std::size_t streamBatchRecords;
    const bool byteStream;
    PublisherSourceStatus source;
    PublisherStatistics statistics;
    std::size_t head = 0;
    std::size_t size = 0;
    std::uint16_t boundPort = 0;
    bool started = false;
    bool closed = false;
    Diagnostic diagnostic;

    explicit PublisherPort(const PublisherOptions &options)
        : pending(options.source || options.catalog ? 0
                  : options.streamBytes
                      ? std::min(options.pendingQuotes, options.recordLimits.pendingBytes / sizeof(SourceRow))
                      : options.pendingQuotes),
          pendingRecords(!options.source && options.catalog ? options.pendingQuotes : 0),
          recordLimits(options.recordLimits), remote(options.source.has_value()),
          localRecords(!options.source && options.catalog), families(enabledFamilies(options)),
          streamBatchRecords(options.streamBatchRecords), byteStream(options.streamBytes.has_value()) {
        if (!pendingRecords.empty())
            localCatalog.emplace(createSchema(options));

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
    const std::size_t streamBatchRecords_;
    const std::optional<PublisherStreamByteLimits> streamBytes_;
    std::vector<RecordRow> recordScratch_;
    std::vector<SourceRow> typedScratch_;
    std::vector<std::array<core::FieldValueView, MAX_DATA_FIELDS>> typedFieldScratch_;
    std::vector<protocol::detail::LocalStreamRow> sourceViews_;
    std::vector<protocol::LiveDataRecordView> outgoingViews_;
    std::vector<std::size_t> outgoingIndices_;
    QueueSet queues_;
    transport::TcpListener listener_;
    std::vector<std::optional<Peer>> peers_;
    const DemandPolicies demandPolicies_;
    std::optional<protocol::LiveEvent> pendingSource_;

    struct RecordApplication {
        std::size_t family;
        std::size_t charge;
        std::size_t records;
    };

    std::optional<RecordApplication> recordApplication_;
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
        auto session = serverSession(schema_, catalogNames_, work, streamBatchRecords_);
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
            auto session = serverSession(schema_, catalogNames_, liveWork, streamBatchRecords_);
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
                    limits.batchRecords = streamBatchRecords_;

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
        if (!policy[family])
            return true;

        const auto id = schema_.recordId(context);

        return id && policy[family]->acceptRecord(schema_.records()[static_cast<std::size_t>(*id)].name, symbol);
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
                        try {
                            for (auto row = delivery.cursor; row < records.size(); ++row) {
                                if (!delivery.suppressed[row] &&
                                    !allowed(peer.demand, family, records[row].context, records[row].symbol))
                                    delivery.suppressed.set(row);
                            }
                        } catch (const Error &) {
                            return false;
                        }

                        releaseSuppressedPrefix(peer, family);

                        if (delivery.batch.records().empty())
                            continue;

                        kind = OutputKind::DATA;
                        peer.outputFamily = family;
                        peer.nextFamily = (family + 1) % familyCount_;

                        if (family == STREAM_FAMILY && streamBytes_) {
                            output = composeDelivery(peer, family, schema_, streamBytes_, outgoingViews_,
                                                     outgoingIndices_, work);

                            if (!output)
                                statistics_.families[family].failedFrames.add(1);
                        } else {
                            output = composeDelivery(peer, family, schema_, {}, outgoingViews_, outgoingIndices_, work);
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
            stats.maxSentFrameRecords = std::max(stats.maxSentFrameRecords, peer.outputRows.count());
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
            releaseSuppressedPrefix(peer, peer.outputFamily);
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
        port.changed.notify_all();
    }

    void progressPolicy(detail::PublisherPort &port) {
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
                const auto &delivery = *peer.delivery[family];
                const auto held = delivery.batch.records();

                for (auto row = delivery.cursor; row < held.size(); ++row) {
                    if (!delivery.suppressed[row] &&
                        !allowed(input->demand, family, held[row].context, held[row].symbol))
                        revoked[family].set(row);
                }
            }

            constexpr std::size_t history = 2;
            const auto losesHistory = std::ranges::any_of(peer.openHistory, [&](const auto &entry) {
                return !allowed(input->demand, history, entry.context, entry.symbol);
            });
            const auto losesPartialFrame = peer.output && peer.outputKind == OutputKind::DATA &&
                                           (revoked[peer.outputFamily] & peer.outputRows).any() &&
                                           peer.output->pendingBytes().size() != peer.output->bytes().size();

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
            if (revoked[family].any()) {
                if (peer.output && peer.outputKind == OutputKind::DATA && peer.outputFamily == family &&
                    (revoked[family] & peer.outputRows).any())
                    peer.output.reset();

                peer.delivery[family]->suppressed |= revoked[family];
                releaseSuppressedPrefix(peer, family);
            }

            publishedRevision_[family] = queues_[family]->revision();
        }

        port.demand = std::move(prepared.demand);
        snapshotPeers(port);
        completePolicy(port, {PublisherPolicyOutcome::APPLIED, revision, {}});
    }

    /** Guard the next STREAM source window against detached record pressure. A ready peer can
     * still advance delivery when another peer exhausts its individual finite budget.
     */
    bool canAdvanceStream() const noexcept {
        const auto &queue = *queues_[STREAM_FAMILY];

        if (queue.resources().pendingRecords != 0)
            return false;

        bool hasSubscribers = false;

        for (const auto &peer : peers_) {
            if (!peer || peer->closing || peer->requested[STREAM_FAMILY].empty())
                continue;

            const auto resources = queue.resources(peer->handles[STREAM_FAMILY]);

            if (!resources || resources->state == mux::QueueClientState::TERMINAL)
                continue;

            hasSubscribers = true;

            if (resources->recordOwners <= streamBatchRecords_ &&
                resources->recordBytes <= streamClientLimits_.recordBytes / 2)
                return true;
        }

        return !hasSubscribers;
    }

    /** Check the next FIFO row under the port mutex. */
    bool canTakeLocal(const detail::PublisherPort &port) const noexcept {
        if (port.closed || port.size == 0)
            return false;

        if (!streamBytes_)
            return true;

        const auto family = port.localRecords ? port.pendingRecords[port.head].family : port.pending[port.head].family;

        return family != STREAM_FAMILY || canAdvanceStream();
    }

    /** Detach consecutive STREAM rows under the mailbox fence without crossing another family. */
    template <typename Row>
    std::size_t popStreamRows(detail::PublisherPort &port, std::vector<Row> &pending, std::span<Row> rows) const {
        const std::lock_guard lock(port.mutex);
        std::size_t count = 0;

        if (!canTakeLocal(port))
            return 0;

        while (port.size != 0 && count < rows.size() && pending[port.head].family == STREAM_FAMILY) {
            rows[count++] = std::move(pending[port.head]);
            port.head = (port.head + 1) % pending.size();
            --port.size;
        }

        return count;
    }

    bool batchesLocalStream() const noexcept {
        return streamBytes_ || streamBatchRecords_ > 1;
    }

    /** Fund one whole STREAM fan-out and the delivery eligibility checks for that window. */
    void fundStreamWindow(mux::QueueWork &work) const noexcept {
        const auto copies = peers_.size() * streamBatchRecords_;
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

    bool takeLocalStream(detail::PublisherPort &port) {
        const auto count = popStreamRows(port, port.pendingRecords, std::span(recordScratch_));

        if (count == 0)
            return false;

        std::size_t charge = 0;

        for (std::size_t i = 0; i < count; ++i) {
            const auto &record = recordScratch_[i];
            sourceViews_[i] = {record.id,
                               schema_.recordContext(record.id),
                               {record.text.get(), record.symbolSize},
                               {record.fields.get(), record.fieldCount}};
            charge += record.charge;
        }

        recordApplication_.emplace(STREAM_FAMILY, charge, count);
        auto work = liveTurn(true);
        auto event = source_->stream(std::span(sourceViews_).first(count), work, streamBytes_.has_value());

        for (std::size_t i = 0; i < count; ++i)
            recordScratch_[i] = {};

        if (!event)
            throw Error(protocolDiagnostic(event.error(), "publisher-local-record-batch"));

        pendingSource_ = std::move(*event);

        return true;
    }

    /** Move retained rows onto the worker's ordinary local event owner and keep the original budget charged. */
    void takeLocalRecord(detail::PublisherPort &port) {
        if (recordApplication_) {
            if (queues_[recordApplication_->family]->resources().pendingRecords != 0)
                return;

            const std::lock_guard lock(port.mutex);
            port.recordBytes -= recordApplication_->charge;
            port.activeRecords -= recordApplication_->records;
            recordApplication_.reset();
        }

        if (batchesLocalStream() && takeLocalStream(port))
            return;

        RecordRow row;
        {
            const std::lock_guard lock(port.mutex);

            if (!canTakeLocal(port))
                return;

            // A STREAM publication may have arrived between the batch probe and this lock.
            if (batchesLocalStream() && port.pendingRecords[port.head].family == STREAM_FAMILY)
                return;

            row = std::move(port.pendingRecords[port.head]);
            port.head = (port.head + 1) % port.pendingRecords.size();
            --port.size;
        }
        recordApplication_.emplace(row.family, row.charge, 1);
        auto turn = liveTurn(true);
        auto event = source_->record(DATA_MESSAGES[row.family], row.id, schema_.recordContext(row.id),
                                     {row.text.get(), row.symbolSize}, {row.fields.get(), row.fieldCount},
                                     row.history.eventFlags, row.history.removalIndex, turn);

        if (!event)
            throw Error(protocolDiagnostic(event.error(), "publisher-local-record"));

        pendingSource_ = std::move(*event);
    }

    bool takeTypedStream(detail::PublisherPort &port) {
        const auto count = popStreamRows(port, port.pending, std::span(typedScratch_));

        if (count == 0)
            return false;

        const auto context = schema_.recordContext(SALE_ID);
        const auto layout = schema_.records()[SALE_ID].fields;

        for (std::size_t i = 0; i < count; ++i) {
            auto &fields = typedFieldScratch_[i];
            const auto &record = typedScratch_[i];

            for (std::size_t j = 0; j < layout.size(); ++j)
                fields[j] = {static_cast<FieldType>(layout[j].serialTypeId), record.raw[j], {}, {}};

            sourceViews_[i] = {SALE_ID, context, record.text(), std::span(fields).first(layout.size())};
        }

        auto work = liveTurn();
        auto event = source_->stream(std::span(sourceViews_).first(count), work, streamBytes_.has_value());

        if (!event)
            throw Error(protocolDiagnostic(event.error(), "publisher-local-event-batch"));

        pendingSource_ = std::move(*event);

        return true;
    }

    /** Adopt a typed publication through its existing fixed physical layout. */
    void takeTypedRecord(detail::PublisherPort &port) {
        if (batchesLocalStream() && takeTypedStream(port))
            return;

        SourceRow row;
        {
            const std::lock_guard lock(port.mutex);

            if (!canTakeLocal(port))
                return;

            if (batchesLocalStream() && port.pending[port.head].family == STREAM_FAMILY)
                return;

            row = port.pending[port.head];
            port.head = (port.head + 1) % port.pending.size();
            --port.size;
        }
        auto liveWork = liveTurn(!catalogNames_.empty());
        const auto id = RECORD_IDS[row.family];
        const auto layout = schema_.records()[static_cast<std::size_t>(id)].fields;
        std::array<core::FieldValue, MAX_DATA_FIELDS> fields{};

        for (std::size_t i = 0; i < layout.size(); ++i)
            fields[i] = {static_cast<FieldType>(layout[i].serialTypeId), row.raw[i]};

        auto event = source_->record(DATA_MESSAGES[row.family], id, schema_.recordContext(id), row.text(),
                                     std::span(fields).first(layout.size()), row.flags, row.removalTime, {}, liveWork);

        if (!event)
            throw Error(protocolDiagnostic(event.error(), "publisher-local-event"));

        pendingSource_ = std::move(*event);
    }

    /** Admit the next local owner if its queue can accept it.
     * @param port Bounded producer mailbox.
     * @param work Remaining queue credits for this turn.
     * @return Whether a source owner entered its queue.
     */
    bool publishLocal(detail::PublisherPort &port, WorkSet &work) {
        if (!pendingSource_) {
            if (port.localRecords)
                takeLocalRecord(port);
            else
                takeTypedRecord(port);

            if (!pendingSource_)
                return false;
        }

        const auto message = static_cast<DataMessage>(*pendingSource_->messageType());
        const auto family = static_cast<std::size_t>(std::ranges::find(DATA_MESSAGES, message) - DATA_MESSAGES.begin());
        const auto records = pendingSource_->records().size();
        const auto admitted = queues_[family]->admit(std::move(*pendingSource_), work[family]);

        if (!admitted) {
            if (admitted.error().error == mux::QueueError::BUSY ||
                admitted.error().error == mux::QueueError::WORK_LIMIT)
                return false;

            throw Error(queueDiagnostic(admitted.error(), "publisher-admit"));
        }

        statistics_.families[family].ingressFrames.add(1);
        statistics_.families[family].ingressRecords.add(records);
        pendingSource_.reset();

        return true;
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
    PublisherServer(transport::RuntimeBootstrap &bootstrap, const PublisherOptions &options, DemandPolicies demand)
        : schema_(createSchema(options)), recordIds_(recordIds(schema_)), catalogNames_(catalogNames(options, schema_)),
          clientLimits_(publisherClientLimits(options)),
          streamClientLimits_(publisherClientLimits(options, options.streamBytes.has_value())),
          streamBatchRecords_(streamWindow(options)), streamBytes_(options.streamBytes),
          recordScratch_(options.catalog && !options.source ? streamBatchRecords_ : 0),
          typedScratch_(!options.catalog && !options.source ? streamBatchRecords_ : 0),
          typedFieldScratch_(typedScratch_.size()), sourceViews_(options.source ? 0 : streamBatchRecords_),
          outgoingViews_(streamBatchRecords_), outgoingIndices_(streamBatchRecords_),
          listener_(listen(bootstrap, options.listener)), peers_(options.listener.maxConnections),
          demandPolicies_(std::move(demand)), familyCount_(enabledFamilies(options)) {
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
            queues_[family].emplace(publisherQueue(
                schema_, peers_.size(), options.source.has_value(), FAMILIES[family], !catalogNames_.empty(),
                family == STREAM_FAMILY ? streamClientLimits_ : clientLimits_, streamBatchRecords_));

        if (!options.source) {
            source_.emplace(localOwner(!catalogNames_.empty(), familyCount_, streamBatchRecords_));

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
        const bool driveLocal = !port.remote && (!port.localRecords || batchesLocalStream());
        // A sequence of IDLE visits may end before the next runnable slot, or before source admission reopens.
        const auto idleRotation = mux::detail::QUEUE_PHASES * peers_.size();
        const auto queueActions = driveLocal ? std::max(DEFAULT_QUEUE_ACTIONS, idleRotation) : DEFAULT_QUEUE_ACTIONS;
        const auto idleLimit = port.localRecords && driveLocal ? idleRotation : queueActions;

        for (std::size_t family = 0; family < familyCount_; ++family) {
            auto &queue = *queues_[family];
            auto &turn = work[family];
            turn = serverQueueTurn(peers_.size(), sourceOptions_.has_value(), !catalogNames_.empty(), familyCount_,
                                   recordIds_.size());

            if (family == STREAM_FAMILY && streamBytes_)
                fundStreamWindow(turn);

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
        const bool canApply =
            !recordApplication_ || queues_[recordApplication_->family]->resources().pendingRecords == 0;

        if (port.closed || (!port.remote && (!port.localRecords || batchesLocalStream()) && canApply &&
                            !pendingSource_ && canTakeLocal(port)))
            return;

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
        PublisherServer server(bootstrap, options, std::move(demand));
        {
            const std::lock_guard lock(port.mutex);
            const auto localHistory = server.localHistory();
            port.localHistory.assign(localHistory.begin(), localHistory.end());
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
    {
        const std::lock_guard lock(port.mutex);
        port.closed = true;
        port.source.connected = false;
        port.peerCount = 0;
        port.diagnostic = diagnostic;
        std::vector<SourceRow>().swap(port.pending);
        std::vector<RecordRow>().swap(port.pendingRecords);
        port.localCatalog.reset();
        std::vector<std::uint8_t>().swap(port.localHistory);
        port.recordBytes = 0;
        port.activeRecords = 0;

        for (auto &demand : port.demand)
            std::vector<PublisherDemand>().swap(demand);

        port.size = 0;

        if (port.policyCommand) {
            port.policyCommand->completion->complete({PublisherPolicyOutcome::PEER_CLOSED, 0, CLOSED});
            port.policyCommand.reset();
        }
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
    if (port && (port->remote || port->localRecords || (indexed && port->families != FAMILY_COUNT))) {
        const std::lock_guard lock(port->mutex);

        const auto refusal = port->remote ? LOCAL_SOURCE_DISABLED : port->localRecords ? RECORD_MODE : MIXED_DISABLED;

        return std::unexpected(port->closed ? CLOSED : refusal);
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

std::expected<void, Diagnostic> Publisher::publish(std::span<const events::TimeAndSale> sales) const {
    if (!port_)
        return std::unexpected(CLOSED);

    auto &port = *port_;
    {
        const std::lock_guard lock(port.mutex);

        if (port.closed)
            return std::unexpected(CLOSED);

        if (port.remote || port.localRecords || port.families != FAMILY_COUNT)
            return std::unexpected(port.remote         ? LOCAL_SOURCE_DISABLED
                                   : port.localRecords ? RECORD_MODE
                                                       : MIXED_DISABLED);

        if (sales.empty() || (!port.byteStream && sales.size() > port.streamBatchRecords))
            return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-batch"});

        if (port.byteStream) {
            if (sales.size() > port.pending.size() - port.size)
                return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-pending-quotes"});

            // Keep tentative inline rows outside the visible FIFO until the entire call is valid.
            for (std::size_t i = 0; i < sales.size(); ++i) {
                auto row = saleRow(sales[i]);

                if (!row)
                    return std::unexpected(row.error());

                port.pending[(port.head + port.size + i) % port.pending.size()] = std::move(*row);
            }

            port.size += sales.size();
        } else {
            std::array<SourceRow, MAX_STREAM_BATCH_RECORDS> pending;

            for (std::size_t i = 0; i < sales.size(); ++i) {
                auto row = saleRow(sales[i]);

                if (!row)
                    return std::unexpected(row.error());

                pending[i] = std::move(*row);
            }

            if (sales.size() > port.pending.size() - port.size)
                return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-pending-quotes"});

            for (std::size_t i = 0; i < sales.size(); ++i)
                port.pending[(port.head + port.size + i) % port.pending.size()] = std::move(pending[i]);

            port.size += sales.size();
        }
    }

    port.changed.notify_all();

    return {};
}

std::expected<void, Diagnostic> Publisher::publish(const events::Order &order) const {
    return publishEvent(port_, order, orderRow, true);
}

std::expected<void, Diagnostic>
Publisher::publishStreamRecords(std::span<const PublisherStreamRecordView> records) const {
    if (!port_)
        return std::unexpected(CLOSED);

    auto &port = *port_;
    {
        const std::lock_guard lock(port.mutex);

        if (port.closed)
            return std::unexpected(CLOSED);

        if (!port.localCatalog)
            return std::unexpected(port.remote ? LOCAL_SOURCE_DISABLED : RECORD_MODE);

        if (records.empty() || (!port.byteStream && records.size() > port.streamBatchRecords))
            return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "publisher-stream-batch"});

        if (records.size() > port.pendingRecords.size() - port.activeRecords)
            return std::unexpected(PENDING_RECORD_LIMIT);

        // A staged row lives in a free slot and remains invisible to the worker under the mutex.
        struct StagedRows {
            detail::PublisherPort &port;
            std::size_t count = 0;
            bool committed = false;

            ~StagedRows() {
                if (!committed)
                    for (std::size_t index = 0; index < count; ++index)
                        slot(index) = {};
            }

            RecordRow &slot(std::size_t index) const noexcept {
                return port.pendingRecords[(port.head + port.size + index) % port.pendingRecords.size()];
            }
        } staged{port};

        std::size_t charge = 0;

        try {
            for (std::size_t i = 0; i < records.size(); ++i) {
                const auto &row = records[i];
                const auto id = port.localCatalog->recordId(row.context);

                if (!id)
                    return std::unexpected(RECORD_SHAPE);

                const auto &layout = port.localCatalog->records()[static_cast<std::size_t>(*id)];
                const auto storage = recordStorage(layout, row.symbol, row.fields, port.recordLimits.rowBytes, false);

                if (!storage)
                    return std::unexpected(storage.error());

                if (storage->charge > port.recordLimits.pendingBytes - port.recordBytes - charge)
                    return std::unexpected(PENDING_RECORD_LIMIT);

                staged.slot(i) = copyRecordRow(STREAM_FAMILY, *id, row.symbol, row.fields, *storage, {});
                ++staged.count;
                charge += storage->charge;
            }
        } catch (const std::bad_alloc &) {
            return std::unexpected(ALLOCATION);
        }

        staged.committed = true;
        port.size += records.size();
        port.activeRecords += records.size();
        port.recordBytes += charge;
    }

    port.changed.notify_all();

    return {};
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
    if (!port_)
        return std::unexpected(CLOSED);

    auto &port = *port_;
    {
        const std::lock_guard lock(port.mutex);

        if (port.closed)
            return std::unexpected(CLOSED);

        if (!port.localCatalog)
            return std::unexpected(port.remote ? LOCAL_SOURCE_DISABLED : RECORD_MODE);

        const auto lane = familyIndex(family);
        const auto id = port.localCatalog->recordId(context);

        if (!lane || !id)
            return std::unexpected(RECORD_SHAPE);

        if (!validHistoryMetadata(family, history) || (family == Family::HISTORY && port.localHistory[*id] == 0))
            return std::unexpected(HISTORY_SHAPE);

        const auto &layout = port.localCatalog->records()[static_cast<std::size_t>(*id)];
        const auto removal = history.removalIndex.has_value();
        const auto storage = recordStorage(layout, symbol, fields, port.recordLimits.rowBytes, removal);

        if (!storage)
            return std::unexpected(storage.error());

        if (family == Family::HISTORY && !removal && reservedHistoryIndex(*layout.indexedTimeLayout, fields))
            return std::unexpected(HISTORY_SHAPE);

        if (port.activeRecords == port.pendingRecords.size() ||
            storage->charge > port.recordLimits.pendingBytes - port.recordBytes)
            return std::unexpected(PENDING_RECORD_LIMIT);

        // Hold the admission fence through copying so close and other producers cannot consume this reservation.
        try {
            const auto index = (port.head + port.size) % port.pendingRecords.size();
            port.pendingRecords[index] = copyRecordRow(*lane, *id, symbol, fields, *storage, history);
            ++port.size;
            ++port.activeRecords;
            port.recordBytes += storage->charge;
        } catch (const std::bad_alloc &) {
            return std::unexpected(ALLOCATION);
        }
    }

    port.changed.notify_all();

    return {};
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
                 wire->targetPayloadBytes >= wire->maxFrameBytes || wire->maxFrameBytes > MAX_FRAME_BYTES))
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
    snapshot.pendingPublications = impl_->port->localRecords ? impl_->port->activeRecords : impl_->port->size;

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
