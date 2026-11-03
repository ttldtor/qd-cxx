// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/order_source.hpp"

#include <array>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events {
namespace {
/** Immutable built-in lookup entry; literals have static storage. */
struct Builtin {
    /** Exact signed source ID. */
    std::int32_t id;
    /** Static exact UTF-16 literal. */
    std::u16string_view name;
    /** Direct publication and full-book capability bits. */
    unsigned flags;
};

constexpr unsigned PUB_ORDER = 0x0001;
constexpr unsigned PUB_ANALYTIC_ORDER = 0x0002;
constexpr unsigned PUB_OTC_MARKETS_ORDER = 0x0004;
constexpr unsigned PUB_SPREAD_ORDER = 0x0008;
constexpr unsigned FULL_ORDER_BOOK = 0x0010;
constexpr unsigned PUB_NUAM_ORDER = 0x0020;
constexpr unsigned PUB_ORDER_IMBALANCE = 0x0040;
constexpr auto BUILTINS = std::to_array<Builtin>({
    {1, u"COMPOSITE_BID", 0},
    {2, u"COMPOSITE_ASK", 0},
    {3, u"REGIONAL_BID", 0},
    {4, u"REGIONAL_ASK", 0},
    {5, u"AGGREGATE_BID", 0},
    {6, u"AGGREGATE_ASK", 0},
    {7, u"COMPOSITE", 0},
    {8, u"REGIONAL", 0},
    {9, u"AGGREGATE", 0},
    {0, u"DEFAULT",
     PUB_ORDER | PUB_ANALYTIC_ORDER | PUB_OTC_MARKETS_ORDER | PUB_SPREAD_ORDER | FULL_ORDER_BOOK | PUB_NUAM_ORDER |
         PUB_ORDER_IMBALANCE},
    {5133398, u"NTV", PUB_ORDER | FULL_ORDER_BOOK | PUB_ORDER_IMBALANCE},
    {7238774, u"ntv", PUB_ORDER},
    {5129816, u"NFX", PUB_ORDER},
    {1163087940, u"ESPD", PUB_ORDER},
    {1481524809, u"XNFI", PUB_ORDER},
    {4801349, u"ICE", PUB_ORDER},
    {4805445, u"ISE", PUB_ORDER | PUB_SPREAD_ORDER},
    {4474177, u"DEA", PUB_ORDER},
    {4474200, u"DEX", PUB_ORDER},
    {6579576, u"dex", PUB_ORDER},
    {4348248, u"BYX", PUB_ORDER},
    {4348504, u"BZX", PUB_ORDER},
    {6453880, u"bzx", PUB_ORDER},
    {1111577669, u"BATE", PUB_ORDER},
    {1128810840, u"CHIX", PUB_ORDER},
    {1128617304, u"CEUX", PUB_ORDER},
    {1113085010, u"BXTR", PUB_ORDER},
    {4805460, u"IST", PUB_ORDER},
    {1112093232, u"BI20", PUB_ORDER},
    {4276805, u"ABE", PUB_ORDER},
    {1178683730, u"FAIR", PUB_ORDER},
    {1196180056, u"GLBX", PUB_ORDER | PUB_ANALYTIC_ORDER},
    {1735156344, u"glbx", PUB_ORDER},
    {1163020627, u"ERIS", PUB_ORDER},
    {1480938834, u"XEUR", PUB_ORDER},
    {2019915122, u"xeur", PUB_ORDER},
    {4408901, u"CFE", PUB_ORDER},
    {1127370584, u"C2OX", PUB_ORDER},
    {1397573189, u"SMFE", PUB_ORDER},
    {1936549477, u"smfe", PUB_ORDER},
    {6907256, u"iex", PUB_ORDER},
    {1296387416, u"MEMX", PUB_ORDER},
    {1835363704, u"memx", PUB_ORDER},
    {1329808705, u"OCEA", PUB_ORDER},
    {1868784993, u"ocea", PUB_ORDER},
    {1885957739, u"pink", PUB_ORDER | PUB_OTC_MARKETS_ORDER},
    {1095910209, u"ARCA", PUB_ORDER | PUB_ORDER_IMBALANCE},
    {1634886497, u"arca", PUB_ORDER},
    {1128612952, u"CEDX", PUB_ORDER},
    {1667589240, u"cedx", PUB_ORDER},
    {4802371, u"IGC", PUB_ORDER},
    {6907747, u"igc", PUB_ORDER},
    {4539480, u"EDX", PUB_ORDER},
    {6644856, u"edx", PUB_ORDER},
    {1314210125, u"NUAM", PUB_ORDER | FULL_ORDER_BOOK | PUB_NUAM_ORDER | PUB_ORDER_IMBALANCE},
    {1853186413, u"nuam", PUB_ORDER | PUB_NUAM_ORDER},
    {1297043278, u"MOON", PUB_ORDER},
    {1836019566, u"moon", PUB_ORDER},
    {1330922317, u"OTCM", PUB_ORDER},
    {1869898605, u"otcm", PUB_ORDER},
    {1112494931, u"BOSS", PUB_ORDER},
    {1651471219, u"boss", PUB_ORDER},
    {5129551, u"NEO", PUB_ORDER},
    {7234927, u"neo", PUB_ORDER},
});

/** @param eventType Exact supported family.
 * @return Capability bit for that family.
 * @throws std::invalid_argument Unrecognized family.
 */
unsigned eventTypeMask(OrderEventType eventType) {
    switch (eventType) {
    case OrderEventType::ORDER:
        return PUB_ORDER;
    case OrderEventType::ANALYTIC_ORDER:
        return PUB_ANALYTIC_ORDER;
    case OrderEventType::OTC_MARKETS_ORDER:
        return PUB_OTC_MARKETS_ORDER;
    case OrderEventType::SPREAD_ORDER:
        return PUB_SPREAD_ORDER;
    case OrderEventType::NUAM_ORDER:
        return PUB_NUAM_ORDER;
    case OrderEventType::ORDER_IMBALANCE:
        return PUB_ORDER_IMBALANCE;
    }

    throw std::invalid_argument("Invalid order event type");
}

/** @param id Resolved source ID.
 * @return Built-in capability bits, or zero for custom sources.
 */
unsigned sourceFlags(std::int32_t id) noexcept {
    for (const auto &entry : BUILTINS) {
        if (entry.id == id) {
            return entry.flags;
        }
    }

    return 0;
}

/** @param mask Publication or full-book capability bit.
 * @return Owned filtered catalog in declaration order.
 * @throws std::bad_alloc Allocation fails.
 * @throws std::length_error Result exceeds container capacity.
 */
std::vector<OrderSource> matchingSources(unsigned mask) {
    std::vector<OrderSource> sources;

    for (const auto &entry : BUILTINS) {
        if ((entry.flags & mask) != 0) {
            sources.push_back(OrderSource::valueOf(entry.id));
        }
    }

    return sources;
}

/** @param unit Exact UTF-16 unit to check.
 * @return True for ASCII letters and digits only.
 */
bool isAlphanumeric(char16_t unit) noexcept {
    return (unit >= u'A' && unit <= u'Z') || (unit >= u'a' && unit <= u'z') || (unit >= u'0' && unit <= u'9');
}
} // namespace

