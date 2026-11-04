// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "feed_filter_expression.hpp"
#include "feed_filter_ipf_config.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <utility>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
using Kind = FilterExpressionError::Kind;
constexpr char16_t NON_ASCII_BUCKET = 127;
constexpr std::uint32_t HASH_MULTIPLIER = 0xB46394CDU;
constexpr std::size_t QD_FINITE_PATTERN_CUTOFF = 10000; ///< Pinned compatibility classification, not a resource grant.

[[noreturn]] void syntax(std::size_t offset, const char *message) {
    throw FilterExpressionError(Kind::SYNTAX, offset, message);
}

bool dictionary(char16_t c) noexcept {
    return (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9');
}

bool upper(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z';
}

bool digit(char16_t c) noexcept {
    return c >= u'0' && c <= u'9';
}

std::size_t rangeStart(std::u16string_view symbol) noexcept {
    std::size_t pos = 0;

    if (symbol.starts_with(u'=')) {
        pos = 1;

        if (pos < symbol.size() && (symbol[pos] == u'+' || symbol[pos] == u'-')) {
            ++pos;
        }

        auto end = pos;

        while (end < symbol.size() && digit(symbol[end])) {
            ++end;
        }

        const bool number = end > pos;

        if (number && end < symbol.size() && symbol[end] == u'.') {
            ++end;

            while (end < symbol.size() && digit(symbol[end])) {
                ++end;
            }
        }

        if (number && end < symbol.size() && symbol[end] == u'*') {
            pos = end + 1;
        }
    }

    while (pos < symbol.size() && !dictionary(symbol[pos])) {
        ++pos;
    }

    return pos;
}

bool feedRecord(std::u16string_view record, bool composite, bool regional) noexcept {
    constexpr std::u16string_view EXACT[]{u"Quote",       u"Trade",        u"TradeETH",  u"Summary",
                                          u"Fundamental", u"Profile",      u"Book",      u"MarketMaker",
                                          u"TimeAndSale", u"TradeHistory", u"NuamTrade", u"NuamTimeAndSale"};
    constexpr std::u16string_view PREFIX[]{u"Order", u"SpreadOrder", u"OtcMarketsOrder", u"NuamOrder",
                                           u"OrderImbalance"};

    for (const auto base : EXACT) {
        if ((composite && record == base) ||
            (regional && record.size() == base.size() + 2 && record.starts_with(base) && record[base.size()] == u'&' &&
             dictionary(record.back()))) {
            return true;
        }
    }

    if (composite) {
        for (const auto prefix : PREFIX) {
            if (record.starts_with(prefix)) {
                return true;
            }
        }
    }

    return false;
}

bool chartRecord(std::u16string_view record) noexcept {
    constexpr std::u16string_view EXACT[]{u"TradeHistory", u"Candle",          u"DividendHistory",
                                          u"SplitHistory", u"EarningsHistory", u"ConferenceCallHistory",
                                          u"NewsHistory"};

    return std::ranges::find(EXACT, record) != std::end(EXACT) || record.starts_with(u"Trade.") ||
           record.starts_with(u"Candle{");
}

bool product(std::u16string_view symbol) noexcept {
    if (symbol.size() < 2 || symbol.front() != u'/') {
        return false;
    }

    for (std::size_t i = 1; i <= 4 && i < symbol.size(); ++i) {
        const auto c = symbol[i];

        if (c == u'{' || c == u':') {
            return i >= 2 && i <= 4;
        }

        if (!upper(c) && !digit(c)) {
            return false;
        }
    }

    return symbol.size() <= 4;
}

bool forex(std::u16string_view symbol) noexcept {
    for (const std::size_t prefix : {3U, 4U}) {
        const auto length = prefix + 4;

        if (symbol.size() < length || symbol[prefix] != u'/') {
            continue;
        }

        bool valid = true;

        for (std::size_t i = 0; i < length; ++i) {
            valid = valid && (i == prefix || upper(symbol[i]));
        }

        if (valid &&
            (symbol.size() == length || symbol[length] == u'#' || symbol[length] == u'{' || symbol[length] == u':')) {
            return true;
        }
    }

    return false;
}

bool conversion(std::u16string_view symbol) noexcept {
    if (symbol.size() == 9) {
        return symbol[3] == u'$' && symbol[4] == u'/' && symbol[8] == u'$' && upper(symbol[0]) && upper(symbol[1]) &&
               upper(symbol[2]) && upper(symbol[5]) && upper(symbol[6]) && upper(symbol[7]);
    }

    return symbol.size() == 13 && symbol.substr(3, 4) == u"$FX/" && symbol.substr(10) == u"$FX" && upper(symbol[0]) &&
           upper(symbol[1]) && upper(symbol[2]) && upper(symbol[7]) && upper(symbol[8]) && upper(symbol[9]);
}
} // namespace

