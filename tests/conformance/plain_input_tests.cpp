// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/compressed_input.hpp"
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
bool trackAllocations = false;   ///< Enabled only around synchronous adapter calls.
std::size_t allocationCalls = 0; ///< Ordinary allocation calls made while tracking.
} // namespace

/** Test allocator preserving ordinary allocation semantics while observing refusal paths.
 * @param size Requested bytes.
 * @return Owned storage, or throws bad_alloc.
 */
void *operator new(std::size_t size) {
    if (trackAllocations) {
        ++allocationCalls;
    }

    if (auto *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc();
}

/** Release ordinary test storage.
 * @param memory Previously allocated storage, possibly null.
 */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** Release sized ordinary test storage.
 * @param memory Previously allocated storage, possibly null.
 */
void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;

/** Nonseeking source with bounded short reads and an optional failure after its prefix. */
struct Source : std::streambuf {
    std::string bytes;         ///< Exact test input.
    std::size_t position = 0;  ///< Consumed physical bytes.
    std::size_t calls = 0;     ///< Physical read/probe invocations.
    std::size_t partition = 1; ///< Maximum successful transfer.
    bool failAtEnd = false;    ///< Throw instead of reporting EOF.

    /** Create an owned source independent from adapter lifetime.
     * @param input Exact bytes.
     */
    explicit Source(std::string input) : bytes(std::move(input)) {
    }

    /** Transfer one bounded source partition.
     * @param output Borrowed destination.
     * @param requested Positive maximum bytes.
     * @return Actually transferred bytes.
     */
    std::streamsize xsgetn(char *output, std::streamsize requested) override {
        ++calls;

        if (position == bytes.size() && failAtEnd) {
            throw std::runtime_error("injected source failure");
        }

        const auto count = std::min({static_cast<std::size_t>(requested), partition, bytes.size() - position});
        std::memcpy(output, bytes.data() + position, count);
        position += count;

        return static_cast<std::streamsize>(count);
    }

    /** Observe EOF without consuming bytes.
     * @return Current byte or physical EOF.
     */
    int_type underflow() override {
        ++calls;

        if (position == bytes.size() && failAtEnd) {
            throw std::runtime_error("injected probe failure");
        }

        return position == bytes.size() ? traits_type::eof() : traits_type::to_int_type(bytes[position]);
    }
};

/** Captured terminal adapter result and complete published prefix. */
struct Result {
    std::string bytes;              ///< Published bytes in order.
    ti::ContainerRead terminal;     ///< Final result.
    ti::ContainerCounters counters; ///< Irreversible final charges.
};

/** Drain only the test adapter, then verify terminal idempotence without source work.
 * @param source Borrowed test source.
 * @param limits Explicit finite bounds.
 * @return Owned observations.
 */
Result run(Source &source, ti::ContainerLimits limits) {
    std::istream physical(&source);
    ti::CompressedInput input(physical, ti::ContainerKind::NONE, limits);
    std::array<std::byte, 2> output{};
    Result result;

    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        result.terminal = input.read(output);
        result.bytes.append(reinterpret_cast<const char *>(output.data()), result.terminal.produced);

        if (result.terminal.error != ti::ContainerError::NONE || result.terminal.physicalComplete) {
            break;
        }
    }

    CHECK((result.terminal.error != ti::ContainerError::NONE || result.terminal.physicalComplete));

    const auto calls = source.calls;
    const auto repeated = input.read(output);
    CHECK(repeated.produced == 0);
    CHECK(repeated.error == result.terminal.error);
    CHECK(repeated.physicalComplete == result.terminal.physicalComplete);
    CHECK(input.finish() == result.terminal.error);
    CHECK(source.calls == calls);
    result.counters = input.counters();

    return result;
}
} // namespace

TEST_CASE("plain source availability and lazy refusal perform no allocation or reads") {
    Source source("abc");
    std::istream physical(&source);
    std::array<std::byte, 1> output{};
    const auto invalid = static_cast<ti::ContainerKind>(255);
    allocationCalls = 0;
    trackAllocations = true;
    const bool plain = ti::containerAvailable(ti::ContainerKind::NONE);
    const bool gzip = ti::containerAvailable(ti::ContainerKind::GZIP);
    const bool zip = ti::containerAvailable(ti::ContainerKind::ZIP);
    const bool unknown = ti::containerAvailable(invalid);
    ti::CompressedInput malformed(physical, invalid);
    const auto unfinished = malformed.finish();
    const auto rejected = malformed.read(output);
    trackAllocations = false;
    CHECK(plain);
    CHECK(gzip == static_cast<bool>(ORG_TTLDTOR_QDCXX_TEST_COMPRESSION));
    CHECK(zip == gzip);
    CHECK_FALSE(unknown);
    CHECK(unfinished == ti::ContainerError::NOT_FINISHED);
    CHECK(rejected.error == ti::ContainerError::INVALID_ARGUMENT);
    CHECK(allocationCalls == 0);
    CHECK(source.calls == 0);

    if (!gzip) {
        for (const auto kind : {ti::ContainerKind::GZIP, ti::ContainerKind::ZIP}) {
            ti::ContainerLimits limits;
            limits.chunkBytes = 0;
            limits.retainedBytes = 0;
            allocationCalls = 0;
            trackAllocations = true;
            ti::CompressedInput unavailable(physical, kind, limits);
            const auto before = unavailable.finish();
            const auto first = unavailable.read(output);
            const auto second = unavailable.read(output);
            trackAllocations = false;
            CHECK(before == ti::ContainerError::NOT_FINISHED);
            CHECK(first.error == ti::ContainerError::UNSUPPORTED);
            CHECK(second.error == first.error);
            CHECK(allocationCalls == 0);
            CHECK(source.calls == 0);
        }
    }
}