const OrderSource OrderSource::COMPOSITE_BID{1, u"COMPOSITE_BID"};
const OrderSource OrderSource::COMPOSITE_ASK{2, u"COMPOSITE_ASK"};
const OrderSource OrderSource::REGIONAL_BID{3, u"REGIONAL_BID"};
const OrderSource OrderSource::REGIONAL_ASK{4, u"REGIONAL_ASK"};
const OrderSource OrderSource::AGGREGATE_BID{5, u"AGGREGATE_BID"};
const OrderSource OrderSource::AGGREGATE_ASK{6, u"AGGREGATE_ASK"};
const OrderSource OrderSource::COMPOSITE{7, u"COMPOSITE"};
const OrderSource OrderSource::REGIONAL{8, u"REGIONAL"};
const OrderSource OrderSource::AGGREGATE{9, u"AGGREGATE"};
const OrderSource OrderSource::DEFAULT{0, u"DEFAULT"};
const OrderSource OrderSource::NTV{5133398, u"NTV"};
const OrderSource OrderSource::NTV_LOWER{7238774, u"ntv"};
const OrderSource OrderSource::NFX{5129816, u"NFX"};
const OrderSource OrderSource::ESPD{1163087940, u"ESPD"};
const OrderSource OrderSource::XNFI{1481524809, u"XNFI"};
const OrderSource OrderSource::ICE{4801349, u"ICE"};
const OrderSource OrderSource::ISE{4805445, u"ISE"};
const OrderSource OrderSource::DEA{4474177, u"DEA"};
const OrderSource OrderSource::DEX{4474200, u"DEX"};
const OrderSource OrderSource::DEX_LOWER{6579576, u"dex"};
const OrderSource OrderSource::BYX{4348248, u"BYX"};
const OrderSource OrderSource::BZX{4348504, u"BZX"};
const OrderSource OrderSource::BZX_LOWER{6453880, u"bzx"};
const OrderSource OrderSource::BATE{1111577669, u"BATE"};
const OrderSource OrderSource::CHIX{1128810840, u"CHIX"};
const OrderSource OrderSource::CEUX{1128617304, u"CEUX"};
const OrderSource OrderSource::BXTR{1113085010, u"BXTR"};
const OrderSource OrderSource::IST{4805460, u"IST"};
const OrderSource OrderSource::BI20{1112093232, u"BI20"};
const OrderSource OrderSource::ABE{4276805, u"ABE"};
const OrderSource OrderSource::FAIR{1178683730, u"FAIR"};
const OrderSource OrderSource::GLBX{1196180056, u"GLBX"};
const OrderSource OrderSource::GLBX_LOWER{1735156344, u"glbx"};
const OrderSource OrderSource::ERIS{1163020627, u"ERIS"};
const OrderSource OrderSource::XEUR{1480938834, u"XEUR"};
const OrderSource OrderSource::XEUR_LOWER{2019915122, u"xeur"};
const OrderSource OrderSource::CFE{4408901, u"CFE"};
const OrderSource OrderSource::C2OX{1127370584, u"C2OX"};
const OrderSource OrderSource::SMFE{1397573189, u"SMFE"};
const OrderSource OrderSource::SMFE_LOWER{1936549477, u"smfe"};
const OrderSource OrderSource::IEX_LOWER{6907256, u"iex"};
const OrderSource OrderSource::MEMX{1296387416, u"MEMX"};
const OrderSource OrderSource::MEMX_LOWER{1835363704, u"memx"};
const OrderSource OrderSource::OCEA{1329808705, u"OCEA"};
const OrderSource OrderSource::OCEA_LOWER{1868784993, u"ocea"};
const OrderSource OrderSource::PINK_LOWER{1885957739, u"pink"};
const OrderSource OrderSource::ARCA{1095910209, u"ARCA"};
const OrderSource OrderSource::ARCA_LOWER{1634886497, u"arca"};
const OrderSource OrderSource::CEDX{1128612952, u"CEDX"};
const OrderSource OrderSource::CEDX_LOWER{1667589240, u"cedx"};
const OrderSource OrderSource::IGC{4802371, u"IGC"};
const OrderSource OrderSource::IGC_LOWER{6907747, u"igc"};
const OrderSource OrderSource::EDX{4539480, u"EDX"};
const OrderSource OrderSource::EDX_LOWER{6644856, u"edx"};
const OrderSource OrderSource::NUAM{1314210125, u"NUAM"};
const OrderSource OrderSource::NUAM_LOWER{1853186413, u"nuam"};
const OrderSource OrderSource::MOON{1297043278, u"MOON"};
const OrderSource OrderSource::MOON_LOWER{1836019566, u"moon"};
const OrderSource OrderSource::OTCM{1330922317, u"OTCM"};
const OrderSource OrderSource::OTCM_LOWER{1869898605, u"otcm"};
const OrderSource OrderSource::BOSS{1112494931, u"BOSS"};
const OrderSource OrderSource::BOSS_LOWER{1651471219, u"boss"};
const OrderSource OrderSource::NEO{5129551, u"NEO"};
const OrderSource OrderSource::NEO_LOWER{7234927, u"neo"};

