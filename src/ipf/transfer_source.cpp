// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "transfer_source.hpp"

#include <boost/url/parse.hpp>
#include <boost/url/url_view.hpp>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <new>

namespace org::ttldtor::qdcxx::ipf {
namespace {

/** Validate complete UTF-8 scalars without decoding or rewriting credential/path bytes.
 * @param value Finite borrowed bytes, bounded by the caller before scanning.
 * @return False for truncation, stray continuation, overlong, surrogate or out-of-range scalar.
 */
bool validUtf8(std::string_view value) noexcept {
    for (std::size_t index = 0; index < value.size();) {
        const auto lead = static_cast<unsigned char>(value[index++]);

        if (lead < 0x80) {
            continue;
        }

        const auto length = lead >= 0xC2 && lead <= 0xDF   ? 1U
                            : lead >= 0xE0 && lead <= 0xEF ? 2U
                            : lead >= 0xF0 && lead <= 0xF4 ? 3U
                                                           : 0U;

        if (length == 0 || length > value.size() - index) {
            return false;
        }

        const auto first = static_cast<unsigned char>(value[index]);

        if ((lead == 0xE0 && first < 0xA0) || (lead == 0xED && first >= 0xA0) || (lead == 0xF0 && first < 0x90) ||
            (lead == 0xF4 && first >= 0x90)) {
            return false;
        }

        for (unsigned suffix = 0; suffix < length; ++suffix) {
            const auto byte = static_cast<unsigned char>(value[index++]);

            if (byte < 0x80 || byte > 0xBF) {
                return false;
            }
        }
    }

    return true;
}

/** Check the ASCII HTTP token alphabet.
 * @param c One unsigned byte.
 * @return True for an RFC token byte.
 */
bool tokenByte(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           std::string_view("!#$%&'*+-.^_"
                            "|~")
                   .find(static_cast<char>(c)) != std::string_view::npos;
}

/** Admit a nonempty credential fragment without control bytes.
 * @param value Borrowed credential bytes.
 * @return True for field-value bytes without control/DEL.
 */
bool credentialBytes(std::string_view value) noexcept {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char c) {
        return c >= 32 && c != 127;
    });
}

/** Check scheme-specific token68 syntax for Basic/Bearer while leaving custom field values opaque.
 * @param scheme Checked ASCII token scheme.
 * @param value Borrowed noncontrol credential bytes.
 * @return True for the selected scheme's supported field syntax.
 */
bool validSchemeCredential(std::string_view scheme, std::string_view value) noexcept {
    const auto equals = [&](std::string_view expected) {
        return scheme.size() == expected.size() &&
               std::equal(scheme.begin(), scheme.end(), expected.begin(), [](unsigned char a, unsigned char b) {
                   return (a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a) == b;
               });
    };
    const auto basic = equals("basic");

    if (!basic && !equals("bearer")) {
        return credentialBytes(value);
    }

    std::size_t padding = 0;
    std::size_t symbols = 0;

    for (const auto c : value) {
        if (c == '=') {
            ++padding;

            continue;
        }

        const auto alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');

        if (padding != 0 ||
            (!alnum && c != '+' && c != '/' && (basic || std::string_view("-._~").find(c) == std::string_view::npos))) {
            return false;
        }

        ++symbols;
    }

    constexpr std::size_t BASE64_QUANTUM = 4;
    constexpr std::size_t BASE64_PADDING_MAX = 2;

    return symbols != 0 && (!basic || (value.size() % BASE64_QUANTUM == 0 && padding <= BASE64_PADDING_MAX));
}

} // namespace

HttpAuthorization::HttpAuthorization(std::string value) : value_(std::move(value)) {
}

std::expected<HttpAuthorization, HttpAuthorizationError>
HttpAuthorization::custom(std::string_view scheme, std::string_view value, std::size_t maximum) {
    if (scheme.empty() || scheme.size() >= maximum || value.size() > maximum - scheme.size() - 1 ||
        !std::ranges::all_of(scheme, tokenByte) || !validSchemeCredential(scheme, value)) {
        return std::unexpected(HttpAuthorizationError::INVALID);
    }

    try {
        std::string result;
        result.reserve(scheme.size() + 1 + value.size());
        result.append(scheme);
        result.push_back(' ');
        result.append(value);

        return HttpAuthorization(std::move(result));
    } catch (const std::bad_alloc &) {
        return std::unexpected(HttpAuthorizationError::ALLOCATION_FAILURE);
    }
}

