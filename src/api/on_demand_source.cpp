// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#include "on_demand_source.hpp"

#include "../events/decimal_projection.hpp"
#include "../transport/raw_byte_owner.hpp"

#include <algorithm>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
// Native polling has no background socket notification to wake the Feed owner.
constexpr auto MAXIMUM_REPLAY_POLL_INTERVAL = std::chrono::milliseconds{10};
constexpr Diagnostic NATIVE_FAILURE{ErrorCode::TRANSPORT_FAILURE, "ondemand-transport"};
constexpr Diagnostic RESPONSE_TIMEOUT{ErrorCode::TRANSPORT_FAILURE, "ondemand-response-deadline"};
constexpr Diagnostic TRUNCATED_RESPONSE{ErrorCode::PROTOCOL_FAILURE, "ondemand-response-truncated"};

Diagnostic nativeError(transport::TcpDiagnostic failure) noexcept {
    switch (failure.error) {
    case transport::TcpError::WORK_LIMIT:
        return {ErrorCode::WORK_LIMIT, "ondemand-transport-work"};
    case transport::TcpError::ALLOCATION_FAILURE:
        return {ErrorCode::ALLOCATION_FAILURE, "ondemand-transport-storage"};
    case transport::TcpError::RESOURCE_LIMIT:
        return {ErrorCode::RESOURCE_LIMIT, "ondemand-transport-limit"};
    case transport::TcpError::DEADLINE:
        return RESPONSE_TIMEOUT;
    default:
        return NATIVE_FAILURE;
    }
}

Diagnostic eventError(protocol::LiveError error) noexcept {
    switch (error) {
    case protocol::LiveError::WORK_LIMIT:
        return {ErrorCode::WORK_LIMIT, "ondemand-event-work"};
    case protocol::LiveError::ALLOCATION_FAILURE:
        return {ErrorCode::ALLOCATION_FAILURE, "ondemand-event-storage"};
    default:
        return {ErrorCode::RESOURCE_LIMIT, "ondemand-event-limit"};
    }
}

protocol::detail::LocalEventOwner eventOwner(protocol::LiveLimits limits, protocol::LiveWork &work) {
    auto owner = protocol::detail::LocalEventOwner::create(limits, work);

    if (!owner)
        throw Error(eventError(owner.error()));

    return std::move(*owner);
}

std::chrono::milliseconds operationTimeout(std::chrono::milliseconds requested) noexcept {
    return std::min(requested, std::chrono::duration_cast<std::chrono::milliseconds>(ON_DEMAND_NETWORK_DEADLINE));
}

} // namespace

OnDemandSource::OnDemandSource(const OnDemandRequest &request, const core::LocalSchema &schema,
                               const EndpointOptions &options, const client::EndpointSettings &settings,
                               const transport::detail::BootstrapLease &bootstrap, protocol::LiveWork &work)
    : request_(request), context_(schema.recordContext(0)), owner_(eventOwner(settings.budgets.session, work)) {
    const auto &limits = settings.budgets.tcp;

    if (limits.residentBytes <= sizeof(ResponseBytes))
        throw Error({ErrorCode::RESOURCE_LIMIT, "ondemand-response-reservation"});

    if (work.bytes < sizeof(ResponseBytes))
        throw Error({ErrorCode::WORK_LIMIT, "ondemand-response-preparation"});

    work.bytes -= sizeof(ResponseBytes);
    received_ = std::make_unique_for_overwrite<ResponseBytes>();
    const transport::TcpOptions tcp{"127.0.0.1",
                                    request.port,
                                    {},
                                    53,
                                    operationTimeout(options.dnsTimeout),
                                    operationTimeout(options.connectTimeout),
                                    operationTimeout(options.readTimeout),
                                    operationTimeout(options.writeTimeout),
                                    std::min(options.pollInterval, MAXIMUM_REPLAY_POLL_INTERVAL),
                                    {limits.inputBytes, limits.writeBytes, limits.addresses, limits.cnameDepth,
                                     limits.contextBytes, limits.handlerBytes,
                                     limits.residentBytes - sizeof(ResponseBytes)}};
    auto made = transport::detail::RawByteOwner::create(bootstrap, tcp,
                                                        {MAXIMUM_RESPONSE_BYTES, ON_DEMAND_MAXIMUM_REQUEST}, work);

    if (!made)
        throw Error(nativeError(made.error()));

    network_ = std::move(*made);
    const auto requestBytes = encodeOnDemandRequest(request);

    if (!requestBytes)
        throw Error(requestBytes.error());

    const auto written = network_->write(std::span(requestBytes->bytes).first(requestBytes->size), work);

    if (!written)
        throw Error(nativeError(written.error()));

    deadline_ = Clock::now() + ON_DEMAND_NETWORK_DEADLINE;
    const auto started = network_->start();

    if (!started)
        throw Error(nativeError(started.error()));
}

OnDemandSource::~OnDemandSource() = default;

