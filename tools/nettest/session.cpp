// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "nettest.hpp"

#include "qd/events/event_flags.hpp"
#include "qd/transport/tcp_listener.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace org::ttldtor::qdcxx::nettest {
namespace {
namespace p = protocol;
namespace t = transport;
using core::SerialFieldType;
using core::SubscriptionFamily;
using namespace std::chrono_literals;
constexpr auto IDLE_CADENCE = 1ms;
constexpr auto HEARTBEAT_PERIOD = 1s;
constexpr auto DRAIN_TIMEOUT = 2s;
constexpr auto IO_TIMEOUT = 5s;
constexpr std::size_t IO_BYTES = 8192;
constexpr std::size_t SUBSCRIPTION_ENTRIES = 4096;
constexpr std::size_t MAX_PEER_DESCRIPTIONS = 1024;
constexpr std::array CONTROLS{p::LiveMessageView{0, u"HEARTBEAT", {}}, p::LiveMessageView{1, u"DESCRIBE_PROTOCOL", {}},
                              p::LiveMessageView{2, u"DESCRIBE_RECORDS", {}}};
constexpr std::array PROPERTIES{p::LivePropertyView{u"type", u"qtp"}, p::LivePropertyView{u"opt", u"hs"}};

struct Descriptor {
    std::array<p::LiveMessageView, 4> data{CONTROLS[0], CONTROLS[1], CONTROLS[2], {}};
    std::array<p::LiveMessageView, 5> subscriptions{CONTROLS[0], CONTROLS[1], CONTROLS[2], {}, {}};

    explicit Descriptor(const Messages &messages) {
        data.back() = messages.data;
        subscriptions[3] = messages.add;
        subscriptions.back() = messages.remove;
    }

    [[nodiscard]] p::LiveProtocolView producer() const noexcept {
        return {PROPERTIES, data, subscriptions};
    }

    [[nodiscard]] p::LiveProtocolView consumer() const noexcept {
        return {PROPERTIES, subscriptions, data};
    }
};

std::string namedError(std::string_view what, int code) {
    return std::string(what) + " (" + std::to_string(code) + ')';
}

template <class T> T require(std::expected<T, t::TcpDiagnostic> result) {
    if (!result)
        throw std::runtime_error(namedError(result.error().stage, static_cast<int>(result.error().error)) +
                                 " protocol=" + std::to_string(static_cast<int>(result.error().protocolError)));

    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}

template <class T> T require(std::expected<T, t::TcpListenerDiagnostic> result) {
    if (!result)
        throw std::runtime_error(namedError(result.error().stage, static_cast<int>(result.error().error)));

    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}

template <class T> T require(std::expected<T, p::LiveError> result, std::string_view what) {
    if (!result)
        throw std::runtime_error(namedError(what, static_cast<int>(result.error())));

    if constexpr (!std::is_void_v<T>)
        return std::move(*result);
}

bool advertises(std::span<const p::LiveMessageView> list, const p::LiveMessageView &message) {
    return std::ranges::any_of(list, [&](const auto &entry) {
        return entry.id == message.id && entry.name == message.name;
    });
}

p::LiveSession producerSession(const Catalog &catalog) {
    constexpr p::IncomingSubscriptionLimits limits{FRAME_BYTES,           1,
                                                   MAX_PEER_DESCRIPTIONS, SUBSCRIPTION_ENTRIES,
                                                   MAX_SYMBOL_UNITS,      SUBSCRIPTION_ENTRIES * MAX_SYMBOL_UNITS};
    auto schema = catalog.schema;
    auto composer = writer(catalog);
    auto work = protocolWork();

    return require(p::LiveSession::createServer(std::move(schema), std::move(composer), sessionLimits(), limits, work),
                   "Cannot configure producer session");
}

t::TcpRuntime consumerSession(t::RuntimeBootstrap &bootstrap, const Options &options, const Catalog &catalog) {
    constexpr std::size_t MIB = 1024 * 1024;
    const t::TcpOptions tcp{options.address,
                            options.port,
                            {},
                            53,
                            options.startup,
                            options.startup,
                            30s,
                            IO_TIMEOUT,
                            IDLE_CADENCE,
                            {IO_BYTES, IO_BYTES, 1, 1, FRAME_BYTES, IO_BYTES, 32 * MIB}};
    auto schema = catalog.schema;
    auto composer = writer(catalog);
    auto work = protocolWork();
    auto runtime =
        require(t::TcpRuntime::create(bootstrap, std::move(schema), std::move(composer), sessionLimits(), tcp, work));
    require(runtime.start());

    return runtime;
}

struct Demand {
    std::optional<std::int64_t> bound;
    bool active = false;
    bool snapshot = false;
};

struct PureRowTime {
    std::int64_t raw;
    unsigned millis;
};

PureRowTime sampleTime(std::uint64_t ordinal) {
    const auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    constexpr std::uint32_t SEQUENCE_BITS = 22;
    constexpr std::uint64_t SEQUENCE_MASK = (std::uint64_t{1} << SEQUENCE_BITS) - 1;
    const auto seconds = std::chrono::floor<std::chrono::seconds>(now);
    const auto millis = static_cast<unsigned>((now - seconds).count());
    const auto low = (std::uint64_t{millis} << SEQUENCE_BITS) | (ordinal & SEQUENCE_MASK);
    const auto bits = (static_cast<std::uint64_t>(seconds.count()) << 32) | low;

    return {std::bit_cast<std::int64_t>(bits), millis};
}

std::int64_t timeHigh(std::int64_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(std::bit_cast<std::uint64_t>(value) >> 32));
}

