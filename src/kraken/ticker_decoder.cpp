// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "ticker_decoder.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>
#include <new>
#include <simdjson.h>
#include <string_view>

namespace org::ttldtor::qdcxx::kraken {
namespace {

constexpr std::size_t MAX_MESSAGE_BYTES = 64 * 1024;
constexpr std::size_t MAX_ROWS = 64;
constexpr std::size_t MAX_OBJECT_FIELDS = 128;
constexpr std::size_t MAX_TIMESTAMP_BYTES = 64;
constexpr std::size_t MAX_CONTROL_BYTES = 256;
constexpr std::size_t MAX_DEPTH = 32;
constexpr std::uint64_t MAX_SIGNIFICAND = (std::uint64_t{1} << 55) - 1;
constexpr int INTEGER_RANK = 128;

/** Validate every JSON numeric token and make out-of-range values safe for DOM parsing.
 * @param input Original JSON, including all unknown fields.
 * @return Same-size JSON copy, or a syntax error. Original bytes are unchanged.
 */
std::expected<std::string, DecodeError> validatedDomInput(std::string_view input) {
    std::string output(input);
    bool inString = false;

    for (std::size_t index = 0; index < input.size();) {
        const char character = input[index];

        if (inString) {
            if (character == '\\' && index + 1 < input.size()) {
                index += 2;
            } else {
                if (character == '"') {
                    inString = false;
                }

                ++index;
            }

            continue;
        }

        if (character == '"') {
            inString = true;
            ++index;
            continue;
        }

        if (character != '-' && (character < '0' || character > '9')) {
            ++index;
            continue;
        }

        const auto start = index;

        if (input[index] == '-') {
            ++index;
        }

        if (index == input.size() || input[index] < '0' || input[index] > '9') {
            return std::unexpected(DecodeError::MALFORMED_JSON);
        }

        if (input[index] == '0') {
            ++index;
        } else {
            while (index < input.size() && input[index] >= '0' && input[index] <= '9') {
                ++index;
            }
        }

        bool floating = false;

        if (index < input.size() && input[index] == '.') {
            floating = true;
            ++index;
            const auto digits = index;

            while (index < input.size() && input[index] >= '0' && input[index] <= '9') {
                ++index;
            }

            if (index == digits) {
                return std::unexpected(DecodeError::MALFORMED_JSON);
            }
        }

        if (index < input.size() && (input[index] == 'e' || input[index] == 'E')) {
            floating = true;
            ++index;

            if (index < input.size() && (input[index] == '+' || input[index] == '-')) {
                ++index;
            }

            const auto digits = index;

            while (index < input.size() && input[index] >= '0' && input[index] <= '9') {
                ++index;
            }

            if (index == digits) {
                return std::unexpected(DecodeError::MALFORMED_JSON);
            }
        }

        if (index < input.size() && input[index] != ',' && input[index] != ']' && input[index] != '}' &&
            input[index] != ' ' && input[index] != '\t' && input[index] != '\r' && input[index] != '\n') {
            return std::unexpected(DecodeError::MALFORMED_JSON);
        }

        const auto token = input.substr(start, index - start);
        bool replace = floating;

        if (!floating && token.front() == '-') {
            std::int64_t value = 0;
            replace = std::from_chars(token.data(), token.data() + token.size(), value).ec != std::errc{};
        } else if (!floating) {
            std::uint64_t value = 0;
            replace = std::from_chars(token.data(), token.data() + token.size(), value).ec != std::errc{};
        }

        if (replace) {
            // A floating token has at least three bytes; an overflowing integer has more.
            output[start] = '0';
            output[start + 1] = '.';
            output[start + 2] = '0';
            std::fill(output.begin() + static_cast<std::ptrdiff_t>(start + 3),
                      output.begin() + static_cast<std::ptrdiff_t>(index), ' ');
        }
    }

    return output;
}

/** Convert a nonnegative decimal token to an exact finite QD wide decimal.
 * @param token Borrowed JSON numeric token.
 * @return Exact raw bits, or a conversion error.
 */
std::expected<core::WideDecimal, DecodeError> exactDecimal(std::string_view token) {
    while (!token.empty() &&
           (token.back() == ' ' || token.back() == '\n' || token.back() == '\r' || token.back() == '\t')) {
        token.remove_suffix(1);
    }

    if (token.empty() || token.front() == '-') {
        return std::unexpected(DecodeError::INVALID_DECIMAL);
    }

    const auto exponentPosition = token.find_first_of("eE");
    const auto mantissa = token.substr(0, exponentPosition);
    int exponent = 0;

    if (exponentPosition != std::string_view::npos) {
        auto exponentText = token.substr(exponentPosition + 1);

        if (!exponentText.empty() && exponentText.front() == '+') {
            exponentText.remove_prefix(1);
        }

        const auto result = std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);

        if (result.ec != std::errc{} || result.ptr != exponentText.data() + exponentText.size() || exponent < -100000 ||
            exponent > 100000) {
            return std::unexpected(DecodeError::INVALID_DECIMAL);
        }
    }

