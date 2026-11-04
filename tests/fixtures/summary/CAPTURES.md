# Summary capture layout

[Русская версия](CAPTURES.ru.md).

The raw dataset is `oracle.tsv` in this directory; provenance.json binds its bytes to locked source,
actual production-class inventory and the [bounded helper](../../../tools/upstream/summary-oracle/README.md).
It observes actual generated SummaryMapping, SummaryDelegate and Summary getters.

Default production composite has10logical fields, regional9without OpenInterest. All six double
fields and composite OpenInterest use wide120 by default. Separate fresh Java processes capture
wide=false tiny fallback and explicit price/volume int. Each record's logical order is reversed
independently while physical two-slot storage and VOID tails remain valid. All records haveTime=false.
Original independent FIELD metadata is retained, including a profile without that metadata mode.

record declares scenario/recordID/name; field declares position/fulltype/local/property keys;
symbol declares sample physical UTF-16 hex units and original FIELD time sequence; value retains
complete scalar raw bits. summary retains20cells after scenario/record/sample:

|Cells|Actual production value|
|---|---|
|0|owning logical event symbol UTF-16 units|
|1..7|eventTime, dayId, prevDayId, business OpenInterest, full flags, day-close PriceType, previous-close PriceType|
|8..13|wide raw open/high/low/close/previous-close/previous-volume|
|14..19|corresponding six actual event business double raw bits|

interest rows additionally retain actual mapping getOpenInterest int, getOpenInterestLong,
getOpenInterestDouble bits and getOpenInterestWideDecimal raw. These accessors can disagree; the
event delegate uses the int path. Flags use the actual package-private event getter through bounded
harness reflection. No native formula generates expected results.

reject/conversion-reject retain actual production factory/delegate failures. Families cover six
numeric representations; tiny128low patterns and five mantissa boundaries; LONG tie/carry/extremes/
1ULP; eleven raw-int representations and two-slot high-word behavior; more than1000wide OpenInterest
rank/tie/overflow probes; all16PriceType combinations/high/negative raw flags; six required-field
removals; each optional alone/all missing; default/tiny/int schemes; metadata absence; attributes
and suffixes; unsupported double/OpenInterest representations. Missing regional OpenInterest remains0
even when the corresponding composite profile rejects an unsupported present OpenInterest.

The native owning/borrowed comparison validates complete shapes and all20cells, with only OI
replaced by the separately classified actual long reference under ADR-0010. Public
installed/moved consumers separately exercise actual demand→physical binding→complete values,
all31five-type selections, regular/extended Trade interop, wrong-record/symbol drops and causal
remove/throw/callbackclose/reconnect. The final ADD marker fences the latest desired replacement.
Only the callback-close case emits an extra five-kind business probe after observed physical REMOVE,
proving all31other registrations remain live. Final REMOVE requires the exact ten-tuple multiset
across complete frames, with missing/duplicate/extra rejection. Ordinary close is strictdrained;
explicit reconnect gap and subscription ABORT history are separate classifications. These are
deterministic local TCP checks, not a public live-service run.

The separate raw dataset `open-interest-long-v1.tsv` has eight cells: reference/scenario/record/sample/delegateOI/mappingLong/correctedOI/classification. `open-interest-boundary-v1.tsv` uses the original record/field/symbol/value/summary layout. Both are bound, together with immutable `oracle.tsv`, by `open-interest-long-v1.provenance.json`. Every successful sample requires exact rawOI and complete reference coverage; unmatched differences fail. See [ADR-0010](../../../docs/decisions/ADR-0010-summary-open-interest-width.md).
