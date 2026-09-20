# XML/DataScheme fixtures and remaining proposals

Status: **17 captured subcases** from S01/S04/S05/S07/S08/S15 for QDCXX-005; other entries below remain QDCXX-003 proposals. Oracle: devexperts/QD 3.355 at `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. See [exact coverage](COVERAGE.md), [executed validation](VALIDATION.md), [ordered inputs](cases.json), [normalized expectations](expected/) and [raw captures/metadata](captures/). These are Java observations, not passing C++ conformance claims.

Rules are in the [semantic inventory](../../../spec/schema/README.md); commit-qualified source/test citations are in its [source inventory](../../../spec/schema/SOURCES.md). Test resources below are relative to upstream `dxfeed-scheme/src/test/resources/`; they are references for a later licensed extraction or independently authored minimal input, not vendored files.

## Capture protocol and current implementation

[Harness instructions](../../../tools/upstream/schema-oracle/README.md) describe the implemented bounded subset of this protocol, provenance and the two permitted environmental normalizations. The broader protocol below still guides deferred cases; enums, tags, generators and other unexercised metadata are not serialized by the current harness.

1. Verify checkout HEAD, origin and tag against upstream.lock. Record JDK/JAXP provider, OS, locale, classpath including service-provider resources, working directory, ordered URLs, loading options and relevant system/explicit properties. Use a fresh JVM for independent cases.
2. Capture both `SchemeModel.newLoader().fromURLs(...).load()` and `DXScheme.newLoader().fromURLs(...).load()` where relevant; explicitly keep sysprops/dxprops disabled for pure XML cases. Invoke the factory separately for exception-wrapping cases. Do not substitute the diagnostic XML writer for a runtime observation.
3. For the model, capture ordered generation sources, field order, resolved type names, index pair, main/all aliases, tags, visibility decisions, enum values, generator values/templates and provenance. Sort only semantically unordered sets in a normalized view, retaining the raw observation.
4. For runtime, capture record array order/IDs/names/class/hasTime; separate int/object slot arrays with full/local/property names, serial type IDs and classes; explicit lookup probes; mapping class list/default mapping; digest. Include VOID slots. Avoid treating digest equality as sufficient evidence.
5. For rejection, capture stage, exception type, causes and message; separately normalize environment-dependent URL paths. Do not invent a single expected exception family before capture.
6. Review results against source. Label raw captured, normalized and hand-authored artifacts distinctly and record generation commands/input hashes. Review provenance/licensing before importing upstream material. QDCXX-005 provides a Java-only harness and original XML inputs, with no copied upstream material.

## Case matrix

The output column retains the original family-level research scope. Only the explicit subcases in [COVERAGE.md](COVERAGE.md) now have captured/reviewed outputs; unlisted portions remain pending.

| ID | Representative input / upstream seed | Outputs and distinctions to capture |
| --- | --- | --- |
| S01 | `01-empty.xml`; `DXSchemeLoaderTests.testEmptySchemeIsEmpty` | Empty model versus no input URLs; record count, merged-file count, errors. |
| S02 | `types/02-types-ok.xml`, `09-types-ok-override-a.xml` and `-b.xml`; `DXSchemeLoaderTypesTests` | Alias chains, forward references, resolved type; duplicate NEW, unbased UPDATE, embedded collision, self/cyclic/unknown base failures using corresponding negative resources. |
| S03 | `enums/01-enums-ok-basic.xml`, `12-enums-ok-override-a.xml` and `-b.xml`; `DXSchemeLoaderEnumsTests` | Names, ordinals and insertion order; gaps/reordering, duplicate names/ordinals, immutable ordinal, documentation-only updates. |
| S04 | `records/01-records-ok-basic.xml`, `02-records-ok-disabled.xml`, `07-records-ok-fdisabled.xml` | Model entities versus emitted entities; IDs and per-category field order; toggling visibility. |
| S05 | `records/04-records-ok-index12.xml`, `05-records-ok-index1_.xml`, `06-records-ok-index_2.xml` | Two index slots, synthetic VOID, hasTime; author variants disabling one/both indexes and using invalid index types. |
| S06 | `records/03-records-ok-regionals.xml`, `08-records-ok-fcompositeonly.xml` | Composite/regional ordering, exchange alphabet, regional-only selection, field exclusion. |
| S07 | `records/09-records-ok-falias.xml` through `12-records-ok-fmultialiasesmain.xml` | Main/full/local/property names, lookups by original name and aliases; collision with semantic/property names and reserved symbol name. |
| S08 | `records/15-records-ok-copy-same.xml`, `16-records-ok-copy-prev-a.xml`/`-b.xml`, `17-records-ok-copy-prev-up-a.xml`/`-b.xml`, `18-records-ok-copy-mid-up-a.xml`/`-b.xml`/`-c.xml` | Copy timing before/after overlays; missing/forward base; copied field duplicate; regionals/doc/disabled/main-alias differences. |
| S09 | `records/QD-1328-a.xml` and `-b.xml`; `testRecordUpdateWithoutType` | Omitted type preserves type; contrast omitted disabled/compositeOnly and explicit false; absent versus empty index attributes. |
| S10 | Independently authored import diamond, repeated root, and import cycle; SchemeModelLoader.loadFileAndImports | Effective load/generation order, deduplication, duplicate definitions, provenance. Test URL aliases separately from exact repeated URL. |
| S11 | Minimal generator based on dxfeed.xml Order/Candle examples | Prefix/suffix, delimiter, empty/duplicate values, empty iterator, template order, append/replace, omitted iterator versus defaulted NEW, generated-name collision. |
| S12 | visibility.xml patterns and VisibilityRuleTest | Last-match precedence, actual/generated name, field semantic name versus alias, tags-only rules, all/none tag logic, event-name OR behavior. |
| S13 | dxfeed.xml via `resource:dxfeed.xml` and `dxfeed` shortcut; DxSchemeExtSchemeTest | Option defaults, property-over-XML and explicit-over-system precedence, nano fields, suffix replacement/extension, decimal selections. Isolate malformed properties and Nuam cases. |
| S14 | dxfeed.xml and programmatic DXFeedScheme; DXSchemeDefaultSchemeTest.testDefaultSchemeEquality | IDs, fields, time, lookups and mapping classes under recorded providers/properties; no unconditional parity assumption. |
| S15 | Minimal ordinary int/object/wide_decimal/long fields; SerialFieldType factories | Separate storage counts/order, VOID tail, recognized but unsupported BYTE/SHORT and reserved types. |
| S16 | Minimal valid XML mapping declaration; equivalent known Quote record with/without optional aliases | Model mappings, runtime mappings, constructor-time required lookups, absent optional getter defaults; isolate Java provider effect from ignored XML declarations. |
| S17 | Minimal Configuration/TextConfiguration with/without Version | Runtime class and update result for older/equal/newer versions; this is module convention, not generic XML mapping. |
| S18 | Hand-authored lexical variants: prefixes, true/false/1/0, whitespace and repeated tag-list containers | DOM attributes/text versus parsed model, accepted-but-skipped content, validation stage; record JAXP provider. |
| S19 | Hand-authored flags, alias-to-flags, duplicate/overlapping/out-of-range bitfields | Metadata retained/skipped, offset+size boundary, exact exception stage/type. |
| S20 | Main-alias copy/removal and multiple overlay aliases; repeated withProperties on same scheme | Source-pointer quirks, effective name/property order, mutation across rebuilds; use fresh processes for controls. |
| S21 | Equivalent runtime fields with changed docs/tags/extra aliases and reordered records | Full runtime observation versus digest; determine which changes digest actually detects. |

## Proposed priority

Capture S01-S09, S11-S12 and S15 first to constrain normalization. Add S18-S20 before freezing edge-case compatibility. S13-S14/S16-S17 establish a separate dxFeed convention profile. S10/S21 test reproducibility and the limits of a digest-only comparison. These priorities are research proposals, not upstream facts.
