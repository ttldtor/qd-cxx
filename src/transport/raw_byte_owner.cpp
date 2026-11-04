// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "raw_byte_owner.hpp"

#include <exception>
#include <limits>
#include <new>
#include <utility>

namespace org::ttldtor::qdcxx::transport::detail {

RawByteOwner::RawByteOwner(BootstrapLease &&bootstrap, RawByteLimits limits)
    : bootstrap_(std::move(bootstrap)), bytes_(limits) {
}

RawByteOwner::~RawByteOwner() {
    if (owner_ != std::this_thread::get_id() || busy_) {
        std::terminate();
    }

    core_.reset();
}

std::optional<TcpDiagnostic> RawByteOwner::rejection(bool allowBusy) const noexcept {
    if (owner_ != std::this_thread::get_id()) {
        return TcpDiagnostic{TcpError::WRONG_THREAD, "raw-owner"};
    }

    if (busy_ && !allowBusy) {
        return TcpDiagnostic{TcpError::INVALID_STATE, "raw-busy"};
    }

    return std::nullopt;
}

std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
RawByteOwner::create(RuntimeBootstrap &bootstrap, TcpOptions options, RawByteLimits limits, protocol::LiveWork &work,
                     std::unique_ptr<ByteEngine> engine) {
    return createImpl(&bootstrap, nullptr, options, limits, work, std::move(engine));
}

std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
RawByteOwner::create(const BootstrapLease &bootstrap, TcpOptions options, RawByteLimits limits,
                     protocol::LiveWork &work, std::unique_ptr<ByteEngine> engine) {
    return createImpl(nullptr, &bootstrap, options, limits, work, std::move(engine));
}

std::expected<std::unique_ptr<RawByteOwner>, TcpDiagnostic>
RawByteOwner::createImpl(RuntimeBootstrap *facade, const BootstrapLease *lease, TcpOptions options,
                         RawByteLimits limits, protocol::LiveWork &work, std::unique_ptr<ByteEngine> engine) {
    if (work.operations == 0) {
        return std::unexpected(TcpDiagnostic{TcpError::WORK_LIMIT, "raw-create-attempt"});
    }

    --work.operations;

    if (limits.inputBytes == 0 || limits.outputBytes == 0) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_CONFIGURATION, "raw-capacities"});
    }

    constexpr auto MAXIMUM = std::numeric_limits<std::size_t>::max();
    std::size_t ownBytes = sizeof(RawByteOwner);

    if (limits.inputBytes > MAXIMUM - ownBytes) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-reservation"});
    }

    ownBytes += limits.inputBytes;

    if (limits.outputBytes > MAXIMUM - ownBytes) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-reservation"});
    }

    ownBytes += limits.outputBytes;

    if (ownBytes > options.limits.residentBytes) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-reservation"});
    }

    constexpr std::size_t PREPARATION_PASSES = 3; // Whole zeroed windows and construction/configuration visits.

    if (ownBytes > MAXIMUM / PREPARATION_PASSES || work.bytes < ownBytes * PREPARATION_PASSES) {
        return std::unexpected(TcpDiagnostic{TcpError::WORK_LIMIT, "raw-preparation-work"});
    }

    work.bytes -= ownBytes * PREPARATION_PASSES;
    auto outer = lease ? BootstrapLease::acquire(*lease) : BootstrapLease::acquire(*facade);

    if (!outer) {
        return std::unexpected(outer.error());
    }

    try {
        auto result = std::unique_ptr<RawByteOwner>(new RawByteOwner(std::move(*outer), limits));
        auto core = RuntimeCore::createRaw(result->bootstrap_.state(), result->bytes_, ownBytes, options, work,
                                           std::move(engine));

        if (!core) {
            return std::unexpected(core.error());
        }

        result->core_ = std::move(*core);

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(TcpDiagnostic{TcpError::ALLOCATION_FAILURE, "raw-storage"});
    }
}

std::expected<void, TcpDiagnostic> RawByteOwner::start() {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (closeRequested_) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_STATE, "raw-start-cutoff"});
    }

    return core_->start();
}

std::expected<TcpPollResult, TcpDiagnostic> RawByteOwner::poll(TcpWork &work, protocol::LiveWork &byteWork) {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (core_->terminal()) {
        return TcpPollResult{TcpPollStatus::CLOSED, std::nullopt};
    }

    busy_ = true;

    /** Restore the owner-turn guard even when a fallible native action returns early. */
    struct Turn {
        bool &busy; ///< Borrow valid through the enclosing serialized owner turn.

        ~Turn() {
            busy = false;
        }
    } turn{busy_};

    if (closeRequested_) {
        core_->outcome.aborted = true;
        core_->closeRequested = true;
    }

    return core_->poll(work, byteWork);
}

