// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "endpoint_configuration.hpp"
#include "../client/endpoint_host.hpp"
#include "../detail/qd_time_period.hpp"
#include "../transport/aggregation.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::api {
namespace {
constexpr std::size_t MAXIMUM_HOST_BYTES =
    253; ///< Existing high-level route ceiling; lower APIs keep their own limits.
constexpr std::size_t MAXIMUM_ROUTE_BYTES =
    MAXIMUM_HOST_BYTES + 8;                      ///< Host, optional brackets, colon and decimal port.
constexpr std::size_t MAXIMUM_LABEL_BYTES = 128; ///< Local diagnostic label, never a connector/thread name alias.
constexpr unsigned char FIRST_ASCII = 0x20;
constexpr unsigned char LAST_ASCII = 0x7e;
constexpr std::array<std::string_view, 14> KEYS{"qdcxx.name",
                                                "dxfeed.wildcard.enable",
                                                "qdcxx.connectTimeoutMillis",
                                                "qdcxx.dnsTimeoutMillis",
                                                "qdcxx.readTimeoutMillis",
                                                "qdcxx.writeTimeoutMillis",
                                                "qdcxx.pollIntervalMillis",
                                                "qdcxx.retryDelayMillis",
                                                "qdcxx.drainTimeoutMillis",
                                                "qdcxx.maximumAttempts",
                                                "qdcxx.deliveryContract",
                                                "dxfeed.aggregationPeriod",
                                                "dxfeed.requestedAggregationPeriod",
                                                "dxfeed.address"};

enum Key : std::size_t {
    NAME,
    WILDCARD,
    CONNECT,
    DNS,
    READ,
    WRITE,
    POLL,
    RETRY,
    DRAIN,
    ATTEMPTS,
    CONTRACT,
    AGGREGATION,
    REQUESTED,
    ADDRESS
};

struct Selected {
    std::string_view value;
    EndpointPropertyOrigin origin = EndpointPropertyOrigin::BASE_OPTIONS;
    bool present = false;
};

using Selection = std::array<Selected, KEYS.size()>;

/** @param code Detached category. @param stage Static label; never append any caller input. */
[[noreturn]] void reject(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

/** @param value Explicit byte sequence. @return True for printable ASCII, including spaces. */
bool printable(std::string_view value) {
    return std::ranges::all_of(value, [](unsigned char c) {
        return c >= FIRST_ASCII && c <= LAST_ASCII;
    });
}

/** @param value Inline property cell. @return Borrowed ASCII-space-trimmed value. */
std::string_view trim(std::string_view value) {
    while (!value.empty() && value.front() == ' ')
        value.remove_prefix(1);

    while (!value.empty() && value.back() == ' ')
        value.remove_suffix(1);

    return value;
}

/** @param left Explicit ASCII input. @param right Canonical ASCII spelling. @return Case-insensitive equality. */
bool equalAscii(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
               const auto lower = [](char c) {
                   return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
               };

               return lower(a) == lower(b);
           });
}

/** @param key Exact endpoint key or verified inline connector alias. @param inlineProperty Connector context.
 * @return Supported canonical slot, otherwise throws without retaining the key.
 */
std::size_t keyIndex(std::string_view key, bool inlineProperty) {
    if (inlineProperty && equalAscii(key, "reconnectDelay"))
        return RETRY;

    if (inlineProperty && equalAscii(key, "requestedAggregationPeriod"))
        return REQUESTED;

    const auto found = std::ranges::find(KEYS, key);

    if (found == KEYS.end() ||
        (inlineProperty && (found - KEYS.begin() == ADDRESS || found - KEYS.begin() == WILDCARD ||
                            found - KEYS.begin() == CONTRACT || found - KEYS.begin() == REQUESTED))) {
        reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-property-unsupported");
    }

    return static_cast<std::size_t>(found - KEYS.begin());
}

/** One caller construction's finite scans; even shadowed entries consume their input envelope. */
struct InputEnvelope {
    EndpointConfigurationLimits limits;
    std::size_t entries = 0;
    std::size_t bytes = 0;

