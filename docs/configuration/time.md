# Timezone data

[Русский](time.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [TimeZoneBindingOptions](time.md#cfg-0681-owner) — `include/qd/time/time_zone_data.hpp`
- [TimeZoneLimits](time.md#cfg-0672-owner) — `include/qd/time/time_zone_data.hpp`

## TimeZoneBindingOptions {#cfg-0681-owner}

[include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp)

### `TimeZoneBindingOptions::rawOffsetReferenceTime` — CFG-0681 {#cfg-0681}

- Type: `std::optional<std::int64_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: signed Unix milliseconds for reference time
- Accepted values: nullopt samples system_clock::now once at binding creation; an explicit int64_t is preserved exactly. Nonfixed raw-offset determination maps times outside[-2208988800000,provider maximum] to maximum; fixed/legacy identities do not depend on reference.
- Read timing: ZoneBinding::create after identity admission; subsequent offset queries do not resample constructor time.
- Mutability and lifetime: create selects the supplied time or samples the clock once and retains that value in the model. Mutating the original options does not change an existing binding.
- Failure: The field itself rejects no int64_t. Provider/reference computation failures map to TimeZoneError; the original reference remains in the immutable model.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp).

### `TimeZoneBindingOptions::maxIdUnits` — CFG-0682 {#cfg-0682}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 code units of the exact timezone ID
- Accepted values: Any representable maxIdUnits value is accepted as a comparison threshold; no separate upper-threshold validation applies. Requires id.size()&lt;=maxIdUnits. At zero an empty ID passes length checking then yields INVALID_ID. Offset grammar, known identity and checked owned-ID size are separate checks of the actual input.
- Read timing: ZoneBinding::create checks length before identity lookup and copying into the model.
- Mutability and lifetime: The limit applies to one ZoneBinding::create call and is not retained by the result. A subsequent call can use another limit independently of existing bindings.
- Failure: TimeZoneError::RESOURCE_LIMIT for excess/bindingLogicalBytes overflow; INVALID_ID for an unknown identity; ALLOCATION for bad_alloc.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp).

## TimeZoneLimits {#cfg-0672-owner}

[include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp)

### `TimeZoneLimits::files` — CFG-0672 {#cfg-0672}

- Type: `std::size_t`. Declared default: `= 11`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: controlled-profile files
- Accepted values: files&gt;=APPROVED_FILE_COUNT=11. Only the 11 approved manifest files are loaded; extra slots do not enlarge the profile. Zero rejects before opening data.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::totalBytes` — CFG-0673 {#cfg-0673}

- Type: `std::size_t`. Declared default: `= 1100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate bytes of 11 approved files
- Accepted values: Sum of file.size() must fit; the inspected manifest totals 855823 bytes. At least 855823 is necessary for this set; controlled content/hashes are checked separately. Zero rejects.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::lineBytes` — CFG-0674 {#cfg-0674}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: line bytes excluding LF
- Accepted values: lineLength resets on LF and must be&lt;=lineBytes; CR/other bytes before LF count. Zero rejects nonempty source lines. Required minimum depends on the files. The inspected 11 files have a maximum line length of 216 bytes under this algorithm; lineBytes&gt;=216 is required for this set.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::lines` — CFG-0675 {#cfg-0675}

- Type: `std::size_t`. Declared default: `= 35000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate LF characters in the source set
- Accepted values: Every LF increments staged.lines and requires&lt;=lines. Counts line endings including empty/comment lines; final text without LF adds no line count by itself. Zero rejects a set containing LF. This set contains 19069 LF characters, so limits.lines&gt;=19069 is required.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::records` — CFG-0676 {#cfg-0676}

- Type: `std::size_t`. Declared default: `= 12000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: recognized line-start source records
- Accepted values: records increments when the first line byte is not &#35;, space, tab or CR. This is the loader exact counter, not all nonempty lines. Zero rejects the current records. Static counting with this exact predicate gives 2616 records; records&gt;=2616 is required for this set.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::zones` — CFG-0677 {#cfg-0677}

- Type: `std::size_t`. Declared default: `= 600`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native source zones
- Accepted values: zones&gt;=344 before reading; parsed db.zones.size() must equal 344 exactly. A larger grant does not permit another data set.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::links` — CFG-0678 {#cfg-0678}

- Type: `std::size_t`. Declared default: `= 400`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native source links
- Accepted values: links&gt;=253 before reading; parsing must produce exactly 253 links for the controlled profile. Zero fails the required minimum.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::rules` — CFG-0679 {#cfg-0679}

- Type: `std::size_t`. Declared default: `= 10000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: adjusted native rules
- Accepted values: rules&gt;=2596 before reading; native adjustment must yield exactly 2596 rules. modelUnits independently bounds further insertions.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).

### `TimeZoneLimits::modelUnits` — CFG-0680 {#cfg-0680}

- Type: `std::size_t`. Declared default: `= 32768`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative native model insertions
- Accepted values: admitNativeModel checks modelUnits&lt;modelLimit before each insertion, including temporary/later discarded models. Zero rejects the first insertion. Erasing models does not refund credits.
- Read timing: TimeZoneData::initialize passes admission limits into process-wide provider::initialize. After pre-gate shape checks, the first EMPTY→INITIALIZING transition consumes the only attempt. File admission and model construction follow. A disabled build returns NOT_READY before consuming limits.
- Mutability and lifetime: This provider admits initialization once per process. Success leaves READY and a post-admission failure leaves FAILED; neither returns to EMPTY. A new facade object does not reset the gate. Another admitted attempt with different limits requires a fresh provider process.
- Failure: provider::Error::LIMIT maps to TimeZoneError::RESOURCE_LIMIT; repeated admission after initialization also yields RESOURCE_LIMIT. Allocation→ALLOCATION; invalid data→DATA.
- Consumer sources: [include/qd/time/time_zone_data.hpp](../../include/qd/time/time_zone_data.hpp), [src/time/time_zone_data.cpp](../../src/time/time_zone_data.cpp), [src/time/timezone/provider.cpp](../../src/time/timezone/provider.cpp).