FilterExpressionError::FilterExpressionError(Kind kindValue, std::size_t offsetValue, const char *message)
    : std::runtime_error(message), kind(kindValue), offset(offsetValue) {
}

FilterPattern::FilterPattern(std::u16string_view pattern, std::size_t maximumPositions) {
    if (!pattern.empty() && pattern.front() >= u'a' && pattern.front() <= u'z') {
        syntax(0, "Lowercase pattern prefixes are reserved");
    }

    negated_ = pattern.starts_with(u'!');
    std::size_t pos = negated_ ? 1 : 0;
    std::size_t positions = 0;
    segments_.emplace_back();
    Bits bits{};
    bool quote = false;
    bool escaped = false;
    bool inClass = false;
    bool classStart = false;
    bool range = false;
    bool inverse = false;
    char16_t last = 0;

    for (; pos < pattern.size(); ++pos) {
        const auto c = pattern[pos];

        if (c < u' ' || c >= NON_ASCII_BUCKET) {
            syntax(pos, "Pattern character outside ASCII 32..126");
        }

        bool regular = false;
        bool finish = false;

        if (quote) {
            if (c == u'\\' && pos + 1 < pattern.size() && pattern[pos + 1] == u'E') {
                quote = false;
                ++pos;
            } else {
                regular = true;
            }
        } else if (escaped) {
            escaped = false;

            if (c == u'Q') {
                quote = true;
            } else {
                regular = true;
            }
        } else {
            switch (c) {
            case u'[':

                if (inClass) {
                    syntax(pos, "Nested character class");
                }

                inClass = true;
                classStart = true;
                break;
            case u']':

                if (!inClass || range) {
                    syntax(pos, "Invalid character class end");
                }

                inClass = false;
                finish = true;
                break;
            case u'^':

                if (classStart) {
                    classStart = false;
                    inverse = true;
                } else {
                    regular = true;
                }

                break;
            case u'-':

                if (inClass) {
                    if (range || last == 0) {
                        syntax(pos, "Invalid character class range");
                    }

                    range = true;
                } else {
                    regular = true;
                }

                break;
            case u'\\':
                escaped = true;
                break;
            case u'*':

                if (!inClass) {
                    if (segments_.size() == 3 || (pos > 0 && pattern[pos - 1] == u'*')) {
                        syntax(pos, "At most two nonadjacent stars are permitted");
                    }

                    segments_.emplace_back();
                    continue;
                }

                regular = true;
                break;
            case u'!':
            case u'|':
            case u'&':
            case u'?':
            case u'+':
            case u'(':
            case u')':
            case u'<':
            case u'>':
            case u'~':
            case u'"':
            case u'\'':
            case u'`':

                if (!inClass) {
                    syntax(pos, "Unquoted reserved pattern character");
                }

                regular = true;
                break;
            default:
                regular = true;
                break;
            }
        }

        if (regular) {
            if (range) {
                for (auto value = last; value <= c; ++value) {
                    bits[value] = true;
                }

                last = 0;
            } else {
                bits[c] = true;
                last = c;
            }

            classStart = false;
            range = false;
            finish = !inClass;
        }

        if (finish) {
            if (inverse) {
                for (std::size_t value = 1; value <= NON_ASCII_BUCKET; ++value) {
                    bits[value] = !bits[value];
                }
            }

            if (positions == maximumPositions) {
                throw FilterExpressionError(Kind::RESOURCE_LIMIT, pos, "Pattern position capacity exhausted");
            }

            ++positions;
            segments_.back().push_back(bits);
            bits = {};
            inverse = false;
        }
    }

    if (quote || escaped || inClass) {
        syntax(pos, "Unterminated pattern quote, escape or class");
    }
}

bool FilterPattern::matches(std::u16string_view text) const noexcept {
    const auto matchAt = [&text](const auto &segment, std::size_t pos) {
        for (const auto &bits : segment) {
            if (!bits[std::min<char16_t>(NON_ASCII_BUCKET, text[pos++])]) {
                return false;
            }
        }

        return true;
    };
    const auto &prefix = segments_.front();
    const auto &suffix = segments_.back();
    std::size_t required = 0;

    for (const auto &segment : segments_) {
        required += segment.size();
    }

    bool accepted = text.size() >= required;

    if (accepted && segments_.size() == 1) {
        accepted = text.size() == prefix.size() && matchAt(prefix, 0);
    } else if (accepted) {
        accepted = matchAt(prefix, 0) && matchAt(suffix, text.size() - suffix.size());

        if (accepted && segments_.size() == 3) {
            const auto &middle = segments_[1];
            accepted = false;
            const auto end = text.size() - suffix.size() - middle.size();

            for (auto pos = prefix.size(); pos <= end; ++pos) {
                if (matchAt(middle, pos)) {
                    accepted = true;
                    break;
                }
            }
        }
    }

    return negated_ != accepted;
}

