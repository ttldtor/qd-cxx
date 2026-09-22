# Default composite Quote through bounded tape

[Русская версия](DEFAULT-QUOTE.ru.md). English is authoritative.

QDCXX-022 contract, written after the first Java runtime capture and before C++ implementation.
Starting HEAD: fa5a0b967e9c021e656846cdd3d6faee5faa4776. The full default Java DXFeedScheme has
331 records; selected composite Quote has ID 0, hasTime=false, 12 int slots and no object slots.
The production event/delegate/file round trip and separate focused parser observations are in
[Quote evidence](../../tests/fixtures/protocol/quote/COVERAGE.md). Regional records remain in Java's
scheme but are not part of this C++ adapter. The original production tape is consumed unchanged.

## Local adapter and mapping

The small reusable qdcxx_quote component depends on core only, separate from protocol, encoding IO
and CLI. defaultQuoteSchema(limits) returns an owned validated schema with LocalProfile::QUOTE_FIELDS_V1,
one record named Quote, local ID 0 and these eight logical fields, in this exact order:

| Local name | Outgoing property name | Full type | Java int head |
| --- | --- | --- | --- |
| Bid.Time | BidTime | 56 | 0 |
| Bid.Exchange | BidExchangeCode | 2 | 1 |
| Bid.Price | BidPrice | 120 | 2 |
| Bid.Size | BidSize | 120 | 4 |
| Ask.Time | AskTime | 56 | 6 |
| Ask.Exchange | AskExchangeCode | 2 | 7 |
| Ask.Price | AskPrice | 120 | 8 |
| Ask.Size | AskSize | 120 | 10 |

Each field has explicit full Quote-prefixed local name, local name and outgoing property lookup.
No guessed aliases, Java VOID tails, external importer or event hierarchy. Wire sender identity is
independent of local identity. Missing fields, repeats, skips and values follow
[QUOTE-VALUES](../encoding/QUOTE-VALUES.md). All eight enabled fields remain present in the local schema.

mapQuote(symbol, fields, symbolUnits) validates eight exact tags/ranges, charges the explicit symbol
copy bound, and returns an owning provisional Quote or structured shape/resource/allocation error.
It keeps four WideDecimal raw values with useful toDouble/toString views, two exact millisecond
timestamps and two low-16-bit exchange codes. timeMs is max(bidTimeMs, askTimeMs); eventTime,
sequence and timeNanoPart are zero in this profile. It has no dependency on decoder or file lifetime.
The default Java scheme loses subsecond/sequence/nano inputs before writing; measured decimal
composition can round input double and converts negative zero to positive zero. C++ compares the
observed Java replay values, not idealized equality with original Quote inputs.

## Tape and output profiles

Tape transport remains exactly [TAPE-READER](TAPE-READER.md): BINARY, uncompressed, NONE time,
explicit EMPTY_SET, STREAM_DATA and zero raw flags. Type acceptance is selected by the validated
local profile, separately from transport. Existing oracle-sre-v1 and the old QTP inspector retain
their accepted inputs, defaults, errors and byte output.

qdcxx-tape-inspect adds explicit --profile default-quote-v1. Its default TSV header is
`format<TAB>qdcxx-tape-inspect<TAB>2<TAB>default-quote-v1`. Existing frame, descriptor, markers and
complete rows keep their grammar. New `typed-record` rows contain frame index, output record index,
local ID, local name UTF-16 hex, symbol UTF-16 hex and a comma-separated sequence of `type:raw`
pairs in local field order. Raw is canonical signed decimal; type is the complete numeric identity.
A following `quote` row contains output index, bidTimeMs, bidExchangeCode as unsigned decimal,
bidPrice double bits, bidSize double bits, askTimeMs, askExchangeCode, askPrice bits, askSize bits,
timeMs, sequence, timeNanoPart, eventTime. Bits are 16 lowercase hexadecimal digits; NaN uses
canonical quiet NaN bits. Field raw values retain any noncanonical NaN/zero/infinity representations.
These rows constitute a separately versioned lossless value representation, not a new tape format.

Human header identifies version 2/default-quote-v1. Record output quotes and escapes symbols/names,
labels the eight fields, shows seconds as milliseconds with units, exchange as escaped UTF-16,
and prices/sizes as decimal text plus exact type/raw identity. Existing safe terminal escaping
applies. Every composed row is counted before dynamic output allocation; temporary decimal text
has the fixed 32-byte bound. Aggregate output/record/value/symbol budgets remain mandatory.

## Structured rejection detail

TapeRejection and TapeDiagnostic belong to qdcxx_protocol and carry refusal data only.
The static name/explanation functions tapeRejectionName and tapeRejectionExplanation belong to
tools/tape-inspect/diagnostics.hpp in qdcxx_tape_inspect_tool. CLI profile names, selection advice
and serialized/human presentation stay in the tool; the protocol library has no such dependency.
This presentation boundary does not change reason identities, format versions, bytes or positions.

TapeDiagnostic adds a bounded static TapeRejection reason: NONE, TIME, OPTIONS, MESSAGE, TYPE,
LOCAL_PROFILE, DESCRIPTOR or SEQUENCE. It contains no untrusted text. Numeric error/detail/stage
and all exact positions retain their old meanings. New version 2 diagnostic rows use
`incomplete-v2` plus the eight legacy data cells (without the old row name) and a final reason name.
Human version 2 additionally gives a fixed readable explanation; unknown input strings are never
interpolated unescaped. Legacy profile diagnostics stay byte-identical, including human mode.
Context-free usage rejects an unknown local profile before opening the file; its explicit
--diagnostics-version 2 option selects this explanation even when profile selection failed.
The same opt-in is available with oracle-sre-v1; without it old diagnostics remain exact.
No position is replaced by prose. New profile defaults to version 2 diagnostics; explicit version 1
retains the legacy diagnostic grammar. Invalid diagnostic versions are usage errors.

The stream inspection API also rejects incompatible schema/display profiles before input reads or
stdout writes: exit 4, stage profile, LOCAL_PROFILE reason in diagnostics version 2. The new display
requires QUOTE_FIELDS_V1 with one Quote record and its eight exact ordered types; legacy displays
require COMPACT_INT_V1. The caller remains responsible for the schema's explicit lookup mapping.

## Boundaries and validation

All new identities keep primitive start error offsets, actual consumed bytes, terminal failure,
completed record prefixes, fresh message-local symbols and atomic description snapshots.
The context copies one typed default shape per local record after schema budgets pass; construction
charges those logical fields through existing schema work charges on every rebuild. Unknown records
allocate no output fields. Typed and legacy result vectors are mutually exclusive, counted once.

Acceptance compares ordered real symbols, all eight raw fields, exact decimal text and binary64
bits, mapped timestamps/exchanges/losses, and complements those with synthetic edge/budget/lifetime
tests. [Validation](../../tests/conformance/protocol/quote/VALIDATION.md) will record only executed
checks, with independent review separately pending. No RAW_DATA, MESSAGE timing, general real tape,
regional Quote, other event types, compression, importer, dynamic replacement or transport is claimed.
