// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_connection.hpp"

#include "../detail/java_double.hpp"
#include "collector_connection.hpp"
#include "connection_control.hpp"
#include "connection_pending.hpp"
#include "connection_sink.hpp"
#include "connection_wait.hpp"
#include "profile_access.hpp"
#include "transfer_source.hpp"
#include <cmath>

#include <array>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>

namespace org::ttldtor::qdcxx::ipf {
namespace connection_detail {
namespace {
constexpr std::size_t OWNER_CONTROL_BYTES = 512;    ///< Conservative owner/port/control/stop ledger block allowance.
constexpr std::size_t OBSERVER_CONTROL_BYTES = 128; ///< Requested node/control allowance beyond callback user captures.

/** Parse the QD TimePeriod source suffix; even inf is a finite numeric delay under lifetime bounds.
 * @param text Borrowed source suffix, already within the source-byte admission.
 * @return Nonnegative representable cooperative delay, or invalid/out-of-profile text.
 */
std::optional<std::chrono::milliseconds> parsePeriod(std::string_view text) {
    using Count = std::chrono::milliseconds::rep;
    const auto maximum = std::numeric_limits<Count>::max();
    // JDK8 Character.isDigit(char)/digit(char,10), captured by the bounded oracle.
    constexpr std::array<unsigned, 35> digitStarts{
        0x30,   0x660,  0x6f0,  0x7c0,  0x966,  0x9e6,  0xa66,  0xae6,  0xb66,  0xbe6,  0xc66,  0xce6,
        0xd66,  0xe50,  0xed0,  0xf20,  0x1040, 0x1090, 0x17e0, 0x1810, 0x1946, 0x19d0, 0x1a80, 0x1a90,
        0x1b50, 0x1bb0, 0x1c40, 0x1c50, 0xa620, 0xa8d0, 0xa900, 0xa9d0, 0xaa50, 0xabf0, 0xff10};
    const auto digit = [&](std::size_t offset) -> std::optional<std::pair<unsigned, std::size_t>> {
        if (offset == text.size()) {
            return {};
        }

        unsigned code = static_cast<unsigned char>(text[offset++]);
        unsigned count = 0;
        unsigned minimum = 0;

        if (code >= 0xc2 && code <= 0xdf) {
            code &= 0x1f;
            count = 1;
            minimum = 0x80;
        } else if (code >= 0xe0 && code <= 0xef) {
            code &= 0x0f;
            count = 2;
            minimum = 0x800;
        } else if (code >= 0x80) {
            return {};
        }

        if (count > text.size() - offset) {
            return {};
        }

        for (unsigned index = 0; index < count; ++index) {
            const auto next = static_cast<unsigned char>(text[offset++]);

            if ((next & 0xc0) != 0x80) {
                return {};
            }

            code = (code << 6) | (next & 0x3f);
        }

        if (code < minimum || (code >= 0xd800 && code <= 0xdfff)) {
            return {};
        }

        for (const auto start : digitStarts) {
            if (code >= start && code - start < 10) {
                return std::pair{code - start, offset};
            }
        }

        return {};
    };
    const auto asciiEqual = [](char actual, char lower) {
        return actual == lower || actual == static_cast<char>(lower - 'a' + 'A');
    };

    if ((text.size() == 3 && asciiEqual(text[0], 'i') && asciiEqual(text[1], 'n') && asciiEqual(text[2], 'f')) ||
        (text.size() == 4 && (text.substr(0, 2) == "\xc4\xb0" || text.substr(0, 2) == "\xc4\xb1") &&
         asciiEqual(text[2], 'n') && asciiEqual(text[3], 'f'))) {
        return std::chrono::milliseconds(maximum);
    }

    std::size_t position = 0;
    Count result = 0;
    bool any = false;
    const auto letter = [&](char value) {
        if (position == text.size()) {
            return false;
        }

        const auto current = text[position];

        return current == value || current == static_cast<char>(value + 'a' - 'A');
    };
    const auto digits = [&] {
        const auto start = position;

        while (const auto next = digit(position)) {
            position = next->second;
        }

        return start;
    };
    const auto integer = [&](std::size_t start, Count scale) {
        if (start == position) {
            return false;
        }

        std::uint64_t value = 0;

        for (auto offset = start; offset < position;) {
            const auto next = digit(offset);

            if (!next ||
                value > (static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) - next->first) / 10) {
                return false;
            }

            value = value * 10 + next->first;
            offset = next->second;
        }

        if (value > static_cast<std::uint64_t>((maximum - result) / scale)) {
            return false;
        }

        result += static_cast<Count>(value) * scale;
        any = true;

        return true;
    };