std::expected<HttpAuthorization, HttpAuthorizationError> HttpAuthorization::bearer(std::string_view token,
                                                                                   std::size_t maximum) {
    return custom("Bearer", token, maximum);
}

std::expected<HttpAuthorization, HttpAuthorizationError>
HttpAuthorization::basic(std::string_view user, std::string_view password, std::size_t maximum) {
    constexpr std::string_view PREFIX = "Basic ";
    constexpr std::string_view ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    if (maximum < PREFIX.size() || user.size() > maximum || password.size() > maximum ||
        user.size() > maximum - password.size() ||
        user.size() + password.size() == (std::numeric_limits<std::size_t>::max)()) {
        return std::unexpected(HttpAuthorizationError::INVALID);
    }

    const auto inputSize = user.size() + password.size() + 1;

    if (inputSize > (maximum - PREFIX.size()) / 4 * 3) {
        return std::unexpected(HttpAuthorizationError::INVALID);
    }

    const auto encodedSize = ((inputSize + 2) / 3) * 4;

    if (encodedSize > maximum - PREFIX.size() || user.find(':') != std::string_view::npos || !validUtf8(user) ||
        !validUtf8(password)) {
        return std::unexpected(HttpAuthorizationError::INVALID);
    }

    try {
        std::string result(PREFIX);
        result.reserve(PREFIX.size() + encodedSize);

        const auto byte = [&](std::size_t index) -> unsigned char {
            if (index < user.size()) {
                return static_cast<unsigned char>(user[index]);
            }

            return index == user.size() ? static_cast<unsigned char>(':')
                                        : static_cast<unsigned char>(password[index - user.size() - 1]);
        };

        for (std::size_t i = 0; i < inputSize; i += 3) {
            const auto a = byte(i);
            const auto b = i + 1 < inputSize ? byte(i + 1) : 0;
            const auto c = i + 2 < inputSize ? byte(i + 2) : 0;
            result.push_back(ALPHABET[a >> 2]);
            result.push_back(ALPHABET[((a & 3) << 4) | (b >> 4)]);
            result.push_back(i + 1 < inputSize ? ALPHABET[((b & 15) << 2) | (c >> 6)] : '=');
            result.push_back(i + 2 < inputSize ? ALPHABET[c & 63] : '=');
        }

        return HttpAuthorization(std::move(result));
    } catch (const std::bad_alloc &) {
        return std::unexpected(HttpAuthorizationError::ALLOCATION_FAILURE);
    }
}

std::string_view HttpAuthorization::value() const noexcept {
    return value_;
}

namespace detail {
namespace {

/** Adapt a checked Boost borrowed byte view.
 * @param value Borrowed Boost view.
 * @return Equivalent nonowning standard view.
 */
std::string_view view(boost::core::string_view value) noexcept {
    return {value.data(), value.size()};
}

/** Compare ASCII text case-insensitively without allocation.
 * @param a First byte string.
 * @param b Second byte string.
 * @return True for ASCII case-insensitive equality.
 */
bool equalAscii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto lower = [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        };

