// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "stream_client.hpp"

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::connect {

/** Explicit built-in schema; record inference and external loading are not performed. */
enum class SchemaProfile {
    QUOTE,  ///< Complete eight-field default Quote.
    PROFILE ///< Complete sixteen-field default Profile.
};
/** Selected exact generic representation or prepared standard-event summary. */
enum class OutputProfile {
    GENERIC, ///< Exact diagnostic generic rows with nullable payloads.
    QUOTE,   ///< Existing prepared Quote summary.
    PROFILE  ///< Existing prepared Profile summary.
};

/** Owned CLI settings resolved before application bootstrap or network activation. */
struct Configuration {
    std::string host;                              ///< Numeric IPv4/IPv6 literal; no resolver discovery.
    std::uint16_t port = 0;                        ///< Required numeric remote port.
    SchemaProfile schema = SchemaProfile::QUOTE;   ///< Full built-in Quote by default.
    OutputProfile output = OutputProfile::GENERIC; ///< Exact generic rows by default.
    std::vector<std::u16string> symbols;           ///< One to sixteen distinct ASCII selectors, at most 128 units each.
    std::chrono::milliseconds duration{5000};      ///< Positive finite admission interval, at most 600000 ms.
    std::optional<std::chrono::milliseconds> removeAfter; ///< Optional removal of the entire initial exact set.
    std::optional<std::size_t> maximumRecords; ///< Optional positive completed-record target, at most 1000000.
};

/** Parse native bounded working arguments without files, bootstrap, sockets or signal changes.
 * @param arguments Arguments after the executable name; help is handled separately by main.
 * @return Complete owned settings or a static error; partial arguments never activate work.
 */
[[nodiscard]] std::expected<Configuration, std::string_view>
parseArguments(std::span<const std::string_view> arguments);

/** Inspect built-in help without initializing any working subsystem.
 * @param russian Select Russian rather than authoritative English text.
 * @return Static complete native help, including limits, effects and exit codes.
 */
[[nodiscard]] std::string_view help(bool russian) noexcept;

} // namespace org::ttldtor::qdcxx::connect
