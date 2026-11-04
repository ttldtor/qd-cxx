// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "source_envelope.hpp"
#include "transfer_response.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace i = org::ttldtor::qdcxx::ipf;
namespace d = i::detail;

namespace {
/** Load a finite synthetic pinned-oracle fixture.
 * @param name Fixture basename without suffix.
 * @return Owned exact encoded bytes.
 */
std::vector<std::byte> fixture(std::string_view name) {
    std::ifstream in(std::string(ORG_TTLDTOR_QDCXX_IPF_COMPRESSION_FIXTURES) + "/" + std::string(name) + ".bin",
                     std::ios::binary);
    REQUIRE(in);
    std::string bytes{std::istreambuf_iterator<char>(in), {}};
    REQUIRE(bytes.size() <= 1048576);
    const auto view = std::as_bytes(std::span(bytes));

    return {view.begin(), view.end()};
}

/** Detached finite decoder-only observation. */
struct Decoded {
    i::IpfTransferResult result; ///< Owning diagnostic/counters.
    std::string text;            ///< Synthetic decoded prefix, at most the explicit test bound.
};

/** Exercise actual production decoder with independent source/output fragmentation.
 * @param bytes Complete finite fixture.
 * @param limits Independent explicit decoder ceilings.
 * @param chunk Maximum presented source bytes.
 * @param forcedGzip True for the HTTP coding selection.
 * @return Detached output prefix and envelope evidence.
 */
Decoded decode(std::span<const std::byte> bytes, i::IpfEnvelopeLimits limits = {}, std::size_t chunk = 1,
               bool forcedGzip = false) {
    Decoded decoded;
    d::SourceEnvelope envelope(limits, decoded.result);

    if (forcedGzip) {
        REQUIRE(envelope.declareGzip());
    }

    std::size_t position = 0;
    std::size_t turns = 0;
    const auto take = [&] {
        const auto output = envelope.output();
        REQUIRE(decoded.text.size() + output.size() <= 1048576);
        decoded.text.append(reinterpret_cast<const char *>(output.data()), output.size());
        envelope.consume(output.size());
    };

    while (position < bytes.size() && !envelope.failed()) {
        REQUIRE(++turns <= 100000);
        position += envelope.feed(bytes.subspan(position, (std::min)(chunk, bytes.size() - position)));
        take();
    }

    while (!envelope.finish()) {
        REQUIRE(++turns <= 100000);
        take();
    }

    take();

    return decoded;
}
} // namespace