std::optional<std::size_t> FilterPattern::finiteSize() const noexcept {
    if (negated_ || segments_.size() != 1) {
        return std::nullopt;
    }

    std::size_t count = 1;

    for (const auto &bits : segments_.front()) {
        if (bits[NON_ASCII_BUCKET]) {
            return std::nullopt;
        }

        const auto choices = static_cast<std::size_t>(std::count(bits.begin(), bits.begin() + NON_ASCII_BUCKET, true));

        if (choices == 0) {
            count = 0;
            continue;
        }

        if (count > QD_FINITE_PATTERN_CUTOFF / choices) {
            return std::nullopt;
        }

        count *= choices;
    }

    return count;
}

std::size_t FilterPattern::positions() const noexcept {
    std::size_t result = 0;

    for (const auto &segment : segments_) {
        result += segment.size();
    }

    return result;
}

bool FilterPattern::visitFinite(const std::function<bool(std::u16string_view)> &visitor, std::size_t &remainingSymbols,
                                std::size_t &remainingWork) const {
    const auto debit = [&](std::size_t work) {
        if (work > remainingWork) {
            throw FilterExpressionError(Kind::WORK_LIMIT, 0, "Finite normalization work exhausted");
        }

        remainingWork -= work;
    };
    const auto size = finiteSize();

    if (!size) {
        throw FilterExpressionError(Kind::SYNTAX, 0, "Internal nonfinite enumeration request");
    }

    if (*size == 0) {
        return true;
    }

    const auto &positions = segments_.front();
    std::u16string first(positions.size(), 0);

    for (std::size_t i = 0; i < positions.size(); ++i) {
        for (char16_t c = 0; c < NON_ASCII_BUCKET; ++c) {
            debit(1);

            if (positions[i][c]) {
                first[i] = c;
                break;
            }
        }
    }

    auto symbol = first;

    for (;;) {
        if (remainingSymbols == 0) {
            throw FilterExpressionError(Kind::RESOURCE_LIMIT, 0, "Finite normalization candidate capacity exhausted");
        }

        --remainingSymbols;
        debit(symbol.size() + 1);

        if (!visitor(symbol)) {
            return false;
        }

        bool advanced = false;

        for (std::size_t i = positions.size(); i-- > 0;) {
            for (char16_t c = symbol[i] + 1; c < NON_ASCII_BUCKET; ++c) {
                debit(1);

                if (positions[i][c]) {
                    symbol[i] = c;

                    for (std::size_t j = i + 1; j < symbol.size(); ++j) {
                        debit(1);
                        symbol[j] = first[j];
                    }

                    advanced = true;
                    break;
                }
            }

            if (advanced) {
                break;
            }
        }

        if (!advanced) {
            return true;
        }
    }
}

struct FilterExpression::Node {
    enum class Type {
        CONSTANT,
        PATTERN,
        RECORD,
        AND,
        OR,
        NOT,
        FEED,
        COMPFEED,
        REGFEED,
        CHARTDATA,
        CONVRATES,
        NWC,
        PRODUCT,
        FOREX,
        HASH,
        IPF,
        RANGE
    };
    enum class FilterKind { RECORD_ONLY, SYMBOL_SET, COFINITE_SET, SYMBOL_OTHER, OTHER };
    FilterKind filterKind = FilterKind::OTHER;
    std::size_t finiteBound = 0;
    Type type = Type::CONSTANT;
    std::shared_ptr<const std::vector<std::u16string>> symbols;
    bool dynamic = false;
    std::u16string resourceExpression;
    bool value = true;
    std::optional<FilterPattern> pattern;
    std::shared_ptr<const Node> left;
    std::shared_ptr<const Node> right;
    std::u16string lower;
    std::u16string upper;
    std::uint32_t hashIndex = 0;
    unsigned hashShift = 0;
    std::size_t depth = 1;

    bool symbolOnly() const noexcept {
        return filterKind == FilterKind::SYMBOL_SET || filterKind == FilterKind::COFINITE_SET ||
               filterKind == FilterKind::SYMBOL_OTHER;
    }

    void collectConjuncts(std::vector<const Node *> &terms) const {
        if (type == Type::AND) {
            left->collectConjuncts(terms);
            right->collectConjuncts(terms);
        } else {
            terms.push_back(this);
        }
    }

