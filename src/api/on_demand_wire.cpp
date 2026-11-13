// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "on_demand_wire.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <limits>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;
constexpr std::int64_t MAXIMUM_SECONDS = std::numeric_limits<std::int32_t>::max();
constexpr std::string_view BINARY_CONTENT_TYPE = "application/octet-stream";

constexpr Diagnostic INVALID_HTTP{ErrorCode::PROTOCOL_FAILURE, "ondemand-http-framing"};
constexpr Diagnostic INVALID_RESPONSE{ErrorCode::PROTOCOL_FAILURE, "ondemand-response"};
constexpr Diagnostic UNSUPPORTED_COMPRESSION{ErrorCode::UNSUPPORTED_PROFILE, "ondemand-compression"};

bool asciiLetter(char16_t value) noexcept {
    return (value >= u'A' && value <= u'Z') || (value >= u'a' && value <= u'z');
}

class Writer {
    std::span<std::byte> remaining_;

    public:
    explicit Writer(std::span<std::byte> bytes) : remaining_(bytes) {
    }

    bool number(std::int32_t value) {
        const auto result = encoding::writeCompactInt(value, remaining_);
        remaining_ = remaining_.subspan(result.written);

        return result.status == encoding::Status::OK;
    }

    bool character(std::int32_t value) {
        const auto result = encoding::writeUtfChar(value, remaining_);
        remaining_ = remaining_.subspan(result.written);

        return result.status == encoding::Status::OK;
    }

    bool text(std::u16string_view value) {
        const auto result = encoding::writeUtfString(value, remaining_, {128, 32});
        remaining_ = remaining_.subspan(result.written);

        return result.status == encoding::Status::OK;
    }

    bool append(std::span<const std::byte> bytes) {
        if (bytes.size() > remaining_.size())
            return false;

        std::ranges::copy(bytes, remaining_.begin());
        remaining_ = remaining_.subspan(bytes.size());

        return true;
    }

    bool blob(std::span<const std::byte> bytes) {
        return number(static_cast<std::int32_t>(bytes.size())) && append(bytes);
    }

    std::size_t available() const noexcept {
        return remaining_.size();
    }
};

class Reader {
    std::span<const std::byte> remaining_;

    template <class Decode, class T> bool take(Decode decode, T &value) noexcept {
        const auto result = decode(remaining_);

        if (result.status != encoding::Status::OK)
            return false;

        value = result.value;
        remaining_ = remaining_.subspan(result.consumed);

        return true;
    }

    public:
    explicit Reader(std::span<const std::byte> bytes) : remaining_(bytes) {
    }

    bool number(std::int64_t &value) noexcept {
        return take(encoding::readCompactLong, value);
    }

    bool character(std::int32_t &value) noexcept {
        return take(encoding::readUtfChar, value);
    }

    bool byte(unsigned &value) noexcept {
        if (remaining_.empty())
            return false;

        value = std::to_integer<unsigned>(remaining_.front());
        remaining_ = remaining_.subspan(1);

        return true;
    }

    bool text(std::u16string &value) {
        auto result = encoding::readUtfString(remaining_, {64, ON_DEMAND_MAXIMUM_SYMBOL});

        if (result.status != encoding::Status::OK || !result.value)
            return false;

        value = std::move(*result.value);
        remaining_ = remaining_.subspan(result.consumed);

        return true;
    }

    bool blob(std::span<const std::byte> &bytes) noexcept {
        std::int64_t size = 0;

        if (!number(size) || size < 0 || std::cmp_greater(size, remaining_.size()))
            return false;

        bytes = remaining_.first(static_cast<std::size_t>(size));
        remaining_ = remaining_.subspan(bytes.size());

        return true;
    }

    bool empty() const noexcept {
        return remaining_.empty();
    }
};

