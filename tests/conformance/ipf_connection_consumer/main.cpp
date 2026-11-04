// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <qd/ipf/instrument_profile_connection.hpp>

namespace {
constexpr const char *SOURCE = "ipf-connection-consumer.ipf";
}

/** Exercise only installed public API and a caller-owned regular source.
 * @return Zero after background delivery and creator retirement; static nonzero phase on failure.
 */
int main() {
    namespace i = org::ttldtor::qdcxx::ipf;

    struct File {
        /** Remove only the exact fixture created in this consumer's owned working directory. */
        ~File() {
            std::remove(SOURCE);
        }
    } file;

    {
        std::ofstream output(SOURCE, std::ios::binary);
        output << "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n##COMPLETE\n";

        if (!output) {
            return 1;
        }
    }
    auto prepared = i::InstrumentProfileConnectionRuntime::create();

    if (!prepared) {
        return 2;
    }

    auto runtime = std::move(*prepared);
    std::shared_ptr<i::InstrumentProfileCollector> model = i::InstrumentProfileCollector::create();
    auto created = runtime->createConnection(SOURCE, model);

    if (!created) {
        return 3;
    }

    auto connection = std::move(*created);
    std::mutex mutex;
    std::condition_variable condition;
    bool complete = false;
    auto listener = connection->addStateChangeListener([&](auto, auto state) {
        if (state == i::IpfConnectionState::COMPLETED) {
            {
                std::lock_guard lock(mutex);
                complete = true;
            }
            condition.notify_one();
            connection->close();
        }
    });

    if (!listener) {
        return 4;
    }

    if (!connection->start()) {
        return 5;
    }

    bool observed = false;
    {
        std::unique_lock lock(mutex);
        observed = condition.wait_for(lock, std::chrono::seconds(4), [&] {
            return complete;
        });
    }
    connection->close();

    if (!runtime->closeAndAwaitTermination()) {
        return 6;
    }

    if (!observed || !connection->observation().retired) {
        return 7;
    }

    auto snapshot = model->snapshot();

    if (!snapshot || (*snapshot)->profiles().size() != 1) {
        return 8;
    }

    return (*snapshot)->profiles().front()->profile.getSymbol() == u"IBM" ? 0 : 9;
}
