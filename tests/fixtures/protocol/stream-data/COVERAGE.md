# STREAM_DATA production observations

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Inputs](inputs.tsv), [actual Java output](expected/stream-data.tsv), [validation](VALIDATION.md),
[harness](../../../../tools/upstream/stream-data-oracle/StreamDataOracle.java),
[contract](../../../../spec/protocol/STREAM-DATA.md).

17 stimulus cases produce 35 independent parser observations, including 18 strict prefixes. Each
prefix is wrapped in a newly complete type-15 frame; its preceding type-2 description stays complete.
The programmatic local scheme is S=0(v), R=1(y,x,z), E=2(); explicit xdot aliases R.x. Sender plans
are 7=R(x,unknown,y,xdot,x), 9=S(v), 31=Unknown(a,b), 4096=E(), 11=R(), 12=R(x).
Every field is exact type 8. Metadata, external event time and field replacers are disabled.
Descriptions use Java writers; data payloads are hand-authored stimuli. Real BinaryQTPParser delivery
is the oracle: no diagnostic reimplementation or C++ output supplies expected records.

| Cases | Observed surface |
| --- | --- |
| mixed | Sender/local IDs differ; x/alias/duplicate writes end at x=44, y=22; missing z=0. Unknown middle field and unknown record consume all values. ABC repeats across unknown record and S; A/NUL/B repeats across E. R with zero wire fields delivers three raw-zero slots. |
| small, skip-prefix, their strict prefixes | Complete-frame payload truncation in symbol, ID, targeted and skipped values; a prefix ending after the complete unknown record succeeds with no delivery. Isolated high surrogate survives unknown-record skip and repeat into S. |
| empty | Empty complete data message succeeds with no records/callbacks. |
| late-failure | Valid S precedes a truncated R field; Java reports message corruption and delivers no records from this message. C++ retained S is a designed result policy. |
| undescribed, negative-id, invalid-symbol, truncated-id | Message corruption, no records; distinct C++ classifications/positions are projections, not Java diagnostics. |
| nonzero-flags, remove-event | Java accepts raw flags 1 and 2. REMOVE_EVENT follows its alternate time path. Both are explicitly unsupported by C++. |
| zero-flags | Explicit zero flag prefix succeeds. |
| reset-valid, reset-repeat | Two valid messages deliver two records in one callback for whole input; a repeat at the start of message two fails, with the first message's record delivered. |
| string-forms | FC NUL, FD supplementary surrogate pair and isolated low surrogate remain exact units. |
| boundaries, overlong | Compact sign/width boundaries, int extrema, values beyond 32 bits and signed 64-bit extrema; overlong ID/value forms; observed low-32-bit local storage. |

Records store lossless hexadecimal UTF-16, not delimiter-sensitive symbol text. Callback counts and
errors are retained separately from records and final outer input position. Source evidence in
BinaryRecordDesc.readFields establishes readCompactLong before FLD_INT narrowing, including skipped
fields; intermediate overwritten values are not independently observable. P01/P03/P04 and binding
captures remain historical evidence, unchanged. Full optional flags/time, conversion, dynamic session,
provider, huge resource, transport and recovery profiles remain open.
