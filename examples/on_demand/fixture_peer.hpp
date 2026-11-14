// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "recovery_test_peer.hpp"

#include <memory>

/** Start the finite synthetic OnDemand peer used by this example.
 * @return An owned loopback peer; port zero indicates startup failure.
 */
[[nodiscard]] std::unique_ptr<org::ttldtor::qdcxx::test::OwnedTcpPeer> startFixturePeer();
