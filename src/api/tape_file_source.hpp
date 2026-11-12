// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../protocol/tape_event_bridge.hpp"
#include "qd/api/feed.hpp"

#include <array>
#include <fstream>

namespace org::ttldtor::qdcxx::api::detail {

/** Worker-owned immediate reader for one binary local file and one local collector epoch. */
class TapeFileSource {
    static constexpr std::size_t READ_BYTES = 4096;
    std::ifstream file_;
    protocol::TapeReader reader_;
    protocol::detail::TapeEventBridge bridge_;
    std::array<std::byte, READ_BYTES> bytes_{};
    std::optional<protocol::TapeFeedResult> pending_;
    std::size_t first_ = 0;
    std::size_t last_ = 0;
    std::size_t readBytes_ = 0;
    std::size_t maximumBytes_;
    std::int32_t familyMessage_;
    bool finished_ = false;

    /** Fill one bounded chunk when the previous buffer is exhausted.
     * @param work Mutable turn credits.
     * @return Success, including physical EOF, or exact bounded refusal.
     */
    std::expected<void, Diagnostic> read(protocol::LiveWork &work);

    public:
    /** Open one stable regular file, with exact schema provenance and no socket or peer state.
     * @param path Source path, borrowed during this worker-side open.
     * @param options Finite file allowances.
     * @param schema Validated immutable receiving schema.
     * @param limits Shared per-frame and event backing policy.
     * @param family Configured local delivery family; the same family is used for RAW data.
     * @param work Remaining epoch construction credits.
     */
    TapeFileSource(const std::filesystem::path &path, TapeSourceOptions options, const core::LocalSchema &schema,
                   protocol::LiveLimits limits, core::SubscriptionFamily family, protocol::LiveWork &work);

    /** Progress one bounded file read and at most one complete envelope.
     * @param work Remaining source credits for the worker turn.
     * @return At most one counted local event, or an explicit parse, work or I/O failure.
     */
    [[nodiscard]] std::expected<std::optional<protocol::LiveEvent>, Diagnostic> next(protocol::LiveWork &work);

    /** @return True after actual clean parser EOF. */
    [[nodiscard]] bool finished() const noexcept;

    /** Retire the file and unadmitted complete reader result without performing more reads. */
    void stop() noexcept;
};

} // namespace org::ttldtor::qdcxx::api::detail
