// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "tls_digest_catalog.hpp"
#include "tls_host_preflight.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {
namespace c = org::ttldtor::qdcxx::connect::closed_tls;
namespace t = org::ttldtor::qdcxx::transport;
constexpr std::size_t SHARED_BYTES = 125582389; ///< Closed x64 host requested-payload reservation.
constexpr std::size_t UNIT_COUNT = static_cast<std::size_t>(c::PreflightUnit::COUNT);
constexpr std::array<std::uint64_t, UNIT_COUNT> EXPECTED_WORK{1, 1, 27, 27, 71, 27, 125, 1};
static_assert(std::is_trivially_copyable_v<c::CheckedClosedTlsHost>);
static_assert(!std::is_copy_constructible_v<c::PreflightOnce>);
static_assert(!std::is_move_constructible_v<c::PreflightOnce>);

/** Maximal supported immutable TLS inputs; no certificate or SDK operation is performed. */
t::TlsResourceLimits limits() noexcept {
    return {524288, 256, 65536, 65536, 255};
}

/** Report only static test assertions, never an SDK/environment/user string. */
bool expect(bool condition, const char *message) noexcept {
    if (!condition)
        std::fprintf(stderr, "FAIL %s\n", message);

    return condition;
}

/** Exact cold requirements are computed only through public pure resource operations. */
bool requirementsCase() noexcept {
    const auto actual = c::preflightRequirements(limits());
    const auto publicResult = t::tlsRequirements(limits(), c::closedHostLimits());

    if (!expect(actual.has_value() && publicResult.has_value(), "pure requirements"))
        return false;

    const auto host = c::closedHostLimits();
    const t::TlsHostLimits expectedHost{6297, 2, 16, 6, 22, 28000, 1000, 1, 9, 9, 1, 2, 1, 1, 0, 0, 0, 3, 4, 1904};
    std::size_t aliases = 0;

    for (const auto &declaration : c::DIGEST_CATALOG)
        aliases += declaration.aliases.size();

    return expect(host == expectedHost, "complete twenty-field declaration") &&
           expect(actual->tls.shape == publicResult->shape, "public calculator shape") &&
           expect(actual->tls.sdk.sharedBytes == publicResult->sdk.sharedBytes &&
                      actual->tls.sdk.sharedBytes == SHARED_BYTES,
                  "public calculator shared reservation") &&
           expect(actual->workQualified && actual->preflightRevision != 0, "enabled exact source binding") &&
           expect(actual->work.units == EXPECTED_WORK, "eight complete transitive units") &&
           expect(c::DIGEST_CATALOG.size() == 27 && aliases == 71, "complete compiled catalogue");
}

/** Parse a single finite work-coordinate selector without allocating or accepting trailing data. */
bool coordinate(std::string_view name, std::string_view prefix, std::size_t &index) noexcept {
    if (!name.starts_with(prefix))
        return false;

    const auto suffix = name.substr(prefix.size());
    const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);

    return parsed.ec == std::errc{} && parsed.ptr == suffix.data() + suffix.size() && index < UNIT_COUNT;
}

/** Recognize the maintained fresh-process cases before printing any supplied argument. */
bool knownCase(std::string_view name) noexcept {
    constexpr std::array fixed{"requirements",   "invalid-domain",  "overflow-domain", "capacity-zero",
                               "capacity-short", "shape",           "revision",        "work-all-zero",
                               "success",        "success-surplus", "environment"};

    for (const auto entry : fixed)
        if (name == entry)
            return true;

    std::size_t index = 0;

    return coordinate(name, "work-zero-", index) || coordinate(name, "work-short-", index);
}

