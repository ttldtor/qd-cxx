// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/transport/tcp_recovery.hpp"

#include <array>
#include <bit>
#include <chrono>
#include <doctest/doctest.h>
#include <string>
#include <thread>
#include <vector>

namespace org::ttldtor::qdcxx::test::recovery {
namespace c = core;
namespace p = protocol;
namespace m = mux;
namespace t = transport;
namespace d = client;
namespace e = encoding;
using namespace std::chrono_literals;

/** Create immutable indexed provenance usable by each family without external schema loading.
 * @return One ordinary record with two raw time words and one integer value.
 */
inline c::LocalSchema schema() {
    const std::array<c::LocalRecord, 1> records{{{0,
                                                  u"R",
                                                  true,
                                                  {{8}, {8}, {8}},
                                                  {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"V", 2}},
                                                  c::IndexedTimePairInt32{0, 1}}}};
    auto result = c::LocalSchema::create(records, {16, 128, 256, 128, 16384}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Create explicit bounded numeric-route settings; tests choose the actual ephemeral port.
 * @param port Owned loopback listener port.
 * @param attempts Lifetime attempt limit.
 * @param delay Fixed delay, zero only for one attempt.
 * @return Valid unactivated endpoint inputs.
 */
inline d::EndpointSettings settings(std::uint16_t port, std::uint32_t attempts = 2,
                                    std::chrono::milliseconds delay = 20ms) {
    auto value = d::controlledStreamSettings();
    REQUIRE(value);
    value->host = "127.0.0.1";
    value->port = port;
    value->retry = {attempts, attempts == 1 ? 0ms : delay};
    value->timeouts.read = 2s;
    value->timeouts.write = 2s;
    value->timeouts.connect = 2s;
    value->timeouts.dns = 2s;
    value->timeouts.poll = 1ms;

    return std::move(*value);
}

/** Copy explicit immutable endpoint input without creating a runtime.
 * @param local Required exact provenance.
 * @param input Explicit valid settings.
 * @return Complete snapshot.
 */
inline d::EndpointOptions endpoint(const c::LocalSchema &local, const d::EndpointSettings &input) {
    auto value = d::EndpointOptions::create(local, input);
    REQUIRE(value);

    return std::move(*value);
}

/** Supply explicit complete outgoing mappings with a fixed client sender ID.
 * @return Fresh blueprint independently validated by the real writer.
 */
inline std::vector<p::OutgoingRecord> blueprint() {
    return {{71, 0, u"R", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"V", 8, 2}}}};
}

/** Supply independent finite recovery reservations, including future desired-map bounds.
 * @return Explicit synthetic test limits, never product defaults.
 */
inline t::RecoveryOptions options() {
    t::RecoveryOptions result;
    result.limits.schemaCopies = {1024, 4096, 4096, 65536};
    result.limits.blueprintCopy = {1024, 4096, 65536};
    result.limits.adoptedBlueprint = {1024, 4096, 65536};
    result.limits.endpointCopyBytes = 2 * 1024 * 1024;
    result.limits.writerConfigurationBytes = 4 * 1024 * 1024;

    for (auto &replay : result.limits.replay)
        replay = {128, 16384};

    return result;
}

/** Give explicit mutable actual queue credits.
 * @return One funded bounded owner turn.
 */
inline m::QueueWork queueWork() {
    return {100000,
            100000000,
            100000000,
            100000000,
            100000000,
            10000,
            10000,
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000},
            {100000, 100000000, 100000000, 100000000}};
}

/** Give actual separate preparation coordinates, including Core/held-event retirement reserves.
 * @return One explicit funded preparation allowance.
 */
inline t::RecoveryPreparationWork preparation() {
    return {{10000, 100000000, 100000000, 100000000},
            {10000, 100000000, 100000000, 100000000},
            {queueWork(), queueWork(), queueWork()}};
}

/** Give one fresh independent native/recovery/session turn; the test, not the library, renews it.
 * @return Explicit mutable caller credits.
 */
inline t::RecoveryTurnWork turn() {
    return {{10000, 100000000, 100000000, 100000000},
            {10000, 100000000, 100000000, 100000000},
            {1000, 10000, 10000000, 10000, 10000, 10000000, 10000000},
            {10000, 100000000, 100000000, 100000000},
            {queueWork(), queueWork(), queueWork()}};
}

