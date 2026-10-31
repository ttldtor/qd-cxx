# Subscription contract coverage and qualification scenarios

[Русская версия](CONTRACTS.ru.md). English is authoritative.

This document qualifies the [native design contract](../../../spec/subscriptions/CONTRACTS.md). It is a finite support/pending/excluded ledger and a table of expected local traces. The [production Java captures](../../fixtures/subscriptions/CAPTURES.md) provide selected-profile evidence; native subscription/collector implementation and the local-trace harness remain pending. Existing [binary coverage](../protocol/real-tape/COVERAGE.md) and [validation](../protocol/real-tape/VALIDATION.md) retain their original profiles.

## Scope matrix

“SUPPORTED foundation” means only the linked existing lower-layer capability. Pure desired state is implemented; other subscription/runtime capabilities remain pending as stated per row. “Design fixed” closes its local design decision, not its implementation or upstream compatibility gate. Required pending rows cannot be reclassified excluded merely to claim complete support.

| Area | State | Meaning / prerequisite |
| --- | --- | --- |
| Schema/record provenance and indexed layout | SUPPORTED foundation | Existing immutable identity and explicit time-layout validation only. |
| Generic binary data families/flags | SUPPORTED foundation | Existing selected wire decoding; no collector delivery follows from this row. |
| Desired mutations and membership barriers | SUPPORTED pure state; PENDING dispatch barriers | [Bounded state](../../../spec/subscriptions/STATE.md) implements atomic/upsert/revision/eligibility; callback cutovers require the later dispatcher. |
| Callable/lease/host lifetime | PENDING implementation; design fixed | Implement guards, nonblocking handle release and quiescence in upper layers. |
| Resources, command/control admission and live-frame policy | PENDING implementation; design fixed | Implement resident/turn limits, reserved credits/control storage and whole-frame admission. |
| Desired/sent state and epoch ownership | PENDING implementation; design fixed | Fresh connection state and reset barriers are fixed; selected peer/auth observations are recorded in the capture profile, with native implementation pending. |
| Six subscription messages and readiness/auth/heartbeat | SUPPORTED composition; OBSERVED Java; PENDING live session | [Composer](../../../spec/subscriptions/WRITER.md) emits six subscription types and descriptions. Native negotiation/auth/heartbeat/readiness remains pending; selected Java observations and PART/compression/converters/legacy limits remain in the capture profile. |
| STREAM collector | OBSERVED Java profile; PENDING native implementation | Every-event/two-client/overlap traces captured; native cutover, ownership and resource tests remain required. |
| TICKER collector | OBSERVED Java profile; PENDING native implementation | Initial/latest/re-add and explicit retention traces captured; no universal cached re-add snapshot. |
| HISTORY collector and raw lower bound | OBSERVED Java profile; PENDING native implementation | Raw bound/snapshot/transaction/removal/aggregation traces captured; raw QDAgent output is distinct from a business-event model. Native completeness/reset tests remain required. |
| Wildcard family/peer combinations | OBSERVED Java profile; PENDING native implementation | Enabled STREAM fanout/overlap is proved; no TICKER/HISTORY fanout. Native All is restricted to proved profile; Java can still accept literal star in aggregate subscriptions. |
| TCP/executor/TLS/configured authentication | PENDING runtime decision and implementation | Select one supported backend/profile, trust/cancellation policy and dependency-OFF behavior before adapter code. |
| Java object/business conversion and full event catalog | EXCLUDED | Preserve existing opaque/generic data boundaries; no claim of all business-event APIs. |
| Server/publisher/RMI/multi-endpoint routing | EXCLUDED | These are separate product capabilities, not prerequisites for the selected read-side client. |
| Exactly-once journals and callback-side-effect retry | EXCLUDED | No measured end-to-end acknowledgement/resume contract justifies them. |

## Local trace catalog

Trace IDs are stable scenario names. Numbers in small examples are logical test inputs, not API defaults, recorded measurements or process-memory claims. Concrete native/Java tests must execute the applicable traces on their final source and distinguish intentional native policy from measured upstream behavior.

