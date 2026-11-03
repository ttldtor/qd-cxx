# Local instrument-profile writer

[Русская версия](WRITER.ru.md). English is authoritative.

## Observable contract

`composeInstrumentProfiles(span<const InstrumentProfile>, options)` prepares one complete,
owning, uncompressed UTF-8 document without I/O. Its expected result contains either all bytes
(including `##COMPLETE\r\n`) or a fixed-size typed diagnostic; no partial document is returned.
Input profiles and their getter views are borrowed only for the call. Callers serialize mutations;
no callbacks, retained spans, queues, background work or global configuration are involved.

Compatibility means canonical `getField` text, exact type/custom names, profile order and CSV
escaping as observed from the pinned Java writer/composer and reader. It does not preserve raw
presence, signed-zero bits or numeric spelling: empty/default fields follow the existing value
model on reading. The Java writer uses lexically sorted UTF-16 type headers, standard enum field
order with SYMBOL always present, then sorted nonempty custom names. Fields are unioned per type;
profiles retain input order. CSV quotes a cell exactly when it contains comma, quote, CR or LF;
quotes double, embedded newline bytes remain exact, and record separators are CRLF.

An empty input produces only the completion marker. Unknown, empty and quoted types are supported,
including ordinary names beginning with '#'. Nonempty custom names and the empty custom name are
case-sensitive and remain distinct from standard names.

## Explicit native fidelity boundary

The value model still represents all its existing values. The writer rejects only these observed
classes before any publication:

- INVALID_TYPE: exact types `##`, `##COMPLETE`, or a type starting '#' and ending '::=TYPE'.
  Actual Java writer output for these types is rejected by its production reader.
- UNREPRESENTABLE_UTF16: an unpaired high/low surrogate in a type, name or value. Actual Java
  UTF-8 output replaces it with '?', losing canonical text. Valid surrogate pairs remain exact.
- REMOVED_FIELDS: a REMOVED profile with nonempty data other than TYPE/SYMBOL. Actual Java composer
  drops those additional fields. Ordinary REMOVED tombstones and empty/default extra fields work.

These are reviewed native writer restrictions, not claims that Java rejects every input above.
There is no silent normalization/filtering or claim of equivalence for every value-model state.
Actual writer bytes and Java reader outcomes are retained by the
[writer oracle](../../tests/fixtures/ipf-writer/README.md).

## Bounds and failure ownership

Engineering defaults are configurable per call:100000 profiles,256 types,256 fields per format
(including TYPE),1Mi retained format UTF-16 units,16Mi total input retained UTF-16 units,64Mi output
bytes and32Mi logical work units. These are engineering choices, not owner workload preferences.
Each input profile is charged by actual retained standard text plus custom names/values and
standard/custom field visits before allocating copied custom names. Actual nonempty custom count
is checked first. The names copy is bounded by that profile's charged retained units and count;
format copies have their own aggregate ledger. Checks precede growth with overflow-safe accounting.
Logical work charges inspected UTF-16 units, field visits and emitted bytes; it is not an exact
instruction count, allocator capacity/RSS measurement or blocking-I/O deadline. Failed work is not
refunded. Allocation failures produce ALLOCATION_FAILURE; no partial owned document is published.

Diagnostics identify a zero-based profile and field where available; no string allocation is
needed to retain a diagnostic. Input values remain unchanged on success and failure. The public
value metadata `retainedUnits()` measures standard text plus custom names/values in UTF-16 units,
excluding configuration/container overhead; `customFieldCount()` counts retained nonempty custom
values, including a possible empty name. These getters allocate nothing.

## Local publication

`writeInstrumentProfiles(ostream&, profiles, options)` prepares the entire body and marker before
the first I/O. It borrows the caller stream only for this call, never closes/seeks/rolls it back,
and preserves its exception mask. It writes/flushes the body before attempting the completion
marker, then writes/flushes the marker. A successful result means all these operations succeeded.
After each write/flush it checks fail/bad state, including exceptions swallowed by iostreams.
Escaping bad_alloc maps to ALLOCATION_FAILURE; a suppressed exception visible only as stream
failure maps to IO_ERROR. It never clears stream state to disguise a failure.

`confirmedBytes` counts only fully successful ostream.write calls. A failing call may already
have external side effects, so it is a lower bound, not an exact physical cursor. Any failure
returns complete=false. A late marker/flush failure may leave a complete-looking marker in the
external bytes; no rollback, transaction or atomic external-stream publication is promised.

`writeInstrumentProfilesFile(path,...)` also prepares everything before opening, uses an owned
binary/truncating ofstream and closes only that file. Existing file contents can be lost once open
succeeds. Open, write, flush and close failures are observable and complete becomes true only after
the owned close succeeds. Preparation failure does not open/truncate the destination. There is no
temporary-file rename or filesystem atomicity promise. URLs, HTTP, compression, live collection and
writer callbacks are outside this API.

