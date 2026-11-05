// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/penta_symbol.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"

#ifdef _WIN32
#    include <windows.h>
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <fcntl.h>
#    include <netinet/tcp.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::connect_tests {
namespace {
namespace e = encoding;
namespace p = protocol;
using namespace std::chrono_literals;
constexpr std::int32_t REMOVE_EVENT = 2;          ///< Exact QTP removal flag used by an explicit test source record.
constexpr std::int32_t FIRST_PEER_RECORD_ID = 17; ///< Default source sender; unrelated to client outgoing ADD IDs.
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket NO_SOCKET = INVALID_SOCKET; ///< Invalid Windows-owned socket.
#else
using Socket = int;
constexpr Socket NO_SOCKET = -1; ///< Invalid POSIX-owned socket.
#endif

/** Check harness setup independently of optimized assertion removal.
 * @param value Required condition.
 */
void require(bool value) {
    if (!value) {
        throw std::runtime_error("controlled peer setup failure");
    }
}

/** Select an explicit peer source sender without conflating it with a client outgoing record ID.
 * @param localRecordId Dense position in the source schema used to encode these controlled values.
 * @return Sender17+local record position, or controlled setup failure on invalid/overflowing input.
 */
std::int32_t senderId(std::int32_t localRecordId) {
    require(localRecordId >= 0 && localRecordId <= std::numeric_limits<std::int32_t>::max() - FIRST_PEER_RECORD_ID);

    return FIRST_PEER_RECORD_ID + localRecordId;
}

/** Close exactly one test-owned socket.
 * @param socket Descriptor or invalid sentinel.
 */
void closeSocket(Socket socket) noexcept {
    if (socket == NO_SOCKET) {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

/** Recognize a client-close reset for the explicitly selected two-session fixture only.
 * @return True for connection reset/abort/not-connected; nonblocking would-block is false.
 */
bool clientCloseError() noexcept {
#ifdef _WIN32
    const auto error = WSAGetLastError();

    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN;
#else

    return errno == ECONNRESET || errno == ENOTCONN;
#endif
}

/** Select nonblocking operation only for a test-owned socket.
 * @param socket Owned listener or accepted connection.
 */
void nonblocking(Socket socket) {
#ifdef _WIN32
    u_long enabled = 1;
    require(ioctlsocket(socket, FIONBIO, &enabled) == 0);
    require(SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0) != 0);
#else
    require(fcntl(socket, F_SETFL, O_NONBLOCK) == 0);
    require(fcntl(socket, F_SETFD, FD_CLOEXEC) == 0);
#endif
}

/** Append a canonical compact primitive to finite test-owned storage.
 * @param bytes Output storage.
 * @param value Exact signed wire value.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> encoded{};
    const auto result = e::writeCompactLong(value, encoded);
    require(result.status == e::Status::OK);
    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append one bounded exact UTFString.
 * @param bytes Output storage.
 * @param value Non-null exact UTF-16.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 8192> encoded{};
    const auto result = e::writeUtfString(value, encoded, {8192, 1024});
    require(result.status == e::Status::OK);
    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append one exact UTFChar primitive.
 * @param bytes Output storage.
 * @param value Exact code point/unit.
 */
void character(std::vector<std::byte> &bytes, std::int32_t value) {
    std::array<std::byte, 4> encoded{};
    const auto result = e::writeUtfChar(value, encoded);
    require(result.status == e::Status::OK);
    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap one exact finite payload with canonical QTP framing.
 * @param type Message identity.
 * @param payload Borrowed complete body after identity.
 * @return Complete owning frame.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> bytes;
    number(bytes, static_cast<std::int64_t>(payload.size() + 1));
    number(bytes, type);
    bytes.insert(bytes.end(), payload.begin(), payload.end());

    return bytes;
}

/** Encode one full supported test value without business conversion.
 * @param bytes Output storage.
 * @param field Exact typed local value.
 */
void field(std::vector<std::byte> &bytes, const core::FieldValue &field) {
    if (field.type == core::SerialFieldType::VOID_FIELD) {
        // Physical VOID slots retain schema identity but occupy no wire bytes.
        return;
    } else if (field.type == core::SerialFieldType::UTF_CHAR) {
        character(bytes, static_cast<std::int32_t>(field.raw));
    } else if (field.type == core::SerialFieldType::INT) {
        constexpr unsigned BYTE_BITS = 8;
        constexpr unsigned INT_BYTES = 4;
        const auto raw = static_cast<std::uint32_t>(field.raw);

        for (unsigned i = INT_BYTES; i > 0; --i) {
            bytes.push_back(static_cast<std::byte>(raw >> ((i - 1) * BYTE_BITS)));
        }
    } else if (field.type == core::SerialFieldType::STRING) {
        std::array<std::byte, 8192> encoded{};
        const auto result = e::writeUtfString(
            field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt, encoded, {8192, 1024});
        require(result.status == e::Status::OK);
        bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
    } else if (field.type == core::SerialFieldType::UTF_CHAR_ARRAY) {
        std::array<std::byte, 8192> encoded{};
        const auto result = e::writeCharArray(
            field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt, encoded, {8192, 1024});
        require(result.status == e::Status::OK);
        bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
    } else if (field.type == core::SerialFieldType::BYTE_ARRAY || field.type == core::SerialFieldType::CUSTOM_OBJECT ||
               field.type == core::SerialFieldType::SERIAL_OBJECT) {
        // Canonical nonnegative int32 lengths have the same compact INT/LONG bytes. The SERIAL_OBJECT
        // reader keeps its distinct projected-INT acceptance grammar; this peer emits canonical forms only.
        require(!field.bytes ||
                field.bytes->size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
        number(bytes, field.bytes ? static_cast<std::int64_t>(field.bytes->size()) : -1);

        if (field.bytes) {
            bytes.insert(bytes.end(), field.bytes->begin(), field.bytes->end());
        }
    } else {
        number(bytes, field.raw);
    }
}

/** Exact known family message identities used by the native peer only. */
struct WireFamily {
    std::int32_t data;
    std::int32_t add;
    std::int32_t remove;
    std::u16string_view dataName;
    std::u16string_view addName;
    std::u16string_view removeName;
};

/** Select the actual public wire family without changing the legacy default.
 * @param family Explicit scripted family.
 * @return Exact known message identities/names, or static fixture failure.
 */
WireFamily wireFamily(core::SubscriptionFamily family) {
    switch (family) {
    case core::SubscriptionFamily::STREAM:
        return {15, 16, 17, u"STREAM_DATA", u"STREAM_ADD_SUBSCRIPTION", u"STREAM_REMOVE_SUBSCRIPTION"};
    case core::SubscriptionFamily::TICKER:
        return {10, 11, 12, u"TICKER_DATA", u"TICKER_ADD_SUBSCRIPTION", u"TICKER_REMOVE_SUBSCRIPTION"};
    case core::SubscriptionFamily::HISTORY:
        return {20, 21, 22, u"HISTORY_DATA", u"HISTORY_ADD_SUBSCRIPTION", u"HISTORY_REMOVE_SUBSCRIPTION"};
    }

    throw std::runtime_error("controlled peer family failure");
}

/** Compose peer readiness then complete source descriptions from the explicit schema.
 * @param schema Ordinary local schema, used solely for deterministic peer source data.
 * @param family Exact scripted wire family.
 * @param describeAllRecords Describe all source records instead of the default first record only.
 * @param fieldMetadata Declare the exact leading EventTime56/EventSequence72 metadata words.
 * @return Finite owning protocol/description prefix.
 */
std::vector<std::byte> initial(const core::LocalSchema &schema, WireFamily family, bool describeAllRecords,
                               bool fieldMetadata) {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(body, 0);
    number(body, 1);
    number(body, family.data);
    text(body, family.dataName);
    number(body, 0);
    number(body, 2);

    for (const auto &[id, name] :
         {std::pair{family.add, family.addName}, std::pair{family.remove, family.removeName}}) {
        number(body, id);
        text(body, name);
        number(body, 0);
    }

    auto result = frame(1, body);
    body.clear();
    require(!schema.records().empty());
    const auto records = describeAllRecords ? schema.records() : schema.records().first(1);

    for (const auto &record : records) {
        number(body, senderId(record.id));
        text(body, record.name);
        number(body, static_cast<std::int64_t>(record.fields.size() + (fieldMetadata ? 2 : 0)));

        if (fieldMetadata) {
            text(body, u"EventTime");
            number(body, std::to_underlying(core::SerialFieldType::TIME_SECONDS));
            text(body, u"EventSequence");
            number(body, std::to_underlying(core::SerialFieldType::SEQUENCE));
        }

        for (std::size_t index = 0; index < record.fields.size(); ++index) {
            const auto lookup = std::ranges::find_if(record.lookups, [index](const auto &item) {
                return item.fieldId == index;
            });
            require(lookup != record.lookups.end());
            text(body, lookup->name);
            number(body, record.fields[index].serialTypeId);
        }
    }

    const auto described = frame(2, body);
    result.insert(result.end(), described.begin(), described.end());

    return result;
}

/** Compose one complete family DATA frame containing exact selected and unselected tuples.
 * @param records Complete explicit test values.
 * @param family Actual selected wire message.
 * @param canonical Explicit canonical symbol profile, leaving the legacy FD default unchanged.
 * @return Finite owning data frame.
 */
std::vector<std::byte> data(std::span<const Record> records, WireFamily family, bool canonical) {
    std::vector<std::byte> body;

    for (const auto &record : records) {
        if (record.flags != 0) {
            body.push_back(std::byte{0xf9});
            number(body, record.flags);
        }

        const auto penta = canonical ? e::encodePentaSymbol(record.symbol) : std::nullopt;

        if (penta) {
            body.insert(body.end(), penta->bytes.begin(),
                        penta->bytes.begin() + static_cast<std::ptrdiff_t>(penta->size));
        } else {
            body.push_back(std::byte{0xfd});
            number(body, static_cast<std::int64_t>(record.symbol.size()));

            for (const auto unit : record.symbol)
                character(body, unit);
        }

        number(body, senderId(record.localRecordId));

        if (record.fieldTime) {
            require(record.fieldTime->rawSequence.has_value());
            number(body, record.fieldTime->rawSeconds);
            number(body, *record.fieldTime->rawSequence);
        }

        if ((record.flags & REMOVE_EVENT) != 0) {
            require(record.removalTime.has_value());
            number(body, *record.removalTime);
        } else {
            require(!record.removalTime);

            for (const auto &value : record.fields) {
                field(body, value);
            }
        }
    }

    return frame(family.data, body);
}

/** Decode the actual received exact subscription tuples, without treating composition as receipt.
 * @param bytes Complete frame payload after message identity.
 * @param expected Exact client symbol sequence, or multiset when membership marks are supplied.
 * @param historyBound Actual expected raw bound for HISTORY ADD, absent for other commands.
 * @param recordId Exact expected client outgoing record identity, independent of the peer's sender identity.
 * @param recordIds Optional explicit client outgoing ID for each expected tuple; empty preserves recordId.
 * @param completed Complete tuples already validated in preceding frames of this one expected command.
 * @param matched Optional complete group membership marks; empty preserves ordered matching.
 * @return True only for identical complete frame tuples; completed advances atomically on success.
 */
bool exactSubscription(std::span<const std::byte> bytes, std::span<const std::u16string> expected,
                       std::optional<std::int64_t> historyBound, std::int32_t recordId,
                       std::span<const std::int32_t> recordIds, std::size_t &completed,
                       std::span<unsigned char> matched = {}) {
    if ((!recordIds.empty() && recordIds.size() != expected.size()) ||
        (!matched.empty() && matched.size() != expected.size())) {
        return false;
    }

    e::SymbolReader symbols({8192, 128, 128}, 0);
    std::size_t index = completed;

    while (!bytes.empty()) {
        auto symbol = symbols.read(bytes);

        if (symbol.status != e::SymbolStatus::OK || index == expected.size() || symbol.value->eventFlags != 0 ||
            (matched.empty() && symbol.value->units != expected[index])) {
            return false;
        }

        bytes = bytes.subspan(symbol.consumed);
        const auto id = e::readCompactInt(bytes);

        if (id.status != e::Status::OK ||
            (matched.empty() && id.value != (recordIds.empty() ? recordId : recordIds[index]))) {
            return false;
        }

        std::size_t member = index;

        if (!matched.empty()) {
            member = 0;

            while (member < expected.size() && (matched[member] || symbol.value->units != expected[member] ||
                                                id.value != (recordIds.empty() ? recordId : recordIds[member])))
                ++member;

            if (member == expected.size())
                return false;
        }

        bytes = bytes.subspan(id.consumed);

        if (historyBound) {
            const auto bound = e::readCompactLong(bytes);

            if (bound.status != e::Status::OK || bound.value != *historyBound)
                return false;

            bytes = bytes.subspan(bound.consumed);
        }

        if (!matched.empty())
            matched[member] = 1;

        ++index;
    }

    completed = index;

    return true;
}
} // namespace

std::vector<std::byte> fixturePeerPrefix(const core::LocalSchema &schema, core::SubscriptionFamily family,
                                         bool describeAllRecords, bool fieldMetadata) {
    return initial(schema, wireFamily(family), describeAllRecords, fieldMetadata);
}

std::vector<std::byte> fixturePeerData(std::span<const Record> records, core::SubscriptionFamily family,
                                       bool canonical) {
    return data(records, wireFamily(family), canonical);
}

Peer::Peer(const core::LocalSchema &schema, std::span<const Record> records, std::span<const std::u16string> expected,
           std::size_t chunk, PeerMode mode, PeerScript script)
    : initial_(mode == PeerMode::NO_READINESS
                   ? std::vector<std::byte>{}
                   : initial(schema, wireFamily(script.family), script.describeAllRecords, script.fieldMetadata)),
      data_(data(records, wireFamily(script.family), script.canonicalSymbols)),
      afterRemove_(script.afterRemove ? data(*script.afterRemove, wireFamily(script.family), script.canonicalSymbols)
                                      : data_),
      afterReconnect_(script.afterReconnect
                          ? data(*script.afterReconnect, wireFamily(script.family), script.canonicalSymbols)
                          : std::vector<std::byte>{}),
      afterFinalRemove_(script.afterFinalRemove && !script.afterFinalRemove->empty()
                            ? data(*script.afterFinalRemove, wireFamily(script.family), script.canonicalSymbols)
                            : std::vector<std::byte>{}),
      afterFollowupAdd_(script.afterFollowupAdd && !script.afterFollowupAdd->empty()
                            ? data(*script.afterFollowupAdd, wireFamily(script.family), script.canonicalSymbols)
                            : std::vector<std::byte>{}),
      expected_(expected.begin(), expected.end()), chunk_(chunk), mode_(mode), script_(std::move(script)),
      dataReleased_(!script_.waitForDataRelease) {
    require(chunk != 0);
    require(script_.subscriptionRecordIds.empty() || script_.subscriptionRecordIds.size() == expected_.size());

    for (const auto &record : records) {
        require(record.fieldTime.has_value() == script_.fieldMetadata);
    }

    if (script_.afterRemove) {
        for (const auto &record : *script_.afterRemove) {
            require(record.fieldTime.has_value() == script_.fieldMetadata);
        }
    }

    if (script_.afterReconnect) {
        require(mode_ == PeerMode::HOLD);

        for (const auto &record : *script_.afterReconnect) {
            require(record.fieldTime.has_value() == script_.fieldMetadata);
        }
    }

    if (script_.removeSequence) {
        for (const auto &command : *script_.removeSequence) {
            require(!command.symbols.empty() && command.symbols.size() == command.recordIds.size());
        }
    }

    if (script_.afterFinalRemove) {
        require(script_.removeSequence && !script_.removeSequence->empty());

        for (const auto &record : *script_.afterFinalRemove) {
            require(record.fieldTime.has_value() == script_.fieldMetadata);
        }
    }

    if (script_.followupAdd) {
        require(!script_.followupAdd->unordered && !script_.followupAdd->symbols.empty());
        require(script_.followupAdd->symbols.size() == script_.followupAdd->recordIds.size());
    }

    if (script_.afterFollowupAdd) {
        require(script_.followupAdd.has_value());

        for (const auto &record : *script_.afterFollowupAdd) {
            require(record.fieldTime.has_value() == script_.fieldMetadata);
        }
    }

    require((script_.family == core::SubscriptionFamily::HISTORY) == script_.historyBound.has_value());
    const auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    require(listener != NO_SOCKET);
    listener_ = static_cast<std::intptr_t>(listener);

    try {
        nonblocking(listener);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        require(listen(listener, 1) == 0);
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        require(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        port_ = ntohs(address.sin_port);
        worker_ = std::jthread([this](std::stop_token stop) {
            serve(stop);
        });
    } catch (...) {
        closeSocket(listener);
        listener_ = -1;

        throw;
    }
}

Peer::~Peer() {
    worker_.request_stop();

    if (worker_.joinable()) {
        worker_.join();
    }

    closeSocket(static_cast<Socket>(listener_));
}

void Peer::serve(std::stop_token stop) noexcept {
    Socket connection = NO_SOCKET;

    try {
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        std::vector<std::byte> output = initial_;
        std::size_t sent = 0;
        bool ending = false;
        bool initialQueued = false;
        bool addReceived = false;
        bool reconnected = false;
        std::size_t initialEnd = 0;
        std::size_t afterRemoveEnd = 0;
        std::size_t addTuples = 0;
        std::size_t followupTuples = 0;
        bool followupReceived = false;
        std::size_t removeTuples = 0;
        std::size_t removeSet = 0;
        std::vector<unsigned char> removeMatches;
        const auto family = wireFamily(script_.family);
        p::FrameReader reader({8192, 8201});

        while (!stop.stop_requested() && std::chrono::steady_clock::now() < deadline) {
            if (connection == NO_SOCKET) {
                connection = accept(static_cast<Socket>(listener_), nullptr, nullptr);

                if (connection != NO_SOCKET) {
                    nonblocking(connection);
                    const int immediate = 1;
                    require(setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&immediate),
                                       sizeof(immediate)) == 0);
                }
            } else {
                if (!initialQueued && addReceived && dataReleased_.load() && mode_ != PeerMode::SILENT &&
                    mode_ != PeerMode::NO_READINESS) {
                    const auto &selectedData = reconnected ? afterReconnect_ : data_;
                    output.insert(output.end(), selectedData.begin(), selectedData.end());
                    initialEnd = output.size();
                    initialQueued = true;

                    if (mode_ == PeerMode::PARTIAL_END)
                        output.push_back(std::byte{0x81});

                    ending = mode_ == PeerMode::CLEAN_END || mode_ == PeerMode::PARTIAL_END;
                }

                if (sent < output.size()) {
                    const auto count = (std::min)(chunk_, output.size() - sent);
#ifdef _WIN32
                    const auto written = send(connection, reinterpret_cast<const char *>(output.data() + sent),
                                              static_cast<int>(count), 0);
#else
                    const auto written = send(connection, output.data() + sent, count, MSG_NOSIGNAL);
#endif

                    if (written > 0) {
                        sent += static_cast<std::size_t>(written);

                        if (initialEnd != 0 && sent >= initialEnd)
                            initialDataSent_ = true;

                        if (afterRemoveEnd != 0 && sent >= afterRemoveEnd)
                            afterRemoveSent_ = true;
                    }
                } else if (ending) {
                    break;
                }

                std::array<std::byte, 8192> input{};
#ifdef _WIN32
                const auto count =
                    recv(connection, reinterpret_cast<char *>(input.data()), static_cast<int>(input.size()), 0);
#else
                const auto count = recv(connection, input.data(), input.size(), 0);
#endif

                if (count == 0 || (count < 0 && script_.afterReconnect && clientCloseError())) {
                    if (!reconnected && script_.afterReconnect) {
                        require(initialQueued && sent >= initialEnd);
                        closeSocket(connection);
                        connection = NO_SOCKET;
                        output = initial_;
                        sent = 0;
                        ending = false;
                        initialQueued = false;
                        addReceived = false;
                        reconnected = true;
                        initialEnd = afterRemoveEnd = 0;
                        addTuples = removeTuples = removeSet = followupTuples = 0;
                        followupReceived = false;
                        removeMatches.clear();
                        reader.reset();

                        continue;
                    }

                    break;
                }

                if (count > 0) {
                    auto bytes = std::span(input).first(static_cast<std::size_t>(count));

                    while (!bytes.empty()) {
                        auto next = reader.feed(bytes);
                        require(next.status != p::FrameStatus::FAILED && next.accepted != 0);
                        bytes = bytes.subspan(next.accepted);

                        if (next.frame && (next.frame->type == family.add || next.frame->type == family.remove)) {
                            const auto adding = next.frame->type == family.add;
                            auto expected = std::span<const std::u16string>(expected_);
                            auto expectedIds = std::span<const std::int32_t>(script_.subscriptionRecordIds);

                            if (!adding && script_.removeSequence) {
                                require(removeSet < script_.removeSequence->size());
                                const auto &command = (*script_.removeSequence)[removeSet];
                                expected = command.symbols;
                                expectedIds = command.recordIds;
                            }

                            if (adding && addReceived) {
                                require(script_.followupAdd.has_value() && !followupReceived);
                                expected = script_.followupAdd->symbols;
                                expectedIds = script_.followupAdd->recordIds;
                            }

                            auto &tuples = adding ? (addReceived ? followupTuples : addTuples) : removeTuples;
                            std::span<unsigned char> matched;

                            if (!adding && script_.removeSequence && (*script_.removeSequence)[removeSet].unordered) {
                                if (removeMatches.empty())
                                    removeMatches.resize(expected.size());

                                matched = removeMatches;
                            }

                            require(exactSubscription(next.frame->payload, expected,
                                                      adding ? script_.historyBound : std::nullopt,
                                                      script_.subscriptionRecordId, expectedIds, tuples, matched));

                            if (tuples != expected.size()) {
                                continue;
                            }

                            if (adding) {
                                if (!addReceived) {
                                    addReceived = true;
                                } else {
                                    followupReceived = true;
                                    output.insert(output.end(), afterFollowupAdd_.begin(), afterFollowupAdd_.end());
                                }

                                ++adds_;
                            } else {
                                if (script_.removeSequence) {
                                    ++removeSet;
                                    removeTuples = 0;
                                    removeMatches.clear();
                                } else {
                                    require(removes_.load() == 0);
                                }

                                ++removes_;
                                const bool finalResponse = script_.afterFinalRemove && script_.removeSequence &&
                                                           removeSet == script_.removeSequence->size();
                                const auto &response = finalResponse ? afterFinalRemove_ : afterRemove_;
                                output.insert(output.end(), response.begin(), response.end());
                                afterRemoveEnd = output.size();
                            }
                        }
                    }
                }
            }

            std::this_thread::sleep_for(1ms);
        }
    } catch (...) {
        failed_ = true;
    }

    closeSocket(connection);
}

std::uint16_t Peer::port() const noexcept {
    return port_;
}

unsigned Peer::adds() const noexcept {
    return adds_.load();
}

unsigned Peer::removes() const noexcept {
    return removes_.load();
}

void Peer::releaseData() noexcept {
    dataReleased_ = true;
}

bool Peer::initialDataSent() const noexcept {
    return initialDataSent_.load();
}

bool Peer::afterRemoveSent() const noexcept {
    return afterRemoveSent_.load();
}

bool Peer::failed() const noexcept {
    return failed_.load();
}
} // namespace org::ttldtor::qdcxx::connect_tests
