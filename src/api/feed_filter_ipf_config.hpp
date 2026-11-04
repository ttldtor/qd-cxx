// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::api::detail {
/** Parsed explicit IPF factory configuration; secrets are never diagnostic text. */
struct FilterIpfConfig {
    std::string source;
    std::optional<std::string> user;
    std::optional<std::string> password;
    std::chrono::milliseconds update{0};
    std::optional<std::u16string> schedule;
};

/** Parse QD bracket/parenthesis properties and source settings under an explicit UTF-16 bound. */
[[nodiscard]] FilterIpfConfig parseFilterIpfConfig(std::u16string_view expression, std::size_t maximumUnits);
} // namespace org::ttldtor::qdcxx::api::detail
