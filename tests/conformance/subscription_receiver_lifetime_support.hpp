// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/client/subscription_receiver.hpp"
#include "recovery_test_support.hpp"

namespace org::ttldtor::qdcxx::test::receiver_lifetime {
namespace r = recovery;
namespace d = client;
namespace c = core;
namespace m = mux;
namespace p = protocol;

/** Build an indexed generic record with an explicit independently prepared Profile projection.
 * @return Exact four-field immutable provenance; the business name is deliberately not Profile.
 */
inline c::LocalSchema schema() {
    const std::array<c::LocalRecord, 1> records{{{0,
                                                  u"R",
                                                  true,
                                                  {{8}, {8}, {8}, {10}},
                                                  {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"V", 2}, {u"Description", 3}},
                                                  c::IndexedTimePairInt32{0, 1}}}};
    auto result = c::LocalSchema::create(records, {16, 128, 256, 128, 16384}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Supply the complete outgoing schema mapping with an explicit distinct sender identity.
 * @return Fresh four-field blueprint matching schema().
 */
inline std::vector<p::OutgoingRecord> blueprint() {
    return {{71, 0, u"R", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"V", 8, 2}, {u"Description", 10, 3}}}};
}

/** Advertise every exact synthetic source field independently of its local mapper.
 * @return Complete type2 frame with directional sender41.
 */
inline std::vector<std::byte> description() {
    std::vector<std::byte> payload;
    r::number(payload, 41);
    r::text(payload, u"R");
    r::number(payload, 4);

    for (const auto name : {u"TimeHigh", u"TimeLow", u"V"}) {
        r::text(payload, name);
        r::number(payload, 8);
    }

    r::text(payload, u"Description");
    r::number(payload, 10);

    return r::frame(2, payload);
}

/** Encode three ordered records into one actual DATA envelope.
 * @return Native exact-symbol STREAM_DATA, without a diagnostic-only event constructor.
 */
inline std::vector<std::byte> data() {
    std::vector<std::byte> payload;
    constexpr std::u16string_view DESCRIPTION = u"owned-description";

    for (const auto value : {101, 102, 103}) {
        payload.push_back(std::byte{0xfd});
        r::number(payload, 1);
        payload.push_back(std::byte{'A'});
        r::number(payload, 41);
        r::number(payload, 0);
        r::number(payload, 1);
        r::number(payload, value);
        r::number(payload, static_cast<std::int64_t>(DESCRIPTION.size()));

        for (const auto unit : DESCRIPTION)
            payload.push_back(static_cast<std::byte>(unit)); // The synthetic CHAR_ARRAY text is ASCII.
    }

    return r::frame(15, payload);
}

/** Public-owner mutation performed inside an actual prepared callback. */
enum class OwnerAction { STOP, DESTROY, REPLACE };

/** Caller-owned observations surviving the actual final application callable release. */
struct Observation {
    std::size_t callbacks = 0;             ///< Actual whole-batch application calls.
    std::size_t batchSize = 0;             ///< Actual records in the one executing callback.
    bool stopAccepted = false;             ///< Actual callback-side public cutoff acceptance.
    bool projected = true;                 ///< Every lazy projection succeeded after the public mutation.
    bool noInventedTime = true;            ///< Indexed rawTime was not projected into business eventTime.
    m::OwnedMuxBatch retained;             ///< Detached immutable generic records, without the facade or plan.
    std::vector<events::Profile> profiles; ///< Independently owned output after actual source/adapter retirement.
    bool inside = false;                   ///< Actual application callback body has not returned.
    bool destroyed = false;                ///< The application callable's destructor has executed.
    bool premature = false;                ///< A destructor incorrectly ran before callback body completion.
};

/** Project after public-owner mutation and final external callable release, without an ownership cycle.
 * @tparam Recovery Existing public TCP or TLS recovery facade.
 */
