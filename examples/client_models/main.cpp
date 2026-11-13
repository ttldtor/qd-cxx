// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <qd/api/order_book_model.hpp>
#include <qd/api/time_series_event_model.hpp>
#include <qd/events/candle.hpp>
#include <qd/events/candle_symbol.hpp>
#include <qd/events/time_helpers.hpp>
#include <qd/glossary/cfi.hpp>
#include <qd/ipf/instrument_profile_collector.hpp>
#include <qd/ipf/option_chains.hpp>

#include <array>
#include <iostream>
#include <string_view>
#include <utility>

namespace q = org::ttldtor::qdcxx;

template <class Result> bool admitted(const Result &result, std::string_view operation) {
    if (result)
        return true;

    std::cerr << operation << " refused: code=" << static_cast<int>(result.error()) << '\n';

    return false;
}

int help(bool russian) {
    if (russian)
        std::cout << "Использование: qdcxx-client-models --run\n"
                     "Локальные учебные Candle, стакан и IPF-профиль: без сети и чтения файлов.\n"
                     "--help и --help-ru показывают справку. Коды: 0 — успех, 1 — отказ, 2 — аргументы.\n";
    else
        std::cout << "Usage: qdcxx-client-models --run\n"
                     "Local example Candle, order book and IPF profile; no network or file access.\n"
                     "--help and --help-ru show help. Exit 0: success, 1: refusal, 2: invalid arguments.\n";

    return std::cout ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
        return help(false);

    if (argc == 2 && std::string_view(argv[1]) == "--help-ru")
        return help(true);

    if (argc != 2 || std::string_view(argv[1]) != "--run") {
        std::cerr << "Invalid arguments; use --help.\n";

        return 2;
    }

    try {
        const auto symbol = q::events::CandleSymbol::valueOf(u"AAPL{=d}");
        const auto logical = symbol.getSymbol();

        if (!logical)
            return 1;

        q::api::TimeSeriesEventModel<q::events::Candle> candles;

        if (!admitted(candles.setSymbol(*logical), "Candle symbol"))
            return 1;

        const auto revision = candles.setFromTime(0);

        if (!admitted(revision, "Candle range"))
            return 1;

        q::events::Candle candle;
        candle.eventSymbol = symbol;
        candle.index = q::events::withTimeSequenceTime(1, 1000);

        if (!admitted(candles.processEvents(*revision, std::span(&candle, 1)), "Candle event"))
            return 1;

        const auto cut = candles.getSnapshot();

        if (!admitted(cut, "Candle snapshot"))
            return 1;

        q::api::OrderBookModel book;
        q::events::Order order;
        order.symbol = u"AAPL";
        order.index = 1;
        order.price = {100 * 256 + 128};
        order.size = {10 * 256 + 128};
        order.flags = std::to_underlying(q::events::Scope::ORDER) | (std::to_underlying(q::events::Side::BUY) << 2);

        if (!admitted(book.processEvents(u"AAPL", 0, std::span(&order, 1)), "Order"))
            return 1;

        const auto bookCut = book.getSnapshot(u"AAPL");

        if (!admitted(bookCut, "Order-book snapshot"))
            return 1;

        q::ipf::InstrumentProfile profile;

        if (!admitted(profile.setType(u"STOCK"), "Profile type") ||
            !admitted(profile.setSymbol(u"AAPL"), "Profile symbol"))
            return 1;

        auto collector = q::ipf::InstrumentProfileCollector::create();
        const auto applied = collector->updateInstrumentProfiles(std::span(&profile, 1));
        const auto profiles = collector->snapshot();

        if (!admitted(applied, "Profile update") || !admitted(profiles, "Profile snapshot"))
            return 1;

        q::ipf::OptionChainsBuilder chains;
        const auto included = chains.addProfile(profile);

        if (!admitted(included, "Option chains"))
            return 1;

        const auto cfi = q::glossary::Cfi::valueOf(u"ESXXXX");
        std::cout << "Local sample: candles=" << cut->events.size() << " bids=" << bookCut->buyOrders.size()
                  << " profiles=" << (*profiles)->profiles().size() << " option-chains=" << chains.getChains().size()
                  << " stock-added-to-chains=" << *included << " cfi=" << cfi.toString() << '\n';
        collector->close();

        return std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << "Model refusal: " << error.what() << '\n';

        return 1;
    }
}
