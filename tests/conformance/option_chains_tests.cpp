// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "qd/ipf/option_chains.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <string_view>

namespace {
thread_local std::ptrdiff_t failAllocationAfter = -1;
}

// Exercise each allocation boundary in the real admission path, without a production test hook.
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
using namespace org::ttldtor::qdcxx::ipf;

/** Split a reviewed TSV line, preserving empty cells.
 * @param text Borrowed line.
 * @param separator Cell delimiter.
 * @return Owned cells.
 */
std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> result;
    std::size_t begin = 0;

    do {
        const auto end = text.find(separator, begin);
        result.emplace_back(text.substr(begin, end == text.npos ? end : end - begin));

        if (end == text.npos) {
            break;
        }

        begin = end + 1;
    } while (true);

    return result;
}

/** Decode exact UTF-16 fixture units without Unicode normalization.
 * @param text Reviewed four-digit-unit hex, or the empty marker.
 * @return Owned decoded units; malformed fixture numbers throw.
 */
std::u16string unhex(std::string_view text) {
    std::u16string result;

    if (text == "-") {
        return result;
    }

    for (std::size_t i = 0; i < text.size(); i += 4) {
        result.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(i, 4)), nullptr, 16)));
    }

    return result;
}

/** Encode arbitrary UTF-16, including unpaired surrogate units.
 * @param text Borrowed field value.
 * @return Lowercase four-digit units or the empty marker.
 */
std::string hex(std::u16string_view text) {
    if (text.empty()) {
        return "-";
    }

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (const auto value : text) {
        result << std::setw(4) << static_cast<unsigned>(value);
    }

    return result.str();
}

/** Encode canonical Java doubleToLongBits observations.
 * @param value Binary64 value; NaN payloads are canonicalized.
 * @return Sixteen lowercase hexadecimal digits.
 */
std::string bits(double value) {
    auto raw = std::bit_cast<std::uint64_t>(value);

    if ((raw & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL && (raw & 0xfffffffffffffULL) != 0) {
        raw = 0x7ff8000000000000ULL;
    }

    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << raw;

    return result.str();
}

/** Encode an oracle strike vector in its supplied order.
 * @param values Borrowed vector.
 * @return Comma-separated canonical bits or the empty marker.
 */
std::string strikes(const std::vector<double> &values) {
    std::string result;

    for (const auto value : values) {
        if (!result.empty()) {
            result += ',';
        }

        result += bits(value);
    }

    return result.empty() ? "-" : result;
}

/** Write one exact TSV observation.
 * @tparam Values Stream-insertable observation cell types.
 * @param out Borrowed observation stream.
 * @param values Cells without embedded TSV delimiters.
 */
template <typename... Values> void row(std::ostream &out, const Values &...values) {
    std::size_t index = 0;
    ((out << (index++ == 0 ? "" : "\t") << values), ...);
    out << '\n';
}

/** Observe the public native API using the production oracle projection.
 * @param name Reviewed case identifier.
 * @param chains Immutable owning snapshot.
 * @return Exact TSV representation; selection errors fail the test.
 */
std::string emit(const std::string &name, const OptionChains &chains) {
    std::ostringstream out;
    row(out, "CASE", name, chains.size());

    for (const auto &[chainSymbol, chain] : chains) {
        const auto symbol = hex(chainSymbol);
        row(out, "CHAIN", name, symbol, chain->getSeries().size());
        std::size_t index = 0;

        for (const auto &series : chain->getSeries()) {
            const auto &metadata = series.getMetadata();
            row(out, "SERIES", name, symbol, index, metadata.expiration, metadata.lastTrade, bits(metadata.multiplier),
                bits(metadata.spc), hex(metadata.additionalUnderlyings), hex(metadata.mmy), hex(metadata.optionType),
                hex(metadata.expirationStyle), hex(metadata.settlementStyle), hex(metadata.cfi));

            for (const auto &[strike, profile] : series.getCalls()) {
                row(out, "CALL", name, symbol, index, bits(strike), hex(profile->getField(u"SYMBOL")));
            }

            for (const auto &[strike, profile] : series.getPuts()) {
                row(out, "PUT", name, symbol, index, bits(strike), hex(profile->getField(u"SYMBOL")));
            }

            row(out, "STRIKES", name, symbol, index, strikes(series.getStrikes()));

            for (const auto count : {0, 1, 2, 3, 20}) {
                for (const auto center :
                     {-std::numeric_limits<double>::infinity(), -0.0, 0.0, 12.0, 100.0,
                      std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
                    const auto around = series.getNStrikesAround(count, center);
                    REQUIRE(around.has_value());
                    row(out, "AROUND", name, symbol, index, count, bits(center), strikes(*around));
                }
            }

            REQUIRE(series.getNStrikesAround(-1, 0).error() == OptionChainError::INVALID_COUNT);
            row(out, "NEGATIVE", name, symbol, index, "IllegalArgumentException");
            ++index;
        }
    }

    return out.str();
}

/** Prepare one realistic typed profile for ownership/error tests.
 * @param symbol Owned profile symbol to copy.
 * @param product Product destination to copy; underlying is ABC.
 * @return Independent finite-strike call profile.
 */
InstrumentProfile option(std::u16string_view symbol = u"call", std::u16string_view product = u"/ABC") {
    InstrumentProfile result;
    REQUIRE(result.setField(u"TYPE", u"OPTION"));
    REQUIRE(result.setField(u"SYMBOL", symbol));
    REQUIRE(result.setField(u"PRODUCT", product));
    REQUIRE(result.setField(u"UNDERLYING", u"ABC"));
    REQUIRE(result.setField(u"CFI", u"OC"));
    REQUIRE(result.setDateField(u"EXPIRATION", 20000));
    REQUIRE(result.setNumericField(u"STRIKE", 10));

    return result;
}
} // namespace

TEST_CASE("production Java option grouping and selection observations") {
    const auto fixture = std::filesystem::path(ORG_TTLDTOR_QDCXX_OPTION_CHAINS_FIXTURE);
    std::ifstream input(fixture.parent_path() / "input.tsv", std::ios::binary);
    REQUIRE(input.good());
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(input, line)));
    const auto fields = split(line, '\t');
    std::vector<std::pair<std::string, std::vector<InstrumentProfile>>> cases{{"empty", {}}};

    while (std::getline(input, line)) {
        const auto cells = split(line, '\t');
        REQUIRE(cells.size() == fields.size());

        if (cases.back().first != cells[0]) {
            cases.emplace_back(cells[0], std::vector<InstrumentProfile>{});
        }

        InstrumentProfile profile;

        for (std::size_t i = 1; i < fields.size(); ++i) {
            std::u16string field(fields[i].begin(), fields[i].end());
            REQUIRE(profile.setField(field, unhex(cells[i])));
        }

        cases.back().second.push_back(std::move(profile));
    }

    std::string observed;

    for (const auto &[name, profiles] : cases) {
        const auto builder = OptionChainsBuilder::build(profiles);
        REQUIRE(builder.has_value());
        observed += emit(name, builder->getChains());
    }

    std::ifstream expectedInput(fixture, std::ios::binary);
    REQUIRE(expectedInput.good());
    const std::string expected{std::istreambuf_iterator<char>(expectedInput), {}};
    CHECK(observed == expected);
}

