# Instrument-profile values {#instrument-profiles}

[Русская версия](INSTRUMENT-PROFILES.ru.md). English is authoritative.

Use `org::ttldtor::qdcxx::ipf::InstrumentProfile` for independent instrument metadata.
It owns standard/custom fields and supports typed numbers and calendar day IDs. This is a separate
value model from the subscribed market-data Profile event.

Enable `ORG_TTLDTOR_QDCXX_BUILD_IPF=ON` when configuring qd-cxx; the default is OFF.
Link `qdcxx_ipf` and include `qd/ipf/instrument_profile.hpp`. No runtime, Feed, JSON, network SDK
or additional library dependency is required. An installed package exports the same target:

```cmake
find_package(qdcxx CONFIG REQUIRED)
add_executable(profile_example main.cpp)
target_link_libraries(profile_example PRIVATE qdcxx_ipf)
```

```cpp
#include <qd/ipf/instrument_profile.hpp>

int main() {
    using org::ttldtor::qdcxx::ipf::InstrumentProfile;
    InstrumentProfile profile;
    if (!profile.setType(u"STOCK") || !profile.setSymbol(u"IBM") ||
        !profile.setDescription(u"International Business Machines") ||
        !profile.setMultiplier(1) || !profile.setField(u"DESK", u"equities")) {
        return 1;
    }
    auto independent = profile;
    return independent.getSymbol() == u"IBM" &&
                   independent.getMultiplier() == 1 &&
                   independent.getField(u"DESK") == u"equities" ? 0 : 2;
}
```

All 31 standard fields have named getters/setters. String getters return borrowed UTF-16 views;
integer and numeric getters return values. Dates use signed days from 1970-01-01 GMT:
`setExpiration(1)` stores day 1, while `setField(u"EXPIRATION",u"1970-01-02")` parses the same value.
Generic `getNumericField()/getDateField()` return `std::expected`; asking for a standard string
field as a number returns `TEXTUAL_FIELD`. Setter failures leave the complete profile unchanged.

Unknown fields and TYPE values are supported, names are case-sensitive, and missing custom fields
read empty. `setField(u"DESK",nullptr)` clears a custom value. Empty and null normalize together;
raw file-presence metadata is separate. Text is exact UTF-16, with no case folding or Unicode
normalization. Copies remain independent after edits. Getter views expire on mutation, assignment
or destruction; custom-name enumeration owns its result. Callers serialize mutation/observation.

Default per-instance limits are 256 custom fields, 256 name units, 65536 units per value, 1048576
aggregate UTF-16 units and 65536 input units. Supply `ProfileLimits` only when your application needs
other bounds. Canonical numbers/dates also count against retained text limits. No global setting
is read. Conversion details, nonfinite numbers, lenient historical dates and exact symbol versus
presentation ordering are defined in the [contract](../spec/ipf/PROFILE.md).

The value object starts no I/O or background work. The same optional target also supplies the
separate local reader below. Writing, HTTP/live collection, option-chain construction and schedule
interpretation remain separate features.
The [oracle fixture](../tests/fixtures/ipf/README.md) documents the actual conversion evidence.

## Read a local IPF stream

Include `qd/ipf/instrument_profile_reader.hpp` and link the same `qdcxx_ipf` target.
`readInstrumentProfilesFile(path)` opens a local file in binary mode. For an already owned stream,
use the synchronous stream overload; it does not close or seek your stream:

```cpp
#include <qd/ipf/instrument_profile_reader.hpp>
#include <sstream>

int main() {
    using org::ttldtor::qdcxx::ipf::readInstrumentProfiles;
    std::istringstream input(
        "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n"
        "STOCK,IBM,International Business Machines\n"
        "##COMPLETE\n");
    auto result = readInstrumentProfiles(input);
    if (result.error || !result.complete || result.profiles.size() != 1) {
        return 1;
    }
    return result.profiles.front().getSymbol() == u"IBM" ? 0 : 2;
}
```

The result owns all complete profiles in source order. On failure, `profiles` keeps the accepted
whole prefix and `error` describes the first failure; no partial row becomes a profile.
`completionSeen` and clean `finished` are separate evidence, and `complete` requires both.
A completion marker does not stop the stream. Default ALLOW accepts clean EOF without a marker
as incomplete; select `IpfCompletionPolicy::REQUIRE` explicitly when your application requires it.

Readers accept declared/redefined formats, exact standard/custom names, quoted commas/newlines,
doubled quotes and JDK-compatible UTF-8 replacements. Default cumulative input is 64 MiB and output
is 100000 profiles/16 Mi UTF-16 units, with separate row/field/header and per-profile bounds.
They are engineering defaults: pass `IpfReadOptions` when the application needs other bounds.
Pure `InstrumentProfileParser` supports incremental byte spans, one owning profile/control item
and explicit replenishable work. See [reader contract](../spec/ipf/READER.md) and
[actual grammar observations](../tests/fixtures/ipf-reader/README.md).

Input/output/work bounds do not impose an I/O deadline. The caller's streambuf may read ahead;
`acceptedBytes` is the parser's logical consumption, not `tellg()` or a rollback promise.
The caller's stream exception mask is preserved. An escaping `std::bad_alloc` maps to
`ALLOCATION_FAILURE`; if the stream suppresses it and sets `badbit`, the observable result is
`IO_ERROR`. Parser/output allocation failures map to `ALLOCATION_FAILURE`. Typed failures retain
complete earlier profiles.
Only local uncompressed input is supported; no HTTP, decompression or file-format auto-detection runs.