# Portable attachment decision validation

[Русская версия](ATTACHMENT-VALIDATION.ru.md). English is authoritative.

This record qualifies the [portable policy](../../spec/events/ATTACHMENTS.md) and current generic
opaque preservation. Typed Message/Configuration projections and codec registration/decoding remain
separate implementation work. Independent contract review and generated-documentation review are
pending; this record does not claim completion, release or acceptance.

## Executed observations

The pinned production [Java oracle](../../tools/upstream/attachment-oracle/README.md) passed Capture
and a separate Verify run with Java 8u422, a 128 MiB heap and 60-second subprocess limits.
Fourteen standard-event cases and two Text field identities produced 29 exact artifacts.
UTC/Pacific-Honolulu matched; all 1290 classes and 27 resources across 14 explicit classpath roots
and the selected source files were unchanged before/after. [Provenance](../fixtures/attachments/provenance.json)
binds all artifacts and tool/runtime/source hashes. No QD rebuild or network source was needed.
Noargs/EN/RU/Get-Help worked without runtime inputs or operational output; incomplete Verify exited 1.

Windows x64 MSVC 19.42 Release/MD and Linux x64 GCC 13.3 Debug ASan/UBSan each passed five focused
CTest entries: `attachment_serialized_contract`, `attachment_fixture_negatives`,
`object_field_owned_values`, `field_type_java_tape`, `field_type_java_values`.
Linux also enabled LeakSanitizer for these native-only tests; no third-party SDK or JVM sanitizer
coverage is claimed. One compiler job was used, with OS builds sequential.

The new test reads actual FileWriter tapes with exact local standard record declarations. It
checks body/null/empty identity, serialized Java array distinction, benign custom serialization
and malformed opaque preservation, signed Configuration version passthrough, input/schema/reader
destruction, independent copies, chunk sizes 1/7/65536, unknown records, zero/exact/limit-minus-one
body bounds, typed RESOURCE_LIMIT and malformed truncated data frames. A copied null-to-empty
oracle mutation is rejected by the actual consumer assertion. Existing object regressions cover
full identity grammars and broader allocation/work/ownership failures.

Separate framework-free public-header consumers compiled and ran with MSVC and GCC sanitizers,
using only public include paths. They test independent owning field copies, null/empty distinction
and invalid mixed representation. No native codec stub is presented as a production registry.
Pinned Java formatting and C++ Uncrustify 0.83/clang-format 20.1.8 checks passed.

## Limits

The Java custom class is a benign fixture class available to that oracle JVM. Its representation
does not establish arbitrary class compatibility. Getter failures are separate from successful wire
parsing. Text observations witness field identity only. Configuration TICKER version filtering,
registered-codec selection, codec exception translation and callback guarantees are normative
future API requirements, not implemented or runtime-qualified by this decision. Wire grammar,
outgoing support and dependencies remain unchanged.
