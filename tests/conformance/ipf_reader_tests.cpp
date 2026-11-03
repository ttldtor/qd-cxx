// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/ipf/instrument_profile_reader.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdlib>
#include <doctest/doctest.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace {
std::atomic<bool> failNextOwnedAllocation{false};
}

/** Replace allocation only in this test executable to exercise real parser allocation failure.
 * @param bytes Requested storage bytes; zero still receives nonnull allocatable storage.
 * @return Storage released through the matching test replacement delete.
 */
void *operator new(std::size_t bytes) {
    if (failNextOwnedAllocation.exchange(false)) {
        throw std::bad_alloc{};
    }

    if (auto memory = std::malloc(bytes == 0 ? 1 : bytes)) {
        return memory;
    }

    throw std::bad_alloc{};
}

/** Release storage from the test executable's replacement allocation.
 * @param memory Previously allocated storage, or null.
 */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** Release sized storage from the test executable's replacement allocation.
 * @param memory Previously allocated storage, or null.
 * @param bytes Original size; the C allocator does not require it.
 */
void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

using namespace org::ttldtor::qdcxx::ipf;

namespace {
std::string readBytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());

    return {std::istreambuf_iterator<char>(input), {}};
}

std::u16string decode(std::string_view text) {
    REQUIRE(text.size() % 4 == 0);
    std::u16string value;

    for (std::size_t index = 0; index < text.size(); index += 4) {
        unsigned unit = 0;
        auto parsed = std::from_chars(text.data() + index, text.data() + index + 4, unit, 16);
        REQUIRE(parsed.ec == std::errc{});
        value.push_back(static_cast<char16_t>(unit));
    }

    return value;
}

std::string hex(std::u16string_view value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');

    for (auto unit : value) {
        output << std::setw(4) << static_cast<unsigned>(unit);
    }

    return output.str();
}

std::string describe(const InstrumentProfile &profile) {
    std::string result = "PROFILE";

    for (auto field : standardFields()) {
        auto text = profile.getField(field.name);

        if (!text.empty()) {
            result += "\t" + hex(field.name) + "=" + hex(text);
        }
    }

    for (auto &name : profile.customFieldNames()) {
        result += "\t" + hex(name) + "=" + hex(profile.getField(name));
    }

    return result;
}

struct Observed {
    std::vector<std::string> events;
    std::optional<IpfDiagnostic> error;
    IpfReadProgress progress;
    bool finished = false;
};

Observed parse(std::string_view input, std::size_t split, IpfReadOptions options = {}, IpfReadWork quota = {}) {
    InstrumentProfileParser parser(options);
    Observed result;
    auto collect = [&](IpfReadStep step) {
        if (step.item) {
            result.events.push_back(step.item->kind == IpfItemKind::PROFILE ? describe(*step.item->profile)
                                    : step.item->kind == IpfItemKind::FLUSH ? "FLUSH"
                                                                            : "COMPLETE");
        }

        return step.status;
    };
    std::size_t position = 0, calls = 0;

    for (auto end : {split, input.size()}) {
        while (true) {
            REQUIRE(++calls < 10000);
            auto work = quota;
            auto chunk = std::as_bytes(std::span(input.data() + position, end - position));
            auto step = parser.feed(chunk, work);
            CHECK(step.accepted <= chunk.size());
            position += step.accepted;
            auto status = collect(std::move(step));

            if (status == IpfReadStatus::FAILED) {
                result.error = parser.diagnostic();
                result.progress = parser.progress();

                return result;
            }

            if (status == IpfReadStatus::NEED_INPUT) {
                REQUIRE(position == end);
                break;
            }
        }
    }

    while (true) {
        REQUIRE(++calls < 10000);
        auto work = quota;
        auto status = collect(parser.finish(work));

        if (status == IpfReadStatus::FAILED) {
            result.error = parser.diagnostic();
            break;
        }

        if (status == IpfReadStatus::FINISHED) {
            result.finished = true;
            break;
        }
    }

    result.progress = parser.progress();

    return result;
}

