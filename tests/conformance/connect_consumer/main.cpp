// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal installed-package acceptance runner: no external peer, working arguments or dependency discovery.
#include "connect_peer.hpp"
#include "example.hpp"
#include "output.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::events;
namespace t = root::transport;
namespace x = root::examples;
namespace a = root::connect;
namespace s = root::connect_tests;

/** Require a real production-consumer outcome in optimized builds.
 * @param condition Observed required condition.
 */
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("installed consumer validation failed");
    }
}

/** Exercise one complete installed standard schema, real TCP source and prepared mapping.
 * @param bootstrap Explicit main-thread SDK lifetime, outliving this scope's peer thread/runtime.
 * @param profile Select complete Profile rather than Quote.
 */
void standard(t::RuntimeBootstrap &bootstrap, bool profile) {
    constexpr c::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto defaults = profile ? e::defaultProfileSchema(LIMITS) : e::defaultQuoteSchema(LIMITS);
    require(defaults.has_value());
    auto schema = c::LocalSchema::create(defaults->records(), LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    require(schema.has_value());
    std::vector<c::FieldValue> values;

    for (const auto &field : schema->records()[0].fields) {
        const auto type = static_cast<c::SerialFieldType>(field.serialTypeId);
        c::FieldValue value{type, 0};

        if (type == c::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"installed";
        }

        values.push_back(std::move(value));
    }

    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<s::Record> records{{u"IBM", values}};
    s::Peer peer(*schema, records, symbols);
    std::ostringstream output;
    auto consumer = std::make_shared<a::OutputConsumer>(output, *schema,
                                                        profile ? a::OutputProfile::PROFILE : a::OutputProfile::QUOTE);
    consumer->begin();
    std::optional<root::client::ClientHandle> control;
    const auto result =
        x::runStream(bootstrap, *schema, {"127.0.0.1", peer.port(), 0, symbols, std::chrono::milliseconds(2000), {}, 1},
                     consumer, control);
    require(a::exitCode(result, true) == 0);
    require(result.local->counts.completedRecords == 1 && result.targetReached);
    require(control && control->terminal());
    require(peer.adds() == 1 && !peer.failed());
    require(output.str().find(profile ? "profile\t1\t00490042004D" : "quote\t1\t00490042004D") != std::string::npos);
}
} // namespace

/** Execute the production-only installed target closure and documented generic example.
 * @return Zero only after generic and both full standard prepared routes retire successfully.
 */
int main() {
    try {
        auto bootstrap = t::RuntimeBootstrap::create();
        require(bootstrap.has_value());
        {
            const c::LocalRecord record{0, u"Consumer", false, {{8}}, {{u"Value", 0}}};
            auto schema =
                c::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 32, 64}, c::LocalProfile::BINARY_FIELDS_V1);
            require(schema.has_value());
            const std::vector<std::u16string> symbols{u"ALPHA"};
            const std::vector<s::Record> records{{u"ALPHA", {{c::SerialFieldType::COMPACT_INT, 42}}}};
            s::Peer peer(*schema, records, symbols);
            std::ostringstream output;
            const auto result = x::runSubscriptionExample(*bootstrap, *schema, peer.port(), output);
            require(a::exitCode(result, true) == 0);
            require(result.local->counts.completedRecords == 1);
            require(output.str() == "local-id=0 sender-id=17 fields=1\n");
            require(peer.adds() == 1 && !peer.failed());
        }

        standard(*bootstrap, false);
        standard(*bootstrap, true);
        std::cout << "installed generic example, full Quote and full Profile: passed\n";

        return 0;
    } catch (const std::exception &) {
        std::cerr << "installed subscription consumer failed\n";

        return 1;
    }
}
