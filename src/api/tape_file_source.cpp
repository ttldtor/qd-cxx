// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tape_file_source.hpp"
#include "../protocol/tape_data_limits.hpp"

#include <algorithm>
#include <limits>
#include <system_error>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
constexpr std::int32_t RAW_DATA = 5;
constexpr std::int32_t TICKER_DATA = 10;
constexpr std::int32_t STREAM_DATA = 15;
constexpr std::int32_t HISTORY_DATA = 20;
constexpr std::size_t SNAPSHOT_UNIT_FACTOR = 4;

std::size_t checkedProduct(std::size_t count, std::size_t units) {
    if (units != 0 && count > std::numeric_limits<std::size_t>::max() / units) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "tape-limit-product"});
    }

    return count * units;
}

std::ifstream openFile(const std::filesystem::path &path, std::size_t maximumBytes) {
    std::error_code error;

    if (!std::filesystem::is_regular_file(path, error)) {
        throw Error({ErrorCode::TAPE_IO_FAILURE, "tape-regular-file", error.value()});
    }

    const auto size = std::filesystem::file_size(path, error);

    if (error) {
        throw Error({ErrorCode::TAPE_IO_FAILURE, "tape-file-size", error.value()});
    }

    if (size > maximumBytes) {
        throw Error({ErrorCode::RESOURCE_LIMIT, "tape-file-size"});
    }

    std::ifstream stream(path, std::ios::binary);

    if (!stream.is_open()) {
        throw Error({ErrorCode::TAPE_IO_FAILURE, "tape-open"});
    }

    return stream;
}

protocol::TapeLimits readerLimits(protocol::LiveLimits limits, TapeSourceOptions options) {
    auto data = limits.data;

    for (const auto member : protocol::detail::TAPE_DATA_TOTALS) {
        data.*member = checkedProduct(data.*member, options.maximumFrames);
    }

    return {limits.frame,
            options.maximumFileBytes,
            options.maximumFrames,
            limits.protocol,
            limits.context,
            checkedProduct(limits.snapshotBytes, options.maximumFrames),
            data,
            options.maximumFrames,
            checkedProduct(limits.snapshotBytes, SNAPSHOT_UNIT_FACTOR),
            limits.data};
}

Diagnostic bridgeFailure(protocol::LiveError error) noexcept {
    const auto code = error == protocol::LiveError::WORK_LIMIT           ? ErrorCode::WORK_LIMIT
                      : error == protocol::LiveError::RESOURCE_LIMIT     ? ErrorCode::RESOURCE_LIMIT
                      : error == protocol::LiveError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                                                                         : ErrorCode::PROTOCOL_FAILURE;

    return {code, "tape-event-owner", 0, static_cast<std::uint32_t>(error)};
}

Diagnostic readerFailure(const protocol::TapeDiagnostic &failure) noexcept {
    const auto code = failure.error == protocol::TapeError::RESOURCE_LIMIT       ? ErrorCode::RESOURCE_LIMIT
                      : failure.error == protocol::TapeError::ALLOCATION_FAILURE ? ErrorCode::ALLOCATION_FAILURE
                      : failure.error == protocol::TapeError::UNSUPPORTED        ? ErrorCode::UNSUPPORTED_PROFILE
                                                                                 : ErrorCode::PROTOCOL_FAILURE;

    return {code, failure.stage, 0, static_cast<std::uint32_t>(failure.error)};
}

protocol::detail::TapeEventBridge makeBridge(protocol::LiveLimits limits, protocol::LiveWork &work) {
    auto bridge = protocol::detail::TapeEventBridge::create(limits, work);

    if (!bridge) {
        throw Error(bridgeFailure(bridge.error()));
    }

    return std::move(*bridge);
}
} // namespace

