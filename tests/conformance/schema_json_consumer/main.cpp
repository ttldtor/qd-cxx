// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/collectors/history.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/schema/json_schema.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
namespace h = org::ttldtor::qdcxx::collectors;
namespace j = org::ttldtor::qdcxx::schema;

/** Require an observed contract without an external test-framework dependency.
 * @param condition Required condition.
 * @param message Static failure description.
 */
void check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Supply finite independent per-turn credits.
 * @return Live protocol work, not a lifetime quota.
 */
p::LiveWork liveWork() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Supply finite collector-turn credits.
 * @return Operations, visits, bytes and field credits.
 */
h::HistoryWork historyWork() {
    return {100000, 1000000000, 1000000000, 1000000000};
}

/** Build the actual complete-frame credential-free binary source profile.
 * @return Independent finite live ownership/admission limits.
 */
p::LiveLimits liveLimits() {
    p::LiveLimits limits{};
    limits.frame = {8192, 8201};
    limits.protocol = {128, 64, {8192, 1024}, 16384};
    limits.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    limits.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    limits.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    limits.residentBytes = 4 * 1024 * 1024;
    limits.owners = 128;
    limits.snapshotBytes = 65536;
    limits.scratchBytes = 65536;
    limits.eventBytes = 262144;
    limits.outputBytes = 8192;
    limits.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    limits.fieldTime = p::FieldTimePolicy::DISABLED;
    limits.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    limits.historySnapshot = true;
    limits.data.processedRecords = 256;
    limits.data.emittedRecords = 256;

    return limits;
}

/** Supply explicit logical indexing and all nullable/opaque payload identities.
 * @param pair Select two signed32 contributors instead of one signed64 contributor.
 * @return Owned schema after destroying all JSON source storage.
 */
c::LocalSchema load(bool pair) {
    const std::vector<int> types =
        pair ? std::vector<int>{8, 8, 10, 137, 9, 233, 249, 56} : std::vector<int>{104, 10, 137, 9, 233, 249, 56};
    std::string input =
        R"({"formatVersion":1,"profile":{"id":"qdcxx-native-binary","version":1},"records":[{"id":0,"name":"Generic#History","hasTime":true,"fields":[)";

    for (std::size_t i = 0; i < types.size(); ++i) {
        input += (i == 0 ? "" : ",") + std::string("{\"serialTypeId\":") + std::to_string(types[i]) + "}";
    }

    input += "],\"lookups\":[";

    for (std::size_t i = 0; i < types.size(); ++i) {
        input += (i == 0 ? "" : ",") + std::string("{\"name\":\"F") + std::to_string(i) +
                 "\",\"fieldId\":" + std::to_string(i) + "}";
    }

    input += "],\"indexedTimeLayout\":";
    input +=
        pair ? R"({"kind":"pair-int32","highFieldId":0,"lowFieldId":1})" : R"({"kind":"single-int64","fieldId":0})";
    input += "}]}";
    auto schema = j::loadBinaryJsonSchema(input);
    check(schema.has_value(), "binary JSON load");
    input.assign(input.size(), 'x');
    input.clear();
    input.shrink_to_fit();

    return std::move(*schema);
}

/** Append a canonical signed integer through the installed codec.
 * @param out Owned destination.
 * @param value Exact signed integer bits.
 */