IpfReadError expectedError(std::u16string_view text) {
    if (text.find(u"unquoted field has quote") != text.npos) {
        return IpfReadError::UNQUOTED_QUOTE;
    }

    if (text.find(u"unpaired quote") != text.npos) {
        return IpfReadError::UNPAIRED_QUOTE;
    }

    if (text.find(u"terminating quote") != text.npos) {
        return IpfReadError::UNTERMINATED_QUOTE;
    }

    if (text.find(u"undefined format") != text.npos) {
        return IpfReadError::UNDEFINED_FORMAT;
    }

    if (text.find(u"forbidden format") != text.npos) {
        return IpfReadError::FORBIDDEN_FORMAT;
    }

    if (text.find(u"wrong number of fields") != text.npos) {
        return IpfReadError::FIELD_COUNT;
    }

    return IpfReadError::PROFILE_CONVERSION;
}

std::vector<std::string> lines(std::string_view text) {
    std::istringstream input{std::string(text)};
    std::vector<std::string> result;
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        result.push_back(line);
    }

    return result;
}
} // namespace

TEST_CASE("Pinned Java local IPF grammar and exact prefix outcomes survive every input split") {
    auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_IPF_READER_FIXTURES);
    auto cases = lines(readBytes(root / "cases.tsv"));
    REQUIRE(cases.size() == 42);

    for (auto &entry : cases) {
        auto id = entry.substr(0, entry.find('\t'));
        CAPTURE(id);
        auto input = readBytes(root / (id + ".ipf"));
        auto oracle = lines(readBytes(root / (id + ".tsv")));
        std::vector<std::string> expected;
        std::optional<IpfReadError> error;

        for (auto &line : oracle) {
            if (line.starts_with("PROFILE") || line == "FLUSH" || line == "COMPLETE") {
                expected.push_back(line);
            } else if (line.starts_with("ERROR\t")) {
                error = expectedError(decode(line.substr(6)));
            }
        }

        for (std::size_t split = 0; split <= input.size(); ++split) {
            CAPTURE(split);
            auto actual = parse(input, split);
            CHECK(actual.events == expected);
            REQUIRE(actual.error.has_value() == error.has_value());

            if (error) {
                CHECK(actual.error->error == *error);
                CHECK_FALSE(actual.finished);
            } else {
                CHECK(actual.finished);
                CHECK(actual.progress.inputBytes == input.size());
            }
        }

        auto tiny = parse(input, input.size() / 2, {}, {1, 1, 1, 1});
        CHECK(tiny.events == expected);
        CHECK(tiny.error.has_value() == error.has_value());
        std::istringstream stream(input);
        auto complete = readInstrumentProfiles(stream);
        std::vector<std::string> profiles;

        for (const auto &profile : complete.profiles) {
            profiles.push_back(describe(profile));
        }

        auto expectedProfiles = expected;
        std::erase_if(expectedProfiles, [](auto &event) {
            return !event.starts_with("PROFILE");
        });
        CHECK(profiles == expectedProfiles);
        CHECK(complete.error.has_value() == error.has_value());
        CHECK(complete.completionSeen == (std::ranges::find(expected, "COMPLETE") != expected.end()));
        CHECK(complete.complete == (!error && complete.completionSeen));
        CHECK(complete.finished == !error);
    }
}

TEST_CASE("Parser work exhaustion accepts no input at zero and preserves owned delivered profiles") {
    InstrumentProfileParser parser;
    std::string source = "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\n";
    auto bytes = std::as_bytes(std::span(source));
    IpfReadWork zero{0, 0, 0, 0};
    auto paused = parser.feed(bytes, zero);
    CHECK(paused.status == IpfReadStatus::WORK_EXHAUSTED);
    CHECK(paused.accepted == 0);
    CHECK(parser.progress().inputBytes == 0);
    std::optional<InstrumentProfile> owned;
    std::size_t accepted = 0;

    while (accepted < bytes.size()) {
        IpfReadWork work;
        auto step = parser.feed(bytes.subspan(accepted), work);
        accepted += step.accepted;

        if (step.item && step.item->profile) {
            owned = std::move(step.item->profile);
        }

        REQUIRE(step.status != IpfReadStatus::FAILED);
    }

    REQUIRE(owned);
    source.assign(source.size(), 'x');
    CHECK(owned->getSymbol() == u"A");
    IpfReadWork work;

    while (parser.finish(work).status == IpfReadStatus::ITEM_READY) {
        work = {};
    }

    auto after = parser.feed({}, work);
    CHECK(after.status == IpfReadStatus::FINISHED);
    CHECK(after.accepted == 0);
}

