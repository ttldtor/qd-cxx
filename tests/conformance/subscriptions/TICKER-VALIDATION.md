# TICKER collector qualification

[Русская версия](TICKER-VALIDATION.ru.md). English is authoritative.

The [collector contract](../../../spec/subscriptions/TICKER-COLLECTOR.md) defines
the pure latest-cache/local-view boundary. Qualification uses controlled offline
inputs and locked production Java QD 3.355. It does not establish a complete
network subscription client, upstream multiplexing or arbitrary business mapping.

## Native profiles

| Profile | Result |
| --- | --- |
| Windows x64, MSVC 19.42, Release; client ON, runtime/JSON OFF | Full CTest 43/43; focused TICKER 30 cases / 6808 assertions. |
| Linux x64, GCC 13.3, Debug, ASan/UBSan; client ON, runtime/JSON OFF | Full CTest 43/43; focused TICKER 30/6808. `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer is excluded. |
| Isolated Linux ThreadSanitizer | One case / 32 assertions; four threads each perform 10000 detached-token iterations after collector/protocol owners retire, including final marker release on a worker. `TSAN_OPTIONS=halt_on_error=1`. |
| Production-only install and relocated consumer, both systems | `BUILD_TESTING=OFF`, client/runtime/JSON OFF; actual generic TICKER consumer succeeds after relocation. All 34 files per system retain their hashes after relocation with the custom `lib64` / `public include` layout. No Threads/SDK discovery in the pure production configuration. |
| Pinned C++ formatting | Uncrustify 0.83.0 then clang-format 20.1.8, Check and fixed-point byte comparison for all three changed public C++ files. |
| Doxygen 1.18 | Zero warnings; generated collector, token, delivery and reset-notice pages inspected for parameters, results, ownership, invalidation, threads and failure semantics. |

Only distinct fixed token objects cross threads in the detached-marker test.
The collector, source events, snapshots and intrusive protocol identities remain
serialized. These checks do not qualify concurrent collector operations or a
protocol lease released on another thread. Byte bounds are requested backing,
not process RSS or a latency guarantee.

## Behavioral boundaries

The named native cases cover:

- Both retention modes; pre-add input, latest/equal data, late views, pending
  conflation, remove/re-add with a keeper and last-interest eviction.
- Partial initial/live delivery, the measured default-100 quantum boundary,
  configurable small-quantum scheduling and abandoned prepared offers.
- Whole-frame exclusion on decode failure, schema/epoch rejection, retained
  binding provenance and exact nullable text/opaque values after driver retirement.
- Complete standard eight-field Quote and sixteen-field Profile fixture bodies
  through generic storage and the existing prepared mappings. Empty and null
  values remain distinct.
- Old-plus-candidate byte/frame peaks, exact and one-less capacity, zero limits,
  checked arithmetic, fixed-backing admission/front/pop and independent views.
- Reserved shared stale/reset evidence, exact owner acknowledgment, foreign/stale/
  reused tokens and move transfer.
- Work rejection before array initialization, global view invalidation, repeated
  desired/key/text scans and prepared-offer publication. Rejection preserves the
  prior offer; a later pop acknowledges one record.

Independent frozen-source probes also pass two cases / 140 assertions for the
undercredited-pop boundary and repeated abandoned/zero-work front operations.
They supplement the complete native suites.

## Actual Java comparison

This TICKER qualification used an 83-case / 8255-observation
[capture profile](../../fixtures/subscriptions/CAPTURES.md); later profile extensions
have their own evidence. Seven additional TICKER cases preserve all previous cases;
wire/session artifacts retain their bytes. Actual Capture and fresh Verify run
on Windows JDK 8. This is not a claim of a Linux JVM run.

Native comparison binds 2162 exact coordinates over nine named cases and 58
retrieval phases. Actual post-`LiveSession` input tuples, local command inputs,
canonical UTF-16 record/symbol identities, raw time/value/flags, output records,
counts and applicable availability/capacity/more/cumulative controls are compared.
Coordinate sets must match exactly after the explicit projection; matching only
their intersection is insufficient.

Full selected behavioral comparisons cover the two existing
`ticker-latest-retention-*` cases and five new cases:

- `ticker-equal-value-retention-false` and `ticker-equal-value-retention-true`;
- `ticker-partial-initial-live-cutover`;
- `ticker-pending-readd-with-keeper`;
- `ticker-initial-update-quantum-boundary`.

The two `ticker-raw-remove-retention-*` cases compare representable phases.
Their direct-distributor `data-input-batch-1` carries a nonzero ordinary value
with REMOVE; wire REMOVE supplies default ordinary fields. That input,
`remove-existing-same-payload` and its `same-payload-availability` query are
excluded from the native comparison. Java observes no state change there;
subsequent selected state remains comparable. Native tests separately cover raw
REMOVE flags, indexed removal time, default fields and normalized delivery flags.

Cipher/raw-symbol encoding details and the helper's RecordBuffer-draining `more`
are excluded. Canonical symbols and the actual provider's bounded-retrieval result
remain distinct and are compared where emitted. The 104-key case observes updated
Q000 after the first 100 initial records, before remaining Q100…Q103. The small
configurable quantum test is native/source-derived; it is not an executed Java
configuration claim.

The ordinary trace is 52361 bytes, SHA-256
`cd34fc4e658cd6b75cc30ae3fa733121ae5d361a520975bdabc40d030e8a548c`.
The quantum trace is 122625 bytes, SHA-256
`3858cc5a22561c526496cefb2a331dec1480c9f2ec1a5082d7cad18c6eded03d`.
Both are byte-identical between Windows and Linux.

The separate live-session production oracle also passes against both native
captures. Its 344-byte control stream and 4778-byte callbacks are unchanged;
the refreshed provenance records the actual root build-file hash.

## Consumer and scope

The [compiled driver fragment](../../../spec/subscriptions/TICKER-COLLECTOR.md#compiled-driver-fragment)
is the unchanged function exercised by both installed consumers. It requires a
ready external source and matching schema; it does not compose wire interests or
close I/O. The package exports `qdcxx_collectors` and its TICKER header without
requiring a runtime or test framework.

Custom Java record equality/converters, complete wire equivalence for every direct
distributor operand, TICKER callbacks, HISTORY, shared upstream demand, reconnect,
TLS, public endpoints and production workloads are outside this qualification.
