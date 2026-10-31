# Subscription capture profile

[Русская версия](CAPTURES.ru.md). English is authoritative.

These offline synthetic observations characterize the production Java paths at
[upstream.lock](../../../upstream.lock), QD source version 3.355, commit
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. They qualify compatibility inputs for the
[native subscription contract](../../../spec/subscriptions/CONTRACTS.md); native subscription,
collector, transport and lifecycle implementations remain pending.

## Provenance and reproducibility

[Oracle instructions](../../../tools/upstream/subscription-oracle/README.md) describe preparation,
capture and exact comparison. `expected/provenance.tsv` hashes the tool and upstream source files;
`expected/manifest.tsv` indexes every other artifact by relative path, byte count and SHA-256.
Provider class bytes are checked against a trusted preparation receipt before and after each run.
Machine paths, preparation receipts, logs and clock-dependent original frames stay outside goldens.
No captured binary is normalized. The public tree contains only deterministic observations/frames.
Runtime classes self-advertise `QDS-UNKNOWN` because this class-directory profile lacks jar version
metadata; source pin 3.355 does not change that actual runtime value.

Each component has `cases.tsv` (case/evidence kind/production path/outcome) and
`observations.tsv` (case/phase/index/key/typed value). Integers are signed decimal `i:`;
`b:0|1` is boolean, `u:` exact UTF-16 code-unit hex, `x:` byte hex, `t:` ASCII and
`n:null` null. Index -1 records counts in applicable phases. Raw UTF-16 hex preserves
empty strings and unpaired surrogates. Ledgers and binary data are machine evidence and are not translated.

## Production observations

| Cases | Observed boundary |
| --- | --- |
| wire/wire-11,12,16,17,21,22-a-b and -b-a | All six add/remove families, both sender/local record-ID mappings, exact empty/duplicate/UTF-16 symbols and literal AB*/A?. HISTORY_ADD consumes signed raw64 time, including extrema; HISTORY_REMOVE consumes no time. |
| wire/layout-absent, -empty, -x, -z, -object-x | Absent/empty requested layout retains producer x/y; x selects x; z intersects to no fields. Object type10 requested for compact type8 x produces the observed type10/null versus compact audit0; this is not authority for native implicit converters. |
| wire/layout-before-first-use, -after-first-use, -after-reset | Request change is effective before first writer use; the cached descriptor persists afterward, including explicit production parser/composer reset. This does not establish reconnect policy. |
| collectors/stream-* | Every-event order, two overlapping clients and enabled STREAM wildcard/exact overlap without duplicate delivery. Disabled STREAM wildcard produces no ALPHA/BETA fanout. |
| collectors/ticker-* | Latest/initial/re-add behavior depends on explicit storeEverything retention. No universal cached snapshot on every re-add is promised. |
| collectors/history-* | Inclusive signed raw lower bounds, earliest aggregate bound/change, empty/complete/snip/restarted/overlapping snapshots, transactions, removal and interrupted unsubscribe. Untimed records are rejected. |
| session/S01–S04 | Actual full/delta/unchanged/reset protocol descriptors, all 16 heartbeat field-presence masks and observed option round trips. |
| session/S05–S07 | Eight capability intersections, real connection heartbeat/input/inactivity paths and explicit before/at-deadline examination. |
| session/S08–S10 | Production server AgentAdapter/AuthManager with a synthetic realm: completion, refusal and pending authentication timeout/Promise cancellation. |
| session/S11-* | Actual DistributorAdapter: no initial subscription before peer descriptor in this trace; receive-add enables add, remove alone does not; both enable add/remove. Broad compatibility with TICKER_DATA alone does not imply subscription readiness. |
| session/S12-* | Production client LoginManager with synthetic handler: challenge callback, authorization-only delta, refusal/retry and pending cancellation on close. Empty-auth and peer capabilities arrive together in the success case; no independent empty-auth gating effect is inferred. |

Ordinary non-mixed wire parser callbacks expose event_flags0 even on removal: an internal
REMOVE_SYMBOL bit must not be described as callback flag preservation.
The HISTORY outputs are raw QDAgent/RecordBuffer streams and can retain TX_PENDING.
They are not dxFeed IndexedEventModel stable business snapshots. The #ONE/#TWO case proves
distinct record/key isolation, not the complete business-source model.

The client remains pending during a bounded 250 ms observation despite the separate server
AuthManager timeout being 200 ms; closing cancels its Promise. This establishes no universal
client timeout guarantee. The native runtime must own explicit deadlines and readiness policy.

## Wildcard qualification

| Family/profile | Java observation | Native selector contract |
| --- | --- | --- |
| STREAM, wildcard enabled | Star expands and overlaps exact interest without duplicate local delivery in this capture. | Explicit All may be implemented for this proved profile. |
| STREAM, wildcard disabled | No star fanout to ordinary symbols. | Do not silently assume peer wildcard support. |
| TICKER/HISTORY | Star can appear in aggregate subscriptions, but gives no ALPHA/BETA wildcard fanout here. | Refuse All for these unproved combinations; this is explicit native policy, not a claim that Java rejects the literal input. |

Literal glob/regex expansion is not introduced. Reserved leading-star exact selectors remain
subject to the native contract; wire acceptance alone is not a new native public selector policy.

## Profile and evidence limits

This qualifies a modern DESCRIBE_PROTOCOL-negotiated, ordinary read-side collector peer using
plain binary QTP and exact supported record/field types. Native negotiation must advertise only
implemented capabilities. Legacy/headerless fallback, RMI PART wrapping, compressed transport
and implicit business/type converters are outside this selected profile. Required unsupported
input must produce an explicit unsupported/failure result; it cannot be skipped and called complete.
If a required deployment needs one of these capabilities, reserve implementation/qualification
work before claiming that deployment supported.

The base MessageAdapter advertises PART even in ordinary adapters. Source
`qd-rmi/.../impl/ComposedMessage.java` supplies an actual wrapping producer; advertisement
alone does not prove ordinary collector output is wrapped. This capture does not qualify PART
reassembly. Retain the existing compressed offline-tape capability boundaries separately.

Requested-layout connection cases use narrowly documented reflection to suppress variable
protocol/heartbeat metadata and inspect the real production descriptor cache. They are marked
production-instrumented, not complete handshake/reconnect observations. Session cases use the
real public connection factory without that suppression. Synthetic handlers/realms and explicit
timer inputs are fixtures, not claims about sockets, TLS, DNS or credentials on an external service.

Only the two malformed HISTORY cases rewrite bytes; their diagnostic classification remains
explicit. The production parser reports message error21 and withholds the failed frame's callback;
an earlier valid PREFIX frame remains observed. These cases supplement the unmodified successful
production captures and do not alone establish compatibility.

[Coverage scenarios](../../conformance/subscriptions/CONTRACTS.md) keep local/native implementation
qualification separate. [Validation](../../conformance/subscriptions/VALIDATION.md) records executed
profiles. No external endpoint, production tape or real credential was used.