TEST_CASE("Completion policy distinguishes clean EOF marker evidence and later failures") {
    IpfReadOptions options;
    options.completion = IpfCompletionPolicy::REQUIRE;
    std::istringstream incomplete("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n");
    auto missing = readInstrumentProfiles(incomplete, options);
    REQUIRE(missing.error);
    CHECK(missing.error->error == IpfReadError::MISSING_COMPLETE);
    REQUIRE(missing.profiles.size() == 1);
    CHECK_FALSE(missing.finished);
    CHECK_FALSE(missing.complete);
    std::istringstream later("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\nBROKEN\n");
    auto failed = readInstrumentProfiles(later, options);
    REQUIRE(failed.error);
    CHECK(failed.completionSeen);
    CHECK_FALSE(failed.complete);
    CHECK_FALSE(failed.finished);
    CHECK(failed.error->recordLine == 4);
    CHECK(failed.error->position.byteOffset == 40);
    std::istringstream exact("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE");
    exact.exceptions(std::ios::badbit | std::ios::failbit);
    auto complete = readInstrumentProfiles(exact, options);
    REQUIRE_FALSE(complete.error);
    CHECK(complete.complete);
}

TEST_CASE("Finite reader input field row header and output bounds preserve committed prefixes") {
    std::string source = "#STOCK::=TYPE,SYMBOL\nSTOCK,A\nSTOCK,B\n##COMPLETE\n";
    auto check = [&](IpfReadOptions options, IpfReadError error, std::size_t prefix) {
        std::istringstream input(source);
        auto result = readInstrumentProfiles(input, options);
        REQUIRE(result.error);
        CHECK(result.error->error == error);
        CHECK(result.profiles.size() == prefix);

        if (prefix != 0) {
            CHECK(result.profiles[0].getSymbol() == u"A");
        }
    };
    IpfReadOptions options;
    options.limits.inputBytes = 10;
    check(options, IpfReadError::INPUT_LIMIT, 0);
    options = {};
    options.limits.fieldUnits = 4;
    check(options, IpfReadError::FIELD_LIMIT, 0);
    options = {};
    options.limits.recordUnits = 8;
    check(options, IpfReadError::RECORD_LIMIT, 0);
    options = {};
    options.limits.fieldsPerRecord = 1;
    check(options, IpfReadError::RECORD_LIMIT, 0);
    options = {};
    options.limits.records = 2;
    check(options, IpfReadError::RECORD_LIMIT, 1);
    options = {};
    options.limits.formats = 0;
    check(options, IpfReadError::FORMAT_LIMIT, 0);
    options = {};
    options.limits.headerUnits = 10;
    check(options, IpfReadError::HEADER_LIMIT, 0);
    options = {};
    options.limits.profiles = 1;
    check(options, IpfReadError::OUTPUT_LIMIT, 1);
    options = {};
    options.limits.profileUnits = 6;
    check(options, IpfReadError::OUTPUT_LIMIT, 1);
    options = {};
    options.limits.profile.valueUnits = 4;
    check(options, IpfReadError::PROFILE_CONVERSION, 0);
    options = {};
    options.limits.headerUnits = 11;
    auto replaced = parse("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n#STOCK::=TYPE,SYMBOL,X\n", 0, options);
    REQUIRE(replaced.error);
    CHECK(replaced.error->error == IpfReadError::HEADER_LIMIT);
    CHECK(replaced.progress.formats == 1);
    CHECK(replaced.progress.headerUnits == 11);
    CHECK(replaced.progress.profiles == 1);
}

namespace {
class ThrowingInput : public std::streambuf {
    std::string text_;
    std::size_t offset_ = 0;
    bool allocation_;

    public:
    explicit ThrowingInput(bool allocation) : text_("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n"), allocation_(allocation) {
    }

