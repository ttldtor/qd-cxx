// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <new>
#include <optional>
#include <utility>

namespace org::ttldtor::qdcxx::api {

namespace detail {
class ModelSourceFence;
} // namespace detail

/** Qualification of one model's committed cut relative to an explicit source epoch. */
enum class ModelSourceStatus {
    UNTRACKED,  ///< Legacy mode without source qualification.
    RECOVERING, ///< The fresh epoch accepts input and has no confirmed complete replay.
    CURRENT,    ///< The caller confirmed replay and the model has a committed cut for this epoch.
    GAP,        ///< Input is fenced. An optionally retained cut is historical.
    CLOSED      ///< Terminal input fence. An optionally retained cut is historical.
};

/** Model data to keep across a gap or close. Pending input always retires. */
enum class ModelRetention {
    DISCARD,         ///< Drop the model's committed cut.
    RETAIN_COMPLETED ///< Preserve the last complete cut for historical inspection.
};

/** Refusal from an epoch lifecycle operation. */
enum class ModelSourceError {
    INVALID_EPOCH,     ///< The proposed ordinal is zero or does not advance the previous one.
    STALE_EPOCH,       ///< The token does not belong to this model's active epoch.
    SOURCE_INACTIVE,   ///< The epoch has a gap or is terminal.
    STALE_SELECTION,   ///< Replay evidence names a different range selection revision.
    REPLAY_INCOMPLETE, ///< The model still has unfinished input or no active selection.
    INVALID_RETENTION, ///< Unrecognized retention policy.
    ALLOCATION_FAILURE ///< Allocation of the non-reused epoch marker failed.
};

/** Pure owning token for one activation. Holding a token never retains the model or Feed.
 * Capture it with the input batch at its originating boundary. Its identity is distinct even
 * between models with the same numeric source ordinal.
 */
class ModelSourceEpoch final {
    std::shared_ptr<const std::uint64_t> marker_;
    friend class detail::ModelSourceFence;

    /** Adopt an allocated immutable activation marker.
     * @param marker Unique marker for this activation.
     */
    explicit ModelSourceEpoch(std::shared_ptr<const std::uint64_t> marker) noexcept : marker_(std::move(marker)) {
    }

    public:
    /** Construct an invalid token. It admits no input. */
    ModelSourceEpoch() = default;

    /** @return Source ordinal, or zero for an invalid token. */
    [[nodiscard]] std::uint64_t ordinal() const noexcept {
        return marker_ ? *marker_ : 0;
    }

    /** @param other Token to compare.
     * @return True for the same activation marker, or for two invalid tokens.
     */
    [[nodiscard]] bool operator==(const ModelSourceEpoch &other) const noexcept = default;
};

/** Detached source qualification. It does not change as the model progresses. */
struct ModelSourceState {
    ModelSourceStatus status = ModelSourceStatus::UNTRACKED; ///< Exact status at this serialized cut.
    std::uint64_t epochOrdinal = 0; ///< Last activated source ordinal, or zero before activation.
};

/** Owning model data and its source qualification at one serialized observation.
 * @tparam Snapshot Owning snapshot type of the particular model.
 */
template <class Snapshot> struct ModelSourceSnapshot {
    ModelSourceState source; ///< Detached qualification, including historical cuts.
    Snapshot snapshot;       ///< Detached data whose lifetime is independent of the owner.
};

namespace detail {

/** Allocation-free transitions after a fresh marker has been prepared. Calls are serialized by the model. */
class ModelSourceFence {
    ModelSourceState state_;
    ModelSourceEpoch epoch_;

    /** @param retention Requested historical cut policy.
     * @return True for a supported retention option.
     */
    [[nodiscard]] static bool supports(ModelRetention retention) noexcept {
        return retention == ModelRetention::DISCARD || retention == ModelRetention::RETAIN_COMPLETED;
    }

    public:
    /** Construct without source qualification. */
    ModelSourceFence() = default;
    ModelSourceFence(const ModelSourceFence &) = delete;
    ModelSourceFence &operator=(const ModelSourceFence &) = delete;

    /** Transfer source authority and fence the old owner if it had tracked input.
     * @param other Previous model's fence.
     */
    ModelSourceFence(ModelSourceFence &&other) noexcept {
        *this = std::move(other);
    }

    /** Transfer source authority without reviving tokens previously used by this owner.
     * @param other Previous model's fence, or this fence for a no-op.
     * @return This fence with the transferred authority.
     */
    ModelSourceFence &operator=(ModelSourceFence &&other) noexcept {
        if (this != &other) {
            state_ = other.state_;
            epoch_ = std::move(other.epoch_);
            other.invalidateInput();
        }

        return *this;
    }

