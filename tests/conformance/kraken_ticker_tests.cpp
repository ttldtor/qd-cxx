// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "ticker_decoder.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>

using namespace org::ttldtor::qdcxx;

TEST_CASE("acknowledgements and control messages") {
    auto accepted = kraken::decodeTicker(
        R"({"method":"subscribe","success":true,"result":{"channel":"ticker","symbol":"BTC/USD"}})");
    REQUIRE(accepted);
    CHECK(accepted->kind == kraken::MessageKind::ACKNOWLEDGEMENT);
    auto rejected = kraken::decodeTicker(R"({"method":"subscribe","success":false,"error":"denied"})");
    REQUIRE(rejected);
    CHECK(rejected->kind == kraken::MessageKind::REJECTION);
    CHECK(rejected->detail == "denied");
    auto maximumId = kraken::decodeTicker(
        R"({"method":"subscribe","success":true,"req_id":18446744073709551615,"result":{"channel":"ticker","symbol":"BTC/USD"}})");
    REQUIRE(maximumId);
    REQUIRE(maximumId->requestId);
    CHECK(*maximumId->requestId == UINT64_MAX);
    CHECK(kraken::decodeTicker(R"({"channel":"heartbeat"})")->kind == kraken::MessageKind::HEARTBEAT);
    auto status = kraken::decodeTicker(R"({"channel":"status","data":[{"system":"online"}]})");
    REQUIRE(status);
    CHECK(status->kind == kraken::MessageKind::STATUS);
    CHECK(status->detail == "online");
}

TEST_CASE("exact values remain owned after input changes") {
    std::string input =
        R"({"channel":"ticker","type":"snapshot","data":[{"symbol":"BTC/USD","bid":123.45678901,"ask":1.23456789012345e2,"bid_qty":0.00000001,"ask_qty":10.00,"timestamp":"2026-01-01T00:00:00Z"}]})";
    auto result = kraken::decodeTicker(input);
    REQUIRE(result);
    input.clear();
    REQUIRE(result->entries.size() == 1);
    const auto &row = result->entries.front();
    CHECK(result->kind == kraken::MessageKind::SNAPSHOT);
    CHECK(row.quote.symbol == u"BTC/USD");
    CHECK(row.quote.bidPrice.toString() == "123.45678901");
    CHECK(row.quote.askPrice.toString() == "123.456789012345");
    CHECK(row.quote.bidSize.toString() == "1E-8");
    CHECK(row.quote.askSize.toString() == "10");
    CHECK(row.quote.bidTimeMs == 0);
    CHECK(row.quote.askTimeMs == 0);
    CHECK(row.quote.timeMs == 0);
    CHECK(row.quote.eventTime == 0);
    CHECK(row.quote.sequence == 0);
    CHECK(row.quote.timeNanoPart == 0);
    CHECK(row.timestamp == "2026-01-01T00:00:00Z");
}

TEST_CASE("multiple update rows preserve order") {
    auto result = kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"},{"symbol":"BTC/USD","bid":5,"ask":6,"bid_qty":7,"ask_qty":8,"timestamp":"b"}]})");
    REQUIRE(result);
    CHECK(result->kind == kraken::MessageKind::UPDATE);
    REQUIRE(result->entries.size() == 2);
    CHECK(result->entries[0].quote.bidPrice.toString() == "1");
    CHECK(result->entries[1].quote.bidPrice.toString() == "5");
}

TEST_CASE("malformed input and nonrepresentable values fail atomically") {
    CHECK_FALSE(kraken::decodeTicker("{"));
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"ticker","channel":"ticker","type":"update","data":[]})"));
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"ticker","type":"update","data":[]})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"ETH/USD","bid":1,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"}]})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1,"bid":1,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"}]})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1e-128,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"}]})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":36028797018963968,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"}]})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":-1,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"}]})"));
    CHECK_FALSE(kraken::decodeTicker(std::string(65537, ' ')));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1,"ask":2,"bid_qty":3,"ask_qty":4,"timestamp":"a"},{"symbol":"BTC/USD","bid":1,"ask":2,"bid_qty":3,"timestamp":"b"}]})"));
}

TEST_CASE("decimal normalization preserves representable exponent extremes") {
    auto result = kraken::decodeTicker(
        R"({"channel":"ticker","type":"snapshot","data":[{"symbol":"BTC/USD","bid":100e127,"ask":1e-127,"bid_qty":36028797018963967,"ask_qty":0,"timestamp":"a"}]})");
    REQUIRE(result);
    REQUIRE(result->entries.size() == 1);
    CHECK(result->entries[0].quote.bidPrice.significand() == 100);
    CHECK(result->entries[0].quote.bidPrice.exponent() == 127);
    CHECK(result->entries[0].quote.askPrice.significand() == 1);
    CHECK(result->entries[0].quote.askPrice.exponent() == -127);
    CHECK(result->entries[0].quote.bidSize.significand() == 36028797018963967);
    CHECK(result->entries[0].quote.askSize.significand() == 0);
    CHECK(result->entries[0].quote.askSize.kind() == core::DecimalKind::FINITE);
}

TEST_CASE("large integers and escaped field names retain their JSON meaning") {
    auto large = kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":100000000000000000000,"ask":1,"bid_qty":1,"ask_qty":1,"timestamp":"x"}]})");
    REQUIRE(large);
    CHECK(large->entries[0].quote.bidPrice.toString() == "1E20");
    auto escaped = kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","d\u0061ta":[{"ask_qty":1,"timestamp":"x","bid_qty":1,"ask":1,"b\u0069d":1e0,"symbol":"BTC/USD"}]})");
    REQUIRE(escaped);
    CHECK(escaped->entries[0].quote.bidPrice.toString() == "1");
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1,"ask":1,"bid_qty":1,"ask_qty":1,"timestamp":"x"}],"unknown":[1,]})"));
    auto twenty = kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":20000000000000000000,"ask":1,"bid_qty":1,"ask_qty":1,"timestamp":"x"}]})");
    REQUIRE(twenty);
    CHECK(twenty->entries[0].quote.bidPrice.toString() == "2E19");
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"heartbeat","unknown":100000000000000000000garbage})"));
    CHECK_FALSE(kraken::decodeTicker(
        R"({"channel":"ticker","type":"update","data":[{"symbol":"BTC/USD","bid":1,"ask":1,"bid_qty":1,"ask_qty":1,"timestamp":"x"}],"unknown":100000000000000000000garbage})"));
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"heartbeat","unknown":01})"));
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"heartbeat","unknown":1e+})"));
    CHECK_FALSE(kraken::decodeTicker(R"({"channel":"heartbeat","unknown":1.})"));
    auto largeUnknown = kraken::decodeTicker(R"({"channel":"heartbeat","unknown":1e99999})");
    REQUIRE(largeUnknown);
    CHECK(largeUnknown->kind == kraken::MessageKind::HEARTBEAT);
    CHECK(kraken::decodeTicker("{\"channel\":\"heartbeat\",\"unknown\":\"escaped \\\"1e+\\\" text\"}"));
}
