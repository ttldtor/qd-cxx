// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "qd/glossary/additional_underlyings.hpp"
#include "qd/glossary/cfi.hpp"

#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <vector>

namespace {
thread_local std::ptrdiff_t failAllocationAfter = -1;
}

// Inject failures at real value allocations without changing production code.
void *operator new(std::size_t size) {
    if (failAllocationAfter == 0) {
        throw std::bad_alloc();
    }

    if (failAllocationAfter > 0) {
        --failAllocationAfter;
    }

    if (auto memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {
using namespace org::ttldtor::qdcxx::glossary;

/** Split fixture cells without interpreting their UTF-16 payload. */
std::vector<std::string> split(std::string_view text, char delimiter) {
    std::vector<std::string> result;
    std::size_t start = 0;

    do {
        auto end = text.find(delimiter, start);
        result.emplace_back(text.substr(start, end == text.npos ? end : end - start));

        if (end == text.npos) {
            break;
        }

        start = end + 1;
    } while (true);

    return result;
}

/** Decode exact code units from the reviewed fixture. */
std::optional<std::u16string> unhex(std::string_view text) {
    if (text == "~") {
        return std::nullopt;
    }

    std::u16string result;

    if (text == "-") {
        return result;
    }

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result += static_cast<char16_t>(std::stoul(std::string(text.substr(i, 4)), nullptr, 16));
    }

    return result;
}

/** Borrow a decoded fixture string only for the synchronous API call. */
std::optional<std::u16string_view> view(const std::optional<std::u16string> &text) {
    return text ? std::optional<std::u16string_view>(*text) : std::nullopt;
}

/** Encode exact UTF-16 units, including unpaired surrogates. */
std::string hex(std::u16string_view text) {
    if (text.empty()) {
        return "-";
    }

    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (auto ch : text) {
        for (int shift = 12; shift >= 0; shift -= 4) {
            result += DIGITS[(ch >> shift) & 15];
        }
    }

    return result;
}

/** Dictionary text contains only ASCII; preserve those exact units in projections. */
std::string hex(std::string_view text) {
    return hex(std::u16string(text.begin(), text.end()));
}

/** Canonical Java longBits encoding (all fixture NaNs canonicalized upstream). */
std::string bits(double value) {
    std::ostringstream out;
    out << std::hex << std::bit_cast<std::uint64_t>(value);

    return out.str();
}

/** Observe public getters, without duplicating parsing or dictionary selection. */
std::string projection(const Cfi &cfi) {
    auto boolean = [](bool value) {
        return value ? "true" : "false";
    };
    std::string out = cfi.getCode() + "|" + std::to_string(cfi.getIntCode()) + "|" + boolean(cfi.isEquity()) + "," +
                      boolean(cfi.isDebtInstrument()) + "," + boolean(cfi.isEntitlement()) + "," +
                      boolean(cfi.isOption()) + "," + boolean(cfi.isFuture()) + "," + boolean(cfi.isOther()) + "|" +
                      hex(cfi.describe());

    for (const auto &value : cfi.decipher()) {
        auto attribute = value.getAttribute();
        out += "|" + std::string(1, value.getCode()) + "," + hex(value.getName()) + "," + hex(value.getDescription()) +
               "," + hex(attribute.getName()) + "," + hex(attribute.getDescription());
    }

    return out;
}

/** Observe the entire dictionary value list in its production order. */
std::string values(const CfiAttribute &attribute) {
    std::string out;

    for (const auto &value : attribute.getValues()) {
        CHECK(value.getAttribute() == attribute);
        CHECK(value.toString() == std::string(1, value.getCode()) + " = " + std::string(value.getName()));

        if (!out.empty()) {
            out += '|';
        }

        out += std::string(1, value.getCode()) + "," + hex(value.getName()) + "," + hex(value.getDescription());
    }

    return out;
}

constexpr auto COMPILE_TIME_OPTION = Cfi::valueOf(std::u16string_view(u"OP"));
static_assert(COMPILE_TIME_OPTION.isOption() && COMPILE_TIME_OPTION.getGroup() == 'P');
static_assert(Cfi::valueOf(0) == Cfi::EMPTY);
// Cross-translation-unit constants must be usable during dynamic initialization before main.
const auto BEFORE_MAIN_CFI = Cfi::EMPTY.getCode();
const auto BEFORE_MAIN_AU = AdditionalUnderlyings::empty().getText();
} // namespace

TEST_CASE("pinned production glossary observations") {
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_SOURCE_DIR) / "tests/fixtures/glossary/oracle.tsv");
    REQUIRE(input.good());
    std::string line;
    std::size_t rows = 0;

    while (std::getline(input, line)) {
        ++rows;
        auto cells = split(line, '\t');
        CAPTURE(rows);
        CAPTURE(cells[0]);
        CAPTURE(cells[1]);

        if (cells[0] == "C" || cells[0] == "I") {
            std::string actual;
            try {
                if (cells[0] == "C") {
                    auto text = unhex(cells[1]);
                    actual = projection(Cfi::valueOf(view(text)));
                } else {
                    actual = projection(Cfi::valueOf(static_cast<std::int32_t>(std::stoll(cells[1]))));
                }
            } catch (const std::invalid_argument &error) {
                actual = "!" + std::string(error.what());
            }
            CHECK(actual == cells[2]);
        } else if (cells[0] == "D") {
            auto cfi = Cfi::valueOf(std::u16string_view(std::u16string(cells[1].begin(), cells[1].end())));
            auto attribute = cfi.decipher()[std::stoul(cells[2])].getAttribute();
            CHECK(hex(attribute.getName()) == cells[3]);
            CHECK(hex(attribute.getDescription()) == cells[4]);
            CHECK(values(attribute) == cells[5]);
            CHECK(attribute.toString() == attribute.getName());
        } else if (cells[0] == "A") {
            auto text = unhex(cells[1]);
            try {
                auto value = AdditionalUnderlyings::valueOf(view(text));
                CHECK(hex(value.getText()) == cells[2]);
                CHECK(value.toString() == value.getText());
                std::string map;

                for (const auto &[symbol, spc] : value.getMap()) {
                    CHECK(bits(value.getSpc(std::u16string_view(symbol))) == bits(spc));

                    if (!map.empty()) {
                        map += '|';
                    }

                    map += hex(symbol) + "," + bits(spc);
                }

                CHECK((map.empty() ? "-" : map) == cells[3]);
            } catch (const std::invalid_argument &error) {
                CHECK("!" + std::string(error.what()) == cells[2]);
            }
        } else if (cells[0] == "Q") {
            auto text = unhex(cells[1]);
            auto symbol = unhex(cells[2]);
            CHECK(bits(AdditionalUnderlyings::getSpc(view(text), view(symbol))) == cells[3]);
        } else if (cells[0] == "M") {
            auto number = std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(cells[1], nullptr, 16)));
            AdditionalUnderlyings::Map map{{u"A", number}};
            try {
                auto value = AdditionalUnderlyings::valueOf(&map);
                CHECK(hex(value.getText()) == cells[2]);
                CHECK(bits(value.getSpc(std::u16string_view(u"A"))) == cells[3]);
                CHECK(bits(value.getMap().at(u"A")) == cells[3]);
                auto reparsed = AdditionalUnderlyings::valueOf(std::u16string_view(value.getText()));
                CHECK(bits(reparsed.getSpc(std::u16string_view(u"A"))) == cells[3]);
            } catch (const std::invalid_argument &error) {
                CHECK("!" + std::string(error.what()) == cells[2]);
            }
        } else {
            FAIL("unrecognized fixture row");
        }
    }

    CHECK(rows > 10000);
}

