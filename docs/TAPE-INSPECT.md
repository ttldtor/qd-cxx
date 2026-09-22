# Tape inspection, version 1

[Русская версия](TAPE-INSPECT.ru.md). English is authoritative.

The optional qdcxx-tape-inspect consumes [bounded tape](../spec/protocol/TAPE-READER.md).
Required arguments: --input PATH --profile oracle-sre-v1. No stdin, network or autodetection.
TSV is default; --output-format human uses the existing safe quoted UTF-8 presentation for records
and version text. No arguments and standalone --help are identical, with stdout/empty stderr/exit 0
on successful output. Nonempty invalid/incomplete arguments return 2 before opening any file.

Limits use unsigned decimal size_t, no duplicate options, no unlimited sentinel; zero is valid except
chunk-size. --max-input, --max-frame, --max-frames, --max-descriptions, --max-actions,
--max-name-units, --max-total-name-units, --max-records, --max-fields, --max-values,
--max-symbol-units, --symbol-units, --max-output, --max-construction, --max-properties,
--max-messages, --max-descriptor-units and --max-text-units set independent finite limits.
Help lists defaults. Frame+9 and streamsize arithmetic must fit. Local schema limits are
1024 records, 65536 fields/lookups, 1024 units/name, 1048576 aggregate units. The built-in profile
is the diagnostic S/R/E adapter, not default Quote or inferred schema. Both inspectors may be built
independently with BUILD_TESTING=OFF, without test fetch or Python discovery.

Output is LF/no BOM, canonical decimal, exact UTF-16 lowercase hex in TSV, '-' for empty sequences.
The existing frame/record row grammar and human record escaping are reused without changing the
old inspector. New header: `format<TAB>qdcxx-tape-inspect<TAB>1`; human header is
`qdcxx-tape-inspect human 1 profile=oracle-sre-v1 identities=local`.
After an accepted descriptor emit `descriptor, version-present(0|1), version-hex-or--,
time(NONE|absent), send(15|-), receive(-)`, separated by TAB. Type=tape and opt empty are guaranteed
by acceptance. Human descriptor quotes version (or shows absent), states type=tape opt=EMPTY_SET,
time=NONE|absent and send=15|-. Version is never interpreted as support.

Before final completion, emit `markers<TAB>count` (human `Markers: count=N authenticated-footer=no`).
This reports empty-envelope presence, not authenticity, physical EOF or recording completeness.
The final complete row uses the old six-cell counter grammar under the new format identity.
Successful parsing requires exit 0 after final flush; output failure may physically truncate any row.
Each row is counted against remaining encoded-byte budget before expansion/allocation/write.
Completed data prefix survives a later parser failure; no partial current decoded record is emitted.
A detected error emits no later markers/completion. Read/flush errors remain nonzero.

Exits: 0 success/help, 2 usage, 3 IO, 4 unsupported/sequence, 5 malformed, 6 resource/allocation.
Diagnostics use `incomplete, code, stage, detail, frame-start, accepted-file-bytes, payload-start,
payload-consumed, payload-error` (9 TAB-separated cells); unavailable positions are '-'.
Nested stage detail is the relevant existing enum ordinal, otherwise zero. The accepted-file position
is not substituted for payload consumption. Absolute payload examination equals payload-start plus
relative position. IO diagnostics use their operation label without inventing parser positions.
Read only bounded chunks; at exact input limit peek once to distinguish clean EOF from excess.
No continuation after failure, stripping/reordering of input or protocol parsing in presentation.

Configure with -DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=ON; target qdcxx_tape_inspect,
filename qdcxx-tape-inspect. [Author validation](../tests/conformance/protocol/tape/VALIDATION.md)
records the completed checks; independent review remains pending.

Descriptor strings also have a fixed 4096-byte encoded-body cap in the CLI; the library exposes this
limit separately. Independent tool callers default to local-schema display metadata and field[index]
labels; the CLI explicitly selects oracle-sre-v1. On a failed decoded event, frame and completed-prefix
rows are written before reporting the parser error; a failure writing that prefix takes IO/output-budget
precedence. This does not allow processing any subsequent input.