    /** @param property Borrowed entry. @param origin Explicit layer. @param inlineProperty Address-property context.
     * @param selected Fixed selected-value table; no raw values are copied before validation.
     */
    void select(EndpointProperty property, EndpointPropertyOrigin origin, bool inlineProperty, Selection &selected) {
        if (entries == limits.properties || property.key.size() > limits.keyBytes ||
            property.value.size() > limits.valueBytes || property.key.size() > limits.totalBytes - bytes)
            reject(ErrorCode::RESOURCE_LIMIT, "endpoint-property-input");

        ++entries;
        bytes += property.key.size();

        if (property.value.size() > limits.totalBytes - bytes)
            reject(ErrorCode::RESOURCE_LIMIT, "endpoint-property-input");

        bytes += property.value.size();

        if (property.key.empty() || !printable(property.key) || !printable(property.value))
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-property-text");

        const auto index = keyIndex(property.key, inlineProperty);
        selected[index] = {property.value, origin, true};
    }
};

/** @param text Complete unsigned decimal. @param maximum Exact native range. @param allowZero Zero applicability.
 * @return Validated integer; signs, whitespace and partial conversions reject.
 */
std::uint64_t integer(std::string_view text, std::uint64_t maximum, bool allowZero = false) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);

    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result > maximum ||
        (!allowZero && result == 0)) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-property-value");
    }

    return result;
}

/** @param value Explicit boolean text. @return Verified true/false; unlike Java, malformed boolean text rejects. */
bool boolean(std::string_view value) {
    if (equalAscii(value, "true"))
        return true;

    if (equalAscii(value, "false"))
        return false;

    reject(ErrorCode::INVALID_ARGUMENT, "endpoint-property-value");
}

/** @param address Complete selected construction address. @param input Finite scan ledger. @param selected Value table.
 * @return Plain route view; flat trailing square-bracket blocks preserve original QD property order.
 */
std::string_view addressProperties(std::string_view address, InputEnvelope &input, Selection &selected) {
    std::size_t offset = 0;

    if (!address.empty() && address.front() == '[') {
        const auto close = address.find(']');

        if (close == std::string_view::npos)
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

        offset = close + 1;
    }

    const auto first = address.find('[', offset);

    if (first == std::string_view::npos)
        return address;

    const auto route = address.substr(0, first);

    for (auto position = first; position < address.size();) {
        if (address[position] != '[')
            reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-address-extension");

        const auto close = address.find(']', position + 1);

        if (close == std::string_view::npos)
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address-properties");

        auto block = address.substr(position + 1, close - position - 1);

        if (block.find_first_of("[]()\\") != std::string_view::npos)
            reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-address-extension");

        for (;;) {
            const auto comma = block.find(',');
            const auto cell = trim(block.substr(0, comma));
            const auto equal = cell.find('=');

            if (equal == std::string_view::npos)
                reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address-properties");

            input.select({trim(cell.substr(0, equal)), trim(cell.substr(equal + 1))},
                         EndpointPropertyOrigin::ADDRESS_PROPERTIES, true, selected);

            if (comma == std::string_view::npos)
                break;

            block.remove_prefix(comma + 1);
        }

        position = close + 1;
    }

    return route;
}

/** @param options Resolved typed values. @param name Native diagnostic label. @return Canonical local policy values. */
std::array<std::string, ADDRESS> values(const EndpointOptions &options, const std::string &name) {
    return {name,
            options.enableWildcard ? "true" : "false",
            std::to_string(options.connectTimeout.count()),
            std::to_string(options.dnsTimeout.count()),
            std::to_string(options.readTimeout.count()),
            std::to_string(options.writeTimeout.count()),
            std::to_string(options.pollInterval.count()),
            std::to_string(options.retryDelay.count()),
            std::to_string(options.drainTimeout.count()),
            std::to_string(options.maximumAttempts),
            options.contract == DeliveryContract::STREAM    ? "stream"
            : options.contract == DeliveryContract::HISTORY ? "history"
                                                            : "ticker",
            detail::aggregationPropertyValue(options.aggregationPeriod),
            detail::aggregationPropertyValue(options.requestedAggregationPeriod)};
}
} // namespace

