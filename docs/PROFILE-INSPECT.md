# Bounded Profile inspection

[Русская версия](PROFILE-INSPECT.ru.md). English is authoritative.

The explicit `--profile default-profile-v1` preset supplies the complete standard Profile schema
from [PROFILE](../spec/events/PROFILE.md). It is a tool-local convenience, not a general schema loader.
Configured schemas use the library's [prepared mapping API](../spec/events/EVENT-MAPPING.md).
Source properties and external XML are resolved outside C++ core/protocol/event mapping.

## Selection and limits

Use the existing `--input`, `--output-format tsv|human` and `--tape-mode legacy|raw-message-v1` options.
Legacy selects uncompressed BINARY/NONE/EMPTY_SET/STREAM_DATA; the explicit RAW mode selects the
existing BINARY/RAW_DATA/MESSAGE contract. QTP event flags remain zero. Profile business Flags are
ordinary record data and may be nonzero. All earlier profiles and their output versions remain unchanged.

The Profile preset requires one ordinary local ID 0 record named Profile, the complete default logical
field order and exact types, and compatible prepared field selections. A wrong schema/display pair
fails before input or output work with exit 4 and LOCAL_PROFILE diagnostics. Diagnostics default to
version 2 for this preset; explicit version 1 preserves that diagnostic grammar.

The preset's one-time `MappingLimits` are fields 16, lookups 64, selections 16, keys 32,
typeAlternatives 32, nameUnits 128, totalNameUnits 4096 and comparisonWork 131072. These fixed
tool bounds do not replace caller-supplied preparation budgets in the configurable library API.

`--max-event-units` bounds cumulative UTF-16 units copied into Profile events, including symbols,
Description and StatusReason; the default is 4194304. Zero is a real allowance. Per-symbol and
per-field bounds also apply. Null and empty strings both charge zero units but remain distinguishable.
This budget is independent of decoding work and encoded stdout bytes. Projection does not refund
previous copies. One temporary owned event coexists with the bounded decoded result; no event history
is retained by the inspector. All existing file, frame, record, field, string and output bounds apply.

## TSV version 5

Output is LF without BOM. The header is five cells:
`format`, `qdcxx-tape-inspect`, `5`, `default-profile-v1`, `<mode>`.
Existing frame, descriptor, heartbeat, timing, markers and completion rows keep the grammar of the
selected tape mode. RAW timing is explicit metadata and is separate from Profile halt times.

Each record produces a `profile-record` row followed by a `profile` event row. The first has seven cells:
row name, frame index, emitted index, local ID, local record-name UTF-16 hex, symbol UTF-16 hex,
and comma-separated logical values. Each value is `type:raw:payload`; scalar payload is `-`, and type 10
uses `null`, `empty` or lowercase four-digit UTF-16 units. This retains every default field's exact raw
identity, independently of its event projection. Name/symbol empty sentinel is `-`.

The event row has exactly 21 cells, in this order:

| Cell | Value |
| --- | --- |
| 0–1 | `profile`, emitted index |
| 2–5 | beta, earningsPerShare, dividendFrequency, exDividendAmount binary64 bits |
| 6 | exDividendDayId, signed decimal |
| 7–12 | high52WeekPrice, low52WeekPrice, shares, freeFloat, highLimitPrice, lowLimitPrice binary64 bits |
| 13–15 | haltStartTimeMs, haltEndTimeMs, raw signed Flags |
| 16–17 | Description and StatusReason: `null`, `empty` or exact lowercase UTF-16 hex |
| 18–19 | tradingStatus and shortSaleRestriction numeric codes |
| 20 | delivered eventTime milliseconds; zero in legacy mode |

Binary64 cells are exactly 16 lowercase hexadecimal digits; NaN uses canonical quiet-NaN bits.
Raw decimal representations remain in `profile-record`. Integers use canonical decimal syntax.
Both rows are counted together against remaining encoded bytes before allocation and their first write.

## Human version 5 and errors

The header is `qdcxx-tape-inspect human 5 profile=default-profile-v1 tape-mode=<mode> identities=local`.
The raw record line begins `Profile <index>: frame=<frame> local=<id> name=<quoted> symbol=<quoted>`
and lists every logical field's type/raw value and nullable text. The following `ProfileValues <index>:`
line labels all event properties, decimal text, millisecond times, Flags, nullable strings and enum codes.
Names, symbols and strings use the existing lossless safe UTF-8 quoting, including escaped controls
and unpaired surrogates. No string is normalized or passed unescaped to a terminal.

Exit codes remain 0 success/help, 2 usage, 3 I/O, 4 unsupported, 5 malformed, 6 resource/allocation.
No arguments and standalone `--help` produce identical stdout, empty stderr and exit 0 when output
succeeds, without opening input. Invalid nonempty argument lists remain usage errors.

Detected failure retains the already written completed prefix and emits no later completion marker.
A failed final flush can occur after completion text reached a sink, so successful output requires
exit 0. Truncated input never becomes successful EOF. Exactly one final completion row is required
by positive comparators; unknown, duplicate and trailing rows are errors.

This capability does not imply indexed events, a properties/XML/JSON loader, additional wire identities,
compression, replay scheduling, transport or compatibility with a private tape. Verification
and independent review are recorded separately.
