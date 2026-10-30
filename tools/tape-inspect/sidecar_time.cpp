// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "sidecar_time.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <limits>
#include <new>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::int64_t MILLIS_PER_MINUTE = 60000;
constexpr std::int64_t MILLIS_PER_HOUR = 3600000;
constexpr std::int64_t MILLIS_PER_DAY = 86400000;
constexpr std::size_t CALENDAR_BYTES = 24;
constexpr std::uint64_t SIGN_BIT = std::uint64_t{1} << 63;
constexpr unsigned PACKED_SECONDS_SHIFT = 32;
constexpr unsigned PACKED_MILLIS_SHIFT = 22;

/** Parse unsigned decimal without overflow or ambient locale.
 * @param text Borrowed nonempty digits.
 * @param maximum Largest admitted value.
 * @param result Receives the exact value only on success.
 * @return Whether every digit and intermediate value is valid.
 */
bool decimal(std::string_view text, std::uint64_t maximum, std::uint64_t &result) noexcept {
    if (text.empty()) {
        return false;
    }

    std::uint64_t value = 0;

    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }

        const auto digit = static_cast<std::uint64_t>(character - '0');

        if (value > maximum / 10 || (value == maximum / 10 && digit > maximum % 10)) {
            return false;
        }

        value = value * 10 + digit;
    }

    result = value;

    return true;
}

/** Parse a signed position, including the exact minimum without signed overflow.
 * @param text Borrowed optional-sign decimal text.
 * @param result Receives exact signed position on success.
 * @return Whether the signed64 domain and full lexical form match.
 */
bool positionValue(std::string_view text, std::int64_t &result) noexcept {
    const bool negative = !text.empty() && text.front() == '-';

    if (!text.empty() && (negative || text.front() == '+')) {
        text.remove_prefix(1);
    }

    std::uint64_t magnitude = 0;

    if (!decimal(text, negative ? SIGN_BIT : SIGN_BIT - 1, magnitude)) {
        return false;
    }

    result =
        negative ? std::bit_cast<std::int64_t>(std::uint64_t{0} - magnitude) : static_cast<std::int64_t>(magnitude);

    return true;
}

/** Parse only fixed producer calendar text, with explicit numeric timezone.
 * @param text Borrowed exact24-character producer timestamp.
 * @param result Receives absolute milliseconds.
 * @return Whether the calendar and numeric timezone are in the finite admitted domain.
 */
bool calendarValue(std::string_view text, std::int64_t &result) noexcept {
    if (text.size() != CALENDAR_BYTES || text[8] != '-' || text[15] != '.' || (text[19] != '+' && text[19] != '-')) {
        return false;
    }

    std::uint64_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, millis = 0;
    std::uint64_t zoneHour = 0, zoneMinute = 0;

    if (!decimal(text.substr(0, 4), 9999, year) || year == 0 || !decimal(text.substr(4, 2), 12, month) ||
        !decimal(text.substr(6, 2), 31, day) || !decimal(text.substr(9, 2), 23, hour) ||
        !decimal(text.substr(11, 2), 59, minute) || !decimal(text.substr(13, 2), 59, second) ||
        !decimal(text.substr(16, 3), 999, millis) || !decimal(text.substr(20, 2), 23, zoneHour) ||
        !decimal(text.substr(22, 2), 59, zoneMinute)) {
        return false;
    }

    const auto date = std::chrono::year{static_cast<int>(year)} / std::chrono::month{static_cast<unsigned>(month)} /
                      std::chrono::day{static_cast<unsigned>(day)};

    if (!date.ok()) {
        return false;
    }

    // Years1..9999 bound days and all following products well inside signed64.
    const auto days = static_cast<std::int64_t>(std::chrono::sys_days{date}.time_since_epoch().count());
    const auto offset = static_cast<std::int64_t>(zoneHour) * MILLIS_PER_HOUR +
                        static_cast<std::int64_t>(zoneMinute) * MILLIS_PER_MINUTE;
    result = days * MILLIS_PER_DAY + static_cast<std::int64_t>(hour) * MILLIS_PER_HOUR +
             static_cast<std::int64_t>(minute) * MILLIS_PER_MINUTE +
             static_cast<std::int64_t>(second) * MILLIS_PER_SECOND + static_cast<std::int64_t>(millis) +
             (text[19] == '+' ? -offset : offset);

    return true;
}