TEST_CASE("plain exact EOF and physical excess use bounded one byte probes") {
    for (const auto size : {2, 3, 4}) {
        Source source(std::string(size, 'x'));
        ti::ContainerLimits limits;
        limits.chunkBytes = 2;
        limits.compressedBytes = 3;
        limits.inflatedBytes = 3;
        const auto result = run(source, limits);
        CHECK(result.bytes == std::string(std::min(size, 3), 'x'));
        CHECK(result.counters.compressedPhysicalRead == size);
        CHECK(result.counters.compressedConsumed == std::min(size, 3));
        CHECK(result.terminal.error == (size > 3 ? ti::ContainerError::COMPRESSED_LIMIT : ti::ContainerError::NONE));
        CHECK(result.terminal.physicalComplete == (size <= 3));
    }
}

TEST_CASE("plain inflated allowance retains only the permitted prefix") {
    for (const auto size : {2, 3, 4}) {
        Source source(std::string(size, 'x'));
        ti::ContainerLimits limits;
        limits.chunkBytes = 1;
        limits.inflatedBytes = 3;
        const auto result = run(source, limits);
        CHECK(result.bytes == std::string(std::min(size, 3), 'x'));
        CHECK(result.counters.logicalProduced == std::min(size, 3));
        CHECK(result.counters.compressedPhysicalRead == size);
        CHECK(result.terminal.error == (size > 3 ? ti::ContainerError::INFLATED_LIMIT : ti::ContainerError::NONE));
    }
}

TEST_CASE("plain work zero and exact L minus one L plus one stay finite") {
    for (const auto work : {0, 5, 6, 7}) {
        Source source("abc");
        ti::ContainerLimits limits;
        limits.chunkBytes = 1;
        limits.workBytes = work;
        limits.inflateCalls = 0;
        const auto result = run(source, limits);
        CHECK(result.bytes == (work == 0 ? "" : work == 5 ? "ab" : "abc"));
        CHECK(result.terminal.error == (work < 6 ? ti::ContainerError::WORK_LIMIT : ti::ContainerError::NONE));
        CHECK(result.counters.workBytes <= work);
        CHECK(result.counters.inflateCalls == 0);

        if (work == 0) {
            CHECK(source.calls == 0);
        }
    }
}

TEST_CASE("plain short partitions preserve prefix before source IO failure") {
    for (const auto partition : {1, 2, 8}) {
        Source source("abc");
        source.partition = partition;
        source.failAtEnd = true;
        ti::ContainerLimits limits;
        limits.chunkBytes = 2;
        const auto result = run(source, limits);
        CHECK(result.bytes == "abc");
        CHECK(result.terminal.error == ti::ContainerError::SOURCE_IO);
        CHECK(result.counters.compressedPhysicalRead == 3);
        CHECK(result.counters.logicalProduced == 3);
    }
}

TEST_CASE("plain destruction never drains and configuration stays lazy") {
    Source source("abc");
    std::istream physical(&source);
    std::array<std::byte, 1> output{};
    std::size_t calls = 0;

    {
        ti::ContainerLimits limits;
        limits.chunkBytes = 1;
        ti::CompressedInput input(physical, ti::ContainerKind::NONE, limits);
        CHECK(input.finish() == ti::ContainerError::NOT_FINISHED);
        CHECK(source.calls == 0);
        CHECK(input.read(output).produced == 1);
        calls = source.calls;
    }

    CHECK(source.calls == calls);
    CHECK(source.position == 1);

    for (const auto invalid : {0, 1, 2}) {
        ti::ContainerLimits limits;

        if (invalid == 0) {
            limits.chunkBytes = 0;
        } else if (invalid == 1) {
            limits.compressedBytes = std::numeric_limits<std::uint64_t>::max();
        } else {
            limits.inflatedBytes = std::numeric_limits<std::uint64_t>::max();
        }

        ti::CompressedInput input(physical, ti::ContainerKind::NONE, limits);
        CHECK(input.finish() == ti::ContainerError::NOT_FINISHED);
        CHECK(input.read(output).error == ti::ContainerError::INVALID_ARGUMENT);
        CHECK(source.calls == calls);
    }

    ti::CompressedInput empty(physical, ti::ContainerKind::NONE);
    CHECK(empty.read({}).error == ti::ContainerError::INVALID_ARGUMENT);
    CHECK(empty.read(output).error == ti::ContainerError::INVALID_ARGUMENT);
    CHECK(source.calls == calls);
}
