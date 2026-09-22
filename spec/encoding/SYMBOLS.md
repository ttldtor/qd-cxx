# Bounded QTP symbol reader

[Русская версия](SYMBOLS.ru.md). English is authoritative.

QDCXX-015 implements one message-local PentaCodec reader in qdcxx_encoding, with no dependencies on
qdcxx_core, qdcxx_protocol, schema, binding, event delivery or transport. The provisional API returns
owned exact UTF-16 and raw encoding metadata; it does not normalize or convert to UTF-8.

## Evidence and profile

Oracle: QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
[Inputs](../../tests/fixtures/encoding/symbols/inputs.tsv) and
[Java observations](../../tests/fixtures/encoding/symbols/expected/symbols.tsv) separate canonical
writer bytes, additional reader forms, strict prefixes, post-error state and reset.
[Production integration](../../tools/upstream/symbol-oracle/SymbolOracle.java) composes/parses seven
symbols in two messages. This uses real callbacks, not a diagnostic walk. Historical P01 stays intact.
The first clean capture was executed before this contract and C++ implementation.

Pinned sources: [PentaCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java)
ReaderImpl/WriterImpl, encodePenta, pentaToString and static character table;
[SymbolCodec](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SymbolCodec.java)
Reader/Writer reset, cipher-symbol contract;
[BufferedInput](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/BufferedInput.java)
readUTFChar/readUTFBody/readUTF2/3/4; [QTP sources](../protocol/SOURCES.md) identify parser
parseData/parseSubscription and composer writeMessageHeader reset calls and PentaCodecTest.
Those upstream tests were inspected, not executed.

Only option mask zero (EMPTY_SET) is accepted by the C++ API. Any other mask is UNSUPPORTED_PROFILE.
Java reader ignores its option set; production parser resets with SUPPORTED_SET, while integration
composer explicitly uses EMPTY_SET. No option-dependent wire form is silently reinterpreted: the
pinned codec has none. Raw signed flags are retained even when a message/profile would mask them;
EventFlag/message semantics and negotiation are not implemented.

## Grammar

| First byte | Remaining encoding |
| --- | --- |
| 00..7f | One byte: 15-bit big-endian penta |
| 80..bf | Three bytes: 30-bit penta |
| c0..df, fa, fb | Reserved; fail after first byte |
| e0..ef | Two bytes: 20-bit penta |
| f0..f7 | Four bytes: 35-bit penta |
| f8 | MRU flags, then symbol |
| f9 | Compact int flags (signed low 32 bits), then symbol |
| fc | Compact long byte length 0..INT_MAX, then QD UTF body |
| fd | Compact long UTF-16-unit length 0..INT_MAX, then that many readUTFChar values |
| fe | Empty penta |
| ff | Previous symbol; undefined after reset is an error |

Penta groups are decoded from the highest nonzero 5-bit group down. 1..26 are A..Z, 27 '.',
28 '/', 29 '$'. 30/31 plus a following group form a 10-bit code 960..1023: ascending ASCII
32..126 excluding A..Z, '.', '/', '$', apostrophe and backtick. Zero groups and dangling 30/31
produce NUL, as measured/source-defined; padding zeros at the start disappear. Noncanonical widths
are accepted. A penta up to 30 bits has cipher penta+0x40000000; a 35-bit value starting with '/'
or '$' has high cipher bits 10/11 and low 30 penta bits; other 35-bit values have cipher zero and
an owned decoded string. Cipher zero never means an empty decoded symbol. FC/FD preserve cipher
zero even for encodable text; FE has cipher 0x40000000 and empty UTF-16.

FC shares UTFString body behavior, including overlong forms, isolated surrogates, supplementary
pairs and consumption past declared byte length before error, bounded by the supplied span. Its
length excludes null and is capped at INT_MAX, unlike UTFString. FD counts units: each readUTFChar
may use 1..4 bytes, accepts overlong/surrogate values but rejects a decoded point above FFFF after
its complete bytes. Canonical writer uses FD for unencodable strings, writing surrogate halves
separately; FC is reader-only. Null cipher/string input is rejected by Java writer.

