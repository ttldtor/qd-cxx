// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/option_chains.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <string_view>
#include <utility>

namespace org::ttldtor::qdcxx::ipf {
namespace {
/** Compare binary64 values without floating-point equality collapsing signed zero.
 * @param left First binary64 value.
 * @param right Second binary64 value.
 * @return Java Double.compare-compatible sign, canonicalizing all NaN payloads.
 */
int compareDouble(double left, double right) noexcept {
    if (left < right) {
        return -1;
    }

    if (left > right) {
        return 1;
    }

    if (std::isnan(left)) {
        return std::isnan(right) ? 0 : 1;
    }

    if (std::isnan(right)) {
        return -1;
    }

    const auto leftBits = std::bit_cast<std::int64_t>(left);
    const auto rightBits = std::bit_cast<std::int64_t>(right);

    return (leftBits > rightBits) - (leftBits < rightBits);
}

/** Add a resource charge without overflow or exceeding its configured ceiling.
 * @param value Current count, updated only on success.
 * @param amount Additional nonnegative charge.
 * @param limit Inclusive ceiling.
 * @return Whether the charge fits.
 */
bool charge(std::size_t &value, std::size_t amount, std::size_t limit) noexcept {
    if (value > limit || amount > limit - value) {
        return false;
    }

    value += amount;

    return true;
}

/** Measure the logical UTF-16 text stored once per series.
 * @param metadata Owned metadata to measure.
 * @return Number of retained UTF-16 units; these strings are disjoint profile fields.
 */
std::size_t metadataUnits(const OptionSeriesMetadata &metadata) noexcept {
    return metadata.additionalUnderlyings.size() + metadata.mmy.size() + metadata.optionType.size() +
           metadata.expirationStyle.size() + metadata.settlementStyle.size() + metadata.cfi.size();
}

/** Count chain symbols without allocation; each key is already charged to retainedUnits.
 * @param chains Coherent builder state whose logical retained charge bounds this sum.
 * @return Sum of map-key lengths, also bounding the separate chain-owned symbol copies.
 */
std::size_t symbolUnits(const OptionChains &chains) noexcept {
    std::size_t result = 0;

    for (const auto &entry : chains) {
        result += entry.first.size();
    }

    return result;
}

/** Extract required standard fields under the input profile's own access contract.
 * @param profile Borrowed source profile.
 * @return Owned metadata or PROFILE_ACCESS when a source access limit prevents extraction.
 */
std::expected<OptionSeriesMetadata, OptionChainError> readMetadata(const InstrumentProfile &profile) {
    const auto expiration = profile.getDateField(u"EXPIRATION");
    const auto lastTrade = profile.getDateField(u"LAST_TRADE");
    const auto multiplier = profile.getNumericField(u"MULTIPLIER");
    const auto spc = profile.getNumericField(u"SPC");

    if (!expiration || !lastTrade || !multiplier || !spc) {
        return std::unexpected(OptionChainError::PROFILE_ACCESS);
    }

    OptionSeriesMetadata metadata;
    metadata.expiration = *expiration;
    metadata.lastTrade = *lastTrade;
    metadata.multiplier = *multiplier;
    metadata.spc = *spc;
    metadata.additionalUnderlyings = profile.getField(u"ADDITIONAL_UNDERLYINGS");
    metadata.mmy = profile.getField(u"MMY");
    metadata.optionType = profile.getField(u"OPTION_TYPE");
    metadata.expirationStyle = profile.getField(u"EXPIRATION_STYLE");
    metadata.settlementStyle = profile.getField(u"SETTLEMENT_STYLE");
    metadata.cfi = profile.getField(u"CFI");

    if (metadata.cfi.size() >= 2) {
        metadata.cfi[1] = u'X';
    }

    return metadata;
}
} // namespace

int OptionSeriesMetadata::compareTo(const OptionSeriesMetadata &other) const noexcept {
    if (expiration != other.expiration) {
        return (expiration > other.expiration) - (expiration < other.expiration);
    }

    if (lastTrade != other.lastTrade) {
        return (lastTrade > other.lastTrade) - (lastTrade < other.lastTrade);
    }

    if (const auto order = compareDouble(multiplier, other.multiplier); order != 0) {
        return order;
    }

    if (const auto order = compareDouble(spc, other.spc); order != 0) {
        return order;
    }

    for (const auto &[left, right] :
         {std::pair{&additionalUnderlyings, &other.additionalUnderlyings}, std::pair{&mmy, &other.mmy},
          std::pair{&optionType, &other.optionType}, std::pair{&expirationStyle, &other.expirationStyle},
          std::pair{&settlementStyle, &other.settlementStyle}, std::pair{&cfi, &other.cfi}}) {
        if (const auto order = left->compare(*right); order != 0) {
            return order;
        }
    }

    return 0;
}

bool OptionStrikeLess::operator()(double left, double right) const noexcept {
    return compareDouble(left, right) < 0;
}

const OptionSeriesMetadata &OptionSeries::getMetadata() const noexcept {
    return metadata_;
}

const OptionMap &OptionSeries::getCalls() const noexcept {
    return calls_;
}

const OptionMap &OptionSeries::getPuts() const noexcept {
    return puts_;
}

std::vector<double> OptionSeries::getStrikes() const {
    std::vector<double> result;
    const auto less = OptionStrikeLess{};
    auto call = calls_.begin();
    auto put = puts_.begin();

    while (call != calls_.end() || put != puts_.end()) {
        if (put == puts_.end() || (call != calls_.end() && less(call->first, put->first))) {
            result.push_back(call++->first);
        } else if (call == calls_.end() || less(put->first, call->first)) {
            result.push_back(put++->first);
        } else {
            result.push_back(call++->first);
            ++put;
        }
    }

    return result;
}

std::expected<std::vector<double>, OptionChainError> OptionSeries::getNStrikesAround(std::int32_t count,
                                                                                     double strike) const {
    if (count < 0) {
        return std::unexpected(OptionChainError::INVALID_COUNT);
    }

    const auto strikes = getStrikes();
    const auto insertion = std::lower_bound(strikes.begin(), strikes.end(), strike, OptionStrikeLess{});
    const auto index = static_cast<std::size_t>(insertion - strikes.begin());
    const auto size = static_cast<std::size_t>(count);
    const auto from = index - std::min(index, size / 2);
    const auto to = from + std::min(size, strikes.size() - from);

    return std::vector<double>(strikes.begin() + static_cast<std::ptrdiff_t>(from),
                               strikes.begin() + static_cast<std::ptrdiff_t>(to));
}

const std::u16string &OptionChain::getSymbol() const noexcept {
    return symbol_;
}

const std::vector<OptionSeries> &OptionChain::getSeries() const noexcept {
    return series_;
}

OptionChainsBuilder::OptionChainsBuilder(OptionChainLimits limits) : limits_(limits) {
}

OptionChainsBuilder::OptionChainsBuilder(OptionChainsBuilder &&other) : OptionChainsBuilder(other.limits_) {
    swap(other);
}

OptionChainsBuilder &OptionChainsBuilder::operator=(OptionChainsBuilder other) {
    swap(other);

    return *this;
}

void OptionChainsBuilder::swap(OptionChainsBuilder &other) {
    using std::swap;
    chains_.swap(other.chains_);
    swap(limits_, other.limits_);
    swap(profiles_, other.profiles_);
    swap(series_, other.series_);
    swap(options_, other.options_);
    swap(retainedUnits_, other.retainedUnits_);
    swap(workUnits_, other.workUnits_);
}

std::expected<OptionChainsBuilder, OptionChainError>
OptionChainsBuilder::build(std::span<const InstrumentProfile> profiles, OptionChainLimits limits) {
    if (profiles.size() > limits.profiles) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    OptionChainsBuilder builder(limits);

    for (const auto &profile : profiles) {
        if (auto result = builder.addProfile(profile); !result) {
            return std::unexpected(result.error());
        }
    }

    return builder;
}

std::expected<bool, OptionChainError> OptionChainsBuilder::addProfile(const InstrumentProfile &profile) {
    auto initialStaging = retainedUnits_;

    if (!charge(initialStaging, profile.retainedUnits(), limits_.stagingUnits)) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    auto nextProfiles = profiles_;
    auto nextWork = workUnits_;

    if (!charge(nextProfiles, 1, limits_.profiles) || !charge(nextWork, 1, limits_.workUnits) ||
        !charge(nextWork, profile.retainedUnits(), limits_.workUnits)) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    const auto ignore = [&]() -> std::expected<bool, OptionChainError> {
        profiles_ = nextProfiles;
        workUnits_ = nextWork;

        return false;
    };

    if (profile.getField(u"TYPE") != u"OPTION") {
        return ignore();
    }

    const auto cfi = profile.getField(u"CFI");
    const auto product = profile.getField(u"PRODUCT");
    const auto underlying = profile.getField(u"UNDERLYING");

    if ((!cfi.starts_with(u"OC") && !cfi.starts_with(u"OP")) || (product.empty() && underlying.empty())) {
        return ignore();
    }

    const auto oldSymbolUnits = symbolUnits(chains_);
    auto metadataStaging = retainedUnits_;

    if (!charge(metadataStaging, oldSymbolUnits, limits_.stagingUnits) ||
        !charge(metadataStaging, profile.retainedUnits(), limits_.stagingUnits)) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    const auto strike = profile.getNumericField(u"STRIKE");
    auto metadata = readMetadata(profile);

    if (!strike || !metadata) {
        return std::unexpected(OptionChainError::PROFILE_ACCESS);
    }

    if (!std::isfinite(*strike) || metadata->expiration == 0) {
        return ignore();
    }

    // Charge staging work before copying the current symbol map or any affected chain.
    if (!charge(nextWork, retainedUnits_, limits_.workUnits) || !charge(nextWork, chains_.size(), limits_.workUnits) ||
        !charge(nextWork, series_, limits_.workUnits) || !charge(nextWork, options_, limits_.workUnits)) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    auto nextSeries = series_;
    auto nextOptions = options_;
    auto nextUnits = retainedUnits_;
    auto addedUnits = std::size_t{0};
    auto nextSymbolUnits = oldSymbolUnits;
    auto nextChainCount = chains_.size();
    const auto isCall = cfi.starts_with(u"OC");
    const auto seriesLess = [](const OptionSeries &series, const OptionSeriesMetadata &key) {
        return series.getMetadata().compareTo(key) < 0;
    };
    std::array<std::u16string_view, 2> destinations{product, underlying};

    if (product == underlying) {
        destinations[1] = {};
    }

    // Preflight both destinations before allocating staged containers or the owned profile copy.
    for (const auto symbol : destinations) {
        if (symbol.empty()) {
            continue;
        }

        const auto chain = chains_.find(symbol);

        if (chain == chains_.end()) {
            if (!charge(nextChainCount, 1, limits_.chains) ||
                !charge(addedUnits, symbol.size(), limits_.retainedUnits) ||
                !charge(nextSymbolUnits, symbol.size(), limits_.retainedUnits)) {
                return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
            }
        }

        const OptionSeries *existing = nullptr;

        if (chain != chains_.end()) {
            const auto &all = chain->second->series_;
            const auto found = std::lower_bound(all.begin(), all.end(), *metadata, seriesLess);

            if (found != all.end() && found->metadata_.compareTo(*metadata) == 0) {
                existing = &*found;
            }
        }

        if (!existing && (!charge(nextSeries, 1, limits_.series) ||
                          !charge(addedUnits, metadataUnits(*metadata), limits_.retainedUnits))) {
            return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
        }

        const OptionMap *slots = existing ? (isCall ? &existing->calls_ : &existing->puts_) : nullptr;

        if (slots) {
            const auto previous = slots->find(*strike);

            if (previous != slots->end()) {
                nextUnits -= previous->second->retainedUnits();
            } else if (!charge(nextOptions, 1, limits_.options)) {
                return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
            }
        } else if (!charge(nextOptions, 1, limits_.options)) {
            return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
        }

        if (!charge(addedUnits, profile.retainedUnits(), limits_.retainedUnits)) {
            return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
        }
    }

    // Debit every displaced slot before applying the complete transaction's positive charges.
    if (!charge(nextUnits, addedUnits, limits_.retainedUnits)) {
        return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
    }

    // Logical retained units count a symbol once. Both live graphs also own chain-symbol copies.
    // An extra old graph allowance covers vector insertion retaining the cloned old series storage.
    // Three input-sized allowances cover extracted metadata and series/key insertion temporaries.
    auto stagingUnits = std::size_t{0};

    for (const auto amount : {retainedUnits_, retainedUnits_, nextUnits, oldSymbolUnits, nextSymbolUnits,
                              profile.retainedUnits(), profile.retainedUnits(), profile.retainedUnits()}) {
        if (!charge(stagingUnits, amount, limits_.stagingUnits)) {
            return std::unexpected(OptionChainError::LIMIT_EXCEEDED);
        }
    }

    auto staged = chains_;
    auto owned = std::make_shared<const InstrumentProfile>(profile);

    for (const auto symbol : destinations) {
        if (symbol.empty()) {
            continue;
        }

        const auto previous = chains_.find(symbol);
        auto chain = previous == chains_.end() ? std::make_shared<OptionChain>()
                                               : std::make_shared<OptionChain>(*previous->second);
        chain->symbol_ = symbol;
        auto series = std::lower_bound(chain->series_.begin(), chain->series_.end(), *metadata, seriesLess);

        if (series == chain->series_.end() || series->metadata_.compareTo(*metadata) != 0) {
            OptionSeries added;
            added.metadata_ = *metadata;
            series = chain->series_.insert(series, std::move(added));
        }

        auto &slots = isCall ? series->calls_ : series->puts_;
        slots.insert_or_assign(*strike, owned);
        staged.insert_or_assign(std::u16string(symbol), std::move(chain));
    }

    chains_.swap(staged);
    profiles_ = nextProfiles;
    series_ = nextSeries;
    options_ = nextOptions;
    retainedUnits_ = nextUnits;
    workUnits_ = nextWork;

    return true;
}

const OptionChains &OptionChainsBuilder::getChains() const noexcept {
    return chains_;
}

OptionChainLimits OptionChainsBuilder::getLimits() const noexcept {
    return limits_;
}

std::size_t OptionChainsBuilder::getProfileCount() const noexcept {
    return profiles_;
}

std::size_t OptionChainsBuilder::getWorkUnits() const noexcept {
    return workUnits_;
}
} // namespace org::ttldtor::qdcxx::ipf
