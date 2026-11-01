# Selector and canonical-symbol validation

[Русская версия](WILDCARD-VALIDATION.ru.md). English is authoritative.

Date: 2026-11-01 UTC. Scope: the immutable [selector profiles](../../../spec/subscriptions/SELECTORS.md),
stateless Penta encoder, explicit STREAM All, canonical Exact and whole-event domain guards.
Implementation-author native execution and qualified source/Java observations are distinguished below;
documentation checks bind the actual compiled excerpts, links, translations and preserved prior contracts.
Historical desired-state/writer/collector validation retains its original scope.

## Final native checks

Both complete configured test graphs used CLIENT ON, runtime OFF, JSON loader OFF and CONNECT OFF.
This does not qualify a runtime/TLS/JSON-enabled full subscription application.

| Profile | Complete CTest | Focused selector cases |
| --- | --- | --- |
| Windows MSVC 19.42, x64 Release | 48/48; exit 0 | 22 cases / 6,845 assertions; exit 0 |
| Ubuntu 24.04 GCC 13.3, x64 Debug ASan+UBSan | 48/48; exit 0 | 22 cases / 6,845 assertions; exit 0 |

ASan used `detect_leaks=0:halt_on_error=1`; UBSan used `halt_on_error=1:print_stacktrace=1`.
LSan was excluded. The last test-only addition exercises actual postdecoder Penta DATA star with
raw time 1, value 12 and flags 0; the approved production implementation remained unchanged.
Isolated TSan passed 2 cases / 77 assertions on the same production code before that test-only addition.
It covers new cross-thread immutable membership and compound detached-batch paths, not concurrent
owner calls, arbitrary callbacks, all source algorithms or a full-suite race-freedom guarantee.

The pinned Uncrustify 0.83.0 / clang-format 20.1.8 fixed-point/check pipeline covered 18 selected public
C++ files. Doxygen 1.18 generated eight inspected API pages with zero warnings, including encoder,
membership, state/writer, three collectors and queue profile contracts. These scoped results are
not an all-build warning-free or binary-ABI claim.

## Qualified production comparisons

The Java reference is QD 3.355 at the [pinned commit](../../../upstream.lock). Actual production
PentaCodec, BinaryQTPComposer/BinaryQTPParser and Matrix Stream paths are linked in the
[selector contract](../../../spec/subscriptions/SELECTORS.md). Selected captures explicitly toggle
the real Stream wildcard switch; no descriptor, local text spelling or remote permission ACK is inferred.
The repeat captures preserve the same provider/module source identities. A later qualified edition
clarifies earlier named captures; old observations are not silently relabeled as broader support.

| Comparison | Exact scope on each native platform |
| --- | --- |
| Penta vectors | 154 vectors / 616 defined coordinates |
| Whole QTP envelopes | 14 complete byte-identical files: six legacy Exact, six canonical Exact, STREAM All add/remove |
| Two-owner STREAM projection | 29 rows / 232 coordinates across 17 phases: nine actual postdecoder inputs and eight output/count observations |
| Canonical TICKER/HISTORY Exact projection | 10 rows / 77 coordinates: all actual input ciphers/counts; selected outputs as qualified below |
| Identical-input concrete DATA star | Three rows / 24 coordinates with actual parsed input, delivered output and count |

The final strict selected-set comparison therefore checks 949 semantic coordinates plus all fourteen
whole envelopes on each operating system. Platform trace bytes agree. This compares actual parser
DATA and actual native callback/collector observations; it does not substitute subscription aggregate
API projections, stored-symbol metadata or pre-parser operands for delivered data.

The STREAM schedule exercises two All owners, same-client Exact overlap, identical consecutive
records, a new unencodable future symbol, a wrong record, first/last All removal and preserved Exact
demand. Canonical Penta DATA star is observed as one concrete delivery; literal FD star remains a
different mapping-domain case and is outside the native canonical incoming profile. Legacy FD Exact
and canonical Penta Exact are not automatically collector-key equivalent merely because decoded
UTF-16 text agrees. This limitation is documented rather than generalized to arbitrary providers.

TICKER/HISTORY comparisons use selected canonical Exact positives and a fixed HISTORY `hs` snapshot
profile, lower bound 0, times 3/2/0 and flags 4/0/10. All actual INPUT ciphers and count scalars compare.
HISTORY non-count projected OUTPUT ciphers are excluded: a synthetic END has no raw source cipher.
Native drained `completeRequestedRange` is asserted separately. No Java rejection/API parity,
arbitrary-adapter normalization or full native Java HISTORY producer equivalence is established.

Fresh default subscription/control oracle capture and Windows/Linux native verification also passed
after source-binding provenance refresh. Established control and callback bytes were preserved.
This default-path regression evidence does not widen the selected All/canonical comparison domains.

## State, failure and finite-resource coverage

[Native tests](../wildcard_subscription_tests.cpp) check legacy defaults and all immutable profiles,
explicit All versus Exact-empty/reserved leading star, literal `AB*`/`A?`, unsupported TICKER/HISTORY
All, duplicate/foreign contexts, whole writer bytes, sparse IDs and actual incoming cipher/text domains.
Matched Exact/All activations are captured once; removing either preserves the other original choice,
while remove/re-add/reset cannot resurrect it. Structural `anyOf` is tested separately from actual matching.

All counts as one entry with zero desired symbol text; complete input/payload/queue/work limits remain.
Whole-event domain/work/arithmetic refusals are checked before ownership/candidate/epoch/stale changes,
including preserved existing offers and HISTORY resource refusal without a STALE transition. Queue
tests distinguish caller-owned direct refusal from already accepted UNSTAGED shared-source failure,
with typed nested causes and normal gaps/discards. No unbounded symbol enumeration, hidden credit
refill or new cache/snapshot algorithm is inferred. Source-compatible aggregate initialization does
not preserve object sizes: rebuilt consumers account current selector/membership metadata.

## Installed consumers and relocation

Fresh Release production-only packages used runtime/JSON OFF on Windows and Linux. The two CLIENT
packages each contained 43 files; the two pure CLIENT OFF packages each contained 38. All four actual
installation directories were renamed before fresh consumer configure/build/run; the original four
paths were absent. Every installed hash survived the move, and all four consumers exited 0.
The four relocated prefixes contain 162 files in total; original copies are not double counted.

Actual imported archive locations, compile includes and link dependencies refer to the relocated
prefixes without original/source SDK leaks. Missing optional inherited-include or leaf-link-closure
properties are property absence, not missing imported archives. Pure packages expose no queue/client
targets or headers and require no networking SDK or Threads discovery.

The [documented queue/collector functions](../../../spec/subscriptions/SELECTORS.md#qualified-stream-queue-fragment)
are verbatim excerpts from the actual compiled consumer. The preamble uses a known `Sample` record
with three full type-8 fields and two Penta `ALPHA` STREAM rows, time 1/value 10. Exact plus All
delivers each concrete input once. The CLIENT example retains callback data, drains its finite local
cutoff and releases immutable backing on another thread after queue/callback-owner destruction;
the pure example stages a whole admitted event and acknowledges bounded borrowed offers. These
fragments require their explicit caller context and do not supply standalone `main` or transport completion.

No errors were detected in these executed checks. Final integration review, owner acceptance and
release remain separate. No throughput, RSS, hard wall-clock bound, full network facade, peer
permission-negotiation or blanket provider compatibility claim follows.
