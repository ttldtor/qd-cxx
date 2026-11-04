// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::glossary {
namespace detail {
struct CfiAttributeData;
}
class CfiValue;

/** Immutable CFI dictionary attribute. Its metadata refers only to program-lifetime literals. */
class CfiAttribute {
    const detail::CfiAttributeData *data_;

    explicit constexpr CfiAttribute(const detail::CfiAttributeData *data) noexcept : data_(data) {
    }
    friend class Cfi;
    friend class CfiValue;

    public:
    /** @return Program-lifetime short attribute name. */
    [[nodiscard]] std::string_view getName() const noexcept;
    /** @return Program-lifetime attribute description. */
    [[nodiscard]] std::string_view getDescription() const noexcept;
    /** @return Independent list of supported values; metadata remains valid after this attribute is destroyed.
     * @throws std::bad_alloc Result allocation fails.
     */
    [[nodiscard]] std::vector<CfiValue> getValues() const;
    /** @return Short attribute name as an owned string. */
    [[nodiscard]] std::string toString() const;
    friend bool operator==(const CfiAttribute &, const CfiAttribute &) = default;
};

/** One deciphered character, including unknown/unrecognized values; metadata has program lifetime. */
class CfiValue {
    CfiAttribute attribute_;
    char code_;
    std::string_view name_;
    std::string_view description_;

    CfiValue(CfiAttribute attribute, char code, std::string_view name, std::string_view description) noexcept
        : attribute_(attribute), code_(code), name_(name), description_(description) {
    }
    friend class Cfi;
    friend class CfiAttribute;

    public:
    /** @return Attribute containing this value, independent of this object's lifetime. */
    [[nodiscard]] CfiAttribute getAttribute() const noexcept {
        return attribute_;
    }

    /** @return Original character, also for unrecognized values. */
    [[nodiscard]] char getCode() const noexcept {
        return code_;
    }

    /** @return Program-lifetime short value name. */
    [[nodiscard]] std::string_view getName() const noexcept {
        return name_;
    }

    /** @return Program-lifetime value description. */
    [[nodiscard]] std::string_view getDescription() const noexcept {
        return description_;
    }

    /** @return Owned text in the form "C = Name". */
    [[nodiscard]] std::string toString() const;
};

/** Six uppercase ASCII CFI letters; syntactic validity does not imply a recognized ISO combination. */
class Cfi {
    static constexpr unsigned CODE_LENGTH = 6;
    static constexpr unsigned BITS_PER_LETTER = 5;
    static constexpr unsigned PACKED_BITS = CODE_LENGTH * BITS_PER_LETTER;
    static constexpr unsigned FIRST_LETTER_SHIFT = (CODE_LENGTH - 1) * BITS_PER_LETTER;
    static constexpr std::uint32_t LETTER_MASK = (std::uint32_t{1} << BITS_PER_LETTER) - 1;
    // Six X=24 fields in the packed five-bit representation.
    static constexpr std::uint32_t EMPTY_CODE = 831283992;
    std::uint32_t intCode_ = EMPTY_CODE;

    explicit constexpr Cfi(std::uint32_t code) noexcept : intCode_(code) {
    }

    constexpr char character(unsigned position) const noexcept {
        return static_cast<char>('A' - 1 +
                                 ((intCode_ >> (FIRST_LETTER_SHIFT - position * BITS_PER_LETTER)) & LETTER_MASK));
    }

    public:
    /** Construct XXXXXX without allocation. */
    constexpr Cfi() noexcept = default;
    static const Cfi EMPTY; ///< Constant-initialized XXXXXX value, safe before main across translation units.

    /** Parse uppercase ASCII letters, padding short input with X; null/empty means XXXXXX.
     * @param code Exact UTF-16 input, borrowed only for this call.
     * @return Independent value.
     * @throws std::invalid_argument Input exceeds six characters or contains a non-uppercase-ASCII letter.
     */
    [[nodiscard]] static constexpr Cfi valueOf(std::optional<std::u16string_view> code) {
        if (!code) {
            return {};
        }

        if (code->size() > CODE_LENGTH) {
            throw std::invalid_argument("code is too long");
        }

        std::uint32_t packed = 0;

        for (unsigned i = 0; i < CODE_LENGTH; ++i) {
            auto ch = i < code->size() ? (*code)[i] : u'X';

            if (ch < u'A' || ch > u'Z') {
                throw std::invalid_argument("code contains invalid character");
            }

            packed = (packed << BITS_PER_LETTER) | static_cast<std::uint32_t>(ch - u'A' + 1);
        }

        return Cfi(packed);
    }

    /** Parse a packed six-letter representation; zero means XXXXXX.
     * @param code Six five-bit A=1 through Z=26 fields, most significant character first.
     * @return Independent value.
     * @throws std::invalid_argument Bits outside the low 30 or invalid character fields are present.
     */
    [[nodiscard]] static constexpr Cfi valueOf(std::int32_t code) {
        if (code == 0) {
            return {};
        }

        auto packed = static_cast<std::uint32_t>(code);

        if ((packed >> PACKED_BITS) != 0) {
            throw std::invalid_argument("code is invalid");
        }

        for (unsigned i = 0; i < CODE_LENGTH; ++i) {
            auto ch = (packed >> (i * BITS_PER_LETTER)) & LETTER_MASK;

            if (ch < 1 || ch > 26) {
                throw std::invalid_argument("code contains invalid character");
            }
        }

        return Cfi(packed);
    }

    /** @return Owned six-character ASCII code. */
    [[nodiscard]] std::string getCode() const;

    /** @return Packed six-letter representation. */
    [[nodiscard]] constexpr std::int32_t getIntCode() const noexcept {
        return static_cast<std::int32_t>(intCode_);
    }

    /** @return First character. */
    [[nodiscard]] constexpr char getCategory() const noexcept {
        return character(0);
    }

    /** @return Second character. */
    [[nodiscard]] constexpr char getGroup() const noexcept {
        return character(1);
    }

    /** @return Whether category is E. */
    [[nodiscard]] constexpr bool isEquity() const noexcept {
        return getCategory() == 'E';
    }

    /** @return Whether category is D. */
    [[nodiscard]] constexpr bool isDebtInstrument() const noexcept {
        return getCategory() == 'D';
    }

    /** @return Whether category is R. */
    [[nodiscard]] constexpr bool isEntitlement() const noexcept {
        return getCategory() == 'R';
    }

    /** @return Whether category is O. */
    [[nodiscard]] constexpr bool isOption() const noexcept {
        return getCategory() == 'O';
    }

    /** @return Whether category is F. */
    [[nodiscard]] constexpr bool isFuture() const noexcept {
        return getCategory() == 'F';
    }

    /** @return Whether category is M. */
    [[nodiscard]] constexpr bool isOther() const noexcept {
        return getCategory() == 'M';
    }

    /** @return Six character explanations referencing immutable program-lifetime dictionary data. */
    [[nodiscard]] std::array<CfiValue, 6> decipher() const noexcept;
    /** @return Owned names of all six values separated by "; ". */
    [[nodiscard]] std::string describe() const;

    /** @return Owned six-character code. */
    [[nodiscard]] std::string toString() const {
        return getCode();
    }

    friend constexpr bool operator==(Cfi, Cfi) noexcept = default;
};

inline constexpr Cfi Cfi::EMPTY{};
} // namespace org::ttldtor::qdcxx::glossary
