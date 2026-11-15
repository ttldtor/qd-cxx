// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/api/feed.hpp"
#include "qd/ipf/instrument_profile_reader.hpp"
#include "qd/ipf/option_chains.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout << "Usage: qdcxx-option-quotes FILE UNDERLYING CENTER [ADDRESS | --list]\n"
                     "Select up to four strikes around CENTER in the first IPF option series,\n"
                     "then subscribe to calls and puts for 10 seconds. ADDRESS defaults to\n"
                     "demo.dxfeed.com:7300. --list prints selected symbols without connecting.\n"
                     "FILE is a complete local UTF-8 IPF (64 MiB, 100,000 profiles max).\n"
                     "Symbols must be printable ASCII. Help opens no file or connection.\n"
                     "Exit 0: success, 1: failure or no quotes, 2: invalid arguments.\n";
        std::cout.flush();

        return std::cout ? 0 : 1;
    }

    if (argc < 4 || argc > 5) {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    const std::string_view underlying(argv[2]);
    const std::string_view centerText(argv[3]);
    double center = 0;
    const auto parsed = std::from_chars(centerText.data(), centerText.data() + centerText.size(), center);

    if (underlying.empty() || underlying.size() > 128 || parsed.ec != std::errc{} ||
        parsed.ptr != centerText.data() + centerText.size() || !std::isfinite(center)) {
        std::cerr << "Invalid underlying or center.\n";

        return 2;
    }

    std::u16string chainSymbol;

    for (unsigned char c : underlying) {
        if (c <= ' ' || c >= 127) {
            std::cerr << "Underlying must be printable ASCII.\n";

            return 2;
        }

        chainSymbol.push_back(static_cast<char16_t>(c));
    }

    const std::string address = argc == 5 ? argv[4] : "demo.dxfeed.com:7300";

    if (address.empty()) {
        std::cerr << "Address must not be empty.\n";

        return 2;
    }

    const bool list = address == "--list";

    try {
        q::ipf::IpfReadOptions options;
        options.completion = q::ipf::IpfCompletionPolicy::REQUIRE;
        const auto profiles = q::ipf::readInstrumentProfilesFile(argv[1], options);

        if (!profiles.complete || profiles.error) {
            std::cerr << "Cannot read a complete IPF file within the sample limits.\n";

            return 1;
        }

        const auto builder = q::ipf::OptionChainsBuilder::build(profiles.profiles);

        if (!builder) {
            std::cerr << "Cannot build option chains.\n";

            return 1;
        }

        const auto &chains = builder->getChains();
        const auto chain = chains.find(chainSymbol);

        if (chain == chains.end() || chain->second->getSeries().empty()) {
            std::cerr << "No option series for the underlying.\n";

            return 1;
        }

        const auto &series = chain->second->getSeries().front();
        const auto strikes = series.getNStrikesAround(4, center);

        if (!strikes || strikes->empty()) {
            std::cerr << "No option strikes found.\n";

            return 1;
        }

        std::vector<std::string> symbols;

        for (const double strike : *strikes) {
            for (const auto *optionsAtStrike : {&series.getCalls(), &series.getPuts()}) {
                const auto option = optionsAtStrike->find(strike);

                if (option == optionsAtStrike->end())
                    continue;

                const auto symbol = option->second->getSymbol();

                if (symbol.empty() || symbol.size() > 128) {
                    std::cerr << "Invalid option symbol.\n";

                    return 1;
                }

                std::string text;

                for (char16_t c : symbol) {
                    if (c <= u' ' || c >= 127) {
                        std::cerr << "Option symbols must be printable ASCII.\n";

                        return 1;
                    }

                    text.push_back(static_cast<char>(c));
                }

                symbols.push_back(std::move(text));
            }
        }

        if (symbols.empty()) {
            std::cerr << "No option symbols found.\n";

            return 1;
        }

        if (list) {
            for (const auto &symbol : symbols)
                std::cout << symbol << '\n';

            std::cout.flush();

            return std::cout ? 0 : 1;
        }

        std::atomic<std::size_t> received{0};
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addEventListener([&](std::span<const q::events::Quote> quotes) {
            for (const auto &quote : quotes) {
                std::string symbol;

                for (char16_t c : quote.symbol)
                    symbol.push_back(c < 127 ? static_cast<char>(c) : '?');

                std::cout << symbol << " bid=" << quote.getBidPrice() << " ask=" << quote.getAskPrice() << '\n'
                          << std::flush;
                ++received;
            }
        });

        for (const auto &symbol : symbols)
            subscription->addSymbols(symbol);

        endpoint->connect(address);
        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 10s));
        const auto state = endpoint->getState();
        const auto beforeClose = endpoint->getDiagnostic();
        const auto result = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();
        std::cout.flush();

        if (result.diagnostic.code != q::api::ErrorCode::NONE)
            std::cerr << "Endpoint diagnostic: " << result.diagnostic.stage << '\n';

        if (!result.drained || result.deadlineExpired || result.hadGap || state != q::api::EndpointState::CONNECTED ||
            beforeClose.code != q::api::ErrorCode::NONE ||
            subscription->getDiagnostic().code != q::api::ErrorCode::NONE || received == 0 || !std::cout) {
            std::cerr << "Quote subscription failed or received no data.\n";

            return 1;
        }

        return 0;
    } catch (const q::api::Error &error) {
        std::cerr << error.what() << '\n';

        return error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT ? 2 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