        if (lower(static_cast<unsigned char>(a[i])) != lower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

/** Fixed append-only address builder; no dynamic fallback.
 */
struct AddressBuilder {
    std::array<char, SOURCE_CAPACITY> bytes{}; ///< Whole fixed encoded-address window.
    std::size_t size = 0;                      ///< Initialized prefix.
    std::size_t maximum;                       ///< Validated logical capacity, at most the fixed window.

    /** Append a whole eligible fragment.
     * @param part Borrowed encoded fragment.
     * @return False without mutation when the full fragment cannot fit.
     */
    bool append(std::string_view part) noexcept {
        if (size > maximum || part.size() > maximum - size || part.size() > bytes.size() - size) {
            return false;
        }

        if (!part.empty()) {
            std::memcpy(bytes.data() + size, part.data(), part.size());
        }

        size += part.size();

        return true;
    }

    /** Borrow initialized builder bytes.
     * @return View valid until builder mutation/retirement.
     */
    std::string_view text() const noexcept {
        return {bytes.data(), size};
    }
};

/** Make a static URL failure.
 * @param stage Static checkpoint.
 * @return Detached invalid-source diagnostic.
 */
IpfTransferDiagnostic badSource(std::string_view stage) noexcept {
    return {IpfTransferError::INVALID_SOURCE, stage};
}

/** Parse a positive finite numeric TCP port.
 * @param text Borrowed decimal bytes.
 * @return Numeric port, empty outside1..65535 or for nondecimal input.
 */
std::optional<std::uint16_t> parsePort(std::string_view text) noexcept {
    unsigned result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);

    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result == 0 ||
        result > 65535) {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(result);
}

/** Construct a normalized HTTP(S) address without copying userinfo into address backing.
 * @param parsed Borrowed validated URL.
 * @param limits Finite source/secret caps.
 * @param extractUserinfo Whether to create an independent Basic value.
 * @return Owning canonical source or detached refusal.
 */
std::expected<TransferSource, IpfTransferDiagnostic>
networkSource(const boost::urls::url_view &parsed, const IpfTransferLimits &limits, bool extractUserinfo) {
    const auto scheme = view(parsed.scheme());
    const auto secure = equalAscii(scheme, "https");

    if (!secure && !equalAscii(scheme, "http")) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::UNSUPPORTED_SOURCE, "source-scheme"});
    }

    if (!parsed.has_authority() || parsed.encoded_host().empty() ||
        parsed.encoded_host().find('%') != boost::core::string_view::npos) {
        return std::unexpected(badSource("source-host"));
    }

    TransferSource result;
    result.secure = secure;
    result.port = secure ? 443 : 80;
    std::string numeric;
    std::string_view host = view(parsed.encoded_host());
    const auto ipv6 = parsed.host_type() == boost::urls::host_type::ipv6;

    if (parsed.host_type() == boost::urls::host_type::ipv4) {
        numeric = parsed.host_ipv4_address().to_string();
        host = numeric;
    } else if (ipv6) {
        numeric = parsed.host_ipv6_address().to_string();
        host = numeric;
    } else if (parsed.host_type() != boost::urls::host_type::name) {
        return std::unexpected(badSource("source-host-kind"));
    }

    if (!ipv6 && !host.empty() && host.back() == '.') {
        host.remove_suffix(1);
    }

    if (host.empty() || host.size() >= result.host.size()) {
        return std::unexpected(badSource("source-host-size"));
    }

    for (const auto c : host) {
        const auto byte = static_cast<unsigned char>(c);

        if (byte > 127 || byte <= 32 || byte == 127) {
            return std::unexpected(badSource("source-host-ascii"));
        }

        result.host[result.hostSize++] = c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    if (parsed.has_port()) {
        const auto port = parsePort(view(parsed.port()));

        if (!port) {
            return std::unexpected(badSource("source-port"));
        }

        result.port = *port;
    }

    if (extractUserinfo && parsed.has_userinfo() && !parsed.encoded_userinfo().empty()) {
        const auto userinfo = view(parsed.encoded_userinfo());
        const auto colon = userinfo.find(':');

        if (colon == std::string_view::npos) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::AUTHORIZATION_FAILURE, "userinfo-colon"});
        }

        auto token =
            HttpAuthorization::basic(userinfo.substr(0, colon), userinfo.substr(colon + 1), limits.authorizationBytes);

        if (!token) {
            return std::unexpected(IpfTransferDiagnostic{token.error() == HttpAuthorizationError::ALLOCATION_FAILURE
                                                             ? IpfTransferError::ALLOCATION_FAILURE
                                                             : IpfTransferError::AUTHORIZATION_FAILURE,
                                                         "userinfo-value"});
        }

        result.userinfo = std::move(*token);
    }

    AddressBuilder output{{}, 0, limits.sourceBytes};
    std::array<char, 6> portText{};
    const auto portEnd = std::to_chars(portText.data(), portText.data() + portText.size(), result.port);
    const auto portView = std::string_view(portText.data(), static_cast<std::size_t>(portEnd.ptr - portText.data()));
    const auto target = view(parsed.encoded_target());
    const auto path = view(parsed.encoded_path());
    const auto defaultPort = secure ? 443 : 80;

    if (!output.append(secure ? "https://" : "http://") || (ipv6 && !output.append("[")) ||
        !output.append(result.hostname()) || (ipv6 && !output.append("]")) ||
        (result.port != defaultPort && (!output.append(":") || !output.append(portView))) ||
        (path.empty() && !output.append("/")) || !output.append(target)) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "source-capacity"});
    }

    result.bytes = output.bytes;
    result.size = output.size;

    if (!boost::urls::parse_uri(result.address())) {
        return std::unexpected(badSource("source-revalidation"));
    }

    return result;
}