| Trace | Boundary | Given / action | Required observation |
| --- | --- | --- | --- |
| SC01 | Idempotence | Empty map at revision 7: add A; repeat identical add A; remove absent B. | Revisions 8, 8, 8; one membership activation and no absent-remove side effect. |
| SC02 | Atomic replacement | Published A; replace with B plus an invalid context/selector or contradictory duplicate. | Reject the whole command; A/revision/activation unchanged, temporary owners released, attempted work charged. |
| SC03 | Peak allocation | Toy charged payload units: old 4, candidate 6, limit 8; other charges nonnegative. | Reject peak at least 10 even though final 6 would fit; published old 4 survives. No release-old-before-validation shortcut. |
| SC04 | Callback self-remove | Callback holds borrowed batch; it queues remove of itself and returns; another matching batch is pending. | Executing callable/batch survive return. Apply remove afterward; no later callback for retired activation. Current batch is not retroactively revoked. |
| SC05 | Remove/re-add ABA | STREAM A has queued activation 10; remove A completes; add A creates activation 11. | Old activation 10 item is withdrawn, not delivered as new activation 11; unchanged other keys keep eligibility. |
| SC06 | Failed current frame | Earlier frame admitted; next data frame decodes record 0 then fails on record 1. | Publish none of the failed frame; earlier admission/callback effects remain. Decoder prefix is diagnostic only on this path. |
| SC07 | Connection reset | Epoch 17 has pending A and a held owning lease; epoch 18 reuses sender ID for B. | Reset/gap before epoch 18 data; old pending data discarded/accounted, old lease remains a valid epoch 17 value and retains its quota. |
| SC08 | Binding update | Same connection/context: normalized A from binding 3 queued; binding 4 changes sender mapping. | Owning A remains A if membership is valid; never reinterpret through binding 4 or silently drop only because binding changed. |
| SC09 | Slow consumer | A/C release promptly; B withholds owning leases to its data-credit limit. | B overflow/gap/terminal is observable from reserved controls; A/C retain their reserved credits. B leases remain charged until release. |
| SC10 | Saturated control request | Ordinary command/data queues full; release last public handle or request abort. | Allocation-free control request is admitted once; owner progress retires safely. Completion is not asserted before callbacks/operations quiesce. |
| SC11 | Failure precedence | Non-cancellation parse failure observed; then graceful stop; then close failure. | Parse cause remains primary, close cause secondary; no successful drain result; terminal exactly once after quiescence. |
| SC12 | Graceful cutoff | Stop applies with admitted data and 10 bytes of an incomplete input frame. | Drain eligible admitted prefix; count 10 unadmitted discarded bytes; do not manufacture a successful EOF/frame. Required sink-flush failure becomes failure. |
| SC13 | Non-preemptible callback | Callback has not returned; deadline elapsed on the supervising clock. | Request escalation but no quiescence/terminal-completion claim until callback returns. Bounded test harness releases its barrier and cleans its processes. |
| SC14 | Continuing work | Many bounded turns cumulatively exceed any one turn's work quota. | Continue while each operation/resident limit holds; no implicit finite-file lifetime exhaustion. Same-turn failure/retry does not refund work. |
| SC15 | Context isolation | Independent schemas have equal local IDs/shapes; a key from run A is submitted to run B. | Reject incompatible context without mutation/emission; schema-copy identity still matches its original context. |
| SC16 | History attribute change | Add existing HISTORY key with a different raw lower bound; then repeat the same entry. | One revision/activation change then no-op; no conversion to milliseconds. Old-view completeness invalidated; precise peer snapshot trace remains pending. |
| SC17 | Reset coalescing | Several resets occur before consumer can observe the pending control interval. | Keep earliest affected/newest epoch and skipped-reset count; finite storage, unknown-loss marker as needed; no new data crosses an undisclosed reset. |
| SC18 | Accepted versus applied | Port accepts a command; close applies before that command; no peer acknowledgement exists. | Completion says cancelled-closing, not applied/emitted/peer-accepted; result remains readable without retaining endpoint operations. |

## Reproducible acceptance workload

The fixture profile `SUBSCRIPTIONS_ACCEPTANCE_V1` fixes a starting workload for later implementation qualification. It is configurable test input, not universal production defaults, a throughput SLA or evidence that a future implementation already handles these values. Every run records its actual limits, source/schema/fixture hashes, compiler/build profile, seed and whether clocks/peers are simulated.

