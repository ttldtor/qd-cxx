// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <doctest/doctest.h>
#include <fstream>
#include <limits>
#include <new>
#include <qd/ipf/instrument_profile_reader.hpp>
#include <qd/ipf/instrument_profile_writer.hpp>
#include <sstream>
#include <stdexcept>

namespace {
std::atomic<bool> failNextOwnedAllocation = false;
}

/** Replace allocation only in this test executable for a real preparation failure.
 * @param size Requested allocation bytes.
 * @return Owned malloc storage, or throws bad_alloc when the scoped fault is armed.
 */
void *operator new(std::size_t size) {
    if (failNextOwnedAllocation.exchange(false))
        throw std::bad_alloc{};

    if (void *p = std::malloc(size ? size : 1))
        return p;

    throw std::bad_alloc{};
}

/** Free storage owned by this test executable.
 * @param p Storage previously returned by the replacement allocation.
 */
void operator delete(void *p) noexcept {
    std::free(p);
}

/** Free sized storage owned by this test executable.
 * @param p Storage previously returned by the replacement allocation.
 * @param size Allocation byte count, unused by malloc/free.
 */
void operator delete(void *p, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(p);
}

using namespace org::ttldtor::qdcxx::ipf;

namespace {
std::string bytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());

    return {std::istreambuf_iterator<char>(in), {}};
}

std::vector<std::string> lines(const std::filesystem::path &path) {
    std::istringstream in(bytes(path));
    std::vector<std::string> result;

    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        result.push_back(line);
    }

    return result;
}

std::u16string unhex(std::string_view text) {
    REQUIRE(text.size() % 4 == 0);
    std::u16string out;

    for (std::size_t i = 0; i < text.size(); i += 4)
        out.push_back(static_cast<char16_t>(std::stoul(std::string(text.substr(i, 4)), nullptr, 16)));

    return out;
}

std::vector<InstrumentProfile> profiles(const std::filesystem::path &path) {
    std::vector<InstrumentProfile> result;

    for (const auto &line : lines(path)) {
        result.emplace_back();
        std::istringstream fields(line);
        std::string token;
        REQUIRE(static_cast<bool>(std::getline(fields, token, '\t')));
        REQUIRE(token == "PROFILE");

        while (std::getline(fields, token, '\t')) {
            auto equal = token.find('=');
            REQUIRE(equal != std::string::npos);
            REQUIRE(result.back().setField(unhex(std::string_view(token).substr(0, equal)),
                                           unhex(std::string_view(token).substr(equal + 1))));
        }
    }

    return result;
}

InstrumentProfile basic() {
    InstrumentProfile p;
    REQUIRE(p.setType(u"STOCK"));
    REQUIRE(p.setSymbol(u"IBM"));

    return p;
}

void sameText(const InstrumentProfile &a, const InstrumentProfile &b) {
    for (const auto &f : standardFields())
        CHECK(a.getField(f.name) == b.getField(f.name));

    REQUIRE(a.customFieldNames() == b.customFieldNames());

    for (const auto &n : a.customFieldNames())
        CHECK(a.getField(n) == b.getField(n));
}

class FailingOutput : public std::streambuf {
    public:
    std::string text;
    std::size_t failAfter = std::numeric_limits<std::size_t>::max();
    int failFlush = 0, flushes = 0, writes = 0;
    bool throwFailure = false, allocation = false;

    std::streamsize xsputn(const char *data, std::streamsize count) override {
        ++writes;
        const auto accepted = std::min(static_cast<std::size_t>(count), failAfter - text.size());
        text.append(data, accepted);

        if (accepted < static_cast<std::size_t>(count) && throwFailure) {
            if (allocation)
                throw std::bad_alloc{};

            throw std::runtime_error("owned injected stream write failure");
        }

        return static_cast<std::streamsize>(accepted);
    }

    int sync() override {
        return ++flushes == failFlush ? -1 : 0;
    }
};

class AllocationFault {
    public:
    AllocationFault() {
        failNextOwnedAllocation = true;
    }

    ~AllocationFault() {
        failNextOwnedAllocation = false;
    }
};

struct TempFile {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("qdcxx-ipf-writer-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".ipf");

    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};
} // namespace