/** Parse the shared finite LONG/TEXT grammar without relative time.
 * @param text Borrowed exact timestamp bytes.
 * @param result Receives exact absolute milliseconds on success.
 * @return Typed lexical/domain result.
 */
SidecarError timestampValue(std::string_view text, std::int64_t &result) noexcept {
    if (!text.empty() && text.front() == '-') {
        return SidecarError::UNSUPPORTED_TIME;
    }

    if (text.size() == CALENDAR_BYTES) {
        return calendarValue(text, result) ? SidecarError::NONE : SidecarError::MALFORMED_TIME;
    }

    std::uint64_t value = 0;

    if (!decimal(text, SIGN_BIT - 1, value) || (text.size() > 1 && text.front() == '0')) {
        return SidecarError::MALFORMED_TIME;
    }

    result = static_cast<std::int64_t>(value);

    return SidecarError::NONE;
}
} // namespace

bool SidecarMark::reached(std::uint64_t logicalPosition) const noexcept {
    return position < 0 || static_cast<std::uint64_t>(position) <= logicalPosition;
}

std::int64_t SidecarMark::packed() const noexcept {
    auto seconds = timestampMs / MILLIS_PER_SECOND;
    auto millis = timestampMs % MILLIS_PER_SECOND;

    if (millis < 0) {
        --seconds;
        millis += MILLIS_PER_SECOND;
    }

    seconds = std::clamp(seconds, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
                         static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));
    const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(seconds)) << PACKED_SECONDS_SHIFT) |
                      (static_cast<std::uint64_t>(millis) << PACKED_MILLIS_SHIFT);

    return std::bit_cast<std::int64_t>(bits);
}

SidecarTime::SidecarTime(SidecarFormat format, SidecarLimits limits) noexcept : limits_(limits) {
    if ((format != SidecarFormat::LONG && format != SidecarFormat::TEXT) || limits.lineBytes > line_.max_size() ||
        limits.lineBytes > std::numeric_limits<std::uint64_t>::max()) {
        diagnostic_ = {SidecarError::INVALID_ARGUMENT, 0, 0, 0, "configuration"};
    }
}

void SidecarTime::fail(SidecarError error, std::string_view stage, std::size_t column) noexcept {
    if (diagnostic_.error == SidecarError::NONE) {
        diagnostic_ = {error, counters_.lines, column, counters_.inputBytes, stage};
    }
}

bool SidecarTime::chargeWork(std::uint64_t count) noexcept {
    if (count > limits_.workBytes - counters_.workBytes) {
        fail(SidecarError::WORK_LIMIT, "work", lineSize_);

        return false;
    }

    counters_.workBytes += count;

    return true;
}

SidecarFeed SidecarTime::completeLine() noexcept {
    if (counters_.lines == limits_.lines) {
        fail(SidecarError::LINE_COUNT_LIMIT, "line-count");

        return {0, SidecarStatus::FAILED, {}};
    }

    ++counters_.lines;

    if (!colon_) {
        lineSize_ = 0;

        return {};
    }

    if (counters_.marks == limits_.marks) {
        fail(SidecarError::MARK_LIMIT, "mark-count");

        return {0, SidecarStatus::FAILED, {}};
    }

    if (!chargeWork(static_cast<std::uint64_t>(lineSize_))) {
        return {0, SidecarStatus::FAILED, {}};
    }

    const std::string_view text{line_.data(), lineSize_};
    SidecarMark mark;
    const auto error = timestampValue(text.substr(0, *colon_), mark.timestampMs);

    if (error != SidecarError::NONE) {
        fail(error, "timestamp");

        return {0, SidecarStatus::FAILED, {}};
    }

    if (!positionValue(text.substr(*colon_ + 1), mark.position)) {
        fail(SidecarError::MALFORMED_POSITION, "position", *colon_ + 1);

        return {0, SidecarStatus::FAILED, {}};
    }

    mark.ordinal = counters_.marks++;

    if (mark.position >= 0) {
        counters_.maximumPosition = std::max(counters_.maximumPosition, static_cast<std::uint64_t>(mark.position));
    }

    lineSize_ = 0;
    colon_.reset();

    return {0, SidecarStatus::MARK, mark};
}