bool equalAscii(std::string_view left, std::string_view right) noexcept {
    const auto lower = [](char value) {
        return value >= 'A' && value <= 'Z' ? value - 'A' + 'a' : value;
    };

    return std::ranges::equal(left, right, [&](char a, char b) {
        return lower(a) == lower(b);
    });
}

bool headerName(char value) noexcept {
    return asciiLetter(static_cast<unsigned char>(value)) || (value >= '0' && value <= '9') ||
           std::string_view("!#$%&'*+-.^`_|~").contains(value);
}

std::string_view trimHeader(std::string_view text) noexcept {
    const auto first = text.find_first_not_of(" \t");

    if (first == std::string_view::npos)
        return {};

    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool readDelta(Reader &reader, std::int32_t &value) noexcept {
    std::int64_t delta = 0;

    if (!reader.number(delta) || !std::in_range<std::int32_t>(delta))
        return false;

    const auto next = static_cast<std::int64_t>(value) + delta;

    if (!std::in_range<std::int32_t>(next))
        return false;

    value = static_cast<std::int32_t>(next);

    return true;
}

} // namespace

bool validOnDemandRequest(const OnDemandRequest &request) noexcept {
    const auto time = request.from.time_since_epoch().count();

    return request.port != 0 && time >= MILLISECONDS_PER_SECOND && time / MILLISECONDS_PER_SECOND <= MAXIMUM_SECONDS &&
           !request.symbol.empty() && request.symbol.size() <= ON_DEMAND_MAXIMUM_SYMBOL &&
           std::ranges::any_of(request.symbol, asciiLetter) && std::ranges::all_of(request.symbol, [](char16_t value) {
               return asciiLetter(value) || (value >= u'0' && value <= u'9') || value == u'.';
           });
}

std::expected<OnDemandWireRequest, Diagnostic> encodeOnDemandRequest(const OnDemandRequest &request) {
    if (!validOnDemandRequest(request))
        return std::unexpected(Diagnostic{ErrorCode::INVALID_ARGUMENT, "ondemand-request"});

    using namespace std::chrono;
    constexpr Diagnostic LIMIT{ErrorCode::RESOURCE_LIMIT, "ondemand-request-bytes"};
    constexpr std::array<std::byte, 32> SYNTHETIC_DIGEST{};
    const auto day = floor<days>(request.from);
    const year_month_day date(day);
    const hh_mm_ss time(request.from - day);
    const auto epoch = std::to_string(request.from.time_since_epoch().count());
    const std::u16string epochText(epoch.begin(), epoch.end());

    std::array<std::byte, ON_DEMAND_MAXIMUM_SYMBOL + 5> keyBytes{};
    Writer key(keyBytes);

    if (!key.text(request.symbol) || !key.character(0) || !key.character('Q'))
        return std::unexpected(LIMIT);

    std::array<std::byte, ON_DEMAND_MAXIMUM_REQUEST> bodyBytes{};
    Writer body(bodyBytes);

    // A synthetic envelope for the local interoperability profile carries no actual access credential.
    if (!body.number(2) || !body.number(7) || !body.text(u"contract") || !body.text(u"") || !body.text(u"user") ||
        !body.text(u"") || !body.text(u"expiration") || !body.text(u"9223372036854775807") || !body.text(u"digest") ||
        !body.blob(SYNTHETIC_DIGEST) || !body.text(u"delay") || !body.text(u"0") || !body.text(u"time") ||
        !body.text(epochText) || !body.text(u"keys") ||
        !body.blob(std::span(keyBytes).first(keyBytes.size() - key.available())))
        return std::unexpected(LIMIT);

    const auto bodySize = bodyBytes.size() - body.available();
    std::array<std::byte, 5> prefix{};
    const auto sizePrefix = encoding::writeCompactInt(static_cast<std::int32_t>(bodySize), prefix);
    std::array<char, ON_DEMAND_MAXIMUM_REQUEST> header{};
    const auto headerSize = std::snprintf(
        header.data(), header.size(),
        "POST /MarketDataReplay HTTP/1.1\r\nHost: 127.0.0.1:%u\r\n"
        "Content-Type: application/octet-stream\r\nAccept: application/octet-stream\r\n"
        "X-OnDemand-StartTime: %04d-%02u-%02uT%02d:%02d:%02d.%03dZ\r\n"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n",
        static_cast<unsigned>(request.port), static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
        static_cast<unsigned>(date.day()), static_cast<int>(time.hours().count()),
        static_cast<int>(time.minutes().count()), static_cast<int>(time.seconds().count()),
        static_cast<int>(time.subseconds().count()), bodySize + sizePrefix.written);

    if (headerSize <= 0 || std::cmp_greater_equal(headerSize, header.size()) ||
        sizePrefix.status != encoding::Status::OK)
        return std::unexpected(LIMIT);

    OnDemandWireRequest result{};
    Writer wire(result.bytes);

    if (!wire.append(std::as_bytes(std::span(header).first(static_cast<std::size_t>(headerSize)))) ||
        !wire.append(std::span(prefix).first(sizePrefix.written)) || !wire.append(std::span(bodyBytes).first(bodySize)))
        return std::unexpected(LIMIT);

    result.size = result.bytes.size() - wire.available();

    return result;
}