    const auto point = mantissa.find('.');

    if (point != std::string_view::npos) {
        exponent -= static_cast<int>(mantissa.size() - point - 1);
    }

    // The input bound makes this owned scratch finite, and preserves decimal digits exactly.
    std::string digits;
    digits.reserve(mantissa.size());

    for (const char character : mantissa) {
        if (character != '.') {
            digits.push_back(character);
        }
    }

    const auto first = digits.find_first_not_of('0');

    if (first == std::string::npos) {
        return core::WideDecimal{INTEGER_RANK};
    }

    digits.erase(0, first);

    while (digits.size() > 1 && digits.back() == '0' && exponent < 127) {
        digits.pop_back();
        ++exponent;
    }

    if (digits.size() > 17 || exponent < -127 || exponent > 143) {
        return std::unexpected(DecodeError::INVALID_DECIMAL);
    }

    std::uint64_t significand = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), significand);

    if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
        return std::unexpected(DecodeError::INVALID_DECIMAL);
    }

    while (exponent > 127) {
        if (significand > MAX_SIGNIFICAND / 10) {
            return std::unexpected(DecodeError::INVALID_DECIMAL);
        }

        significand *= 10;
        --exponent;
    }

    if (significand > MAX_SIGNIFICAND) {
        return std::unexpected(DecodeError::INVALID_DECIMAL);
    }

    const auto rank = static_cast<std::uint64_t>(INTEGER_RANK - exponent);

    return core::WideDecimal{std::bit_cast<std::int64_t>((significand << 8) | rank)};
}

/** Check duplicate direct object keys.
 * @param object Validated JSON object.
 * @return True when each direct key is unique.
 */
bool uniqueKeys(simdjson::dom::object object) {
    std::size_t fieldCount = 0;

    for (auto field : object) {
        if (++fieldCount > MAX_OBJECT_FIELDS) {
            return false;
        }

        std::size_t matches = 0;

        for (auto candidate : object) {
            if (field.key == candidate.key && ++matches > 1) {
                return false;
            }
        }
    }

    return true;
}

/** Read a required string from a JSON object.
 * @param object Validated JSON object.
 * @param name Field name.
 * @param value Receives a borrowed decoded string.
 * @return True when the field is a string.
 */
bool getString(simdjson::dom::object object, std::string_view name, std::string_view &value) {
    return !object[name].get(value);
}

} // namespace

