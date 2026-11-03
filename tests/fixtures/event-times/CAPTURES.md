# Common time and regional Quote production observations

[Русская версия](CAPTURES.ru.md). English is authoritative.

`oracle.tsv` is the raw output of the [event-time oracle](../../../tools/upstream/event-time-oracle/README.md).
`provenance.json` binds its bytes to the harness, launcher, production source hashes and unchanged
production class inventory at QD3.355 commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
JDK8 production calls in UTC and Pacific/Honolulu produced identical2152-row outputs.
The [contract](../../../spec/events/EVENT-TIME.md) distinguishes these observations from native policy.

Nine fresh-JVM scenarios preserve actual generated Quote, Quote&Q, Quote&z and Profile field declarations:

| Scenario | Selected configuration/variant |
| --- | --- |
| `default` | Standard second-precision business time; absent Quote Sequence/TimeNanoPart |
| `sequence` | `dxscheme.enabled.Sequence=Quote` |
| `nano` | `dxscheme.enabled.TimeNanoPart=Quote` |
| `both` | `dxscheme.nanoTime=true` |
| `millis` | Both optional Quote fields plus `dxscheme.bat=millis` |
| `millis-default` | Millisecond business time with absent Sequence/TimeNanoPart |
| `minimal` | Clone only required fields from the generated records |
| `reordered` | Reverse generated logical field order, with both optional Quote fields |
| `missing` | Clone without required fields; actual factory/delegate failures |

Each of the eight accepted scenarios has five cursor samples per record:160 complete cursor/delegate
observations. Optional fields retain raw signed32 boundaries, negative and oversized nano parts and
all ten packed millisecond bits. Business times include signed32 second and signed64 millisecond
boundaries, negative subsecond values and unequal bid/ask times. Independent timestamp-sequence
words include zero, minus1, signed64 minimum/maximum and a mixed-bit pattern. Profile strings
include null, empty and nonempty values. Required-field failures produce four `reject` rows.

The default scenario adds21 `route` rows through actual delegates and14 `selection` rows through
actual market delegate sets. Inputs contain existing exchange suffixes and trailing attributes;
both a regional catalog and composite-only fallback are observed. The lowercase exchange witness
prevents treating the required Q case as a special code path.

The separate `representability.tsv` adds four actual Quote&Q delegate roundtrips. The two lossy
spellings `AAPL&N&Q` and `AAPL{x=y}&Q` are distinguished from representable attributed symbols.
Its separate hash is recorded in the same provenance; the original2152-row corpus remains unchanged.

Rows use exact TSV cells: `record` identifies the scenario/local record; `field` retains logical
position, full serial identity, local name and actual property name; `input` retains symbol UTF-16
units and raw timestamp sequence; `value` retains raw scalar/text values; `quote`/`profile` retain
production getter results. `route` retains event-to-QD and QD-to-event symbols; `selection` retains
the actually chosen record and QD symbol. Strings use hexadecimal UTF-16 units, `-` for empty and
`null` for absent. Physical VOID tails are not logical fields. No diagnostic copy computes expected times.

Native checks `event_time_java`, `event_time_routing` and `event_time_feed` compare owning/borrowed
projections, exact route selection, finite workspace boundaries and real loopback Feed delivery.
The standalone Feed test also serves as an installed and physically relocated package consumer,
using only public headers. A public-service demonstration is separate and is not claimed here.
