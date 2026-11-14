// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "qd/api/publisher.hpp"

#include "../protocol/local_event_owner.hpp"
#include "../transport/raw_byte_owner.hpp"
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
constexpr std::size_t DATA_FIELDS = 10;
constexpr std::int32_t QUOTE_ID = 0;
constexpr std::int32_t SEQUENCE_BITS = 22;
constexpr std::int32_t SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::chrono::milliseconds IO_CADENCE{2};
constexpr std::chrono::seconds HEARTBEAT_PERIOD{5};
constexpr std::chrono::seconds HANDSHAKE_TIMEOUT{10};
constexpr std::chrono::seconds WRITE_TIMEOUT{10};

constexpr std::array<std::u16string_view, DATA_FIELDS> QUOTE_FIELD_NAMES{
    u"Bid.Time",     u"Bid.Exchange", u"Bid.Price", u"Bid.Size", u"Ask.Time",
    u"Ask.Exchange", u"Ask.Price",    u"Ask.Size",  u"Sequence", u"TimeNanoPart"};
constexpr std::array<FieldType, DATA_FIELDS> QUOTE_FIELD_TYPES{
    FieldType::TIME_SECONDS, FieldType::UTF_CHAR,   FieldType::WIDE_DECIMAL, FieldType::WIDE_DECIMAL,
    FieldType::TIME_SECONDS, FieldType::UTF_CHAR,   FieldType::WIDE_DECIMAL, FieldType::WIDE_DECIMAL,
    FieldType::SEQUENCE,     FieldType::COMPACT_INT};

constexpr core::SchemaLimits SCHEMA_LIMITS{1, DATA_FIELDS, DATA_FIELDS, 32, 256};
constexpr Diagnostic QUOTE_SHAPE{ErrorCode::INVALID_ARGUMENT, "publisher-quote-shape"};
constexpr Diagnostic ALLOCATION{ErrorCode::ALLOCATION_FAILURE, "publisher-storage"};
constexpr Diagnostic CLOSED{ErrorCode::CLOSED, "publisher-closed"};
constexpr Diagnostic LOCAL_SOURCE_DISABLED{ErrorCode::INVALID_STATE, "publisher-remote-source"};

/** Copied input with bounded text and an exact ordered local field representation. */
struct QuoteRow {
    std::array<char16_t, SYMBOL_UNITS> symbol;
    std::size_t length;
    std::array<core::FieldValue, DATA_FIELDS> fields;

    [[nodiscard]] std::u16string_view text() const noexcept {
        return {symbol.data(), length};
    }
};

std::int64_t floorSeconds(std::int64_t milliseconds) noexcept {
    return milliseconds / MILLIS_PER_SECOND - (milliseconds % MILLIS_PER_SECOND < 0);
}

bool exactSymbol(std::u16string_view symbol) noexcept {
    return !symbol.empty() && symbol.size() <= SYMBOL_UNITS && symbol.front() != u'*';
}