std::expected<std::optional<std::span<const std::byte>>, Diagnostic>
readOnDemandHttp(std::span<const std::byte> bytes) {
    if (bytes.empty())
        return std::nullopt;

    const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    const auto end = text.find("\r\n\r\n");

    if (end == std::string_view::npos) {
        if (bytes.size() >= ON_DEMAND_MAXIMUM_HEADER)
            return std::unexpected(INVALID_HTTP);

        return std::nullopt;
    }

    const auto offset = end + 4;
    const auto statusEnd = text.find("\r\n");

    if (offset > ON_DEMAND_MAXIMUM_HEADER || statusEnd < 12 ||
        !(text.starts_with("HTTP/1.0 ") || text.starts_with("HTTP/1.1 ")) || (statusEnd > 12 && text[12] != ' ') ||
        !std::ranges::all_of(
            text.substr(12, statusEnd - 12),
            [](char c) {
                return c >= ' ' && c <= '~';
            }))
        return std::unexpected(INVALID_HTTP);

    const auto status = text.substr(9, 3);

    if (status == "401")
        return std::unexpected(Diagnostic{ErrorCode::UNSUPPORTED_PROFILE, "ondemand-access-refused", 0, 401});

    if (status != "200")
        return std::unexpected(INVALID_HTTP);

    std::optional<std::size_t> length;
    bool contentType = false;
    auto lineStart = statusEnd + 2;

    while (lineStart < end) {
        const auto lineEnd = text.find("\r\n", lineStart);
        const auto line = text.substr(lineStart, lineEnd - lineStart);
        const auto separator = line.find(':');

        if (separator == 0 || separator == std::string_view::npos ||
            !std::ranges::all_of(line.substr(0, separator), headerName) ||
            !std::ranges::all_of(line.substr(separator + 1), [](char c) {
                return c == '\t' || (c >= ' ' && c <= '~');
            }))
            return std::unexpected(INVALID_HTTP);

        const auto key = line.substr(0, separator);
        const auto value = trimHeader(line.substr(separator + 1));

        if (equalAscii(key, "Content-Length")) {
            if (value.empty())
                return std::unexpected(INVALID_HTTP);

            std::size_t size = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), size);

            if (length || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || size == 0 ||
                size > ON_DEMAND_MAXIMUM_BODY)
                return std::unexpected(INVALID_HTTP);

            length = size;
        } else if (equalAscii(key, "Content-Type")) {
            if (contentType || !equalAscii(value, BINARY_CONTENT_TYPE))
                return std::unexpected(INVALID_HTTP);

            contentType = true;
        } else if (equalAscii(key, "Transfer-Encoding") || equalAscii(key, "Content-Encoding")) {
            return std::unexpected(INVALID_HTTP);
        }

        lineStart = lineEnd + 2;
    }

    if (!length || !contentType || bytes.size() > offset + *length)
        return std::unexpected(INVALID_HTTP);

    if (bytes.size() < offset + *length)
        return std::nullopt;

    return std::optional{bytes.subspan(offset)};
}

