// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/events/configuration.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <cstdlib>
#include <doctest/doctest.h>
#include <future>
#include <thread>

namespace {
thread_local std::ptrdiff_t allocationFailAfter = -1; ///< Fault injection is local to this test thread only.
}

/** Inject ordinary allocation failure without involving another process or callback thread.
 * @param size Requested ordinary allocation bytes.
 * @return Owned malloc backing or bad_alloc at the selected local checkpoint.
 */
void *operator new(std::size_t size) {
    if (allocationFailAfter == 0) {
        throw std::bad_alloc();
    }

    if (allocationFailAfter > 0) {
        --allocationFailAfter;
    }

    if (auto *result = std::malloc(size ? size : 1)) {
        return result;
    }

    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void *pointer) noexcept {
    std::free(pointer);
}

void operator delete(void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

namespace {
namespace e = org::ttldtor::qdcxx::events;
namespace c = org::ttldtor::qdcxx::core;
using Error = e::AttachmentError;
using Output = std::expected<e::AttachmentCodecOutput, Error>;
constexpr e::AttachmentDecodeLimits DECODE_LIMITS{1024, 1024, 1024, 1024};
constexpr e::AttachmentCodecRegistryLimits REGISTRY_LIMITS{8, 64, 65536};

/** Build an owned nullable body independent of caller storage.
 * @param bytes Exact input bytes; nullopt means absent, empty span means engaged-empty.
 * @return Independent SERIAL_OBJECT value.
 */
e::SerializedAttachment attachment(std::optional<std::span<const std::byte>> bytes) {
    auto value = e::SerializedAttachment::create(c::SerialFieldType::SERIAL_OBJECT, bytes, 1024);
    REQUIRE(value);

    return std::move(*value);
}

/** Define a deliberately small native byte-to-ASCII test codec; this is no Java decoder.
 * @param calls Shared owned invocation counter.
 * @return Explicit application registration whose result and captures own their storage.
 */
e::AttachmentCodecRegistration ascii(std::shared_ptr<std::atomic<unsigned>> calls) {
    return {"native-ascii",
            1,
            c::SerialFieldType::SERIAL_OBJECT,
            typeid(std::string),
            [calls](std::span<const std::byte> bytes, e::AttachmentDecodeContext &context) -> Output {
                ++*calls;

                if (bytes.empty() && !bytes.data()) {
                    return std::unexpected(Error::INVALID_INPUT);
                }

                if (!context.chargeOutput(sizeof(std::string) + bytes.size() + 1) ||
                    !context.chargeWork(bytes.size())) {
                    return std::unexpected(Error::RESOURCE_LIMIT);
                }

                std::string result;

                for (const auto byte : bytes) {
                    const auto ch = std::to_integer<unsigned>(byte);

                    if (ch > 127) {
                        return std::unexpected(Error::MALFORMED_BODY);
                    }

                    result.push_back(static_cast<char>(ch));
                }

                return e::AttachmentCodecOutput{std::move(result), bytes.size()};
            },
            sizeof(std::shared_ptr<std::atomic<unsigned>>) + sizeof(std::atomic<unsigned>)};
}

TEST_CASE("serialized attachments distinguish null empty and kind with owning atomic replacement") {
    std::array bytes{std::byte{'A'}, std::byte{'B'}};
    auto original = attachment(bytes);
    bytes[0] = std::byte{'X'};
    CHECK((*original.bytes())[0] == std::byte{'A'});
    auto copy = original;
    const auto empty = attachment(std::span<const std::byte>{});
    const auto null = attachment(std::nullopt);
    CHECK(original == copy);
    CHECK(copy != empty);
    CHECK(empty != null);
    CHECK_FALSE(empty.isNull());
    CHECK(null.isNull());
    CHECK(e::SerializedAttachment::create(c::SerialFieldType::BYTE_ARRAY, original.bytes(), 2) !=
          e::SerializedAttachment::create(c::SerialFieldType::SERIAL_OBJECT, original.bytes(), 2));
    CHECK(e::SerializedAttachment::create(c::SerialFieldType::SERIAL_OBJECT, original.bytes(), 1).error() ==
          Error::RESOURCE_LIMIT);
    CHECK(original == copy);
    CHECK(e::SerializedAttachment::create(c::SerialFieldType::UTF_CHAR_ARRAY, original.bytes(), 2).error() ==
          Error::INVALID_INPUT);
}

TEST_CASE("explicit codec selection validates null type version kind before running trusted code") {
    auto calls = std::make_shared<std::atomic<unsigned>>(0);
    const std::array registrations{ascii(calls)};
    auto registry = e::AttachmentCodecRegistry::create(registrations, REGISTRY_LIMITS);
    REQUIRE(registry);
    const auto null = attachment(std::nullopt);
    CHECK(registry->decode<std::string>(null, "unknown", 1, DECODE_LIMITS).error() == Error::UNKNOWN_CODEC);
    CHECK(registry->decode<std::string>(null, "native-ascii", 2, DECODE_LIMITS).error() == Error::UNSUPPORTED_VERSION);
    CHECK(registry->decode<int>(null, "native-ascii", 1, DECODE_LIMITS).error() == Error::OUTPUT_TYPE_MISMATCH);
    auto absent = registry->decode<std::string>(null, "native-ascii", 1, DECODE_LIMITS);
    REQUIRE(absent);
    CHECK_FALSE(*absent);
    CHECK(*calls == 0);
    const auto empty = attachment(std::span<const std::byte>{});
    auto decodedEmpty = registry->decode<std::string>(empty, "native-ascii", 1, DECODE_LIMITS);
    REQUIRE(decodedEmpty);
    REQUIRE(*decodedEmpty);
    CHECK((**decodedEmpty).empty());
    CHECK(*calls == 1);
    auto differentKind = e::SerializedAttachment::create(c::SerialFieldType::BYTE_ARRAY, empty.bytes(), 0);
    REQUIRE(differentKind);
    CHECK(registry->decode<std::string>(*differentKind, "native-ascii", 1, DECODE_LIMITS).error() ==
          Error::FIELD_TYPE_MISMATCH);
    CHECK(*calls == 1);
    const std::array body{std::byte{'O'}, std::byte{'K'}};
    const auto value = attachment(body);
    auto restricted = DECODE_LIMITS;
    restricted.inputBytes = 1;
    CHECK(registry->decode<std::string>(value, "native-ascii", 1, restricted).error() == Error::RESOURCE_LIMIT);
    CHECK(*calls == 1);
    restricted = DECODE_LIMITS;
    restricted.outputBytes = 0;
    CHECK(registry->decode<std::string>(value, "native-ascii", 1, restricted).error() == Error::RESOURCE_LIMIT);
    CHECK(value == attachment(body));
    auto success = registry->decode<std::string>(value, "native-ascii", 1, DECODE_LIMITS);
    REQUIRE(success);
    REQUIRE(*success);
    CHECK(**success == "OK");
    auto registryCopy = *registry;
    registry = std::unexpected(Error::INVALID_INPUT);
    CHECK(**success == "OK");
    CHECK(registryCopy.decode<std::string>(value, "native-ascii", 1, DECODE_LIMITS));
}

TEST_CASE("codec failures preserve bytes and publish no partial native result") {
    const std::array body{std::byte{'A'}};
    const auto value = attachment(body);

    for (const auto mode : {0, 1, 2, 3, 4, 5}) {
        e::AttachmentCodecRegistration registration{
            "failure",
            0,
            c::SerialFieldType::SERIAL_OBJECT,
            typeid(std::string),
            [mode](std::span<const std::byte> bytes, e::AttachmentDecodeContext &context) -> Output {
                if (mode == 0) {
                    throw std::runtime_error("must not escape as diagnostic text");
                }

                if (mode == 1) {
                    throw std::bad_alloc();
                }

                if (mode == 2) {
                    return e::AttachmentCodecOutput{std::string("partial"), 0};
                }

                if (mode == 3) {
                    return e::AttachmentCodecOutput{42, bytes.size()};
                }

                if (mode == 4) {
                    static_cast<void>(context.chargeScratch(1025));
                }

                if (mode == 5) {
                    static_cast<void>(context.chargeWork(1025));
                }

                return e::AttachmentCodecOutput{std::string("value"), bytes.size()};
            },
            0};
        auto registry = e::AttachmentCodecRegistry::create(std::span(&registration, 1), REGISTRY_LIMITS);
        REQUIRE(registry);
        const auto result = registry->decode<std::string>(value, "failure", 0, DECODE_LIMITS);
        REQUIRE_FALSE(result);
        const auto expected = mode == 0   ? Error::CODEC_EXCEPTION
                              : mode == 1 ? Error::ALLOCATION_FAILURE
                              : mode == 2 ? Error::MALFORMED_BODY
                              : mode == 3 ? Error::OUTPUT_TYPE_MISMATCH
                                          : Error::RESOURCE_LIMIT;
        CHECK(result.error() == expected);
        CHECK(value == attachment(body));
    }

    auto calls = std::make_shared<std::atomic<unsigned>>(0);
    const std::array duplicate{ascii(calls), ascii(calls)};
    CHECK(e::AttachmentCodecRegistry::create(duplicate, REGISTRY_LIMITS).error() == Error::DUPLICATE_REGISTRATION);
    CHECK(e::AttachmentCodecRegistry::create(std::span(duplicate).first(1), {0, 64, 65536}).error() ==
          Error::RESOURCE_LIMIT);
    CHECK(e::AttachmentCodecRegistry::create(std::span(duplicate).first(1), {8, 0, 65536}).error() ==
          Error::RESOURCE_LIMIT);
    CHECK(e::AttachmentCodecRegistry::create(std::span(duplicate).first(1), {8, 64, 0}).error() ==
          Error::RESOURCE_LIMIT);
}

TEST_CASE("actual registry allocation failures are contained and attachment replacement remains atomic") {
    auto calls = std::make_shared<std::atomic<unsigned>>(0);
    auto registration = ascii(calls);
    registration.key = std::string(200, 'k');
    unsigned failures = 0;
    unsigned successes = 0;

    for (std::ptrdiff_t point = 0; point < 16; ++point) {
        allocationFailAfter = point;
        const auto result = e::AttachmentCodecRegistry::create(std::span(&registration, 1), {1, 256, 65536});
        allocationFailAfter = -1;

        if (!result) {
            CHECK(result.error() == Error::ALLOCATION_FAILURE);
            ++failures;
        } else {
            ++successes;
        }
    }

    CHECK(failures >= 2);
    CHECK(successes >= 1);
    const std::array oldBody{std::byte{'A'}};
    const std::array newBody{std::byte{'B'}, std::byte{'C'}};
    auto target = attachment(oldBody);
    const auto unchanged = target;
    const auto replacement = attachment(newBody);
    bool threw = false;
    allocationFailAfter = 0;
    try {
        target = replacement;
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    allocationFailAfter = -1;
    CHECK(threw);
    CHECK(target == unchanged);
    allocationFailAfter = 0;
    auto refused = e::SerializedAttachment::create(c::SerialFieldType::SERIAL_OBJECT, replacement.bytes(), 2);
    allocationFailAfter = -1;
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == Error::ALLOCATION_FAILURE);
}

TEST_CASE("same registration reentry is rejected while registry snapshots and callback state stay owned") {
    struct State {
        std::weak_ptr<e::AttachmentCodecRegistry> registry;
        std::atomic<unsigned> calls{0};
    };

    auto state = std::make_shared<State>();
    const auto value = attachment(std::span<const std::byte>{});
    e::AttachmentCodecRegistration registration{
        "recursive",
        1,
        c::SerialFieldType::SERIAL_OBJECT,
        typeid(std::string),
        [state](std::span<const std::byte> bytes, e::AttachmentDecodeContext &context) -> Output {
            ++state->calls;
            auto snapshot = state->registry.lock();

            if (!snapshot) {
                return std::unexpected(Error::INVALID_INPUT);
            }

            const auto input = attachment(bytes);
            const auto nested = snapshot->decode<std::string>(input, "recursive", 1, DECODE_LIMITS);

            if (nested || nested.error() != Error::REENTRY) {
                return std::unexpected(Error::INVALID_INPUT);
            }

            if (!context.chargeOutput(sizeof(std::string) + 6)) {
                return std::unexpected(Error::RESOURCE_LIMIT);
            }

            return e::AttachmentCodecOutput{std::string("owned"), bytes.size()};
        },
        sizeof(State)};
    auto created = e::AttachmentCodecRegistry::create(std::span(&registration, 1), REGISTRY_LIMITS);
    REQUIRE(created);
    auto owner = std::make_shared<e::AttachmentCodecRegistry>(std::move(*created));
    state->registry = owner;
    const auto result = owner->decode<std::string>(value, "recursive", 1, DECODE_LIMITS);
    REQUIRE(result);
    CHECK(**result == "owned");
    CHECK(state->calls == 1);
    owner.reset();
    CHECK(**result == "owned");
}

TEST_CASE("an in flight snapshot owns callback captures after the caller releases its registry") {
    struct State {
        std::promise<void> entered;
        std::shared_future<void> release;
    };

    std::promise<void> release;
    auto state = std::make_shared<State>();
    state->release = release.get_future().share();
    auto entered = state->entered.get_future();
    std::weak_ptr<State> weak = state;
    e::AttachmentCodecRegistration registration{
        "lifetime",
        1,
        c::SerialFieldType::SERIAL_OBJECT,
        typeid(std::string),
        [state](std::span<const std::byte> bytes, e::AttachmentDecodeContext &context) -> Output {
            state->entered.set_value();
            state->release.wait();

            if (!context.chargeOutput(sizeof(std::string) + 6)) {
                return std::unexpected(Error::RESOURCE_LIMIT);
            }

            return e::AttachmentCodecOutput{std::string("alive"), bytes.size()};
        },
        sizeof(State)};
    auto owner = e::AttachmentCodecRegistry::create(std::span(&registration, 1), REGISTRY_LIMITS);
    REQUIRE(owner);
    const auto input = attachment(std::span<const std::byte>{});
    auto pending = std::async(std::launch::async, [snapshot = *owner, input]() mutable {
        const auto result = snapshot.decode<std::string>(input, "lifetime", 1, DECODE_LIMITS);

        return result;
    });
    const auto status = entered.wait_for(std::chrono::seconds(3));

    if (status != std::future_status::ready) {
        release.set_value();
        REQUIRE(status == std::future_status::ready);
    }

    registration.decode = {};
    state.reset();
    owner = std::unexpected(Error::INVALID_INPUT);
    CHECK_FALSE(weak.expired());
    release.set_value();
    const auto result = pending.get();
    REQUIRE(result);
    REQUIRE(*result);
    CHECK(**result == "alive");
    pending = {};
    CHECK(weak.expired());
}

TEST_CASE("two concurrent codecs cannot create an A to B B to A registry lock cycle") {
    struct State {
        std::weak_ptr<e::AttachmentCodecRegistry> registry;
        std::barrier<> phase{2};
        std::atomic<unsigned> busy{0};
    };

    auto state = std::make_shared<State>();
    auto make = [state](std::string key, std::string other) {
        return e::AttachmentCodecRegistration{
            std::move(key),
            1,
            c::SerialFieldType::SERIAL_OBJECT,
            typeid(std::string),
            [state, other = std::move(other)](std::span<const std::byte> bytes,
                                              e::AttachmentDecodeContext &context) -> Output {
                auto registry = state->registry.lock();

                if (!registry) {
                    return std::unexpected(Error::INVALID_INPUT);
                }

                state->phase.arrive_and_wait();
                const auto input = attachment(bytes);
                const auto nested = registry->decode<std::string>(input, other, 1, DECODE_LIMITS);

                if (!nested && nested.error() == Error::CODEC_BUSY) {
                    ++state->busy;
                }

                state->phase.arrive_and_wait();

                if (!context.chargeOutput(sizeof(std::string) + 5)) {
                    return std::unexpected(Error::RESOURCE_LIMIT);
                }

                return e::AttachmentCodecOutput{std::string("done"), bytes.size()};
            },
            sizeof(State) + other.size() + 1};
    };
    const std::array registrations{make("A", "B"), make("B", "A")};
    auto created = e::AttachmentCodecRegistry::create(registrations, REGISTRY_LIMITS);
    REQUIRE(created);
    auto owner = std::make_shared<e::AttachmentCodecRegistry>(std::move(*created));
    state->registry = owner;
    const auto input = attachment(std::span<const std::byte>{});
    auto first = std::async(std::launch::async, [owner, input] {
        return owner->decode<std::string>(input, "A", 1, DECODE_LIMITS);
    });
    auto second = std::async(std::launch::async, [owner, input] {
        return owner->decode<std::string>(input, "B", 1, DECODE_LIMITS);
    });
    REQUIRE(first.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    REQUIRE(second.wait_for(std::chrono::seconds(3)) == std::future_status::ready);
    CHECK(first.get());
    CHECK(second.get());
    CHECK(state->busy == 2);
}
} // namespace