std::expected<void, Diagnostic> OnDemandSource::pollNetwork(transport::TcpWork &nativeWork, protocol::LiveWork &work,
                                                            Clock::time_point now) {
    if (now >= deadline_)
        return std::unexpected(RESPONSE_TIMEOUT);

    const auto progress = network_->poll(nativeWork, work);

    if (!progress)
        return std::unexpected(nativeError(progress.error()));

    const auto bytes = network_->input();

    if (!bytes)
        return std::unexpected(nativeError(bytes.error()));

    if (!bytes->empty()) {
        if (bytes->size() > received_->size() - receivedSize_)
            return std::unexpected(Diagnostic{ErrorCode::PROTOCOL_FAILURE, "ondemand-http-framing"});

        const auto total = receivedSize_ + bytes->size();
        // Charge the append, a bounded full framing scan, and the possible final body decode.
        const auto need = bytes->size() + total * 3;

        if (need > work.bytes || work.operations == 0)
            return std::unexpected(Diagnostic{ErrorCode::WORK_LIMIT, "ondemand-response-work"});

        work.bytes -= need;
        --work.operations;
        std::ranges::copy(*bytes, received_->begin() + receivedSize_);
        receivedSize_ = total;
        // Raw TCP cannot rearm while an old application prefix remains retained.
        const auto consumed = network_->consumeInput(bytes->size());

        if (!consumed)
            return std::unexpected(nativeError(consumed.error()));

        const auto framed = readOnDemandHttp(std::span(*received_).first(receivedSize_));

        if (!framed)
            return std::unexpected(framed.error());

        if (*framed) {
            auto decoded = decodeOnDemandResponse(**framed, request_);

            if (!decoded)
                return std::unexpected(decoded.error());

            response_.emplace(std::move(*decoded));
            received_.reset();
            static_cast<void>(network_->requestClose());

            return {};
        }
    }

    if (progress->status == transport::TcpPollStatus::CLOSED) {
        const auto terminal = network_->core().terminal();

        if (terminal && terminal->primary.error != transport::TcpError::NONE)
            return std::unexpected(nativeError(terminal->primary));

        return std::unexpected(TRUNCATED_RESPONSE);
    }

    return {};
}

std::expected<void, Diagnostic> OnDemandSource::progressNetwork(transport::TcpWork &nativeWork,
                                                                protocol::LiveWork &work, Clock::time_point now) {
    if (!response_)
        return pollNetwork(nativeWork, work, now);

    if (!retired())
        static_cast<void>(network_->poll(nativeWork, work));

    return {};
}

void OnDemandSource::beginPlayback(Clock::time_point now) noexcept {
    playbackClock_.start(now);
}

std::expected<std::optional<protocol::LiveEvent>, Diagnostic> OnDemandSource::next(protocol::LiveWork &work) {
    if (closing_ || !playbackClock_.started() || finished() || response_->records[next_].time > currentTime())
        return std::nullopt;

    using Type = core::SerialFieldType;
    const auto &record = response_->records[next_];
    const auto &field = record.fields;
    const std::array<core::FieldValue, 8> fields{
        {{Type::TIME_SECONDS, field[0]},
         {Type::UTF_CHAR, field[1]},
         {Type::WIDE_DECIMAL, events::detail::projectPresentDecimal(Type::DECIMAL, field[2]).raw},
         {Type::WIDE_DECIMAL, events::detail::projectPresentDecimal(Type::COMPACT_INT, field[3]).raw},
         {Type::TIME_SECONDS, field[4]},
         {Type::UTF_CHAR, field[5]},
         {Type::WIDE_DECIMAL, events::detail::projectPresentDecimal(Type::DECIMAL, field[6]).raw},
         {Type::WIDE_DECIMAL, events::detail::projectPresentDecimal(Type::COMPACT_INT, field[7]).raw}}};
    const protocol::FieldEventTime time{record.time / 1000, (record.time % 1000) << 22, next_};
    auto event = owner_.quote(context_, request_.symbol, fields, time, work);

    if (!event)
        return std::unexpected(eventError(event.error()));

    ++next_;

    return std::optional{std::move(*event)};
}

void OnDemandSource::close(transport::TcpWork &nativeWork, protocol::LiveWork &work) noexcept {
    closing_ = true;

    if (retired())
        return;

    static_cast<void>(network_->requestClose());
    static_cast<void>(network_->poll(nativeWork, work));
}

bool OnDemandSource::covered() const noexcept {
    return response_.has_value();
}

bool OnDemandSource::finished() const noexcept {
    return response_ && next_ == response_->size;
}

bool OnDemandSource::retired() const noexcept {
    return network_->core().state() == transport::TcpState::CLOSED;
}

std::int64_t OnDemandSource::currentTime() const noexcept {
    return request_.from.time_since_epoch().count() + playbackClock_.position();
}

OnDemandPlayback OnDemandSource::playback(double speed, Clock::time_point now) noexcept {
    const auto maximumOffset =
        response_ ? response_->records[response_->size - 1].time - request_.from.time_since_epoch().count() : 0;
    playbackClock_.advance(speed, now, std::max(std::int64_t{0}, maximumOffset));

    return {playbackClock_.speed(), std::chrono::sys_time{std::chrono::milliseconds{currentTime()}}, true,
            playbackClock_.started()};
}

OnDemandSource::Clock::time_point OnDemandSource::nextWake(Clock::time_point now) const noexcept {
    auto wake = now + MAXIMUM_REPLAY_POLL_INTERVAL;

    if (const auto native = network_->core().nextWake())
        wake = std::min(wake, *native);

    if (!closing_ && !response_)
        wake = std::min(wake, deadline_);

    if (!closing_ && playbackClock_.started() && !finished()) {
        const auto offset = response_->records[next_].time - request_.from.time_since_epoch().count();

        return playbackClock_.wakeBefore(offset, now, wake);
    }

    return wake;
}

} // namespace org::ttldtor::qdcxx::api::detail
