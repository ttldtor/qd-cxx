// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "configuration.hpp"

#include "output.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
namespace app = org::ttldtor::qdcxx::connect;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace t = org::ttldtor::qdcxx::transport;
namespace x = org::ttldtor::qdcxx::examples;
volatile std::sig_atomic_t interrupted = 0; ///< Only the application signal handler writes this flag.

/** Record an interrupt without allocation, I/O, locks or runtime calls.
 * @param signal Native signal identity, deliberately not interpreted as peer evidence.
 */
void onSignal(int signal) {
    (void)signal;
    interrupted = 1;
}

/** Application-only temporary signal dispositions, restored before process return. */
class SignalGuard {
    using Handler = void (*)(int);
    Handler interrupt_ = SIG_ERR;
    Handler terminate_ = SIG_ERR;
#ifndef _WIN32
    Handler pipe_ = SIG_ERR;
#endif

    public:
    /** Acquire app-owned interrupt handling and POSIX broken-pipe error reporting. */
    SignalGuard() {
        interrupt_ = std::signal(SIGINT, onSignal);
        terminate_ = std::signal(SIGTERM, onSignal);
#ifndef _WIN32
        pipe_ = std::signal(SIGPIPE, SIG_IGN);
#endif
    }

    /** Restore only dispositions successfully acquired by this application boundary. */
    ~SignalGuard() {
        if (interrupt_ != SIG_ERR) {
            std::signal(SIGINT, interrupt_);
        }

        if (terminate_ != SIG_ERR) {
            std::signal(SIGTERM, terminate_);
        }

#ifndef _WIN32

        if (pipe_ != SIG_ERR) {
            std::signal(SIGPIPE, pipe_);
        }

#endif
    }

    /** Inspect whether all requested application dispositions were installed.
     * @return True only for complete installation; destruction still restores any successful prefix.
     */
    bool ready() const noexcept {
        return interrupt_ != SIG_ERR && terminate_ != SIG_ERR
#ifndef _WIN32
               && pipe_ != SIG_ERR
#endif
            ;
    }
};

/** Build the complete selected schema under the binary live profile without altering provenance later.
 * @param profile Explicit complete built-in source.
 * @return Owned validated local schema, or static preparation exception before activation.
 */
c::LocalSchema schema(app::SchemaProfile profile) {
    constexpr c::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto defaults =
        profile == app::SchemaProfile::QUOTE ? e::defaultQuoteSchema(LIMITS) : e::defaultProfileSchema(LIMITS);

    if (!defaults) {
        throw std::runtime_error("built-in schema preparation failed");
    }

    auto result = c::LocalSchema::create(defaults->records(), LIMITS, c::LocalProfile::BINARY_FIELDS_V1);

    if (!result) {
        throw std::runtime_error("binary schema preparation failed");
    }

    return std::move(*result);
}
} // namespace

/** Native application entry: help precedes parsing, schemas, bootstrap, signals and network work.
 * @param argc Native argument cardinality including the executable.
 * @param argv Native NUL-terminated argument values, borrowed through this invocation.
 * @return Documented native CLI status; failed stdout/stderr publication never reports success.
 */
int main(int argc, char **argv) {
    if (argc == 1 ||
        (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "--help-ru"))) {
        std::cout << app::help(argc == 2 && std::string_view(argv[1]) == "--help-ru");
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    try {
        std::vector<std::string_view> arguments;

        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }

        auto config = app::parseArguments(arguments);

        if (!config) {
            std::cerr << "qdcxx-connect: " << config.error() << '\n';

            return 1;
        }

        auto local = schema(config->schema);
        auto endpoint = app::effectiveEndpoint(*config, local);

        if (!endpoint) {
            std::cerr << "qdcxx-connect: configuration failure stage=" << endpoint.error().stage << '\n';

            return 1;
        }

        SignalGuard signals;

        if (!signals.ready()) {
            throw std::runtime_error("signal setup failed");
        }

        auto output = std::make_shared<app::OutputConsumer>(std::cout, local, config->output);
        output->begin();
        auto bootstrap = t::RuntimeBootstrap::create();

        if (!bootstrap) {
            std::cerr << "qdcxx-connect: bootstrap failure stage=" << bootstrap.error().stage << '\n';

            return 1;
        }

        std::optional<org::ttldtor::qdcxx::client::ClientHandle> control;
        const x::StreamOptions options{config->host,           config->port,     0,
                                       config->symbols,        config->duration, config->removeAfter,
                                       config->maximumRecords, &interrupted};
        const auto result = x::runStream(*bootstrap, *endpoint, options, output, control);
        bool sinkFailed = false;

        try {
            output->flush();
        } catch (const std::exception &) {
            sinkFailed = true;
        }

        app::writeResult(std::cerr, result);

        if (sinkFailed) {
            std::cerr << "output sink-failure=1\n";

            return 1;
        }

        return app::exitCode(result, config->maximumRecords.has_value());
    } catch (const std::exception &) {
        // Arbitrary exception text can contain application data; keep diagnostics static.
        std::cerr << "qdcxx-connect: preparation, mapping or output failure\n";

        return 1;
    }
}
