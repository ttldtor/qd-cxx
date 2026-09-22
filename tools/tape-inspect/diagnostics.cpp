// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "diagnostics.hpp"

namespace org::ttldtor::qdcxx::tape_inspect {
std::string_view tapeRejectionName(protocol::TapeRejection reason) noexcept {
    switch (reason) {
    case protocol::TapeRejection::TIME:
        return "TIME";
    case protocol::TapeRejection::OPTIONS:
        return "OPTIONS";
    case protocol::TapeRejection::MESSAGE:
        return "MESSAGE";
    case protocol::TapeRejection::TYPE:
        return "TYPE";
    case protocol::TapeRejection::LOCAL_PROFILE:
        return "LOCAL_PROFILE";
    case protocol::TapeRejection::DESCRIPTOR:
        return "DESCRIPTOR";
    case protocol::TapeRejection::SEQUENCE:
        return "SEQUENCE";
    default:
        return "NONE";
    }
}

std::string_view tapeRejectionExplanation(protocol::TapeRejection reason) noexcept {
    switch (reason) {
    case protocol::TapeRejection::TIME:
        return "Only absent time or explicit NONE timing is supported.";
    case protocol::TapeRejection::OPTIONS:
        return "Explicit empty protocol options and zero raw event flags are required.";
    case protocol::TapeRejection::MESSAGE:
        return "Only STREAM_DATA and the restricted descriptor message set are supported.";
    case protocol::TapeRejection::TYPE:
        return "The full serial type is unsupported or requires an excluded conversion.";
    case protocol::TapeRejection::LOCAL_PROFILE:
        return "Select oracle-sre-v1 or default-quote-v1 explicitly.";
    case protocol::TapeRejection::DESCRIPTOR:
        return "The descriptor contains an unsupported property, value or extension.";
    case protocol::TapeRejection::SEQUENCE:
        return "Exactly one initial protocol descriptor is required.";
    default:
        return "Inspect the error category, stage and exact positions.";
    }
}

} // namespace org::ttldtor::qdcxx::tape_inspect