TapeFileSource::TapeFileSource(const std::filesystem::path &path, TapeSourceOptions options,
                               const core::LocalSchema &schema, protocol::LiveLimits limits,
                               core::SubscriptionFamily family, protocol::LiveWork &work)
    : file_(openFile(path, options.maximumFileBytes)), reader_(protocol::detail::TapeEventBridge::makeFixedSchemaReader(
                                                           schema, readerLimits(limits, options), limits.fieldTime)),
      bridge_(makeBridge(limits, work)), maximumBytes_(options.maximumFileBytes),
      familyMessage_(family == core::SubscriptionFamily::TICKER    ? TICKER_DATA
                     : family == core::SubscriptionFamily::HISTORY ? HISTORY_DATA
                                                                   : STREAM_DATA) {
}

std::expected<void, Diagnostic> TapeFileSource::read(protocol::LiveWork &work) {
    if (file_.eof()) {
        return {};
    }

    const auto count = std::min(bytes_.size(), maximumBytes_ - readBytes_);
    const auto charge = std::max(count, std::size_t{1});

    if (work.operations == 0 || charge > work.bytes) {
        return std::unexpected(Diagnostic{ErrorCode::WORK_LIMIT, "tape-file-read"});
    }

    --work.operations;
    work.bytes -= charge;

    if (count == 0) {
        if (file_.peek() != std::char_traits<char>::eof()) {
            return std::unexpected(Diagnostic{ErrorCode::RESOURCE_LIMIT, "tape-file-size"});
        }
    } else {
        file_.read(reinterpret_cast<char *>(bytes_.data()), static_cast<std::streamsize>(count));
    }

    if (file_.bad() || (file_.fail() && !file_.eof())) {
        return std::unexpected(Diagnostic{ErrorCode::TAPE_IO_FAILURE, "tape-file-read"});
    }

    first_ = 0;
    last_ = count == 0 ? 0 : static_cast<std::size_t>(file_.gcount());
    readBytes_ += last_;

    return {};
}

std::expected<std::optional<protocol::LiveEvent>, Diagnostic> TapeFileSource::next(protocol::LiveWork &work) {
    if (finished_) {
        return std::optional<protocol::LiveEvent>{};
    }

    if (!pending_) {
        if (first_ == last_) {
            const auto filled = read(work);

            if (!filled) {
                return std::unexpected(filled.error());
            }
        }

        if (first_ == last_ && file_.eof()) {
            if (reader_.finish() != protocol::TapeStatus::FINISHED) {
                return std::unexpected(readerFailure(reader_.diagnostic()));
            }

            finished_ = true;
            stop();

            return std::optional<protocol::LiveEvent>{};
        }

        if (work.operations == 0 || last_ - first_ > work.bytes) {
            return std::unexpected(Diagnostic{ErrorCode::WORK_LIMIT, "tape-frame-read"});
        }

        --work.operations;
        auto decoded = reader_.feed(std::span(bytes_).subspan(first_, last_ - first_));
        first_ += decoded.accepted;
        work.bytes -= decoded.accepted;

        if (decoded.status == protocol::TapeStatus::FAILED) {
            return std::unexpected(readerFailure(reader_.diagnostic()));
        }

        if (decoded.status != protocol::TapeStatus::EVENT || !decoded.frame || decoded.records.empty() ||
            (decoded.frame->type != RAW_DATA && decoded.frame->type != familyMessage_)) {
            return std::optional<protocol::LiveEvent>{};
        }

        pending_.emplace(std::move(decoded));
    }

    auto event = bridge_.copy(*pending_, familyMessage_, work);

    if (!event) {
        return std::unexpected(bridgeFailure(event.error()));
    }

    pending_.reset();

    return std::optional<protocol::LiveEvent>(std::move(*event));
}

bool TapeFileSource::finished() const noexcept {
    return finished_;
}

void TapeFileSource::stop() noexcept {
    pending_.reset();

    if (file_.is_open()) {
        file_.close();
    }
}

} // namespace org::ttldtor::qdcxx::api::detail