    const Node *finiteSeed() const {
        std::vector<const Node *> terms;
        collectConjuncts(terms);
        const Node *seed = nullptr;

        for (const auto *term : terms) {
            if (term->filterKind == FilterKind::SYMBOL_SET && !term->dynamic &&
                (!seed || term->finiteBound < seed->finiteBound)) {
                seed = term;
            }
        }

        return seed;
    }

    void prepayEvaluation(std::size_t symbolUnits, std::size_t &work) const {
        const auto factor = pattern             ? pattern->positions() + 1
                            : type == Type::IPF ? std::bit_width(symbols->size()) + 1
                                                : 1;
        const auto units = symbolUnits + 1;
        constexpr std::size_t COMPARISON_FACTOR = 8;

        if (units > work / COMPARISON_FACTOR || factor > work / (units * COMPARISON_FACTOR)) {
            throw FilterExpressionError(Kind::WORK_LIMIT, 0, "Finite predicate work exhausted");
        }

        work -= units * COMPARISON_FACTOR * factor;

        if (left) {
            left->prepayEvaluation(symbolUnits, work);
        }

        if (right) {
            right->prepayEvaluation(symbolUnits, work);
        }
    }

    bool finiteContains(std::u16string_view symbol) const noexcept {
        if (type == Type::PATTERN) {
            return pattern->matches(symbol);
        }

        if (type == Type::OR) {
            return left->finiteContains(symbol) || right->finiteContains(symbol);
        }

        const auto acceptsTerm = [&](const auto &term) {
            return term.filterKind == FilterKind::SYMBOL_SET ? term.finiteContains(symbol)
                                                             : term.accepts({}, symbol, symbol == u"*");
        };

        return acceptsTerm(*left) && acceptsTerm(*right);
    }

    bool visitFinite(const std::function<bool(std::u16string_view)> &visitor, std::size_t &remainingSymbols,
                     std::size_t &remainingWork) const {
        if (type == Type::PATTERN) {
            return pattern->visitFinite(visitor, remainingSymbols, remainingWork);
        }

        if (type == Type::OR) {
            return left->visitFinite(visitor, remainingSymbols, remainingWork) &&
                   right->visitFinite(visitor, remainingSymbols, remainingWork);
        }

        const auto *seed = finiteSeed();

        if (!seed || seed == this) {
            throw FilterExpressionError(Kind::SYNTAX, 0, "Internal finite seed is missing");
        }

        return seed->visitFinite(
            [&](std::u16string_view symbol) {
                prepayEvaluation(symbol.size(), remainingWork);

                return !finiteContains(symbol) || visitor(symbol);
            },
            remainingSymbols, remainingWork);
    }

    struct Possibilities {
        bool accept;
        bool reject;
    };

    Possibilities wildcardPossibilities(std::u16string_view record) const noexcept {
        if (type == Type::CONSTANT || type == Type::RECORD || type == Type::FEED || type == Type::COMPFEED ||
            type == Type::REGFEED || type == Type::CHARTDATA || type == Type::NWC) {
            const bool accepted = accepts(record, {}, true);

            return {accepted, !accepted};
        }

        if (type == Type::NOT) {
            const auto child = left->wildcardPossibilities(record);

            return {child.reject, child.accept};
        }

        if (type == Type::AND || type == Type::OR) {
            const auto a = left->wildcardPossibilities(record);
            const auto b = right->wildcardPossibilities(record);

            return type == Type::AND ? Possibilities{a.accept && b.accept, a.reject || b.reject}
                                     : Possibilities{a.accept || b.accept, a.reject && b.reject};
        }

        if (type == Type::CONVRATES && !(record == u"Quote" || (record.size() == 7 && record.starts_with(u"Quote&") &&
                                                                dictionary(record.back())))) {
            return {false, true};
        }

        // ALL asks whether a symbol can match. Independent partial-symbol constraints are a conservative
        // projection: no exact cross-leaf satisfiability search or hidden enumeration is performed.
        return {true, true};
    }