std::int64_t timeLow(std::int64_t value) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

void generate(std::span<core::FieldValueView> out, const core::LocalRecord &record, std::uint64_t ordinal,
              PureRowTime time) {
    constexpr auto TEXT = std::u16string_view(u"NetTest");
    static constexpr std::array BYTES{std::byte{0x51}, std::byte{0x44}};
    constexpr std::int64_t FIELD_SPREAD = 120;
    constexpr std::int64_t WIDE_DECIMAL_INTEGER_RANK = 128;

    for (std::size_t field = 0; field < out.size(); ++field) {
        const auto type = static_cast<SerialFieldType>(record.fields[field].serialTypeId);
        const auto n = static_cast<std::int64_t>((ordinal + field) % FIELD_SPREAD) + 1;
        auto &value = out[field];
        value = {type, n, {}, {}};

        switch (type) {
        case SerialFieldType::VOID_FIELD:
        case SerialFieldType::SERIAL_OBJECT:
            // Serialized objects require a Java/application codec. Generate a real wire null.
            value.raw = 0;
            break;
        case SerialFieldType::UTF_CHAR:
            value.raw = 'A' + n % 26;
            break;
        case SerialFieldType::TIME_SECONDS:
            value.raw = timeHigh(time.raw);
            break;
        case SerialFieldType::TIME_MILLIS:
            value.raw = timeHigh(time.raw) * 1000 + time.millis;
            break;
        case SerialFieldType::SEQUENCE:
            value.raw = timeLow(time.raw);
            break;
        case SerialFieldType::WIDE_DECIMAL:
            value.raw = (n << 8) | WIDE_DECIMAL_INTEGER_RANK;
            break;
        case SerialFieldType::UTF_CHAR_ARRAY:
        case SerialFieldType::STRING:
            value.raw = 0;
            value.units = TEXT;
            break;
        case SerialFieldType::BYTE_ARRAY:
        case SerialFieldType::CUSTOM_OBJECT:
            value.raw = 0;
            value.bytes = std::span(BYTES);
            break;
        default:
            break;
        }
    }

    if (record.indexedTimeLayout) {
        if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*record.indexedTimeLayout)) {
            const auto first = out[pair->firstField].type;
            const auto second = out[pair->secondField].type;

            if (first != SerialFieldType::VOID_FIELD)
                out[pair->firstField].raw = first == SerialFieldType::UTF_CHAR ? u'N' : timeHigh(time.raw);

            if (second != SerialFieldType::VOID_FIELD)
                out[pair->secondField].raw = timeLow(time.raw);
        } else if (const auto *one = std::get_if<core::IndexedTimeSingleInt64>(&*record.indexedTimeLayout)) {
            out[one->field].raw = time.raw;
        } else if (const auto *low = std::get_if<core::IndexedTimeLowInt32>(&*record.indexedTimeLayout)) {
            out[low->field].raw = timeLow(time.raw);
        }
    }
}

