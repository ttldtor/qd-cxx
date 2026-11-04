// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <chrono>
#include <cstdio>
#include <fstream>
#include <qd/ipf/instrument_profile_connection.hpp>

namespace {
constexpr const char *SOURCE = "ipf-connection-consumer.ipf";
}

/** Use only installed public API, a finite first-snapshot wait and explicit creator shutdown.
 * @return Zero after owning snapshot delivery and retirement; static phase code on failure.
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

    if (!connection->start()) {
        return 4;
    }

    auto completed = connection->awaitCompleted(std::chrono::steady_clock::now() + std::chrono::seconds(4));
    connection->close();

    if (!runtime->closeAndAwaitTermination()) {
        return 5;
    }

    if (completed != i::IpfCompletionWaitStatus::COMPLETED || !connection->observation().retired) {
        return 6;
    }

    auto snapshot = model->snapshot();

    if (!snapshot || (*snapshot)->profiles().size() != 1) {
        return 7;
    }

    return (*snapshot)->profiles().front()->profile.getSymbol() == u"IBM" ? 0 : 8;
}