    bool accepts(std::u16string_view record, std::u16string_view symbol, bool wildcard) const noexcept {
        switch (type) {
        case Type::CONSTANT:
            return value;
        case Type::PATTERN:
            return wildcard || pattern->matches(symbol);
        case Type::RECORD:
            return pattern->matches(record);
        case Type::AND:
            return left->accepts(record, symbol, wildcard) && right->accepts(record, symbol, wildcard);
        case Type::OR:
            return left->accepts(record, symbol, wildcard) || right->accepts(record, symbol, wildcard);
        case Type::NOT:
            return wildcard ? wildcardPossibilities(record).accept : !left->accepts(record, symbol, false);
        case Type::FEED:
            return feedRecord(record, true, true);
        case Type::COMPFEED:
            return feedRecord(record, true, false);
        case Type::REGFEED:
            return feedRecord(record, false, true);
        case Type::CHARTDATA:
            return chartRecord(record);
        case Type::IPF: {
            if (wildcard || std::ranges::binary_search(*symbols, symbol)) {
                return true;
            }

            if (symbol.size() < 2 || symbol.back() != u'}') {
                return false;
            }

            const auto opener = symbol.find(u'{');

            return opener != std::u16string_view::npos &&
                   std::ranges::binary_search(*symbols, symbol.substr(0, opener));
        }
        case Type::NWC:
            return !wildcard;
        case Type::PRODUCT:
            return wildcard || product(symbol);
        case Type::FOREX:
            return wildcard || forex(symbol);
        case Type::CONVRATES:
            return (record == u"Quote" ||
                    (record.size() == 7 && record.starts_with(u"Quote&") && dictionary(record.back()))) &&
                   (wildcard || conversion(symbol));
        case Type::HASH: {
            std::uint32_t hash = 0;

            for (const auto c : symbol) {
                hash = hash * 31U + c;
            }

            return wildcard || ((hash * HASH_MULTIPLIER) >> hashShift) == hashIndex;
        }
        case Type::RANGE: {
            const auto tail = symbol.substr(rangeStart(symbol));

            return wildcard || ((lower.empty() || tail >= lower) && (upper.empty() || tail < upper));
        }
        }

        return false;
    }

    bool usesResource(std::u16string_view expression) const noexcept {
        return resourceExpression == expression || (left && left->usesResource(expression)) ||
               (right && right->usesResource(expression));
    }

    struct Parser;
};

struct FilterExpression::Node::Parser {
    std::u16string_view text;
    FilterExpressionLimits limits;
    std::span<const FilterSymbolResource> resources;
    std::vector<std::u16string> *referenced = nullptr;
    std::size_t remainingFiniteSymbols = limits.finiteSymbols;
    std::size_t remainingNormalizationWork = limits.normalizationWork;
    std::size_t pos = 0;
    std::size_t next = 0;
    std::size_t nodes = 0;
    char16_t current = 0;
    bool keyword = false;

    static bool special(char16_t c) noexcept {
        return c == 0 || c == u'!' || c == u'&' || c == u',' || c == u'(' || c == u')';
    }

    static bool delimiter(char16_t c) noexcept {
        return special(c) || c == u' ' || c == u':' || c == u'*' || c == u'{' || c == u'}';
    }

    char16_t at(std::size_t index) const noexcept {
        return index < text.size() ? text[index] : u' ';
    }

    std::size_t skipSpaces(std::size_t index) const noexcept {
        while (index < text.size() && text[index] == u' ') {
            ++index;
        }

        return index;
    }

    void scan() {
        const auto start = skipSpaces(pos);
        constexpr std::pair<std::u16string_view, char16_t> KEYWORDS[]{
            {u"not", u'!'}, {u"and", u'&'}, {u"or", u','}, {u"record", u':'}, {u"any", u'*'}};

        if ((start == 0 || delimiter(at(start - 1))) && !delimiter(at(start))) {
            for (const auto &[name, symbol] : KEYWORDS) {
                if (text.substr(start).starts_with(name) && delimiter(at(start + name.size()))) {
                    current = symbol;
                    next = skipSpaces(start + name.size());
                    keyword = true;

                    return;
                }
            }
        }

        keyword = false;
        current = pos < text.size() ? text[pos] : 0;
        next = pos < text.size() ? pos + 1 : pos;
    }

    void advance() {
        pos = next;
        scan();
    }

    std::shared_ptr<Node> make(Type type) {
        if (nodes == limits.nodes) {
            throw FilterExpressionError(Kind::RESOURCE_LIMIT, pos, "Expression node capacity exhausted");
        }

        ++nodes;
        auto node = std::make_shared<Node>();
        node->type = type;

        switch (type) {
        case Type::RECORD:
        case Type::FEED:
        case Type::COMPFEED:
        case Type::REGFEED:
        case Type::CHARTDATA:
            node->filterKind = FilterKind::RECORD_ONLY;
            break;
        case Type::PATTERN:
        case Type::PRODUCT:
        case Type::FOREX:
        case Type::HASH:
        case Type::RANGE:
        case Type::IPF:
            node->filterKind = FilterKind::SYMBOL_OTHER;
            break;
        default:
            break;
        }

        return node;
    }

    void checkDepth(std::size_t depth) const {
        if (depth > limits.depth) {
            throw FilterExpressionError(Kind::RESOURCE_LIMIT, pos, "Expression depth capacity exhausted");
        }
    }