TEST_CASE("snapshots and copied builders own immutable independent profiles") {
    OptionChains retained;
    {
        auto input = option();
        OptionChainsBuilder builder;
        REQUIRE(builder.addProfile(input).value());
        retained = builder.getChains();
        auto copied = builder;
        REQUIRE(input.setField(u"SYMBOL", u"changed"));
        REQUIRE(builder.addProfile(input).value());
        CHECK(retained.at(u"ABC")->getSeries()[0].getCalls().at(10)->getField(u"SYMBOL") == u"call");
        CHECK(copied.getChains().at(u"ABC")->getSeries()[0].getCalls().at(10)->getField(u"SYMBOL") == u"call");
        CHECK(builder.getChains().at(u"ABC")->getSeries()[0].getCalls().at(10)->getField(u"SYMBOL") == u"changed");
    }
    CHECK(retained.at(u"ABC")->getSeries()[0].getCalls().at(10)->getField(u"SYMBOL") == u"call");
    CHECK(retained.at(u"/ABC")->getSeries()[0].getCalls().at(10) ==
          retained.at(u"ABC")->getSeries()[0].getCalls().at(10));
}

TEST_CASE("each finite budget rejects atomically and boundary admission remains usable") {
    auto input = option();

    for (const auto member : {&OptionChainLimits::profiles, &OptionChainLimits::chains, &OptionChainLimits::series,
                              &OptionChainLimits::options, &OptionChainLimits::retainedUnits,
                              &OptionChainLimits::stagingUnits, &OptionChainLimits::workUnits}) {
        OptionChainLimits limits;
        limits.*member = 0;
        OptionChainsBuilder builder(limits);
        CHECK(builder.addProfile(input).error() == OptionChainError::LIMIT_EXCEEDED);
        CHECK(builder.getChains().empty());
        CHECK(builder.getProfileCount() == 0);
        CHECK(builder.getWorkUnits() == 0);
    }

    OptionChainLimits limits;
    limits.chains = 1;
    OptionChainsBuilder dual(limits);
    CHECK(dual.addProfile(input).error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(dual.getChains().empty());
    REQUIRE(input.setField(u"PRODUCT", u"ABC"));
    REQUIRE(dual.addProfile(input).value());
    CHECK(dual.getChains().size() == 1);
    limits = {};
    limits.options = 2;
    OptionChainsBuilder bounded(limits);
    auto original = option();
    REQUIRE(bounded.addProfile(original).value());
    const auto snapshot = bounded.getChains();
    auto extra = original;
    REQUIRE(extra.setNumericField(u"STRIKE", 20));
    CHECK(bounded.addProfile(extra).error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(bounded.getChains() == snapshot);
    CHECK(bounded.getProfileCount() == 1);
    REQUIRE(original.setField(u"SYMBOL", u"replacement"));
    REQUIRE(bounded.addProfile(original).value());
    CHECK(bounded.getChains().at(u"ABC")->getSeries()[0].getCalls().at(10)->getField(u"SYMBOL") == u"replacement");
    limits = {};
    limits.profiles = 1;
    const std::array profiles{option(), option()};
    CHECK(OptionChainsBuilder::build(profiles, limits).error() == OptionChainError::LIMIT_EXCEEDED);
}

TEST_CASE("profile access limits are errors without partial publication") {
    ProfileLimits limits;
    limits.nameUnits = 5;
    InstrumentProfile input(limits);
    REQUIRE(input.setField(u"TYPE", u"OPTION"));
    REQUIRE(input.setField(u"CFI", u"OC"));
    // Text access is unrestricted, but source numeric/date access rejects names beyond its configured bound.
    OptionChainsBuilder builder;
    CHECK_FALSE(builder.addProfile(input).value());
    limits.nameUnits = 7;
    InstrumentProfile inaccessible(limits);
    REQUIRE(inaccessible.setField(u"TYPE", u"OPTION"));
    REQUIRE(inaccessible.setField(u"PRODUCT", u"A"));
    REQUIRE(inaccessible.setField(u"CFI", u"OC"));
    CHECK(builder.addProfile(inaccessible).error() == OptionChainError::PROFILE_ACCESS);
    CHECK(builder.getChains().empty());
    CHECK(builder.getProfileCount() == 1);
}

TEST_CASE("allocation failure at every admission allocation leaves both chains unchanged") {
    auto original = option();
    auto replacement = option(u"replacement");
    REQUIRE(replacement.setNumericField(u"STRIKE", 20));
    bool reachedSuccess = false;
    std::size_t failures = 0;

    for (std::ptrdiff_t failAt = 0; failAt < 1000 && !reachedSuccess; ++failAt) {
        OptionChainsBuilder builder;
        REQUIRE(builder.addProfile(original).value());
        const auto snapshot = builder.getChains();
        const auto profiles = builder.getProfileCount();
        const auto work = builder.getWorkUnits();
        failAllocationAfter = failAt;
        try {
            const auto added = builder.addProfile(replacement);
            failAllocationAfter = -1;
            REQUIRE(added.has_value());
            REQUIRE(*added);
            reachedSuccess = true;
        } catch (const std::bad_alloc &) {
            failAllocationAfter = -1;
            ++failures;
            CHECK(builder.getChains() == snapshot);
            CHECK(builder.getProfileCount() == profiles);
            CHECK(builder.getWorkUnits() == work);
            REQUIRE(builder.addProfile(replacement).value());
        } catch (...) {
            failAllocationAfter = -1;
            throw;
        }
    }

    CHECK(reachedSuccess);
    CHECK(failures > 10);
}

TEST_CASE("binary64 total ordering handles signed zero and all NaN representations") {
    const auto nan = std::bit_cast<double>(std::uint64_t{0xfff0000000000001ULL});
    OptionSeriesMetadata left;
    OptionSeriesMetadata right;
    left.multiplier = -0.0;
    CHECK(left.compareTo(right) < 0);
    right.multiplier = std::numeric_limits<double>::infinity();
    left.multiplier = nan;
    CHECK(left.compareTo(right) > 0);
    right.multiplier = std::numeric_limits<double>::quiet_NaN();
    CHECK(left.compareTo(right) == 0);
    CHECK(OptionStrikeLess{}(-0.0, 0.0));
    CHECK_FALSE(OptionStrikeLess{}(0.0, -0.0));
    CHECK_FALSE(OptionStrikeLess{}(nan, right.multiplier));
}

TEST_CASE("builder move and assignment preserve coherent reusable source and target") {
    auto input = option();
    OptionChainsBuilder source;
    REQUIRE(source.addProfile(input).value());
    auto moved = std::move(source);
    CHECK(source.getChains().empty());
    CHECK(source.getProfileCount() == 0);
    CHECK(source.getWorkUnits() == 0);
    REQUIRE(source.addProfile(input).value());
    OptionChainsBuilder assigned;
    assigned = moved;
    CHECK(assigned.getChains() == moved.getChains());
    assigned = std::move(source);
    CHECK(source.getChains().empty());
    CHECK(source.getProfileCount() == 0);
    REQUIRE(source.addProfile(input).value());
    bool completed = false;

    for (std::ptrdiff_t failAt = 0; failAt < 100 && !completed; ++failAt) {
        OptionChainsBuilder target;
        const auto before = target.getChains();
        failAllocationAfter = failAt;
        try {
            target = moved;
            failAllocationAfter = -1;
            completed = true;
            CHECK(target.getChains() == moved.getChains());
        } catch (const std::bad_alloc &) {
            failAllocationAfter = -1;
            CHECK(target.getChains() == before);
            CHECK(target.getProfileCount() == 0);
            CHECK(target.getWorkUnits() == 0);
            REQUIRE(target.addProfile(input).value());
        } catch (...) {
            failAllocationAfter = -1;
            throw;
        }
    }

    CHECK(completed);
}

TEST_CASE("retained staging and work ceilings include exact boundary and replacement charges") {
    auto input = option();
    const auto units = input.retainedUnits();
    const auto retained = 7 + 4 + 2 * units; // Product/underlying symbol units, normalized CFI per series, two slots.
    OptionChainLimits limits;
    limits.retainedUnits = retained;
    limits.stagingUnits = retained + 7 + 3 * units;
    limits.workUnits = 1 + units;
    OptionChainsBuilder exact(limits);
    REQUIRE(exact.addProfile(input).value());
    CHECK(exact.getWorkUnits() == 1 + units);
    const auto before = exact.getChains();
    CHECK(exact.addProfile(input).error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(exact.getChains() == before);
    CHECK(exact.getProfileCount() == 1);
    limits.workUnits = OptionChainLimits{}.workUnits;
    --limits.retainedUnits;
    OptionChainsBuilder tooSmall(limits);
    CHECK(tooSmall.addProfile(input).error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(tooSmall.getChains().empty());
    ++limits.retainedUnits;
    --limits.stagingUnits;
    OptionChainsBuilder tooLittleStaging(limits);
    CHECK(tooLittleStaging.addProfile(input).error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(tooLittleStaging.getChains().empty());
    limits = {};
    limits.profiles = 1;
    InstrumentProfile ignored;
    OptionChainsBuilder boundedIgnored(limits);
    CHECK_FALSE(boundedIgnored.addProfile(ignored).value());
    CHECK(boundedIgnored.getWorkUnits() == 1);
    CHECK(boundedIgnored.addProfile(ignored).error() == OptionChainError::LIMIT_EXCEEDED);
}

TEST_CASE("aggregate replacement subtracts all displaced profiles before retained ceiling") {
    for (const bool largeProduct : {false, true}) {
        auto product = option(u"", u"A");
        REQUIRE(product.setField(u"UNDERLYING", u""));
        REQUIRE(product.setDateField(u"EXPIRATION", 1));
        REQUIRE(product.setNumericField(u"STRIKE", 0));
        auto underlying = product;
        REQUIRE(underlying.setField(u"PRODUCT", u""));
        REQUIRE(underlying.setField(u"UNDERLYING", u"B"));
        REQUIRE((largeProduct ? product : underlying).setField(u"DESCRIPTION", std::u16string(200, u'x')));
        auto incoming = product;
        REQUIRE(incoming.setField(u"UNDERLYING", u"B"));
        REQUIRE(incoming.setField(u"DESCRIPTION", std::u16string(20, u'x')));
        OptionChainLimits limits;
        limits.retainedUnits = 244; // Independently reproduced old aggregate; replacement is 86 units.
        OptionChainsBuilder builder(limits);
        REQUIRE(builder.addProfile(product).value());
        REQUIRE(builder.addProfile(underlying).value());
        const auto snapshot = builder.getChains();
        const auto beforeWork = builder.getWorkUnits();
        const auto added = builder.addProfile(incoming);
        REQUIRE(added.has_value());
        REQUIRE(*added);
        CHECK(builder.getProfileCount() == 3);
        CHECK(builder.getWorkUnits() == beforeWork + 291);

        for (const auto symbol : {u"A", u"B"}) {
            const auto &slot = builder.getChains().at(symbol)->getSeries()[0].getCalls().at(0);
            CHECK(slot->getField(u"DESCRIPTION").size() == 20);
        }

        CHECK(snapshot.at(u"A")->getSeries()[0].getCalls().at(0)->getField(u"DESCRIPTION").size() ==
              (largeProduct ? 200 : 0));
        CHECK(snapshot.at(u"B")->getSeries()[0].getCalls().at(0)->getField(u"DESCRIPTION").size() ==
              (largeProduct ? 0 : 200));
        CHECK(builder.getChains().at(u"A")->getSeries()[0].getCalls().at(0) ==
              builder.getChains().at(u"B")->getSeries()[0].getCalls().at(0));
    }

    auto product = option(u"", u"A");
    REQUIRE(product.setField(u"UNDERLYING", u""));
    REQUIRE(product.setDateField(u"EXPIRATION", 1));
    REQUIRE(product.setNumericField(u"STRIKE", 0));
    auto underlying = product;
    REQUIRE(underlying.setField(u"PRODUCT", u""));
    REQUIRE(underlying.setField(u"UNDERLYING", u"B"));
    auto incoming = product;
    REQUIRE(incoming.setField(u"UNDERLYING", u"B"));
    REQUIRE(incoming.setField(u"DESCRIPTION", std::u16string(200, u'x')));

    for (const auto retainedLimit : {445U, 446U}) {
        OptionChainLimits limits;
        limits.retainedUnits = retainedLimit;
        OptionChainsBuilder builder(limits);
        REQUIRE(builder.addProfile(product).value());
        REQUIRE(builder.addProfile(underlying).value());
        const auto before = builder.getChains();
        const auto beforeWork = builder.getWorkUnits();
        const auto added = builder.addProfile(incoming);

        if (retainedLimit == 446) {
            REQUIRE(added.has_value());
            CHECK(*added);
            CHECK(builder.getProfileCount() == 3);
        } else {
            REQUIRE_FALSE(added.has_value());
            CHECK(added.error() == OptionChainError::LIMIT_EXCEEDED);
            CHECK(builder.getChains() == before);
            CHECK(builder.getProfileCount() == 2);
            CHECK(builder.getWorkUnits() == beforeWork);
        }
    }
}

TEST_CASE("staging ceiling bounds long symbol graphs and insertion temporaries") {
    const std::u16string symbol(4096, u'A');
    auto original = option(u"", symbol);
    REQUIRE(original.setField(u"UNDERLYING", u""));
    REQUIRE(original.setDateField(u"EXPIRATION", 1));
    REQUIRE(original.setNumericField(u"STRIKE", 0));
    auto replacement = original;
    REQUIRE(replacement.setField(u"DESCRIPTION", u"x"));
    CHECK(original.retainedUnits() == 4114);
    CHECK(replacement.retainedUnits() == 4115);
    OptionChainLimits unsafe;
    unsafe.stagingUnits = 20540; // Independently reproduced v1 admission below six live 4096-unit symbol copies.
    OptionChainsBuilder rejected(unsafe);
    const auto first = rejected.addProfile(original);
    REQUIRE_FALSE(first.has_value());
    CHECK(first.error() == OptionChainError::LIMIT_EXCEEDED);
    CHECK(rejected.getChains().empty());
    const std::size_t exactReplacement = 45174; // 2*8212 + 8213 + 4096 + 4096 + 3*4115.

    for (const auto cap : {exactReplacement - 1, exactReplacement}) {
        OptionChainLimits limits;
        limits.stagingUnits = cap;
        OptionChainsBuilder builder(limits);
        REQUIRE(builder.addProfile(original).value());
        const auto beforeWork = builder.getWorkUnits();
        const auto added = builder.addProfile(replacement);

        if (cap == exactReplacement) {
            REQUIRE(added.has_value());
            CHECK(*added);
            CHECK(builder.getProfileCount() == 2);
            CHECK(builder.getChains().at(symbol)->getSeries()[0].getCalls().at(0)->getField(u"DESCRIPTION") == u"x");
        } else {
            REQUIRE_FALSE(added.has_value());
            CHECK(added.error() == OptionChainError::LIMIT_EXCEEDED);
            CHECK(builder.getProfileCount() == 1);
            CHECK(builder.getWorkUnits() == beforeWork);
            CHECK(builder.getChains().at(symbol)->getSeries()[0].getCalls().at(0)->getField(u"DESCRIPTION").empty());
        }
    }
}
