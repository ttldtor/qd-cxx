# ADR-0012: Symmetric decorated subscription identity

[Русская версия](ADR-0012-decorated-subscription-identity.ru.md). English is authoritative.

Status: accepted implementation policy for QDCXX-136; independent implementation verification is separate.

## Context and observed defect

Pinned Java QD 3.355 revision `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` defines IndexedEventSubscriptionSymbol (IES) identity by event symbol/source and TimeSeriesSubscriptionSymbol (TSS) identity by event symbol alone. TSS extends IES. IES.equals accepts any IES subtype, while TSS.equals accepts only TSS.

Actual production Java8 observations show `IES("AAPL", DEFAULT).equals(TSS("AAPL", 0)) == true` and the reverse `false`. The same asymmetry occurs at fromTime -1,1,MIN/MAX. The tested DEFAULT-cross-class hashes are equal; no hash-contract defect is claimed. Actual HashSet insertion of IES then TSS yields size2; the reverse yields size1. This is a collection witness, not proof of the real DXFeed subscription container or network route behavior.

The Java Object.equals public contract requires symmetry. Thus the observed cross-class relation is a confirmed equality-contract defect, rather than merely an unusual sentinel or undocumented runtime guess. TSS's exclusion of fromTime and source ID-based equality despite different/null names are documented intended behavior and remain separate facts. Explicit WildcardSymbol.ALL versus String("*") is unequal in both directions with equal hash42: that is a valid typed distinction and permitted hash collision.

Primary pinned sources: [IES](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/IndexedEventSubscriptionSymbol.java), [TSS](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/TimeSeriesSubscriptionSymbol.java), [WildcardSymbol](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/WildcardSymbol.java). The installed JDK8 Object.java contract was checked; [Object.equals reference](https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#equals-java.lang.Object-) describes the same symmetry requirement. [Raw observations](../../tests/fixtures/selectors/oracle.tsv) and [provenance](../../tests/fixtures/selectors/provenance.json) preserve the upstream results unchanged.

## Decision

Use a flat owning SubscriptionSelector with explicit PLAIN, INDEXED, TIME_SERIES and ALL kinds. Equality includes kind and is symmetric. Within INDEXED, source identity uses signed ID, not its name; within TIME_SERIES, fromTime is excluded from identity but included in effective demand comparison. ALL remains distinct from literal text, including "*". Per-kind Java hashes remain compatible; cross-kind collisions are valid.

Do not reproduce the pinned asymmetry. Follow Java behavior where its contract is coherent, while correcting this confirmed defect. Do not infer a general policy to alter unfamiliar values or classify every discrepancy as an upstream bug.

## Alternatives and consequences

Copying Java inheritance/instanceof equality would preserve an invalid relation and order-dependent native container behavior; rejected. Collapsing IES(DEFAULT), TSS and plain text into one identity would erase kind/source/time distinctions and is unsupported by their separate public contracts; rejected.

This is an explicit cross-kind Java difference. There is no existing native decorated identity to break. Current string/Candle APIs retain plain identity and existing exact routing. Effective fromTime changes must update desired demand even when identity remains equal. Native typed Feed currently rejects unsupported TSS activation atomically rather than pretending to deliver a future indexed/time-series family. Source demand uses existing validated bindings; time conversion remains a value-only operation with explicit raw requests. [Decorated selector contract](../../spec/subscriptions/DECORATED-SELECTORS.md) defines the supported boundary, lifetime and finite resource behavior.
