# Default Quote Java evidence coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

Locked QD 3.355 (`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`), full built-in DXFeedScheme,
no property override, direct production file path. This finite set does not claim exhaustive
decimal or Unicode compatibility. Source and actual command hashes are in [captures](CAPTURES.md).

| Evidence | Observed coverage |
| --- | --- |
| [schema.tsv](expected/schema.tsv) | All 331 default records, exact local names/property aliases/full types, int and object slots. Composite Quote is ID 0, 12 int slots, no object slots, hasTime=false. Eight logical fields are 56/2/120/120/56/2/120/120; long tails remain local VOID slots. Regional records remain in the scheme. |
| [default-quote.qtp](expected/default-quote.qtp), [inputs.tsv](expected/inputs.tsv), [put.tsv](expected/put.tsv), [default-quote.tsv](expected/default-quote.tsv) | Five real Quote objects: IBM/MSFT/IBM/SPECIAL/IBM, original FileWriter bytes, raw delegate output and FileReader deliveries, all fields and double bits. BINARY/NONE/EMPTY_SET/STREAM_DATA; flags and event-time metadata zero. |
| [primitives.tsv](expected/primitives.tsv) | 14 UTF_CHAR writes, 27 reads with consumed positions; five signed TIME_SECONDS bounds; 19 raw compact-long/decimal cases and 13 double composition cases. |
| char-1 through char-11 | Production field/parser acceptance of ordinary bytes, overlong 2/3/4-byte forms, isolated surrogates, U+FFFF, U+10000 and U+10FFFF. Raw int survives; Quote exchange uses its low 16 bits. |
| time--2147483648 through time-2147483647 | Five production parser/mapping cases: seconds -2147483648, -1, 0, 1, 2147483647 become exact int64 milliseconds. |
| decimal-0 through decimal-18 | Production parser raw preservation and actual Quote mapping for NaN, canonical/noncanonical zero, signed canonical/noncanonical infinities, +/-1, extreme ranks, signed int64 bounds and significands around the 53-bit boundary. |
| [missing.tsv](expected/missing.tsv) | Missing incoming fields default to raw zero. Price/size raw zero means NaN; times/exchanges mean zero. |
| [duplicate.tsv](expected/duplicate.tsv) | Two exact BidPrice targets: later raw 640 wins, mapped value 2. |
| [reordered.tsv](expected/reordered.tsv) | Reversed incoming fields bind to the normal eight local targets. |
| [unknown-fields.tsv](expected/unknown-fields.tsv), [unknown-record.tsv](expected/unknown-record.tsv) | Unknown supported fields 56/2/120 are consumed before a known value; unknown record is skipped with zero deliveries and zero corruption errors. |

The original production tape is the real event end-to-end evidence. All other QTP probes are
diagnostically constructed envelopes delivered to production FileReader; they are not FileWriter
observations. The focused character reader is `DataIntField.readInt(BufferedInput)`, implemented by
`CompactCharField` and `BufferedInput.readUTFChar`. Writes exercise the corresponding production field.
Malformed/truncated character probes are focused primitive observations, not full-file callbacks.

Observed losses are explicit: default optional Sequence/TimeNanoPart are absent. Input bid/ask
milliseconds .123/.456, sequence 17 and nano part 987654 return whole seconds and zero sequence/nano.
Decimal -0.0 returns canonical +0.0; Double.MIN_VALUE composes to decimal zero; Double.MAX_VALUE
composes to positive infinity. Input 1.2345678901234567 rounds to decimal 1.234567890123457 and changes
the output IEEE double by one bit. These are Java observations, not idealized input equality.

Raw wide values retain all 64 bits. Java decimal semantics use signed significand `raw >> 8`,
rank `raw & 255`, and finite exponent `128 - rank`; rank zero classifies NaN/infinity by significand
sign. Java `WideDecimal.toDouble` performs a correctly rounded conversion, including a decimal-string
fallback outside its exact-arithmetic range. The fixture retains Java spelling and exact canonical
NaN double bits (`Double.doubleToLongBits`); it does not preserve a NaN payload from input double.

UTF_CHAR utility accepts U+0000..U+10FFFF including isolated surrogate values and noncanonical
encodings. It rejects values above U+10FFFF, invalid leading/continuation bytes and incomplete tails.
The captured position includes all bytes consumed by the primitive; 3/4-byte paths read their whole
tail before checking continuation bits when enough bytes exist. UTFString is a separate contract.

No conversion matrix, decimal composition implementation in C++, publisher endpoint, regional event
mapping, RAW_DATA/MESSAGE, compressed input, recovery, transport or private-tape support is established.
Old tape, symbol and encoding fixtures remain independent regression evidence. See the separately
specified [value contract](../../../../spec/encoding/QUOTE-VALUES.md) and
[default Quote profile](../../../../spec/protocol/DEFAULT-QUOTE.md).