/** Separate literal UTF-8 initial userinfo before strict sanitized URI validation.
 * Only this credential component permits non-ASCII bytes; no IRI path/host conversion is performed.
 * @param source Borrowed complete initial text; never copied with credentials.
 * @param limits Finite address/secret caps.
 * @param extractUserinfo False for an explicit overriding credential source.
 * @return Sanitized owned source and independent Basic value, or static refusal.
 */
std::expected<TransferSource, IpfTransferDiagnostic>
extendedUserinfoSource(std::string_view source, const IpfTransferLimits &limits, bool extractUserinfo) {
    const auto colon = source.find(':');

    if (colon == std::string_view::npos || source.substr(colon, 3) != "://" ||
        (!equalAscii(source.substr(0, colon), "http") && !equalAscii(source.substr(0, colon), "https"))) {
        return std::unexpected(badSource("source-syntax"));
    }

    const auto begin = colon + 3;
    const auto delimiter = source.find_first_of("/?#", begin);
    const auto end = delimiter == std::string_view::npos ? source.size() : delimiter;
    const auto authority = source.substr(begin, end - begin);
    const auto at = authority.find('@');

    if (at == std::string_view::npos || at != authority.rfind('@')) {
        return std::unexpected(badSource("source-userinfo-syntax"));
    }

    const auto userinfo = authority.substr(0, at);
    bool extended = false;

    for (std::size_t index = 0; index < userinfo.size(); ++index) {
        const auto byte = static_cast<unsigned char>(userinfo[index]);

        if (byte >= 128) {
            extended = true;

            continue;
        }

        const auto alnum = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9');

        if (alnum || std::string_view("!$&'()*+,;=:-._~").find(static_cast<char>(byte)) != std::string_view::npos) {
            continue;
        }

        if (byte == '%' && index + 2 < userinfo.size()) {
            const auto hex = [](unsigned char c) {
                return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
            };

            if (hex(static_cast<unsigned char>(userinfo[index + 1])) &&
                hex(static_cast<unsigned char>(userinfo[index + 2]))) {
                index += 2;

                continue;
            }
        }

        return std::unexpected(badSource("source-userinfo-bytes"));
    }

    if (!extended) {
        return std::unexpected(badSource("source-syntax"));
    }

    AddressBuilder sanitized{{}, 0, limits.sourceBytes};

    if (!sanitized.append(source.substr(0, begin)) || !sanitized.append(authority.substr(at + 1)) ||
        !sanitized.append(source.substr(end))) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "source-capacity"});
    }

    const auto parsed = boost::urls::parse_uri(sanitized.text());

    if (!parsed) {
        return std::unexpected(badSource("source-sanitized-syntax"));
    }

    auto result = networkSource(*parsed, limits, false);

    if (result && extractUserinfo) {
        const auto split = userinfo.find(':');

        if (split == std::string_view::npos) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::AUTHORIZATION_FAILURE, "userinfo-colon"});
        }

        auto value =
            HttpAuthorization::basic(userinfo.substr(0, split), userinfo.substr(split + 1), limits.authorizationBytes);

        if (!value) {
            return std::unexpected(IpfTransferDiagnostic{value.error() == HttpAuthorizationError::ALLOCATION_FAILURE
                                                             ? IpfTransferError::ALLOCATION_FAILURE
                                                             : IpfTransferError::AUTHORIZATION_FAILURE,
                                                         "userinfo-value"});
        }

        result->userinfo = std::move(*value);
    }

    return result;
}

/** Remove one final encoded path segment from fixed output.
 * @param output Fixed path under construction.
 */
