// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::api::detail {
/** Finite parser and retained expression bounds, independent of transport. */
struct FilterExpressionLimits {
    std::size_t textUnits = 4096;
    std::size_t nodes = 1024;
    std::size_t depth = 64;
    std::size_t patternPositions = 4096;
    std::size_t finiteSymbols = 100000;
    std::size_t normalizationWork = 100000000;
};

/** Detached syntax/resource refusal with an input UTF-16 offset. */
class FilterExpressionError final : public std::runtime_error {
    public:
    enum class Kind { SYNTAX, RESOURCE_LIMIT, WORK_LIMIT, DYNAMIC_CONTEXT_REQUIRED };
    Kind kind;
    std::size_t offset;
    FilterExpressionError(Kind kind, std::size_t offset, const char *message);
};

/** One immutable compiled QD pattern; positions contain ASCII buckets including non-ASCII 127. */
class FilterPattern final {
    using Bits = std::array<bool, 128>;
    std::vector<std::vector<Bits>> segments_;
    bool negated_ = false;

    public:
    explicit FilterPattern(std::u16string_view pattern, std::size_t maximumPositions);
    [[nodiscard]] bool matches(std::u16string_view text) const noexcept;
    [[nodiscard]] std::optional<std::size_t> finiteSize() const noexcept;
    [[nodiscard]] std::size_t positions() const noexcept;
    bool visitFinite(const std::function<bool(std::u16string_view)> &visitor, std::size_t &remainingSymbols,
                     std::size_t &remainingWork) const;
};

/** Exact explicitly configured source token and independently owned immutable symbol snapshot. */
struct FilterSymbolResource {
    std::u16string expression;
    std::shared_ptr<const std::vector<std::u16string>> symbols;
    bool dynamic = false;
    std::shared_ptr<const std::vector<std::u16string>> comparisonMetadata;
};

/** Finite immutable compiled expression evaluated on already resolved QD coordinates. */
class FilterExpression final {
    struct Node;
    std::shared_ptr<const Node> root_;
    explicit FilterExpression(std::shared_ptr<const Node> root);

    public:
    [[nodiscard]] static FilterExpression compile(std::u16string_view text, FilterExpressionLimits limits,
                                                  std::span<const FilterSymbolResource> resources = {},
                                                  std::size_t *sharedNormalizationWork = nullptr);
    [[nodiscard]] bool isDynamic() const noexcept;
    [[nodiscard]] bool usesResource(std::u16string_view expression) const noexcept;
    [[nodiscard]] static std::vector<std::u16string> referencedResources(std::u16string_view text,
                                                                         FilterExpressionLimits limits);
    [[nodiscard]] bool accepts(std::u16string_view record, std::u16string_view symbol, bool wildcard) const noexcept;
};
} // namespace org::ttldtor::qdcxx::api::detail
