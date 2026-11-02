// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: PROGRAM RUN are explicit; only owned local peers/children/files are touched.
#include "connect_peer.hpp"
#include "connect_process_support.hpp"

#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/transport/tcp_runtime.hpp"

#ifdef _WIN32
#    include <windows.h>
#else
#    include <cerrno>
#    include <csignal>
#    include <fcntl.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace c = root::core;
namespace e = root::events;
namespace t = root::transport;
namespace s = root::connect_tests;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

using s::process::Child;
using s::process::read;
using s::process::require;
using s::process::rows;

/** Build complete standard binary schema and finite synthetic values.
 * @param profile Select complete Profile rather than Quote.
 * @return Owning validated immutable schema.
 */
c::LocalSchema schema(bool profile) {
    constexpr c::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto defaults = profile ? e::defaultProfileSchema(LIMITS) : e::defaultQuoteSchema(LIMITS);
    require(defaults.has_value(), "full standard schema");
    auto result = c::LocalSchema::create(defaults->records(), LIMITS, c::LocalProfile::BINARY_FIELDS_V1);
    require(result.has_value(), "binary standard schema");

    return std::move(*result);
}

/** Fill every local standard field, without reducing the actual schema.
 * @param local Complete explicit standard schema.
 * @return Exact finite typed values.
 */
std::vector<c::FieldValue> fields(const c::LocalSchema &local) {
    std::vector<c::FieldValue> result;

    for (const auto &field : local.records()[0].fields) {
        const auto type = static_cast<c::SerialFieldType>(field.serialTypeId);
        c::FieldValue value{type, 0};

        if (type == c::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"CLI";
        }

        result.push_back(std::move(value));
    }

    return result;
}
} // namespace

/** Run real CLI help, loopback, peer-loss, broken-pipe and owned-process cleanup scenarios.
 * @param argc Must contain exact executable and private output-directory arguments.
 * @param argv Test invocation values, borrowed through main.
 * @return Zero only for all actual child/peer observations; diagnostics name the failing harness condition.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "PROGRAM RUN required");
        const std::filesystem::path program = std::filesystem::absolute(argv[1]);
        const std::filesystem::path run = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(run);
        {
            Child noargs(program, {}, run / "noargs.out", run / "noargs.err");
            require(noargs.wait() == 0, "noargs with blocked stdin");
            Child help(program, {"--help"}, run / "help.out", run / "help.err");
            require(help.wait() == 0, "help with blocked stdin");
            require(read(run / "noargs.out") == read(run / "help.out"), "help equality");
            require(read(run / "noargs.err").empty() && read(run / "help.err").empty(), "help empty stderr");
            Child failedHelp(program, {"--help"}, run / "unused.out", run / "help-failed.err", true);
            failedHelp.closeOutput(false);
            require(failedHelp.wait() != 0, "closed help pipe cannot succeed");
        }

        auto bootstrap = t::RuntimeBootstrap::create();
        require(bootstrap.has_value(), "parent main-thread bootstrap before peer threads");

        for (const bool profile : {false, true}) {
            auto local = schema(profile);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}, {u"OTHER", fields(local)}};
            s::Peer peer(local, records, selected);
            const std::string name = profile ? "profile" : "quote";
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--schema-profile", name, "--output", name, "--max-records", "1", "--duration-ms", "2000"},
                        run / (name + ".out"), run / (name + ".err"));
            require(child.wait() == 0, "actual standard CLI target exit");
            require(rows(read(run / (name + ".out")), name + "\t") == 1, "exact selected CLI standard record");
            require(peer.adds() == 1 && !peer.failed(), "actual peer add observation");
            require(read(run / (name + ".err")).find("target-reached=1") != std::string::npos, "data target evidence");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "600", "--remove-after-ms", "300"},
                        run / "remove.out", run / "remove.err");
            const auto code = child.wait();
            const auto errors = read(run / "remove.err");
#ifdef _WIN32
            const bool ambiguity =
                errors.find("primary=10 primary-stage=socket-read primary-native=1236") != std::string::npos;
            require(code == (ambiguity ? 1 : 0), "Windows removal close status preserves native failure");
#else
            require(code == 0, "removal intentional cutoff exit");
#endif
            require(peer.adds() == 1 && peer.removes() == 1 && !peer.failed(), "actual peer add/remove tuples");
            require(rows(read(run / "remove.out"), "record\t") == 1, "no post-remove callback");
            require(errors.find("remove-applied=1 remove-composed=1") != std::string::npos, "separate removal stages");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected, 8192, s::PeerMode::PARTIAL_END);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "2000"},
                        run / "partial.out", run / "partial.err");
            require(child.wait() == 1, "partial-frame peer loss fails");
            require(rows(read(run / "partial.out"), "record\t") == 1, "complete admitted prefix before peer loss");
            require(read(run / "partial.err").find("primary=14") != std::string::npos,
                    "protocol terminal remains visible");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "2000"},
                        run / "unused-sink.out", run / "sink.err", true);
            child.closeOutput(true);
            require(child.wait() == 1, "real callback broken-pipe output fails");
            require(peer.adds() == 1 && !peer.failed(), "sink fails after actual add and source activation");
            require(read(run / "sink.err").find("local outcome=2") != std::string::npos, "sink failure local terminal");
        }

        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            const std::vector<s::Record> records{{u"IBM", fields(local)}};
            s::Peer peer(local, records, selected);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM", "--output",
                         "quote", "--max-records", "1", "--duration-ms", "2000"},
                        run / "stderr-failed.out", run / "unused-stderr.err", false, true);
            require(child.wait() == 1, "failed diagnostic stderr cannot report successful target");
            require(rows(read(run / "stderr-failed.out"), "quote\t") == 1,
                    "stderr failure follows complete data output");
            require(peer.adds() == 1 && !peer.failed(), "stderr failure follows actual peer activation");
        }

#ifndef _WIN32
        {
            auto local = schema(false);
            const std::vector<std::u16string> selected{u"IBM"};
            s::Peer peer(local, {}, selected, 8192, s::PeerMode::SILENT);
            Child child(program,
                        {"--host", "127.0.0.1", "--port", std::to_string(peer.port()), "--symbol", "IBM",
                         "--duration-ms", "5000", "--max-records", "1"},
                        run / "unused-interrupt.out", run / "interrupt.err", true);
            child.closeOutput(true);
            const auto deadline = Clock::now() + 3s;

            while (peer.adds() == 0 && Clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }

            require(peer.adds() == 1, "interrupt after actual connection and peer add");
            child.interrupt();
            require(child.wait() == 130, "actual owned-child SIGINT cooperative exit");
            require(read(run / "interrupt.err").find("interrupted=1") != std::string::npos,
                    "actual interrupt diagnostic");
        }
#endif

        std::cout << "CLI blocked-stdin help, failed-help pipe, full Quote/Profile, add/remove, peer loss, callback "
                     "broken pipe; owned children retired\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