    /** @return Exact detached qualification. */
    [[nodiscard]] ModelSourceState state() const noexcept {
        return state_;
    }

    /** @return Whether the old untagged input API remains enabled. */
    [[nodiscard]] bool untracked() const noexcept {
        return state_.status == ModelSourceStatus::UNTRACKED;
    }

    /** @return Whether a plain read can return an unqualified or current cut. */
    [[nodiscard]] bool readable() const noexcept {
        return untracked() || state_.status == ModelSourceStatus::CURRENT;
    }

    /** @return Whether the source has terminally closed. */
    [[nodiscard]] bool closed() const noexcept {
        return state_.status == ModelSourceStatus::CLOSED;
    }

    /** Allocate and publish a distinct token, leaving the previous fence unchanged on refusal.
     * @param ordinal Strictly increasing positive ordinal at the caller's source boundary.
     * @return The new activation token or a typed refusal.
     */
    [[nodiscard]] std::expected<ModelSourceEpoch, ModelSourceError> begin(std::uint64_t ordinal) noexcept {
        if (closed()) {
            return std::unexpected(ModelSourceError::SOURCE_INACTIVE);
        }

        if (ordinal == 0 || ordinal <= state_.epochOrdinal) {
            return std::unexpected(ModelSourceError::INVALID_EPOCH);
        }

        try {
            ModelSourceEpoch next(std::make_shared<const std::uint64_t>(ordinal));
            epoch_ = next;
            state_ = {ModelSourceStatus::RECOVERING, ordinal};

            return next;
        } catch (const std::bad_alloc &) {
            return std::unexpected(ModelSourceError::ALLOCATION_FAILURE);
        }
    }

    /** Check a captured input token before admission.
     * @param epoch Token captured with the originating batch.
     * @return No value for a receiving matching token, or the exact source refusal.
     */
    [[nodiscard]] std::optional<ModelSourceError> inputError(const ModelSourceEpoch &epoch) const noexcept {
        if (epoch.ordinal() == 0 || epoch != epoch_) {
            return ModelSourceError::STALE_EPOCH;
        }

        if (state_.status != ModelSourceStatus::RECOVERING && state_.status != ModelSourceStatus::CURRENT) {
            return ModelSourceError::SOURCE_INACTIVE;
        }

        return std::nullopt;
    }

    /** Qualify completed replay.
     * @param epoch Token for the matching receiving source.
     * @param incomplete The selected model still has unfinished input or no active selection.
     * @return Success or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> confirm(const ModelSourceEpoch &epoch,
                                                                bool incomplete) noexcept {
        if (const auto error = inputError(epoch)) {
            return std::unexpected(*error);
        }

        if (incomplete) {
            return std::unexpected(ModelSourceError::REPLAY_INCOMPLETE);
        }

        state_.status = ModelSourceStatus::CURRENT;

        return {};
    }

    /** Fence input for a matching gap or close before the model retires data.
     * @param epoch Token for the affected activation.
     * @param retention Validated discard or historical retention choice.
     * @param terminal True for close, false for gap.
     * @return Success, including a repeated close, or a typed refusal.
     */
    [[nodiscard]] std::expected<void, ModelSourceError> stop(const ModelSourceEpoch &epoch, ModelRetention retention,
                                                             bool terminal) noexcept {
        if (!supports(retention)) {
            return std::unexpected(ModelSourceError::INVALID_RETENTION);
        }

        if (epoch.ordinal() == 0 || epoch != epoch_) {
            return std::unexpected(ModelSourceError::STALE_EPOCH);
        }

        if (closed() && !terminal) {
            return std::unexpected(ModelSourceError::SOURCE_INACTIVE);
        }

        state_.status = terminal ? ModelSourceStatus::CLOSED : ModelSourceStatus::GAP;

        return {};
    }

    /** Fence queued input after a data-only reset or authority transfer. */
    void invalidateInput() noexcept {
        if (!untracked() && !closed()) {
            state_.status = ModelSourceStatus::GAP;
        }
    }

    /** A new range needs new replay evidence while it keeps the same source epoch. */
    void invalidateSelection() noexcept {
        if (state_.status == ModelSourceStatus::CURRENT) {
            state_.status = ModelSourceStatus::RECOVERING;
        }
    }
};

} // namespace detail
} // namespace org::ttldtor::qdcxx::api
