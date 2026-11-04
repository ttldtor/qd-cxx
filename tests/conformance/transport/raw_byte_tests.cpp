// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "../recovery_test_peer.hpp"
#include "raw_byte_owner.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace t = org::ttldtor::qdcxx::transport;
namespace td = t::detail;
namespace p = org::ttldtor::qdcxx::protocol;
namespace x = org::ttldtor::qdcxx::test;
using namespace std::chrono_literals;

namespace {

/** Make finite numeric-route native settings for controlled loopback tests.
 * @param port Explicit owned listener port.
 * @return Copied endpoint view and generous finite native backing allowance.
 */
t::TcpOptions options(std::uint16_t port) {
    return {"127.0.0.1", port, {}, 53, 1s, 1s, 5s, 5s, 1ms, {64, 3, 64, 16, 65536, 8192, 8 * 1024 * 1024}};
}

/** Renew finite preparation/copy work explicitly in the test caller.
 * @return Finite source-derived work allowance, never renewed by production code.
 */
p::LiveWork byteTurn() {
    return {10000, 10000000, 10000000, 100 * 1024 * 1024};
}

/** Renew one finite native turn.
 * @return Explicit native/DNS work, ample for one bounded poll.
 */
t::TcpWork nativeTurn() {
    return {128, 128, 10000000, 128, 2, 131070, 1000000000};
}

/** Copy an ordinary synthetic test string into helper-owned bytes.
 * @param text Ordinary nonsecret fixture text.
 * @return Owning byte vector.
 */
std::vector<std::byte> bytes(std::string_view text) {
    const auto span = std::as_bytes(std::span{text.data(), text.size()});

    return {span.begin(), span.end()};
}

/** Drive a bounded raw operation to an explicit test condition.
 * @tparam Predicate Synchronous owner-only nullary predicate.
 * @param owner Stable raw operation.
 * @param done Exact completion condition.
 * @return True if the condition became true within five seconds.
 */
template <class Predicate> bool pump(td::RawByteOwner &owner, Predicate done) {
    const auto deadline = td::Clock::now() + 5s;

    while (!done() && td::Clock::now() < deadline) {
        auto native = nativeTurn();
        auto work = byteTurn();
        const auto result = owner.poll(native, work);
        REQUIRE(result);

        if (result->status == t::TcpPollStatus::IDLE || result->status == t::TcpPollStatus::YIELD) {
            std::this_thread::sleep_for(1ms);
        }
    }

    return done();
}

/** Observe an exact immutable application write tuple across simulated WANT entries. */
struct WantObservation {
    const std::byte *address = nullptr;  ///< First borrowed immutable output address.
    std::vector<std::byte> copied;       ///< Detached comparison bytes.
    unsigned entries = 0;                ///< Number of controlled codec entries.
    bool exact = true;                   ///< Every retry kept address, length and content identical.
    bool retired = false;                ///< Destructor ran only after native core retirement.
    td::RawByteOwner *owner = nullptr;   ///< Optional request-only reentry target, never retained by production.
    bool closeStayedRequestOnly = false; ///< No inline native/engine abort occurred during reentry.
};

/** Private deterministic codec substitute; no SSL or application callback is installed. */
class WantEngine final : public td::ByteEngine {
    WantObservation &observed_; ///< Test-owned observation outlives the whole operation.
    bool aborted_ = false;      ///< Cached allocation-free cutoff.
    bool copyInput_ = false;    ///< Internal instrumentation may pass one verified input byte.
    bool inputReady_ = false;   ///< One controlled byte awaits explicit plaintext consumption.
    std::byte inputByte_{};     ///< Stable fake-codec input backing.

    public:
    /** Retain test observation only.
     * @param observed Stable test fixture.
     * @param copyInput Pass one verified input byte through the controlled codec seam.
     */
    explicit WantEngine(WantObservation &observed, bool copyInput = false)
        : observed_(observed), copyInput_(copyInput) {
    }

    ~WantEngine() override {
        observed_.retired = true;
    }

    [[nodiscard]] std::span<const std::byte> ciphertext() const noexcept override {
        return {};
    }

    [[nodiscard]] bool consumeCiphertext(std::size_t count) noexcept override {
        return count == 0;
    }

    [[nodiscard]] std::span<const std::byte> plaintext() const noexcept override {
        if (inputReady_) {
            return {&inputByte_, 1};
        }

        return {};
    }

    void consumePlaintext(std::size_t bytes) noexcept override {
        if (bytes == 1) {
            inputReady_ = false;
        }
    }