namespace detail {
std::string aggregationPropertyValue(std::optional<std::chrono::milliseconds> period) {
    const auto encoded = transport::detail::aggregationRequestText(period);
    std::string result;
    result.reserve(encoded.size);

    for (const auto unit : encoded.view())
        result.push_back(static_cast<char>(unit));

    return result;
}

void validateEndpointOptions(const EndpointOptions &options) {
    if (options.aggregationPeriod.count() < 0 || options.aggregationPeriod > MAXIMUM_LOCAL_AGGREGATION_PERIOD)
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-local-aggregation-period");

    if (options.requestedAggregationPeriod &&
        (options.requestedAggregationPeriod->count() < 0 ||
         *options.requestedAggregationPeriod > transport::MAXIMUM_REQUESTED_AGGREGATION_PERIOD))
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-aggregation-period");

    if (options.enableWildcard && options.contract != DeliveryContract::STREAM)
        reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-wildcard-contract");

    if (options.eventRegistry && !options.eventRegistry->valid())
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-registry");

    const auto durationValid = [](std::chrono::milliseconds value) {
        return value.count() > 0 && value.count() <= INT_MAX;
    };

    if ((options.contract != DeliveryContract::TICKER && options.contract != DeliveryContract::STREAM &&
         options.contract != DeliveryContract::HISTORY) ||
        !durationValid(options.connectTimeout) || !durationValid(options.dnsTimeout) ||
        !durationValid(options.readTimeout) || !durationValid(options.writeTimeout) ||
        !durationValid(options.pollInterval) || !durationValid(options.drainTimeout) || options.maximumAttempts == 0 ||
        (options.maximumAttempts > 1 && !durationValid(options.retryDelay)) || options.batchRecords == 0 ||
        options.batchRecords > EndpointOptions::MAXIMUM_BATCH_RECORDS || options.pendingRecords == 0 ||
        options.pendingRecords > EndpointOptions::MAXIMUM_PENDING_RECORDS ||
        options.batchRecords > options.pendingRecords) {
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-options");
    }
}

EndpointAddress parseEndpointAddress(std::string_view address) {
    if (address.empty() || address.size() > MAXIMUM_ROUTE_BYTES || !printable(address) ||
        address.find(' ') != std::string_view::npos)
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

    if (address.front() == ':' || address.find_first_of("+,/()@;?=#\\") != std::string_view::npos)
        reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-address-extension");

    const auto separator = address.rfind(':');

    if (separator == std::string_view::npos || separator == 0)
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

    auto host = address.substr(0, separator);
    const auto portText = address.substr(separator + 1);

    if (portText.find('[') != std::string_view::npos || address.back() == ']')
        reject(ErrorCode::UNSUPPORTED_PROFILE, "endpoint-address-properties-at-connect");

    if (portText.size() > 5)
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

    const auto port = integer(portText, std::numeric_limits<std::uint16_t>::max());

    if (host.front() == '[') {
        if (host.size() < 3 || host.back() != ']' || host.find(':') == std::string_view::npos)
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

        host.remove_prefix(1);
        host.remove_suffix(1);
    } else if (host.find(':') != std::string_view::npos)
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

    if (host.size() > MAXIMUM_HOST_BYTES || !client::detail::validEndpointHost(host))
        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-address");

    return {std::string(host), static_cast<std::uint16_t>(port), std::string(address)};
}
} // namespace detail

struct EndpointConfiguration::State {
    EndpointOptions options;
    EndpointConfigurationSnapshot snapshot;
};

EndpointConfiguration::EndpointConfiguration(std::shared_ptr<const State> state) noexcept : state_(std::move(state)) {
}

