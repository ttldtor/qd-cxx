# Time-series event model

[Русская версия](TIME-SERIES-EVENT-MODEL.ru.md). English is authoritative.

`TimeSeriesEventModel<Event>` composes the
[indexed transaction model](INDEXED-EVENT-MODEL.md) with one logical symbol and
an inclusive signed Unix-millisecond lower bound. It supports the native
TimeSeriesEvent types Candle, Greeks, Underlying, TheoPrice and TimeAndSale.
Their source is DEFAULT, ID zero. The symbol is exact, including the regional
suffix or canonical Candle attributes when present.

A new model has no symbol and `TIME_SERIES_FROM_TIME_DISABLED` (signed64 MAX)
as its lower bound. Both a symbol and an active bound are needed for input.
Changing a symbol clears its old state. Changing an active bound retains
completed values still inside the range and discards all unfinished indexed
work. A higher bound evicts older values. A lower bound preserves the retained
overlap and needs new input to populate the wider range. Setting MAX disables
input and clears data while retaining the configured symbol. `clear` also
removes the symbol. Equal settings leave state and revision unchanged.

An actual selection change advances an owner-local range revision. Pass the
revision associated with a delivered batch to `processEvents`. A changed
revision refuses that batch while preserving current state. The caller maps
delivery to the proper revision, controls the corresponding Feed demand and
serializes model operations. A revision is a local selection fence and gives
no guarantee about source coverage or upstream admission.

The model uses each event's business `getTime()`, independent of FIELD
`eventTime`. Outside-range values can delimit an indexed transaction or
snapshot without becoming retained values. Pending changes stay invisible.
Input limits and allocation refusals preserve the whole previously accepted
state. The range and its underlying indexed state both use
`IndexedEventModelLimits`. A bounded temporary copy preserves the original
flags if an outside-range value needs to act only as a boundary.

`getSnapshot` returns the selected symbol, lower bound, revision and completed
values, sorted by ascending business time and then signed 64-bit index. Every
value has its indexed flags cleared. Detached cuts outlive later changes, move
and destruction. Moving transfers the current state and leaves the old model
inactive. The model holds no callback, runtime or subscription.

The pinned Java `com.dxfeed.event.TimeSeriesEvent` and
`com.dxfeed.model.TimeSeriesEventModel` specify DEFAULT source, ordered business
time, pending and snapshot isolation, and MAX-disabled input. The detached C++
range changes immediately when `fromTime` changes. It does not depend on the
equality of Java `TimeSeriesSubscriptionSymbol`, which compares only its
underlying symbol.
