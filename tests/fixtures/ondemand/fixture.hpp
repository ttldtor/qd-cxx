// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <string_view>

namespace org::ttldtor::qdcxx::test::ondemand {
// Synthetic upstream QD v2 request and responses, reproduced by OnDemandFixtureOracle.
inline constexpr std::string_view REQUEST =
    "8081020708636f6e7472616374000475736572000a65787069726174696f6e13393232333337323033363835343737353830"
    "37066469676573742000000000000000000000000000000000000000000000000000000000000000000564656c6179013004"
    "74696d650d31373037373438323035303030046b65797307044d4f434b0051";
inline constexpr std::string_view RESPONSE_750MS =
    "31020106626c6f636b73272600044d4f434b0051f065ca2b6880419388ff058051c04c9b0a05805187da1482ee22864080a0";
inline constexpr std::string_view RESPONSE_2000MS =
    "31020106626c6f636b73272600044d4f434b0051f065ca2b6880419388ff058051c04c9b0a05805187da1487d022864080a0";

} // namespace org::ttldtor::qdcxx::test::ondemand
