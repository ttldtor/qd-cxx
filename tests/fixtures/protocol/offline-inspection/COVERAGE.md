# Offline Java composition and parsing evidence

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Inputs](inputs.tsv), [Java observations](expected/offline-inspection.tsv),
[raw binary stream](expected/composed.qtp), [validation](VALIDATION.md).

The additive [harness](../../../../tools/upstream/offline-inspection-oracle/OfflineInspectionOracle.java)
uses locked QD 3.355 production BinaryQTPComposer and BinaryQTPParser, not C++ output or a diagnostic
reimplementation. The composer is constructed with automatic description interleaving disabled;
the harness invokes its actual protected describeRecord method for all four sender records inside
a beginMessage/endMessage scope, then uses production visitData twice. Empty composing scopes
collapse in the production composer. This is explicit offline driving, not parser instrumentation.
No upstream source is modified. Sender records are R=0(x,unknown,y,xdot), S=1(v), Unknown=2(a,b),
E=3(). The local profile is S=0(v), R=1(y,x,z) with xdot→x, E=2(), all ordinary type 8.

Eight input records across two messages yield seven observable local records in one callback;
the unknown record is skipped. Four initial description frames and two data frames occupy 116 bytes.
Measured: remapped IDs, unknown field/record skip, y/x ordering, final alias write, missing z=0,
zero-field E, repeated ABC, message-local symbol reset, NUL, isolated high/low surrogates, empty
symbol and int32 extrema. The captured raw .qtp has no tape header/container. EMPTY_SET is explicit;
metadata, field replacers and externally supplied event time are disabled. The empty
visitHeartbeat(new HeartbeatPayload()) call emits no bytes in this composer; empty-heartbeat CLI
coverage uses designed fixtures, not a fabricated Java callback claim.

Existing [STREAM_DATA](../stream-data/COVERAGE.md), [binding](../binding/COVERAGE.md) and
[symbols](../../encoding/symbols/COVERAGE.md) evidence remains unchanged and supplies broader
type-8/malformed/symbol observations. Their semantics are reused; they are not new executions here.
Repeated intermediate writes are source-supported; callbacks expose their final value only.

[designed.json](designed.json) and [designed streams](designed/) are explicitly specified local CLI policy
expectations, not Java captures. They cover strict sequencing, errors/positions, resource boundaries,
completed-prefix output, noncanonical headers and heartbeat envelopes. Initial 53 cases preceded
CLI implementation; 19 additional boundary cases were added during validation. No expected
record semantics are derived solely from the executable under test.

No importer, transport, dynamic session, conversions, nonzero flag support, enormous allocations,
timing or full protocol compatibility is claimed. Empty-file success is a policy check, not populated
semantic coverage. See [tool conformance](../../../conformance/protocol/offline-inspection/COVERAGE.md).
