# Tape reader Java coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

All 51 inputs use the locked QD 3.355 production FileReader with explicit BINARY, compression NONE,
MAX_SPEED, cycle=false and real local S/R/E DefaultScheme. [Harness and commands](../../../../tools/upstream/tape-reader-oracle/README.md).
Inputs other than original are diagnostic byte stimuli; outputs are actual production callbacks,
not a diagnostic parser. [Validation](VALIDATION.md) identifies clean builds and comparison.

| Cases | Observed result / coverage |
| --- | --- |
| original | Unchanged 137-byte QDCXX-019 FileWriter tape; seven ordered records, late new IDs, reordered local R fields, extremes, Unicode and zero-field E |
| empty, header-only, no-footer, missing-header | Java returns without error; missing-header/no-footer deliver seven; empty/header-only deliver none |
| repeated-header, duplicate-id, redefined-id | Java returns without error and delivers seven; replacement is not the C++ append-only policy |
| unknown-record, undescribed-id | Unknown described record skips both records without error; undescribed data ID reports one message error with no delivery |
| partial-update | One malformed complete description message installs its completed first description; two subsequent records delivered and one message error reported |
| footer-then-data, explicit-zero | Nine and seven records respectively, without errors; empty marker is not an authenticated end |
| properties-0..12 | Missing opt/type/version; nonempty opt; NONE/empty/MESSAGE time; unknown property; duplicate type/opt; null opt; empty version/type. All deliver seven without errors. Duplicate type uses last value; null remains observable |
| send-empty, send-raw, receive-stream | Seven records without errors, retaining actual descriptor sets; not evidence that C++ must support those sets |
| extension | One message error, then seven records; the single trailing zero is a deliberately malformed endpoint extension |
| negative-properties, negative-send, negative-receive, bad-magic | One message error, no delivered records |
| truncated-1/2/20/57/59/65/68/74/80/100/135 | Partial header/description/data envelopes: one stream error. Prefix is zero records through 74, two at 80/100, four at 135 |
| malformed-header-0/1/4/5/49, malformed-data | Complete envelopes with truncated payload primitives: one message error, no delivery |

Properties use compact-long count, message lists compact-int count; message entries contain ID,
UTFString name and a property map. Source evidence: BufferedInput.readProperties,
ProtocolDescriptor.parseFrom/parseMessageListFrom and MessageDescriptor.parseFrom at upstream.lock.
The runtime probes exercise ordinary entries, sets, properties and negative counts; extreme allocations,
all possible malformed Unicode/count widths and general negotiation are not Java runtime claims here.
Existing primitive fixtures cover additional encodings independently.

No Java callback reports descriptor primitive offsets. `case` records exact whole-input bytes; `record`
indices are delivery order, not byte positions. Designed C++ consumed/error projections are separate.
Observer `complete` means the bounded experiment finished, including expected error callbacks.
No recovery, compressed/time-sidecar/default connector/Quote/transport compatibility is established.
