# ADR-0010 — Preserve the full Summary OpenInterest width

[Русская версия](ADR-0010-summary-open-interest-width.ru.md). English is authoritative.

Status: **Accepted for QDCXX-146.** This is an explicit correction of a demonstrated pinned Java data-loss defect, under the policy to follow Java behavior without reproducing confirmed errors.

## Context

Pinned QD commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` documents Summary open interest as the number of open contracts and exposes a public Java long getter/setter. Its actual default record uses WIDE_DECIMAL120: SchemeProperties defaults wide=true, and MarketFactoryImpl selects that representation for OpenInterest. With explicit wide=false, the legacy representation is COMPACT_INT8.

The production mapping long accessor preserves the finite value 3000000000 in the default record. SummaryDelegate instead reads the int accessor, then widens the damaged result to long; its publication path explicitly casts the event long to int. Actual JDK8 production observations demonstrate 3000000000→-1294967296 and 4294967296→0 on both delegate read and event put/get roundtrip. A manually constructed Java Summary retains the original long. The selected default physical representation can preserve these values exactly, so this loss is in the delegate bridge, not a necessary wire restriction.

The generator declares Summary OpenInterest as INT_DECIMAL with DecimalMapper(int), explaining the generated accessor/cast. That implementation choice does not document a public int32 precondition. The inspected checkout is shallow; no historical migration date, intent or upstream acknowledgment is inferred. The presence of a long accessor alone is not the defect proof: the finite, representable roundtrip counterexample is.

## Decision

Keep the existing C++ `std::int64_t openInterest` field and getter, using the actual pinned SummaryMapping.getOpenInterestLong / RecordMapping.getAsLong semantics:

| Admitted source | Corrected C++ value |
|---|---|
| INT2/4/8 | signed physical int, extended without widening its storage domain |
| LONG104 | complete signed 64-bit source value |
| DECIMAL24 | actual Decimal.toDouble, Java truncation/saturation/NaN-to-zero into long |
| WIDE120 | exact WideDecimal.toLong: nearest integer, half ties toward positive infinity, signed 64-bit saturation and NaN-zero |
| Missing optional / regional field |0 |

There is no double intermediate for LONG or WIDE and no subsequent int narrowing. The physical INT8 limitation remains: a value already narrowed by32-bit storage cannot be recovered. Unsupported present representations40/152 remain explicit preparation errors; no formats, fields, public types, layouts, defaults or receiving capacities are added. The six business-double views and their inline snapshots are unchanged. Wide precision/scaling limits also remain: the corrected value is the actual long accessor of the stored value, not a promise to recover every requested long after decimal quantization.

## Evidence and compatibility

The original `oracle.tsv` and its provenance remain immutable raw Java delegate observations. A separate `open-interest-long-v1.tsv` records, for every successful original and additional boundary sample: original delegate OI, actual production long-accessor OI, corrected expected OI and `IDENTICAL` or `CORRECTED_INT_ACCESSOR_LOSS`. The native reader requires exact coverage, uniqueness, the original delegate value and the matching classification before applying that single expected cell. Other event fields retain their original expected values; mismatches are not ignored.

`open-interest-boundary-v1.tsv` adds actual production observations around2^31 and2^32, signed 64-bit edges, all six numeric representations, NaN/infinities and regional absence. Tiny and wide conversions retain their distinct Java long semantics. Capture/Verify binds both datasets to the original capture, pinned production classes and source hashes; no native formula generates expected values.

Ordinary values within the prior range remain compatible. Large representable OI intentionally differs from the pinned Java delegate while preserving the public 64-bit contract. Applications must not depend on negative/zero corruption produced by the old bridge. Existing aggregate initialization, ownership, routing, callback lifetime and finite preparation/accounting contracts are unchanged.

## Alternatives

Exact reproduction of the delegate int narrowing would preserve demonstrated data loss and was rejected by the stated policy. Clamping to int32 also loses valid information and does not match the actual long mapping. A new event type, compatibility switch or wider receiving profile is unnecessary because the public field already has the required width.

## Primary sources

[Summary public contract](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/Summary.java), [SummaryDelegate](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/SummaryDelegate.java), [SummaryMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/SummaryMapping.java), [RecordMapping](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/ng/RecordMapping.java), [generator](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-codegen/src/main/java/com/dxfeed/api/codegen/ImplCodeGen.java). See the [Summary contract](../../spec/events/SUMMARY.md) and [oracle helper](../../tools/upstream/summary-oracle/README.md).
