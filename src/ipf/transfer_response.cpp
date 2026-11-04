// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "transfer_response.hpp"
#include "live_transfer.hpp"

#include <boost/beast/http/error.hpp>
#include <boost/url/parse.hpp>

#include <algorithm>
#include <cstring>
#include <limits>

namespace org::ttldtor::qdcxx::ipf::detail {
namespace {

/** Borrow a Beast byte view without retaining it.
 * @param value Borrowed field/body.
 * @return Equivalent standard view.
 */
std::string_view view(boost::beast::string_view value) noexcept {
    return {value.data(), value.size()};
}

/** Preserve a static first failure without replacing an earlier native/parser cause.
 * @param result Stable detached result.
 * @param error New category.
 * @param stage Static checkpoint.
 */
void firstFailure(IpfTransferResult &result, IpfTransferError error, std::string_view stage) {
    if (!result.error) {
        result.error = IpfTransferDiagnostic{error, stage};
    }
}

} // namespace

bool charge(std::size_t &used, std::size_t amount, std::size_t maximum) noexcept {
    if (used > maximum || amount > maximum - used) {
        return false;
    }

    used += amount;

    return true;
}

TransferProfiles::TransferProfiles(IpfReadOptions options, IpfTransferResult &result, TransferWork &work,
                                   const IpfTransferLimits &limits)
    : parser_(options), result_(result), work_(work), limits_(limits), profileLimit_(options.limits.profiles),
      envelope_(limits.envelope, result) {
}

void TransferProfiles::observe(IpfReadStep step) {
    const auto progress = parser_.progress();
    result_.ipf.acceptedBytes = progress.inputBytes;
    result_.ipf.flushes = progress.flushes;
    result_.ipf.completions = progress.completions;
    result_.ipf.completionSeen = progress.completionSeen;

    if (step.item) {
        if (step.item->kind == IpfItemKind::PROFILE) {
            auto &profiles = result_.ipf.profiles;

            if (profiles.size() == profiles.capacity()) {
                constexpr auto MAXIMUM = (std::numeric_limits<std::size_t>::max)();

                if (profiles.size() > MAXIMUM / sizeof(InstrumentProfile) ||
                    progress.profileUnits > MAXIMUM / sizeof(char16_t) ||
                    !charge(work_.presented, profiles.size() * sizeof(InstrumentProfile), limits_.presentedBytes) ||
                    !charge(work_.presented, progress.profileUnits * sizeof(char16_t), limits_.presentedBytes)) {
                    failed_ = true;
                    firstFailure(result_, IpfTransferError::WORK_LIMIT, "ipf-output-growth-work");

                    return;
                }

                // Clamp requested capacity to the unchanged logical profile bound. Non-nothrow move
                // platforms copy the old prefix; both old/candidate text remain within twice its bound.
                const auto increment =
                    (std::min)(profileLimit_ - profiles.size(), (std::max)(std::size_t(1), profiles.size()));
                profiles.reserve(profiles.size() + increment);
            }

            profiles.push_back(std::move(*step.item->profile));
        }
    }

    if (step.status == IpfReadStatus::FAILED) {
        failed_ = true;
        result_.ipf.error = parser_.diagnostic();
        firstFailure(result_, IpfTransferError::IPF_FAILURE, "ipf-parser");
    }

    if (step.status == IpfReadStatus::FINISHED) {
        result_.ipf.finished = true;
        result_.ipf.complete = result_.ipf.completionSeen;
    }
}

bool TransferProfiles::drain(IpfReadWork &work) {
    while ((!envelope_.output().empty() || parserPending_) && !failed_) {
        auto step = parser_.feed(envelope_.output(), work);
        envelope_.consume(step.accepted);
        const auto status = step.status;
        parserPending_ =
            status != IpfReadStatus::NEED_INPUT && status != IpfReadStatus::FAILED && status != IpfReadStatus::FINISHED;
        observe(std::move(step));

        if (status == IpfReadStatus::WORK_EXHAUSTED || status == IpfReadStatus::FAILED) {
            break;
        }
    }

    return (envelope_.output().empty() && !parserPending_) || failed_;
}

std::size_t TransferProfiles::feed(std::span<const std::byte> input, IpfReadWork &work) {
    if (!drain(work) || failed_) {
        return 0;
    }

    auto accepted = envelope_.feed(input);
    parserPending_ = !envelope_.output().empty();
    const auto drained = drain(work);

    // A staged sniff/header transition may consume no new source bytes. At most one extra
    // finite decoder step preserves ordinary plain-input progress without an unbounded loop.
    if (accepted == 0 && !input.empty() && drained && !failed_ && !envelope_.failed()) {
        accepted = envelope_.feed(input);
        parserPending_ = !envelope_.output().empty();
        (void)drain(work);
    }

    return accepted;
}

bool TransferProfiles::finish(IpfReadWork &work) {
    if (!drain(work)) {
        return false;
    }

    if (failed_ || envelope_.failed()) {
        return true;
    }

    if (!envelope_.finish()) {
        parserPending_ = parserPending_ || !envelope_.output().empty();
        (void)drain(work);

        return false;
    }

    if (envelope_.failed()) {
        return true;
    }

    auto step = parser_.finish(work);
    const auto status = step.status;
    observe(std::move(step));

    return status == IpfReadStatus::FINISHED || status == IpfReadStatus::FAILED;
}

template <class Profiles>
BasicTransferResponse<Profiles>::BasicTransferResponse(const IpfTransferLimits &limits, TransferWork &work,
                                                       Profiles &profiles, IpfTransferResult &result)
    : limits_(limits), work_(work), profiles_(profiles), result_(result) {
    header_limit(static_cast<std::uint32_t>(limits.headerBytes));
    body_limit(limits.bodyBytes);
    eager(false);
}

template <class Profiles>
void BasicTransferResponse<Profiles>::fail(IpfTransferError error, std::string_view stage,
                                           boost::system::error_code &ec) {
    firstFailure(result_, error, stage);
    ec = boost::beast::http::error::bad_value;
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_request_impl(boost::beast::http::verb, boost::beast::string_view,
                                                      boost::beast::string_view, int, boost::system::error_code &ec) {
    fail(IpfTransferError::HTTP_FAILURE, "http-response-only", ec);
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_response_impl(int code, boost::beast::string_view, int,
                                                       boost::system::error_code &ec) {
    status = static_cast<unsigned>(code);
    result_.httpStatus = status;
    result_.lastModified.reset();
    result_.live = IpfLiveSupport::ABSENT;

    if (status == 101) {
        fail(IpfTransferError::HTTP_STATUS, "http-upgrade", ec);
    }
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_field_impl(boost::beast::http::field field, boost::beast::string_view name,
                                                    boost::beast::string_view value, boost::system::error_code &ec) {
    if (++fields_ > limits_.headerFields) {
        fail(IpfTransferError::RESOURCE_LIMIT, "http-header-fields", ec);

        return;
    }

    if (boost::beast::iequals(name, "X-Live")) {
        if (liveSeen_) {
            fail(IpfTransferError::HTTP_FAILURE, "http-x-live", ec);

            return;
        }

        liveSeen_ = true;
        result_.live = view(value) == "provided" ? IpfLiveSupport::PROVIDED : IpfLiveSupport::OTHER;
    }

    switch (field) {
    case boost::beast::http::field::location:

        if (locationSeen_ || value.size() > limits_.locationBytes || value.size() > location_.size()) {
            fail(IpfTransferError::RESOURCE_LIMIT, "http-location", ec);

            return;
        }

        {
            const auto location = boost::urls::parse_uri_reference({value.data(), value.size()});

            if (!location || location->has_userinfo()) {
                fail(IpfTransferError::REDIRECT_REJECTED, "http-location-policy", ec);

                return;
            }
        }

        locationSeen_ = true;
        locationSize_ = value.size();

        if (locationSize_ != 0) {
            std::memcpy(location_.data(), value.data(), value.size());
        }

        break;
    case boost::beast::http::field::last_modified:

        if (modifiedSeen_) {
            fail(IpfTransferError::HTTP_FAILURE, "http-last-modified", ec);

            return;
        }

        modifiedSeen_ = true;
        result_.lastModified = parseHttpDate(view(value));

        break;
    case boost::beast::http::field::content_encoding:

        if (codingSeen_) {
            fail(IpfTransferError::HTTP_FAILURE, "http-duplicate-content-encoding", ec);
        } else if (boost::beast::iequals(value, "gzip")) {
            if (status == 200 && !profiles_.declareGzip()) {
                fail(IpfTransferError::HTTP_FAILURE, "http-gzip-state", ec);
            }
        } else if (!boost::beast::iequals(value, "identity")) {
            fail(IpfTransferError::UNSUPPORTED_SOURCE, "http-content-encoding", ec);
        }

        codingSeen_ = true;
        break;
    default:
        break;
    }
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_trailer_field_impl(boost::beast::http::field field,
                                                            boost::beast::string_view name, boost::beast::string_view,
                                                            boost::system::error_code &ec) {
    if (++trailers_ > limits_.trailerFields) {
        fail(IpfTransferError::RESOURCE_LIMIT, "http-trailer-fields", ec);

        return;
    }

    // Routing, framing, authentication and interpretation metadata cannot be supplied by trailers.
    switch (field) {
    case boost::beast::http::field::content_length:
    case boost::beast::http::field::transfer_encoding:
    case boost::beast::http::field::content_encoding:
    case boost::beast::http::field::content_type:
    case boost::beast::http::field::host:
    case boost::beast::http::field::location:
    case boost::beast::http::field::authorization:
    case boost::beast::http::field::proxy_authorization:
    case boost::beast::http::field::www_authenticate:
    case boost::beast::http::field::proxy_authenticate:
    case boost::beast::http::field::last_modified:
    case boost::beast::http::field::connection:
    case boost::beast::http::field::trailer:
    case boost::beast::http::field::upgrade:
        fail(IpfTransferError::HTTP_FAILURE, "http-trailer-metadata", ec);

        return;
    default:

        if (boost::beast::iequals(name, "X-Live")) {
            fail(IpfTransferError::HTTP_FAILURE, "http-trailer-metadata", ec);
        }

        break;
    }
}

template <class Profiles> void BasicTransferResponse<Profiles>::on_header_impl(boost::system::error_code &) {
    // Beast adds the final chunk CRLF to this allowance before parsing the trailer fields.
    header_limit(static_cast<std::uint32_t>(limits_.trailerBytes - 2));
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_body_init_impl(boost::optional<std::uint64_t> const &,
                                                        boost::system::error_code &) {
}

template <class Profiles>
std::size_t BasicTransferResponse<Profiles>::body(boost::beast::string_view value, boost::system::error_code &ec) {
    const auto available = limits_.bodyBytes - (std::min)(limits_.bodyBytes, work_.body);
    const auto count = (std::min)(value.size(), available);

    if (count == 0 && !value.empty()) {
        fail(IpfTransferError::RESOURCE_LIMIT, "http-payload-bytes", ec);

        return 0;
    }

    const auto accepted =
        status == 200 ? profiles_.feed(std::as_bytes(std::span(value.data(), count)), ipfWork) : count;
    work_.body += accepted;

    if (profiles_.failed()) {
        ec = boost::beast::http::error::bad_value;
    }

    return accepted;
}

template <class Profiles>
std::size_t BasicTransferResponse<Profiles>::on_body_impl(boost::beast::string_view value,
                                                          boost::system::error_code &ec) {
    return body(value, ec);
}

template <class Profiles>
void BasicTransferResponse<Profiles>::on_chunk_header_impl(std::uint64_t, boost::beast::string_view extensions,
                                                           boost::system::error_code &ec) {
    if (extensions.size() > limits_.chunkExtensionBytes ||
        !charge(work_.extensions, extensions.size(), limits_.totalChunkExtensionBytes)) {
        fail(IpfTransferError::RESOURCE_LIMIT, "http-chunk-extensions", ec);
    }
}

template <class Profiles>
std::size_t BasicTransferResponse<Profiles>::on_chunk_body_impl(std::uint64_t, boost::beast::string_view value,
                                                                boost::system::error_code &ec) {
    return body(value, ec);
}

template <class Profiles> void BasicTransferResponse<Profiles>::on_finish_impl(boost::system::error_code &) {
}

template <class Profiles> std::string_view BasicTransferResponse<Profiles>::location() const noexcept {
    return {location_.data(), locationSize_};
}

template class BasicTransferResponse<TransferProfiles>;
template class BasicTransferResponse<StreamingProfiles>;
} // namespace org::ttldtor::qdcxx::ipf::detail
