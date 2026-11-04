// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "feed_filter_ipf_config.hpp"

#include "../detail/qd_time_period.hpp"
#include "qd/api/feed.hpp"

#include <algorithm>
#include <vector>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
[[noreturn]] void invalid(const char *stage = "filter-ipf-config") {
    throw Error({ErrorCode::INVALID_ARGUMENT, stage});
}

std::u16string_view trim(std::u16string_view text) noexcept {
    while (!text.empty() && text.front() <= u' ') {
        text.remove_prefix(1);
    }

    while (!text.empty() && text.back() <= u' ') {
        text.remove_suffix(1);
    }

    return text;
}

bool escaped(std::u16string_view text, std::size_t pos) noexcept {
    std::size_t count = 0;

    while (pos > 0 && text[--pos] == u'\\') {
        ++count;
    }

    return (count & 1U) != 0;
}

std::u16string unescape(std::u16string_view text) {
    std::u16string result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\\') {
            if (++i == text.size()) {
                invalid();
            }
        }

        result.push_back(text[i]);
    }

    return result;
}

std::string utf8(std::u16string_view text) {
    std::string result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t unit = text[i];

        if (unit >= 0xd800 && unit <= 0xdbff) {
            if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff) {
                invalid("filter-ipf-unicode");
            }

            unit = 0x10000 + ((unit - 0xd800) << 10) + (text[i] - 0xdc00);
        } else if (unit >= 0xdc00 && unit <= 0xdfff) {
            invalid("filter-ipf-unicode");
        }

        if (unit < 0x80) {
            result.push_back(static_cast<char>(unit));
        } else if (unit < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (unit >> 6)));
            result.push_back(static_cast<char>(0x80 | (unit & 0x3f)));
        } else if (unit < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (unit >> 12)));
            result.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (unit & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (unit >> 18)));
            result.push_back(static_cast<char>(0x80 | ((unit >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (unit & 0x3f)));
        }
    }

    return result;
}
} // namespace

FilterIpfConfig parseFilterIpfConfig(std::u16string_view expression, std::size_t maximumUnits) {
    if (expression.size() > maximumUnits) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "filter-ipf-config-units"});
    }

    auto description = trim(expression);
    std::vector<std::u16string> properties;
    std::vector<char16_t> stack;

    while (!description.empty() && (description.back() == u')' || description.back() == u']') &&
           !escaped(description, description.size() - 1)) {
        std::size_t end = description.size() - 1;
        bool found = false;

        for (auto pos = description.size(); pos-- > 0;) {
            const auto c = description[pos];

            if (c == u'\\' || escaped(description, pos)) {
                continue;
            }

            if (c == u']' || c == u')') {
                stack.push_back(c == u']' ? u'[' : u'(');
            } else if (c == u',' && stack.size() == 1) {
                properties.emplace_back(trim(description.substr(pos + 1, end - pos - 1)));
                end = pos;
            } else if (c == u'[' || c == u'(') {
                if (stack.empty() || stack.back() != c) {
                    invalid();
                }

                stack.pop_back();

                if (stack.empty()) {
                    properties.emplace_back(trim(description.substr(pos + 1, end - pos - 1)));
                    description = trim(description.substr(0, pos));
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            invalid();
        }
    }

    if (description != u"ipf" || properties.empty()) {
        invalid();
    }

    std::ranges::reverse(properties);
    FilterIpfConfig config;
    config.source = utf8(unescape(properties.front()));

    for (std::size_t i = 1; i < properties.size(); ++i) {
        const auto property = std::u16string_view(properties[i]);
        const auto separator = property.find(u'=');

        auto name = std::u16string(trim(property.substr(0, separator)));

        for (auto &c : name) {
            if (c >= u'A' && c <= u'Z') {
                c += u'a' - u'A';
            } else if (c == u'\u017f') {
                c = u's';
            }
        }

        const auto value = separator == std::u16string_view::npos
                               ? std::u16string{}
                               : std::u16string(trim(property.substr(separator + 1)));

        if (name == u"update") {
            const auto period = org::ttldtor::qdcxx::detail::parseQdTimePeriod(utf8(value));

            if (!period) {
                invalid("filter-ipf-update-period");
            }

            config.update = *period;
        } else if (name == u"schedule") {
            config.schedule = value;
        } else if (name == u"user") {
            config.user = utf8(value);
        } else if (name == u"password") {
            config.password = utf8(value);
        } else {
            invalid("filter-ipf-property");
        }
    }

    if (config.schedule && config.update.count() == 0) {
        invalid("filter-ipf-schedule-without-update");
    }

    return config;
}
} // namespace org::ttldtor::qdcxx::api::detail