OrderSource::OrderSource(std::int32_t id, std::u16string_view name) : source_(id, std::u16string(name)) {
}

OrderSource OrderSource::valueOf(std::int32_t id) {
    for (const auto &entry : BUILTINS) {
        if (entry.id == id) {
            return OrderSource(entry.id, entry.name);
        }
    }

    constexpr int BITS_PER_UNIT = 8;
    constexpr int HIGHEST_SHIFT = 24;
    constexpr std::uint32_t UNIT_MASK = 0xff;
    const auto bits = static_cast<std::uint32_t>(id);
    std::u16string name;

    for (int shift = HIGHEST_SHIFT; shift >= 0; shift -= BITS_PER_UNIT) {
        if ((bits >> shift) == 0) {
            continue;
        }

        const auto unit = static_cast<char16_t>((bits >> shift) & UNIT_MASK);

        if (!isAlphanumeric(unit)) {
            throw std::invalid_argument("Order source ID contains a non-alphanumeric byte");
        }

        name.push_back(unit);
    }

    if (name.empty()) {
        throw std::invalid_argument("Order source name must contain from 1 to 4 characters");
    }

    return OrderSource(id, name);
}

OrderSource OrderSource::valueOf(std::optional<std::u16string_view> name) {
    if (!name) {
        throw std::invalid_argument("Order source name is null");
    }

    for (const auto &entry : BUILTINS) {
        if (entry.name == *name) {
            return OrderSource(entry.id, entry.name);
        }
    }

    constexpr std::size_t MAX_NAME_UNITS = 4;
    constexpr int BITS_PER_UNIT = 8;

    if (name->empty() || name->size() > MAX_NAME_UNITS) {
        throw std::invalid_argument("Order source name must contain from 1 to 4 characters");
    }

    std::int32_t id = 0;

    for (const auto unit : *name) {
        if (!isAlphanumeric(unit)) {
            throw std::invalid_argument("Order source name must contain only ASCII alphanumeric characters");
        }

        id = (id << BITS_PER_UNIT) | unit;
    }

    return OrderSource(id, *name);
}

bool OrderSource::isPublishable(OrderEventType eventType) const {
    const auto mask = eventTypeMask(eventType);

    return (sourceFlags(id()) & mask) != 0;
}

bool OrderSource::isFullOrderBook() const noexcept {
    return (sourceFlags(id()) & FULL_ORDER_BOOK) != 0;
}

std::vector<OrderSource> OrderSource::publishable(OrderEventType eventType) {
    return matchingSources(eventTypeMask(eventType));
}

std::vector<OrderSource> OrderSource::fullOrderBook() {
    return matchingSources(FULL_ORDER_BOOK);
}

std::int32_t OrderSource::id() const noexcept {
    return source_.id();
}

std::u16string_view OrderSource::name() const noexcept {
    return *source_.name();
}

std::int32_t OrderSource::hashCode() const noexcept {
    return source_.hashCode();
}

const IndexedEventSource &OrderSource::indexedSource() const noexcept {
    return source_;
}

bool OrderSource::operator==(const OrderSource &other) const noexcept {
    return source_ == other.source_;
}

bool OrderSource::operator==(const IndexedEventSource &other) const noexcept {
    return source_ == other;
}
} // namespace org::ttldtor::qdcxx::events