    [[nodiscard]] bool needsStep(bool input, bool output) const noexcept override {
        return (output || (copyInput_ && input)) && !aborted_ && !inputReady_;
    }

    [[nodiscard]] td::ByteEngineStep advance(std::span<const std::byte> input,
                                             std::span<const std::byte> output) noexcept override {
        if (copyInput_ && !input.empty()) {
            inputByte_ = input.front();
            inputReady_ = true;

            return {1, 0, true, std::nullopt};
        }

        ++observed_.entries;

        if (observed_.entries == 1) {
            observed_.address = output.data();
            observed_.copied.assign(output.begin(), output.end());
        } else {
            observed_.exact &= observed_.address == output.data() && std::ranges::equal(observed_.copied, output);
        }

        if (observed_.owner) {
            const auto requested = observed_.owner->requestClose();
            observed_.closeStayedRequestOnly =
                requested.has_value() && !aborted_ && !observed_.owner->core().closeApplied;
        }

        return {0, observed_.entries < 3 ? 0 : output.size(), true, std::nullopt};
    }

    [[nodiscard]] bool verified() const noexcept override {
        return true;
    }

    [[nodiscard]] bool finished() const noexcept override {
        return false;
    }

    [[nodiscard]] std::optional<t::TcpDiagnostic> nativeEnd() noexcept override {
        return std::nullopt;
    }

    void abort() noexcept override {
        aborted_ = true;
    }
};

} // namespace

TEST_CASE("raw native flow owns output and exposes only explicitly consumed input prefixes") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    x::TcpPeerScript script;
    script.expectedClientBytes = bytes("request-native-short-writes");
    script.replyBytes = bytes("response-with-no-QTP-frame");
    x::OwnedTcpPeer peer({{script}});
    REQUIRE(peer.port() != 0);
    auto work = byteTurn();
    auto made = td::RawByteOwner::create(*bootstrap, options(peer.port()), {5, 128}, work);
    REQUIRE(made);
    auto &owner = **made;
    auto request = script.expectedClientBytes;
    REQUIRE(owner.write(request, work));
    std::ranges::fill(request, std::byte{0});
    REQUIRE(owner.start());
    std::vector<std::byte> received;
    REQUIRE(pump(owner, [&] {
        const auto input = owner.input();
        REQUIRE(input);

        if (!input->empty()) {
            const auto count = (std::min)(std::size_t{2}, input->size());
            received.insert(received.end(), input->begin(), input->begin() + count);
            REQUIRE(owner.consumeInput(count));
        }

        return owner.core().terminal().has_value() && owner.input()->empty();
    }));
    CHECK(received == script.replyBytes);
    CHECK(owner.core().terminal()->peerEnded);
    CHECK(owner.core().terminal()->primary.error == t::TcpError::NONE);
    CHECK(owner.core().terminal()->reportedWriteBytes == script.expectedClientBytes.size());
    CHECK(owner.core().resources().sessionBytes == 0);
    CHECK(owner.core().pending == 0);
    CHECK(owner.core().callbackDepth == 0);
    const auto terminalBeforeClose = *owner.core().terminal();

    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        REQUIRE(owner.requestClose());
        t::TcpWork noNativeWork{};
        p::LiveWork noByteWork{};
        const auto closed = owner.poll(noNativeWork, noByteWork);
        REQUIRE(closed);
        CHECK(closed->status == t::TcpPollStatus::CLOSED);
        const auto after = *owner.core().terminal();
        CHECK(after.primary.error == terminalBeforeClose.primary.error);
        CHECK(after.primary.stage == terminalBeforeClose.primary.stage);
        CHECK(after.primary.nativeCode == terminalBeforeClose.primary.nativeCode);
        CHECK(after.primary.protocolError == terminalBeforeClose.primary.protocolError);
        CHECK(after.secondary.error == terminalBeforeClose.secondary.error);
        CHECK(after.secondary.stage == terminalBeforeClose.secondary.stage);
        CHECK(after.secondary.nativeCode == terminalBeforeClose.secondary.nativeCode);
        CHECK(after.secondary.protocolError == terminalBeforeClose.secondary.protocolError);
        CHECK(after.aborted == terminalBeforeClose.aborted);
        CHECK(after.peerEnded == terminalBeforeClose.peerEnded);
        CHECK(after.writeAcceptanceUnknown == terminalBeforeClose.writeAcceptanceUnknown);
        CHECK(after.reportedWriteBytes == terminalBeforeClose.reportedWriteBytes);
        CHECK(after.discardedInputBytes == terminalBeforeClose.discardedInputBytes);
        CHECK(after.pendingOutputBytes == terminalBeforeClose.pendingOutputBytes);
    }

    peer.stop();
    CHECK(peer.snapshot().sessions.front().matched);
}

