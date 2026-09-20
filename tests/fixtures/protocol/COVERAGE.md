# Executed protocol coverage

[Русская версия](COVERAGE.ru.md). This English document is authoritative.

QDCXX-006: nine cases, pinned QD 3.355; [driver](../../../tools/upstream/protocol-oracle/README.md), [manifest](cases.json), [validation](VALIDATION.md). Captured evidence is descriptive, not full wire or C++ conformance.

| Case | Executed stimulus and observation | Remaining scope |
| --- | --- | --- |
| P01 | Description plus two ABC data records and a subsequent ABC data frame: 31 bytes. Whole input, all 30 nonempty two-chunk splits, bytewise: 32 schedules. Three ordered records invariant; partial frames wait; batching differs. | Other lengths/symbols, primitive acceptance, malformed/truncated final EOF, flags/time and mixed types. |
| P03-A-B | A R=0/S=1 sends R data and STREAM_ADD to B S=0/R=1; both delivered as local R=1. | Other record names, all subscription/data families, unknown records and re-description. |
| P03-B-A | B R=1/S=0 sends R data and STREAM_ADD to A R=0/S=1; both delivered as local R=0. | Same deferred matrix; no assumption of shared directional map. |
| P04-order | Sender x,y to receiver y,x,z. Wire x,y; receiver raw slots 22,11,0. | Object null slots, numeric representation/default matrices, collisions/duplicate targets. |
| P04-alias | Sender local x.dot/property xdot; receiver x alias xdot at slot 1. Wire xdot,y; receiver 22,11,0. addFieldAlias also changes receiver property name. | Broader alias/property collisions, XML alias generation and metadata propagation. |
| P05-none | No incoming description; real connection chooses x,y; audit 11,22,0. | Dynamic requests, writer-cache/reset reuse, version conversion. |
| P05-zero | Peer R=1 has zero fields; subscription maps to publisher R=0; real connection chooses x,y. | Same deferred scope. |
| P05-x | Peer requests only x; connection describes/sends x; audit 11,0,0. | Requested order/types, conversion matrix. |
| P05-z | Nonempty request for absent z; connection describes zero fields but sends R/ABC; audit 0,0,0. | Other unsupported/missing field combinations. |

All P05 replies use publisher ID 0 and deliver peer-local ID 1. No parser errors or premature close/restart occurred; each connection was closed explicitly after observation. Exact expectations and ordered-record semantic projections are separate files in [expected](expected/).

P02, P06–P13 remain unexecuted, including conversion, negotiation, malformed/reset/PART, auth, sockets and timing integration. Unlisted portions of exercised families remain open. Upstream test suites are skipped. Java round trips are not independent encoding validation or proof of complete compatibility.