    int_type underflow() override {
        if (offset_ == text_.size()) {
            if (allocation_) {
                throw std::bad_alloc{};
            }

            throw std::runtime_error("owned injected read failure");
        }

        return traits_type::to_int_type(text_[offset_]);
    }

    int_type uflow() override {
        auto value = underflow();
        ++offset_;

        return value;
    }
};
} // namespace

TEST_CASE("Local driver maps throwing stream failures and validates quotas before input") {
    for (bool allocation : {false, true}) {
        for (bool rethrowBadbit : {false, true}) {
            ThrowingInput buffer(allocation);
            std::istream input(&buffer);

            if (rethrowBadbit) {
                input.exceptions(std::ios::badbit);
            }

            const auto exceptionMask = input.exceptions();
            CHECK(exceptionMask == (rethrowBadbit ? std::ios::badbit : std::ios::goodbit));
            auto result = readInstrumentProfiles(input);
            REQUIRE(result.error);
            const auto expected =
                allocation && rethrowBadbit ? IpfReadError::ALLOCATION_FAILURE : IpfReadError::IO_ERROR;
            CHECK(result.error->error == expected);
            REQUIRE(result.profiles.size() == 1);
            CHECK(result.profiles[0].getSymbol() == u"A");
            CHECK(result.acceptedBytes == std::string_view("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n").size());
            CHECK(result.error->position.byteOffset == result.acceptedBytes);
            CHECK_FALSE(result.finished);
            CHECK_FALSE(result.complete);
            CHECK(input.bad());
            CHECK(input.exceptions() == exceptionMask);
        }
    }

    IpfReadOptions options;
    options.turnWork.decodedUnits = 0;
    std::istringstream input("not consumed");
    auto before = input.tellg();
    auto result = readInstrumentProfiles(input, options);
    REQUIRE(result.error);
    CHECK(result.error->error == IpfReadError::INVALID_OPTIONS);
    CHECK(input.tellg() == before);
    auto missing = readInstrumentProfilesFile(std::filesystem::path(ORG_TTLDTOR_QDCXX_IPF_READER_FIXTURES) /
                                              "missing-owned-case.ipf");
    REQUIRE(missing.error);
    CHECK(missing.error->error == IpfReadError::IO_ERROR);
}

TEST_CASE("Binary local file reading preserves quoted CRLF without taking borrowed stream ownership") {
    auto path = std::filesystem::path(ORG_TTLDTOR_QDCXX_IPF_READER_FIXTURES) / "multiline.ipf";
    auto result = readInstrumentProfilesFile(path);
    REQUIRE_FALSE(result.error);
    REQUIRE(result.profiles.size() == 1);
    CHECK(result.profiles[0].getDescription() == u"line1\nline2\rline3\r\nline4");
    CHECK(result.complete);
}

