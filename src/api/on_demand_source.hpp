// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../protocol/local_event_owner.hpp"
#include "on_demand_playback_clock.hpp"
#include "on_demand_wire.hpp"
#include "qd/client/endpoint_options.hpp"
#include "qd/transport/tcp_runtime.hpp"

namespace org::ttldtor::qdcxx::transport::detail {
class RawByteOwner;
} // namespace org::ttldtor::qdcxx::transport::detail

namespace org::ttldtor::qdcxx::api::detail {

/** One finite worker-owned POST, replay cursor and local event owner. */
class OnDemandSource {
    using Clock = std::chrono::steady_clock;

    OnDemandRequest request_;
    core::RecordContext context_;
    protocol::detail::LocalEventOwner owner_;
    std::unique_ptr<transport::detail::RawByteOwner> network_;
    Clock::time_point deadline_;
    std::optional<OnDemandResponse> response_;
    OnDemandPlaybackClock playbackClock_;
    std::size_t next_ = 0;
    bool closing_ = false;

    [[nodiscard]] std::int64_t currentTime() const noexcept;

    [[nodiscard]] std::expected<void, Diagnostic> pollNetwork(transport::TcpWork &nativeWork, protocol::LiveWork &work,
                                                              Clock::time_point now);

    public:
    OnDemandSource(const OnDemandRequest &request, const core::LocalSchema &schema, const EndpointOptions &options,
                   const client::EndpointSettings &settings, const transport::detail::BootstrapLease &bootstrap,
                   protocol::LiveWork &work);
    ~OnDemandSource();

    /** Advance the one HTTP exchange or retire its already completed socket independently of delivery. */
    [[nodiscard]] std::expected<void, Diagnostic> progressNetwork(transport::TcpWork &nativeWork,
                                                                  protocol::LiveWork &work, Clock::time_point now);
    /** Start the playback clock when the endpoint's delivery path is ready. */
    void beginPlayback(Clock::time_point now) noexcept;
    /** Settle the old rate before applying a validated new rate and report one coherent observation. */
    [[nodiscard]] OnDemandPlayback playback(double speed, Clock::time_point now) noexcept;
    [[nodiscard]] std::expected<std::optional<protocol::LiveEvent>, Diagnostic> next(protocol::LiveWork &work);

    /** Continue native retirement with the same finite worker credits. */
    void close(transport::TcpWork &nativeWork, protocol::LiveWork &work) noexcept;

    [[nodiscard]] bool covered() const noexcept;
    [[nodiscard]] bool finished() const noexcept;
    [[nodiscard]] bool retired() const noexcept;
    [[nodiscard]] Clock::time_point nextWake(Clock::time_point now) const noexcept;
};

} // namespace org::ttldtor::qdcxx::api::detail
