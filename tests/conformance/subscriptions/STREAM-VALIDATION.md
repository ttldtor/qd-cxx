# STREAM collector validation

[Русская версия](STREAM-VALIDATION.ru.md). English is authoritative.

The native test exercises the [collector contract](../../../spec/subscriptions/STREAM-COLLECTOR.md)
through actual complete `LiveSession` events and desired-state mutations. It adds
no decoder, dispatcher or socket. Initial collector-only qualification passed **41/41** on
Windows x64 MSVC 19.42 Release and Linux x86-64 GCC 13.3 Debug ASan/UBSan; main LSan
is OFF. The original collector had 15 cases, including full standard Quote admission/mapping.

The prepared-batch extension adds six cases for atomic prefix acknowledgement,
stale single/batch interleavings, work preflight, zero/withdrawn prefixes, provenance
and move invalidation. Its current 21-case qualification runs in the client-enabled
42-test profile on both platforms; see [client validation](CLIENT-VALIDATION.md).

## Behavioral coverage

- Identical consecutive records remain separate and interleaved records preserve order.
  Unrequested/pre-add input is not a latest-value cache.
- Effective revisions withdraw queued activations; failed/no-op commands preserve
  offers. Remove/re-add does not revive old data. Two collectors with independent
  provenance do not acquire each other's interests.
- Whole-frame record/frame/backing limits have exact/exceeded cases and reserved
  terminal evidence. Work exhaustion preserves input/queue/offer for retry.
- Last event/snapshot release is preflighted. Earlier binding generations and
  retained values survive session destruction; a drained collector keeps its epoch pin.
- Empty/control/wrong-family/moved events, zero capacities, unusual exact UTF-16,
  fixed-ring reuse and continued traffic beyond any single turn are checked.
- Factory allocation failure preserves desired ownership; stage/front/pop run
  without allocation. Malformed current frames publish no decoder prefix.
- The complete eight-field default Quote schema consumes original `time-1.qtp`
  type2/type15 envelopes. After session destruction, all raw/type/context values,
  prepared Quote mapping and independently retained result after pop are checked.

## Actual production comparisons

`CollectorOracle` executes locked Java QD STREAM/QDAgent/QDDistributor/RecordBuffer
paths. Three cases are selected: `stream-every-event-order`,
`stream-duplicate-two-clients`, and `stream-identical-filter`. The last captures
three identical ALPHA values, unrelated record/symbol input and no pre-add replay.
All earlier collector rows and wire/session artifacts remain unchanged.

The native test emits actual admitted input, subscription and offered/popped output
semantic tuples/counts. Windows/GCC traces are byte-identical: 12,723 bytes,
SHA256 `ba2aa451c2a265a51c93534a102f749009dba3871d8d4a15c08d3331cfa3e1b3`.
The independent comparison matches 185 coordinates in 20 phases across the three
cases. Cipher/raw-symbol representation and QD pull-loop `more` markers are
explicitly excluded; queued local membership barriers remain normative local
behavior, not a new Java observation or shared-upstream MUX proof.

Fresh production Capture and Verify cover 76 cases/5,681 observations in the whole
subscription profile (22 wire, 22 collector, 32 session cases). Separate fresh
native-control verification accepts Windows/GCC live-session output; 344-byte
controls and the 4,778-byte production callback artifact remain unchanged.
Java executions use Windows JDK8 for both native captures.

## Build, ownership and documentation

The initial collector-only production builds and relocated installed consumers passed on both platforms,
including factory/add, real feed/stage and explicit front/pop. Each package has
27 hash-preserved installed files and needs no runtime/JSON SDK. The optional TCP
profile coexists in a separately checked MSVC configure/export tree; that check
does not rebuild or execute the SDK. Existing runtime execution paths are unchanged.

Independent native execution covers the unchanged 13 contract cases and the new
Quote case, plus two protected session-contract cases. Pinned C++ format/check and
idempotence, Java format/check and Doxygen 1.18 with zero warnings pass. Generated
API pages and the compiled installed-consumer example are inspected separately.

Requested capacities and byte-work allowances exclude allocator overhead, CPU
instruction counts and RSS. Destruction/move assignment is outside turn credits.
GCC Release also emits an existing `wide_decimal.cpp` conversion-buffer warning;
that source is unchanged and the full sanitizer regression suite passes. No new
wildcard, TICKER/HISTORY, MUX or full-subscription qualification follows. Local
callback qualification is described separately in [client validation](CLIENT-VALIDATION.md).
