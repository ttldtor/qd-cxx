# Executed schema coverage — QDCXX-005

[Русская версия](COVERAGE.ru.md). This English document is authoritative.

The machine-readable ordered invocation list is [cases.json](cases.json); all XML in [inputs](inputs/) was independently authored for this task. Results are Java QD 3.355 observations, not C++ conformance tests. Each row below denotes one fresh JVM with separate model and runtime loads.

| Subcase | Measured observation | Deferred in this family |
| --- | --- | --- |
| S01-empty | Empty XML merges one file; empty model; zero runtime records. | Factory wrapping and shortcuts. |
| S01-no-urls | Both direct loaders reject with IllegalStateException, with distinct messages. | Same as above. |
| S04-order-disabled | Alpha/Beta emitted as IDs 0/1; HiddenRecord and Hidden field retained in model but omitted at runtime. Mixed XML field order partitions into ordered int/object slots. | Visibility-rule toggles, regional combinations. |
| S05-both | First/Second occupy int slots 0/1 ahead of Ordinary despite XML field order; hasTime=true. | Object/long/duplicate or invalid indexes. |
| S05-first | Only first index declared; slot 1 is synthetic VOID; undeclared Second remains ordinary. | Same as above. |
| S05-second | Only second index declared; slot 0 is synthetic VOID; undeclared First remains ordinary. | Same as above. |
| S05-disabled-first | Both indexes declared; disabled first becomes slot-0 VOID and is absent from lookups. | Same as above. |
| S05-disabled-second | Both indexes declared; disabled second becomes slot-1 VOID and is absent from lookups. | Same as above. |
| S05-disabled-both | Model succeeds; runtime SchemeException: all index fields disabled. | Same as above. |
| S07-explicit | Second alias Chosen.Name explicitly main; full/local/final property names and original, alias, dotted/full, collapsed and missing lookups captured. | Alias/name collisions, reserved symbol, overlay alias order/removal. |
| S07-fallback | First alias wins without explicit main; field without aliases uses semantic name. | Same as above. |
| S08-before | Ordered inputs base, copy, overlay: Base gains Added; Copy retains Original only. | Missing/forward bases, duplicate copied fields, doc/regionals/disabled-copy behavior. |
| S08-after | Ordered inputs base, overlay, copy: Base and Copy both contain Original and Added. | Same as above. |
| S08-main-alias | Base uses second explicit main Chosen.Name; Copy uses first First.Name, although copied alias list still marks Chosen.Name main. | Alias removal pointer and rebuild isolation (S20). |
| S15-slots | COMPACT_INT, UTF_CHAR_ARRAY, BYTE_ARRAY, LONG, WIDE_DECIMAL: six int slots including two VOID tails; two object slots, category-relative order. | Remaining/reserved types and representation combinations. |
| S15-byte | Model resolves BYTE (ID 1); runtime NullPointerException at DefaultRecord constructor line 64. | Same as above. |
| S15-short | Model resolves SHORT (ID 3); same runtime rejection site. | Same as above. |

S02/S03/S06/S09–S14/S16–S21 are not covered as families. A copied-main-alias observation overlaps one S20 concern but does not establish S20 coverage. Imports, generators, enums, bitfields, full provider/property parity, known-event mapping construction, parser edge cases, rebuild sequences, factory exception wrapping and digest sensitivity remain deferred.

Every emitted record has null default mapping and an empty mapping-class list under the recorded five upstream factory providers. This is a measured result for artificial record names, not evidence that XML records never acquire mappings. Model record-map iteration is captured verbatim and can differ from ordered `generation` rows; runtime order is determined by generation sources. No unordered-map iteration is promoted to a schema ordering contract.

The captures expose no unexplained disagreement with the QDCXX-003 source reading. In particular, copy/main-alias loss and unsupported-type constructor failures confirm previously identified risks. The lookup captures also show that final property name `Value` coexists with a retained collapsed-name lookup (`ChosenName`/`FirstName`); prefixed semantic/secondary alias names do not automatically become scheme-level full-name lookups.
