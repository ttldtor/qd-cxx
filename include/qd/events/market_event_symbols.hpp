// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events::market_symbols {
/** Supported regional code units, in ASCII order; codes do not identify a venue by themselves. */
inline constexpr std::u16string_view SUPPORTED_EXCHANGES =
    u"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
/** Default regional code units when no override is selected. */
inline constexpr std::u16string_view DEFAULT_EXCHANGES = u"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/** Inspect the syntactic regional suffix before a recognized attribute block.
 * @param symbol Exact UTF-16 borrowed for this call; nullopt represents Java null.
 * @return True if an ampersand precedes exactly one final code unit; codes are not validated.
 */
[[nodiscard]] bool hasExchangeCode(std::optional<std::u16string_view> symbol) noexcept;

/** Read the syntactic regional code without validating its membership in SUPPORTED_EXCHANGES.
 * @param symbol Exact UTF-16 borrowed for this call; nullopt represents Java null.
 * @return The code unit, or zero for null/composite symbols; use hasExchangeCode to distinguish an explicit NUL.
 */
[[nodiscard]] char16_t getExchangeCode(std::optional<std::u16string_view> symbol) noexcept;

/** Copy the base symbol, removing the regional suffix and recognized attribute block.
 * @param symbol Exact UTF-16 borrowed for this call; no storage is retained.
 * @return Independently owned base, or nullopt for null input.
 * @throws std::bad_alloc Allocation fails.
 * @throws std::length_error The owned result exceeds the string maximum size.
 */
[[nodiscard]] std::optional<std::u16string> getBaseSymbol(std::optional<std::u16string_view> symbol);

/** Replace/remove the regional code, preserving the attribute block verbatim.
 * @param symbol Exact UTF-16 borrowed for this call; no storage is retained.
 * @param exchangeCode Any UTF-16 unit; zero removes the suffix. No code validation is performed.
 * @return Independently owned symbol; null input yields nullopt for zero, otherwise an ampersand and code.
 * @throws std::bad_alloc Allocation fails.
 * @throws std::length_error The owned result exceeds the string maximum size.
 */
[[nodiscard]] std::optional<std::u16string> changeExchangeCode(std::optional<std::u16string_view> symbol,
                                                               char16_t exchangeCode);

/** Replace the base, preserving the existing regional code and attribute block verbatim.
 * @param symbol Exact UTF-16 borrowed for this call; no storage is retained.
 * @param baseSymbol Replacement borrowed for this call. Null becomes literal "null" when concatenating a suffix.
 * @return Independently owned result; without an old suffix, returns an owned copy of baseSymbol, including nullopt.
 * @throws std::bad_alloc Allocation fails.
 * @throws std::length_error The owned result exceeds the string maximum size.
 */
[[nodiscard]] std::optional<std::u16string> changeBaseSymbol(std::optional<std::u16string_view> symbol,
                                                             std::optional<std::u16string_view> baseSymbol);
} // namespace org::ttldtor::qdcxx::events::market_symbols