TEST_CASE("owning values, exact equality and finite model limits") {
    CHECK(BEFORE_MAIN_CFI == "XXXXXX");
    CHECK(BEFORE_MAIN_AU.empty());
    std::u16string input = u"B 2; A 1; A 3";
    auto value = AdditionalUnderlyings::valueOf(std::u16string_view(input));
    input.assign(u"changed");
    CHECK(value.getText() == u"B 2; A 1; A 3");
    CHECK(value.getSpc(std::u16string_view(u"A")) == 1);
    CHECK(AdditionalUnderlyings::getSpc(std::u16string_view(value.getText()), std::u16string_view(u"A")) == 3);
    CHECK_FALSE(value == AdditionalUnderlyings::valueOf(std::u16string_view(u"A 1; B 2")));
    CHECK(value.getSpc(std::nullopt) == 0);
    CHECK(value.getSpc(std::u16string_view(u"absent")) == 0);
    CHECK(AdditionalUnderlyings::valueOf(std::nullopt).getText().empty());
    CHECK(AdditionalUnderlyings::valueOf(static_cast<const AdditionalUnderlyings::Map *>(nullptr)).getText().empty());
    CHECK_THROWS_AS((void)AdditionalUnderlyings::valueOf(std::u16string_view(u"A 1"), {2, 1}), std::length_error);
    CHECK(AdditionalUnderlyings::valueOf(std::u16string_view(u"A 1"), {3, 1}).getMap().size() == 1);
    CHECK_THROWS_AS((void)AdditionalUnderlyings::valueOf(std::u16string_view(u"A 1; A 2"), {100, 1}),
                    std::length_error);
    AdditionalUnderlyings::Map map{{u"B", -0.0}, {u"A", 1}};
    CHECK(AdditionalUnderlyings::valueOf(&map, {9, 2}).getText() == u"A 1; B -0");
    CHECK_THROWS_AS((void)AdditionalUnderlyings::valueOf(&map, {8, 2}), std::length_error);
    CHECK_THROWS_AS((void)AdditionalUnderlyings::valueOf(&map, {100, 1}), std::length_error);

    for (auto symbol : {u"", u"A B", u"A;B"}) {
        AdditionalUnderlyings::Map invalid{{symbol, 1}};
        CHECK_THROWS_AS((void)AdditionalUnderlyings::valueOf(&invalid), std::invalid_argument);
    }

    auto metadata = [] {
        return Cfi::valueOf(std::u16string_view(u"OPASPS")).decipher();
    }();
    CHECK(metadata[1].getName() == "Put options");
    CHECK(metadata[0].getAttribute().getName() == "Categories");
}

