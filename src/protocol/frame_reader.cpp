#include "qd/protocol/frame_reader.hpp"

#include "qd/encoding/compact.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace qd::protocol {
namespace {
constexpr std::size_t MAX_COMPACT_PREFIX_BYTES = 9; ///< Full-width compact long escape plus eight bytes.

/** Validate arithmetic and storage compatibility before any allocation.
 * @param limits Caller-specified body and pending budgets.
 * @return Fixed byte-element allocation size; throws invalid_argument for incompatible budgets.
 */
std::size_t checkedStorageBytes(FrameLimits limits) {
    if (limits.maximumBodyBytes > std::numeric_limits<std::int64_t>::max() ||
        limits.maximumBodyBytes > std::numeric_limits<std::size_t>::max() - MAX_COMPACT_PREFIX_BYTES) {
        throw std::invalid_argument("Frame body budget cannot accommodate its compact prefix");
    }

    const auto size = limits.maximumBodyBytes + MAX_COMPACT_PREFIX_BYTES;

    if (limits.maximumPendingBytes < size) {
        throw std::invalid_argument("Pending budget must accommodate maximum body plus nine bytes");
    }

    return size;
}
} // namespace

FrameReader::FrameReader(FrameLimits limits)
    : limits_(limits), storageBytes_(checkedStorageBytes(limits)), storage_(new std::byte[storageBytes_]) {
}

FeedResult FrameReader::feed(std::span<const std::byte> input) {
    if (status_ == FrameStatus::FAILED || status_ == FrameStatus::FINISHED) {
        return {0, status_, std::nullopt};
    }

    std::size_t accepted = 0;

    while (true) {
        if (prefixBytes_ == 0 && pendingBytes_ != 0) {
            const auto length = encoding::readCompactLong(pending());

            if (length.status == encoding::Status::OK) {
                if (length.value < 0) {
                    error_ = FrameError::INVALID_LENGTH;
                    break;
                }

                if (static_cast<std::uint64_t>(length.value) > limits_.maximumBodyBytes) {
                    error_ = FrameError::BODY_LIMIT;
                    break;
                }

                prefixBytes_ = length.consumed;
                bodyBytes_ = static_cast<std::size_t>(length.value);
            }
        }

        if (prefixBytes_ != 0 && pendingBytes_ == prefixBytes_ + bodyBytes_) {
            Frame frame{FrameKind::EMPTY_HEARTBEAT, std::nullopt, {}, totalAccepted_ - pendingBytes_, totalAccepted_};

            if (bodyBytes_ != 0) {
                const auto body = pending().subspan(prefixBytes_, bodyBytes_);
                const auto type = encoding::readCompactLong(body);

                if (type.status != encoding::Status::OK) {
                    error_ = FrameError::TRUNCATED_TYPE;
                    break;
                }

                if (type.value < 0 || type.value > std::numeric_limits<std::int32_t>::max()) {
                    error_ = FrameError::INVALID_TYPE;
                    break;
                }

                frame.kind = FrameKind::MESSAGE;
                frame.type = static_cast<std::int32_t>(type.value);
                const auto payload = body.subspan(type.consumed);
                try {
                    frame.payload.assign(payload.begin(), payload.end());
                } catch (const std::bad_alloc &) {
                    error_ = FrameError::ALLOCATION_FAILURE;
                    break;
                } catch (const std::length_error &) {
                    error_ = FrameError::ALLOCATION_FAILURE;
                    break;
                }
            }

            pendingBytes_ = 0;
            prefixBytes_ = 0;
            bodyBytes_ = 0;

            return {accepted, FrameStatus::FRAME_READY, std::move(frame)};
        }

        if (accepted == input.size()) {
            return {accepted, FrameStatus::NEED_INPUT, std::nullopt};
        }

        // Probe length one byte at a time (at most nine); copy only the declared body's remainder.
        const auto needed = prefixBytes_ == 0 ? 1 : prefixBytes_ + bodyBytes_ - pendingBytes_;
        const auto offsetRoom = std::numeric_limits<std::uint64_t>::max() - totalAccepted_;

        if (offsetRoom == 0) {
            error_ = FrameError::OFFSET_OVERFLOW;
            break;
        }

        // Accept the representable prefix even at offset exhaustion, independently of feed partitioning.
        const auto count =
            static_cast<std::size_t>(std::min<std::uint64_t>(std::min(needed, input.size() - accepted), offsetRoom));
        std::copy_n(input.data() + accepted, count, storage_.get() + pendingBytes_);
        accepted += count;
        pendingBytes_ += count;
        totalAccepted_ += count;
    }

    status_ = FrameStatus::FAILED;

    return {accepted, status_, std::nullopt};
}

FrameStatus FrameReader::finish() noexcept {
    if (status_ == FrameStatus::FAILED || status_ == FrameStatus::FINISHED) {
        return status_;
    }

    if (pendingBytes_ != 0) {
        error_ = FrameError::TRUNCATED_INPUT;
        status_ = FrameStatus::FAILED;
    } else {
        status_ = FrameStatus::FINISHED;
    }

    return status_;
}

void FrameReader::reset() noexcept {
    pendingBytes_ = 0;
    prefixBytes_ = 0;
    bodyBytes_ = 0;
    totalAccepted_ = 0;
    status_ = FrameStatus::NEED_INPUT;
    error_ = FrameError::NONE;
}

std::span<const std::byte> FrameReader::pending() const noexcept {
    return {storage_.get(), pendingBytes_};
}

FrameError FrameReader::error() const noexcept {
    return error_;
}

std::uint64_t FrameReader::errorOffset() const noexcept {
    return totalAccepted_ - pendingBytes_;
}

std::uint64_t FrameReader::totalAccepted() const noexcept {
    return totalAccepted_;
}

std::size_t FrameReader::storageBytes() const noexcept {
    return storageBytes_;
}

} // namespace qd::protocol