std::int64_t rawIndex(std::span<const core::FieldValueView> values, const core::IndexedTimeLayout &layout) {
    if (const auto *pair = std::get_if<core::IndexedTimePairInt32>(&layout)) {
        const auto high = static_cast<std::uint64_t>(values[pair->firstField].raw) << 32;
        const auto low = static_cast<std::uint32_t>(values[pair->secondField].raw);

        return std::bit_cast<std::int64_t>(high | low);
    }

    if (const auto *low = std::get_if<core::IndexedTimeLowInt32>(&layout))
        return static_cast<std::uint32_t>(values[low->field].raw);

    return values[std::get<core::IndexedTimeSingleInt64>(layout).field].raw;
}

std::size_t safeBatch(const Options &options, const core::LocalRecord &record) {
    // Every row reserves a worst-case full UTF-16 symbol, flags, sender ID and encoded fields.
    // For generated variable-length data, also reserve the fixed generator payload size.
    constexpr std::size_t COMPACT_LONG_BYTES = 9;
    constexpr std::size_t UTF8_BYTES_PER_UNIT = 3;
    constexpr std::size_t GENERATED_PAYLOAD_BYTES = 7 * UTF8_BYTES_PER_UNIT;
    const auto longestSymbol = std::ranges::max_element(options.symbols, {}, &std::u16string::size)->size();
    auto rowBytes = 3 * COMPACT_LONG_BYTES + longestSymbol * UTF8_BYTES_PER_UNIT;

    for (const auto &field : record.fields) {
        rowBytes += COMPACT_LONG_BYTES;

        switch (static_cast<SerialFieldType>(field.serialTypeId)) {
        case SerialFieldType::UTF_CHAR_ARRAY:
        case SerialFieldType::STRING:
        case SerialFieldType::BYTE_ARRAY:
        case SerialFieldType::CUSTOM_OBJECT:
            rowBytes += GENERATED_PAYLOAD_BYTES;
            break;
        default:
            break;
        }
    }

    return std::min(options.batch, (FRAME_BYTES - COMPACT_LONG_BYTES) / rowBytes);
}

class Generator {
    const std::size_t capacity_;
    std::vector<core::FieldValueView> values_;
    std::vector<p::LiveDataRecordView> rows_;
    std::vector<std::size_t> snapshots_;

    public:
    Generator(const Options &options, const core::LocalRecord &record)
        : capacity_(safeBatch(options, record)), values_(capacity_ * record.fields.size()) {
        rows_.reserve(capacity_);
        snapshots_.reserve(capacity_);
    }

    std::span<const p::LiveDataRecordView> build(const Options &options, const core::LocalRecord &record,
                                                 std::span<Demand> demand, bool historySnapshot, std::uint64_t &ordinal,
                                                 std::size_t count, std::uint64_t &generated) {
        rows_.clear();
        snapshots_.clear();
        std::array<bool, MAX_SYMBOLS> snapshotInBatch{};
        auto values = std::span(values_);

        for (std::size_t attempt = 0; attempt < count; ++attempt) {
            const auto index = static_cast<std::size_t>(ordinal++ % options.symbols.size());

            if (!demand[index].active)
                continue;

            auto fields = values.subspan(rows_.size() * record.fields.size(), record.fields.size());
            generate(fields, record, ordinal, sampleTime(ordinal));
            ++generated;

            if (demand[index].bound && rawIndex(fields, *record.indexedTimeLayout) < *demand[index].bound)
                continue;

            std::int32_t flags = 0;

            if (historySnapshot && demand[index].snapshot && !snapshotInBatch[index]) {
                flags = events::indexed_event_flags::SNAPSHOT_BEGIN | events::indexed_event_flags::SNAPSHOT_END;
                snapshotInBatch[index] = true;
                snapshots_.push_back(index);
            }

            rows_.push_back({SENDER_ID, options.symbols[index], fields, flags});
        }

        return rows_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    void accept(std::span<Demand> demand) const {
        for (const auto index : snapshots_)
            demand[index].snapshot = false;
    }
};

struct Peer {
    t::TcpConnectionId id;
    p::LiveSession session;
    std::array<std::byte, IO_BYTES> input;
    std::array<Demand, MAX_SYMBOLS> demand{};
    Clock::time_point accepted;
    Clock::time_point heartbeatAt;
    Clock::time_point writeDeadline;
    std::optional<Clock::time_point> offeredFrom;
    std::optional<p::LivePendingOutput> output;
    std::size_t inputStart = 0;
    std::size_t inputEnd = 0;
    std::size_t outputRecords = 0;
    std::uint64_t offered = 0;
    std::uint64_t ordinal = 0;
    bool described = false;

