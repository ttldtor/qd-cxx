# Candle payload mapping

[Русская версия](CANDLE-PAYLOAD.ru.md). English is authoritative.

`qd/events/candle.hpp` provides owning `Candle` values and `PreparedCandle`.
Link `qdcxx_candle` in the source tree or from the installed package.
The mapper takes an explicit local record and a resolved `CandleSymbol`.
Use the [physical route catalog](CANDLE-ROUTES.md) to choose the record and restore its symbol.

| Physical mapping | Required fields | Optional fields |
| --- | --- | --- |
| `CANDLE` | Time, Sequence, Open, High, Low, Close | Count, Volume, VWAP, Bid.Volume, Ask.Volume, ImpVolatility, OpenInterest |
| `TRADE_HISTORY` | Time, Sequence, Price, Size | Other physical fields are validated as part of the source record |

The TradeHistory mapping sets Count to one, copies Price to all four OHLC fields
and reads Volume from Size. A missing Count on a Candle record yields zero.
Unavailable numeric values are NaN. Count uses the integer conversion of the
source representation. For wide decimals it rounds exact decimal halves toward
positive infinity and saturates at signed 64-bit bounds. The other numeric
getters retain the Java business-double result. The integral volume getters
apply Java narrowing to that double.

The index contains the signed seconds word and all low millisecond and sequence
bits. The mapper retains indexed event flags and independent FIELD event time.
Returned values own their data. The shape of every field, the record origin and
the finite symbol-copy limit are checked before projection.

The [Java oracle](../../tools/upstream/candle-payload-oracle/README.md) records
production delegates for Candle, TradeHistory and a fixed-period record, with
wide and tiny decimals, optional fields, LONG values and removal boundaries.