void removeSegment(AddressBuilder &output) noexcept {
    while (output.size != 0 && output.bytes[output.size - 1] != '/') {
        --output.size;
    }

    if (output.size != 0) {
        --output.size;
    }
}

/** Apply RFC3986 dot-segment removal to literal encoded path components.
 * @param path Borrowed encoded path, never percent decoded.
 * @param maximum Finite output capacity.
 * @return Checked fixed builder, or empty on capacity refusal.
 */
std::optional<AddressBuilder> removeDots(std::string_view path, std::size_t maximum) noexcept {
    AddressBuilder output{{}, 0, maximum};

    while (!path.empty()) {
        if (path.starts_with("../")) {
            path.remove_prefix(3);
        } else if (path.starts_with("./")) {
            path.remove_prefix(2);
        } else if (path.starts_with("/./")) {
            path.remove_prefix(2);
        } else if (path == "/.") {
            path = "/";
        } else if (path.starts_with("/../")) {
            path.remove_prefix(3);
            removeSegment(output);
        } else if (path == "/..") {
            path = "/";
            removeSegment(output);
        } else if (path == "." || path == "..") {
            path = {};
        } else {
            const auto slash = path.find('/', path.front() == '/' ? 1 : 0);
            const auto count = slash == std::string_view::npos ? path.size() : slash;

            if (!output.append(path.substr(0, count))) {
                return std::nullopt;
            }

            path.remove_prefix(count);
        }
    }

    return output;
}

/** Decode a file URI path only, rejecting embedded NUL.
 * @param path Borrowed encoded file path.
 * @param maximum Finite decoded capacity.
 * @return Fixed decoded path or detached refusal.
 */
std::expected<TransferSource, IpfTransferDiagnostic> localSource(std::string_view path, std::size_t maximum) {
    TransferSource result;
    result.local = true;

    const auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }

        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }

        return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    };

    for (std::size_t i = 0; i < path.size(); ++i) {
        auto c = path[i];

        if (c == '%') {
            if (i + 2 >= path.size() || hex(path[i + 1]) < 0 || hex(path[i + 2]) < 0) {
                return std::unexpected(badSource("file-percent"));
            }

            c = static_cast<char>(hex(path[i + 1]) * 16 + hex(path[i + 2]));
            i += 2;
        }

        if (c == '\0' || result.size == maximum || result.size == result.bytes.size()) {
            return std::unexpected(badSource("file-path-capacity"));
        }

        result.bytes[result.size++] = c;
    }

#ifdef _WIN32

    if (result.size >= 3 && result.bytes[0] == '/' && result.bytes[2] == ':') {
        std::memmove(result.bytes.data(), result.bytes.data() + 1, --result.size);
    }

#endif

    if (result.size == 0 || !validUtf8(result.address())) {
        return std::unexpected(badSource("file-empty"));
    }

    return result;
}

} // namespace

std::string_view TransferSource::address() const noexcept {
    return {bytes.data(), size};
}

std::string_view TransferSource::hostname() const noexcept {
    return {host.data(), hostSize};
}

std::size_t transferSourceWorkingBytes() noexcept {
    // Current source, nested normalization result and overlapping by-value return destinations.
    constexpr std::size_t SOURCE_OWNER_SLOTS = 4;
    // merged/path/normalized plus networkSource output; removeDots return overlap has the same bound.
    constexpr std::size_t ADDRESS_BUILDER_SLOTS = 4;
    // Parsed base/reference/revalidated result plus bounded component-view/control slack.
    constexpr std::size_t PARSED_VIEW_SLOTS = 8;
    // Owned Location size/capacity overlap on the qualified standard-library profiles.
    constexpr std::size_t LOCATION_STORAGE_PASSES = 2;

    return SOURCE_OWNER_SLOTS * sizeof(std::expected<TransferSource, IpfTransferDiagnostic>) +
           ADDRESS_BUILDER_SLOTS * sizeof(AddressBuilder) + PARSED_VIEW_SLOTS * sizeof(boost::urls::url_view) +
           LOCATION_STORAGE_PASSES * (SOURCE_CAPACITY + 1);
}

