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

This component reads no files or URLs and starts no background work. IPF CSV reading/writing,
HTTP/live collection, option-chain construction and schedule interpretation are separate features.
The [oracle fixture](../tests/fixtures/ipf/README.md) documents the actual conversion evidence.