TEST_CASE("copy assignment and map construction preserve values on allocation failure") {
    const std::u16string text = u"LONG_UNDERLYING_SYMBOL_ALPHA 1.23456789; LONG_UNDERLYING_SYMBOL_BETA -0";
    auto source = AdditionalUnderlyings::valueOf(std::u16string_view(text));
    bool completed = false;

    for (std::ptrdiff_t failAfter = 0; failAfter < 100 && !completed; ++failAfter) {
        auto target = AdditionalUnderlyings::valueOf(std::u16string_view(u"OLD 7"));
        failAllocationAfter = failAfter;
        bool failed = false;
        try {
            target = source;
        } catch (const std::bad_alloc &) {
            failed = true;
        }
        failAllocationAfter = -1;
        CHECK(source.getText() == text);

        if (failed) {
            CHECK(target.getText() == u"OLD 7");
            CHECK(target.getMap().size() == 1);
            CHECK(target.getSpc(std::u16string_view(u"OLD")) == 7);
        } else {
            CHECK(target == source);
            completed = true;
        }
    }

    CHECK(completed);
    completed = false;

    for (std::ptrdiff_t failAfter = 0; failAfter < 100 && !completed; ++failAfter) {
        failAllocationAfter = failAfter;
        try {
            auto value = AdditionalUnderlyings::valueOf(&source.getMap());
            completed = value.getMap().size() == 2;
        } catch (const std::bad_alloc &) {
        }
        failAllocationAfter = -1;
        CHECK(source.getText() == text);
        CHECK(source.getMap().size() == 2);
    }

    CHECK(completed);
    completed = false;

    for (std::ptrdiff_t failAfter = 0; failAfter < 100 && !completed; ++failAfter) {
        auto original = source;
        std::optional<AdditionalUnderlyings> destination;
        failAllocationAfter = failAfter;
        bool failed = false;
        try {
            destination.emplace(std::move(original));
        } catch (const std::bad_alloc &) {
            failed = true;
        }
        failAllocationAfter = -1;

        if (failed) {
            CHECK(original == source);
            CHECK(original.getMap() == source.getMap());
        } else {
            REQUIRE(destination.has_value());
            CHECK(destination->getText() == text);
            CHECK(original.getText().empty());
            CHECK(original.getMap().empty());
            completed = true;
        }
    }

    CHECK(completed);
    auto moved = std::move(source);
    CHECK(source.getText().empty());
    CHECK(source.getMap().empty());
    source = AdditionalUnderlyings::valueOf(std::u16string_view(u"REUSED 5"));
    CHECK(source.getSpc(std::u16string_view(u"REUSED")) == 5);
    CHECK(moved.getText() == text);
    failAllocationAfter = 0;
    const auto decoded = Cfi::EMPTY.decipher();
    bool metadataAvailable = decoded[0].getName() == "Unknown";
    failAllocationAfter = -1;
    CHECK(metadataAvailable);
}
