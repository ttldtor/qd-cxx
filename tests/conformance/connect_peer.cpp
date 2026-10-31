// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"

#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
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
#include <chrono>
#include <stdexcept>

namespace org::ttldtor::qdcxx::connect_tests {
namespace {
namespace e = encoding;
namespace p = protocol;
using namespace std::chrono_literals;
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
    if (field.type == core::SerialFieldType::UTF_CHAR) {
        character(bytes, static_cast<std::int32_t>(field.raw));
    } else if (field.type == core::SerialFieldType::UTF_CHAR_ARRAY) {
        std::array<std::byte, 8192> encoded{};
        const auto result = e::writeCharArray(
            field.units ? std::optional<std::u16string_view>(*field.units) : std::nullopt, encoded, {8192, 1024});
        require(result.status == e::Status::OK);
        bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
    } else if (field.type == core::SerialFieldType::BYTE_ARRAY) {
        number(bytes, field.bytes ? static_cast<std::int64_t>(field.bytes->size()) : -1);

        if (field.bytes) {
            bytes.insert(bytes.end(), field.bytes->begin(), field.bytes->end());
        }
    } else {
        number(bytes, field.raw);
    }
}

/** Compose peer readiness then complete source descriptions from the explicit schema.
 * @param schema Ordinary local schema, used solely for deterministic peer source data.
 * @return Finite owning protocol/description prefix.
 */
std::vector<std::byte> initial(const core::LocalSchema &schema) {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(body, 0);
    number(body, 1);
    number(body, 15);
    text(body, u"STREAM_DATA");
    number(body, 0);
    number(body, 2);

    for (const auto &[id, name] :
         {std::pair{16, u"STREAM_ADD_SUBSCRIPTION"}, std::pair{17, u"STREAM_REMOVE_SUBSCRIPTION"}}) {
        number(body, id);
        text(body, name);
        number(body, 0);
    }

    auto result = frame(1, body);
    body.clear();
    const auto &record = schema.records()[0];
    number(body, 17);
    text(body, record.name);
    number(body, static_cast<std::int64_t>(record.fields.size()));

    for (std::size_t index = 0; index < record.fields.size(); ++index) {
        const auto lookup = std::ranges::find_if(record.lookups, [index](const auto &item) {
            return item.fieldId == index;
        });
        text(body, lookup->name);
        number(body, record.fields[index].serialTypeId);
    }

    const auto described = frame(2, body);
    result.insert(result.end(), described.begin(), described.end());

    return result;
}

/** Compose one complete STREAM data frame containing exact selected and unselected tuples.
 * @param records Complete explicit test values.
 * @return Finite owning data frame.
 */
std::vector<std::byte> data(std::span<const Record> records) {
    std::vector<std::byte> body;

    for (const auto &record : records) {
        body.push_back(std::byte{0xfd});
        number(body, static_cast<std::int64_t>(record.symbol.size()));

        for (const auto unit : record.symbol) {
            character(body, unit);
        }

        number(body, 17);

        for (const auto &value : record.fields) {
            field(body, value);
        }
    }

    return frame(15, body);
}

/** Decode the actual received exact subscription tuples, without treating composition as receipt.
 * @param bytes Complete frame payload after message identity.
 * @param expected Exact expected client symbol sequence.
 * @return True only for identical complete tuples with sender ID zero.
 */
bool exactSubscription(std::span<const std::byte> bytes, std::span<const std::u16string> expected) {
    e::SymbolReader symbols({8192, 128, 128}, 0);
    std::size_t index = 0;

    while (!bytes.empty()) {
        auto symbol = symbols.read(bytes);

        if (symbol.status != e::SymbolStatus::OK || index == expected.size() ||
            symbol.value->units != expected[index]) {
            return false;
        }

        bytes = bytes.subspan(symbol.consumed);
        const auto id = e::readCompactInt(bytes);

        if (id.status != e::Status::OK || id.value != 0) {
            return false;
        }

        bytes = bytes.subspan(id.consumed);
        ++index;
    }

    return index == expected.size();
}
} // namespace

Peer::Peer(const core::LocalSchema &schema, std::span<const Record> records, std::span<const std::u16string> expected,
           std::size_t chunk, PeerMode mode)
    : initial_(initial(schema)), data_(data(records)), expected_(expected.begin(), expected.end()), chunk_(chunk),
      mode_(mode) {
    require(chunk != 0);
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

                if (count == 0) {
                    break;
                }

                if (count > 0) {
                    auto bytes = std::span(input).first(static_cast<std::size_t>(count));

                    while (!bytes.empty()) {
                        auto next = reader.feed(bytes);
                        require(next.status != p::FrameStatus::FAILED && next.accepted != 0);
                        bytes = bytes.subspan(next.accepted);

                        if (next.frame && (next.frame->type == 16 || next.frame->type == 17)) {
                            require(exactSubscription(next.frame->payload, expected_));

                            if (next.frame->type == 16) {
                                ++adds_;

                                if (mode_ != PeerMode::SILENT) {
                                    output.insert(output.end(), data_.begin(), data_.end());
                                }

                                if (mode_ == PeerMode::PARTIAL_END) {
                                    output.push_back(std::byte{0x81});
                                }

                                ending = mode_ == PeerMode::CLEAN_END || mode_ == PeerMode::PARTIAL_END;
                            } else {
                                ++removes_;
                                output.insert(output.end(), data_.begin(), data_.end());
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

bool Peer::failed() const noexcept {
    return failed_.load();
}
} // namespace org::ttldtor::qdcxx::connect_tests
