// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "stream_client.hpp"

#include <iosfwd>

namespace org::ttldtor::qdcxx::examples {
/** Run the small exact-ALPHA generic callback example against one controlled local TCP peer.
 * No schema is inferred: supply a validated ordinary record at local ID zero. All generic fields
 * remain available to the callback. This example prints only identity/count coordinates, not a tape.
 * @param bootstrap Main-thread process owner, created before any application threads.
 * @param schema Caller-supplied validated schema, copied with provenance preserved.
 * @param port Explicit controlled IPv4 loopback peer port.
 * @param output Borrowed synchronous sink, preserved until return; failures become callback failure.
 * @return Separate application/local/native evidence after explicit retirement, without peer ACK claims.
 */
[[nodiscard]] StreamResult runSubscriptionExample(transport::RuntimeBootstrap &bootstrap,
                                                  const core::LocalSchema &schema, std::uint16_t port,
                                                  std::ostream &output);
} // namespace org::ttldtor::qdcxx::examples