EndpointConfiguration EndpointConfiguration::create(const EndpointOptions &fallback,
                                                    std::span<const EndpointProperty> defaults,
                                                    std::span<const EndpointProperty> properties,
                                                    EndpointConfigurationLimits limits) {
    try {
        if (limits.properties == 0 || limits.keyBytes == 0 || limits.valueBytes == 0 || limits.totalBytes == 0)
            reject(ErrorCode::INVALID_ARGUMENT, "endpoint-configuration-limits");

        Selection selected{};
        InputEnvelope input{limits};

        for (const auto &property : defaults)
            input.select(property, EndpointPropertyOrigin::DEFAULT_PROPERTIES, false, selected);

        for (const auto &property : properties)
            input.select(property, EndpointPropertyOrigin::EXPLICIT_PROPERTIES, false, selected);

        std::optional<detail::EndpointAddress> address;

        if (selected[ADDRESS].present) {
            const auto route = addressProperties(selected[ADDRESS].value, input, selected);
            address = detail::parseEndpointAddress(route);
        }

        auto options = fallback;
        std::string name;

        for (std::size_t key = 0; key < ADDRESS; ++key) {
            if (!selected[key].present)
                continue;

            const auto text = selected[key].value;

            if (key == NAME) {
                if (text.size() > MAXIMUM_LABEL_BYTES)
                    reject(ErrorCode::RESOURCE_LIMIT, "endpoint-name");

                name = text;
            } else if (key == WILDCARD)
                options.enableWildcard = boolean(text);
            else if (key == CONTRACT) {
                if (text == "stream")
                    options.contract = DeliveryContract::STREAM;
                else if (text == "ticker")
                    options.contract = DeliveryContract::TICKER;
                else if (text == "history")
                    options.contract = DeliveryContract::HISTORY;
                else
                    reject(ErrorCode::INVALID_ARGUMENT, "endpoint-property-value");
            } else if (key == AGGREGATION) {
                const auto parsed = org::ttldtor::qdcxx::detail::parseQdTimePeriod(text);

                if (!parsed || *parsed > MAXIMUM_LOCAL_AGGREGATION_PERIOD)
                    reject(ErrorCode::INVALID_ARGUMENT, "endpoint-local-aggregation-period");

                options.aggregationPeriod = *parsed;
            } else if (key == REQUESTED) {
                if (text.empty() && selected[key].origin == EndpointPropertyOrigin::ADDRESS_PROPERTIES)
                    options.requestedAggregationPeriod.reset();
                else {
                    const auto parsed = org::ttldtor::qdcxx::detail::parseQdTimePeriod(text);

                    if (!parsed || *parsed > transport::MAXIMUM_REQUESTED_AGGREGATION_PERIOD)
                        reject(ErrorCode::INVALID_ARGUMENT, "endpoint-aggregation-period");

                    options.requestedAggregationPeriod = *parsed;
                }
            } else if (key == ATTEMPTS)
                options.maximumAttempts =
                    static_cast<std::uint32_t>(integer(text, std::numeric_limits<std::uint32_t>::max()));
            else {
                const auto value = std::chrono::milliseconds(integer(text, INT_MAX, key == RETRY));

                switch (key) {
                case CONNECT:
                    options.connectTimeout = value;
                    break;
                case DNS:
                    options.dnsTimeout = value;
                    break;
                case READ:
                    options.readTimeout = value;
                    break;
                case WRITE:
                    options.writeTimeout = value;
                    break;
                case POLL:
                    options.pollInterval = value;
                    break;
                case RETRY:
                    options.retryDelay = value;
                    break;
                case DRAIN:
                    options.drainTimeout = value;
                    break;
                default:
                    reject(ErrorCode::INVALID_ARGUMENT, "endpoint-property-value");
                }
            }
        }

        detail::validateEndpointOptions(options);
        auto requested = values(options, name);

        if (options.maximumAttempts == 1)
            options.retryDelay = std::chrono::milliseconds{0};

        auto effective = values(options, name);
        EndpointConfigurationSnapshot snapshot;
        snapshot.properties.reserve(ADDRESS);

        for (std::size_t key = 0; key < ADDRESS; ++key)
            snapshot.properties.push_back(
                {std::string(KEYS[key]), std::move(requested[key]), std::move(effective[key]), selected[key].origin});

        if (address) {
            snapshot.configuredAddress = std::move(address->spelling);
            snapshot.addressOrigin = selected[ADDRESS].origin;
        }

        return EndpointConfiguration(std::make_shared<const State>(State{std::move(options), std::move(snapshot)}));
    } catch (const std::bad_alloc &) {
        reject(ErrorCode::ALLOCATION_FAILURE, "endpoint-configuration-storage");
    } catch (const std::length_error &) {
        reject(ErrorCode::RESOURCE_LIMIT, "endpoint-configuration-storage");
    }
}

const EndpointOptions &EndpointConfiguration::options() const noexcept {
    return state_->options;
}

EndpointConfigurationSnapshot EndpointConfiguration::snapshot() const {
    try {
        return state_->snapshot;
    } catch (const std::bad_alloc &) {
        reject(ErrorCode::ALLOCATION_FAILURE, "endpoint-configuration-snapshot");
    } catch (const std::length_error &) {
        reject(ErrorCode::RESOURCE_LIMIT, "endpoint-configuration-snapshot");
    }
}
} // namespace org::ttldtor::qdcxx::api