TEST_CASE("IPF envelopes match actual JDK8 selected first-file and gzip outcomes") {
    constexpr std::string_view IPF = "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n##COMPLETE\n";
    const std::vector<std::string> positive{"plain",
                                            "gzip",
                                            "gzip-optional",
                                            "gzip-concatenated",
                                            "gzip-trailing-junk",
                                            "gzip-next-header-truncated",
                                            "zip-stored",
                                            "zip-deflate",
                                            "zip-descriptor",
                                            "zip64-small",
                                            "zip64-descriptor64-small",
                                            "zip-descriptor-nosignature",
                                            "zip-directory-nosignature-first",
                                            "zip-directory-first",
                                            "zip-followed-file",
                                            "zip-followed-corruption",
                                            "zip-full-archive",
                                            "zip64-stored-small",
                                            "zip-descriptor-nosignature-followed",
                                            "zip64-descriptor64-nosignature-small",
                                            "zip64-directory-nosignature-first",
                                            "gzip-next-header-invalid"};

    for (const auto &name : positive) {
        CAPTURE(name);
        const auto bytes = fixture(name);

        for (const auto chunk : {std::size_t(1), std::size_t(7), std::size_t(4096)}) {
            CAPTURE(chunk);
            auto decoded = decode(bytes, {}, chunk);
            INFO((decoded.result.error ? decoded.result.error->stage : "none"));
            REQUIRE_FALSE(decoded.result.error);
            CHECK(decoded.text == IPF);
            CHECK(decoded.result.envelope.finished);
            CHECK(decoded.result.envelope.inputBytes == bytes.size());
            CHECK(decoded.result.envelope.kind == (name.starts_with("gzip")  ? i::IpfEnvelopeKind::GZIP
                                                   : name.starts_with("zip") ? i::IpfEnvelopeKind::ZIP
                                                                             : i::IpfEnvelopeKind::NONE));
        }
    }

    for (const auto name :
         {"gzip-truncated", "gzip-bad-crc", "zip64-descriptor-small", "zip-stored-descriptor", "zip-directory-only",
          "zip-bad-crc", "zip-truncated", "zip-truncated-descriptor", "gzip-concat-second-bad", "gzip-bad-size",
          "zip-encrypted", "zip-bad-size", "gzip-header-truncated", "zip-header-truncated"}) {
        CAPTURE(name);
        auto decoded = decode(fixture(name));
        REQUIRE(decoded.result.error);
        CHECK(decoded.result.error->error == i::IpfTransferError::CONTAINER_FAILURE);
        CHECK_FALSE(decoded.result.envelope.finished);

        if (std::string_view(name) == "gzip-bad-crc" || std::string_view(name) == "gzip-truncated" ||
            std::string_view(name) == "zip-bad-crc") {
            CHECK(decoded.text == IPF);
        }
    }

    auto emptyZip = decode(fixture("zip-empty-eocd"));
    CHECK_FALSE(emptyZip.result.error);
    CHECK(emptyZip.result.envelope.kind == i::IpfEnvelopeKind::NONE);
}

TEST_CASE("IPF envelope exact and one-below caps cover real decode progress") {
    const auto bytes = fixture("gzip");
    const auto baseline = decode(bytes);
    REQUIRE_FALSE(baseline.result.error);
    const auto verify = [&](i::IpfEnvelopeLimits exact, i::IpfEnvelopeLimits below, i::IpfTransferError error) {
        auto accepted = decode(bytes, exact);
        INFO((accepted.result.error ? accepted.result.error->stage : "none"));
        REQUIRE_FALSE(accepted.result.error);
        CHECK(accepted.text == baseline.text);
        auto rejected = decode(bytes, below);
        REQUIRE(rejected.result.error);
        CHECK(rejected.result.error->error == error);
        CHECK(baseline.text.starts_with(rejected.text));
    };
    i::IpfEnvelopeLimits exact;
    exact.inputBytes = bytes.size();
    auto below = exact;
    --below.inputBytes;
    verify(exact, below, i::IpfTransferError::RESOURCE_LIMIT);
    exact = {};
    exact.outputBytes = baseline.text.size();
    below = exact;
    --below.outputBytes;
    verify(exact, below, i::IpfTransferError::RESOURCE_LIMIT);
    exact = {};
    exact.calls = baseline.result.envelope.calls;
    below = exact;
    --below.calls;
    verify(exact, below, i::IpfTransferError::WORK_LIMIT);
    exact = {};
    exact.workBytes = baseline.result.envelope.workBytes;
    below = exact;
    --below.workBytes;
    verify(exact, below, i::IpfTransferError::WORK_LIMIT);
    exact = {};
    exact.inflaterBytes = baseline.result.envelope.peakInflaterBytes;
    below = exact;
    --below.inflaterBytes;
    verify(exact, below, i::IpfTransferError::RESOURCE_LIMIT);

    for (const auto name : {"zip-directory-first", "gzip-concatenated"}) {
        auto accepted = decode(fixture(name));
        REQUIRE_FALSE(accepted.result.error);
        exact = {};
        exact.entries = accepted.result.envelope.entries;
        CHECK_FALSE(decode(fixture(name), exact).result.error);
        --exact.entries;
        CHECK(decode(fixture(name), exact).result.error->error == i::IpfTransferError::RESOURCE_LIMIT);
    }

    auto zip = decode(fixture("zip64-small"));
    REQUIRE_FALSE(zip.result.error);
    exact = {};
    exact.metadataBytes = zip.result.envelope.metadataBytes;
    CHECK_FALSE(decode(fixture("zip64-small"), exact).result.error);
    --exact.metadataBytes;
    CHECK(decode(fixture("zip64-small"), exact).result.error->error == i::IpfTransferError::RESOURCE_LIMIT);
    MESSAGE("inflater peak=" << baseline.result.envelope.peakInflaterBytes
                             << " sizeof decoder=" << sizeof(d::SourceEnvelope));
}