TEST_CASE("Pinned Java writer bytes round trip canonical fields through the native reader") {
    const std::filesystem::path root = ORG_TTLDTOR_QDCXX_IPF_WRITER_FIXTURES;
    std::size_t cases = 0, accepted = 0;
    const auto nativeDirectory = std::getenv("ORG_TTLDTOR_QDCXX_IPF_WRITER_ORACLE_OUTPUT");

    if (nativeDirectory)
        std::filesystem::create_directories(nativeDirectory);

    for (const auto &line : lines(root / "cases.tsv")) {
        std::istringstream row(line);
        std::string id, status;
        row >> id >> status;
        CAPTURE(id);
        auto input = profiles(root / (id + ".input.tsv"));
        const auto before = input;
        auto composed = composeInstrumentProfiles(input);
        CHECK(input == before);
        ++cases;

        if (status != "OK") {
            REQUIRE_FALSE(composed);
            const auto expected = status == "INVALID_TYPE"     ? IpfWriteError::INVALID_TYPE
                                  : status == "REMOVED_FIELDS" ? IpfWriteError::REMOVED_FIELDS
                                                               : IpfWriteError::UNREPRESENTABLE_UTF16;
            CHECK(composed.error().error == expected);
            continue;
        }

        ++accepted;
        REQUIRE(composed);
        CHECK(*composed == bytes(root / (id + ".ipf")));
        std::istringstream stream(*composed);
        auto read = readInstrumentProfiles(stream);
        REQUIRE_FALSE(read.error);
        REQUIRE(read.complete);
        REQUIRE(read.profiles.size() == input.size());

        for (std::size_t i = 0; i < input.size(); ++i)
            sameText(input[i], read.profiles[i]);

        if (nativeDirectory) {
            std::ofstream out(std::filesystem::path(nativeDirectory) / (id + ".ipf"), std::ios::binary);
            out.write(composed->data(), static_cast<std::streamsize>(composed->size()));
            out.close();
            REQUIRE(out.good());
        }
    }

    CHECK(cases == 32);
    CHECK(accepted == 25);
}

TEST_CASE("Writer limits refuse growth with no input mutation or external publication") {
    const std::vector input{basic()};
    const auto prepared = composeInstrumentProfiles(input);
    REQUIRE(prepared);
    const auto defaults = IpfWriteOptions{};

    for (auto error : {IpfWriteError::PROFILE_LIMIT, IpfWriteError::TYPE_LIMIT, IpfWriteError::FIELD_LIMIT,
                       IpfWriteError::HEADER_LIMIT, IpfWriteError::INPUT_LIMIT, IpfWriteError::OUTPUT_LIMIT,
                       IpfWriteError::WORK_LIMIT}) {
        auto options = defaults;

        switch (error) {
        case IpfWriteError::PROFILE_LIMIT:
            options.limits.profiles = 0;
            break;
        case IpfWriteError::TYPE_LIMIT:
            options.limits.types = 0;
            break;
        case IpfWriteError::FIELD_LIMIT:
            options.limits.fieldsPerType = 1;
            break;
        case IpfWriteError::HEADER_LIMIT:
            options.limits.headerUnits = 10;
            break;
        case IpfWriteError::INPUT_LIMIT:
            options.limits.inputUnits = input[0].retainedUnits() - 1;
            break;
        case IpfWriteError::OUTPUT_LIMIT:
            options.limits.outputBytes = prepared->size() - 1;
            break;
        case IpfWriteError::WORK_LIMIT:
            options.limits.work = 0;
            break;
        default:
            FAIL("unreachable resource case");
        }

        FailingOutput buffer;
        std::ostream output(&buffer);
        const auto result = writeInstrumentProfiles(output, input, options);
        REQUIRE(result.error);
        CHECK(result.error->error == error);
        CHECK_FALSE(result.complete);
        CHECK(result.confirmedBytes == 0);
        CHECK(buffer.writes == 0);
        CHECK(buffer.flushes == 0);
        CHECK(output.good());
    }

    IpfWriteOptions exact;
    exact.limits.profiles = 1;
    exact.limits.types = 1;
    exact.limits.fieldsPerType = 2;
    exact.limits.headerUnits = 11;
    exact.limits.inputUnits = input[0].retainedUnits();
    exact.limits.outputBytes = prepared->size();
    const auto exactDocument = composeInstrumentProfiles(input, exact);
    REQUIRE(exactDocument);
    CHECK(*exactDocument == *prepared);
    InstrumentProfile wide({999999, 999999, 999999, 999999, 999999});
    REQUIRE(wide.setType(u"STOCK"));
    REQUIRE(wide.setSymbol(u"A"));
    REQUIRE(wide.setField(u"", u"x"));
    exact.limits.fieldsPerType = 3;
    exact.limits.headerUnits = 11;
    exact.limits.inputUnits = 7;
    exact.limits.outputBytes = 1024;
    CHECK(composeInstrumentProfiles(std::span(&wide, 1), exact).has_value());
    auto two = input;
    two.push_back(input.front());
    REQUIRE(two.back().setType(u"BOND"));
    exact = defaults;
    exact.limits.types = 1;
    CHECK(composeInstrumentProfiles(two, exact).error().error == IpfWriteError::TYPE_LIMIT);
    two[1] = input.front();
    REQUIRE(two[1].setField(u"X", u"1"));
    exact = defaults;
    exact.limits.fieldsPerType = 2;
    CHECK(composeInstrumentProfiles(two, exact).error().error == IpfWriteError::FIELD_LIMIT);
    auto invalid = input;
    REQUIRE(invalid.front().setType(u"##"));
    invalid.push_back(input.front());
    exact = defaults;
    exact.limits.inputUnits = invalid.front().retainedUnits();
    CHECK(composeInstrumentProfiles(invalid, exact).error().error == IpfWriteError::INVALID_TYPE);

    CHECK(input[0].getSymbol() == u"IBM");
}

