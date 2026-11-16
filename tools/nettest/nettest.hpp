// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "qd/core/subscription.hpp"
#include "qd/protocol/live_session.hpp"

#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace org::ttldtor::qdcxx::nettest {
using Clock = std::chrono::steady_clock;
constexpr std::size_t MAX_CONNECTIONS = 8;
constexpr std::size_t MAX_SYMBOLS = 64;
constexpr std::size_t MAX_SYMBOL_UNITS = 256;
constexpr std::size_t MAX_BATCH = 128;
constexpr std::size_t MAX_FIELDS = 256;
constexpr std::size_t FRAME_BYTES = 65536;
constexpr std::int32_t SENDER_ID = 0;

enum class Mode { PRODUCER, CONSUMER };

struct Options {
    Mode mode = Mode::CONSUMER;
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
    bool hasPort = false;
    std::filesystem::path bundle;
    std::u16string record = u"TimeAndSale";
    core::SubscriptionFamily family = core::SubscriptionFamily::STREAM;
    std::chrono::seconds duration{10};
    std::chrono::seconds startup{10};
    std::uint32_t rate = 1000;
    std::size_t batch = 100;
    std::size_t instances = 1;
    std::vector<std::u16string> symbols;
    std::int64_t historyBound = 0;
    bool qdText = false;
    bool verifyStream = false;
    bool profileProducer = false;
};

struct Catalog {
    core::LocalSchema schema;
    std::vector<std::u16string> fieldNames;
};

struct Messages {
    protocol::LiveMessageView data;
    protocol::LiveMessageView add;
    protocol::LiveMessageView remove;
};

/** Elapsed producer work on DATA calls, measured with the monotonic clock. */
struct ProducerProfile {
    std::uint64_t generationNs = 0;
    std::uint64_t generatedBatches = 0;
    std::uint64_t composeNs = 0;
    std::uint64_t composedFrames = 0;
    std::uint64_t writeNs = 0;
    std::uint64_t writeAttempts = 0;
    std::uint64_t wouldBlock = 0;
    std::uint64_t partialWrites = 0;
};

struct Report {
    ProducerProfile profile;
    std::uint64_t connections = 0;
    std::uint64_t generated = 0;
    std::uint64_t composed = 0;
    std::uint64_t written = 0;
    std::uint64_t wireRecords = 0;
    std::uint64_t delivered = 0;
    std::uint64_t checksum = 0;
    std::uint64_t bytesRead = 0;
    std::uint64_t bytesWritten = 0;
    std::chrono::nanoseconds measured{};
};

int entry(std::span<const std::string_view> arguments);
Catalog loadCatalog(const Options &options);
Messages messages(core::SubscriptionFamily family);
protocol::LiveWork protocolWork();
protocol::LiveLimits sessionLimits();
protocol::SubscriptionWriter writer(const Catalog &catalog);
Report produce(const Options &options, const Catalog &catalog);
Report consume(const Options &options, const Catalog &catalog);

} // namespace org::ttldtor::qdcxx::nettest
