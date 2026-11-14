// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_reader.hpp"
#include "qd/ipf/option_chains.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

/** Print input-free help in the selected language.
 * @param russian True for Russian output.
 * @return Zero on successful output, one on output failure.
 */
int printHelp(bool russian) {
    if (russian)
        std::cout << "Использование: qdcxx-ipf-option-chain ФАЙЛ БАЗОВЫЙ_АКТИВ\n"
                     "Читает локальный IPF в UTF-8 с ##COMPLETE и выводит серии опционов и страйки.\n"
                     "Базовый актив задаётся точным ASCII-символом. Справка не читает файл.\n"
                     "Коды: 0 успех, 1 ошибка чтения, построения или вывода, 2 неверные аргументы.\n";
    else
        std::cout << "Usage: qdcxx-ipf-option-chain FILE UNDERLYING\n"
                     "Read a local UTF-8 IPF file with ##COMPLETE and print option series and strikes.\n"
                     "Use an exact ASCII underlying symbol. Help does not open an input file.\n"
                     "Exit codes: 0 success, 1 read, build or output failure, 2 invalid arguments.\n";

    return std::cout ? 0 : 1;
}

} // namespace

/** Read instrument profiles and show an owning option-chain view.
 * @param argc Argument count.
 * @param argv Borrowed command arguments.
 * @return Documented help, data or failure status.
 */
int main(int argc, char **argv) {
    namespace ipf = org::ttldtor::qdcxx::ipf;

    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
        return printHelp(false);

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru")
        return printHelp(true);

    if (argc != 3) {
        std::cerr << "Expected a file path and an underlying symbol.\n";

        return 2;
    }

    const std::string_view symbol(argv[2]);
    constexpr std::size_t MAX_SYMBOL_BYTES = 128;
    constexpr unsigned char ASCII_DELETE = 127;

    if (symbol.empty() || symbol.size() > MAX_SYMBOL_BYTES) {
        std::cerr << "Invalid underlying symbol length.\n";

        return 2;
    }

    std::u16string underlying;

    for (const unsigned char unit : symbol) {
        if (unit <= ' ' || unit >= ASCII_DELETE) {
            std::cerr << "Expected an exact ASCII underlying symbol.\n";

            return 2;
        }

        underlying.push_back(static_cast<char16_t>(unit));
    }

    try {
        ipf::IpfReadOptions options;
        options.completion = ipf::IpfCompletionPolicy::REQUIRE;
        const auto profiles = ipf::readInstrumentProfilesFile(argv[1], options);

        if (!profiles.complete || profiles.error) {
            std::cerr << "The IPF file did not yield a complete profile list.\n";

            return 1;
        }

        const auto builder = ipf::OptionChainsBuilder::build(profiles.profiles);

        if (!builder) {
            std::cerr << "The option chain could not be built.\n";

            return 1;
        }

        const auto &chains = builder->getChains();
        const auto chain = chains.find(underlying);

        if (chain == chains.end()) {
            std::cerr << "No option chain for the requested underlying.\n";

            return 1;
        }

        const auto &series = chain->second->getSeries();
        std::cout << symbol << " series=" << series.size() << '\n';

        for (const auto &expiry : series) {
            std::cout << "calls=" << expiry.getCalls().size() << " puts=" << expiry.getPuts().size() << " strikes:";

            for (const double strike : expiry.getStrikes())
                std::cout << ' ' << strike;

            std::cout << '\n';
        }

        return std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
