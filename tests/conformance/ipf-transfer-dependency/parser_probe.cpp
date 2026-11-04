// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include <boost/asio/buffer.hpp>
#include <boost/beast/http/basic_parser.hpp>
#include <boost/url/parse.hpp>
#include <boost/version.hpp>

#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace http = boost::beast::http;
using ErrorCode = boost::system::error_code;
using StringView = boost::beast::string_view;

/** A callback-only parser probe; all borrowed fields are inspected during callbacks. */
class Parser final : public http::basic_parser<false> {
    public:
    /// Last response status received during the callback.
    int status = 0;
    /// Number of body bytes accepted synchronously.
    std::size_t bodyBytes = 0;
    /// Number of header and trailer fields observed.
    std::size_t fields = 0;
    /// Whether the parser issued its completion callback.
    bool finished = false;

    private:
    void on_request_impl(http::verb, StringView, StringView, int, ErrorCode &) override {
    }

    void on_response_impl(int code, StringView, int, ErrorCode &) override {
        status = code;
    }

    void on_field_impl(http::field, StringView, StringView, ErrorCode &) override {
        ++fields;
    }

    void on_trailer_field_impl(http::field, StringView, StringView, ErrorCode &) override {
        ++fields;
    }

    void on_header_impl(ErrorCode &) override {
    }

    void on_body_init_impl(const boost::optional<std::uint64_t> &, ErrorCode &) override {
    }

    std::size_t on_body_impl(StringView body, ErrorCode &) override {
        bodyBytes += body.size();

        return body.size();
    }

    void on_chunk_header_impl(std::uint64_t, StringView, ErrorCode &) override {
    }

    std::size_t on_chunk_body_impl(std::uint64_t, StringView body, ErrorCode &) override {
        bodyBytes += body.size();

        return body.size();
    }

    void on_finish_impl(ErrorCode &) override {
        finished = true;
    }
};

/** Fail the probe when an externally observable condition is false.
 * @param condition Observed condition.
 * @param label Nonsecret fixed case identifier.
 */
void require(bool condition, const char *label) {
    if (!condition) {
        throw std::runtime_error(label);
    }
}

/** Feed exactly one contiguous caller-owned buffer.
 * @param parser Parser receiving callback-only borrows.
 * @param wire Buffer alive through this call.
 * @param error Receives the parser outcome.
 * @return Count consumed; unconsumed input remains caller-owned.
 */
std::size_t put(Parser &parser, std::string_view wire, ErrorCode &error) {
    return parser.put(boost::asio::const_buffer(wire.data(), wire.size()), error);
}

/** Exercise the selected pure parser and borrowed URI vocabulary. */
void runProbes() {
    static_assert(BOOST_VERSION == 109200);

    ErrorCode error;
    Parser success;
    success.eager(true);
    const std::string_view fixed = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabcNEXT";
    const auto consumed = put(success, fixed, error);
    require(!error && success.is_done() && success.finished && success.status == 200 && success.bodyBytes == 3 &&
                consumed == fixed.size() - 4,
            "content-length-and-unconsumed-suffix");

    Parser empty;
    put(empty, "HTTP/1.1 304 Not Modified\r\n\r\n", error);
    require(!error && empty.is_done() && empty.bodyBytes == 0 && empty.status == 304, "no-body-status");

    Parser chunked;
    chunked.eager(true);
    put(chunked, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\nX-End: yes\r\n\r\n", error);
    require(!error && chunked.is_done() && chunked.bodyBytes == 3 && chunked.fields == 2, "chunked-trailer");

    Parser bounded;
    bounded.body_limit(3);
    bounded.eager(true);
    put(bounded, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc", error);
    require(!error && bounded.is_done(), "body-limit-exact");

    Parser exceeded;
    exceeded.body_limit(3);
    put(exceeded, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n", error);
    require(error == http::error::body_limit, "body-limit-exceeded");

    Parser headerLimit;
    headerLimit.header_limit(16);
    put(headerLimit, "HTTP/1.1 200 OK\r\nLong: field\r\n\r\n", error);
    require(error == http::error::header_limit, "header-limit");

    Parser malformed;
    put(malformed, "HTTP/1.1 xyz Broken\r\n\r\n", error);
    require(static_cast<bool>(error), "invalid-status");

    Parser ambiguous;
    put(ambiguous, "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n", error);
    require(static_cast<bool>(error), "conflicting-content-length");

    Parser partial;
    partial.eager(true);
    put(partial, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\na", error);
    require(!error && !partial.is_done(), "partial-not-complete");
    partial.put_eof(error);
    require(error == http::error::partial_message, "truncated-eof");

    Parser split;
    const std::string_view fragment = "HTTP/1.1 200 O";
    require(put(split, fragment, error) == 0 && error == http::error::need_more, "partial-header-retained-by-caller");
    put(split, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", error);
    require(!error && split.is_done(), "caller-replays-unconsumed-header");

    std::string uri = "https://user:p%3Ass@example.test:443/a?opaque=%2F#fragment";
    const auto parsed = boost::urls::parse_uri(uri);
    require(parsed && parsed->buffer().data() == uri.data() && parsed->encoded_userinfo() == "user:p%3Ass" &&
                parsed->encoded_query() == "opaque=%2F" && parsed->port_number() == 443,
            "uri-borrows-input-preserves-encoding");
    require(!boost::urls::parse_uri("https://[broken/path"), "uri-invalid-host");
    require(!boost::urls::parse_uri("https://example.test/%GG"), "uri-invalid-percent");
    require(!boost::urls::parse_uri("relative/path"), "uri-absolute-required");
    require(boost::urls::parse_uri_reference("../next?x=1").has_value(), "relative-reference");
    require(boost::urls::parse_uri("other:opaque").has_value(),
            "generic-parser-does-not-enforce-transfer-scheme-policy");
    require(!boost::urls::parse_uri(std::string_view("https://host/\0tail", 18)), "embedded-nul-rejected");
}
} // namespace

/** @return Zero for all selected probes; one for a failed condition. */
int main() {
    try {
        runProbes();
        std::puts("IPF dependency probes passed: contiguous parser and borrowed URI; no networking");

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Probe failure: %s\n", error.what());

        return 1;
    }
}
