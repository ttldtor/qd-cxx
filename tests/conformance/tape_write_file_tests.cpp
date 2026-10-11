// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "file_boundary.hpp"

#include <algorithm>
#include <atomic>
#include <doctest/doctest.h>
#include <fstream>
#include <iterator>
#include <string>

namespace {
namespace file = org::ttldtor::qdcxx::tape_write;
namespace fs = std::filesystem;
const std::string INPUT_BYTES = "original-input"; ///< input-preservation sentinel.
const std::string OUTPUT = "complete-output";     ///< Expected published native bytes.
const std::string OLD = "existing-destination";   ///< Never-overwritten destination sentinel.

/** Store an exact small test sentinel.
 * @param path Owned test filename.
 * @param bytes Exact expected bytes.
 */
void store(const fs::path &path, std::string_view bytes) {
    std::ofstream stream(path, std::ios::binary);
    REQUIRE(stream.is_open());
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    REQUIRE_FALSE(stream.fail());
}

/** Read an exact small fixture without production-boundary instrumentation.
 * @param path Existing test file.
 * @return Owned sentinel bytes.
 */
std::string load(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.is_open());

    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/** Sole owner of one exclusively created test directory and its fixture files. */
struct Fixture {
    fs::path root;   ///< Exclusively created test directory.
    fs::path input;  ///< Original input sentinel.
    fs::path output; ///< Initially absent destination.

    /** Exclusively claim one test directory without reusing pre-existing entries. */
    Fixture() {
        static std::atomic<unsigned> sequence{};

        for (unsigned attempt = 0; attempt < 1024; ++attempt) {
            root = fs::temp_directory_path() / ("qdcxx-file-boundary-" + std::to_string(sequence++));
            std::error_code error;

            if (fs::create_directory(root, error)) {
                input = root / "input.qtp";
                output = root / "output.qtp";
                store(input, INPUT_BYTES);

                return;
            }
        }

        FAIL("could not exclusively create test directory");
    }

    /** Remove only the exclusively owned fixture directory, after all owners have died. */
    ~Fixture() {
        std::error_code error;
        fs::remove_all(root, error);
    }

    /** Resolve normal fresh paths through the actual production resolver.
     * @return Successful owning paths.
     */
    file::FilePaths paths() const {
        const auto result = file::resolvePaths(input, output);
        REQUIRE(result.has_value());

        return *result;
    }
};

/** Faults injected at actual physical operations, while retaining native ownership semantics. */
enum class Fault {
    NONE,         ///< Real successful operations.
    READ,         ///< Fail actual input read boundary.
    WRITE,        ///< Fail actual output write boundary.
    ZERO_WRITE,   ///< Return zero progress at output write boundary.
    FLUSH,        ///< Fail native flush boundary.
    CLOSE,        ///< Consume real handle once, then report close failure.
    LINK,         ///< Fail actual publication boundary.
    REMOVE,       ///< Fail owned-name removal after successful link.
    WRITE_REMOVE, ///< Primary write failure plus secondary cleanup failure.
    RACE,         ///< Create an old target immediately before real hard-link publication.
    COLLISION     ///< Create old entries before every actual exclusive-open attempt.
};

/** Borrowed callback context forwarding successful operations to the same production table. */
struct Probe {
    file::FileOperations native{file::productionFileOperations()}; ///< Actual native boundary.
    Fault fault{Fault::NONE};                                      ///< Selected operation fault.
    bool partial{};                   ///< Limit reads/writes to two bytes to exercise owning progress loops.
    bool afterProgress{};             ///< Delay injected read/write failure until one real transfer succeeded.
    unsigned opens{};                 ///< Number of actual owning open calls.
    unsigned reads{};                 ///< Number of actual owning read calls.
    unsigned writes{};                ///< Number of actual owning write calls.
    unsigned closes{};                ///< Number of consumed handles.
    unsigned links{};                 ///< Number of publication attempts.
    unsigned removes{};               ///< Number of owned-name cleanup attempts.
    fs::path temporary;               ///< Last output attempt path, for preservation assertions.
    std::vector<fs::path> collisions; ///< Old entries never owned by OutputFile.