    if (letter('P')) {
        ++position;
    }

    auto start = digits();

    if (letter('D')) {
        if (!integer(start, 86400000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('T')) {
        if (start != position) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('H')) {
        if (!integer(start, 3600000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (letter('M')) {
        if (!integer(start, 60000)) {
            return {};
        }

        ++position;
        start = digits();
    }

    if (position < text.size() && text[position] == '.') {
        ++position;
        (void)digits();
    }

    if (start != position) {
        std::u16string seconds;
        seconds.reserve(position - start);

        for (auto i = start; i < position; ++i) {
            if (static_cast<unsigned char>(text[i]) >= 0x80) {
                return {};
            }

            seconds.push_back(static_cast<char16_t>(text[i]));
        }

        const auto parsed = org::ttldtor::qdcxx::detail::parseJavaDouble(seconds);

        if (!parsed) {
            return {};
        }

        const auto scaled = *parsed * 1000;
        const auto whole = std::floor(scaled);
        auto millis = org::ttldtor::qdcxx::detail::javaLong(whole);

        if (millis != maximum && scaled - whole >= 0.5) {
            ++millis;
        }

        if (millis < 0 || millis > maximum - result) {
            return {};
        }

        result += millis;
        any = true;

        if (letter('S')) {
            ++position;
        } else if (text.substr(position, 2) == "\xc5\xbf") {
            position += 2;
        }
    }

    if (!any || position != text.size()) {
        return {};
    }

    return std::chrono::milliseconds(result);
}

/** Construct a detached static failure.
 * @param error Existing transfer category.
 * @param stage Library-owned checkpoint.
 * @return Independent diagnostic without input text.
 */
IpfConnectionDiagnostic diagnostic(IpfTransferError error, std::string_view stage) {
    return {{error, stage}, {}};
}
} // namespace

/** Worker-only pending batch/generation adapter, including witnessed failed-attempt carryover. */
struct Sink final : StreamConsumer {
    std::shared_ptr<Port> port;
    std::vector<InstrumentProfile> pending;
    std::vector<std::uint64_t> old;
    std::size_t units = 0;
    std::size_t pendingBackingUsed = 0;
    std::size_t workUnits = 0;
    std::uint64_t generation = 0;
    std::uint64_t nextGeneration = 0;
    std::optional<std::chrono::sys_time<std::chrono::milliseconds>> candidate;
    bool live = false;
    bool skipped = false;
    bool refused = false;
    std::optional<IpfTransferDiagnostic> rejected;
    Clock::time_point deadline;

    /** Prepare finite pending/tag vectors before source activation.
     * @param value Independently retained stable connection port.
     * @param cutoff Original connection lifetime deadline.
     */
    explicit Sink(std::shared_ptr<Port> value, Clock::time_point cutoff) : port(std::move(value)), deadline(cutoff) {
        workUnits = port->observed.workUnits;
        pending.reserve(port->limits.batchProfiles);
        old.reserve(port->limits.oldGenerations);
    }

    /** Debit irreversible connection work before pending-copy or collector admission.
     * @param amount Conservative UTF-16/call/tag visits.
     * @return True after whole admission, false without growth or publication.
     */
    bool charge(std::size_t amount) {
        if (amount > port->limits.workUnits || workUnits > port->limits.workUnits - amount) {
            rejected = diagnostic(IpfTransferError::WORK_LIMIT, "connection-copy-control-work").transfer;
            port->fail({*rejected, {}});
            refused = true;

            return false;
        }

        workUnits += amount;
        std::lock_guard lock(port->mutex);
        port->observed.workUnits = workUnits;

        return true;
    }

    /** Keep all pending values across errors; only attempt-local evidence is renewed. */
    bool begin() {
        generation = 0;
        candidate.reset();
        skipped = false;
        refused = false;
        rejected.reset();
        std::lock_guard lock(port->mutex);

        if (port->closed.load()) {
            return false;
        }

        port->observed.activeAttempt = true;
        port->observed.unchangedDate = false;
        port->observed.diagnostic.reset();
        ++port->observed.attempts;

        return true;
    }

    /** Admit a whole collector span without mutating it through application callbacks.
     * @return True after atomic publication; current source profile is not accepted on false.
     */
    bool flush() {
        if (port->closed.load()) {
            return false;
        }

        if (pending.empty()) {
            return true;
        }

        if (generation == 0) {
            if (old.size() >= port->limits.oldGenerations ||
                nextGeneration == std::numeric_limits<std::uint64_t>::max()) {
                rejected = diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-generations").transfer;
                port->fail({*rejected, {}});
                refused = true;

                return false;
            }

            generation = ++nextGeneration;
        }

        if (!charge(1)) {
            return false;
        }

        auto result =
            collector_detail::ConnectionCollectorAccess::update(*port->collector, pending, port->domain, generation);

        if (!result) {
            auto error = diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-collector-update");
            error.collector = result.error();
            rejected = error.transfer;
            port->fail(error);
            refused = true;

            return false;
        }

        pending.clear();
        units = 0;
        pendingBackingUsed = 0;

        return !port->closed.load();
    }

    /** Complete application replacement independently of HTTP/TLS/container end.
     * @return True after ordered flush and old-generation removal.
     */
    bool complete() {
        if (!flush()) {
            return false;
        }

        if (old.size() == std::numeric_limits<std::size_t>::max() || !charge(old.size() + 1)) {
            return false;
        }

        auto removed = collector_detail::ConnectionCollectorAccess::remove(*port->collector, old, port->domain);

        if (!removed) {
            auto error = diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-collector-remove");
            error.collector = removed.error();
            rejected = error.transfer;
            port->fail(error);
            refused = true;

            return false;
        }

        old.clear();
        port->completeInitialSnapshot();
        port->dispatch();

        return !port->closed.load();
    }

    HeaderAction headers(const IpfTransferResult &result,
                         std::optional<std::chrono::sys_time<std::chrono::milliseconds>> modified) override {
        {
            std::lock_guard lock(port->mutex);

            if (port->closed.load()) {
                return HeaderAction::REJECT;
            }

            candidate = modified;

            if (candidate && candidate->time_since_epoch().count() != 0 && port->observed.lastModified == candidate) {
                skipped = true;
                port->observed.unchangedDate = true;

                return HeaderAction::SKIP;
            }

            port->observed.capability = result.live;
            live = result.live == IpfLiveSupport::PROVIDED;

            if (port->observed.state == IpfConnectionState::CONNECTING) {
                port->transition(IpfConnectionState::CONNECTED);
            }
        }
        port->dispatch();

        return port->closed.load() ? HeaderAction::REJECT : HeaderAction::ACCEPT;
    }

    bool profile(const InstrumentProfile &profile) override {
        if (port->closed.load()) {
            return false;
        }

        const auto amount = collector_detail::CollectorProfileAccess::units(profile);

        if (amount > port->limits.pendingUnits || units > port->limits.pendingUnits - amount) {
            rejected = diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-pending-units").transfer;
            port->fail({*rejected, {}});
            refused = true;

            return false;
        }

        if (amount == std::numeric_limits<std::size_t>::max() || !charge(amount + 1)) {
            return false;
        }

        if (!appendPendingProfile(pending, profile, pendingBackingUsed, port->pendingBackingLimit)) {
            rejected = diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-pending-backing").transfer;
            port->fail({*rejected, {}});
            refused = true;

            return false;
        }

        units += amount;

        return pending.size() < port->limits.batchProfiles || flush();
    }

    bool control(IpfItemKind kind) override {
        return kind == IpfItemKind::COMPLETE ? complete() : flush();
    }

    bool checkpoint() override {
        port->dispatch();

        if (port->closed.load()) {
            return false;
        }

        if (Clock::now() >= deadline) {
            rejected = diagnostic(IpfTransferError::WORK_LIMIT, "connection-lifetime").transfer;
            port->fail({*rejected, {}});
            refused = true;
        }

        return !refused;
    }

    std::optional<IpfTransferDiagnostic> refusal() const noexcept override {
        return rejected;
    }

    /** Finalize normal EOF only; preserve unflushed prefix on errors and move current tag to old.
     * @param result Retired attempt evidence.
     */
    void end(const IpfTransferResult &result) {
        if (!result.error && result.status != IpfTransferStatus::NOT_MODIFIED && !port->closed.load()) {
            if (flush() && !live) {
                (void)complete();
            }

            if (!refused && !port->closed.load()) {
                std::lock_guard lock(port->mutex);

                if (!port->closed.load()) {
                    port->observed.lastModified = candidate;
                }
            }
        }

        if (generation != 0) {
            old.push_back(generation); // Space was reserved before the first publication.
        }

        if (result.error) {
            std::lock_guard lock(port->mutex);
            const auto collector = port->observed.diagnostic ? port->observed.diagnostic->collector
                                                             : std::optional<ProfileCollectorError>{};
            port->observed.diagnostic = IpfConnectionDiagnostic{*result.error, collector};
        }

        std::lock_guard lock(port->mutex);
        port->observed.lastAttempt =
            IpfConnectionAttemptObservation{port->observed.attempts,   result.status,        result.httpStatus,
                                            result.httpFinished,       result.nativeRetired, result.ipf.finished,
                                            result.ipf.completionSeen, result.envelope,      result.ipf.error};
        port->observed.activeAttempt = false;
    }
};

/** Main-thread join/SDK owner. Public producer calls retain only Control/Port leases. */
struct Runtime {
    std::optional<transport::RuntimeBootstrap> owned;
    transport::detail::BootstrapLease bootstrap;
    const std::thread::id owner = std::this_thread::get_id();
    IpfConnectionRuntimeOptions options;
    std::size_t pendingBackingLimit;
    std::shared_ptr<Control> control = std::make_shared<Control>();
    std::shared_ptr<Port> port;
    std::thread worker;
    bool joining = false;

    /** Bind creator-affine SDK lifetime before a worker exists.
     * @param lease Admitted stable explicit bootstrap identity.
     * @param value Already validated finite profile, copied before publication.
     * @param admittedBytes Existing whole-owner admission including logical pending-unit reservation.
     */
    Runtime(transport::detail::BootstrapLease lease, const IpfConnectionRuntimeOptions &value,
            std::size_t admittedBytes)
        : bootstrap(std::move(lease)), options(value),
          pendingBackingLimit(value.limits.residentBytes -
                              (admittedBytes - value.limits.pendingUnits * sizeof(char16_t))) {
        control->runtime = this;
    }

    /** Drain/join on creator before member leases and owned bootstrap retire. */
    ~Runtime() {
        if (owner != std::this_thread::get_id()) {
            std::terminate();
        }

        (void)shutdown();
    }

    /** Check all own backing/shape before settings, callbacks or native preparation.
     * @param value Explicit one-slot profile.
     * @return Required admitted owner bytes, including the existing logical-unit reserve, or detached failure.
     */
    static std::expected<std::size_t, IpfConnectionDiagnostic> validate(const IpfConnectionRuntimeOptions &value) {
        const auto &limits = value.limits;

        if (value.maximumConnections != 1 || limits.batchProfiles == 0 || limits.lifetime.count() <= 0 ||
            limits.lifetime > std::chrono::duration_cast<std::chrono::milliseconds>(Clock::duration::max() / 4)) {
            return std::unexpected(diagnostic(IpfTransferError::INVALID_OPTIONS, "connection-profile"));
        }

        std::size_t bytes = sizeof(Runtime) + sizeof(Port) + sizeof(Control) + sizeof(Sink) + sizeof(ObserverLedger) +
                            OWNER_CONTROL_BYTES;
        const auto add = [&](std::size_t count, std::size_t size) {
            if (count > std::numeric_limits<std::size_t>::max() / size) {
                return false;
            }

            const auto amount = count * size;

            if (bytes > limits.residentBytes || amount > limits.residentBytes - bytes) {
                return false;
            }

            bytes += amount;

            return true;
        };

        if (!add(limits.completionWaiters, sizeof(CompletionWaitFrame)) ||
            !add(limits.batchProfiles, sizeof(InstrumentProfile)) || !add(limits.pendingUnits, sizeof(char16_t)) ||
            !add(limits.oldGenerations, sizeof(std::uint64_t)) ||
            !add(limits.listenerSlots,
                 sizeof(Observer) + 2 * sizeof(std::shared_ptr<Observer>) + OBSERVER_CONTROL_BYTES) ||
            !add(value.transfer.trustPem.size(), sizeof(char)) || !add(1, sizeof(char)) ||
            !add(value.transfer.limits.sourceBytes, 4 * sizeof(char)) ||
            !add(value.transfer.limits.authorizationBytes, 2) || bytes > limits.residentBytes) {
            return std::unexpected(diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-owner-backing"));
        }

        const auto transfer = TransferHost::validate(value.transfer);

        if (!transfer) {
            return std::unexpected(IpfConnectionDiagnostic{transfer.error(), {}});
        }

        return bytes;
    }

    /** Start only while the producer Control gate is held; no callbacks inline.
     * @return Acceptance or thread/capacity refusal before state publication.
     */
    std::expected<void, IpfConnectionDiagnostic> start(const std::shared_ptr<Port> &value) {
        std::lock_guard lock(value->mutex);

        if (control->stopping || value->observed.state != IpfConnectionState::NOT_CONNECTED) {
            return std::unexpected(diagnostic(IpfTransferError::INVALID_STATE, "connection-start-state"));
        }

        value->startedAt = Clock::now();
        try {
            worker = std::thread([this] {
                run();
            });
        } catch (const std::bad_alloc &) {
            return std::unexpected(
                diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-thread-start-allocation"));
        } catch (const std::system_error &) {
            return std::unexpected(diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-thread-start"));
        }
        control->worker = worker.get_id();
        value->started = true;
        value->transition(IpfConnectionState::CONNECTING);
        control->condition.notify_one();

        return {};
    }

    /** Execute the one lifetime connection, preserving pending batches and all grants across retries. */
    void run() noexcept {
        auto value = port;
        try {
            value->dispatch();

            if (!value->closed.load()) {
                auto prepared = TransferHost::create(bootstrap, options.transfer);

                if (!prepared) {
                    value->fail({prepared.error(), {}});
                    value->close();
                } else {
                    auto host = std::move(*prepared);
                    const auto lifetime = std::chrono::duration_cast<Clock::duration>(options.limits.lifetime);
                    const auto deadline = value->startedAt > Clock::time_point::max() - lifetime
                                              ? Clock::time_point::max()
                                              : value->startedAt + lifetime;
                    Sink sink(value, deadline);
                    auto retry = std::chrono::milliseconds(1000);

                    while (!value->closed.load()) {
                        {
                            std::lock_guard lock(value->mutex);

                            if (value->observed.attempts >= value->limits.attempts || Clock::now() >= deadline) {
                                if (!value->observed.diagnostic) {
                                    value->observed.diagnostic =
                                        diagnostic(IpfTransferError::WORK_LIMIT, "connection-lifetime");
                                }

                                break;
                            }

                            value->request.ifModifiedSince.reset();

                            if (!value->local && value->observed.capability != IpfLiveSupport::PROVIDED &&
                                value->observed.lastModified &&
                                value->observed.lastModified->time_since_epoch().count() != 0) {
                                value->request.ifModifiedSince =
                                    std::chrono::floor<std::chrono::seconds>(*value->observed.lastModified);
                            }
                        }

                        if (!sink.begin()) {
                            break;
                        }

                        auto result = host->attempt(value->source, value->request, value->stop.get_token(), sink);
                        sink.end(result);
                        value->dispatch();

                        if (sink.refused ||
                            (result.error && (result.error->error == IpfTransferError::WORK_LIMIT ||
                                              result.error->error == IpfTransferError::RESOURCE_LIMIT ||
                                              result.error->error == IpfTransferError::ALLOCATION_FAILURE))) {
                            value->close();
                            break;
                        }

                        if (value->closed.load()) {
                            break;
                        }

                        std::chrono::milliseconds period;
                        {
                            std::lock_guard lock(value->mutex);
                            period = value->period;
                        }
                        const auto delay = result.error ? retry : period;
                        const auto now = Clock::now();
                        const auto remaining =
                            now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
                                           : std::chrono::milliseconds::zero();
                        const auto due = now + std::min(delay, remaining);
                        {
                            std::unique_lock lock(control->mutex);
                            control->condition.wait_until(lock, due, [&] {
                                return control->stopping || value->closed.load();
                            });
                        }

                        if (result.error) {
                            std::lock_guard lock(value->mutex);
                            const auto doubled = retry.count() > std::numeric_limits<std::int64_t>::max() / 2
                                                     ? std::chrono::milliseconds::max()
                                                     : retry * 2;
                            retry = std::min(doubled, value->period);
                        }
                    }
                }
            }
        } catch (const std::bad_alloc &) {
            value->fail(diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-worker-allocation"));
        } catch (...) {
            value->fail(diagnostic(IpfTransferError::IO_ERROR, "connection-worker-failure"));
        }
        value->close();
        value->retire();
    }

    /** Creator-only joined retirement; public callbacks can request close but cannot join.
     * @return Acceptance or thread/reentry refusal before touching join.
     */
    std::expected<void, IpfConnectionDiagnostic> shutdown() {
        if (owner != std::this_thread::get_id()) {
            return std::unexpected(diagnostic(IpfTransferError::WRONG_THREAD, "connection-runtime-join"));
        }

        if (joining) {
            return std::unexpected(diagnostic(IpfTransferError::INVALID_STATE, "connection-runtime-reentry"));
        }

        joining = true;
        {
            std::lock_guard lock(control->mutex);
            control->stopping = true;
        }

        if (port) {
            port->close();
        }

        control->condition.notify_all();

        if (worker.joinable()) {
            worker.join();
        } else if (port) {
            port->retire();
        }

        {
            std::lock_guard lock(control->mutex);
            control->runtime = nullptr;
        }
        joining = false;

        return {};
    }

    /** Prepare the single immutable source/auth port before starting any worker.
     * @param address Borrowed bounded source, optional update suffix.
     * @param collector Explicit owning collector.
     * @param request Explicit stable HTTP credentials/provider, without caller conditional metadata.
     * @return Detached producer handle, or refusal before slot publication.
     */
    std::expected<std::shared_ptr<InstrumentProfileConnection>, IpfConnectionDiagnostic>
    create(std::string_view address, std::shared_ptr<InstrumentProfileCollector> collector,
           const IpfUrlRequest &request) {
        if (owner != std::this_thread::get_id()) {
            return std::unexpected(diagnostic(IpfTransferError::WRONG_THREAD, "connection-create-owner"));
        }

        if (options.limits.workUnits == 0) {
            return std::unexpected(diagnostic(IpfTransferError::WORK_LIMIT, "connection-domain-work"));
        }

        if (port || control->stopping) {
            return std::unexpected(diagnostic(IpfTransferError::INVALID_STATE, "connection-lifetime-slot"));
        }

        if (!collector || (!collector && collector.use_count() != 0) || request.ifModifiedSince ||
            (request.provider && request.authorization) || (!request.provider && request.provider.use_count() != 0)) {
            return std::unexpected(diagnostic(IpfTransferError::INVALID_OPTIONS, "connection-request"));
        }

        if (address.size() > options.transfer.limits.sourceBytes + 64 ||
            (request.authorization &&
             !detail::validAuthorization(request.authorization->value(), options.transfer.limits.authorizationBytes))) {
            return std::unexpected(diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-source-auth"));
        }

        auto period = std::chrono::milliseconds(60000);
        const auto suffix = address.rfind("[update=");

        if (suffix != std::string_view::npos) {
            if (address.back() != ']') {
                return std::unexpected(diagnostic(IpfTransferError::INVALID_SOURCE, "connection-update-suffix"));
            }

            const auto parsed = parsePeriod(address.substr(suffix + 8, address.size() - suffix - 9));

            if (!parsed) {
                return std::unexpected(diagnostic(IpfTransferError::INVALID_SOURCE, "connection-update-period"));
            }

            period = *parsed;
            address = address.substr(0, suffix);
        }

        auto source =
            detail::parseTransferSource(address, options.transfer.limits, !request.authorization && !request.provider);

        if (!source) {
            return std::unexpected(IpfConnectionDiagnostic{source.error(), {}});
        }

        if ((source->local && (request.authorization || request.provider)) ||
            (!source->local && !source->secure && (request.authorization || request.provider || source->userinfo))) {
            return std::unexpected(diagnostic(IpfTransferError::AUTHORIZATION_FAILURE, "connection-plaintext-auth"));
        }

        try {
            auto prepared = std::make_shared<Port>(control, options.limits);
            prepared->pendingBackingLimit = pendingBackingLimit;
            prepared->source = source->address();
            prepared->local = source->local;
            prepared->collector = std::move(collector);
            prepared->request = request;

            if (source->userinfo) {
                prepared->request.authorization = std::move(source->userinfo);
            }

            prepared->period = period;
            auto facade = std::shared_ptr<InstrumentProfileConnection>(new InstrumentProfileConnection(prepared));
            auto domain = collector_detail::ConnectionCollectorAccess::reserve(*prepared->collector);

            if (!domain) {
                return std::unexpected(IpfConnectionDiagnostic{
                    {IpfTransferError::RESOURCE_LIMIT, "connection-collector-domain"}, domain.error()});
            }

            prepared->domain = *domain;
            prepared->observed.workUnits = 1;
            port = std::move(prepared);

            return facade;
        } catch (const std::bad_alloc &) {
            return std::unexpected(diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-port"));
        }
    }
};
} // namespace connection_detail

InstrumentProfileConnection::InstrumentProfileConnection(std::shared_ptr<connection_detail::Port> port) noexcept
    : port_(std::move(port)) {
}

InstrumentProfileConnection::~InstrumentProfileConnection() {
    close();
}

void InstrumentProfileConnection::close() noexcept {
    auto port = port_;
    port->close();
}

IpfConnectionState InstrumentProfileConnection::getState() const {
    return observation().state;
}

std::optional<std::chrono::sys_time<std::chrono::milliseconds>> InstrumentProfileConnection::getLastModified() const {
    return observation().lastModified;
}

IpfConnectionObservation InstrumentProfileConnection::observation() const {
    auto port = port_;
    std::lock_guard lock(port->mutex);

    return port->observed;
}

std::expected<IpfCompletionWaitStatus, IpfConnectionDiagnostic>
InstrumentProfileConnection::awaitCompleted(std::chrono::steady_clock::time_point deadline,
                                            std::stop_token stop) const {
    auto port = port_;

    return connection_detail::awaitCompleted(port, deadline, stop);
}

std::chrono::milliseconds InstrumentProfileConnection::getUpdatePeriod() const {
    auto port = port_;
    std::lock_guard lock(port->mutex);

    return port->period;
}

std::expected<void, IpfConnectionDiagnostic>
InstrumentProfileConnection::setUpdatePeriod(std::chrono::milliseconds period) {
    auto port = port_;

    if (period.count() < 0) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::INVALID_OPTIONS, "connection-update-period"));
    }

    std::lock_guard lock(port->mutex);

    if (port->closed.load()) {
        return std::unexpected(connection_detail::diagnostic(IpfTransferError::INVALID_STATE, "connection-closed"));
    }

    port->period = period;

    return {};
}

std::expected<void, IpfConnectionDiagnostic> InstrumentProfileConnection::start() {
    auto port = port_;
    auto gate = port->control.lock();

    if (!gate) {
        return std::unexpected(connection_detail::diagnostic(IpfTransferError::INVALID_STATE, "connection-detached"));
    }

    std::lock_guard lock(gate->mutex);

    if (!gate->runtime || gate->stopping) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::INVALID_STATE, "connection-runtime-closed"));
    }

    return gate->runtime->start(port);
}

std::expected<std::uint64_t, IpfConnectionDiagnostic>
InstrumentProfileConnection::addStateChangeListener(StateChangeListener function) {
    auto port = port_;

    return port->listen(std::move(function));
}

bool InstrumentProfileConnection::removeStateChangeListener(std::uint64_t token) {
    auto port = port_;

    return port->forget(token);
}

InstrumentProfileConnectionRuntime::InstrumentProfileConnectionRuntime(
    std::unique_ptr<connection_detail::Runtime> runtime) noexcept
    : runtime_(std::move(runtime)) {
}

InstrumentProfileConnectionRuntime::~InstrumentProfileConnectionRuntime() = default;

std::expected<std::unique_ptr<InstrumentProfileConnectionRuntime>, IpfConnectionDiagnostic>
InstrumentProfileConnectionRuntime::create(const IpfConnectionRuntimeOptions &options) {
    const auto valid = connection_detail::Runtime::validate(options);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    auto bootstrap = transport::RuntimeBootstrap::create();

    if (!bootstrap) {
        return std::unexpected(IpfConnectionDiagnostic{
            {IpfTransferError::TRANSPORT_FAILURE, "connection-bootstrap", bootstrap.error()}, {}});
    }

    auto lease = transport::detail::BootstrapLease::acquireForRuntime(*bootstrap);

    if (!lease) {
        return std::unexpected(IpfConnectionDiagnostic{
            {IpfTransferError::TRANSPORT_FAILURE, "connection-bootstrap-lease", lease.error()}, {}});
    }

    try {
        auto impl = std::make_unique<connection_detail::Runtime>(std::move(*lease), options, *valid);
        impl->owned.emplace(std::move(*bootstrap));

        return std::unique_ptr<InstrumentProfileConnectionRuntime>(
            new InstrumentProfileConnectionRuntime(std::move(impl)));
    } catch (const std::bad_alloc &) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-runtime"));
    }
}

std::expected<std::unique_ptr<InstrumentProfileConnectionRuntime>, IpfConnectionDiagnostic>
InstrumentProfileConnectionRuntime::create(transport::RuntimeBootstrap &bootstrap,
                                           const IpfConnectionRuntimeOptions &options) {
    const auto valid = connection_detail::Runtime::validate(options);

    if (!valid) {
        return std::unexpected(valid.error());
    }

    auto lease = transport::detail::BootstrapLease::acquireForRuntime(bootstrap);

    if (!lease) {
        return std::unexpected(IpfConnectionDiagnostic{
            {IpfTransferError::TRANSPORT_FAILURE, "connection-bootstrap-lease", lease.error()}, {}});
    }

    try {
        auto impl = std::make_unique<connection_detail::Runtime>(std::move(*lease), options, *valid);

        return std::unique_ptr<InstrumentProfileConnectionRuntime>(
            new InstrumentProfileConnectionRuntime(std::move(impl)));
    } catch (const std::bad_alloc &) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-runtime"));
    }
}

std::expected<std::shared_ptr<InstrumentProfileConnection>, IpfConnectionDiagnostic>
InstrumentProfileConnectionRuntime::createConnection(std::string_view source,
                                                     std::shared_ptr<InstrumentProfileCollector> collector,
                                                     const IpfUrlRequest &request) {
    try {
        return runtime_->create(source, std::move(collector), request);
    } catch (const std::bad_alloc &) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::ALLOCATION_FAILURE, "connection-source-preparation"));
    } catch (const std::length_error &) {
        return std::unexpected(
            connection_detail::diagnostic(IpfTransferError::RESOURCE_LIMIT, "connection-source-capacity"));
    }
}

std::expected<void, IpfConnectionDiagnostic> InstrumentProfileConnectionRuntime::closeAndAwaitTermination() {
    return runtime_->shutdown();
}
} // namespace org::ttldtor::qdcxx::ipf