TEST_CASE("Borrowed output errors preserve masks and never claim complete publication") {
    const std::vector input{basic()};
    const auto document = composeInstrumentProfiles(input);
    REQUIRE(document);
    const auto body = document->size() - std::string_view("##COMPLETE\r\n").size();

    for (bool allocation : {false, true})
        for (bool rethrow : {false, true}) {
            FailingOutput buffer;
            buffer.failAfter = 5;
            buffer.throwFailure = true;
            buffer.allocation = allocation;
            std::ostream out(&buffer);

            if (rethrow)
                out.exceptions(std::ios::badbit);

            const auto mask = out.exceptions();
            const auto result = writeInstrumentProfiles(out, input);
            REQUIRE(result.error);
            CHECK(result.error->error ==
                  (allocation && rethrow ? IpfWriteError::ALLOCATION_FAILURE : IpfWriteError::IO_ERROR));
            CHECK_FALSE(result.complete);
            CHECK(result.confirmedBytes == 0);
            CHECK(buffer.text == document->substr(0, 5));
            CHECK(out.bad());
            CHECK(out.exceptions() == mask);
            CHECK(buffer.flushes == 0);
        }

    for (int mode = 0; mode < 3; ++mode) {
        FailingOutput buffer;

        if (mode == 0)
            buffer.failAfter = body + 4;
        else
            buffer.failFlush = mode;

        std::ostream out(&buffer);
        const auto result = writeInstrumentProfiles(out, input);
        REQUIRE(result.error);
        CHECK(result.error->error == IpfWriteError::IO_ERROR);
        CHECK_FALSE(result.complete);

        if (mode == 0) {
            CHECK(result.confirmedBytes == body);
            CHECK(buffer.text == document->substr(0, body + 4));
        }

        if (mode == 1) {
            CHECK(result.confirmedBytes == body);
            CHECK(buffer.text == document->substr(0, body));
        }

        if (mode == 2) {
            CHECK(result.confirmedBytes == document->size());
            CHECK(buffer.text == *document);
        }

        CHECK(out.fail());
        CHECK(out.exceptions() == std::ios::goodbit);
    }

    FailingOutput buffer;
    std::ostream out(&buffer);
    out.setstate(std::ios::badbit);
    auto result = writeInstrumentProfiles(out, input);
    REQUIRE(result.error);
    CHECK_FALSE(result.complete);
    CHECK(result.confirmedBytes == 0);
    CHECK(buffer.text.empty());
    CHECK(out.bad());
}