std::expected<TransferSource, IpfTransferDiagnostic>
parseTransferSource(std::string_view source, const IpfTransferLimits &limits, bool extractUserinfo) {
    if (source.empty() || source.size() > limits.sourceBytes || limits.sourceBytes > SOURCE_CAPACITY ||
        source.find('\0') != std::string_view::npos || !validUtf8(source)) {
        return std::unexpected(badSource("source-size"));
    }

    const auto colon = source.find(':');
    const auto separator = source.find_first_of("/\\");

    if (colon == std::string_view::npos || (separator != std::string_view::npos && separator < colon)) {
        TransferSource result;
        result.local = true;
        result.size = source.size();
        std::memcpy(result.bytes.data(), source.data(), source.size());

        return result;
    }

    if (source.find('/') == std::string_view::npos && colon > 0) {
        const auto query = source.find('?');
        const auto port =
            parsePort(source.substr(colon + 1, query == std::string_view::npos ? query : query - colon - 1));

        if (port) {
            AddressBuilder expanded{{}, 0, limits.sourceBytes};

            if (!expanded.append("http://") || !expanded.append(source.substr(0, query)) ||
                !expanded.append("/ipf/all.ipf.gz") ||
                (query != std::string_view::npos && !expanded.append(source.substr(query)))) {
                return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "shortcut-capacity"});
            }

            const auto parsed = boost::urls::parse_uri(expanded.text());

            return parsed ? networkSource(*parsed, limits, extractUserinfo)
                          : std::unexpected(badSource("shortcut-syntax"));
        }
    }

#ifdef _WIN32

    // The reference's Windows drive adjustment follows its host:port shortcut classification.
    if (source.size() > 2 && colon == 1 &&
        ((source[0] >= 'A' && source[0] <= 'Z') || (source[0] >= 'a' && source[0] <= 'z'))) {
        TransferSource result;
        result.local = true;
        result.size = source.size();
        std::memcpy(result.bytes.data(), source.data(), source.size());

        return result;
    }

#endif
    const auto parsed = boost::urls::parse_uri(source);

    if (!parsed) {
        return extendedUserinfoSource(source, limits, extractUserinfo);
    }

    if (equalAscii(view(parsed->scheme()), "file")) {
        if (parsed->has_userinfo() || parsed->has_port() || parsed->has_query() || parsed->has_fragment() ||
            (parsed->has_authority() && !parsed->encoded_host().empty() &&
             !equalAscii(view(parsed->encoded_host()), "localhost"))) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::UNSUPPORTED_SOURCE, "file-authority"});
        }

        return localSource(view(parsed->encoded_path()), limits.sourceBytes);
    }

    return networkSource(*parsed, limits, extractUserinfo);
}

std::expected<TransferSource, IpfTransferDiagnostic>
resolveTransferSource(const TransferSource &base, std::string_view location, const IpfTransferLimits &limits) {
    if (base.local || location.size() > limits.locationBytes || location.size() > SOURCE_CAPACITY ||
        location.find('\0') != std::string_view::npos) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::REDIRECT_REJECTED, "redirect-size"});
    }

    const auto reference = boost::urls::parse_uri_reference(location);
    const auto parsedBase = boost::urls::parse_uri(base.address());

    if (!reference || !parsedBase || reference->has_userinfo()) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::REDIRECT_REJECTED, "redirect-syntax"});
    }

    AddressBuilder merged{{}, 0, limits.sourceBytes};
    AddressBuilder path{{}, 0, limits.sourceBytes};
    const auto hasScheme = reference->has_scheme();
    const auto hasAuthority = reference->has_authority();
    const auto scheme = hasScheme ? view(reference->scheme()) : view(parsedBase->scheme());
    const auto authority =
        hasScheme || hasAuthority ? view(reference->encoded_authority()) : view(parsedBase->encoded_authority());
    auto refPath = view(reference->encoded_path());
    auto query = view(reference->encoded_query());
    auto hasQuery = reference->has_query();

    if (hasScheme || hasAuthority) {
        if (!path.append(refPath)) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "redirect-path"});
        }
    } else if (refPath.empty()) {
        if (!path.append(view(parsedBase->encoded_path()))) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "redirect-path"});
        }

        if (!hasQuery) {
            query = view(parsedBase->encoded_query());
            hasQuery = parsedBase->has_query();
        }
    } else {
        if (!refPath.starts_with("/")) {
            const auto oldPath = view(parsedBase->encoded_path());
            const auto slash = oldPath.rfind('/');

            if (!path.append(slash == std::string_view::npos ? std::string_view("/") : oldPath.substr(0, slash + 1))) {
                return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "redirect-merge"});
            }
        }

        if (!path.append(refPath)) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "redirect-merge"});
        }
    }

    const auto normalized = removeDots(path.text(), limits.sourceBytes);

    if (!normalized || !merged.append(scheme) || !merged.append("://") || !merged.append(authority) ||
        !merged.append(normalized->text()) || (hasQuery && (!merged.append("?") || !merged.append(query)))) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::RESOURCE_LIMIT, "redirect-capacity"});
    }

    const auto validated = boost::urls::parse_uri(merged.text());

    if (!validated) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::REDIRECT_REJECTED, "redirect-revalidation"});
    }

    auto result = networkSource(*validated, limits, false);

    if (!result || (base.secure && !result->secure)) {
        return std::unexpected(IpfTransferDiagnostic{IpfTransferError::REDIRECT_REJECTED, "redirect-policy"});
    }

    return result;
}