SidecarFeed SidecarTime::feed(std::span<const std::byte> input) noexcept {
    if (diagnostic_.error != SidecarError::NONE) {
        return {0, SidecarStatus::FAILED, {}};
    }

    if (closing_) {
        fail(SidecarError::INVALID_ARGUMENT, "after-end");

        return {0, SidecarStatus::FAILED, {}};
    }

    if (!input.empty() && line_.empty() && limits_.lineBytes == 0) {
        fail(SidecarError::LINE_LIMIT, "line");

        return {0, SidecarStatus::FAILED, {}};
    }

    if (!input.empty() && line_.empty() && limits_.lineBytes > limits_.retainedBytes) {
        fail(SidecarError::RETAINED_LIMIT, "retained");

        return {0, SidecarStatus::FAILED, {}};
    }

    std::size_t accepted = 0;

    for (const auto byte : input) {
        if (counters_.inputBytes == limits_.inputBytes) {
            fail(SidecarError::INPUT_LIMIT, "input", lineSize_);

            break;
        }

        if (!chargeWork(1)) {
            break;
        }

        const auto character = static_cast<char>(std::to_integer<unsigned char>(byte));
        ++accepted;
        ++counters_.inputBytes;

        if (afterCr_) {
            afterCr_ = false;

            if (character == '\n') {
                continue;
            }
        }

        if (character == '\r' || character == '\n') {
            afterCr_ = character == '\r';
            auto result = completeLine();

            if (result.status != SidecarStatus::NEED_INPUT) {
                result.accepted = accepted;

                return result;
            }

            continue;
        }

        if (lineSize_ == limits_.lineBytes) {
            fail(SidecarError::LINE_LIMIT, "line", lineSize_);

            break;
        }

        if (line_.empty()) {
            if (limits_.lineBytes > limits_.retainedBytes) {
                fail(SidecarError::RETAINED_LIMIT, "retained", lineSize_);

                break;
            }

            try {
                line_.resize(limits_.lineBytes);
            } catch (const std::bad_alloc &) {
                fail(SidecarError::ALLOCATION, "allocation", lineSize_);

                break;
            } catch (...) {
                fail(SidecarError::INVALID_ARGUMENT, "allocation-size", lineSize_);

                break;
            }
        }

        if (character == ':') {
            colon_ = lineSize_;
        }

        line_[lineSize_++] = character;
    }

    return {accepted, diagnostic_.error == SidecarError::NONE ? SidecarStatus::NEED_INPUT : SidecarStatus::FAILED, {}};
}

SidecarFeed SidecarTime::endInput() noexcept {
    if (diagnostic_.error != SidecarError::NONE) {
        return {0, SidecarStatus::FAILED, {}};
    }

    closing_ = true;

    if (!ended_ && lineSize_ != 0) {
        auto result = completeLine();

        if (result.status != SidecarStatus::NEED_INPUT) {
            return result;
        }
    }

    ended_ = true;

    return {0, SidecarStatus::END, {}};
}

SidecarError SidecarTime::finish(std::uint64_t dataLogicalBytes) noexcept {
    if (diagnostic_.error != SidecarError::NONE) {
        return diagnostic_.error;
    }

    if (!ended_) {
        fail(SidecarError::NOT_FINISHED, "finish");
    } else if (!finished_ && counters_.maximumPosition > dataLogicalBytes) {
        fail(SidecarError::POSITION_AFTER_DATA, "data-eof");
    }

    finished_ = true;

    return diagnostic_.error;
}

SidecarCounters SidecarTime::counters() const noexcept {
    return counters_;
}

SidecarDiagnostic SidecarTime::diagnostic() const noexcept {
    return diagnostic_;
}
} // namespace org::ttldtor::qdcxx::tape_inspect