/** Append one canonical compact integer through the independent primitive codec.
 * @param bytes Owned fixture bytes.
 * @param value Exact raw signed value.
 */
inline void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto encoded = e::writeCompactLong(value, buffer);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Append one exact non-null UTFString field.
 * @param bytes Owned fixture bytes.
 * @param value Explicit fixture text.
 */
inline void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 8192> buffer{};
    const auto encoded = e::writeUtfString(value, buffer, {8192, 1024});
    REQUIRE(encoded.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
}

/** Wrap a complete fixture body in one selected message envelope.
 * @param type Actual wire identity.
 * @param payload Complete body excluding the type.
 * @return Complete owning frame.
 */
inline std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Append complete bounded fixture bytes.
 * @param target Owned destination.
 * @param source Whole independent frame or prefix.
 */
inline void append(std::vector<std::byte> &target, std::span<const std::byte> source) {
    target.insert(target.end(), source.begin(), source.end());
}

/** Build an independent complete peer descriptor for all three subscription families.
 * @return DXP3 descriptor declaring actual SEND data and RECEIVE add/remove support.
 */
inline std::vector<std::byte> ready() {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(payload, 0);
    number(payload, 3);
    constexpr std::array<std::int32_t, 3> data{10, 15, 20};
    constexpr std::array<std::u16string_view, 3> names{u"TICKER_DATA", u"STREAM_DATA", u"HISTORY_DATA"};

    for (std::size_t index = 0; index != data.size(); ++index) {
        number(payload, data[index]);
        text(payload, names[index]);
        number(payload, 0);
    }

    number(payload, 6);
    constexpr std::array<std::int32_t, 6> messages{11, 12, 16, 17, 21, 22};
    constexpr std::array<std::u16string_view, 6> subscriptions{
        u"TICKER_ADD_SUBSCRIPTION",    u"TICKER_REMOVE_SUBSCRIPTION", u"STREAM_ADD_SUBSCRIPTION",
        u"STREAM_REMOVE_SUBSCRIPTION", u"HISTORY_ADD_SUBSCRIPTION",   u"HISTORY_REMOVE_SUBSCRIPTION"};

    for (std::size_t index = 0; index != messages.size(); ++index) {
        number(payload, messages[index]);
        text(payload, subscriptions[index]);
        number(payload, 0);
    }

    return frame(1, payload);
}

/** Build one independent complete incoming record declaration.
 * @param sender Peer-selected sender ID, deliberately varied across attempts.
 * @return Type2 whole frame binding R and its three compact fields.
 */
inline std::vector<std::byte> description(std::int32_t sender) {
    std::vector<std::byte> payload;
    number(payload, sender);
    text(payload, u"R");
    number(payload, 3);

    for (auto name : {u"TimeHigh", u"TimeLow", u"V"}) {
        text(payload, name);
        number(payload, 8);
    }

    return frame(2, payload);
}

/** Build one selected ordinary data point through independent primitive codecs.
 * @param sender Actual current peer sender ID.
 * @param symbol Exact synthetic symbol.
 * @param value Integer payload.
 * @param type Actual family type10/15/20.
 * @param time Raw signed source time, not a guessed replay cursor.
 * @return Whole selected DATA frame.
 */
inline std::vector<std::byte> data(std::int32_t sender, std::u16string_view symbol, std::int32_t value,
                                   std::int32_t type = 15, std::int64_t time = 1) {
    std::vector<std::byte> payload{std::byte{0xfd}};
    number(payload, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        std::array<std::byte, 4> buffer{};
        const auto encoded = e::writeUtfChar(unit, buffer);
        payload.insert(payload.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    }

    number(payload, sender);
    const auto bits = std::bit_cast<std::uint64_t>(time);
    number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
    number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
    number(payload, value);

    return frame(type, payload);
}

/** Stop and actually drain an owned plain recovery run under explicit fresh caller turns.
 * @param owner Actual public runtime.
 */
inline void stop(t::TcpRecovery &owner) {
    REQUIRE(owner.requestStop());
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (owner.status().phase != t::RecoveryPhase::CLOSED && std::chrono::steady_clock::now() < deadline) {
        auto work = turn();
        const auto progressed = owner.poll(work);
        REQUIRE(progressed);
        std::this_thread::sleep_for(1ms);
    }

    CHECK(owner.status().phase == t::RecoveryPhase::CLOSED);
}

} // namespace org::ttldtor::qdcxx::test::recovery
