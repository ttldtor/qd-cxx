// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <ios>
#include <streambuf>

namespace org::ttldtor::qdcxx::test_support {
/** Stream operation selected for an all-or-nothing I/O failure. */
enum class FaultOperation {
    READ,  ///< Use the buffer as input; its first underflow throws.
    WRITE, ///< Reject an entire block write.
    FLUSH  ///< Accept block writes and reject flushing.
};

/** Simple stream fault double; partial writes and repeated-flush scenarios use separate doubles. */
class FaultBuffer : public std::streambuf {
    FaultOperation operation_; ///< Selected output fault, or input-only READ use.

    public:
    /** Select the operation that fails.
     * @param operation READ uses this buffer only as input; WRITE/FLUSH use it only as output.
     */
    explicit FaultBuffer(FaultOperation operation) : operation_(operation) {
    }

    protected:
    /** Fail input underflow without producing bytes.
     * @return Never returns normally.
     */
    int_type underflow() override {
        throw std::ios_base::failure("injected read");
    }

    /** Accept the complete block unless write failure was selected.
     * @param data Borrowed output bytes, intentionally discarded.
     * @param count Requested byte count.
     * @return Zero on injected write failure, otherwise count.
     */
    std::streamsize xsputn(const char *data, std::streamsize count) override {
        static_cast<void>(data);

        return operation_ == FaultOperation::WRITE ? 0 : count;
    }

    /** Report the selected flush outcome.
     * @return Minus one on injected flush failure, otherwise zero.
     */
    int sync() override {
        return operation_ == FaultOperation::FLUSH ? -1 : 0;
    }
};
} // namespace org::ttldtor::qdcxx::test_support
