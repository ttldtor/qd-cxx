# Receiving profile qualification

[Русская версия](DELIVERY-VALIDATION.ru.md). English is authoritative.

Final receiving qualification was completed on 2026-11-03 (Europe/Moscow; execution logs use UTC).
It covers the bounded [application receiver](../../../docs/SUBSCRIPTION-RECEIVER.md),
[recovery CLI](../../../docs/SUBSCRIPTIONS.md) and their existing component contracts.
The source reference remains Java QD 3.355 at the revision in [upstream.lock](../../../upstream.lock).
No public endpoint, registration or external network is required for these checks.

## Native matrix

| Platform | JSON disabled | JSON enabled | Configuration |
| --- | --- | --- | --- |
| Windows x64 | 108 required tests covered* | 109/109 passed | MSVC 19.42, Release, dynamic Release CRT |
| Linux x64 | 108/108 passed | 109/109 passed | GCC 13.3, Debug, ASan and UBSan; LSan disabled |

These are CTest registrations, each of which can contain multiple cases and assertions.
*The Windows JSON-disabled full run passed 107 tests. Its TLS process fixture still expected
clean completion after an observed STREAM gap; that expectation was corrected to the documented
ABORTED outcome. The changed runner's repeated-directory refusal was also corrected. Its final
targeted execution passed; the other 107 unchanged successful tests were retained. Historical
failed runs were not relabelled as passing full runs.

The matrix includes all three collectors, subscription state and wire composition, shared queues,
receiving callbacks, TCP/TLS recovery, schema epochs, stop/failure precedence and existing regressions.
The sustained recovery case uses two healthy clients and a paused slow client over three epochs,
with 48 applied offline interest changes. Collector tests separately establish initial/latest TICKER
delivery and HISTORY overlap, transactions, snapshot restart, snip, removal and interrupted ranges.
No new throughput or latency target is inferred from these workloads.

## CLI and installed application

Real loopback process tests verify complete client ADD before DATA, equal STREAM records, applied
removal followed by excluded late DATA, whole-batch record-target crossing, full Quote projection,
readiness/target refusal, partial protocol, failed stdout and owned process retirement. JSON-enabled
runs also verify complete, snipped and incomplete HISTORY with signed raw bounds. Linux exercises
actual SIGTERM and requires ABORTED/130; Windows external console signalling is NOT_CHECKED because
these test children have no console.

The documented installed C++ Quote example was built and run on both platforms against the relocated
final public package. It verifies no-argument/EN/RU help, invalid input, explicit DNS/CNAME resolution,
the complete eight-field Quote schema, exclusion of an unselected symbol and DRAINED without deadline expiry.
The recovery CLI's explicit DNS/CNAME route also passed on both platforms. The published HISTORY
command was executed with the tracked [TimeAndSale pair schema](../../../spec/schema/examples/native-binary/pair.json),
raw lower bound 0 and two points: all six field values, actual full ADD, current eligible COMPLETE
requested/delivered ranges and DRAINED/no expiry were observed. Only the owned peer port and absolute
schema path were substituted.

## Verified TLS and controlled host

On each platform, the same 12 real process scenarios pass: TLS 1.2/1.3, verified Basic authorization,
the maximum supported canonical token, wrong identity, untrusted chain, authorization denial/timeout,
partial protocol, abrupt EOF, broken stdout and a fresh authenticated successor. Captured client bytes
are compared with independent primitive-codec protocol/authorization/ADD fixtures. No credentials are
released on trust or identity failure. Successor delivery retains actual gap/ABORTED evidence instead
of manufacturing clean STREAM completion.

Closed-host preflight passes 30 fresh-process checks per platform, including all 27 digest methods
and 71 aliases, exact/surplus work, individual shortages, domain/revision/one-shot refusals and environment
gating. Pure input tests pass eight cases/813 assertions per platform; output tests including the numeric
TLS DTO pass 12 cases/2395 assertions. Input headers/translation units also compile with TLS absent or
explicitly zero, without SDK/package includes.

Exact final CLI images retain qualified static OpenSSL inputs, the native startup CPUID initialization
and the selected NULL-input assembly probe's early return before capability/payload reads. Their
post-link 12-case executions pass on both platforms. Different C++ archive producer paths are retained
as provenance; whole-archive byte identity is not required or claimed. The
[closed host contract](../../../spec/cli/TLS-HOST-PROFILE.md) defines the single-owner/fresh-process premises.
These results are not SDK isolation from arbitrary foreign users, security certification, a loader/CRT
deadline or an RSS bound. Local DRAINED does not certify TLS close_notify or peer acknowledgement.

## Compatibility and dependency boundaries

The selected native-to-Java Matrix comparison retains two identical native captures per platform,
12 cases per run and four Windows Java oracle executions consuming both native platforms' outputs.
The 43 relevant native source hashes are unchanged. Repeat/cross-platform byte comparisons agree;
a Linux JVM run is not claimed. Existing [production captures](VALIDATION.md) retain their original scope.

Production-only recovery builds with TLS/JSON disabled pass on both platforms, including their actual
CLI/process checks and feature refusals. The unchanged legacy-only graph was checked separately.
Reconfiguring the final minimal all-optional-OFF graph produced no build work and retained only eight
base archives; its earlier relocated Profile consumer passed seven cases/155 assertions on each OS.
No SDK lookup/link/download is introduced into that minimal graph. All 27 changed C++ files pass the
pinned Uncrustify 0.83.0 then clang-format 20.1.8 pipeline.

The delivered scope is receiving within the documented finite binary/schema/peer profiles. Publishing,
servers, exchange adapters, multiple transport channels, global route provenance, a complete Java event
catalog and Java CLI syntax parity remain separate. Release and user acceptance are not established
by these qualification results.
