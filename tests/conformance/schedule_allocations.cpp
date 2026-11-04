// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schedule_allocations.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> enabled{false};
std::atomic<std::size_t> failure{0};
std::atomic<std::size_t> calls{0};
} // namespace

void *operator new(std::size_t size) {
    if (enabled.load(std::memory_order_relaxed)) {
        const auto ordinal = calls.fetch_add(1, std::memory_order_relaxed) + 1;

        if (ordinal == failure.load(std::memory_order_relaxed)) {
            throw std::bad_alloc{};
        }
    }

    if (auto *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc{};
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete[](void *memory) noexcept {
    ::operator delete(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    ::operator delete(memory);
}

namespace org::ttldtor::qdcxx::schedule::allocation_test {
void arm(std::size_t ordinal) noexcept {
    calls.store(0, std::memory_order_relaxed);
    failure.store(ordinal, std::memory_order_relaxed);
    enabled.store(true, std::memory_order_relaxed);
}

std::size_t disarm() noexcept {
    enabled.store(false, std::memory_order_relaxed);

    return calls.load(std::memory_order_relaxed);
}
} // namespace org::ttldtor::qdcxx::schedule::allocation_test