    /** Static I/O failure for injected callbacks.
     * @return A native-independent failure condition.
     */
    static std::error_code error() noexcept {
        return std::make_error_code(std::errc::io_error);
    }

    /** Build a borrowed table forwarding all nonfaulted operations to actual production callbacks.
     * @return Complete context table, valid only while this Probe exists.
     */
    file::FileOperations operations() {
        return {this,
                [](void *context, const fs::path &path,
                   bool exclusive) noexcept -> std::expected<file::FileHandle, std::error_code> {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.opens;

                    try {
                        if (exclusive) {
                            probe.temporary = path;

                            if (probe.fault == Fault::COLLISION) {
                                std::ofstream stream(path, std::ios::binary);
                                stream << OLD;
                                stream.close();
                                probe.collisions.push_back(path);
                            }
                        }
                    } catch (...) {
                        return std::unexpected(error());
                    }

                    return probe.native.open(nullptr, path, exclusive);
                },
                [](void *context, file::FileHandle handle,
                   std::span<std::byte> bytes) noexcept -> std::expected<std::size_t, std::error_code> {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.reads;

                    if (probe.fault == Fault::READ && (!probe.afterProgress || probe.reads > 1)) {
                        return std::unexpected(error());
                    }

                    return probe.native.read(
                        nullptr, handle, probe.partial ? bytes.first(std::min(bytes.size(), std::size_t{2})) : bytes);
                },
                [](void *context, file::FileHandle handle,
                   std::span<const std::byte> bytes) noexcept -> std::expected<std::size_t, std::error_code> {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.writes;

                    if ((probe.fault == Fault::WRITE || probe.fault == Fault::WRITE_REMOVE) &&
                        (!probe.afterProgress || probe.writes > 1)) {
                        return std::unexpected(error());
                    }

                    if (probe.fault == Fault::ZERO_WRITE && (!probe.afterProgress || probe.writes > 1)) {
                        return 0;
                    }

                    return probe.native.write(
                        nullptr, handle, probe.partial ? bytes.first(std::min(bytes.size(), std::size_t{2})) : bytes);
                },
                [](void *context, file::FileHandle handle) noexcept {
                    auto &probe = *static_cast<Probe *>(context);

                    return probe.fault == Fault::FLUSH ? error() : probe.native.flush(nullptr, handle);
                },
                [](void *context, file::FileHandle handle) noexcept {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.closes;
                    const auto nativeError = probe.native.close(nullptr, handle);

                    return probe.fault == Fault::CLOSE ? error() : nativeError;
                },
                [](void *context, const fs::path &source, const fs::path &destination) noexcept {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.links;

                    if (probe.fault == Fault::LINK) {
                        return error();
                    }

                    if (probe.fault == Fault::RACE) {
                        std::ofstream stream(destination, std::ios::binary);
                        stream << OLD;
                        stream.close();
                    }

                    return probe.native.link(nullptr, source, destination);
                },
                [](void *context, const fs::path &path) noexcept {
                    auto &probe = *static_cast<Probe *>(context);
                    ++probe.removes;

                    return probe.fault == Fault::REMOVE || probe.fault == Fault::WRITE_REMOVE
                               ? error()
                               : probe.native.remove(nullptr, path);
                }};
    }
};

/** Borrow the exact output sentinel as bytes.
 * @return Span valid while the static OUTPUT string remains alive.
 */
std::span<const std::byte> outputBytes() {
    return std::as_bytes(std::span(OUTPUT.data(), OUTPUT.size()));
}
} // namespace

