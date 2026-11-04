// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed_filter.hpp"

namespace org::ttldtor::qdcxx::api {
struct FeedFilterIpfSource::Port {};

struct FeedFilterIpfRuntime::Impl {};

FeedFilterIpfSource::FeedFilterIpfSource(std::shared_ptr<Port> port) noexcept : port_(std::move(port)) {
}

FeedFilterIpfSource::~FeedFilterIpfSource() = default;

void FeedFilterIpfSource::close() noexcept {
}

FeedFilterIpfObservation FeedFilterIpfSource::observation() const {
    return {false, true, true, 0, Diagnostic{ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-backend-disabled"}};
}

bool FeedFilterIpfSource::awaitInitialSnapshot(std::chrono::steady_clock::time_point) const {
    throw Error({ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-backend-disabled"});
}

FeedFilterIpfRuntime::FeedFilterIpfRuntime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {
}

FeedFilterIpfRuntime::~FeedFilterIpfRuntime() = default;

std::unique_ptr<FeedFilterIpfRuntime> FeedFilterIpfRuntime::create(transport::RuntimeBootstrap &,
                                                                   const ipf::IpfTransferOptions &, FeedFilterIpfLimits,
                                                                   FeedFilterIpfSchedule, FeedFilterContextLimits) {
    throw Error({ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-backend-disabled"});
}

std::shared_ptr<FeedFilterContext> FeedFilterIpfRuntime::context() const {
    throw Error({ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-backend-disabled"});
}

std::shared_ptr<FeedFilterIpfSource> FeedFilterIpfRuntime::start(std::shared_ptr<FeedFilterContext>, std::u16string,
                                                                 const ipf::IpfUrlRequest &) {
    throw Error({ErrorCode::UNSUPPORTED_PROFILE, "filter-ipf-backend-disabled"});
}

void FeedFilterIpfRuntime::closeAndAwaitTermination() {
}
} // namespace org::ttldtor::qdcxx::api
