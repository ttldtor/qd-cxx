// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "configuration.hpp"

#include "output.hpp"
#include "recovery_configuration.hpp"

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_RECOVERY)
#    include "recovery_application.hpp"
#    include "recovery_preparation.hpp"
#endif

#include <csignal>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace {
namespace app = org::ttldtor::qdcxx::connect;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace t = org::ttldtor::qdcxx::transport;
namespace x = org::ttldtor::qdcxx::examples;
volatile std::sig_atomic_t interrupted = 0; ///< Only the application signal handler writes this flag.

/** Recognize the explicit route only in option positions; an option-looking symbol remains a value.
 * @param arguments Native option/value pairs, borrowed only during this decision.
 * @return True when --mode is an option; its actual value/contradictions belong to the recovery parser.
 */
bool selectsRecoveryMode(std::span<const std::string_view> arguments) noexcept {
    for (std::size_t index = 0; index < arguments.size(); index += 2) {
        if (arguments[index] == "--mode") {
            return true;
        }
    }

    return false;
}

/** Report only explicit linked CLI capabilities, without discovering dependencies or entering an SDK. */
constexpr app::CompiledRecoveryFeatures recoveryFeatures() noexcept {
    app::CompiledRecoveryFeatures features;
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_RECOVERY)
    features.recovery = true;
#endif
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_TLS)
    features.tls = true;
#endif
#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_JSON)
    features.json = true;
#endif

    return features;
}

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
int runArguments(int argc, char **argv) {
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

        if (selectsRecoveryMode(arguments)) {
            auto recovery = app::parseRecoveryArguments(arguments, recoveryFeatures());

            if (!recovery) {
                std::cerr << "qdcxx-connect: " << recovery.error() << '\n';

                return 2;
            }

#if defined(ORG_TTLDTOR_QDCXX_CONNECT_HAS_RECOVERY)
            auto prepared = app::prepareRecovery(*recovery);

            if (!prepared) {
                std::cerr << "qdcxx-connect: " << prepared.error() << '\n';

                return 1;
            }

            SignalGuard signals;

            if (!signals.ready()) {
                throw std::runtime_error("signal setup failed");
            }

            return app::runRecoveryApplication(*recovery, *prepared, &interrupted, std::cout, std::cerr);
#else

            return 2;
#endif
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

#ifdef _WIN32
/** Preserve native Windows paths through an explicit UTF-8 argument boundary.
 * @param argc Native UTF-16 argument count, including the executable.
 * @param argv Process-owned UTF-16 values; help bypasses conversion/allocation.
 * @return Existing CLI categories after strict bounded conversion; no codepage is changed.
 */
int wmain(int argc, wchar_t **argv) {
    constexpr int MAXIMUM_ARGUMENTS = 256; ///< Exceeds every accepted finite option/selector combination.
    constexpr std::size_t MAXIMUM_ARGUMENT_UNITS = 32768; ///< Independent native argument-copy ceiling.

    if (argc == 1) {
        return runArguments(1, nullptr);
    }

    if (argc == 2 && (std::wstring_view(argv[1]) == L"--help" || std::wstring_view(argv[1]) == L"--help-ru")) {
        char english[] = "--help";
        char russian[] = "--help-ru";
        char *arguments[]{nullptr, std::wstring_view(argv[1]) == L"--help-ru" ? russian : english};

        return runArguments(2, arguments);
    }

    bool recovery = false;

    for (int index = 1; index < argc && index < MAXIMUM_ARGUMENTS; index += 2) {
        recovery = recovery || std::wstring_view(argv[index]) == L"--mode";
    }

    try {
        if (argc < 1 || argc > MAXIMUM_ARGUMENTS) {
            std::cerr << "qdcxx-connect: native argument count exceeds the profile\n";

            return recovery ? 2 : 1;
        }

        std::vector<std::string> owned;
        std::vector<char *> pointers;
        owned.reserve(static_cast<std::size_t>(argc));
        pointers.reserve(static_cast<std::size_t>(argc));
        owned.emplace_back("qdcxx-connect");

        for (int index = 1; index < argc; ++index) {
            std::size_t units = 0;

            while (units <= MAXIMUM_ARGUMENT_UNITS && argv[index][units] != L'\0') {
                ++units;
            }

            if (units > MAXIMUM_ARGUMENT_UNITS) {
                std::cerr << "qdcxx-connect: native argument exceeds the profile\n";

                return recovery ? 2 : 1;
            }

            if (units == 0) {
                owned.emplace_back();
                continue;
            }

            const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], static_cast<int>(units),
                                                  nullptr, 0, nullptr, nullptr);

            if (size == 0) {
                std::cerr << "qdcxx-connect: native argument encoding failed\n";

                return recovery ? 2 : 1;
            }

            std::string argument(static_cast<std::size_t>(size), '\0');

            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[index], static_cast<int>(units),
                                    argument.data(), size, nullptr, nullptr) != size) {
                std::cerr << "qdcxx-connect: native argument encoding failed\n";

                return recovery ? 2 : 1;
            }

            owned.push_back(std::move(argument));
        }

        for (auto &argument : owned) {
            pointers.push_back(argument.data());
        }

        return runArguments(argc, pointers.data());
    } catch (...) {
        std::cerr << "qdcxx-connect: native argument preparation failed\n";

        return 1;
    }
}
#else
/** Enter the native POSIX byte-argument boundary without changing process locale or settings.
 * @param argc Argument count including the executable.
 * @param argv Process-owned argument values.
 * @return Documented CLI process category.
 */
int main(int argc, char **argv) {
    return runArguments(argc, argv);
}
#endif