bool sameOrigin(const TransferSource &left, const TransferSource &right) noexcept {
    return !left.local && !right.local && left.secure == right.secure && left.port == right.port &&
           left.hostname() == right.hostname();
}

bool validAuthorization(std::string_view value, std::size_t maximum) noexcept {
    if (value.size() > maximum) {
        return false;
    }

    const auto space = value.find(' ');

    return space != std::string_view::npos && space != 0 && std::ranges::all_of(value.substr(0, space), tokenByte) &&
           validSchemeCredential(value.substr(0, space), value.substr(space + 1));
}

/** Parse one fixed UTC date without allocation.
 * @param value Exact IMF-fixdate bytes.
 * @return Validated UTC instant or empty.
 */
std::optional<std::chrono::sys_seconds> parseImfDate(std::string_view value) noexcept {
    constexpr std::array<std::string_view, 12> MONTHS{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    constexpr std::array<std::string_view, 7> DAYS{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    if (value.size() != 29 || value.substr(3, 2) != ", " || value[7] != ' ' || value[11] != ' ' || value[16] != ' ' ||
        value[19] != ':' || value[22] != ':' || value.substr(25) != " GMT" ||
        std::ranges::find(DAYS, value.substr(0, 3)) == DAYS.end()) {
        return std::nullopt;
    }

    const auto number = [&](std::size_t offset, std::size_t count) {
        unsigned result = 0;
        const auto parsed = std::from_chars(value.data() + offset, value.data() + offset + count, result);

        return parsed.ec == std::errc{} && parsed.ptr == value.data() + offset + count ? result : 100000U;
    };
    const auto month = std::ranges::find(MONTHS, value.substr(8, 3));
    const auto year = number(12, 4);
    const auto hour = number(17, 2);
    const auto minute = number(20, 2);
    const auto second = number(23, 2);

    if (month == MONTHS.end() || year < 1601 || year > 9999 || hour > 23 || minute > 59 || second > 60) {
        return std::nullopt;
    }

    const std::chrono::year_month_day date{std::chrono::year(static_cast<int>(year)),
                                           std::chrono::month(static_cast<unsigned>(month - MONTHS.begin() + 1)),
                                           std::chrono::day(number(5, 2))};

    if (!date.ok()) {
        return std::nullopt;
    }

    return std::chrono::sys_days(date) + std::chrono::hours(hour) + std::chrono::minutes(minute) +
           std::chrono::seconds(second);
}

/** Normalize the three HTTP-date wire forms into the same bounded numeric parser.
 * @param value Borrowed HTTP date field, bounded by its message header.
 * @return UTC timestamp, empty for invalid syntax/calendar.
 */
std::optional<std::chrono::sys_seconds> parseHttpDate(std::string_view value) noexcept {
    if (value.size() == 29 && value[3] == ',') {
        return parseImfDate(value);
    }

    constexpr std::string_view PATTERN = "Sun, 00 Jan 0000 00:00:00 GMT";
    std::array<char, 29> normalized{};
    std::copy(PATTERN.begin(), PATTERN.end(), normalized.begin());

    if (value.size() == 24 && value[3] == ' ' && value[7] == ' ' && value[10] == ' ' && value[19] == ' ') {
        std::copy_n(value.data(), 3, normalized.data());
        std::copy_n(value.data() + 4, 3, normalized.data() + 8);
        normalized[5] = value[8] == ' ' ? '0' : value[8];
        normalized[6] = value[9];
        std::copy_n(value.data() + 20, 4, normalized.data() + 12);
        std::copy_n(value.data() + 11, 8, normalized.data() + 17);

        return parseImfDate({normalized.data(), normalized.size()});
    }

    const auto comma = value.find(", ");
    constexpr std::array<std::string_view, 7> DAYS{"Sunday",   "Monday", "Tuesday", "Wednesday",
                                                   "Thursday", "Friday", "Saturday"};

    if (comma == std::string_view::npos || std::ranges::find(DAYS, value.substr(0, comma)) == DAYS.end()) {
        return std::nullopt;
    }

    const auto rest = value.substr(comma + 2);

    if (rest.size() != 22 || rest[2] != '-' || rest[6] != '-' || rest[9] != ' ' || rest.substr(18) != " GMT" ||
        rest[7] < '0' || rest[7] > '9' || rest[8] < '0' || rest[8] > '9') {
        return std::nullopt;
    }

    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const auto today = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day current{today};
    const auto currentYear = static_cast<int>(current.year());
    const auto shortYear = (rest[7] - '0') * 10 + rest[8] - '0';
    auto year = currentYear / 100 * 100 + shortYear;
    std::copy_n(value.data(), 3, normalized.data());
    std::copy_n(rest.data(), 2, normalized.data() + 5);
    std::copy_n(rest.data() + 3, 3, normalized.data() + 8);
    std::copy_n(rest.data() + 10, 8, normalized.data() + 17);

    const auto installYear = [&] {
        auto remaining = year;

        for (unsigned index = 0; index < 4; ++index) {
            normalized[15 - index] = static_cast<char>('0' + remaining % 10);
            remaining /= 10;
        }
    };
    installYear();
    auto parsed = parseImfDate({normalized.data(), normalized.size()});
    const auto future = std::chrono::sys_days(current + std::chrono::years(50)) + (now - today);

    if (parsed && *parsed > future) {
        year -= 100;
        installYear();
        parsed = parseImfDate({normalized.data(), normalized.size()});
    }

    return parsed;
}

bool httpDateRepresentable(std::chrono::sys_seconds value) noexcept {
    constexpr auto EARLIEST = std::chrono::sys_days(std::chrono::year(1601) / 1 / 1);
    constexpr auto LATEST = std::chrono::sys_days(std::chrono::year(9999) / 12 / 31) + std::chrono::days(1);

    return value >= EARLIEST && value < LATEST;
}

std::string formatHttpDate(std::chrono::sys_seconds value) {
    if (!httpDateRepresentable(value)) {
        return {};
    }

    constexpr std::array<std::string_view, 12> MONTHS{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    constexpr std::array<std::string_view, 7> DAYS{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const auto days = std::chrono::floor<std::chrono::days>(value);
    const std::chrono::year_month_day date{days};
    const std::chrono::hh_mm_ss time{value - days};
    const auto year = static_cast<int>(date.year());

    if (year < 1601 || year > 9999) {
        return {};
    }

    std::string result = "Sun, 00 Jan 0000 00:00:00 GMT";
    result.replace(0, 3, DAYS[std::chrono::weekday(days).c_encoding()]);
    result.replace(8, 3, MONTHS[static_cast<unsigned>(date.month()) - 1]);

    const auto digits = [&](std::size_t at, unsigned number, unsigned width) {
        for (unsigned i = 0; i < width; ++i) {
            result[at + width - i - 1] = static_cast<char>('0' + number % 10);
            number /= 10;
        }
    };
    digits(5, static_cast<unsigned>(date.day()), 2);
    digits(12, static_cast<unsigned>(year), 4);
    digits(17, static_cast<unsigned>(time.hours().count()), 2);
    digits(20, static_cast<unsigned>(time.minutes().count()), 2);
    digits(23, static_cast<unsigned>(time.seconds().count()), 2);

    return result;
}

} // namespace detail
} // namespace org::ttldtor::qdcxx::ipf