TEST_CASE("Successful publication owns no input and file preparation failure preserves destination") {
    auto p = basic();
    REQUIRE(p.setDescription(u"line1\r\nline2"));
    const std::vector input{p};
    auto document = composeInstrumentProfiles(input);
    REQUIRE(document);
    FailingOutput buffer;
    std::ostream output(&buffer);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    const auto mask = output.exceptions();
    auto result = writeInstrumentProfiles(output, input);
    REQUIRE_FALSE(result.error);
    CHECK(result.complete);
    CHECK(result.confirmedBytes == document->size());
    CHECK(buffer.text == *document);
    CHECK(output.exceptions() == mask);
    CHECK(output.good());
    CHECK(buffer.flushes == 2);
    REQUIRE(p.setSymbol(u"changed"));
    CHECK(buffer.text == *document);
    TempFile file;
    {
        std::ofstream initial(file.path, std::ios::binary);
        initial << "keep me";
    }
    IpfWriteOptions options;
    options.limits.outputBytes = 0;
    result = writeInstrumentProfilesFile(file.path, input, options);
    REQUIRE(result.error);
    CHECK_FALSE(result.complete);
    CHECK(bytes(file.path) == "keep me");
    result = writeInstrumentProfilesFile(file.path, input);
    REQUIRE_FALSE(result.error);
    CHECK(result.complete);
    CHECK(bytes(file.path) == *document);
    auto read = readInstrumentProfilesFile(file.path);
    REQUIRE(read.complete);
    REQUIRE(read.profiles.size() == 1);
    CHECK(read.profiles[0].getDescription() == u"line1\r\nline2");
    auto missing = file.path / "missing-parent.ipf";
    result = writeInstrumentProfilesFile(missing, input);
    REQUIRE(result.error);
    CHECK(result.error->error == IpfWriteError::IO_ERROR);
    CHECK_FALSE(result.complete);
}

TEST_CASE("Canonical zero text and ordinary removed tombstones remain writable") {
    auto p = basic();
    REQUIRE(p.setStrike(-0.0));
    REQUIRE(p.setType(u"REMOVED"));
    REQUIRE(p.setField(u"x", u""));
    auto document = composeInstrumentProfiles(std::span(&p, 1));
    REQUIRE(document);
    CHECK(document->find("STRIKE") == std::string::npos);
    std::istringstream in(*document);
    auto read = readInstrumentProfiles(in);
    REQUIRE(read.complete);
    REQUIRE(read.profiles.size() == 1);
    CHECK(read.profiles[0].getStrike() == 0.0);
    CHECK(read.profiles[0].getType() == u"REMOVED");
    CHECK(read.profiles[0].getSymbol() == u"IBM");
    REQUIRE(p.setField(u"x", u"v"));
    CHECK(composeInstrumentProfiles(std::span(&p, 1)).error().error == IpfWriteError::REMOVED_FIELDS);
    REQUIRE(p.setField(u"x", u""));
    REQUIRE(p.setDescription(u"lost"));
    CHECK(composeInstrumentProfiles(std::span(&p, 1)).error().error == IpfWriteError::REMOVED_FIELDS);
}

TEST_CASE("A failed later output chunk confirms only successful earlier calls") {
    auto profile = basic();
    REQUIRE(profile.setDescription(std::u16string(10000, u'x')));
    const std::vector input{profile};
    const auto document = composeInstrumentProfiles(input);
    REQUIRE(document);
    FailingOutput buffer;
    buffer.failAfter = 4101;
    std::ostream output(&buffer);
    const auto result = writeInstrumentProfiles(output, input);
    REQUIRE(result.error);
    CHECK(result.error->error == IpfWriteError::IO_ERROR);
    CHECK_FALSE(result.complete);
    CHECK(result.confirmedBytes == 4096);
    CHECK(buffer.text == document->substr(0, 4101));
    CHECK(buffer.writes == 2);
    CHECK(buffer.flushes == 0);
    CHECK(output.bad());
}

TEST_CASE("A real preparation allocation failure publishes nothing and preserves source values") {
    const std::vector input{basic()};
    auto before = input;
    std::expected<std::string, IpfWriteDiagnostic> result;
    IpfWriteOptions blocked;
    blocked.limits.inputUnits = 0;
    {
        AllocationFault fault;
        result = composeInstrumentProfiles(input, blocked);
    }
    REQUIRE_FALSE(result);
    CHECK(result.error().error == IpfWriteError::INPUT_LIMIT);
    {
        AllocationFault fault;
        result = composeInstrumentProfiles(input);
    }
    REQUIRE_FALSE(result);
    CHECK(result.error().error == IpfWriteError::ALLOCATION_FAILURE);
    CHECK(input == before);
    CHECK(composeInstrumentProfiles(input).has_value());
}