std::expected<TcpPollResult, TcpDiagnostic> RawByteOwner::drainOwnedInput(TcpWork &work, protocol::LiveWork &byteWork) {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (core_->engine || core_->outcome.primary.error != TcpError::READ_FAILURE) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_STATE, "raw-owned-prefix-state"});
    }

    if (closeRequested_ || core_->closeRequested || core_->terminal() || !bytes_.input().empty()) {
        return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
    }

    bool completedPrefix = false;
    std::size_t available = core_->inputEnd - core_->inputBegin;
#ifdef _WIN32
    completedPrefix = core_->readReady && core_->readCount != 0;

    if (completedPrefix) {
        if (available != 0 || core_->readCount > core_->options.limits.inputBytes) {
            return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-owned-prefix-count"});
        }

        available = core_->readCount;
    }

#endif

    if (available == 0) {
        return TcpPollResult{TcpPollStatus::IDLE, std::nullopt};
    }

    // Retain the former completed-read promotion + input-copy step charges, without polling context.
    const std::size_t nativeSteps = completedPrefix ? 2 : 1;
    const auto count = (std::min)(available, byteWork.bytes);

    if (work.steps < nativeSteps || byteWork.operations == 0 || count == 0) {
        return TcpPollResult{TcpPollStatus::YIELD, std::nullopt};
    }

    work.steps -= nativeSteps;
    --byteWork.operations;

    if (completedPrefix) {
        core_->inputBegin = 0;
        core_->inputEnd = core_->readCount;
        core_->readCount = 0;
        core_->readReady = false;
        core_->readErrorPending = static_cast<bool>(core_->readError);
    }

    const auto accepted = bytes_.acceptInput({core_->input.get() + core_->inputBegin, count});
    byteWork.bytes -= accepted;
    core_->inputBegin += accepted;

    if (core_->inputBegin == core_->inputEnd) {
        core_->inputBegin = core_->inputEnd = 0;
    }

    return TcpPollResult{accepted == 0 ? TcpPollStatus::YIELD : TcpPollStatus::PROGRESS, std::nullopt};
}

std::expected<void, TcpDiagnostic> RawByteOwner::write(std::span<const std::byte> bytes, protocol::LiveWork &work) {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (closeRequested_ || core_->closeRequested || core_->outcome.primary.error != TcpError::NONE ||
        core_->terminal()) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_STATE, "raw-output-cutoff"});
    }

    if (!bytes_.output().empty()) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-output-slot"});
    }

    if (bytes.size() > bytes_.outputCapacity()) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-output-capacity"});
    }

    if (work.operations == 0 || work.bytes < bytes.size()) {
        return std::unexpected(TcpDiagnostic{TcpError::WORK_LIMIT, "raw-output-work"});
    }

    --work.operations;
    work.bytes -= bytes.size();

    if (!bytes_.publishOutput(bytes)) {
        return std::unexpected(TcpDiagnostic{TcpError::RESOURCE_LIMIT, "raw-output-capacity"});
    }

    if (!bytes.empty() && !core_->setDeadline(core_->writeDeadline, core_->options.writeTimeout)) {
        return std::unexpected(core_->outcome.primary);
    }

    return {};
}

std::expected<std::span<const std::byte>, TcpDiagnostic> RawByteOwner::input() const noexcept {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    return bytes_.input();
}

std::expected<void, TcpDiagnostic> RawByteOwner::consumeInput(std::size_t bytes) noexcept {
    if (const auto error = rejection()) {
        return std::unexpected(*error);
    }

    if (!bytes_.consumeInput(bytes)) {
        return std::unexpected(TcpDiagnostic{TcpError::INVALID_STATE, "raw-input-prefix"});
    }

    return {};
}

std::expected<void, TcpDiagnostic> RawByteOwner::requestClose() noexcept {
    if (const auto error = rejection(true)) {
        return std::unexpected(*error);
    }

    if (!core_->terminal()) {
        closeRequested_ = true;
    }

    return {};
}

std::size_t RawByteOwner::pendingOutput() const noexcept {
    return bytes_.output().size();
}

RuntimeCore &RawByteOwner::core() noexcept {
    return *core_;
}

} // namespace org::ttldtor::qdcxx::transport::detail
