# Local IPF reader contract {#local-ipf-reader}

[Русская версия](READER.ru.md). English is authoritative.

The independent `qdcxx_ipf` reader accepts plain, uncompressed UTF-8 Instrument Profile Format.
Its references are pinned Java QD 3.355 revision `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`:
[InstrumentProfileParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/impl/InstrumentProfileParser.java),
[CSVReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/CSVReader.java),
[InstrumentProfileReader](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileReader.java).
The [42-case actual Java corpus](../../tests/fixtures/ipf-reader/README.md) records input bytes,
ordered whole profiles/controls, first errors and the full reader's completion result.
Profile values use the separate [value contract](PROFILE.md).

## Grammar and data

A format row begins with `#<type>::=TYPE`, followed by exact field names. Each subsequent data
row starts with that type and must have exactly its declared remaining field count. Formats can
be redefined atomically, with custom/empty type names and custom/empty field names. Undefined
formats and forbidden metadata/control format names fail explicitly. A known format is looked up
before treating a leading `#` as metadata; custom metadata-looking types remain usable.

CSV uses comma separators and double quotes. A quote may start only at a field's first unit;
paired quotes inside quoted text produce one quote. Text after a closing quote, unquoted quotes
and EOF inside a quoted field fail. Quoted fields preserve commas and physical CR/LF/CRLF units.
Outside quotes all three newline forms end a row; CRLF counts once. A final closed row without a
newline is valid. A short/extra data row fails, with no partial profile emitted. Syntax errors in
an extra row tail are observed before its field-count error. Standard field conversion occurs
as each field completes; an earlier invalid number therefore wins over a later CSV quote error.

All nonempty field cells apply in source order through the profile value model. Empty cells are
skipped. Duplicate fields therefore keep an earlier nonempty value when a later cell is empty;
a later nonempty cell replaces it. A `TYPE` column can override the initial format type. Custom
names and case remain exact. No header selection, symbol uniqueness or business-type validation
is imposed beyond the configured operational limits.

The UTF-8 decoder follows measured JDK replacement behavior. Invalid lead bytes and malformed
prefixes become U+FFFD; a truncated final sequence becomes a replacement rather than a syntax
error by itself. Surrogate encodings consume their complete three-byte sequence as one replacement.
A UTF-8 BOM is preserved as U+FEFF, so it does not silently turn a BOM-prefixed first header into a
recognized header. Valid supplementary scalars become their exact two UTF-16 units. Decode state
holds at most four bytes and one pending low surrogate across calls.

## Controls, EOF and partial outcomes

`##` emits FLUSH; `##COMPLETE` emits COMPLETE, after their entire CSV remainder validates.
Other metadata rows are comments. Controls may contain ignored extra fields; their CSV syntax
still matters. Completion is evidence, not an end-of-input instruction: later profiles, controls
or errors are processed. Blank rows are skipped.

`completionSeen` records an admitted marker even if later work fails. `finished` requires clean
explicit EOF under the selected policy; `complete` is exactly `finished && completionSeen`.
`ALLOW` is the default: clean EOF without a marker succeeds but is incomplete, corresponding to
the pinned Java reader's default warn-and-return behavior without ambient logging. `REQUIRE`
reports MISSING_COMPLETE at clean EOF, retaining accepted earlier profiles. No Java system property
is read. Neither an empty feed nor a work pause is EOF.

Each parser call returns one independent item, required input/work, FINISHED or the first terminal
FAILED status. `accepted` is the exact consumed prefix of that call's borrowed byte span; the
caller resubmits its suffix. Previously returned profiles remain valid after further feeds or
parser destruction. No incomplete profile escapes. Terminal calls accept zero more bytes.

Diagnostics use a fixed error category, logical byte cursor, one-based physical line/CSV row,
zero-based field, row/field start lines and an optional nested ProfileError. Syntax failures point
to the offending code unit's first byte; unterminated quotes point to EOF. Conversion failures
point to the field start; undefined/forbidden formats point to the row start. Cumulative budget
failures point to the examined parser cursor. These logical positions are not Java buffered
InputStreamReader prefetch offsets or a filesystem seek/rollback guarantee.

## Work, storage and ownership

`InstrumentProfileParser` owns its UTF-8/CSV state, formats and current field/profile. It performs
no I/O, callbacks or background work and has no output queue. It is noncopyable/nonmovable;
callers serialize all use. Options are copied at construction. Input spans are borrowed only for
the call; results contain no references to input or header storage.

`IpfReadWork` debits input bytes, decoded UTF-16 units, completed fields and admitted-row operations.
All four remaining allowances must be nonzero to start/continue an operation loop; conservative
WORK_EXHAUSTED yields are permitted even when some pending operation would not need the exhausted
dimension. Zero work accepts no input. The caller can replenish and resume with the unaccepted
suffix or another finish call. Failed work is not refunded. Synchronous drivers require positive
turn quotas before any I/O and replenish them internally; cumulative limits bound the whole read.

Engineering defaults, independently configurable per reader:

| Bound | Default | Accounting |
| --- | --- | --- |
| Input | 67108864 bytes | Cumulative parser-accepted UTF-8 bytes |
| Field | 65536 units | Unescaped UTF-16 field value |
| Active row | 1048576 units | Decoded syntax/text, excluding its terminator |
| Fields/row | 256 | Includes the first/type field |
| Rows | 1000000 | Includes blank/comment/header/control rows |
| Formats | 256 | Distinct retained type definitions |
| Header storage | 1048576 units | Type names plus declared field names |
| Profiles | 100000 | Whole emitted profiles |
| Profile output | 16777216 units | Cumulative standard values plus custom names/values |
| Turn work | 4096/4096/256/256 | Bytes/decoded units/fields/rows |

Each profile also keeps its independent `ProfileLimits`. Temporary header replacement is bounded
by the active-row/field limits while the previous retained definition remains intact; retained
header accounting is checked before its replacement commits. Current profile/cell storage and
returned owned output have separate limits. Counter arithmetic is checked before addition/growth;
limits describe logical units/counts, not exact allocator capacity, RSS or CPU deadlines.

## Local stream and file boundary

`readInstrumentProfiles(std::istream&, options)` consumes from the current position without
closing, seeking or rolling back the borrowed stream. It drains determined field/row outcomes
before trying more I/O. The streambuf may prefetch physical bytes, and a byte fetched to distinguish
CRLF may be ahead of logical parser consumption when an earlier error is returned. `acceptedBytes`
reports only logical consumption. Resource/work limits do not impose a blocking-I/O deadline.
Normal EOF remains normal when the caller enables stream exception masks. The caller's exception
mask is preserved. A `std::bad_alloc` escaping a stream operation maps to ALLOCATION_FAILURE;
if the stream suppresses that exception and sets `badbit`, the observable failure maps to IO_ERROR.
Other stream failures map to IO_ERROR. Parser/output allocation failures map to ALLOCATION_FAILURE.
Every typed failure retains the owned complete profile prefix.

`readInstrumentProfilesFile(path, options)` opens its own binary ifstream and closes only that
owned stream. Binary mode preserves quoted CRLF on Windows. Opening failure returns IO_ERROR.
No URL resolution, HTTP, credentials, decompression, writing, live collection, option-chain or
schedule interpretation is provided. See the [application guide](../../docs/INSTRUMENT-PROFILES.md).

Optional URL/file/HTTP(S) acquisition is documented separately in [URL-READER](URL-READER.md); the local functions here do not resolve URLs.
