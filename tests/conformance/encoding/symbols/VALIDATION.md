# Final symbol validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

## Initial implementation checks (historical)

recorded checks executed 2026-09-21 UTC after the integration/test and readability follow-ups. This is
not an independent review report. No errors were detected in these checks; no full protocol claim.

| Final profile | Result / UTC evidence |
| --- | --- |
| MSVC 19.42.34444, Debug / Release, x64 | All five suites pass: Debug, Release |
| ClangCL 19.1.5, Debug / Release, x64 | All five suites pass: Debug, Release |
| WSL GCC 13.3.0, Debug / Release, x86_64 | All five suites pass: Debug, Release |
| GCC Debug ASan+UBSan | All five suites pass; -fsanitize=address,undefined -fno-omit-frame-pointer: log |
| Production-only GCC, BUILD_TESTING=OFF | Configure/build pass with FETCHCONTENT_FULLY_DISCONNECTED=ON; no _deps directory: build |
| Pinned formatting | Uncrustify 0.83.0 then clang-format 20.1.8, 20 files: Check; google-java-format 1.36.1/JDK21, seven Java files: Check |
| Negative/baseline fixture checks | Ten mutations/missing inputs fail, unchanged baseline passes: [proof](negative-proof.json), log |
| Direct symbol suite | Two tests, 29130 assertions: counts |

The four existing compact/UTFString, framing, description and binding suites pass alongside the new
symbol suite. Build logs with matching names are retained in logs. [Build profile manifest](build-profile.json)
records compiler configuration and sanitizer flags; [source hashes](source-sha256.pre-tsv-review.json) identify final
code/CMake/test inputs. All build/test commands run via tools/run_utc.py. Existing configuration
commands are in [CPP-BUILD](../../../../docs/CPP-BUILD.md); use QD_SYMBOL_FIXTURES only for temporary
negative tests. The reusable [negative checker](../../check_symbol_negative.py) never writes goldens.

[Java validation](../../../fixtures/encoding/symbols/VALIDATION.md) records two clean equal captures,
negative/baseline Verify, full provenance and the 706 unchanged historical fixture/lock paths.
Final audit checks exact current tool/input hashes, capture equality,
historical hashes, source hashes, relative links and diff whitespace. EN/RU claims and evidence links
were synchronized. Independent review remains pending.

The initial sandbox denied Windows SDK/WSL access; authorized execution with access passed. An initial
local Linux runner ended on a CRLF blank line after successful commands; its LF-corrected full matrix
passed. Early post-build checks following a denied build were not accepted as final evidence. Final
matrix was rerun after all code and CMake cleanup; no source/header semantics changed afterward.

Limits: EMPTY_SET codec only, provisional API, explicit caller reset, atomic terminal C++ errors
rather than Java continuation. No allocation-failure injection, huge resources, 32-bit target,
exhaustive penta/Unicode/flag enumeration, resolver, negotiated/event/time semantics, event decoder,
writer, importer, incoming-ID installation, PART, transport, performance or complete protocol parity.
See [coverage](COVERAGE.md) and [contract](../../../../spec/encoding/SYMBOLS.md).

## TSV review follow-up

Rerun on 2026-09-21 UTC after the TSV-only review fix; this is not independent review.
All 13 row types now have exact column/schema checks and explicit comparison or diagnostic-only
handling. Exactly one complete must terminate the file. See the [schema/coverage](COVERAGE.md#tsv-review-follow-up).
Production code, Java tooling, fixtures and upstream.lock were not changed or regenerated.

| New negative case | Actual result |
| --- | --- |
| unknown-row, before complete | Rejected, exit 1; Unknown Java TSV row type |
| wrong-column-count, trailing empty profile column | Rejected, exit 1; Wrong TSV column count |
| duplicate-complete | Rejected, exit 1; observations after complete prohibited |
| row-after-complete, valid parse.begin | Rejected, exit 1; observations after complete prohibited |

The checker asserts those specific diagnostics, retains all original ten negative cases and verifies
that every temporary input and the baseline remain unchanged. All fourteen negatives were rejected;
the unchanged baseline passed. [Proof with diagnostics and baseline SHA-256](review-tsv-negative-proof.json),
UTC log. Mutations are temporary copies only.

Final matrix after the code changes: all five suites pass on MSVC 19.42.34444
Debug / Release,
ClangCL 19.1.5 Debug / Release,
GCC 13.3.0 Debug / Release
and GCC ASan+UBSan. The new production-only directory uses
BUILD_TESTING=OFF / FETCHCONTENT_FULLY_DISCONNECTED=ON and has no _deps:
configure, build,
[profile manifest](review-tsv-build-profile.json). The symbol suite has two tests / 29245 assertions:
counts. Negative mutations were run with the MSVC Debug executable.

Pinned C++ Format then Check and
Java Check pass. Java sources were not edited; no Java oracle rerun
was required. Evidence audit passes with updated hashes for only the two
changed test sources in [current source manifest](source-sha256.json). The previous manifest is preserved
[verbatim](source-sha256.pre-tsv-review.json). All previous logs, negative proof and build-profile remain
untouched. Preservation report records the before/after SHA-256 comparison
for production, tooling, fixtures, lock and prior evidence. EN/RU coverage/validation and links are synchronized.
No errors were detected in these checks. Existing protocol/allocator/architecture coverage limits still
apply.
