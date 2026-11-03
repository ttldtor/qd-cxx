// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/source_selection.hpp"

#include <utility>

namespace org::ttldtor::qdcxx::events {
SourceSelection::SourceSelection(std::optional<IndexedEventSource> source) : source_(std::move(source)) {
}

SourceSelection SourceSelection::all() {
    return SourceSelection(std::nullopt);
}

SourceSelection SourceSelection::specific(IndexedEventSource source) {
    return SourceSelection(std::move(source));
}

bool SourceSelection::isAll() const noexcept {
    return !source_;
}

const IndexedEventSource *SourceSelection::source() const noexcept {
    return source_ ? &*source_ : nullptr;
}

bool SourceSelection::matches(const IndexedEventSource &candidate) const noexcept {
    return !source_ || *source_ == candidate;
}

bool SourceSelection::operator==(const SourceSelection &other) const noexcept {
    return source_ == other.source_;
}
} // namespace org::ttldtor::qdcxx::events