TEST_CASE("raw terminal preserves a retained application prefix and reports empty EOF separately") {
    for (const bool sendPrefix : {false, true}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        x::TcpPeerScript script;
        script.initialBytes = sendPrefix ? bytes("prefix") : std::vector<std::byte>{};
        x::OwnedTcpPeer peer({{script}});
        auto work = byteTurn();
        auto made = td::RawByteOwner::create(*bootstrap, options(peer.port()), {64, 64}, work);
        REQUIRE(made);
        auto &owner = **made;
        REQUIRE(owner.start());

        if (sendPrefix) {
            REQUIRE(pump(owner, [&] {
                return !owner.input()->empty();
            }));
            // A caller can request retirement without consuming an already published prefix.
            REQUIRE(owner.requestClose());
        }

        REQUIRE(pump(owner, [&] {
            return owner.core().terminal().has_value();
        }));
        const auto retained = owner.input();
        REQUIRE(retained);
        CHECK(std::ranges::equal(*retained, script.initialBytes));
        CHECK(owner.core().terminal()->peerEnded == !sendPrefix);
        REQUIRE_FALSE(owner.consumeInput(retained->size() + 1));
        REQUIRE(owner.consumeInput(retained->size()));
        CHECK(owner.input()->empty());
    }
}

TEST_CASE("raw exact backing and native preparation work reject one byte or visit below") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto config = options(1);
    const td::RawByteLimits limits{17, 23};
    const auto initial = byteTurn();
    auto remaining = initial;
    auto made = td::RawByteOwner::create(*bootstrap, config, limits, remaining);
    REQUIRE(made);
    const auto exact = (*made)->core().resources().adapterBytes;
    const auto usedBytes = initial.bytes - remaining.bytes;
    const auto usedVisits = initial.visits - remaining.visits;
    const auto usedOperations = initial.operations - remaining.operations;
    made->reset();
    config.limits.residentBytes = exact;
    auto exactWork = p::LiveWork{usedOperations, usedVisits, 0, usedBytes};
    auto accepted = td::RawByteOwner::create(*bootstrap, config, limits, exactWork);
    REQUIRE(accepted);
    CHECK(exactWork.operations == 0);
    CHECK(exactWork.visits == 0);
    CHECK(exactWork.bytes == 0);
    CHECK((*accepted)->core().resources().adapterBytes == exact);
    auto outputWork = byteTurn();
    const auto originalOutputWork = outputWork;
    const std::vector<std::byte> overCapacity(limits.outputBytes + 1, std::byte{0x41});
    REQUIRE_FALSE((*accepted)->write(overCapacity, outputWork));
    CHECK(outputWork.operations == originalOutputWork.operations);
    CHECK(outputWork.bytes == originalOutputWork.bytes);
    const auto atCapacity = std::span{overCapacity}.first(limits.outputBytes);
    auto exactOutputWork = p::LiveWork{1, 0, 0, limits.outputBytes};
    REQUIRE((*accepted)->write(atCapacity, exactOutputWork));
    CHECK(exactOutputWork.operations == 0);
    CHECK(exactOutputWork.bytes == 0);
    CHECK((*accepted)->pendingOutput() == limits.outputBytes);
    accepted->reset();

    for (const auto coordinate : {0, 1, 2}) {
        auto work = p::LiveWork{usedOperations, usedVisits, 0, usedBytes};
        auto reduced = config;

        if (coordinate == 0) {
            --reduced.limits.residentBytes;
        } else if (coordinate == 1) {
            --work.bytes;
        } else {
            --work.visits;
        }

        const auto rejected = td::RawByteOwner::create(*bootstrap, reduced, limits, work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == (coordinate == 0 ? t::TcpError::RESOURCE_LIMIT : t::TcpError::WORK_LIMIT));
    }
}

