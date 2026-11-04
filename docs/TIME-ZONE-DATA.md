# Explicit timezone data and bindings

[Русская версия](TIME-ZONE-DATA.ru.md). English is authoritative.

The always-exported C++23 `qdcxx_time_zone_data` target provides immutable owning
`org::ttldtor::qdcxx::time::ZoneBinding` values from `<qd/time/time_zone_data.hpp>`.
It has no event-value, Schedule, runtime SDK or transport dependency. Its backend-free configuration
supports the fixed identities in the pinned Java policy and normalized `GMT+HH:MM`/`GMT-HH:MM`
custom IDs. Actual offset results are whole seconds; this API does not represent arbitrary
sub-second fixed offsets.

The optional native backend is selected by `ORG_TTLDTOR_QDCXX_BUILD_TIME_ZONE_DATA=ON` or
`ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON`. Enabling another consumer alone does not enable it.
`find_package(qdcxx REQUIRED COMPONENTS time_zone_data)` checks the always-available target;
request `time_zone_data_backend` when the installed native backend is required. A missing backend
produces an actionable configure error. With backend OFF, `TimeZoneData::initialize` returns
`NOT_READY` without reading its path, and valid non-fixed IDs without data return `NOT_READY`.

## Initialize once, retain bindings

```cpp
#include <qd/time/time_zone_data.hpp>

int main() {
    namespace time = org::ttldtor::qdcxx::time;

    auto fixed = time::ZoneBinding::create(u"GMT+05:30", nullptr, {0, 128});

    if (!fixed) {
        return 1;
    }

    auto offset = fixed->getOffset(0);

    return offset && offset->totalSeconds == 19800 ? 0 : 1;
}
```

For named zones, call `TimeZoneData::initialize(absoluteDataDirectory, limits)` once and retain its
successful capability. `data.bind(id, options)` and `ZoneBinding::create(id, &data, options)` are
equivalent. The returned binding owns its exact UTF-16 ID, raw-reference instant and shared immutable
provider profile. Input ID views and the optional data pointer are discarded after construction.
Copies and moves retain ownership; the original moved-from handle remains valid. The provider and
binding outlive their original data variable. Replacing a handle concurrently requires external
synchronization; concurrent queries through stable handle copies are supported.

No Date/TZDB implementation types are public. `getOffset(utcMillis)` returns a copied
`TimeZoneOffset{totalSeconds,referenceRawSeconds}`. It reads no clock/files, invokes no callback,
starts no workers and retains no query state. `getQueryBounds()` exposes the particular binding's
inclusive scalar range. Fixed bindings cover the whole signed 64-bit millisecond domain. Named
bindings cover its minimum through the end of native year32766, leaving the provider's next-year
search headroom; query instants are never converted to nanoseconds. Historical pre-1900 instants
use Java ZoneInfo's final raw-base convention. SystemV identities use the captured legacy transition
profile and source recurrence rather than modern US IANA rules. Three-letter aliases follow the
actual pinned Java identity mapping, not an IANA-only acceptance path.

`rawOffsetReferenceTime` explicitly replaces the model constructor clock. Without it, construction
captures current Unix milliseconds once. Raw metadata is obtained from standard history with
[documented rearguard conventions](../resources/time-zone-data/calendar-offset-conventions.json),
including negative SAVE eras. It is separate from actual historical offset and cannot generally be
computed as `IANA offset - SAVE` without those conventions. Before the Java transition-table boundary
or beyond the native envelope, raw-reference metadata uses the final standard base. The reference
does not limit query dates. The binding returns typed invalid-ID, not-ready, range, resource,
allocation or data errors; `maxIdUnits` is checked before copying an ID.

## Exact offline dependency and data

The [source manifest](../third_party/schedule-date-3.0.5/manifest.json) pins Howard Hinnant date/tz
v3.0.5, full commit `179a6b921ccd6179b5b04dce812a89f71a51b4df`, original/patched source hashes,
MIT license and the complete maintained patch. The [data manifest](../resources/time-zone-data/tzdb-2026f/manifest.json)
pins IANA TZDB2026f and its archive and individual file hashes. Both source and data are tracked;
configure performs no acquisition or floating-version resolution. The builder validates manifests,
generates approved-byte backing in bulk and compiles the private namespace
`org_ttldtor_qdcxx_date_305_v1`. A static archive alone would not isolate the native global registry;
the namespace and controlled input patch provide that separation.

Native initialization validates an explicit absolute directory, refuses symlinked profile files,
and reads exactly the eleven approved files into owned memory before native parsing. It checks bytes,
line/record counts and native cumulative insertions before growth. All 344 zones are initialized and
253 links resolved before the native registry is ready; the public capability additionally resolves
the complete 631-ID Java mapping, including fixed and legacy identities, before publication.
There is one native admission attempt/generation per process. Failure after native admission publishes no usable public
capability and does not trigger retry, reload or a second generation. An invalid path-length argument
rejected before native admission does not consume the attempt. There is no CURL, download, home/OS
discovery, current-zone mapping or automatic refresh.

Default admission bounds are11 files,1100000 bytes,1024 bytes/line,35000 lines,12000 source records,
600 zones,400 links,10000 rules and32768 cumulative native insertions. The exact snapshot contains
855823 bytes,19069 lines,2616 source records and2596 adjusted native rules. `getLogicalBytes()` counts
the distinct owned binding model/ID fields; shared profile/static backing and standard allocator,
control-block/capacity/runtime overhead are separate. These limits and measured allocation envelopes
are not a configurable hard native heap or RSS quota. Ordinary allocation failures fail closed.

## Installation and relocation

A backend-enabled install includes the versioned data directory
`share/qdcxx/time-zone-data/tzdb-2026f`, IANA license/manifest, Java compatibility observations/conventions,
and MIT license/source manifest/patch/patched source under `date-3.0.5-notices`. Copy the installed
`share/qdcxx/time-zone-data` subtree with the application's deployment, or retain it under a relocated
SDK prefix. Pass the resulting absolute `tzdb-2026f` directory explicitly at initialization. The
installed libraries/headers contain no source-tree or private working-directory paths and perform no
ambient search for this directory. Applications choose their own executable/configuration path policy.

Timezone updates require a maintainer-reviewed offline source/data/manifest/patch update, validation,
deployment and process restart. The separate MPL market-calendar resource and typed calendar
generations are unaffected by that update mechanism. A parser identity table alone does not qualify
a native lookup profile or establish compatibility of different TZDB datasets.