    void normalizeAnd(const std::shared_ptr<Node> &node) {
        std::vector<const Node *> terms;
        node->collectConjuncts(terms);
        const Node *seed = nullptr;
        bool allStaticSymbol = true;

        for (const auto *term : terms) {
            allStaticSymbol = allStaticSymbol && term->symbolOnly() && !term->dynamic;

            if (term->filterKind == FilterKind::SYMBOL_SET && !term->dynamic &&
                (!seed || term->finiteBound < seed->finiteBound)) {
                seed = term;
            }
        }

        if (!seed) {
            return;
        }

        bool nonempty = false;
        (void)seed->visitFinite(
            [&](std::u16string_view symbol) {
                for (const auto *term : terms) {
                    if (!term->symbolOnly() || term->dynamic) {
                        continue;
                    }

                    term->prepayEvaluation(symbol.size(), remainingNormalizationWork);
                    const bool accepts = term->filterKind == FilterKind::SYMBOL_SET
                                             ? term->finiteContains(symbol)
                                             : term->accepts({}, symbol, symbol == u"*");

                    if (!accepts) {
                        return true;
                    }
                }

                nonempty = true;

                return false;
            },
            remainingFiniteSymbols, remainingNormalizationWork);

        if (!nonempty) {
            node->type = Type::CONSTANT;
            node->value = false;
            node->dynamic = false;
            node->filterKind = FilterKind::OTHER;
            node->left.reset();
            node->right.reset();
            node->depth = 1;
        } else if (allStaticSymbol) {
            node->filterKind = FilterKind::SYMBOL_SET;
            node->finiteBound = seed->finiteBound;
        }
    }

    std::shared_ptr<const Node> combine(Type type, std::shared_ptr<const Node> left,
                                        std::shared_ptr<const Node> right) {
        if (referenced) {
            return left;
        }

        if (left->type == Type::CONSTANT) {
            return (type == Type::AND ? left->value : !left->value) ? right : left;
        }

        if (right->type == Type::CONSTANT) {
            return (type == Type::AND ? right->value : !right->value) ? left : right;
        }

        auto node = make(type);
        node->left = std::move(left);
        node->right = std::move(right);
        node->dynamic = node->left->dynamic || node->right->dynamic;
        node->depth = 1 + std::max(node->left->depth, node->right->depth);
        checkDepth(node->depth);

        if (node->left->filterKind == FilterKind::RECORD_ONLY && node->right->filterKind == FilterKind::RECORD_ONLY) {
            node->filterKind = FilterKind::RECORD_ONLY;
        } else if (node->left->symbolOnly() && node->right->symbolOnly()) {
            node->filterKind = FilterKind::SYMBOL_OTHER;
        }

        if (type == Type::OR && node->left->filterKind == FilterKind::SYMBOL_SET &&
            node->right->filterKind == FilterKind::SYMBOL_SET) {
            if (node->left->finiteBound > std::numeric_limits<std::size_t>::max() - node->right->finiteBound) {
                throw FilterExpressionError(Kind::RESOURCE_LIMIT, pos, "Finite candidate bound overflow");
            }

            node->filterKind = FilterKind::SYMBOL_SET;
            node->finiteBound = node->left->finiteBound + node->right->finiteBound;
        } else if (type == Type::AND && !referenced) {
            normalizeAnd(node);
        }

        return node;
    }

    std::shared_ptr<const Node> parseOr(std::size_t depth) {
        auto node = parseAnd(depth);

        while (current == u',') {
            advance();
            node = combine(Type::OR, std::move(node), parseAnd(depth));
        }

        return node;
    }

    std::shared_ptr<const Node> parseAnd(std::size_t depth) {
        auto node = parseToken(depth);

        while (current == u'&') {
            advance();
            node = combine(Type::AND, std::move(node), parseToken(depth));
        }

        return node;
    }

