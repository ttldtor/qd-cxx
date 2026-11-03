// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/indexed_event_source.hpp"

#include <utility>

namespace org::ttldtor::qdcxx::events {
const IndexedEventSource IndexedEventSource::DEFAULT{0, u"DEFAULT"};

IndexedEventSource::IndexedEventSource(std::int32_t id, std::optional<std::u16string> name)
    : id_(id), name_(std::move(name)) {
}

std::int32_t IndexedEventSource::id() const noexcept {
    return id_;
}

std::optional<std::u16string_view> IndexedEventSource::name() const noexcept {
    if (!name_) {
        return std::nullopt;
    }

    return *name_;
}

std::int32_t IndexedEventSource::hashCode() const noexcept {
    return id_;
}

bool IndexedEventSource::operator==(const IndexedEventSource &other) const noexcept {
    return id_ == other.id_;
}
} // namespace org::ttldtor::qdcxx::events