/** Exercise one scope per process; no SDK accessor, hook or test adapter precedes preflight. */
bool entryCase(std::string_view name) noexcept {
    const auto required = c::preflightRequirements(limits());

    if (!expect(required.has_value(), "initial pure requirements"))
        return false;

    auto input = limits();
    c::PreflightCapacity capacity{required->tls.sdk.sharedBytes};
    c::PreflightWork work{required->tls.shape, required->preflightRevision, required->work};
    c::PreflightError expected = c::PreflightError::RESOURCE_SHAPE;
    const bool success = name == "success" || name == "success-surplus";
    const bool admitted = success || name == "environment";
    std::size_t index = 0;

    if (name == "invalid-domain") {
        input.trustBytes = 0;
    } else if (name == "overflow-domain") {
        input.anchors = std::numeric_limits<std::uint64_t>::max();
    } else if (name == "capacity-zero" || name == "capacity-short") {
        capacity.sharedBytes = name == "capacity-zero" ? 0 : capacity.sharedBytes - 1;
        expected = c::PreflightError::RESOURCE_LIMIT;
    } else if (name == "shape") {
        ++work.tlsShape.host.namemapGroups;
        expected = c::PreflightError::SHAPE_MISMATCH;
    } else if (name == "revision") {
        ++work.preflightRevision;
        expected = c::PreflightError::SHAPE_MISMATCH;
    } else if (name == "work-all-zero") {
        work.remaining = {};
        expected = c::PreflightError::WORK_LIMIT;
    } else if (coordinate(name, "work-zero-", index)) {
        work.remaining.units[index] = 0;
        expected = c::PreflightError::WORK_LIMIT;
    } else if (coordinate(name, "work-short-", index)) {
        --work.remaining.units[index];
        expected = c::PreflightError::WORK_LIMIT;
    } else if (name == "environment") {
        expected = c::PreflightError::ENVIRONMENT;
    } else if (name == "success-surplus") {
        capacity.sharedBytes += 17;

        for (auto &value : work.remaining.units)
            value += 3;
    }

    const auto beforeCapacity = capacity.sharedBytes;
    const auto beforeWork = work;
    c::PreflightOnce once;
    const auto result = c::checkClosedTlsHost(once, input, capacity, work);

    if (!expect(result.has_value() == success, "expected preflight outcome")) {
        if (!result)
            std::fprintf(stderr, "DETAIL category=%u declaration=%zu alias=%zu code=%llu\n",
                         static_cast<unsigned>(result.error().error), result.error().declaration, result.error().alias,
                         static_cast<unsigned long long>(result.error().sdkCode));

        return false;
    }

    if (success) {
        if (!expect(result->limits == c::closedHostLimits() && result->reservedSharedBytes == SHARED_BYTES,
                    "detached receipt after all actual 27/71 checks"))
            return false;
    } else {
        const auto &diagnostic = result.error();

        if (!expect(diagnostic.error == expected, "typed refusal") ||
            !expect(diagnostic.declaration == 0 && diagnostic.alias == 0 && diagnostic.sdkCode == 0,
                    "pre-entry refusal has no SDK observation"))
            return false;

        if (name == "environment" &&
            !expect(diagnostic.stage == "preflight.cpu-environment", "static redacted environment stage"))
            return false;
    }

    const auto expectedCapacity = admitted ? beforeCapacity - required->tls.sdk.sharedBytes : beforeCapacity;

    if (!expect(capacity.sharedBytes == expectedCapacity, "whole storage debit or atomic refusal"))
        return false;

    for (std::size_t unit = 0; unit < UNIT_COUNT; ++unit) {
        const auto expectedUnits =
            admitted ? beforeWork.remaining.units[unit] - required->work.units[unit] : beforeWork.remaining.units[unit];

        if (!expect(work.remaining.units[unit] == expectedUnits, "whole work debit or atomic refusal"))
            return false;
    }

    if (!expect(work.tlsShape == beforeWork.tlsShape && work.preflightRevision == beforeWork.preflightRevision,
                "admission never rewrites qualification identity"))
        return false;

    // Enlarged allowances cannot bypass consumption after either admission or a pure refusal.
    capacity.sharedBytes = required->tls.sdk.sharedBytes;
    work = {required->tls.shape, required->preflightRevision, required->work};
    const auto repeated = c::checkClosedTlsHost(once, limits(), capacity, work);

    return expect(!repeated && repeated.error().error == c::PreflightError::ALREADY_ENTERED &&
                      repeated.error().stage == "preflight.already-entered",
                  "one-shot refusal even with new allowances") &&
           expect(capacity.sharedBytes == required->tls.sdk.sharedBytes && work.remaining.units == required->work.units,
                  "repeat has no debit or SDK reentry");
}
} // namespace

/** Fresh-process runner: connect_tls_host --case NAME; see --help for cases and exit codes.
 * Each invocation owns its process SDK lifetime. No network, files, secrets, mutation hooks,
 * fault injection or external SDK users are introduced. The launch environment is caller-owned.
 */
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::puts("Usage: connect_tls_host --case NAME\n"
                  "Fresh-process closed TLS host conformance; no default case.\n"
                  "Cases: requirements, invalid-domain, overflow-domain, capacity-zero, capacity-short,\n"
                  "       shape, revision, work-all-zero, work-zero-N, work-short-N (N=0..7),\n"
                  "       success, success-surplus, environment.\n"
                  "Success cases enter the exact qualified static SDK and check all 27 methods/71 aliases.\n"
                  "environment requires a present OPENSSL_ia32cap in this child's launch environment.\n"
                  "Other cases require the variable absent; no case changes the environment.\n"
                  "Run each case in a new process, with one SDK image/user/thread and no foreign hooks.\n"
                  "Output: RUN and PASS, or bounded static/numeric failure evidence. No input echo.\n"
                  "Exit: 0 passed/help, 1 assertion failed, 2 invalid invocation.\n"
                  "Side effects: admitted cases initialize process-local SDK state; no network/files.\n"
                  "Help and pure refusals perform no application-controlled SDK entry.");

        return 0;
    }

    if (argc != 3 || std::string_view(argv[1]) != "--case" || !knownCase(argv[2])) {
        std::fputs("Invalid invocation; use --help.\n", stderr);

        return 2;
    }

    const std::string_view name(argv[2]);
    std::printf("RUN %.*s\n", static_cast<int>(name.size()), name.data());
    const bool passed = name == "requirements" ? requirementsCase() : entryCase(name);

    if (passed)
        std::printf("PASS %.*s\n", static_cast<int>(name.size()), name.data());

    return passed ? 0 : 1;
}
