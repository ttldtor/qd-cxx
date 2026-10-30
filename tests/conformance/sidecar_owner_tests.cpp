// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../tools/tape-inspect/sidecar_time.hpp"
#include "compressed_input.hpp"
#include "qd/protocol/tape_reader.hpp"
#include <array>
#include <cstring>
#include <doctest/doctest.h>
#include <memory>
#include <sstream>
#include <zlib.h>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace p = org::ttldtor::qdcxx::protocol;
/** Test-local causal category, separate from bounded secondary cleanup flags. */
enum class Cause { NONE, OPEN, READ, PARSE, SIDECAR, CANCEL, CLOSE, FLUSH };

/** Finite input buffer with an injected read checkpoint and observable operation count. */
class SourceBuffer : public std::streambuf {
    std::string bytes_;
    std::size_t position_ = 0;

    public:
    std::size_t reads = 0;                                        ///< Physical read attempts, including EOF probes.
    std::size_t failAt = std::numeric_limits<std::size_t>::max(); ///< First forbidden byte checkpoint.

    /** Own the finite test bytes.
     * @param bytes Synthetic input, moved into this buffer.
     */
    explicit SourceBuffer(std::string bytes) : bytes_(std::move(bytes)) {
    }

    protected:
    /** Supply a bounded physical block, failing only this source at its selected checkpoint.
     * @param output Borrowed destination.
     * @param count Requested size.
     * @return Actual bytes copied before EOF.
     */
    std::streamsize xsgetn(char *output, std::streamsize count) override {
        ++reads;

        if (position_ >= failAt) {
            throw std::ios_base::failure("injected source read");
        }

        const auto length = std::min({static_cast<std::size_t>(count), bytes_.size() - position_, failAt - position_});
        std::memcpy(output, bytes_.data() + position_, length);
        position_ += length;

        return static_cast<std::streamsize>(length);
    }

    /** Keep speculative availability from changing the selected read schedule.
     * @return Zero; actual reads establish EOF.
     */
    std::streamsize showmanyc() override {
        return 0;
    }
};

/** Independent owned stream with a close boundary that can fail without throwing. */
struct OwnedSource {
    SourceBuffer buffer;       ///< Own finite source bytes and read observations.
    std::istream stream;       ///< Borrows this object's buffer until destruction.
    bool opened = true;        ///< A partial-open test may leave one member unopened.
    bool closeFails = false;   ///< Injected close result.
    bool adapterAlive = false; ///< Cleared only after the actual borrowed adapter destructor returns.
    unsigned closes = 0;       ///< At most one close per opened member.

    /** Prepare one finite owned source.
     * @param bytes Synthetic input.
     */
    explicit OwnedSource(std::string bytes) : buffer(std::move(bytes)), stream(&buffer) {
    }

    /** Attempt exactly one physical close without input work.
     * @return Whether this source closed successfully.
     */
    bool close() {
        REQUIRE(opened);
        REQUIRE(closes == 0);
        REQUIRE_FALSE(adapterAlive);
        ++closes;

        return !closeFails;
    }
};

/** Bounded owner proof result, independent of scanner/parser completion states. */
struct PairResult {
    Cause primary = Cause::NONE;                      ///< First observed causal failure.
    bool dataCloseFailed = false;                     ///< Secondary data cleanup result.
    bool sidecarCloseFailed = false;                  ///< Secondary companion cleanup result.
    bool flushFailed = false;                         ///< Secondary output cleanup result.
    bool complete = false;                            ///< Set only after every close/flush succeeds.
    unsigned callbacks = 0;                           ///< Completed native parser envelopes submitted to the sink.
    std::array<std::size_t, 2> readsAtCancellation{}; ///< Physical reads at the cancellation checkpoint.
    std::array<unsigned, 5> cleanup{};                ///< Adapter releases, companion close, data close, sink flush.
};

/** Supply finite legacy header-only reader bounds without schema dependencies.
 * @return Bounded native parser configuration.
 */
p::TapeLimits parserLimits() {
    return {{4096, 4105},
            65536,
            128,
            {16, 8, {1024, 256}, 2048},
            {{10, 20, 20, 20, 200}, 20, {100, 100, 1000}},
            10000,
            {4096, 100, 200, 100, 200, 1000, {4096, 100, 100}}};
}