    std::shared_ptr<const Node> parseToken(std::size_t depth) {
        checkDepth(depth);

        if (current == u'!') {
            advance();
            auto child = parseToken(depth + 1);

            if (child->type == Type::CONSTANT) {
                auto node = make(Type::CONSTANT);
                node->value = !child->value;

                return node;
            }

            if (child->type == Type::NOT) {
                return child->left;
            }

            auto node = make(Type::NOT);
            node->left = std::move(child);
            node->filterKind = node->left->filterKind == FilterKind::RECORD_ONLY  ? FilterKind::RECORD_ONLY
                               : node->left->filterKind == FilterKind::SYMBOL_SET ? FilterKind::COFINITE_SET
                               : node->left->symbolOnly()                         ? FilterKind::SYMBOL_OTHER
                                                                                  : FilterKind::OTHER;
            node->dynamic = node->left->dynamic;
            node->depth = node->left->depth + 1;
            checkDepth(node->depth);

            return node;
        }

        if (current == u'(') {
            advance();
            auto node = parseOr(depth + 1);

            if (current != u')') {
                syntax(pos, "Missing closing parenthesis");
            }

            advance();

            return node;
        }

        std::size_t brackets = 0;
        std::size_t braces = 0;
        std::u16string token;

        while ((current != 0 && (brackets > 0 || braces > 0)) || !special(current)) {
            if (current == u'[') {
                ++brackets;
            } else if (current == u']') {
                if (brackets == 0) {
                    syntax(pos, "Extra closing bracket");
                }

                --brackets;
            }

            if (current == u'\\' && pos + 1 < text.size()) {
                const auto quoted = text[pos + 1];

                if (quoted == u'Q') {
                    pos += 2;

                    while (pos + 1 < text.size() && !(text[pos] == u'\\' && text[pos + 1] == u'E')) {
                        token.push_back(text[pos++]);
                    }

                    if (pos + 1 >= text.size()) {
                        syntax(pos, "Unterminated block quote");
                    }
                } else {
                    token.push_back(u'\\');
                    token.push_back(quoted);
                }

                pos += 2;
                scan();
                continue;
            }

            if (brackets == 0) {
                if (current == u'{') {
                    ++braces;
                } else if (current == u'}') {
                    if (braces == 0) {
                        syntax(pos, "Extra closing brace");
                    }

                    --braces;
                }
            }

            if (brackets > 0 && keyword) {
                token.append(text.substr(pos, next - pos));
            } else {
                token.push_back(current);
            }

            advance();
        }

        if (brackets > 0 || braces > 0 || token.empty()) {
            syntax(pos, "Missing filter pattern or closing bracket/brace");
        }

        return leaf(token);
    }

    static std::uint32_t decimal(std::u16string_view value, std::size_t offset) {
        if (value.empty()) {
            syntax(offset, "Missing hash integer");
        }

        std::uint32_t result = 0;
        constexpr auto MAX_JAVA_INT = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

        for (const auto c : value) {
            if (!digit(c) || result > (MAX_JAVA_INT - (c - u'0')) / 10U) {
                syntax(offset, "Invalid hash integer");
            }

            result = result * 10U + (c - u'0');
        }

        return result;
    }

    std::shared_ptr<const Node> leaf(std::u16string_view token) {
        constexpr std::pair<std::u16string_view, Type> BUILTINS[]{
            {u"feed", Type::FEED},           {u"compfeed", Type::COMPFEED},   {u"regfeed", Type::REGFEED},
            {u"chartdata", Type::CHARTDATA}, {u"convrates", Type::CONVRATES}, {u"nwc", Type::NWC}};

        for (const auto &[name, type] : BUILTINS) {
            if (token == name) {
                return make(type);
            }
        }

        if (token.starts_with(u"hash")) {
            const auto separator = token.find(u"of", 4);

            if (separator == std::u16string_view::npos) {
                syntax(pos, "Invalid hash filter");
            }

            const auto index = decimal(token.substr(4, separator - 4), pos);
            const auto count = decimal(token.substr(separator + 2), pos);

            if (count < 2 || !std::has_single_bit(count) || index >= count) {
                syntax(pos, "Hash count must be a power of two >= 2 and index must be smaller");
            }

            auto node = make(Type::HASH);
            node->hashIndex = index;
            node->hashShift = 32U - std::countr_zero(count);

            return node;
        }

        if (token.starts_with(u"range")) {
            if (!token.starts_with(u"range-") || !token.ends_with(u'-')) {
                syntax(pos, "Invalid range filter");
            }

            const auto separator = token.find(u'-', 6);

            if (separator == std::u16string_view::npos) {
                syntax(pos, "Missing range separator");
            }

            const auto lower = token.substr(6, separator - 6);
            const auto upper = token.substr(separator + 1, token.size() - separator - 2);

            if (lower.size() > 8 || upper.size() > 8 || !std::ranges::all_of(lower, dictionary) ||
                !std::ranges::all_of(upper, dictionary) || (!lower.empty() && !upper.empty() && lower >= upper)) {
                syntax(pos, "Invalid range boundaries");
            }

            if (lower.empty() && upper.empty()) {
                return make(Type::CONSTANT);
            }

            auto node = make(Type::RANGE);
            node->lower = lower;
            node->upper = upper;

            return node;
        }

        constexpr std::pair<std::u16string_view, std::u16string_view> FAMILIES[]{
            {u"bs", u"[^/.=]*"}, {u"fut", u"/*"},       {u"opt", u".*"},       {u"spread", u"=*"},
            {u"cs", u"[^.]*"},   {u"csopt", u".[^/]*"}, {u"bsopt", u".[^/]*"}, {u"futopt", u"./*"},
            {u"ind", u"$*"},     {u"prod", u""},        {u"fx", u""}};

        for (const auto &[family, pattern] : FAMILIES) {
            const bool symbolOnly = token.size() == family.size() + 6 && token.starts_with(family) &&
                                    token.substr(family.size()) == u"symbol";

            if (token != family && !symbolOnly) {
                continue;
            }

            auto node = make(family == u"prod" ? Type::PRODUCT : family == u"fx" ? Type::FOREX : Type::PATTERN);

            if (node->type == Type::PATTERN) {
                node->pattern.emplace(pattern, limits.patternPositions);
            }

            if (!symbolOnly) {
                return combine(Type::AND, std::move(node), make(Type::FEED));
            }

            return node;
        }

        if (token.starts_with(u"ipf[")) {
            (void)parseFilterIpfConfig(token, limits.textUnits);

            if (referenced) {
                referenced->emplace_back(token);

                return make(Type::CONSTANT);
            }

            const auto found = std::ranges::find(resources, token, &FilterSymbolResource::expression);

            if (found == resources.end() || !found->symbols) {
                throw FilterExpressionError(Kind::DYNAMIC_CONTEXT_REQUIRED, pos,
                                            "IPF filter requires a configured explicit resource context");
            }

            auto node = make(Type::IPF);
            node->symbols = found->symbols;
            node->resourceExpression = token;
            node->dynamic = found->dynamic;

            return node;
        }

        const bool record = token.starts_with(u':');

        if (record) {
            token.remove_prefix(1);
        }

        if (token.empty() || token == u"*") {
            return make(Type::CONSTANT);
        }

        auto node = make(record ? Type::RECORD : Type::PATTERN);
        node->pattern.emplace(token, limits.patternPositions);

        if (!record) {
            if (const auto finite = node->pattern->finiteSize()) {
                node->filterKind = FilterKind::SYMBOL_SET;
                node->finiteBound = *finite;
            }
        }

        return node;
    }
};