## State and results

Construct SymbolReader with mandatory SymbolLimits {inputBytes, codeUnits, stateUnits} and an
explicit option mask. beginMessage(mask) resets symbol, MRU=1 and terminal status. Caller must reset
between complete messages, not records/chunks. Input to read is a borrowed remaining span inside
one complete payload. Exactly one symbol is examined; trailing bytes remain for the caller.

Each call starts flags=0 and flagBytes=0. F8 takes MRU and reports flagBytes=1. F9 updates MRU and
reports the compact integer byte count, excluding F9 (Java API behavior). A second flag prefix is
invalid after consuming that prefix. No prefix leaves MRU unchanged, with result flags zero.
Successful FF preserves the symbol/cipher but obtains fresh per-call flags.

Result owns optional SymbolValue {units, cipher, eventFlags, eventFlagsBytes}. Only OK has a value;
consumed counts examined bytes in this call. Results survive subsequent reads, reset and reader
destruction. No resolver, shared cache, global state or automatic message-boundary detection exists.
Use is single-threaded and non-reentrant.

C++ deliberately publishes state only on complete success and makes any error terminal until
beginMessage. Later read returns FAILED with zero consumption; reset clears failure. Unsupported configuration
continues returning UNSUPPORTED_PROFILE until a supported beginMessage. Java may
retain the previous symbol and already update MRU on failure, as after-repeat/after-mru rows show.
Those post-error rows are evidence, not C++ continuation expectations. This atomic terminal policy
is authorized by QDCXX-015 and is not claimed as Java primitive or production recovery equivalence.

## Resource and failure precedence

Budgets are finite size_t logical counts, no unlimited defaults/sentinel; zero is valid. inputBytes
bounds the supplied remaining span, codeUnits the returned symbol, stateUnits the retained symbol.
The effective unit bound is min(codeUnits,stateUnits), since success retains one symbol. Caller-held
results are outside state accounting. At most the previous state, a tentative symbol and an output
copy coexist, each bounded; container capacity/allocator overhead is not an exact heap-byte promise.

1. Terminal/profile status precedes input-size budget; excess span fails RESOURCE_LIMIT at zero.
2. Consume prefix and required fixed/compact bytes. Truncation consumes the available span and
   returns TRUNCATED_INPUT. Reserved prefixes, duplicate flags, undefined repeat and malformed
   characters return INVALID_ENCODING at measured positions; invalid lengths return INVALID_LENGTH.
3. Validate FC/FD length 0..INT_MAX before count-driven allocation. FC byte length exceeding inputBytes
   and FD unit count exceeding the effective unit bound fail RESOURCE_LIMIT after length. No reserve
   follows untrusted counts. FC delegates to bounded UTFString only after the stricter length check.
4. Penta/FD append checks effective unit bounds before growth. UTFString checks its unit budget after
   reading a character and before declared-body overrun validation. Repeat checks the effective bound
   before copying. These resource priorities are designed C++ policy, not Java wire limits.
5. bad_alloc/length_error become ALLOCATION_FAILURE, no partial result/state is published. Consumed
   is the last committed examination position; a throwing delegated UTFString call leaves it at
   the start of its length prefix. Allocation-failure injection is not claimed.

No scan crosses the span. An incomplete symbol in a complete message cannot borrow another frame's
bytes; terminal state prohibits retry without explicit reset. No writer, event decoder, incoming-ID
installation, conversions, importer, time metadata, PART, transport or performance guarantee.
See paired [conformance coverage](../../tests/conformance/encoding/symbols/COVERAGE.md) and
[validation](../../tests/conformance/encoding/symbols/VALIDATION.md) for executed checks and limits.