TEST_CASE("IPF coding forces one gzip layer and never applies a second magic pass") {
    auto plain = fixture("plain");
    auto declared = decode(plain, {}, 1, true);
    REQUIRE(declared.result.error);
    CHECK(declared.result.error->error == i::IpfTransferError::CONTAINER_FAILURE);
    auto gzip = decode(fixture("gzip"), {}, 1, true);
    REQUIRE_FALSE(gzip.result.error);
    CHECK(gzip.result.envelope.kind == i::IpfEnvelopeKind::GZIP);
    CHECK(gzip.result.envelope.outputBytes == plain.size());
}

TEST_CASE("IPF whole-profile prefix and first cause survive a later container error") {
    i::IpfTransferLimits limits;
    i::IpfReadOptions options;
    i::IpfTransferResult result;
    d::TransferWork work;
    d::TransferProfiles profiles(options, result, work, limits);
    const auto bytes = fixture("gzip-bad-crc");
    std::size_t position = 0;

    for (std::size_t turns = 0; position < bytes.size() && !profiles.failed(); ++turns) {
        REQUIRE(turns < 10000);
        auto credits = options.turnWork;
        position += profiles.feed(std::span(bytes).subspan(position), credits);
    }

    while (profiles.hasOwnedInput()) {
        auto credits = options.turnWork;
        (void)profiles.drain(credits);
    }

    REQUIRE(result.error);
    CHECK(result.error->error == i::IpfTransferError::CONTAINER_FAILURE);
    CHECK(result.ipf.profiles.size() == 1);
    CHECK(result.ipf.completionSeen);
    CHECK_FALSE(result.ipf.finished);
    CHECK_FALSE(result.envelope.finished);
    i::IpfTransferResult first;
    first.error = i::IpfTransferDiagnostic{i::IpfTransferError::TRANSPORT_FAILURE, "first-native"};
    d::SourceEnvelope envelope(limits.envelope, first);
    std::size_t consumed = 0;

    while (consumed < bytes.size() && !envelope.failed()) {
        consumed += envelope.feed(std::span(bytes).subspan(consumed));
        envelope.consume(envelope.output().size());
    }

    CHECK(first.error->stage == "first-native");
}

TEST_CASE("IPF expansion output cap refuses the first unadmitted byte") {
    auto bytes = fixture("gzip-expansion");
    auto baseline = decode(bytes, {}, 4096);
    REQUIRE_FALSE(baseline.result.error);
    CHECK(baseline.text.size() == 100032);
    i::IpfEnvelopeLimits exact;
    exact.outputBytes = baseline.text.size();
    CHECK_FALSE(decode(bytes, exact, 4096).result.error);
    --exact.outputBytes;
    auto below = decode(bytes, exact, 4096);
    REQUIRE(below.result.error);
    CHECK(below.result.error->error == i::IpfTransferError::RESOURCE_LIMIT);
    CHECK(below.text.size() <= exact.outputBytes);
    CHECK(baseline.text.starts_with(below.text));
    CHECK_FALSE(below.result.envelope.finished);
    CHECK(below.result.envelope.peakInflaterBytes <= exact.inflaterBytes);
    auto nested = decode(fixture("gzip-nested"));
    REQUIRE_FALSE(nested.result.error);
    CHECK(nested.result.envelope.kind == i::IpfEnvelopeKind::GZIP);
    auto original = fixture("gzip");
    CHECK(nested.text.size() == original.size());
    CHECK(std::equal(original.begin(), original.end(), reinterpret_cast<const std::byte *>(nested.text.data())));
}
