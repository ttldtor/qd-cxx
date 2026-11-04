// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/glossary/cfi.hpp"

#include <span>

namespace org::ttldtor::qdcxx::glossary {
namespace {
struct DictionaryValue {
    char code;
    std::string_view name;
    std::string_view description;
};
} // namespace

namespace detail {
struct CfiAttributeData {
    std::string_view name;
    std::string_view description;
    std::span<const DictionaryValue> values;
};
} // namespace detail

namespace {
#include "cfi_dictionary.hpp"
}

std::string_view CfiAttribute::getName() const noexcept {
    return data_->name;
}

std::string_view CfiAttribute::getDescription() const noexcept {
    return data_->description;
}

std::string CfiAttribute::toString() const {
    return std::string(getName());
}

std::vector<CfiValue> CfiAttribute::getValues() const {
    std::vector<CfiValue> values;
    values.reserve(data_->values.size());

    for (const auto &value : data_->values) {
        values.push_back(CfiValue(*this, value.code, value.name, value.description));
    }

    return values;
}

std::string CfiValue::toString() const {
    return std::string(1, code_) + " = " + std::string(name_);
}

std::string Cfi::getCode() const {
    std::string code(CODE_LENGTH, 'X');

    for (unsigned i = 0; i < CODE_LENGTH; ++i) {
        code[i] = character(i);
    }

    return code;
}

std::array<CfiValue, 6> Cfi::decipher() const noexcept {
    std::array<const detail::CfiAttributeData *, 6> attributes{&CATEGORIES, &XATTR, &XATTR, &XATTR, &XATTR, &XATTR};

    for (const auto &group : GROUPS) {
        if (group.category == getCategory()) {
            attributes[1] = group.attribute;
            break;
        }
    }

    for (const auto &entry : ATTRIBUTES) {
        if (entry.prefix[0] == getCategory() && entry.prefix[1] == getGroup()) {
            for (unsigned i = 0; i < 4; ++i) {
                attributes[i + 2] = entry.attributes[i];
            }

            break;
        }
    }

    auto find = [](const detail::CfiAttributeData *attribute, char code) {
        for (const auto &value : attribute->values) {
            if (value.code == code) {
                return CfiValue(CfiAttribute(attribute), code, value.name, value.description);
            }
        }

        return CfiValue(CfiAttribute(attribute), code, code == 'X' ? "Unknown" : "Unrecognized",
                        code == 'X' ? "Attribute value is unknown." : "Attribute value is not recognized.");
    };

    return {find(attributes[0], character(0)), find(attributes[1], character(1)), find(attributes[2], character(2)),
            find(attributes[3], character(3)), find(attributes[4], character(4)), find(attributes[5], character(5))};
}

std::string Cfi::describe() const {
    std::string text;

    for (const auto &value : decipher()) {
        if (!text.empty()) {
            text += "; ";
        }

        text += value.getName();
    }

    return text;
}
} // namespace org::ttldtor::qdcxx::glossary