    Peer(t::TcpConnectionId connection, const Catalog &catalog, const Descriptor &descriptor, Clock::time_point now)
        : id(connection), session(producerSession(catalog)), accepted(now), heartbeatAt(now + HEARTBEAT_PERIOD),
          writeDeadline(now + IO_TIMEOUT) {
        auto work = protocolWork();
        output.emplace(
            require(session.composeProtocol(descriptor.producer(), work), "Cannot compose producer descriptor"));
    }

    void submit(p::LivePendingOutput message, std::size_t records, Clock::time_point now) {
        output.emplace(std::move(message));
        outputRecords = records;
        writeDeadline = now + IO_TIMEOUT;
    }
};

bool readPeer(t::TcpListener &listener, Peer &peer, const Options &options, const Catalog &catalog,
              const Messages &wire, Report &report, bool &activity) {
    if (peer.inputStart == peer.inputEnd) {
        const auto read = listener.read(peer.id, peer.input);

        if (!read || read->status == t::TcpIoStatus::PEER_EOF)
            return false;

        if (read->status == t::TcpIoStatus::WOULD_BLOCK)
            return true;

        peer.inputStart = 0;
        peer.inputEnd = read->bytes;
        report.bytesRead += read->bytes;
    }

    auto work = protocolWork();
    const auto fed =
        peer.session.feedServer(std::span(peer.input).subspan(peer.inputStart, peer.inputEnd - peer.inputStart), work);

    if (fed.status == p::LiveStatus::FAILED || fed.accepted == 0) {
        const auto diagnostic = peer.session.diagnostic();
        throw std::runtime_error(namedError(diagnostic.stage, static_cast<int>(diagnostic.error)));
    }

    activity = true;
    peer.inputStart += fed.accepted;

    if (!fed.subscription)
        return true;

    const auto action = std::to_underlying(fed.subscription->message);

    if (action != wire.add.id && action != wire.remove.id)
        return true;

    const auto context = catalog.schema.recordContext(SENDER_ID);

    for (const auto &entry : fed.subscription->entries) {
        if (!entry.context.matches(context))
            continue;

        const auto symbol = std::ranges::find(options.symbols, entry.symbol);

        if (symbol == options.symbols.end())
            continue;

        auto &demand = peer.demand[static_cast<std::size_t>(symbol - options.symbols.begin())];
        demand = {entry.historyBound, action == wire.add.id, true};
    }

    return true;
}

bool writePeer(t::TcpListener &listener, Peer &peer, const Options &options, Report &report, Clock::time_point now,
               bool &activity) {
    if (!peer.output)
        return true;

    if (now >= peer.writeDeadline)
        return false;

    const auto pending = peer.output->pendingBytes();
    const bool profiled = options.profileProducer && peer.outputRecords != 0;
    Clock::time_point started;

    if (profiled)
        started = Clock::now();

    const auto written = listener.write(peer.id, pending);

    if (profiled) {
        auto &profile = report.profile;
        profile.writeNs += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count());
        ++profile.writeAttempts;

        if (written && written->status == t::TcpIoStatus::WOULD_BLOCK)
            ++profile.wouldBlock;
        else if (written && written->status == t::TcpIoStatus::PROGRESS && written->bytes < pending.size())
            ++profile.partialWrites;
    }

    if (!written)
        return false;

    if (written->status == t::TcpIoStatus::WOULD_BLOCK)
        return true;

    activity = true;
    require(peer.output->consume(written->bytes), "Invalid producer write boundary");
    report.bytesWritten += written->bytes;
    peer.writeDeadline = now + IO_TIMEOUT;

    if (peer.output->pendingBytes().empty()) {
        report.written += peer.outputRecords;
        peer.output.reset();
    }

    return true;
}