std::expected<QuoteRow, Diagnostic> quoteRow(const events::Quote &quote) {
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
    const std::array<std::int64_t, DATA_FIELDS> raw{bid,
                                                    quote.bidExchangeCode,
                                                    quote.bidPrice.raw,
                                                    quote.bidSize.raw,
                                                    ask,
                                                    quote.askExchangeCode,
                                                    quote.askPrice.raw,
                                                    quote.askSize.raw,
                                                    std::bit_cast<std::int32_t>(sequence),
                                                    quote.timeNanoPart};
    QuoteRow row{};
    row.length = quote.symbol.size();
    std::ranges::copy(quote.symbol, row.symbol.begin());

    for (std::size_t i = 0; i < row.fields.size(); ++i)
        row.fields[i] = {QUOTE_FIELD_TYPES[i], raw[i]};

    return row;
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

mux::QueueWork serverQueueTurn(std::size_t peers, bool remote) noexcept {
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
    const auto epochBytes = remote ? REMOTE_EPOCH_BYTES : SOURCE_EPOCH_BYTES;
    const auto maximum = std::numeric_limits<std::size_t>::max();
    work.ticker.bytes =
        keys + 2 > (maximum - work.ticker.bytes) / epochBytes ? maximum : work.ticker.bytes + (keys + 2) * epochBytes;

    return work;
}

protocol::LiveLimits liveLimits() noexcept {
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

core::LocalSchema quoteSchema() {
    core::LocalRecord record{QUOTE_ID, u"Quote", false, {}, {}};

    for (std::size_t i = 0; i < QUOTE_FIELD_NAMES.size(); ++i) {
        record.fields.push_back({std::to_underlying(QUOTE_FIELD_TYPES[i])});
        record.lookups.push_back({std::u16string(QUOTE_FIELD_NAMES[i]), i});
    }

    auto schema = core::LocalSchema::create(std::span(&record, 1), SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);

    if (!schema)
        throw Error({schema.error() == core::SchemaError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                             : ErrorCode::INVALID_STATE,
                     "publisher-schema", 0, static_cast<std::uint32_t>(schema.error())});

    return std::move(*schema);
}

protocol::SubscriptionWriter quoteWriter(const core::LocalSchema &schema) {
    protocol::OutgoingRecord outgoing{QUOTE_ID, QUOTE_ID, u"Quote", {}};

    for (std::size_t i = 0; i < QUOTE_FIELD_NAMES.size(); ++i)
        outgoing.fields.push_back({std::u16string(QUOTE_FIELD_NAMES[i]), std::to_underlying(QUOTE_FIELD_TYPES[i]),
                                   static_cast<std::int32_t>(i)});

    protocol::SubscriptionWriterWork writerWork{64, 65536, 65536, 256 * 1024};
    auto writer =
        protocol::SubscriptionWriter::create(schema, std::span(&outgoing, 1), {SCHEMA_LIMITS, 1, DATA_FIELDS, 32, 256},
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
    auto writer = quoteWriter(schema);
    auto local = schema;
    auto session = protocol::LiveSession::createServer(
        std::move(local), std::move(writer), liveLimits(),
        {MAX_FRAME_BYTES, 1, 128, CLIENT_KEYS, SYMBOL_UNITS, CLIENT_SYMBOL_UNITS}, work);

    if (!session)
        throw Error(protocolDiagnostic(session.error(), "publisher-server-session"));

    return std::move(*session);
}

constexpr mux::ClientLimits CLIENT_LIMITS{32, 512 * 1024, 8,    128 * 1024, 1, 32,          SYMBOL_UNITS,
                                          0,  0,          4096, 2,          4, CLIENT_KEYS, CLIENT_SYMBOL_UNITS};

mux::MuxQueue tickerQueue(const core::LocalSchema &schema, std::size_t peers, bool remote) {
    const auto allKeys = peers * CLIENT_KEYS;
    const auto allUnits = peers * CLIENT_LIMITS.commandSymbolUnits;
    const core::SubscriptionLimits desired{1, allKeys, SYMBOL_UNITS, allUnits, allKeys * 2, allUnits * 2, allKeys};
    const mux::MuxLimits interests{1, peers, allKeys, allKeys, SYMBOL_UNITS, allUnits, CLIENT_KEYS, 8 * 1024 * 1024};
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
    const auto cacheFrames = allKeys + 1;
    const auto cacheBytes = cacheFrames * ((remote ? REMOTE_EVENT_BYTES : EVENT_BYTES) + EVENT_OWNER_ALLOWANCE);
    const collectors::TickerLimits ticker{peers,  allKeys, cacheFrames, cacheBytes, cacheBytes + 32 * 1024 * 1024,
                                          desired};
    auto work = queueTurn();
    auto queue = mux::MuxQueue::createTicker(schema, interests, ticker, {false, 100}, limits, work);

    if (!queue)
        throw Error(queueDiagnostic(queue.error(), "publisher-ticker"));

    return std::move(*queue);
}

protocol::detail::LocalEventOwner localOwner() {
    auto work = liveTurn();
    protocol::LiveLimits limits{};
    limits.residentBytes = SOURCE_EPOCH_BYTES;
    limits.owners = MAX_PEERS * CLIENT_KEYS + 4;
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
    std::shared_ptr<Delivery> delivery;
    mux::MuxHandle handle;
    std::optional<mux::MuxCommandTicket> command;
    std::optional<protocol::LivePendingOutput> output;
    Clock::time_point acceptedAt;
    Clock::time_point lastWrite;
    Clock::time_point heartbeatAt;
    OutputKind outputKind = OutputKind::PROTOCOL;
    bool sentProtocol = false;
    bool sentDescription = false;
    bool ready = false;
    bool closing = false;
    bool socketClosed = false;
    std::array<std::byte, INPUT_BYTES> input;
    std::size_t inputStart = 0;
    std::size_t inputEnd = 0;
};

constexpr std::array<protocol::LiveMessageView, 4> SERVER_SEND{
    {{0, u"HEARTBEAT", {}}, {1, u"DESCRIBE_PROTOCOL", {}}, {2, u"DESCRIBE_RECORDS", {}}, {10, u"TICKER_DATA", {}}}};
constexpr std::array<protocol::LiveMessageView, 5> SERVER_RECEIVE{{{0, u"HEARTBEAT", {}},
                                                                   {1, u"DESCRIBE_PROTOCOL", {}},
                                                                   {2, u"DESCRIBE_RECORDS", {}},
                                                                   {11, u"TICKER_ADD_SUBSCRIPTION", {}},
                                                                   {12, u"TICKER_REMOVE_SUBSCRIPTION", {}}}};

bool canReceiveTicker(const protocol::LiveSession &session) noexcept {
    const auto peer = session.peer();

    return std::ranges::any_of(peer.protocol().receive, [](const protocol::LiveMessageView &message) {
        return message.id == 10 && message.name == u"TICKER_DATA";
    });
}

std::expected<protocol::LivePendingOutput, protocol::LiveError> composeQuote(Peer &peer, protocol::LiveWork &work) {
    const auto &record = peer.delivery->batch.records().front();
    std::array<core::FieldValueView, DATA_FIELDS> fields{};

    if (record.typedValues.size() != fields.size())
        return std::unexpected(protocol::LiveError::INVALID_CONFIGURATION);

    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto &field = record.typedValues[i];
        fields[i] = {field.type, field.raw, field.units, field.bytes};
    }

    const protocol::LiveDataRecordView outgoing{QUOTE_ID, record.symbol, fields};

    return peer.session.publishData(protocol::LiveDataMessage::TICKER_DATA, std::span(&outgoing, 1), work);
}

} // namespace

namespace detail {
/** Detached bounded producer state. It contains no worker or native owner. */
struct PublisherPort {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<QuoteRow> pending;
    std::vector<std::u16string> demand;
    const bool remote;
    PublisherSourceStatus source;
    std::size_t head = 0;
    std::size_t size = 0;
    std::uint16_t boundPort = 0;
    bool started = false;
    bool closed = false;
    Diagnostic diagnostic;

    PublisherPort(std::size_t count, bool remote) : pending(remote ? 0 : count), remote(remote) {
    }
};
} // namespace detail

namespace {
constexpr std::array<protocol::LiveMessageView, 5> SOURCE_SEND{{{0, u"HEARTBEAT", {}},
                                                                {1, u"DESCRIBE_PROTOCOL", {}},
                                                                {2, u"DESCRIBE_RECORDS", {}},
                                                                {11, u"TICKER_ADD_SUBSCRIPTION", {}},
                                                                {12, u"TICKER_REMOVE_SUBSCRIPTION", {}}}};
constexpr std::array<protocol::LiveMessageView, 4> SOURCE_RECEIVE{
    {{0, u"HEARTBEAT", {}}, {1, u"DESCRIBE_PROTOCOL", {}}, {2, u"DESCRIBE_RECORDS", {}}, {10, u"TICKER_DATA", {}}}};
constexpr protocol::LivePropertyView SOURCE_PROPERTIES[]{{u"type", u"qtp"}};

Diagnostic sourceDiagnostic(transport::TcpDiagnostic diagnostic) noexcept {
    return {ErrorCode::TRANSPORT_FAILURE, diagnostic.stage, diagnostic.nativeCode,
            static_cast<std::uint32_t>(diagnostic.error)};
}

bool hasCapability(std::span<const protocol::LiveMessageView> messages, const protocol::LiveMessageView &expected) {
    return std::ranges::any_of(messages, [&](const auto &message) {
        return message.id == expected.id && message.name == expected.name;
    });
}

/** One upstream connection. Its raw output slot copies and owns the whole frame through the local write barrier. */
class SourceConnection {
    protocol::LiveSession session_;
    std::unique_ptr<transport::detail::RawByteOwner> bytes_;
    Clock::time_point handshakeDeadline_;
    Clock::time_point heartbeatAt_;
    std::optional<std::uint64_t> pendingDemand_;
    bool described_ = false;
    bool ready_ = false;
    std::optional<protocol::LiveEvent> pendingEvent_;

    static protocol::LiveSession newSession(const core::LocalSchema &schema, protocol::LiveWork &work) {
        auto writer = quoteWriter(schema);
        auto limits = liveLimits();
        limits.residentBytes = REMOTE_EPOCH_BYTES;
        limits.eventBytes = REMOTE_EVENT_BYTES;
        limits.owners = MAX_PEERS * CLIENT_KEYS + 12;
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
            ready_ = session_.authentication() != protocol::LiveAuthentication::REQUIRED &&
                     hasCapability(descriptor.send, SOURCE_RECEIVE.back()) &&
                     hasCapability(descriptor.receive, SOURCE_SEND[3]) &&
                     hasCapability(descriptor.receive, SOURCE_SEND[4]);

            return;
        }

        if (incoming.event->kind() == protocol::LiveEventKind::DATA) {
            if (!ready_ || incoming.event->messageType() != std::to_underlying(protocol::LiveDataMessage::TICKER_DATA))
                throw Error({ErrorCode::PROTOCOL_FAILURE, "publisher-upstream-data"});

            if (!incoming.event->records().empty())
                pendingEvent_ = std::move(incoming.event);
        }
    }

    public:
    SourceConnection(const core::LocalSchema &schema, const transport::detail::BootstrapLease &bootstrap,
                     PublisherSourceOptions options, Clock::time_point now, protocol::LiveWork &work)
        : session_(newSession(schema, work)), bytes_(newTransport(bootstrap, options, work)),
          handshakeDeadline_(now + HANDSHAKE_TIMEOUT), heartbeatAt_(now + HEARTBEAT_PERIOD) {
        const auto started = bytes_->start();

        if (!started)
            throw Error(sourceDiagnostic(started.error()));

        send(session_.composeProtocol({SOURCE_PROPERTIES, SOURCE_SEND, SOURCE_RECEIVE}, work), work);
    }

    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    void progress(mux::MuxQueue &queue, mux::DemandTracker &demand, Clock::time_point now, mux::QueueWork &queueWork,
                  mux::DemandWork &demandWork) {
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
            const auto admitted = queue.admit(std::move(*pendingEvent_), queueWork);

            if (admitted)
                pendingEvent_.reset();
            else if (admitted.error().error != mux::QueueError::BUSY &&
                     admitted.error().error != mux::QueueError::WORK_LIMIT)
                throw Error(queueDiagnostic(admitted.error(), "publisher-upstream-admit"));
        }

        if (bytes_->pendingOutput())
            return;

        if (pendingDemand_) {
            if (!demand.acknowledge(*pendingDemand_))
                throw Error({ErrorCode::INVALID_STATE, "publisher-upstream-demand-ack"});

            pendingDemand_.reset();
        }

        if (ready_ && !described_) {
            send(session_.describe(std::span(&QUOTE_ID, 1), work), work);
            described_ = true;

            return;
        }

        if (ready_) {
            const auto next = demand.next(queue.aggregateEntries(), demandWork);

            if (!next)
                throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-upstream-demand", 0,
                             static_cast<std::uint32_t>(next.error())});

            if (*next) {
                const auto message = (*next)->action == mux::DemandAction::ADD
                                         ? protocol::SubscriptionMessage::TICKER_ADD
                                         : protocol::SubscriptionMessage::TICKER_REMOVE;
                send(session_.subscribe(message, std::span(&(*next)->request, 1), work), work);
                pendingDemand_ = (*next)->sequence;

                return;
            }
        }

        if (now >= heartbeatAt_) {
            send(session_.composeHeartbeat({false, {}, {}, {}, {}}, work), work);
            heartbeatAt_ = now + HEARTBEAT_PERIOD;
        }
    }
};

