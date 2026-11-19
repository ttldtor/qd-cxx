# Schedules

[Русский](schedule.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [CalendarDefaults](schedule.md#cfg-0640-owner) — `include/qd/schedule/schedule.hpp`
- [ScheduleLimits](schedule.md#cfg-0642-owner) — `include/qd/schedule/schedule.hpp`

## CalendarDefaults {#cfg-0640-owner}

[include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp)

### `CalendarDefaults::versionMillis` — CFG-0640 {#cfg-0640}

- Type: `std::int64_t`. Declared default: `= 0`
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Typed comparison version; older versions are ignored.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit setDefaults versioned transaction; copied borrowed entries; old versions ignored
- Mutability and lifetime: Atomic versioned generation update; failure preserves current generation and reader pins
- Failure: ScheduleError; RESOURCE_LIMIT/ALLOCATION/CONCURRENT_CHANGE and typed parser/query failures; no partial generation
- Additional owner vocabulary (not a field-to-error mapping): `ScheduleError`.

### `CalendarDefaults::entries` — CFG-0641 {#cfg-0641}

- Type: `std::span<const CalendarDefaultsEntry>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: explicit dependency or data input; lifetime must be established by its owner. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Complete borrowed entries in source order.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: Explicit setDefaults versioned transaction; copied borrowed entries; old versions ignored
- Mutability and lifetime: Atomic versioned generation update; failure preserves current generation and reader pins
- Failure: ScheduleError; RESOURCE_LIMIT/ALLOCATION/CONCURRENT_CHANGE and typed parser/query failures; no partial generation
- Additional owner vocabulary (not a field-to-error mapping): `ScheduleError`.

## ScheduleLimits {#cfg-0642-owner}

[include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp)

### `ScheduleLimits::inputUnits` — CFG-0642 {#cfg-0642}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 instrument-definition/venue units
- Accepted values: instrumentDefinition and separate venue length&lt;=inputUnits before parsing/copying. Zero permits empty text only where grammar allows it; built-in defaults have separate fields.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::defaultUnits` — CFG-0643 {#cfg-0643}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 defaults names/definitions
- Accepted values: Sum of typed entry.name+definition&lt;=defaultUnits. Initial construction also requires sizeof(CALENDAR_BYTES)&lt;=defaultUnits and&lt;=stagingBytes/sizeof(char16_t), so 0 cannot create the standard context.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::defaultEntries` — CFG-0644 {#cfg-0644}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: named lists and venues per generation
- Accepted values: typed entries.size()&lt;=defaultEntries; the built-in loader checks while appending. Zero cannot fit the nonempty built-in set. No separate upper ceiling applies.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::properties` — CFG-0645 {#cfg-0645}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: distinct merged schedule properties
- Accepted values: Before insertion props.size()&lt;properties. An existing identical key is removed first, so replacement needs no extra count; preparation work/storage are separately charged. Zero rejects a nonempty set.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dateValues` — CFG-0646 {#cfg-0646}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative constructed date values
- Accepted values: chargeDates requires count&lt;=dateValues-construction.dateValues and then reserves count*sizeof(int32_t). Zero permits no constructed date values; merges/intermediate values also consume the grant.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dayDefinitions` — CFG-0647 {#cfg-0647}

- Type: `std::size_t`. Declared default: `= 8192`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative abstract day definitions
- Accepted values: Every dayDefinition requires construction.dayDefinitions&lt;dayDefinitions and charges one before constructing its model. Zero rejects the first such parse; count is cumulative per operation.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::sessionsPerDay` — CFG-0648 {#cfg-0648}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: abstract sessions per day including inserted gaps
- Accepted values: appendSession requires sessions.size()&lt;sessionsPerDay. Zero rejects the first session; gaps inserted for day coverage also count.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::work` — CFG-0649 {#cfg-0649}

- Type: `std::size_t`. Declared default: `= 16000000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical inspection/copy work during construction
- Accepted values: Construction::inspect debits amount from work using the remainder check. Consumption is monotonic per operation;0 cannot read built-in defaults in standard create.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::stagingBytes` — CFG-0650 {#cfg-0650}

- Type: `std::size_t`. Declared default: `= 33554432`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative logical staging bytes
- Accepted values: Construction::allocate debits each requested size without refunding released temporaries. Zero cannot fit the fixed domain. Size representability is checked separately.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedBytes` — CFG-0651 {#cfg-0651}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: shared logical retained-model bytes
- Accepted values: create requires&gt;=sizeof(Context)+sizeof(Ledger). acquire additionally checks bytes+sizeof(Lease) against remaining shared ledger. Candidates are staged before/during acquire; retainedBytes alone does not bound all prior allocations.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedGenerations` — CFG-0652 {#cfg-0652}

- Type: `std::size_t`. Declared default: `= 4`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live defaults generations
- Accepted values: acquire(GENERATION) needs a free count;0 prevents initial create with the built-in generation. Old reader pins retain generations and bytes during updates.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedSchedules` — CFG-0653 {#cfg-0653}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: exact schedule identities
- Accepted values: acquire(IDENTITY) requires count&lt;retainedSchedules. Zero permits the context but no new Schedule identity; inactive-cache collection may retire idle identities before construction.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedRuleModels` — CFG-0654 {#cfg-0654}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: identity/generation rule models
- Accepted values: Each rule model occupies RULES until its last owner retires. Zero permits an empty context but rejects models requiring rules. Candidates first consume staging.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedBundles` — CFG-0655 {#cfg-0655}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: current/candidate/retained bundles
- Accepted values: makeBundle/acquire(BUNDLE) charges a whole snapshot. Zero rejects initial bundle construction; updates may need room for old and new bundles simultaneously.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::inactiveCacheTarget` — CFG-0656 {#cfg-0656}

- Type: `std::size_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: target inactive cached identities
- Accepted values: Soft collection target on explicit factory turns: removes inactive-target, and retainedSchedules pressure may require further retirement. Zero targets all eligible inactive entries; active pins are not revoked.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: Exceeding the idle-entry target triggers collection; exceeding this target itself does not return RESOURCE_LIMIT. Collection can fail with ScheduleError::RESOURCE_LIMIT through work/stagingBytes or other hard quotas, or ALLOCATION through std::bad_alloc. Active entries are skipped.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::queryWork` — CFG-0657 {#cfg-0657}

- Type: `std::size_t`. Declared default: `= 100000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: work units per date query
- Accepted values: queryLimits replaces work with queryWork. Cache lookup also calls inspect;0 may reject during cache lookup. Resource failures do not become an absent calendar result.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::queryStagingBytes` — CFG-0658 {#cfg-0658}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: cumulative logical staging bytes per date query
- Accepted values: queryLimits replaces stagingBytes with queryStagingBytes. DayModel/session/cache construction consumes it;0 may serve an existing cache hit only when no allocation is needed.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedDays` — CFG-0659 {#cfg-0659}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live immutable day models
- Accepted values: acquire(DAY) charges one for a complete candidate. Zero rejects a new retained DayModel; queryStagingBytes independently controls earlier staging. Cache hits may share an existing object.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedSessions` — CFG-0660 {#cfg-0660}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: sessions in all live day models
- Accepted values: acquire(SESSION,...,result.sessions.size()) bounds aggregate count. Zero permits no sessions; prior construction is independently bounded by queryStagingBytes and querySessionsPerDay.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::querySessionsPerDay` — CFG-0661 {#cfg-0661}

- Type: `std::size_t`. Declared default: `= 16384`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physical session records per constructed day
- Accepted values: append requires sessions.size()&lt;querySessionsPerDay, including joined days/gaps. Zero rejects the first record. Separate from abstract sessionsPerDay and aggregate retainedSessions.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dayCacheEntries` — CFG-0662 {#cfg-0662}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: weak entries per day cache
- Accepted values: Zero disables cache publication, not day construction. Positive values bound retained weak_ptr entries plus the new result; expired entries are omitted. They do not strongly retain DayModel.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: Zero disables weak-cache publication after day construction. Reaching capacity reduces copied older entries; filling this cache itself does not return RESOURCE_LIMIT. Constructing the day/new cache consumes work/stagingBytes and hard retention quotas, which can yield ScheduleError::RESOURCE_LIMIT or ALLOCATION. A failed publication compare_exchange is ignored and the complete day is returned.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedDayCaches` — CFG-0663 {#cfg-0663}

- Type: `std::size_t`. Declared default: `= 16`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: live immutable day-cache snapshots
- Accepted values: Every new cache candidate occupies DAY_CACHE in the shared ledger;0 prevents publication when dayCacheEntries is enabled. dayCacheEntries=0 avoids this resource. Readers/competing candidates may retain old snapshots.
- Read timing: ScheduleDataContext captures ScheduleLimits. Construction consumes operation budgets; acquire/ledger tracks published/retained models. Date queries use queryWork/queryStagingBytes.
- Mutability and lifetime: ScheduleDataContext::create copies limits into Context and Ledger; mutating the original argument does not reconfigure the context. Separately, Schedule::getTradingVenues uses limits only for its current call and returns an ordinary string vector.
- Failure: std::expected: ScheduleError::RESOURCE_LIMIT for budget/overflow/length_error; ALLOCATION for bad_alloc. Calendar grammar, NOT_READY and date-range failures have separate errors.
- Consumer sources: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).