TEST_CASE("raw wrong thread and request-only abort preserve callback and bootstrap lifetime") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    x::TcpPeerScript script;
    script.waitForControllerClose = true;
    x::OwnedTcpPeer peer({{script}});
    auto witness = td::BootstrapLease::acquire(*bootstrap);
    REQUIRE(witness);
    auto work = byteTurn();
    auto made = td::RawByteOwner::create(*bootstrap, options(peer.port()), {64, 64}, work);
    REQUIRE(made);
    auto &owner = **made;
    CHECK(witness->state()->borrowers == 3);
    std::optional<t::TcpDiagnostic> wrong;
    std::jthread foreign([&] {
        const auto result = owner.requestClose();
        REQUIRE_FALSE(result);
        wrong = result.error();
    });
    foreign.join();
    REQUIRE(wrong);
    CHECK(wrong->error == t::TcpError::WRONG_THREAD);
    REQUIRE(owner.start());
    REQUIRE(pump(owner, [&] {
        return owner.core().readPending;
    }));
    const auto pending = owner.core().pending;
    REQUIRE(pending != 0);
    REQUIRE(owner.requestClose());
    CHECK(owner.core().pending == pending);
    CHECK_FALSE(owner.core().pauseCancellation);
    CHECK_FALSE(owner.core().closeApplied);
    made->reset(); // Destructor owns and drains the pending native cancellation, including its late handler.
    CHECK(witness->state()->borrowers == 1);
}

TEST_CASE("raw pending native write abort accounts a raced prefix before releasing output backing") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    x::TcpPeerScript script;
    script.waitForControllerClose = true;
    x::OwnedTcpPeer peer({{script}});
    auto config = options(peer.port());
    constexpr std::size_t OUTPUT_BYTES = 1024 * 1024;
    config.socketSendBufferBytes = 1024;
    config.limits.writeBytes = 65536;
    auto work = byteTurn();
    auto made = td::RawByteOwner::create(*bootstrap, config, {64, OUTPUT_BYTES}, work);
    REQUIRE(made);
    auto &owner = **made;
    const std::vector<std::byte> output(OUTPUT_BYTES, std::byte{0x41});
    REQUIRE(owner.write(output, work));
    REQUIRE(owner.start());
    REQUIRE(pump(owner, [&] {
        return owner.core().writePending;
    }));
    const auto address = owner.core().applicationBytes().data();
    REQUIRE(owner.requestClose());
    CHECK(owner.core().applicationBytes().data() == address);
    CHECK_FALSE(owner.core().closeApplied);
    REQUIRE(pump(owner, [&] {
        return owner.core().terminal().has_value();
    }));
    const auto terminal = owner.core().terminal();
    REQUIRE(terminal);
    CHECK(terminal->reportedWriteBytes + owner.core().pendingApplicationAtClose == OUTPUT_BYTES);
    CHECK(terminal->pendingOutputBytes == owner.core().pendingApplicationAtClose);
    CHECK(owner.core().pending == 0);
    CHECK(owner.core().callbackDepth == 0);
}

TEST_CASE("raw immutable engine WANT retries and reentrant request-only close") {
    for (const bool reenter : {false, true}) {
        auto bootstrap = t::RuntimeBootstrap::create();
        REQUIRE(bootstrap);
        x::TcpPeerScript script;
        script.waitForControllerClose = true;
        x::OwnedTcpPeer peer({{script}});
        WantObservation observed;
        auto work = byteTurn();
        auto made = td::RawByteOwner::create(*bootstrap, options(peer.port()), {64, 64}, work,
                                             std::make_unique<WantEngine>(observed));
        REQUIRE(made);
        auto &owner = **made;
        auto output = bytes("immutable-WANT-tuple");
        REQUIRE(owner.write(output, work));
        std::ranges::fill(output, std::byte{0});
        observed.owner = reenter ? &owner : nullptr;
        REQUIRE(owner.start());
        REQUIRE(pump(owner, [&] {
            return reenter ? owner.core().terminal().has_value() : observed.entries == 3;
        }));

        if (reenter) {
            CHECK(observed.closeStayedRequestOnly);
            CHECK(owner.core().terminal()->aborted);
        } else {
            CHECK(observed.exact);
            CHECK(owner.pendingOutput() == 0);
        }

        made->reset();
        CHECK(observed.retired);
    }
}