TEST_CASE("physical input authenticates EOF and close before yielding owned bytes") {
    Fixture fixture;
    Probe probe;
    probe.partial = true;
    const auto input = file::readInput(fixture.paths(), {}, probe.operations());
    REQUIRE(input.has_value());
    CHECK(std::string(reinterpret_cast<const char *>(input->data()), input->size()) == INPUT_BYTES);
    CHECK(probe.reads > 1);
    CHECK(probe.closes == 1);
    CHECK_FALSE(fs::exists(fixture.output));
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("physical input errors and byte boundaries never create output") {
    for (const auto fault : {Fault::READ, Fault::CLOSE}) {
        Fixture fixture;
        Probe probe;
        probe.fault = fault;
        const auto input = file::readInput(fixture.paths(), {}, probe.operations());
        REQUIRE_FALSE(input.has_value());
        CHECK(input.error().code == (fault == Fault::READ ? "READ_FAILED" : "CLOSE_FAILED"));
        CHECK(probe.closes == 1);
        CHECK_FALSE(fs::exists(fixture.output));
        CHECK(load(fixture.input) == INPUT_BYTES);
    }

    for (const auto byteLimit : {std::size_t{0}, INPUT_BYTES.size() - 1, INPUT_BYTES.size(), INPUT_BYTES.size() + 1}) {
        Fixture fixture;
        auto limits = file::FileLimits{};
        limits.inputBytes = byteLimit;
        const auto input = file::readInput(fixture.paths(), limits);
        CHECK(input.has_value() == (byteLimit >= INPUT_BYTES.size()));
        CHECK_FALSE(fs::exists(fixture.output));
        CHECK(load(fixture.input) == INPUT_BYTES);
    }
}

TEST_CASE("actual hard-link publication follows partial native writes and checked close") {
    Fixture fixture;
    Probe probe;
    probe.partial = true;
    {
        auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
        REQUIRE(output.has_value());
        REQUIRE(output->write(outputBytes()).has_value());
        REQUIRE(output->finishPublish().has_value());
        CHECK_FALSE(output->finishPublish().has_value());
    }

    CHECK(probe.writes > 1);
    CHECK(probe.closes == 1);
    CHECK(probe.links == 1);
    CHECK(probe.removes == 1);
    CHECK_FALSE(fs::exists(probe.temporary));
    CHECK(load(fixture.output) == OUTPUT);
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("write zero-progress flush close and link failures preserve input and remove only owned temp") {
    for (const auto fault : {Fault::WRITE, Fault::ZERO_WRITE, Fault::FLUSH, Fault::CLOSE, Fault::LINK}) {
        Fixture fixture;
        Probe probe;
        probe.fault = fault;
        {
            auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
            REQUIRE(output.has_value());
            const auto written = output->write(outputBytes());
            const auto result = written ? output->finishPublish() : written;
            REQUIRE_FALSE(result.has_value());
            CHECK_FALSE(result.error().published);
            CHECK_FALSE(result.error().cleanupFailed);
            const auto expected = fault == Fault::WRITE        ? "WRITE_FAILED"
                                  : fault == Fault::ZERO_WRITE ? "ZERO_PROGRESS"
                                  : fault == Fault::FLUSH      ? "FLUSH_FAILED"
                                  : fault == Fault::CLOSE      ? "CLOSE_FAILED"
                                                               : "LINK_FAILED";
            CHECK(result.error().code == expected);
            CHECK_FALSE(output->write(outputBytes()).has_value());
        }

        CHECK(probe.closes == 1);
        CHECK(probe.removes == 1);
        CHECK_FALSE(fs::exists(probe.temporary));
        CHECK_FALSE(fs::exists(fixture.output));
        CHECK(load(fixture.input) == INPUT_BYTES);
    }
}

TEST_CASE("successful link plus cleanup failure preserves complete published target and exact owned path") {
    Fixture fixture;
    Probe probe;
    probe.fault = Fault::REMOVE;
    {
        auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
        REQUIRE(output.has_value());
        REQUIRE(output->write(outputBytes()).has_value());
        const auto result = output->finishPublish();
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().published);
        CHECK(result.error().cleanupFailed);
        CHECK(result.error().ownedTemporary == probe.temporary);
        CHECK(result.error().code == "REMOVE_FAILED");
    }

    CHECK(probe.closes == 1);
    CHECK(probe.removes == 1);
    CHECK(fs::equivalent(probe.temporary, fixture.output));
    CHECK(load(fixture.output) == OUTPUT);
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("cleanup failure cannot replace first write or caller failure") {
    Fixture fixture;
    Probe probe;
    probe.fault = Fault::WRITE_REMOVE;
    auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
    REQUIRE(output.has_value());
    const auto result = output->write(outputBytes());
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == "WRITE_FAILED");
    CHECK(result.error().cleanupFailed);
    CHECK(result.error().ownedTemporary == probe.temporary);
    CHECK_FALSE(result.error().published);
    CHECK_FALSE(fs::exists(fixture.output));
    CHECK(load(fixture.input) == INPUT_BYTES);

    Probe second;
    auto another = file::OutputFile::create(fixture.paths(), {}, second.operations());
    REQUIRE(another.has_value());
    const auto discarded = another->discard({"parse", "TRUNCATED_INPUT"});
    CHECK(discarded.code == "TRUNCATED_INPUT");
    CHECK_FALSE(fs::exists(second.temporary));
    CHECK(second.closes == 1);
}

TEST_CASE("publication races refuse rather than overwrite the newly appeared target") {
    Fixture fixture;
    Probe probe;
    probe.fault = Fault::RACE;
    auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
    REQUIRE(output.has_value());
    REQUIRE(output->write(outputBytes()).has_value());
    const auto result = output->finishPublish();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == "LINK_FAILED");
    CHECK_FALSE(result.error().published);
    CHECK_FALSE(fs::exists(probe.temporary));
    CHECK(load(fixture.output) == OLD);
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("native exclusive-name collisions never transfer cleanup ownership") {
    Fixture fixture;
    Probe probe;
    probe.fault = Fault::COLLISION;
    const auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
    REQUIRE_FALSE(output.has_value());
    CHECK(output.error().code == "EXCLUSIVE_COLLISION");
    CHECK(probe.collisions.size() == 64);
    CHECK(probe.removes == 0);
    CHECK(probe.closes == 0);

    for (const auto &path : probe.collisions) {
        CHECK(load(path) == OLD);
    }

    CHECK_FALSE(fs::exists(fixture.output));
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("resolved lexical parent hard-link and symlink aliases and existing entries are refused") {
    Fixture fixture;
    CHECK_FALSE(file::resolvePaths(fixture.input, fixture.input).has_value());
    CHECK_FALSE(
        file::resolvePaths(fixture.input, fixture.root / ".." / fixture.root.filename() / "input.qtp").has_value());
    fs::create_hard_link(fixture.input, fixture.output);
    const auto hardLink = file::resolvePaths(fixture.input, fixture.output);
    REQUIRE_FALSE(hardLink.has_value());
    CHECK(hardLink.error().code == "INPUT_OUTPUT_ALIAS");
    CHECK(load(fixture.output) == INPUT_BYTES);
    fs::remove(fixture.output);
    store(fixture.output, OLD);
    CHECK_FALSE(file::resolvePaths(fixture.input, fixture.output).has_value());
    CHECK(load(fixture.output) == OLD);
    fs::remove(fixture.output);
    fs::create_directory(fixture.output);
    CHECK_FALSE(file::resolvePaths(fixture.input, fixture.output).has_value());
    fs::remove(fixture.output);
    std::error_code error;
    fs::create_symlink(fixture.input, fixture.output, error);

    if (!error) {
        CHECK_FALSE(file::resolvePaths(fixture.input, fixture.output).has_value());
        CHECK(load(fixture.output) == INPUT_BYTES);
        fs::remove(fixture.output);
        fs::create_symlink(fixture.root / "absent.qtp", fixture.output);
        CHECK_FALSE(file::resolvePaths(fixture.input, fixture.output).has_value());
        fs::remove(fixture.output);
        fs::create_directory_symlink(fixture.root, fixture.root / "parent-link");
        CHECK_FALSE(file::resolvePaths(fixture.input, fixture.root / "parent-link" / "input.qtp").has_value());
    } else {
        MESSAGE("native symlink creation unavailable: ", error.message());
    }

#ifdef _WIN32
    CHECK_FALSE(file::resolvePaths(fixture.input, fixture.root / "INPUT.QTP").has_value());
#endif
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("finite physical work path limits and destructor preserve ownership") {
    Fixture fixture;
    auto limits = file::FileLimits{};
    limits.pathUnits = 1;
    CHECK_FALSE(file::resolvePaths(fixture.input, fixture.output, limits).has_value());
    limits = {};
    limits.workUnits = 0;
    CHECK_FALSE(file::readInput(fixture.paths(), limits).has_value());
    CHECK_FALSE(file::OutputFile::create(fixture.paths(), limits).has_value());
    Probe probe;
    {
        auto owner = file::OutputFile::create(fixture.paths(), {}, probe.operations());
        REQUIRE(owner.has_value());
        auto moved = std::move(*owner);
        REQUIRE(moved.write(outputBytes()).has_value());
    }

    CHECK(probe.closes == 1);
    CHECK(probe.removes == 1);
    CHECK_FALSE(fs::exists(probe.temporary));
    CHECK_FALSE(fs::exists(fixture.output));
    CHECK(load(fixture.input) == INPUT_BYTES);
}

TEST_CASE("physical byte-operation budgets accept exact work and refuse one below before publication") {
    Fixture fixture;
    const auto paths = fixture.paths();

    for (const auto work : {INPUT_BYTES.size() + 3, INPUT_BYTES.size() + 4, INPUT_BYTES.size() + 5}) {
        file::FileLimits limits;
        limits.workUnits = work;
        Probe probe;
        const auto result = file::readInput(paths, limits, probe.operations());
        CHECK(result.has_value() == (work >= INPUT_BYTES.size() + 4));
        CHECK(probe.closes == 1);
    }

    for (const auto work : {OUTPUT.size() + 5, OUTPUT.size() + 6, OUTPUT.size() + 7}) {
        file::FileLimits limits;
        limits.workUnits = work;
        Probe probe;
        auto output = file::OutputFile::create(paths, limits, probe.operations());
        REQUIRE(output.has_value());
        REQUIRE(output->write(outputBytes()).has_value());
        const auto result = output->finishPublish();
        CHECK(result.has_value() == (work >= OUTPUT.size() + 6));

        if (work >= OUTPUT.size() + 6) {
            CHECK(load(fixture.output) == OUTPUT);
            fs::remove(fixture.output);
        } else {
            // The create-only link already succeeded; a later work refusal never rolls it back.
            CHECK(result.error().published);
            CHECK(load(fixture.output) == OUTPUT);
            fs::remove(fixture.output);
        }

        CHECK(probe.closes == 1);
        CHECK(probe.removes == 1);
        CHECK_FALSE(fs::exists(probe.temporary));
    }

    Probe partial;
    partial.partial = true;
    file::FileLimits limits;
    limits.inputBytes = INPUT_BYTES.size();
    limits.workUnits = 4 * INPUT_BYTES.size() + 68;
    const auto input = file::readInput(paths, limits, partial.operations());
    REQUIRE(input.has_value());
    CHECK(input->capacity() <= limits.inputBytes);
    CHECK(load(fixture.input) == INPUT_BYTES);
    std::string embedded = fixture.output.string();
    embedded.push_back('\0');
    embedded += "ignored";
    CHECK_FALSE(file::resolvePaths(fixture.input, fs::path(embedded)).has_value());
}

TEST_CASE("read and write failures after real partial progress do not authenticate or publish prefixes") {
    Fixture fixture;
    Probe input;
    input.fault = Fault::READ;
    input.partial = true;
    input.afterProgress = true;
    const auto read = file::readInput(fixture.paths(), {}, input.operations());
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == "READ_FAILED");
    CHECK(input.reads == 2);
    CHECK(input.closes == 1);

    for (const auto fault : {Fault::WRITE, Fault::ZERO_WRITE}) {
        Probe probe;
        probe.fault = fault;
        probe.partial = true;
        probe.afterProgress = true;
        auto output = file::OutputFile::create(fixture.paths(), {}, probe.operations());
        REQUIRE(output.has_value());
        const auto result = output->write(outputBytes());
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == (fault == Fault::WRITE ? "WRITE_FAILED" : "ZERO_PROGRESS"));
        CHECK(probe.writes == 2);
        CHECK(probe.closes == 1);
        CHECK(probe.removes == 1);
        CHECK_FALSE(fs::exists(probe.temporary));
        CHECK_FALSE(fs::exists(fixture.output));
    }

    CHECK(load(fixture.input) == INPUT_BYTES);
}
