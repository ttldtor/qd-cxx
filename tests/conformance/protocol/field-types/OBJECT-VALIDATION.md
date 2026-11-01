# Owned byte/string/object fields: validation

[Русская версия](OBJECT-VALIDATION.ru.md). English is authoritative.

Executed 2026-10-30 UTC for the explicit ordinary read-only BINARY_FIELDS_V1
extension with IDs 9/137/233/249. The [field contract](../../../../spec/encoding/FIELD-TYPES.md)
defines exact values, mapping-independent grammars and logical resource bounds.
The [capture manifest](../../../fixtures/encoding/field-types/object-captures.json)
records final oracle-source/runtime/classpath-inventory hashes and every original
capture hash at both pins. No private tape or Java object execution is involved.

## Executed production evidence

QD3.355 `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` and QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19` each produced 49 original artifacts:
48 files from requested composer/FileWriter captures and one table of 803 rows
(801 observations and two framing rows). Two
final-source prebuilt-classpath captures per pin match the reviewed expectations
exactly. The locked pin also passed the actual complete governed clean-reactor
Verify, including the unchanged 82 root and 25 scalar artifacts and 49 new object artifacts.
The current-pin captures use its already qualified pinned classes; they are not
a second clean-reactor Verify. Runtime is Zulu8u422, heap128MiB, timeout30s;
the formatter JDK is separate OpenJDK23.0.2.

| Surface | Observations per pin | Meaning |
| --- | --- | --- |
| Requested BinaryQTPComposer |24 | Four exact wire IDs, null/empty/content, STREAM and RAW |
| FileWriter/FileReader |24 | Actual local types9/137/249; requested233 uses local9 |
| BinaryQTPParser |324 | Known and unknown-field consumers, both data families |
| Additional parser bindings |186 | Unknown records and repeated targets |
| Production FieldReader over BufferedInputPart |162 | Exact field/sentinel values, errors and examination |
| Supporting bounded primitives |81 | Primitive result and position, separate from message observation |

RAW's three callbacks are fanout from one wire record. Only requested composer
captures establish wire233; its FileWriter uses ByteArrayField's actual9 identity.
MarshalledObjField observations call getBytes only. Known249 projects compact INT;
unknown Java objects instead use LONG byte arrays. Known137 validates UTF, while
unknown Java object skips do not. Generic C++ intentionally applies exact249 INT
and137 UTF validation even when skipped. These differences are tested, not hidden
behind a Java local-conversion or full DataScheme compatibility claim.

Captures cover null/empty/opaque bytes, known serialized-string bytes and arbitrary
unknown tags,4096-byte content, negative/high32/overlong/truncated lengths, finite
body prefixes, BMP/supplementary/CESU/unpaired/overlong UTF, invalid tails/ranges,
declared-byte sequence splits and true tail exhaustion. A malformed split may
examine following bytes before failure; position charges reflect actual examination.
Explicit timestamped cursors make RAW files deterministic without normalization.
Observation rows use explicit LF and no BOM, preserving original capture hashes
under the fixture text policy. Capped UTF-unit room is proven at INT32_MAX and
SIZE_MAX arithmetic boundaries before either one-unit or two-unit append; no
multi-gigabyte allocation is used to demonstrate this bound.

The governed comparator validates finite files/rows/canonical projections and all
bytes. All28 deliberate mutations were rejected, followed by unchanged-baseline
PASS. Five new mutations target null/empty, payload bytes, UTF examination,
completion and actual local identity. Historical capture receipts remain unchanged.

## Executed C++ recorded checks

Windows MSVC19.42 Release focused build and four CTest entries passed on the frozen
source: `field_type_java_values`, `field_type_java_tape`, `object_field_owned_values`
and `field_type_inspection_io`. These are recorded checks, not independent review.

The tests consume both pin directories through actual FileWriter containers and
requested composer frames with chunk sizes1/7/65536. Generic canonical values,
sentinels and known-consumer examination match production observations. Unknown
field/record and repeated-target paths test fixed exact grammars. Further cases
cover old aggregate meaning and profiles, invalid manual payload mixes, defaults,
deep ownership after source/context/schema/reader destruction, RAW/STREAM equality,
zero/L-1/L/L+1 work/storage bounds, skipped/replaced work without refunds, mixed10/137
unit charges, old+temporary/completed+current+temporary peaks, injected allocation
failure and completed-prefix retention, multi-frame terminal charges, and early
read-only OutgoingLayouts refusal. Generic formatter versions4/6/7 refuse every
new payload identity including null/empty; no payload is silently omitted.

Pinned Uncrustify0.83.0 followed by clang-format20.1.8 Format/Check passed for13
changed C++ files. Global Java Format/Check passed for31 files; final-source
production checks followed formatting. Doxygen completed with zero warning bytes.
Final-source Windows x64 MSVC 19.42 Release and Linux x64 GCC 13.3 Debug
ASan/UBSan each passed all 54 CTests. Leak detection was disabled; no LeakSanitizer
qualification is claimed. Both platforms passed production BUILD_TESTING=OFF
profiles for minimal libraries, tools/replay with JSON OFF, and tools/replay with
JSON ON. The JSON-OFF wire CLI passed; no production test-framework acquisition
or JSON-OFF optional-dependency acquisition occurred. JSON ON used explicitly
supplied existing simdjson sources.

Independent critical review found the STRING unit-reservation overrun, verified
the checked preappend fix and its allocation-free INT32_MAX/SIZE_MAX proofs, and
passed copied Windows value/tape/ownership/formatter tests. Its final LF recheck
verified captures, source hashes and copied tape/formatter tests. Final Java
Format/Check passed for 31 files without further source changes; the final-header
Doxygen run had zero warning bytes with EXTRACT_ALL enabled, which does not by
itself prove complete member documentation.

## Limits

These observations qualify a finite ordinary generic field projection. Existing
profiles and container grammars remain exact; outgoing admission/replay producer
support is not broadened. Indexed schemas, description replacement, broader flags,
families/container policies, output grammar, event adapters, Java object semantics,
transport and complete private-tape compatibility remain outside this change.
Logical limits exclude allocator capacity/metadata and RSS. Source equality or
saved-artifact consistency is not an independently repeated runtime observation.