/** Create a complete finite protocol header plus empty marker.
 * @return Exact synthetic tape bytes.
 */
std::string tape() {
    const unsigned char bytes[]{23,  1,  68,  88,  80, 51,  2,   4,   116, 121, 112, 101, 4,
                                116, 97, 112, 101, 3,  111, 112, 116, 0,   0,   0,   0};

    return {reinterpret_cast<const char *>(bytes), sizeof(bytes)};
}

/** Produce an actual finite GZIP envelope using the approved private zlib build.
 * @param input Borrowed synthetic logical bytes.
 * @return Owning compressed bytes.
 */
std::string gzip(std::string_view input) {
    z_stream state{};
    REQUIRE(deflateInit2(&state, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    std::array<char, 512> output{};
    state.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
    state.avail_in = static_cast<uInt>(input.size());
    state.next_out = reinterpret_cast<Bytef *>(output.data());
    state.avail_out = static_cast<uInt>(output.size());
    REQUIRE(deflate(&state, Z_FINISH) == Z_STREAM_END);
    const auto size = state.total_out;
    REQUIRE(deflateEnd(&state) == Z_OK);

    return {output.data(), size};
}

/** Append an unsigned little-endian ZIP word without host-endian assumptions.
 * @param output Owning ZIP bytes.
 * @param value Low-order bits to encode.
 * @param width Explicit ZIP16/ZIP32 width.
 */
void zipWord(std::string &output, std::uint32_t value, unsigned width) {
    for (unsigned index = 0; index < width; ++index) {
        output.push_back(static_cast<char>((value >> (index * 8)) & 255));
    }
}

/** Produce a finite one-file stored ZIP32 with consistent local/directory CRC.
 * @param input Borrowed synthetic logical bytes.
 * @return Owning complete archive, including actual central directory and EOCD.
 */
std::string zip(std::string_view input) {
    const auto crc = static_cast<std::uint32_t>(
        crc32(0, reinterpret_cast<const Bytef *>(input.data()), static_cast<uInt>(input.size())));
    const auto size = static_cast<std::uint32_t>(input.size());
    std::string bytes;
    zipWord(bytes, 0x04034b50, 4);

    for (const auto value : {20, 0, 0, 0, 0}) {
        zipWord(bytes, static_cast<std::uint32_t>(value), 2);
    }

    for (const auto value : {crc, size, size}) {
        zipWord(bytes, value, 4);
    }

    zipWord(bytes, 1, 2);
    zipWord(bytes, 0, 2);
    bytes += 'x';
    bytes += input;
    const auto directory = static_cast<std::uint32_t>(bytes.size());
    zipWord(bytes, 0x02014b50, 4);

    for (const auto value : {20, 20, 0, 0, 0, 0}) {
        zipWord(bytes, static_cast<std::uint32_t>(value), 2);
    }

    for (const auto value : {crc, size, size}) {
        zipWord(bytes, value, 4);
    }

    for (const auto value : {1, 0, 0, 0, 0}) {
        zipWord(bytes, static_cast<std::uint32_t>(value), 2);
    }

    zipWord(bytes, 0, 4);
    zipWord(bytes, 0, 4);
    bytes += 'x';
    const auto directoryBytes = static_cast<std::uint32_t>(bytes.size()) - directory;
    zipWord(bytes, 0x06054b50, 4);
    zipWord(bytes, 0, 2);
    zipWord(bytes, 0, 2);
    zipWord(bytes, 1, 2);
    zipWord(bytes, 1, 2);
    zipWord(bytes, directoryBytes, 4);
    zipWord(bytes, directory, 4);
    zipWord(bytes, 0, 2);

    return bytes;
}

/** Select a finite test envelope.
 * @param kind Explicit native container.
 * @param input Borrowed synthetic bytes.
 * @return Independent owning encoded bytes.
 */
std::string envelope(ti::ContainerKind kind, std::string_view input) {
    return kind == ti::ContainerKind::GZIP  ? gzip(input)
           : kind == ti::ContainerKind::ZIP ? zip(input)
                                            : std::string(input);
}

/** Exercise real scanner/container/parser owners without a public source framework.
 * @param data Owned data stream.
 * @param companion Owned companion stream.
 * @param kind Explicit envelope for each member.
 * @param cancel Stop the sink at its first complete parser envelope.
 * @param failFlush Inject final output flush failure.
 * @return Primary/cleanup/completion evidence after both close attempts.
 */
PairResult runPair(OwnedSource &data, OwnedSource &companion, ti::ContainerKind kind, bool cancel = false,
                   bool failFlush = false) {
    PairResult result;
    auto record = [&](Cause cause) {
        if (result.primary == Cause::NONE) {
            result.primary = cause;

            if (cause == Cause::CANCEL) {
                result.readsAtCancellation = {data.buffer.reads, companion.buffer.reads};
            }
        }
    };
    ti::ContainerLimits limits;
    limits.chunkBytes = 8;
    std::unique_ptr<ti::CompressedInput> dataAdapter, timeAdapter;
    ti::SidecarTime scanner(ti::SidecarFormat::LONG);
    p::TapeReader parser(parserLimits());
    bool dataComplete = false, timeComplete = false;
    std::uint64_t logicalBytes = 0;

    if (!data.opened || !companion.opened) {
        record(Cause::OPEN);
    } else {
        dataAdapter = std::make_unique<ti::CompressedInput>(data.stream, kind, limits);
        timeAdapter = std::make_unique<ti::CompressedInput>(companion.stream, kind, limits);
        data.adapterAlive = true;
        companion.adapterAlive = true;
        std::array<std::byte, 8> scratch{};

        // This header-only owner proof uses marks at0; frame/record chronology is a separate test.
        while (!timeComplete && result.primary == Cause::NONE) {
            const auto read = timeAdapter->read(scratch);

            if (read.error != ti::ContainerError::NONE) {
                record(Cause::READ);
            }

            std::size_t position = 0;

            while (position < read.produced) {
                const auto parsed = scanner.feed(std::span(scratch).first(read.produced).subspan(position));
                position += parsed.accepted;

                if (parsed.status == ti::SidecarStatus::FAILED) {
                    record(Cause::SIDECAR);
                    break;
                }
            }

            timeComplete = read.physicalComplete;
        }

        while (!dataComplete && result.primary == Cause::NONE) {
            const auto read = dataAdapter->read(scratch);

            if (read.error != ti::ContainerError::NONE) {
                record(Cause::READ);
            }

            std::size_t position = 0;

            while (position < read.produced) {
                const auto parsed = parser.feed(std::span(scratch).first(read.produced).subspan(position));
                position += parsed.accepted;

                if (parsed.frame) {
                    ++result.callbacks;

                    if (cancel) {
                        record(Cause::CANCEL);
                        break;
                    }
                }

                if (parsed.status != p::TapeStatus::NEED_INPUT && parsed.status != p::TapeStatus::EVENT) {
                    record(Cause::PARSE);
                    break;
                }
            }

            dataComplete = read.physicalComplete;
            logicalBytes += position;
        }

        if (result.primary == Cause::NONE) {
            if (dataAdapter->finish() != ti::ContainerError::NONE ||
                timeAdapter->finish() != ti::ContainerError::NONE) {
                record(Cause::READ);
            }

            auto ended = scanner.endInput();

            if (ended.mark) {
                ended = scanner.endInput();
            }

            if (ended.status != ti::SidecarStatus::END || scanner.finish(logicalBytes) != ti::SidecarError::NONE) {
                record(Cause::SIDECAR);
            }

            if (parser.finish() != p::TapeStatus::FINISHED) {
                record(Cause::PARSE);
            }
        }
    }

    const auto dataReads = data.buffer.reads;
    const auto timeReads = companion.buffer.reads;
    dataAdapter.reset();
    data.adapterAlive = false;
    result.cleanup[0] = 1;
    timeAdapter.reset();
    companion.adapterAlive = false;
    result.cleanup[1] = 2;
    REQUIRE_FALSE(result.complete);

    if (companion.opened) {
        result.cleanup[2] = 3;
        result.sidecarCloseFailed = !companion.close();

        if (result.sidecarCloseFailed) {
            record(Cause::CLOSE);
        }
    }

    if (data.opened) {
        result.cleanup[3] = 4;
        result.dataCloseFailed = !data.close();

        if (result.dataCloseFailed) {
            record(Cause::CLOSE);
        }
    }

    result.cleanup[4] = 5;
    result.flushFailed = failFlush;

    if (failFlush) {
        record(Cause::FLUSH);
    }

    CHECK(data.buffer.reads == dataReads);
    CHECK(companion.buffer.reads == timeReads);

    if (result.primary == Cause::CANCEL) {
        CHECK(data.buffer.reads == result.readsAtCancellation[0]);
        CHECK(companion.buffer.reads == result.readsAtCancellation[1]);
        CHECK(result.callbacks == 1);
    }

    result.complete = result.primary == Cause::NONE && dataComplete && timeComplete;

    return result;
}
} // namespace

TEST_CASE("paired native owner complete close flush and cancellation") {
    for (const auto kind : {ti::ContainerKind::NONE, ti::ContainerKind::GZIP, ti::ContainerKind::ZIP}) {
        for (const auto companion : {"", "0:0"}) {
            OwnedSource data(envelope(kind, tape()));
            OwnedSource time(envelope(kind, companion));
            const auto result = runPair(data, time, kind);
            CHECK(result.complete);
            CHECK(result.primary == Cause::NONE);
            CHECK(data.closes == 1);
            CHECK(time.closes == 1);
            CHECK(result.cleanup == std::array<unsigned, 5>{1, 2, 3, 4, 5});
        }
    }

    for (unsigned fault = 0; fault < 7; ++fault) {
        OwnedSource data(tape()), time("0:0\n");
        data.closeFails = fault == 0 || fault == 2 || fault == 3;
        time.closeFails = fault == 1 || fault == 2 || fault == 3;

        if (fault == 4) {
            time.opened = false;
        }

        if (fault == 5) {
            data.opened = false;
        }

        const auto result = runPair(data, time, ti::ContainerKind::NONE, fault == 3, fault == 6 || fault == 3);
        CHECK_FALSE(result.complete);
        CHECK(data.closes == static_cast<unsigned>(data.opened));
        CHECK(time.closes == static_cast<unsigned>(time.opened));
        CHECK(result.primary == (fault == 3                 ? Cause::CANCEL
                                 : fault == 4 || fault == 5 ? Cause::OPEN
                                 : fault == 6               ? Cause::FLUSH
                                                            : Cause::CLOSE));
    }
}

TEST_CASE("paired native owner read late CRC malformed tail and cleanup precedence") {
    for (bool failTime : {false, true}) {
        OwnedSource data(tape()), time("0:0\n");
        (failTime ? time : data).buffer.failAt = 0;
        data.closeFails = true;
        time.closeFails = true;
        const auto result = runPair(data, time, ti::ContainerKind::NONE, false, true);
        CHECK(result.primary == Cause::READ);
        CHECK(result.dataCloseFailed);
        CHECK(result.sidecarCloseFailed);
        CHECK(result.flushFailed);
        CHECK_FALSE(result.complete);
    }

    for (bool failTime : {false, true}) {
        auto dataBytes = gzip(tape());
        auto timeBytes = gzip("0:0\n");
        auto &bad = failTime ? timeBytes : dataBytes;
        bad[bad.size() - 8] ^= 1;
        OwnedSource data(std::move(dataBytes)), time(std::move(timeBytes));
        const auto result = runPair(data, time, ti::ContainerKind::GZIP);
        CHECK(result.primary == Cause::READ);
        CHECK_FALSE(result.complete);
        CHECK(data.closes == 1);
        CHECK(time.closes == 1);

        if (!failTime) {
            CHECK(result.callbacks > 0);
        }
    }

    for (const auto body : {"0:999\n", "0:0\nbad:bad\n"}) {
        OwnedSource data(tape()), time(body);
        const auto result = runPair(data, time, ti::ContainerKind::NONE);
        CHECK(result.primary == Cause::SIDECAR);
        CHECK_FALSE(result.complete);
    }

    OwnedSource corrupt("bad"), companion("0:0\n");
    corrupt.closeFails = true;
    companion.closeFails = true;
    const auto failedParse = runPair(corrupt, companion, ti::ContainerKind::NONE, false, true);
    CHECK(failedParse.primary == Cause::PARSE);
    CHECK(failedParse.dataCloseFailed);
    CHECK(failedParse.sidecarCloseFailed);
    CHECK(failedParse.flushFailed);
    CHECK_FALSE(failedParse.complete);
}
