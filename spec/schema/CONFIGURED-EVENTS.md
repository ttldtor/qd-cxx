# Configured Quote and Profile schemas

[Русская версия](CONFIGURED-EVENTS.ru.md). English is authoritative.

This QDCXX-025 contract follows new locked Java observations and precedes production C++.
The [oracle](../../tools/upstream/event-mapping-oracle/EventMappingOracle.java) uses production
`MarketFactoryImpl`, `QuoteDelegate`, `ProfileDelegate`, `FileWriter` and `FileReader`.
[Coverage](../../tests/fixtures/events/mapping/COVERAGE.md) distinguishes file replay, direct
mapping probes and source configuration observations. The existing JSON profile is unchanged.

## Resolved boundary

Source acquisition is outside core, protocol and event adapters. Java resolves properties and XML;
C++ receives an explicitly validated immutable ordinary local schema. Compatible Quote/Profile
records from pinned `dxlink.xml` and the separate type-10 extension retain their actual fields,
local/property lookup names, logical order and full identities. Java physical VOID tails are not
logical fields. The complete default Profile and default Quote witnesses are unchanged.
Reorder, minimal and numeric-int are separately named programmatic witnesses; they do not replace defaults.
The entire dxlink scheme contains other records/types and is not an accepted C++ schema profile.

Preparation resolves explicit lookup keys without dot stripping, case folding or inferred aliases.
Java `DefaultRecord` installs full/local/default property names before mapping construction and
mapping-specific aliases afterward. Its `addFieldAlias` accepts another key for the same field
and rejects a key targeting another field. The captured `ChosenPrice` alias resolves Bid.Price;
Bid.Time -> BidPrice is rejected. C++ retains its bounded ambiguity rejection when supplied keys
resolve different logical fields; a record name alone does not authenticate compatibility.
Schema identity/ownership and preparation limits belong to [EVENT-MAPPING](../events/EVENT-MAPPING.md).

## Configuration ledger subset

These are Java observations, not a C++ properties loader or universal settings precedence.
Every scenario runs in a fresh JVM; only mutation deliberately changes properties after initialization.

| Keys / consumer | Grammar and default | Sources, precedence and timing | Capture |
| --- | --- | --- | --- |
| `dxscheme.nanoTime`, `SchemeProperties` | Boolean; absent false | Explicit `dxscheme.enabled.Sequence` and `.TimeNanoPart` selections override the shortcut independently | default, sequence, nano, both, explicit |
| `dxscheme.enabled.<Property>` | Event-name glob; Quote enables only Quote; Profile excludes Quote | Copied when SchemeProperties is constructed; enabled selections precede nano shortcut fallback | sequence, nano, explicit |
| `dxscheme.price`, `.size`, `.shares`, `MarketFactoryImpl` | Case-insensitive int/wide/tiny/decimal; absent uses wide=true defaults | Snapshot at schema construction; the fixture requests int explicitly | integer-representation |
| `dxscheme.wide`, `SchemeProperties` | Boolean, default true | false selects some full type24 fields; the original tiny-unsupported capture is retained, with current adapter support defined separately | tiny-unsupported |
| `dxscheme.nanoTime` invalid value | Invalid boolean logs an error and falls back to false | Java behavior is observed, not a recommendation to silently accept invalid C++ input | invalid-boolean |
| `dxfeed.properties`, FEED `DXEndpointImpl.Builder` | Explicit local properties-file path | For tested keys: builder withProperty > file > JVM property; resolution at build | endpoint-file, endpoint-api |
| `System.setProperty` / `DXFeedScheme` | Explicit before/after mutation | Before class initialization affects singleton; after does not change its existing snapshot; a new explicitly configured endpoint differs | set-before, mutation |
| `DXScheme.Loader` XML options | dxprops/sysprops both false by default | resource:dxlink.xml ignores startup nanoTime; explicitly enabled options merge system then loader properties | xml, xml-system, xml-properties |

The property file sets nanoTime=false and enabled.Sequence=Quote. Startup conflicting values are
nanoTime=true and enabled.Sequence=Profile. The file case yields Sequence only. The API case
sets nanoTime=true and enabled.Sequence=Profile explicitly: TimeNanoPart only. The mutation
case preserves the initial singleton but an explicitly configured new endpoint has both fields.
XML-properties enables options and explicitly chooses nanoTime=false/Sequence=Quote over startup
true/Sequence=Profile. No environment variables, credentials or private files are involved.
Aliases for other settings, repeated endpoint builds and live changes remain outside measured coverage.

## Exact representations and absence

The original configured-event witnesses establish full120 and full8 numeric projections. Current
Quote/Profile numeric, raw-slot, String and missing conversions are defined by the exhaustive
[field conversion contract](../events/FIELD-CONVERSIONS.md), including direct Java business getters
alongside retained WideDecimal fields. This remains local event projection, not wire coercion:
wire/local full identities must still match. Other exact field types retain their semantic roles.

Optional absent local numeric properties produce NaN; an existing wire-omitted full120 value has
raw zero and also projects NaN, while wire-omitted full8 projects numeric zero. These equal/different
outputs do not merge the distinct schema states. Missing string slots produce null; explicit null
and empty strings stay distinct. Missing required local fields yield no compatible production event
delegate. An existing local required Description may receive wire omission and deliver null.

The separate XML extension uses Java `string` full137, and wide=false includes full24. Their original
captures and historical unsupported labels remain unchanged; the current adapters explicitly support
their declared local String/numeric representations. This does not add XML loading or silently widen
wire/local binding. No new serial identities, indexed records, general conversion, transport or session
semantics are introduced. Generated file modes remain uncompressed STREAM/NONE or RAW/MESSAGE,
EMPTY_SET options and zero QTP event flags. Profile business Flags are independent record data.