std::expected<OnDemandResponse, Diagnostic> decodeOnDemandResponse(std::span<const std::byte> bytes,
                                                                   const OnDemandRequest &request) {
    if (!validOnDemandRequest(request) || bytes.size() > ON_DEMAND_MAXIMUM_BODY)
        return std::unexpected(INVALID_RESPONSE);

    Reader outer(bytes);
    std::span<const std::byte> payload;

    if (!outer.blob(payload) || !outer.empty())
        return std::unexpected(INVALID_RESPONSE);

    Reader message(payload);
    std::int64_t version = 0;
    std::int64_t entries = 0;
    std::u16string element;

    if (!message.number(version) || version != 2 || !message.number(entries) || entries != 1 || !message.text(element))
        return std::unexpected(INVALID_RESPONSE);

    if (element == u"blocks.gz")
        return std::unexpected(UNSUPPORTED_COMPRESSION);

    if (element != u"blocks" || !message.blob(payload) || !message.empty())
        return std::unexpected(INVALID_RESPONSE);

    Reader blocks(payload);

    if (!blocks.blob(payload) || !blocks.empty())
        return std::unexpected(INVALID_RESPONSE);

    Reader block(payload);
    std::u16string symbol;
    std::int32_t exchange = 0;
    std::int32_t type = 0;
    std::int64_t start = 0;
    std::int64_t duration = 0;

    if (!block.number(version))
        return std::unexpected(INVALID_RESPONSE);

    if (version != 0)
        return std::unexpected(UNSUPPORTED_COMPRESSION);

    if (!block.text(symbol) || symbol != request.symbol || !block.character(exchange) || exchange != 0 ||
        !block.character(type) || type != 'Q' || !block.number(start) || start <= 0 || start > MAXIMUM_SECONDS ||
        !block.number(duration) || duration <= 0 || duration > MAXIMUM_SECONDS - start)
        return std::unexpected(INVALID_RESPONSE);

    const auto from = request.from.time_since_epoch().count();
    const auto end = (start + duration) * MILLISECONDS_PER_SECOND;
    auto time = start * MILLISECONDS_PER_SECOND;

    if (from < time || from >= end || end - from > ON_DEMAND_MAXIMUM_WINDOW.count())
        return std::unexpected(INVALID_RESPONSE);

    const auto seconds = static_cast<std::int32_t>(start);
    std::array<std::int32_t, 8> fields{seconds, 0, 0, 0, seconds, 0, 0, 0};
    OnDemandResponse response;

    while (!block.empty()) {
        if (response.size == response.records.size())
            return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "ondemand-event-count"});

        std::int64_t delta = 0;
        unsigned flags = 0;

        if (!block.number(delta) || delta < 0 || delta >= end - time || !block.byte(flags))
            return std::unexpected(INVALID_RESPONSE);

        time += delta;

        for (std::size_t index = 0; index < fields.size(); ++index) {
            if ((flags & (0x80U >> index)) && !readDelta(block, fields[index]))
                return std::unexpected(INVALID_RESPONSE);
        }

        if (!std::in_range<std::uint16_t>(fields[1]) || !std::in_range<std::uint16_t>(fields[5]))
            return std::unexpected(INVALID_RESPONSE);

        response.records[response.size++] = {time, fields};
    }

    if (response.size == 0)
        return std::unexpected(INVALID_RESPONSE);

    return response;
}

} // namespace org::ttldtor::qdcxx::api::detail