void number(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto result = e::writeCompactLong(value, buffer);
    check(result.status == e::Status::OK, "compact encode");
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append exact non-null UTFString metadata.
 * @param out Owned destination.
 * @param value Borrowed exact UTF16.
 */
void text(std::vector<std::byte> &out, std::u16string_view value) {
    std::array<std::byte, 8192> buffer{};
    const auto result = e::writeUtfString(value, buffer, {8192, 1024});
    check(result.status == e::Status::OK, "UTFString encode");
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Construct one actual QTP envelope.
 * @param type Exact message family.
 * @param payload Complete body, borrowed during copying.
 * @return Owned complete frame.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> out;
    number(out, static_cast<std::int64_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());

    return out;
}

/** Admit a complete source frame through the real installed protocol driver.
 * @param source Serialized complete-frame owner.
 * @param bytes Complete borrowed input.
 * @return Owning admitted event, never a fabricated callback view.
 */
p::LiveEvent admit(p::LiveSession &source, std::span<const std::byte> bytes) {
    auto work = liveWork();
    auto result = source.feed(bytes, work);
    check(result.status == p::LiveStatus::EVENT && result.accepted == bytes.size() && result.event.has_value(),
          "source admission");

    return std::move(*result.event);
}

/** Prepare a real source epoch with explicit full field names/types.
 * @param schema Immutable JSON-loaded source context, copied with provenance preserved.
 * @return Ready serialized source with its own schema and description owners.
 */
p::LiveSession session(const c::LocalSchema &schema) {
    const auto &record = schema.records()[0];
    p::OutgoingRecord outgoing{17, 0, record.name, {}};

    for (std::size_t i = 0; i < record.fields.size(); ++i) {
        outgoing.fields.push_back(
            {record.lookups[i].name, record.fields[i].serialTypeId, static_cast<std::int32_t>(i)});
    }

    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(schema, std::span(&outgoing, 1),
                                                {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    check(writer.has_value(), "subscription writer");
    auto copy = schema;
    auto work = liveWork();
    auto result = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(), work);
    check(result.has_value(), "live source");
    std::vector<std::byte> ready{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(ready, 0);
    number(ready, 3);
    number(ready, 15);
    text(ready, u"STREAM_DATA");
    number(ready, 0);
    number(ready, 10);
    text(ready, u"TICKER_DATA");
    number(ready, 0);
    number(ready, 20);
    text(ready, u"HISTORY_DATA");
    number(ready, 0);
    number(ready, 0);
    (void)admit(*result, frame(1, ready));
    std::vector<std::byte> description;
    number(description, 17);
    text(description, record.name);
    number(description, static_cast<std::int64_t>(record.fields.size()));

    for (std::size_t i = 0; i < record.fields.size(); ++i) {
        text(description, record.lookups[i].name);
        number(description, record.fields[i].serialTypeId);
    }

    (void)admit(*result, frame(2, description));

    return std::move(*result);
}

/** Append actual symbol flags and directional sender identity.
 * @param out Owned payload.
 * @param flags Exact source flags.
 * @param symbol Exact UTF16, including non-scalar units.
 */
void prefix(std::vector<std::byte> &out, int flags, std::u16string_view symbol) {
    out.push_back(std::byte{0xf9});
    number(out, flags);
    out.push_back(std::byte{0xfd});
    number(out, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        std::array<std::byte, 4> buffer{};
        const auto encoded = e::writeUtfChar(unit, buffer);
        check(encoded.status == e::Status::OK, "symbol encode");
        out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(encoded.written));
    }

    number(out, 17);
}

/** Exercise actual installed HISTORY ownership for one explicit indexing shape.
 * @param pair Select two signed32 contributors or one signed64 contributor.
 */
void exercise(bool pair) {
    auto schema = load(pair);
    auto copy = schema;
    auto work = historyWork();
    const h::HistoryLimits bounds{
        16, 256, 128, 64, 2097152, 4, 256, 64, 2097152, 33554432, {16, 32, 128, 2048, 64, 4096, 64}};
    auto history = h::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    check(history.has_value(), "history create");
    const std::u16string symbol{u'S', char16_t{0}, char16_t{0xd800}};
    const std::u16string units{char16_t{0}, u'A', char16_t{0xd800}};
    {
        auto source = session(schema);
        std::vector<std::byte> body;
        prefix(body, 4, symbol);

        if (pair) {
            number(body, 0);
        }

        number(body, 7);
        std::array<std::byte, 128> encoded{};
        const auto chars = e::writeCharArray(units, encoded, {128, 32});
        check(chars.status == e::Status::OK, "char array encode");
        body.insert(body.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(chars.written));
        number(body, -1); // Exact null UTFString.
        number(body, 0);  // Engaged empty byte array.
        number(body, 3);
        body.insert(body.end(), {std::byte{0}, std::byte{0xff}, std::byte{1}});
        number(body, -1); // Exact null opaque marshalled payload.
        number(body, -5);
        auto event = admit(source, frame(20, body));
        check(history->stage(std::move(event), work).has_value(), "history stage");
        std::vector<std::byte> end;
        prefix(end, 10, symbol);
        number(end, 0);
        auto closing = admit(source, frame(20, end));
        check(history->stage(std::move(closing), work).has_value(), "history closing stage");
    }

    auto view = history->openView({true}, work);
    check(view.has_value(), "history view");
    c::SubscriptionWork command{10000, 1000000, 1000000};
    const c::SubscriptionRequest request{{schema.recordContext(0), symbol}, 0};
    check(history->add(*view, std::span(&request, 1), command, work).has_value(), "history desired");
    auto offered = history->front(*view, work);
    check(offered.has_value() && offered->has_value(), "owned history offer");
    const auto &record = (**offered).record;
    const std::size_t payload = pair ? 2 : 1;
    check(record.rawTime == 7 && record.symbol == symbol, "time and symbol after source destruction");
    check(record.fields[payload].units && *record.fields[payload].units == units, "exact nullable UTF16");
    check(!record.fields[payload + 1].units, "null UTFString");
    check(record.fields[payload + 2].bytes && record.fields[payload + 2].bytes->empty(), "empty bytes");
    check(record.fields[payload + 3].bytes && record.fields[payload + 3].bytes->size() == 3 &&
              (*record.fields[payload + 3].bytes)[1] == std::byte{0xff},
          "opaque custom bytes");
    check(!record.fields[payload + 4].bytes && record.fields[payload + 5].raw == -5, "null opaque and extra field");
    check((**offered).sourceRecord && (**offered).sourceRecord->context.matches(schema.recordContext(0)),
          "actual source provenance");
    auto foreign = load(pair);
    const c::SubscriptionRequest foreignRequest{{foreign.recordContext(0), symbol}, 0};
    check(!history->add(*view, std::span(&foreignRequest, 1), command, work), "foreign JSON identity rejected");
    check(history->pop(*view, work).has_value(), "history pop");
}
} // namespace

/** Run an installed, relocated core composition witness without input files, sockets or callbacks.
 * @return Zero after both explicit layouts retain actual source ownership; one on failure.
 */
int main() {
    try {
        exercise(true);
        exercise(false);
        std::cout << "schema-json-consumer\t1\tOK\n";
        std::cout.flush();

        return std::cout ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