bool enqueue(Peer &peer, const Options &options, const Catalog &catalog, const Messages &wire, Generator &generator,
             Report &report, Clock::time_point now) {
    const auto snapshot = peer.session.peer();
    const auto protocol = snapshot.protocol();
    const bool ready = advertises(protocol.receive, wire.data);
    auto work = protocolWork();

    if (!peer.described && ready) {
        peer.submit(require(peer.session.describe(std::span(&SENDER_ID, 1), work), "Cannot describe physical record"),
                    0, now);
        peer.described = true;

        return true;
    }

    if (peer.described && std::ranges::any_of(peer.demand, &Demand::active)) {
        if (!peer.offeredFrom)
            peer.offeredFrom = now;

        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - *peer.offeredFrom).count();
        constexpr std::uint64_t NANOSECONDS_PER_SECOND = 1000000000;
        const auto available = static_cast<std::uint64_t>(elapsed) * options.rate / NANOSECONDS_PER_SECOND;
        const auto count =
            static_cast<std::size_t>(std::min<std::uint64_t>(available - peer.offered, generator.capacity()));
        peer.offered += count;

        if (count != 0) {
            const auto &record = catalog.schema.records().front();
            Clock::time_point started;

            if (options.profileProducer)
                started = Clock::now();

            const auto rows =
                generator.build(options, record, peer.demand,
                                peer.session.historySnapshotEnabled() && options.family == SubscriptionFamily::HISTORY,
                                peer.ordinal, count, report.generated);

            if (options.profileProducer) {
                report.profile.generationNs += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count());
                ++report.profile.generatedBatches;
            }

            if (!rows.empty()) {
                if (options.profileProducer)
                    started = Clock::now();

                auto data = peer.session.publishData(static_cast<p::LiveDataMessage>(wire.data.id), rows, work);

                if (options.profileProducer) {
                    report.profile.composeNs += static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count());
                    ++report.profile.composedFrames;
                }

                peer.submit(require(std::move(data), "Cannot compose record batch"), rows.size(), now);
                report.composed += rows.size();
                generator.accept(peer.demand);

                return true;
            }
        }
    } else {
        peer.offeredFrom.reset();
        peer.offered = 0;
    }

    if (now >= peer.heartbeatAt) {
        peer.submit(require(peer.session.composeHeartbeat({false, {}, {}, {}, {}}, work), "Cannot compose heartbeat"),
                    0, now);
        peer.heartbeatAt = now + HEARTBEAT_PERIOD;

        return true;
    }

    return false;
}

struct Consumer {
    enum class State { PROTOCOL, DESCRIBE, ADD, SUBSCRIBED, ACTIVE, CLOSED };

    t::TcpRuntime runtime;
    State state = State::PROTOCOL;
    std::uint64_t outputBaseline = 0;
    std::uint64_t readBaseline = 0;
    std::uint64_t readBytesBaseline = 0;
    std::uint64_t writtenBytesBaseline = 0;
    std::uint64_t delivered = 0;
    std::uint64_t checksum = 14695981039346656037ULL;
    Clock::time_point heartbeatAt{};

    Consumer(t::RuntimeBootstrap &bootstrap, const Options &options, const Catalog &catalog)
        : runtime(consumerSession(bootstrap, options, catalog)) {
    }
};

/** Match the producer's 22-bit wire sequence against its full-width generated ordinal. */
constexpr bool matchesStreamSequence(std::uint32_t sequence, std::uint64_t ordinal) {
    constexpr std::uint64_t SEQUENCE_MASK = (std::uint64_t{1} << 22) - 1;

    return (sequence & SEQUENCE_MASK) == (ordinal & SEQUENCE_MASK);
}

static_assert(matchesStreamSequence(0x003fffff, 0x003fffff));
static_assert(matchesStreamSequence(0, 0x00400000));
static_assert(matchesStreamSequence(1, 0x00400001));
static_assert(!matchesStreamSequence(1, 0x00400000));