FilterExpression::FilterExpression(std::shared_ptr<const Node> root) : root_(std::move(root)) {
}

FilterExpression FilterExpression::compile(std::u16string_view text, FilterExpressionLimits limits,
                                           std::span<const FilterSymbolResource> resources,
                                           std::size_t *sharedNormalizationWork) {
    if (text.size() > limits.textUnits) {
        throw FilterExpressionError(Kind::RESOURCE_LIMIT, 0, "Expression text capacity exhausted");
    }

    if (sharedNormalizationWork) {
        limits.normalizationWork = std::min(limits.normalizationWork, *sharedNormalizationWork);
    }

    Node::Parser parser{text, limits, resources};

    struct AttemptedWorkCharge {
        std::size_t *shared;
        std::size_t initial;
        const std::size_t &remaining;

        ~AttemptedWorkCharge() {
            if (shared) {
                *shared -= initial - remaining;
            }
        }
    } charge{sharedNormalizationWork, limits.normalizationWork, parser.remainingNormalizationWork};

    parser.pos = parser.skipSpaces(0);
    parser.scan();

    if (text.empty()) {
        return FilterExpression(parser.make(Node::Type::CONSTANT));
    }

    auto root = parser.parseOr(1);

    if (parser.current != 0) {
        syntax(parser.pos, "Unexpected expression token");
    }

    return FilterExpression(std::move(root));
}

bool FilterExpression::usesResource(std::u16string_view expression) const noexcept {
    return root_->usesResource(expression);
}

std::vector<std::u16string> FilterExpression::referencedResources(std::u16string_view text,
                                                                  FilterExpressionLimits limits) {
    if (text.size() > limits.textUnits) {
        throw FilterExpressionError(Kind::RESOURCE_LIMIT, 0, "Expression text capacity exhausted");
    }

    std::vector<std::u16string> result;
    Node::Parser parser{text, limits, {}, &result};
    parser.pos = parser.skipSpaces(0);
    parser.scan();

    if (!text.empty()) {
        (void)parser.parseOr(1);

        if (parser.current != 0) {
            syntax(parser.pos, "Unexpected expression token");
        }
    }

    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}

bool FilterExpression::isDynamic() const noexcept {
    return root_->dynamic;
}

bool FilterExpression::accepts(std::u16string_view record, std::u16string_view symbol, bool wildcard) const noexcept {
    return root_->accepts(record, symbol, wildcard);
}
} // namespace org::ttldtor::qdcxx::api::detail
