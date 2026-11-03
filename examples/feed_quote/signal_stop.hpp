// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <memory>
#include <stop_token>

namespace feed_quote {
/** Application-owned console interruption bridge; the SDK installs no process signal policy.
 * Construct after ApiRuntime bootstrap and before any application/SDK worker starts. Keep it installed
 * until runtime quiescence, then destroy on its creating thread before ApiRuntime destruction.
 * POSIX saves/restores SIGINT/SIGTERM/SIGPIPE dispositions and the creator's signal mask.
 * Windows handles only Ctrl+C/Break and restores this process's prior handler chain on removal.
 */
class SignalStop final {
    struct Impl;
    std::unique_ptr<Impl> impl_; ///< Owns the signal reader and native resources through join/restoration.

    public:
    /** Install scoped interruption policy and start one application reader thread.
     * @param source Copy of application cancellation source; request_stop runs only on the ordinary reader.
     * @throws std::system_error If native installation or reader creation fails; partial setup is rolled back.
     */
    explicit SignalStop(std::stop_source source);
    /** Wake/join the owned reader and restore prior process policy; native teardown refusal terminates. */
    ~SignalStop();
    SignalStop(const SignalStop &) = delete;
    SignalStop &operator=(const SignalStop &) = delete;
    /** Distinguish an actual OS interruption from application cancellation or the synthetic teardown wake.
     * @return True after a handled SIGINT/SIGTERM or Ctrl+C/Break.
     */
    [[nodiscard]] bool interrupted() const noexcept;
    /** Inspect a reader-side native wait failure, independently from ordinary cancellation.
     * @return True if the native wait failed and requested application cancellation.
     */
    [[nodiscard]] bool failed() const noexcept;
};
} // namespace feed_quote