TEST_CASE("raw failed DNS and connect remain native failures without protocol completion") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);

    for (const bool dns : {false, true}) {
        // This owned peer reserves an endpoint, then retires it before the controlled connect.
        x::TcpPeerScript script;
        x::OwnedTcpPeer peer({{script}});
        const auto port = peer.port();
        peer.stop();
        auto config = options(port);
        const std::array<t::DnsServer, 1> servers{{{{127, 0, 0, 1}}}};

        if (dns) {
            config.endpoint = "missing.invalid";
            config.dnsServers = servers;
            config.dnsPort = port;
            config.dnsTimeout = 100ms;
            config.limits.residentBytes = 128 * 1024 * 1024;
        }

        auto work = byteTurn();

        if (dns) {
            const auto dnsWork = td::RuntimeCore::dnsPreparationWorkRequirement(1);
            REQUIRE(dnsWork);
            work.bytes += dnsWork->bytes;
            work.visits += dnsWork->visits;
        }

        auto made = td::RawByteOwner::create(*bootstrap, config, {64, 64}, work);
        INFO("factory category=" << (made ? 0 : static_cast<int>(made.error().error))
                                 << " stage=" << (made ? "none" : made.error().stage));
        REQUIRE(made);
        auto &owner = **made;
        REQUIRE(owner.start());
        REQUIRE(pump(owner, [&] {
            return owner.core().terminal().has_value();
        }));
        const auto terminal = owner.core().terminal();
        REQUIRE(terminal);
        CHECK_FALSE(terminal->peerEnded);
        CHECK(terminal->primary.error != t::TcpError::NONE);
        CHECK(terminal->primary.error != t::TcpError::PROTOCOL_FAILURE);
        CHECK(owner.input()->empty());
    }
}

#ifdef _WIN32
TEST_CASE("raw fatal read prefix cuts new output admission before prefix delivery") {
    auto bootstrap = t::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    constexpr std::size_t WINDOW_BYTES = 64;

    for (const bool withCodec : {false, true}) {
        WantObservation observed;
        std::unique_ptr<td::ByteEngine> codec;

        if (withCodec) {
            codec = std::make_unique<WantEngine>(observed, true);
        }

        auto work = byteTurn();
        auto made =
            td::RawByteOwner::create(*bootstrap, options(1), {WINDOW_BYTES, WINDOW_BYTES}, work, std::move(codec));
        REQUIRE(made);
        auto &owner = **made;
        auto &core = owner.core();
        // Internal instrumentation of the Windows completion tuple, not an actual OS read failure.
        core.runState = t::TcpState::CONNECTED;
        REQUIRE(core.setDeadline(core.readDeadline, core.options.readTimeout));
        core.input[0] = std::byte{0x50};
        core.pending = 1;
        core.readPending = true;
        core.completed(true, asio::error::connection_reset, 1);
        REQUIRE(core.outcome.primary.error == t::TcpError::READ_FAILURE);
        REQUIRE_FALSE(core.closeRequested);
        const auto beforeWork = work;
        const auto beforeError = core.outcome.primary;
        const auto beforeAccepted = core.outcome.reportedWriteBytes;
        const auto beforeAddress = core.applicationBytes().data();
        // The private factory owns this full zeroed window even while its published suffix is empty.
        const std::vector<std::byte> beforeBacking(beforeAddress, beforeAddress + WINDOW_BYTES);
        const auto block = bytes("must-not-be-admitted-after-read-failure");
        const auto rejected = owner.write(block, work);
        CHECK_FALSE(rejected);

        if (!rejected) {
            CHECK(rejected.error().error == t::TcpError::INVALID_STATE);
        }

        CHECK(work.operations == beforeWork.operations);
        CHECK(work.visits == beforeWork.visits);
        CHECK(work.textUnits == beforeWork.textUnits);
        CHECK(work.bytes == beforeWork.bytes);
        CHECK(owner.pendingOutput() == 0);
        CHECK(core.applicationBytes().data() == beforeAddress);
        CHECK(std::ranges::equal(beforeBacking, std::span<const std::byte>{beforeAddress, WINDOW_BYTES}));
        CHECK(core.outcome.reportedWriteBytes == beforeAccepted);
        CHECK(core.outcome.primary.error == beforeError.error);
        CHECK(core.outcome.primary.stage == beforeError.stage);
        CHECK(core.outcome.primary.nativeCode == beforeError.nativeCode);
        CHECK(core.outcome.primary.protocolError == beforeError.protocolError);
        CHECK(observed.entries == 0);
        REQUIRE(pump(owner, [&] {
            return core.terminal().has_value();
        }));
        REQUIRE(owner.input());
        CHECK(observed.entries == 0);
        REQUIRE(owner.input()->size() == 1);
        CHECK(owner.input()->front() == std::byte{0x50});
        CHECK(core.terminal()->primary.error == t::TcpError::READ_FAILURE);
        CHECK(core.terminal()->primary.nativeCode == beforeError.nativeCode);
        CHECK_FALSE(core.terminal()->peerEnded);
        REQUIRE(owner.consumeInput(1));
    }
}
#endif
