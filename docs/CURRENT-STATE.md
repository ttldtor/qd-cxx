# Current capabilities and limits

[Русская версия](CURRENT-STATE.ru.md). English is authoritative.

The project implements selected bounded profiles against Java QD 3.355 at the revision in
[upstream.lock](../upstream.lock). Full QD, dxFeed, QTP or tape compatibility is not claimed.

## Implemented components

| Component | Behavior and contract |
| --- | --- |
| Encoding | Compact int/long acceptance, exact nullable UTF-16 strings, owned symbols, finite budgets and terminal errors: [primitives](../spec/encoding/PRIMITIVES.md), [UTFString](../spec/encoding/UTFSTRING.md), [symbols](../spec/encoding/SYMBOLS.md). |
| QTP | Incremental framing, raw descriptions, immutable ordinary local schemas, static binding and complete STREAM_DATA decoding: [framing](../spec/protocol/FRAMING.md), [descriptions](../spec/protocol/DESCRIBE-RECORDS.md), [binding](../spec/protocol/STATIC-BINDING.md), [stream data](../spec/protocol/STREAM-DATA.md). |
| Offline tape | Explicit uncompressed legacy STREAM and RAW_DATA/MESSAGE profiles, append-only descriptions, owned timing, finite work/ownership budgets and completed-prefix terminal errors: [tape reader](../spec/protocol/TAPE-READER.md), [RAW tape](../spec/protocol/RAW-TAPE.md). |
| Field values | Explicit exact full IDs 2/8/10/40/56/72/88/120; nullable UTF-16 is owned, raw scalar representations are retained: [field types](../spec/encoding/FIELD-TYPES.md). Existing narrower profiles remain available. |
| Events | Default Quote plus schema-bound prepared Quote/Profile mappings with explicit local IDs and preserved generic extra/custom fields: [default Quote](../spec/protocol/DEFAULT-QUOTE.md), [configured events](../spec/schema/CONFIGURED-EVENTS.md). |
| Native JSON | Optional separately pinned loader, owned validated schemas, typed errors before tape I/O and generic inspection: [native JSON](../spec/schema/NATIVE-JSON.md). Core, codecs, protocol and event mappings remain JSON-independent. |
| Java schema export | Offline resolved settings/XML sources, complete actual lookup keys, explicit logical storage-slot projection and failure-safe native JSON publication: [export contract](../spec/schema/JAVA-EXPORT.md), [usage](../tools/schema-export/README.md). Existing C++ consumers use saved outputs without Java. |
| Utilities | Optional `qdcxx-qtp-inspect` and `qdcxx-tape-inspect`, finite input/output budgets, TSV/human diagnostics and explicit profiles: [build/API](CPP-BUILD.md), [QTP inspection](QTP-INSPECT.md), [tape inspection](TAPE-INSPECT.md), [Profile inspection](PROFILE-INSPECT.md). |
| Developer tools | Native UTC logging and portable CMake formatting; ordinary configure/build/CTest does not require Python or PowerShell: [instructions](../tools/dev/README.md). Test-only dependencies and the JSON loader retain separate opt-in boundaries. |

## Verification evidence

[Schema-export validation](../tests/conformance/schema-export/VALIDATION.md) records the most recent
integrated implementation: Windows MSVC Release 45/45 CTest and Linux GCC Debug ASan+UBSan 45/45 in
the implementation checks. Separate verification reran Windows 45/45, two Linux schema-export
tests, fresh JDK 8 command tests and targeted diagnostic regressions. The full Linux and
production-only matrices were not repeated in that separate verification. These are historical
results, not fresh executions caused by this capability summary.

The report separates Java production observations from diagnostic projection and C++ conformance.
Earlier component reports retain their own executed profiles and limits. The formatter, capture
and source hashes describe their recorded revisions; they are not assertions about future inputs.

## Known limits

- Native XML construction, general Java-property resolution and live schema replacement are absent.
  The Java bridge supports only its documented settings and controlled source closure.
- Indexed schemas, additional full types/conversions, broader options/event flags, dynamic sessions,
  recovery/compression, writer/replay scheduling and network transport remain outside implemented profiles.
- No adapter beyond the documented Quote/Profile mappings is supplied. Raw timing and field bits
  do not imply business conversions or ticker/history delivery.
- Private aggregate tape observations in [remaining gaps](REAL-TAPE-GAPS.md) do not establish full-file support.
- macOS, LeakSanitizer, forced allocation failure and hostile concurrent filesystem behavior are
  unverified in the latest export validation. XML preflight is not a general hostile-input sandbox.
- Priority between invalid configuration and simultaneous output failure remains unspecified;
  see [schema-export validation](../tests/conformance/schema-export/VALIDATION.md).

[Open upstream questions](upstream/OPEN-QUESTIONS.md), [configuration requirements](CONFIGURATION.md),
[compatibility policy](COMPATIBILITY.md) and [roadmap](NEXT-STEPS.md) define the remaining boundaries.