/** Check one deterministic record and update the time-independent FNV-1a checksum. */
void verifyStreamRecord(Consumer &connection, const p::LiveRecordView &record, const core::LocalRecord &schema) {
    constexpr std::uint64_t MAX_RECORDS = 3600ULL * 1000000ULL;
    constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
    const auto values = record.typedValues;

    if (record.localId != SENDER_ID || record.symbol != u"IBM" || record.eventFlags != 0 ||
        values.size() != schema.fields.size() || values.size() != 10 || connection.delivered >= MAX_RECORDS)
        throw std::runtime_error("Unexpected verification record, symbol, flags or sequence range");

    const auto sequence = static_cast<std::uint32_t>(values[1].raw);
    const auto ordinal = connection.delivered + 1;

    if (!matchesStreamSequence(sequence, ordinal) || (sequence >> 22) >= 1000)
        throw std::runtime_error("TimeAndSale sequence is missing, repeated or out of order");

    const auto high = static_cast<std::uint32_t>(values[0].raw);
    const auto timeBits = (static_cast<std::uint64_t>(high) << 32) | sequence;
    const PureRowTime time{std::bit_cast<std::int64_t>(timeBits), sequence >> 22};
    std::array<core::FieldValueView, 10> expected{};
    generate(expected, schema, ordinal, time);

    for (std::size_t field = 0; field < values.size(); ++field) {
        const auto &actual = values[field];
        const auto &want = expected[field];

        if (actual.type != want.type || actual.raw != want.raw || actual.units || actual.bytes)
            throw std::runtime_error("TimeAndSale field differs at record " + std::to_string(ordinal) + ", field " +
                                     std::to_string(field));

        // Time seconds and the millisecond bits vary between producer runs.
        const auto normalized = field == 0   ? std::uint64_t{0}
                                : field == 1 ? ordinal
                                             : static_cast<std::uint64_t>(actual.raw);

        for (unsigned byte = 0; byte < 8; ++byte) {
            connection.checksum ^= (normalized >> (byte * 8)) & 0xff;
            connection.checksum *= FNV_PRIME;
        }
    }

    ++connection.delivered;

    if (connection.delivered == 1) {
        std::cout << "ACTIVE" << std::endl;

        if (!std::cout)
            throw std::runtime_error("Cannot report verifier activation");
    }
}

bool pumpConsumer(Consumer &connection, const Options &options, const Descriptor &descriptor, const Messages &wire,
                  std::span<const core::SubscriptionRequest> requests, const core::LocalRecord &schema, bool measuring,
                  bool stopping, Clock::time_point now) {
    auto &runtime = connection.runtime;
    auto work = protocolWork();
    t::TcpWork native{8, 8, 2 * IO_BYTES, 0, 0, 0, 0};
    auto progress = require(runtime.poll(native, work));
    const auto stats = runtime.statistics();

    if (progress.status == t::TcpPollStatus::CLOSED) {
        if (const auto end = runtime.terminal();
            end && end->primary.error != t::TcpError::NONE && !(stopping && end->aborted)) {
            throw std::runtime_error(namedError(end->primary.stage, static_cast<int>(end->primary.error)) +
                                     " protocol=" + std::to_string(static_cast<int>(end->primary.protocolError)));
        }

        connection.state = Consumer::State::CLOSED;

        return true;
    }

    if (options.verifyStream && progress.event && progress.event->kind() == p::LiveEventKind::DATA &&
        progress.event->messageType() != wire.data.id)
        throw std::runtime_error("Verifier received an unexpected data contract");

    if (progress.event && progress.event->kind() == p::LiveEventKind::DATA &&
        progress.event->messageType() == wire.data.id && (measuring || options.verifyStream)) {
        for (const auto &record : progress.event->records()) {
            if (options.verifyStream)
                verifyStreamRecord(connection, record, schema);
            else if (record.localId == SENDER_ID &&
                     std::ranges::find(options.symbols, record.symbol) != options.symbols.end())
                ++connection.delivered;
        }
    }

    if (stopping || runtime.state() != t::TcpState::CONNECTED)
        return progress.status != t::TcpPollStatus::IDLE;

    const auto queue = [&](auto result, Consumer::State next) {
        // Windows can expose native write completion before retiring the sole output owner.
        if (!result && result.error().error == t::TcpError::RESOURCE_LIMIT && result.error().stage == "output-slot")
            return false;

        require(std::move(result));
        connection.outputBaseline = stats.writtenMessages.value;
        connection.state = next;

        return true;
    };

    const auto sent = stats.writtenMessages.value > connection.outputBaseline;

    switch (connection.state) {
    case Consumer::State::PROTOCOL:
        return queue(runtime.composeProtocol(descriptor.consumer(), work), Consumer::State::DESCRIBE);
    case Consumer::State::DESCRIBE: {
        const auto snapshot = runtime.peer();
        const auto peer = snapshot.protocol();

        if (sent && advertises(peer.send, wire.data) && advertises(peer.receive, wire.add))
            return queue(runtime.describe(std::span(&SENDER_ID, 1), work), Consumer::State::ADD);

        break;
    }
    case Consumer::State::ADD:

        if (sent)
            return queue(runtime.subscribe(static_cast<p::SubscriptionMessage>(wire.add.id), requests, work),
                         Consumer::State::SUBSCRIBED);

        break;
    case Consumer::State::SUBSCRIBED:

        if (sent) {
            connection.heartbeatAt = now + HEARTBEAT_PERIOD;
            connection.state = Consumer::State::ACTIVE;

            return true;
        }

        break;
    case Consumer::State::ACTIVE:

        if (!options.verifyStream && sent && now >= connection.heartbeatAt) {
            connection.heartbeatAt = now + HEARTBEAT_PERIOD;

            return queue(runtime.composeHeartbeat({false, {}, {}, {}, {}}, work), Consumer::State::ACTIVE);
        }

        break;
    case Consumer::State::CLOSED:
        break;
    }

    return progress.status != t::TcpPollStatus::IDLE;
}
} // namespace

