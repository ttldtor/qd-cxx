// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "example.hpp"

#include <array>
#include <ostream>
#include <stdexcept>

namespace org::ttldtor::qdcxx::examples {
namespace {
/** Application-owned callback; no borrowed batch storage escapes invocation. */
class PrintRecord final : public client::Consumer {
    std::ostream &output_;

    public:
    /** Preserve a borrowed synchronous output sink.
     * @param output Stream that outlives the complete run.
     */
    explicit PrintRecord(std::ostream &output) : output_(output) {
    }

    /** Print generic source coordinates; standard-event mapping is an independent caller choice.
     * @param batch Borrowed immutable records valid only through this callback.
     */
    void onBatch(const client::BorrowedBatch &batch) override {
        for (const auto &record : batch.records()) {
            output_ << "local-id=" << record.localId << " sender-id=" << record.senderId
                    << " fields=" << record.typedValues.size() << '\n';
        }

        output_.flush();

        if (!output_) {
            throw std::runtime_error("example output failed");
        }
    }
};
} // namespace

StreamResult runSubscriptionExample(transport::RuntimeBootstrap &bootstrap, const core::LocalSchema &schema,
                                    std::uint16_t port, std::ostream &output) {
    const std::array<std::u16string, 1> symbols{u"ALPHA"};
    auto consumer = std::make_shared<PrintRecord>(output);
    std::optional<client::ClientHandle> control;

    return runStream(bootstrap, schema, {"127.0.0.1", port, 0, symbols, std::chrono::milliseconds(2000), {}, 1},
                     consumer, control);
}
} // namespace org::ttldtor::qdcxx::examples