class QuoteServer {
    core::LocalSchema schema_;
    std::optional<protocol::detail::LocalEventOwner> source_;
    mux::MuxQueue queue_;
    transport::TcpListener listener_;
    std::vector<std::optional<Peer>> peers_;
    std::optional<protocol::LiveEvent> pendingSource_;
    std::string sourceAddress_;
    std::optional<PublisherSourceOptions> sourceOptions_;
    std::optional<transport::detail::BootstrapLease> sourceBootstrap_;
    std::optional<mux::DemandTracker> sourceDemand_;
    std::optional<SourceConnection> upstream_;
    Clock::time_point retryAt_{};
    bool resetPending_ = false;
    PublisherSourceStatus sourceStatus_;
    std::uint64_t publishedRevision_ = 0;

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
            auto delivery = std::make_shared<Delivery>();
            auto work = queueTurn();
            auto handle = queue_.open(CLIENT_LIMITS, delivery, work);

            if (!handle)
                throw Error(queueDiagnostic(handle.error(), "publisher-open-client"));

            const auto now = Clock::now();
            vacant->emplace(Peer{**accepted,
                                 std::move(session),
                                 std::move(delivery),
                                 std::move(*handle),
                                 {},
                                 {},
                                 now,
                                 now,
                                 now + HEARTBEAT_PERIOD});
        } catch (...) {
            const auto result = listener_.retire(**accepted);

            if (!result)
                throw Error(nativeDiagnostic(result.error()));
        }
    }

    void retire(Peer &peer) noexcept {
        if (!peer.closing) {
            peer.closing = true;
            peer.output.reset();
            peer.delivery->batch = {};
            peer.command.reset();
            (void)peer.handle.requestStop(mux::StopMode::ABORT);
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

            if (result.event->kind() == protocol::LiveEventKind::PROTOCOL)
                peer.ready = canReceiveTicker(peer.session);
        }

        if (result.subscription) {
            if (!peer.ready)
                return false;

            mux::QueueSubmitWork submit{8, 65536, 65536, 1024 * 1024};
            auto ticket = queue_.submitIncomingSubscription(peer.handle, *result.subscription, submit);

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
            output = peer.session.composeProtocol({{}, SERVER_SEND, SERVER_RECEIVE}, work);
        } else if (peer.ready && !peer.sentDescription) {
            kind = OutputKind::DESCRIPTION;
            output = peer.session.describe(std::span(&QUOTE_ID, 1), work);
        } else if (peer.sentDescription && !peer.delivery->batch.records().empty()) {
            kind = OutputKind::DATA;
            output = composeQuote(peer, work);
        } else if (now >= peer.heartbeatAt) {
            output = peer.session.composeHeartbeat({false, {}, {}, {}, {}}, work);
        } else {
            return true;
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
            peer.delivery->batch = {};
            (void)peer.handle.requestResume();
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

            if (const auto notice = peer.handle.resetNotice(); notice && !notice->globalAckPending)
                (void)peer.handle.acknowledgeReset(notice->sequence);

            if (peer.closing || peer.handle.terminal() || (!peer.ready && now - peer.acceptedAt >= HANDSHAKE_TIMEOUT) ||
                !receive(peer, work) || !send(peer, now, work)) {
                retire(peer);

                if (peer.socketClosed && peer.handle.terminal())
                    entry.reset();
            }
        }
    }

    void publishDemand(detail::PublisherPort &port) {
        const auto revision = queue_.revision();

        if (publishedRevision_ == revision)
            return;

        const auto entries = queue_.aggregateEntries();
        std::vector<std::u16string> symbols;
        symbols.reserve(entries.size());

        for (const auto &entry : entries)
            symbols.emplace_back(entry.key.symbol);

        {
            const std::lock_guard lock(port.mutex);
            port.demand = std::move(symbols);
        }

        publishedRevision_ = revision;
        port.changed.notify_all();
    }

    void publishQuote(detail::PublisherPort &port, mux::QueueWork &work) {
        if (!pendingSource_) {
            std::optional<QuoteRow> quote;
            {
                const std::lock_guard lock(port.mutex);

                if (port.size == 0 || port.closed)
                    return;

                quote = port.pending[port.head];
                port.head = (port.head + 1) % port.pending.size();
                --port.size;
            }
            auto liveWork = liveTurn();
            auto event = source_->quote(schema_.recordContext(QUOTE_ID), quote->text(), quote->fields, {}, liveWork);

            if (!event)
                throw Error(protocolDiagnostic(event.error(), "publisher-local-event"));

            pendingSource_ = std::move(*event);
        }

        const auto admitted = queue_.admit(std::move(*pendingSource_), work);

        if (admitted)
            pendingSource_.reset();
        else if (admitted.error().error != mux::QueueError::BUSY)
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

            if (!peer.delivery->batch.records().empty()) {
                peer.delivery->batch = {};
                (void)peer.handle.requestResume();
            }
        }
    }

    void progressSource(detail::PublisherPort &port, mux::QueueWork &work) {
        const auto now = Clock::now();
        const auto keys = peers_.size() * CLIENT_KEYS;
        // One validation and search pass over every pair in each of the two finite demand maps.
        const auto comparisons = 4 * (keys + 1) * (keys + 1);
        mux::DemandWork demandWork{4, comparisons, comparisons * SYMBOL_UNITS};

        if (resetPending_) {
            const auto reset = queue_.resetStatus();

            if (reset.failure)
                throw Error(queueDiagnostic(*reset.failure, "publisher-upstream-reset"));

            if (reset.phase != mux::ResetPhase::NONE || !reset.globalAcknowledged)
                return;

            resetPending_ = false;
        }

        try {
            if (!upstream_) {
                if (now < retryAt_)
                    return;

                auto liveWork = liveTurn();
                upstream_.emplace(schema_, *sourceBootstrap_, *sourceOptions_, now, liveWork);
            }

            upstream_->progress(queue_, *sourceDemand_, now, work, demandWork);
        } catch (const Error &failure) {
            if (sourceStatus_.connected || (upstream_ && upstream_->ready())) {
                fenceSource();
                auto resetWork = serverQueueTurn(peers_.size(), true);
                const auto reset = queue_.resetSource(resetWork);

                if (!reset)
                    throw Error(queueDiagnostic(reset.error(), "publisher-upstream-reset"));

                resetPending_ = true;
            }

            upstream_.reset();

            if (!sourceDemand_->reset(demandWork))
                throw Error({ErrorCode::INVALID_STATE, "publisher-upstream-demand-reset"});

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
    QuoteServer(transport::RuntimeBootstrap &bootstrap, PublisherOptions options)
        : schema_(quoteSchema()),
          queue_(tickerQueue(schema_, options.listener.maxConnections, options.source.has_value())),
          listener_(listen(bootstrap, options.listener)), peers_(options.listener.maxConnections) {
        if (!options.source) {
            source_.emplace(localOwner());

            return;
        }

        sourceAddress_ = options.source->address;
        sourceOptions_ = *options.source;
        sourceOptions_->address = sourceAddress_;
        auto lease = transport::detail::BootstrapLease::acquire(bootstrap);

        if (!lease)
            throw Error(sourceDiagnostic(lease.error()));

        sourceBootstrap_.emplace(std::move(*lease));
        auto demand = mux::DemandTracker::create(core::SubscriptionFamily::TICKER,
                                                 {peers_.size() * CLIENT_KEYS, peers_.size() * CLIENT_SYMBOL_UNITS});

        if (!demand)
            throw Error({ErrorCode::RESOURCE_LIMIT, "publisher-upstream-demand-storage"});

        sourceDemand_.emplace(std::move(*demand));
    }

    [[nodiscard]] std::uint16_t port() const noexcept {
        return listener_.port();
    }

    void progress(detail::PublisherPort &port) {
        acceptPeer();
        progressPeers();
        auto work = serverQueueTurn(peers_.size(), sourceOptions_.has_value());
        constexpr std::size_t QUEUE_ACTIONS = 16;

        for (std::size_t i = 0; i < QUEUE_ACTIONS; ++i) {
            const auto moved = queue_.progress(work);

            if (!moved) {
                if (moved.error().error == mux::QueueError::WORK_LIMIT)
                    break;

                throw Error(queueDiagnostic(moved.error(), "publisher-progress"));
            }
        }

        if (const auto terminal = queue_.terminal())
            throw Error({ErrorCode::INVALID_STATE, "publisher-queue-terminal", 0,
                         static_cast<std::uint32_t>(terminal->outcome)});

        publishDemand(port);

        if (sourceOptions_)
            progressSource(port, work);
        else
            publishQuote(port, work);
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
        QuoteServer server(bootstrap, options);
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
        std::vector<QuoteRow>().swap(port.pending);
        std::vector<std::u16string>().swap(port.demand);
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

std::expected<void, Diagnostic> Publisher::publish(const events::Quote &quote) const {
    if (port_ && port_->remote) {
        const std::lock_guard lock(port_->mutex);

        return std::unexpected(port_->closed ? CLOSED : LOCAL_SOURCE_DISABLED);
    }

    auto row = quoteRow(quote);

    if (!row)
        return std::unexpected(row.error());

    if (!port_)
        return std::unexpected(CLOSED);

    {
        const std::lock_guard lock(port_->mutex);

        if (port_->closed)
            return std::unexpected(CLOSED);

        if (port_->size == port_->pending.size())
            return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "publisher-pending-quotes"});

        const auto index = (port_->head + port_->size) % port_->pending.size();
        port_->pending[index] = std::move(*row);
        ++port_->size;
    }

    port_->changed.notify_all();

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
        impl->port = std::make_shared<detail::PublisherPort>(options.pendingQuotes, options.source.has_value());
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
    if (!exactSymbol(symbol))
        return false;

    auto &port = *impl_->port;
    std::unique_lock lock(port.mutex);
    const auto matches = [&] {
        return (std::ranges::find(port.demand, symbol) != port.demand.end()) == subscribed;
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