Report produce(const Options &options, const Catalog &catalog) {
    auto bootstrap = require(t::RuntimeBootstrap::create());
    auto listener = require(t::TcpListener::create(bootstrap, {options.address, options.port, 16, options.instances}));
    const auto wire = messages(options.family);
    const Descriptor descriptor(wire);
    std::vector<std::optional<Peer>> peers(options.instances);
    Generator generator(options, catalog.schema.records().front());
    Report report;
    std::optional<Clock::time_point> started;
    const auto startupDeadline = Clock::now() + options.startup;
    std::cout << "PORT " << listener.port() << std::endl;

    if (!std::cout)
        throw std::runtime_error("Cannot report producer port");

    for (;;) {
        const auto now = Clock::now();
        const bool draining = started && now >= *started + options.duration;

        if (draining &&
            (now >= *started + options.duration + DRAIN_TIMEOUT || std::ranges::none_of(peers, [](const auto &peer) {
                 return peer && peer->output && peer->outputRecords;
             })))
            break;

        bool activity = false;

        if (!draining) {
            const auto vacant = std::ranges::find_if(peers, [](const auto &peer) {
                return !peer;
            });

            if (vacant != peers.end()) {
                if (const auto connection = require(listener.accept())) {
                    vacant->emplace(*connection, catalog, descriptor, now);
                    ++report.connections;
                    activity = true;
                }
            }
        }

        for (auto &peer : peers) {
            if (!peer)
                continue;

            const auto usable = draining || readPeer(listener, *peer, options, catalog, wire, report, activity);
            const auto timedOut = !peer->described && now >= peer->accepted + options.startup;

            if (!usable || timedOut || !writePeer(listener, *peer, options, report, now, activity)) {
                require(listener.retire(peer->id));
                peer.reset();
                continue;
            }

            if (!draining && !peer->output) {
                activity |= enqueue(*peer, options, catalog, wire, generator, report, now);

                if (!started && peer->offeredFrom)
                    started = peer->offeredFrom;
            }
        }

        if (!started && now >= startupDeadline)
            throw std::runtime_error("Producer timed out waiting for matching demand");

        if (!activity)
            std::this_thread::sleep_for(IDLE_CADENCE);
    }

    require(listener.shutdown());
    report.measured = options.duration;

    return report;
}