| Parameter | Fixture value / unit |
| --- | --- |
| Synthetic data | 1,000,000 records; deterministic seed 0x51444358; exact symbols S000…S063 |
| Schemas | Explicit ordinary Quote/Profile plus a synthetic indexed record with high/low signed32 time fields and one signed32 payload; no time/index semantics inferred from names |
| Clients | 3: prompt A, withheld-credit B, membership-churning C; at most 3 reserved registrations |
| Desired capacity | 256 keys per client, 768 aggregate; symbol capacity 64 UTF-16 units per selector |
| Per-command/turn work | 65,536 key visits; 1,048,576 examined/copied UTF-16 units; 16 commands, 4 frames, 32 callbacks per turn; codec-specific bounds recorded separately |
| Input/staging | 1,048,576 bytes per frame, 256 records per frame, 64 fields per record; 8,388,608 aggregate charged resident bytes; simultaneous maxima need not all be admissible |
| Data credits | 32 items and 65,536 bytes per client; 96 items and 196,608 bytes aggregate, including retained library leases |
| Control credits | 2 slots per client, 6 aggregate; separate bounded completion capacity 16 per client |
| Retired metadata | At most 16 outstanding binding/epoch metadata generations in the context; held leases keep required reservations |
| Churn/recovery | 100 scripted resets with sender-ID reassignment; C changes 256 times; logical interests/leases released explicitly between reusable registrations |
| Virtual timing | 1,024 input records per virtual second; heartbeat 1,000 ms; inactivity 3,000 ms; retry delays 10/20/40/80 ms capped at 80 ms, at most 8 attempts per episode; these are fixture parameters only |

Run separate STREAM, TICKER and HISTORY episodes with fresh state and 1,000,000 input records each. One fixture client is one registration; all three registrations use the episode's family. STREAM cycles Quote/Profile/indexed records, TICKER cycles ordinary Quote/Profile, and HISTORY uses the explicitly indexed synthetic record with raw lower bound zero. For record ordinal i starting at zero, select the record by floor(i / 64) modulo the eligible record count and symbol by (i + (seed modulo 64)) modulo 64. In the indexed record, high time word is zero, low time word is floor(i / 64) modulo 32 and payload is (i + seed) modulo 1024. Bounded repeated raw-time keys exercise continuing updates without requiring an unbounded history cache; these values imply no business time conversion.

A/B request all eligible keys. C alternates the first/last 32 symbols; its replacement count advances when floor((i + 1) * 256 / 1,000,000) advances. Reset count similarly advances using factor 100. Apply these scripted controls after that record and before the next; when both occur, apply the membership change before reset so resubscribe uses the new desired set. Release outstanding C leases before retiring/reusing its registration. Record the concrete ordinary-field fixture values and exact framing/flags in the captured fixture manifest; they are not supplied by a guessed protocol encoder.

Use synthetic values/credentials and a controlled Java/scripted peer. Codec/capability snapshots and exact wire expectations are produced during compatibility qualification, not guessed from this workload. A real public endpoint is supplementary qualification requiring its own access/data permission; it is not contacted by writing these contracts.

A/C continue within their reserved credits when B reaches its own queue/lease bound. Shared-state exhaustion is tested separately as a visible shared-scope failure. Repeat boundary cases with zero/exact/exceeded limits and failed candidate peaks; long traffic must not fail merely because cumulative input exceeds a per-turn quota. Run fake-clock control/state cases first; real socket/TLS cancellation/closure belongs to the selected adapter's tests.

## Evidence required for implementation qualification

Record production parser/composer/collector inputs and outputs at the locked revision, options/auth/runtime settings, native actual results and exact comparison scope. Cover empty/duplicate/removal/membership cases, TICKER retained versus absent snapshots, HISTORY lower-bound and transaction/snapshot/remove/empty/snip behavior, wildcard restrictions, raw sender reassignment, credentials/certificate failures, stop/flush and resource recovery.

Independent ownership/state review and affected Windows/Linux checks remain mandatory when the code exists. Synthetic captures must not contain private original tape data, secrets or application paths. Static document/link checks establish specification consistency only. Doxygen currently reads headers, so these standalone Markdown specifications are not a newly built API reference page.
