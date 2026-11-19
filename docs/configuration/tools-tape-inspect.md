# Tape inspector

[Русский](tools-tape-inspect.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [ContainerLimits](tools-tape-inspect.md#cfg-0885-owner) — `tools/tape-inspect/compressed_input.hpp`
- [Limits](tools-tape-inspect.md#cfg-0898-owner) — `tools/tape-inspect/inspect.hpp`
- [SidecarLimits](tools-tape-inspect.md#cfg-0902-owner) — `tools/tape-inspect/sidecar_time.hpp`
- [TapeSequenceOptions](tools-tape-inspect.md#cfg-0908-owner) — `tools/tape-inspect/tape_sequence.hpp`

## ContainerLimits {#cfg-0885-owner}

[tools/tape-inspect/compressed_input.hpp](../../tools/tape-inspect/compressed_input.hpp)

### `ContainerLimits::compressedBytes` — CFG-0885 {#cfg-0885}

- Type: `std::uint64_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: physically read compressed bytes
- Accepted values: UINT64_MAX is explicitly forbidden on first read, leaving space for one diagnostic excess probe. 0 grants no ordinary bytes, but one probe may establish physical EOF/excess.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::COMPRESSED_LIMIT; the error is sticky and an already produced prefix remains in the result. UINT64_MAX separately yields INVALID_ARGUMENT on first read.

### `ContainerLimits::inflatedBytes` — CFG-0886 {#cfg-0886}

- Type: `std::uint64_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical inflated bytes before QTP consumption
- Accepted values: UINT64_MAX is explicitly forbidden on first read. Output is bounded by inflatedBytes-logicalProduced; an additional diagnostic byte detects excess without declaring a complete source.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::INFLATED_LIMIT; the error is sticky and an already produced prefix remains in the result. UINT64_MAX separately yields INVALID_ARGUMENT on first read.

### `ContainerLimits::expansionRatio` — CFG-0887 {#cfg-0887}

- Type: `std::uint64_t`. Declared default: `= 1000`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: per-member expansion ratio
- Accepted values: After the initial ratioSlack, ceil((output-ratioSlack)/expansionRatio)≤consumed payload is required. 0 does not disable checking: beyond slack it immediately yields RATIO_LIMIT. There is no separate scalar upper comparison.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::RATIO_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::ratioSlack` — CFG-0888 {#cfg-0888}

- Type: `std::uint64_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of initial per-member expansion slack
- Accepted values: output≤ratioSlack passes ratio validation without division; 0 requires ratio compliance from the first output byte. No direct scalar upper threshold exists.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::RATIO_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::gzipMembers` — CFG-0889 {#cfg-0889}

- Type: `std::uint64_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: GZIP members including empty ones
- Accepted values: A new member needs counts.members&lt;gzipMembers; 0 forbids the first. NONE/ZIP do not use the GZIP-specific branch.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::MEMBER_ENTRY_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::zipEntries` — CFG-0890 {#cfg-0890}

- Type: `std::uint64_t`. Declared default: `= 64`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: ZIP entries including directories
- Accepted values: A new local entry needs counts.members&lt;zipEntries; table capacity is additionally bounded by size_t/sizeof(Entry) and retainedBytes. 0 forbids the first entry; the profile separately requires exactly one file.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::MEMBER_ENTRY_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::headerBytes` — CFG-0891 {#cfg-0891}

- Type: `std::uint64_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes per envelope metadata record
- Accepted values: record+new bytes≤headerBytes; every slice must also fit totalHeaderBytes and workBytes. 0 forbids positive metadata processing.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::HEADER_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::totalHeaderBytes` — CFG-0892 {#cfg-0892}

- Type: `std::uint64_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate scanned metadata bytes
- Accepted values: counts.headerBytes+new bytes≤totalHeaderBytes; repeated central-directory comparisons also pass through header(). 0 is an empty cumulative budget.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::HEADER_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::workBytes` — CFG-0893 {#cfg-0893}

- Type: `std::uint64_t`. Declared default: `= 268435456`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate physical/inflated/metadata byte work
- Accepted values: Every work(bytes) checks the remainder; 0 stops positive work. Reads and the inflater bound the next portion by the remainder and separately handle diagnostic excess.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::WORK_LIMIT; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::inflateCalls` — CFG-0894 {#cfg-0894}

- Type: `std::uint64_t`. Declared default: `= 262144`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Actual calls, including zero-output work.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Actual calls, including zero-output work.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `ContainerError`.

### `ContainerLimits::retainedBytes` — CFG-0895 {#cfg-0895}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: scratch and retained ZIP-metadata bytes
- Accepted values: First read requires chunkBytes≤retainedBytes; later names and Entry capacity must fit the retained peak. 0 cannot hold positive chunkBytes.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::ALLOCATION; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::inflaterBytes` — CFG-0896 {#cfg-0896}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of simultaneous zlib allocations including headers
- Accepted values: Before each zalloc, count×unit+sizeof(Allocation) and remaining inflaterBytes are checked. 0 admits no positive allocation; NONE does not need zlib.
- Read timing: The constructor only copies ContainerLimits without allocation/input; first read checks representability and later reads apply counters during consumption.
- Mutability and lifetime: Bounds are fixed for the decoder; destruction neither reads nor closes the borrowed stream.
- Failure: Exhausting this scope yields ContainerError::ALLOCATION; the error is sticky and an already produced prefix remains in the result.

### `ContainerLimits::chunkBytes` — CFG-0897 {#cfg-0897}

- Type: `std::size_t`. Declared default: `= 4096`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive, representable input scratch size.
- Units: bytes; scope defined by field contract
- Declared constraints: Positive, representable input scratch size.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `ContainerError`.

## Limits {#cfg-0898-owner}

[tools/tape-inspect/inspect.hpp](../../tools/tape-inspect/inspect.hpp)

### `Limits::chunkSize` — CFG-0898 {#cfg-0898}

- Type: `std::size_t`. Declared default: `= 4096`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Bounded read buffer, positive.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Bounded read buffer, positive.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `Limits::output` — CFG-0899 {#cfg-0899}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: output bytes including headers and completion
- Accepted values: A complete row must fit remainingOutput and streamsize max; 0 cannot hold a nonempty header. Catalog mode uses a separate CatalogOutput with the same aggregate bound.
- Read timing: inspect copies output into a remainder passed to the formatter before row construction and then checked by emit before physical writing. The catalog path constructs CatalogOutput with its own remainder and passes rowBudget to the formatter.
- Mutability and lifetime: The remainder is spent over one run; the external profile is unchanged.
- Failure: Insufficient remainder can throw OutputLimit during formatting before emit, including the first header; its handler returns code 6, write. The ordinary emit full-row check also yields Failure{6} and 6/write; physical failure yields Failure{3} and 3/write. The catalog path uses CatalogOutput: excess available budget yields SequenceAbort{6,"write","output-limit"}, and sink failure yields SequenceAbort{3,"write","sink-io"}; formatter OutputLimit also maps to 6/write.

### `Limits::tape` — CFG-0900 {#cfg-0900}

- Type: `protocol::TapeLimits`. Declared default: `{{1048576, 1048585}, 67108864, 4096, {16, 8, {4096, 1024}, 8192}, {{1024, 65536, 65536, 1024, 1048576}, 1024, {65536, 1024, 1048576}}, 16777216, {1048576, 1048576, 4194304, 1048576, 4194304, 4194304, {1048576, 4096, 4096}, {65536, 16384}, 16777216, 4194304, 4194304}, 4096}`
- Classification: composition input; nested typed settings, not a new scalar unit. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Explicit finite defaults, including RAW timing attempts.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Not applicable as a scalar range. Nested constraints or returned observations retain their declared meaning.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `Limits::eventUnits` — CFG-0901 {#cfg-0901}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 units of prepared Profile values
- Accepted values: Used only when profileMapping is present: the remainder bounds map; symbol, optional description and statusReason are charged. 0 provides no string capacity. Generic/catalog paths have no such projection or charge.
- Read timing: Copied into remainingEventUnits at inspect start; read before every Profile projection and debited after successful mapping.
- Mutability and lifetime: A new run receives a fresh profile; the current run only spends it.
- Failure: MappingError::RESOURCE_LIMIT/ALLOCATION_FAILURE yields Failure{6}; other mapping errors yield Failure{4}. The field causes no refusal on the generic path.

## SidecarLimits {#cfg-0902-owner}

[tools/tape-inspect/sidecar_time.hpp](../../tools/tape-inspect/sidecar_time.hpp)

### `SidecarLimits::inputBytes` — CFG-0902 {#cfg-0902}

- Type: `std::uint64_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: logical companion bytes including terminators
- Accepted values: inputBytes counts every admitted byte before CR/LF handling; 0 forbids nonempty feed.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::INPUT_LIMIT, stage=input.

### `SidecarLimits::lineBytes` — CFG-0903 {#cfg-0903}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: body bytes per unterminated line
- Accepted values: Construction requires lineBytes≤line_.max_size() and UINT64_MAX; 0 is numerically admitted but first nonempty feed yields LINE_LIMIT. At first content, the entire lineBytes scratch is allocated and must fit retainedBytes.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::LINE_LIMIT, stage=line. An unrepresentable configured lineBytes separately yields INVALID_ARGUMENT; nonfitting scratch is RETAINED_LIMIT and actual allocation failure is ALLOCATION.

### `SidecarLimits::lines` — CFG-0904 {#cfg-0904}

- Type: `std::uint64_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: line attempts including empty/ignored lines
- Accepted values: completeLine requires counters.lines&lt;lines before classifying the line; 0 forbids completing the first line.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::LINE_COUNT_LIMIT, stage=line-count.

### `SidecarLimits::marks` — CFG-0905 {#cfg-0905}

- Type: `std::uint64_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: owning time marks
- Accepted values: Only a colon-bearing line requires counters.marks&lt;marks before mark parsing; an ignored line returns earlier. 0 forbids the first mark.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::MARK_LIMIT, stage=mark-count.

### `SidecarLimits::workBytes` — CFG-0906 {#cfg-0906}

- Type: `std::uint64_t`. Declared default: `= 8388608`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate logical byte visits of parsing
- Accepted values: chargeWork(count)≤workBytes-counters.workBytes; input and additional parsing passes are counted. 0 forbids the first positive work charge.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::WORK_LIMIT, stage=work.

### `SidecarLimits::retainedBytes` — CFG-0907 {#cfg-0907}

- Type: `std::size_t`. Declared default: `= 128`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: retained line-scratch bytes
- Accepted values: On nonempty feed, lineBytes≤retainedBytes; 0 cannot hold positive lineBytes. It bounds the fully preallocated line, including ignored lines.
- Read timing: SidecarTime captures the constructor profile; feed/completeLine then debit cumulative counters.
- Mutability and lifetime: The scanner profile is fixed; failure is sticky and further input cannot resume work.
- Failure: The stated exhaustion yields SidecarError::RETAINED_LIMIT, stage=retained.

## TapeSequenceOptions {#cfg-0908-owner}

[tools/tape-inspect/tape_sequence.hpp](../../tools/tape-inspect/tape_sequence.hpp)

### `TapeSequenceOptions::time` — CFG-0908 {#cfg-0908}

- Type: `SourceTimePolicy`. Declared default: `= SourceTimePolicy::NONE`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: common sequence timing policy
- Accepted values: Only NONE/MESSAGE/FIELD/LONG/TEXT. LONG/TEXT require a companion for every source; other policies forbid companions. One policy governs the list.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Unknown enum is SequenceError::INVALID_ARGUMENT, component=time; companion mismatch is UNSUPPORTED_SOURCE_PROFILE, companion.selection.

### `TapeSequenceOptions::aggregate` — CFG-0909 {#cfg-0909}

- Type: `SequenceReservations`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: full aggregate reservations of every nested scope
- Accepted values: For every occurrence, all TapeLimits, container, scanner and chunk/output/event/probe capacities are summed, including per-value and peak bounds. Each sum must fit its aggregate field; defaults are not automatically multiplied by occurrences.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Admission failure is SequenceError::RESOURCE_LIMIT, component=tape/container/scanner capacity field; an unrepresentable sum/product is UNREPRESENTABLE. Sources have not been opened.

### `TapeSequenceOptions::occurrences` — CFG-0910 {#cfg-0910}

- Type: `std::size_t`. Declared default: `= 256`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: selected source occurrences including repeated paths
- Accepted values: segments must be nonempty and segments.size()≤occurrences. 0 admits no valid sequence; an empty one separately yields EMPTY_SEQUENCE.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: An empty list yields SequenceError::EMPTY_SEQUENCE. Nonempty segments.size()&gt;occurrences yields SequenceError::RESOURCE_LIMIT, component=sequence.configuration.

### `TapeSequenceOptions::pathNativeUnits` — CFG-0911 {#cfg-0911}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native code units per path excluding terminator
- Accepted values: Every nonempty data/companion path.native().size()≤pathNativeUnits. 0 cannot hold a valid nonempty path. NUL is rejected separately.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Admission failure is SequenceError::RESOURCE_LIMIT, component=path.nativeUnits; an unrepresentable sum/product is UNREPRESENTABLE. Sources have not been opened.

### `TapeSequenceOptions::totalPathNativeUnits` — CFG-0912 {#cfg-0912}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: native code units of all data/companion paths including repeats
- Accepted values: Sum of native.size()≤totalPathNativeUnits; the same path in separate occurrences is charged again. 0 cannot hold a nonempty list.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Admission failure is SequenceError::RESOURCE_LIMIT, component=paths.nativeUnits; an unrepresentable sum/product is UNREPRESENTABLE. Sources have not been opened.

### `TapeSequenceOptions::pathEncodedBytes` — CFG-0913 {#cfg-0913}

- Type: `std::size_t`. Declared default: `= 2097152`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of safe hexadecimal native-path encoding
- Accepted values: Each path contributes native.size()×sizeof(native char)×2 to pathEncodedBytes; the product is checked first. 0 cannot hold a nonempty path.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Admission failure is SequenceError::RESOURCE_LIMIT, component=paths.encodedBytes; an unrepresentable sum/product is UNREPRESENTABLE. Sources have not been opened.

### `TapeSequenceOptions::pathStorageBytes` — CFG-0914 {#cfg-0914}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of copied spec objects and terminated native paths
- Accepted values: Sum of segments.size()×sizeof(TapeSegmentSpec) and (native.size()+1)×sizeof(native char) per path≤pathStorageBytes. Overflow is handled separately.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Admission failure is SequenceError::RESOURCE_LIMIT, component=paths.objects/paths.storageBytes; an unrepresentable sum/product is UNREPRESENTABLE. Sources have not been opened.

### `TapeSequenceOptions::diagnosticBytes` — CFG-0915 {#cfg-0915}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: bytes of fixed owning diagnostics
- Accepted values: diagnosticBytes≥4×sizeof(SourceDiagnostic): primary, data-close, time-close, flush. This reserves objects separately from serialized stdout. 0 is rejected.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: diagnosticBytes&lt;4×sizeof(SourceDiagnostic) yields SequenceError::RESOURCE_LIMIT, component=sequence.configuration before copying/opening sources.

### `TapeSequenceOptions::sourceHandles` — CFG-0916 {#cfg-0916}

- Type: `std::size_t`. Declared default: `= 2`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: simultaneously owned physical file handles
- Accepted values: sourceHandles≥2 for LONG/TEXT and ≥1 otherwise. Only one pair is opened at a time; occurrences do not multiply handle count. 0 is rejected.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Insufficient sourceHandles for one active pair (2 for LONG/TEXT, 1 for other policies) yields SequenceError::RESOURCE_LIMIT, component=sourceHandles.

### `TapeSequenceOptions::outputBytes` — CFG-0917 {#cfg-0917}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate output bytes including boundaries and terminal report
- Accepted values: First, terminalBytes≤outputBytes is checked as configuration. The sum of per-segment output reservations must then be≤outputBytes-terminalBytes. After successful admitSequence, header/source-identity bytes are calculated; this sum plus segment reservations must fit the same remainder before copying the list or opening files.
- Read timing: admitSequence reads outputBytes for configuration and terminal-reservation checks. After it succeeds, runTapeSequence reuses the remainder for header/source identities; CatalogOutput then accounts for actual write attempts.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: terminalBytes&gt;outputBytes yields SequenceError::RESOURCE_LIMIT, component=sequence.configuration. Segment reservations exceeding outputBytes-terminalBytes yield RESOURCE_LIMIT, component=output.terminalReservation. If header/identity bytes plus segment reservations do not fit after successful admission, runTapeSequence throws SequenceAbort{6,"admission","identity-output-limit"}.

### `TapeSequenceOptions::terminalBytes` — CFG-0918 {#cfg-0918}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: reserved terminal-output bytes within outputBytes
- Accepted values: terminalReserveWorstCase(92,4)≤terminalBytes≤outputBytes. The reservation is set before publishing the first segment; 0 is rejected.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: An unavailable terminalMinimum, terminalBytes&lt;terminalReserveWorstCase(92,4) or terminalBytes&gt;outputBytes yields SequenceError::RESOURCE_LIMIT, component=sequence.configuration. An admitted terminal reservation reduces publication capacity; later sums may separately trigger output.terminalReservation or SequenceAbort{6,"admission","identity-output-limit"}.

### `TapeSequenceOptions::format` — CFG-0919 {#cfg-0919}

- Type: `inspect::OutputFormat`. Declared default: `= inspect::OutputFormat::TSV`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: safe record-presentation format
- Accepted values: Only inspect::OutputFormat::TSV or HUMAN. admitSequence does not separately validate this enum; runTapeSequence checks it before allocating/copying identity rows and opening sources.
- Read timing: admitSequence checks the complete list without allocation/copy/open; runTapeSequence then uses the admitted profile while opening sources sequentially and producing output. Presentation has the stated separate phase.
- Mutability and lifetime: The profile is borrowed immutably throughout the synchronous call; no live updates exist.
- Failure: Unknown enum is SequenceAbort{6,"configuration","invalid-presentation"} in runTapeSequence. Successful admitSequence alone does not establish presentation validity.