Report consume(const Options &options, const Catalog &catalog) {
    const auto &schema = catalog.schema.records().front();

    if (options.verifyStream) {
        constexpr std::array<std::int32_t, 10> TYPES{56, 72, 2, 104, 120, 120, 120, 120, 40, 8};

        if (schema.fields.size() != TYPES.size())
            throw std::invalid_argument("--verify-stream requires the default TimeAndSale layout");

        constexpr std::array<std::u16string_view, 10> NAMES{u"Time",      u"Sequence",  u"Exchange",
                                                            u"TradeId",   u"Price",     u"Size",
                                                            u"Bid.Price", u"Ask.Price", u"ExchangeSaleConditions",
                                                            u"Flags"};

        for (std::size_t field = 0; field < TYPES.size(); ++field) {
            const auto lookup = std::ranges::find(schema.lookups, NAMES[field], &core::FieldLookup::name);

            if (schema.fields[field].serialTypeId != TYPES[field] || lookup == schema.lookups.end() ||
                lookup->fieldId != field)
                throw std::invalid_argument("--verify-stream requires the default TimeAndSale field layout");
        }
    }

    auto bootstrap = require(t::RuntimeBootstrap::create());
    const auto wire = messages(options.family);
    const Descriptor descriptor(wire);
    std::vector<core::SubscriptionRequest> requests;
    const auto bound =
        options.family == SubscriptionFamily::HISTORY ? std::optional{options.historyBound} : std::nullopt;

    for (const auto &symbol : options.symbols)
        requests.push_back({{catalog.schema.recordContext(SENDER_ID), symbol}, bound});

    std::vector<Consumer> connections;
    connections.reserve(options.instances);

    for (std::size_t index = 0; index < options.instances; ++index)
        connections.emplace_back(bootstrap, options, catalog);

    const auto startupDeadline = Clock::now() + options.startup;
    std::optional<Clock::time_point> started;
    Clock::time_point ended;

    for (;;) {
        const auto now = Clock::now();
        bool activity = false;

        for (auto &connection : connections)
            if (connection.state != Consumer::State::CLOSED)
                activity |= pumpConsumer(connection, options, descriptor, wire, requests, schema, started.has_value(),
                                         false, now);

        if (!started) {
            const bool active = std::ranges::all_of(connections, [](const auto &connection) {
                return connection.state == Consumer::State::ACTIVE;
            });

            if (active) {
                started = now;

                for (auto &connection : connections) {
                    const auto stats = connection.runtime.statistics();
                    connection.readBaseline = options.verifyStream ? 0 : stats.readDataRecords.value;
                    connection.readBytesBaseline = stats.readBytes.value;
                    connection.writtenBytesBaseline = stats.writtenBytes.value;
                }
            } else if (now >= startupDeadline || std::ranges::any_of(connections, [](const auto &connection) {
                           return connection.state == Consumer::State::CLOSED;
                       })) {
                throw std::runtime_error("Consumer timed out waiting for negotiated subscriptions");
            }
        }

        if (started &&
            (now >= *started + options.duration || std::ranges::all_of(connections, [](const auto &connection) {
                 return connection.state == Consumer::State::CLOSED;
             }))) {
            if (options.verifyStream && std::ranges::any_of(connections, [](const auto &connection) {
                    return connection.state != Consumer::State::CLOSED;
                }))
                throw std::runtime_error("Verifier deadline expired before source closed");

            ended = now;
            break;
        }

        if (!activity)
            std::this_thread::sleep_for(IDLE_CADENCE);
    }

    Report report;
    report.measured = ended - *started;

    for (auto &connection : connections) {
        const auto stats = connection.runtime.statistics();

        if (options.verifyStream && connection.delivered == 0)
            throw std::runtime_error("Verifier received no records");

        report.delivered += connection.delivered;
        report.checksum = connection.checksum;
        report.wireRecords += stats.readDataRecords.value - connection.readBaseline;
        report.bytesRead += stats.readBytes.value - connection.readBytesBaseline;
        report.bytesWritten += stats.writtenBytes.value - connection.writtenBytesBaseline;
        report.connections += stats.establishedConnections.value;

        if (connection.state != Consumer::State::CLOSED)
            require(connection.runtime.requestClose());
    }

    const auto deadline = ended + DRAIN_TIMEOUT;

    while (std::ranges::any_of(connections, [](const auto &connection) {
        return connection.state != Consumer::State::CLOSED;
    })) {
        const auto now = Clock::now();

        if (now >= deadline)
            throw std::runtime_error("Consumer close did not quiesce before the deadline");

        bool activity = false;

        for (auto &connection : connections)
            if (connection.state != Consumer::State::CLOSED)
                activity |= pumpConsumer(connection, options, descriptor, wire, requests, schema, false, true, now);

        if (!activity)
            std::this_thread::sleep_for(IDLE_CADENCE);
    }

    return report;
}
} // namespace org::ttldtor::qdcxx::nettest