template <typename Recovery> struct Consumer final : d::SubscriptionConsumer {
    std::optional<Recovery> &owner; ///< External sole public facade; its old poll retains the execution lease.
    Observation &observed;          ///< Caller-owned results, independent of callable lifetime.
    OwnerAction action;             ///< Explicit stop, destruction or replacement of the public facade.
    std::optional<Recovery> *replacement = nullptr; ///< Externally owned configured successor, only for REPLACE.
    std::shared_ptr<Consumer> *external = nullptr;  ///< Actual final application-held strong reference.

    /** Borrow only an externally owned public facade.
     * @param current Sole public facade used by the in-flight operation.
     * @param output Independent observations outliving this callable.
     * @param selected Explicit public-owner mutation.
     */
    Consumer(std::optional<Recovery> &current, Observation &output, OwnerAction selected)
        : owner(current), observed(output), action(selected) {
    }

    /** Record final callable destruction without retaining another reference to it. */
    ~Consumer() override {
        observed.destroyed = true;
        observed.premature = observed.inside;
    }

    /** Mutate the public owner before using the still-valid executing batch/plan.
     * @param batch Callback-only facade; no reference to it escapes.
     * @return Existing whole-batch action; the queue cutoff must prevent future calls.
     */
    m::BatchAction onBatch(const d::SubscriptionBatch &batch) override {
        observed.inside = true;
        ++observed.callbacks;
        observed.batchSize = batch.records().size();

        if (observed.callbacks != 1 || !owner) {
            observed.projected = false;
            observed.inside = false;

            return m::BatchAction::CONTINUE;
        }

        if (external)
            external->reset();

        observed.stopAccepted = owner->requestStop().has_value();

        if (action == OwnerAction::DESTROY) {
            owner.reset();
        } else if (action == OwnerAction::REPLACE) {
            if (replacement && *replacement) {
                *owner = std::move(**replacement);
                replacement->reset();
            } else {
                observed.projected = false;
            }
        }

        observed.retained = batch.retain();
        d::ReceiverProjectionWork work{10000, 100000, 1024 * 1024};

        for (std::size_t i = 0; i < batch.records().size(); ++i) {
            auto result = batch.project(i, work);

            if (!result || !std::holds_alternative<events::Profile>(result->value)) {
                observed.projected = false;
                continue;
            }

            observed.noInventedTime = observed.noInventedTime && !result->eventTimeMs;
            observed.profiles.push_back(std::get<events::Profile>(std::move(result->value)));
        }

        observed.inside = false;

        return m::BatchAction::CONTINUE;
    }
};

/** Prepare one real typed receiver and submit explicit exact membership.
 * @param queue Existing unbound STREAM source adopted later by recovery.
 * @param local Exact immutable provenance shared by source and mapper.
 * @param consumer Independently owned application callback.
 * @return Existing public handle; command completion remains an independent local fact.
 */
inline m::MuxHandle subscribe(m::MuxQueue &queue, const c::LocalSchema &local,
                              const std::shared_ptr<d::SubscriptionConsumer> &consumer) {
    const std::array<d::PreparedSubscriptionBinding, 1> bindings{
        {{0, d::PreparedEventKind::PROFILE, {64, 128, 32, 64, 128, 128, 8192, 100000}, {128, 256, 512}}}};
    d::ReceiverWork receiverWork{10000, 1000000};
    auto queueWork = r::queueWork();
    auto handle = d::openSubscriptionReceiver(
        queue, local, c::SubscriptionFamily::STREAM, c::SubscriptionSelectorPolicy::EXACT_ONLY, bindings, consumer,
        {1, 4, 32}, {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, receiverWork,
        queueWork);
    REQUIRE(handle);
    const std::array<c::SubscriptionRequest, 1> desired{{{{local.recordContext(0), u"A"}, {}}}};
    m::QueueSubmitWork submit{10000, 1000000, 1000000, 1000000};
    const auto ticket = handle->replace(desired, submit);
    REQUIRE(ticket);

    for (unsigned turn = 0; turn < 1000 && !ticket->result(); ++turn) {
        auto work = r::queueWork();
        REQUIRE(queue.progress(work));
    }

    REQUIRE(ticket->result());
    REQUIRE(ticket->result()->status == m::CommandStatus::APPLIED);

    return std::move(*handle);
}

/** Inspect retained values only after the old native/queue owners have actually retired.
 * @param consumer Independent application-owned observations and detached results.
 */
inline void check(const Observation &consumer) {
    CHECK(consumer.callbacks == 1);
    CHECK(consumer.batchSize == 3);
    CHECK(consumer.stopAccepted);
    CHECK(consumer.projected);
    CHECK(consumer.noInventedTime);
    CHECK(consumer.destroyed);
    CHECK_FALSE(consumer.premature);
    CHECK_FALSE(consumer.inside);
    REQUIRE(consumer.profiles.size() == 3);
    const auto records = consumer.retained.records();
    REQUIRE(records.size() == 3);

    for (std::size_t i = 0; i < records.size(); ++i) {
        REQUIRE(records[i].typedValues.size() == 4);
        CHECK(records[i].typedValues[2].raw == 101 + static_cast<std::int64_t>(i));
        REQUIRE(records[i].typedValues[3].units);
        CHECK(*records[i].typedValues[3].units == u"owned-description");
        CHECK(consumer.profiles[i].description == std::optional<std::u16string>(u"owned-description"));
        CHECK(consumer.profiles[i].eventTime == 0);
    }
}
} // namespace org::ttldtor::qdcxx::test::receiver_lifetime