std::expected<Message, DecodeError> decodeTicker(std::string_view json) {
    if (json.size() > MAX_MESSAGE_BYTES) {
        return std::unexpected(DecodeError::RESOURCE_LIMIT);
    }

    if (json.empty()) {
        return std::unexpected(DecodeError::MALFORMED_JSON);
    }

    try {
        auto domInput = validatedDomInput(json);

        if (!domInput) {
            return std::unexpected(domInput.error());
        }

        simdjson::dom::parser validator;

        if (validator.allocate(std::max(json.size(), simdjson::dom::MINIMAL_DOCUMENT_CAPACITY), MAX_DEPTH)) {
            return std::unexpected(DecodeError::RESOURCE_LIMIT);
        }

        simdjson::dom::element root;

        if (validator.parse(domInput->data(), domInput->size()).get(root)) {
            return std::unexpected(DecodeError::MALFORMED_JSON);
        }

        simdjson::dom::object object;

        if (root.get(object) || !uniqueKeys(object)) {
            return std::unexpected(DecodeError::INVALID_MESSAGE);
        }

        std::string_view channel;

        if (!getString(object, "channel", channel)) {
            std::string_view method;
            bool success = false;

            if (!getString(object, "method", method) || method != "subscribe" || object["success"].get(success)) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            Message decoded{success ? MessageKind::ACKNOWLEDGEMENT : MessageKind::REJECTION, {}, {}, {}};
            auto requestId = object["req_id"];

            if (!requestId.error()) {
                std::uint64_t value = 0;

                if (requestId.get(value)) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                decoded.requestId = value;
            }

            if (success) {
                simdjson::dom::object result;
                std::string_view resultChannel;
                std::string_view symbol;

                if (object["result"].get(result) || !uniqueKeys(result) ||
                    !getString(result, "channel", resultChannel) || resultChannel != "ticker" ||
                    !getString(result, "symbol", symbol) || symbol != "BTC/USD") {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }
            } else {
                std::string_view error;

                if (!getString(object, "error", error) || error.empty() || error.size() > MAX_CONTROL_BYTES) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                decoded.detail.assign(error);
            }

            return decoded;
        }

        if (channel == "heartbeat") {
            return Message{MessageKind::HEARTBEAT, {}, {}, {}};
        }

        if (channel == "status") {
            simdjson::dom::array data;

            if (object["data"].get(data)) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            Message decoded{MessageKind::STATUS, {}, {}, {}};
            std::size_t count = 0;

            for (auto value : data) {
                simdjson::dom::object row;
                std::string_view status;

                if (++count != 1 || value.get(row) || !uniqueKeys(row) || !getString(row, "system", status) ||
                    status.empty() || status.size() > MAX_CONTROL_BYTES) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                decoded.detail.assign(status);
            }

            if (count != 1) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            return decoded;
        }

        std::string_view type;
        simdjson::dom::array rows;

        if (channel != "ticker" || !getString(object, "type", type) || (type != "snapshot" && type != "update") ||
            object["data"].get(rows)) {
            return std::unexpected(DecodeError::INVALID_MESSAGE);
        }

        std::size_t count = 0;

        for (auto rowValue : rows) {
            simdjson::dom::object row;
            std::string_view symbol;
            std::string_view timestamp;

            if (++count > MAX_ROWS) {
                return std::unexpected(DecodeError::RESOURCE_LIMIT);
            }

            if (rowValue.get(row) || !uniqueKeys(row) || !getString(row, "symbol", symbol) || symbol != "BTC/USD" ||
                !getString(row, "timestamp", timestamp) || timestamp.empty() ||
                timestamp.size() > MAX_TIMESTAMP_BYTES) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            for (const auto name : {"bid", "ask", "bid_qty", "ask_qty"}) {
                auto field = row[name];

                if (field.error() || (field.value().type() != simdjson::dom::element_type::DOUBLE &&
                                      field.value().type() != simdjson::dom::element_type::INT64 &&
                                      field.value().type() != simdjson::dom::element_type::UINT64)) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }
            }
        }

        if (!count) {
            return std::unexpected(DecodeError::INVALID_MESSAGE);
        }

        // The DOM pass validates complete JSON before On-Demand exposes original numeric tokens.
        simdjson::padded_string padded(json);
        simdjson::ondemand::parser parser;
        auto document = parser.iterate(padded);
        simdjson::ondemand::object rawRoot;

        if (document.get_object().get(rawRoot)) {
            return std::unexpected(DecodeError::INVALID_MESSAGE);
        }

        Message result{type == "snapshot" ? MessageKind::SNAPSHOT : MessageKind::UPDATE, {}, {}, {}};
        result.entries.reserve(count);
        auto validated = rows.begin();

        for (auto rootField : rawRoot) {
            std::string_view rootName;

            if (rootField.unescaped_key().get(rootName)) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            if (rootName != "data") {
                continue;
            }

            simdjson::ondemand::array rawRows;

            if (rootField.value().get_array().get(rawRows)) {
                return std::unexpected(DecodeError::INVALID_MESSAGE);
            }

            for (auto rawRow : rawRows) {
                simdjson::ondemand::object rawObject;

                if (rawRow.get_object().get(rawObject)) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                Observation observation;
                observation.quote.symbol = u"BTC/USD";
                simdjson::dom::object sourceRow;
                std::string_view timestamp;

                if ((*validated).get(sourceRow) || !getString(sourceRow, "timestamp", timestamp)) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                observation.timestamp.assign(timestamp);
                ++validated;

                core::WideDecimal *outputs[] = {&observation.quote.bidPrice, &observation.quote.askPrice,
                                                &observation.quote.bidSize, &observation.quote.askSize};
                constexpr std::string_view names[] = {"bid", "ask", "bid_qty", "ask_qty"};
                std::size_t read = 0;

                for (auto field : rawObject) {
                    std::string_view key;

                    if (field.unescaped_key().get(key)) {
                        return std::unexpected(DecodeError::INVALID_MESSAGE);
                    }

                    for (std::size_t index = 0; index < std::size(names); ++index) {
                        if (key != names[index]) {
                            continue;
                        }

                        std::string_view token;

                        if (field.value().raw_json_token().get(token)) {
                            return std::unexpected(DecodeError::INVALID_MESSAGE);
                        }

                        auto decimal = exactDecimal(token);

                        if (!decimal) {
                            return std::unexpected(decimal.error());
                        }

                        *outputs[index] = *decimal;
                        ++read;
                        break;
                    }
                }

                if (read != std::size(names)) {
                    return std::unexpected(DecodeError::INVALID_MESSAGE);
                }

                result.entries.push_back(std::move(observation));
            }
        }

        if (result.entries.size() != count) {
            return std::unexpected(DecodeError::INVALID_MESSAGE);
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(DecodeError::ALLOCATION_FAILURE);
    } catch (const simdjson::simdjson_error &) {
        return std::unexpected(DecodeError::MALFORMED_JSON);
    }
}

} // namespace org::ttldtor::qdcxx::kraken