TEST_CASE("Logical diagnostic byte line field and retained-prefix positions are exact") {
    auto root = std::filesystem::path(ORG_TTLDTOR_QDCXX_IPF_READER_FIXTURES);
    auto quote = readBytes(root / "unquoted-quote.ipf");
    auto quoteOffset = quote.find("A\"B") + 1;
    auto invalidQuote = parse(quote, quote.size());
    REQUIRE(invalidQuote.error);
    CHECK(invalidQuote.error->position.byteOffset == quoteOffset);
    CHECK(invalidQuote.progress.inputBytes == quoteOffset + 1);
    CHECK(invalidQuote.error->recordLine == 3);
    CHECK(invalidQuote.error->fieldLine == 3);
    CHECK(invalidQuote.error->position.line == 3);
    CHECK(invalidQuote.error->position.field == 1);
    REQUIRE(invalidQuote.events.size() == 1);

    auto truncated = readBytes(root / "quote-unfinished.ipf");
    auto unfinished = parse(truncated, 0);
    REQUIRE(unfinished.error);
    CHECK(unfinished.error->position.byteOffset == truncated.size());
    CHECK(unfinished.error->fieldLine == 3);
    CHECK(unfinished.error->position.line == 4);
    CHECK(unfinished.error->position.field == 1);
    REQUIRE(unfinished.events.size() == 1);

    auto badNumber = readBytes(root / "invalid-number-first.ipf");
    auto numberOffset = badNumber.find("bad");
    auto number = parse(badNumber, badNumber.size());
    REQUIRE(number.error);
    CHECK(number.error->error == IpfReadError::PROFILE_CONVERSION);
    CHECK(number.error->profileError == ProfileError::INVALID_NUMBER);
    CHECK(number.error->position.byteOffset == numberOffset);
    CHECK(number.progress.inputBytes == numberOffset + 4);
    CHECK(number.error->recordLine == 2);
    CHECK(number.error->position.field == 1);

    IpfReadOptions options;
    options.limits.inputBytes = 10;
    InstrumentProfileParser parser(options);
    IpfReadWork work;
    auto bytes = std::as_bytes(std::span(quote));
    auto failed = parser.feed(bytes, work);
    CHECK(failed.status == IpfReadStatus::FAILED);
    CHECK(failed.accepted == 10);
    REQUIRE(parser.diagnostic());
    CHECK(parser.diagnostic()->position.byteOffset == 10);
    CHECK(work.inputBytes == 4086);
    CHECK(work.decodedUnits == 4086);
    auto before = parser.progress();
    work = {};
    auto terminal = parser.feed(bytes, work);
    CHECK(terminal.accepted == 0);
    CHECK(terminal.status == IpfReadStatus::FAILED);
    CHECK(parser.progress().inputBytes == before.inputBytes);
}

TEST_CASE("Semantic start positions keep byte line and field context consistent after newline examination") {
    std::string source = "#STOCK::=TYPE,SYMBOL,EXPIRATION\nSTOCK,A,not-date\n";
    auto invalid = parse(source, 0);
    REQUIRE(invalid.error);
    CHECK(invalid.error->error == IpfReadError::PROFILE_CONVERSION);
    CHECK(invalid.error->position.byteOffset == source.find("not-date"));
    CHECK(invalid.error->position.line == 2);
    CHECK(invalid.error->position.field == 2);
    auto undefined = parse("\r\r\n\nMISSING\n", 1);
    REQUIRE(undefined.error);
    CHECK(undefined.error->position.byteOffset == 4);
    CHECK(undefined.error->position.line == 4);
    CHECK(undefined.error->position.field == 0);
    CHECK(undefined.error->recordLine == 4);
}

TEST_CASE("A real parser allocation failure preserves delivered profile and retained header ownership") {
    InstrumentProfileParser parser;
    std::string prefix = "#STOCK::=TYPE,SYMBOL\nSTOCK,A\n";
    IpfReadWork work;
    auto initial = parser.feed(std::as_bytes(std::span(prefix)), work);
    REQUIRE(initial.status == IpfReadStatus::ITEM_READY);
    REQUIRE(initial.item);
    REQUIRE(initial.item->profile);
    auto owned = std::move(*initial.item->profile);
    auto before = parser.progress();
    std::string next = "STOCK," + std::string(1024, 'B') + "\n";
    auto bytes = std::as_bytes(std::span(next));
    IpfReadStep failed;

    struct FaultScope {
        FaultScope() {
            failNextOwnedAllocation = true;
        }

        ~FaultScope() {
            failNextOwnedAllocation = false;
        }
    };

    work = {};
    {
        FaultScope fault;
        failed = parser.feed(bytes, work);
    }
    REQUIRE(failed.status == IpfReadStatus::FAILED);
    REQUIRE(parser.diagnostic());
    CHECK(parser.diagnostic()->error == IpfReadError::ALLOCATION_FAILURE);
    CHECK(failed.accepted > 0);
    CHECK(failed.accepted < bytes.size());
    CHECK_FALSE(failed.item);
    CHECK(owned.getSymbol() == u"A");
    CHECK(parser.progress().profiles == before.profiles);
    CHECK(parser.progress().records == before.records);
    CHECK(parser.progress().formats == before.formats);
    CHECK(parser.progress().headerUnits == before.headerUnits);
    work = {};
    auto terminal = parser.feed(bytes.subspan(failed.accepted), work);
    CHECK(terminal.status == IpfReadStatus::FAILED);
    CHECK(terminal.accepted == 0);
}
