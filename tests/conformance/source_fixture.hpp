// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

namespace org::ttldtor::qdcxx::test_support::source_fixture {
namespace encoding = org::ttldtor::qdcxx::encoding;
constexpr std::size_t RECORDS_PER_FRAME = 256;

/** Append a finite scalar to fixture storage; no input/output source is retained.
 * @param bytes Owned bounded fixture message storage, appended on success.
 * @param value Exact signed compact-long value.
 */
inline void compact(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> encoded{};
    const auto result = encoding::writeCompactLong(value, encoded);

    if (result.status != encoding::Status::OK) {
        throw std::runtime_error("fixture compact encoding");
    }

    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append one UTF-16 protocol string to bounded fixture storage.
 * @param bytes Owned bounded fixture message storage, appended on success.
 * @param value Borrowed exact UTF-16 units, at most 128.
 */
inline void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 256> encoded{};
    const auto result = encoding::writeUtfString(value, encoded, {256, 128});

    if (result.status != encoding::Status::OK) {
        throw std::runtime_error("fixture text encoding");
    }

    bytes.insert(bytes.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap a complete message body with an explicit type and compact envelope.
 * @param type Exact one-byte nonnegative message identity used by this fixture.
 * @param payload Borrowed complete bounded message body.
 * @return Owning binary envelope, or an encoding/allocation exception.
 */
inline std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> result;

    compact(result, static_cast<std::int64_t>(payload.size() + 1));
    compact(result, type);
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

/** Supply an independent self-contained BINARY/NONE descriptor with one STREAM_DATA family.
 * @return Owning fixed descriptor envelope with explicit NONE time.
 */
inline std::vector<std::byte> header() {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};

    compact(body, 3);
    text(body, u"type");
    text(body, u"tape");
    text(body, u"opt");
    text(body, u"");
    text(body, u"time");
    text(body, u"NONE");
    compact(body, 1);
    compact(body, 15);
    text(body, u"STREAM_DATA");
    compact(body, 0);
    compact(body, 0);

    return frame(1, body);
}

/** Declare the full one-field wire identity R.V, exact serial type8.
 * @return Owning fixed description envelope; no external schema is needed.
 */
inline std::vector<std::byte> description() {
    std::vector<std::byte> body;

    compact(body, 0);
    text(body, u"R");
    compact(body, 1);
    text(body, u"V");
    compact(body, 8);

    return frame(2, body);
}

/** Build at most 256 explicit-empty-symbol records; values cycle independently of chunking.
 * @param first Aggregate zero-based first record ordinal.
 * @param count Number of records in this frame, at most 256.
 * @return One owning bounded STREAM_DATA envelope.
 */
inline std::vector<std::byte> data(std::size_t first, std::size_t count) {
    std::vector<std::byte> body;

    body.reserve(RECORDS_PER_FRAME * 3);

    for (std::size_t i = 0; i < count; ++i) {
        body.push_back(std::byte{0xfe});
        compact(body, 0);
        compact(body, static_cast<std::int64_t>((first + i) % 17));
    }

    return frame(15, body);
}

/** Write only the named owned temporary fixture, streaming bounded frames.
 * Called by qdcxx_large_tape_tests --fixture PATH COUNT, or its streaming test mode.
 * The returned physical byte count is independent of inspector counters; all failures throw.
 * @param path Explicit owned temporary destination, opened with truncation.
 * @param records Positive finite generated-record count.
 * @return Actual bytes successfully written and physically closed.
 */
inline std::uint64_t write(const std::filesystem::path &path, std::size_t records) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        throw std::runtime_error("fixture open");
    }

    std::uint64_t bytes = 0;
    const auto emit = [&](const auto &buffer) {
        output.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

        if (!output) {
            throw std::runtime_error("fixture write");
        }

        bytes += buffer.size();
    };

    emit(header());
    emit(description());

    for (std::size_t first = 0; first < records; first += RECORDS_PER_FRAME) {
        emit(data(first, std::min(RECORDS_PER_FRAME, records - first)));
    }

    output.flush();

    if (!output) {
        throw std::runtime_error("fixture flush");
    }

    output.close();

    if (!output) {
        throw std::runtime_error("fixture close");
    }

    return bytes;
}
} // namespace org::ttldtor::qdcxx::test_support::source_fixture
