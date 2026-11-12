// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/order.hpp"

namespace org::ttldtor::qdcxx::events {
/** Nuam matching-engine order type; reserved codes map to UNDEFINED. */
enum class NuamOrderType { UNDEFINED, LIMIT, MARKET, MARKET_TO_LIMIT, BEST_ORDER, IMBALANCE };
/** Nuam validity-period type; reserved codes map to UNDEFINED. */
enum class NuamTimeInForceType {
    UNDEFINED,
    REST_OF_DAY,
    GTC,
    IMMEDIATE_OR_CANCEL,
    FILL_OR_KILL,
    GOOD_TIL_SESSION,
    NUMBER_OF_DAYS
};

/** Independently owned Nuam order and matching-engine fields. */
struct NuamOrder : Order {
    std::int32_t actorId = 0;                      ///< Actor identifier.
    std::int32_t participantId = 0;                ///< Participant identifier.
    std::int32_t submitterId = 0;                  ///< Submitter identifier.
    std::int32_t onBehalfOfSubmitterId = 0;        ///< On-behalf-of submitter identifier.
    std::optional<std::u16string> clientOrderId;   ///< Owned nullable client order ID.
    std::optional<std::u16string> customerAccount; ///< Owned nullable account.
    std::optional<std::u16string> customerInfo;    ///< Owned nullable customer information.
    std::optional<std::u16string> exchangeInfo;    ///< Owned nullable exchange information.
    std::int32_t timeInForceData = 0;              ///< Additional validity-period data.
    std::int32_t triggerOrderBookId = 0;           ///< Trigger order book identifier.
    core::WideDecimal triggerPrice{0};             ///< Trigger price, NaN when appropriate.
    std::int32_t triggerSessionType = 0;           ///< Trigger session type.
    core::WideDecimal orderQuantity{0};            ///< Original order quantity.
    core::WideDecimal displayQuantity{0};          ///< Display quantity.
    core::WideDecimal refreshQuantity{0};          ///< Refresh quantity.
    core::WideDecimal leavesQuantity{0};           ///< Remaining quantity.
    core::WideDecimal matchedQuantity{0};          ///< Matched quantity.
    std::int32_t nuamFlags = 0;                    ///< Raw Nuam flags, including reserved bits.
    std::optional<std::array<detail::BusinessNumericSnapshot, 6>>
        nuamNumericSnapshot{}; ///< Original business doubles and validity keys.

    /** @return Matching-engine actor identifier. */
    [[nodiscard]] std::int32_t getActorId() const noexcept {
        return actorId;
    }

    /** @return Matching-engine participant identifier. */
    [[nodiscard]] std::int32_t getParticipantId() const noexcept {
        return participantId;
    }

    /** @return Matching-engine submitter identifier. */
    [[nodiscard]] std::int32_t getSubmitterId() const noexcept {
        return submitterId;
    }

    /** @return Identifier of the submitter represented by this order. */
    [[nodiscard]] std::int32_t getOnBehalfOfSubmitterId() const noexcept {
        return onBehalfOfSubmitterId;
    }

    /** @return Borrowed nullable client order ID while this event remains unmodified and alive. */
    [[nodiscard]] const std::optional<std::u16string> &getClientOrderId() const noexcept {
        return clientOrderId;
    }

    /** @return Borrowed nullable account while this event remains unmodified and alive. */
    [[nodiscard]] const std::optional<std::u16string> &getCustomerAccount() const noexcept {
        return customerAccount;
    }

    /** @return Borrowed nullable customer information while this event remains unmodified and alive. */
    [[nodiscard]] const std::optional<std::u16string> &getCustomerInfo() const noexcept {
        return customerInfo;
    }

    /** @return Borrowed nullable exchange information while this event remains unmodified and alive. */
    [[nodiscard]] const std::optional<std::u16string> &getExchangeInfo() const noexcept {
        return exchangeInfo;
    }

    /** @return Additional validity-period data. */
    [[nodiscard]] std::int32_t getTimeInForceData() const noexcept {
        return timeInForceData;
    }

    /** @return Trigger order book identifier. */
    [[nodiscard]] std::int32_t getTriggerOrderBookId() const noexcept {
        return triggerOrderBookId;
    }

    /** @return Trigger session type. */
    [[nodiscard]] std::int32_t getTriggerSessionType() const noexcept {
        return triggerSessionType;
    }

    /** @return Order type decoded from the low four Nuam flag bits. */
    [[nodiscard]] NuamOrderType getOrderType() const noexcept;
    /** @return Validity-period type decoded from Nuam flag bits 4..7. */
    [[nodiscard]] NuamTimeInForceType getTimeInForce() const noexcept;
    /** @return Source business trigger price while its raw value remains unchanged. */
    [[nodiscard]] double getTriggerPrice() const noexcept;
    /** @return Source business order quantity while its raw value remains unchanged. */
    [[nodiscard]] double getOrderQuantity() const noexcept;
    /** @return Source business display quantity while its raw value remains unchanged. */
    [[nodiscard]] double getDisplayQuantity() const noexcept;
    /** @return Source business refresh quantity while its raw value remains unchanged. */
    [[nodiscard]] double getRefreshQuantity() const noexcept;
    /** @return Source business remaining quantity while its raw value remains unchanged. */
    [[nodiscard]] double getLeavesQuantity() const noexcept;
    /** @return Source business matched quantity while its raw value remains unchanged. */
    [[nodiscard]] double getMatchedQuantity() const noexcept;
};

/** Construct the default source-neutral NuamOrder schema.
 * @param limits Finite complete schema allowances.
 * @return Owned schema or construction refusal.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultNuamOrderSchema(core::SchemaLimits limits);

/** Immutable Nuam order mapping with explicit indexed source identity. */
class PreparedNuamOrder {
    EventMapping mapping_;
    std::int32_t sourceId_;
    PreparedNuamOrder(EventMapping mapping, std::int32_t sourceId) noexcept;

    public:
    /** Prepare one complete bounded record mapping.
     * @param schema Validated source schema borrowed during the call.
     * @param localId Exact source record identity.
     * @param limits Finite mapping allowances.
     * @return Independently owned mapping or typed refusal.
     */
    [[nodiscard]] static std::expected<PreparedNuamOrder, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed record context, valid while this mapper is alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Exact indexed source derived from the record name. */
    [[nodiscard]] std::int32_t sourceId() const noexcept;
    /** Map complete owning input to an independent event.
     * @param symbol Source symbol borrowed during the call.
     * @param values Complete context-tagged fields.
     * @param limits Finite owned text bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed transaction flags.
     * @return Owned event or typed refusal.
     */
    [[nodiscard]] std::expected<NuamOrder, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                             EventValueLimits limits, std::int64_t eventTime = 0,
                                                             std::int32_t eventFlags = 0) const;
    /** Map complete borrowed input without retaining it.
     * @param symbol Source symbol borrowed during the call.
     * @param values Complete context-tagged fields alive through return.
     * @param limits Finite owned text bounds.
     * @param eventTime Independent FIELD milliseconds.
     * @param eventFlags Indexed transaction flags.
     * @return Owned event or typed refusal.
     */
    [[nodiscard]] std::expected<NuamOrder, MappingError> mapView(std::u16string_view symbol,
                                                                 BorrowedRecordValueView values,
                                                                 EventValueLimits limits, std::int64_t eventTime = 0,
                                                                 std::int32_t eventFlags = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
