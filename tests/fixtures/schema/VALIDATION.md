# QDCXX-005 executed validation

[Русская версия](VALIDATION.ru.md). This English document is authoritative.

Executed on **2026-09-20**, Windows 11 amd64 / PowerShell 7.6.6, upstream QD 3.355 commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Zulu JDK 1.8.0_422-b05 and Maven 3.9.16 were already installed. JAXP is the JDK's Xerces DOM/SchemaFactory implementation. The native upstream reactor was rebuilt with tests and build-cache reuse disabled for each run; the enforcer remained enabled.

## Results

| Check | Result |
| --- | --- |
| Locked origin/HEAD/peeled tag, POM/release-notes version, clean source | Passed before and after each run. |
| Capture A and independently rebuilt capture B | All 17 cases complete; each normalized output equals the reviewed expectation byte-for-byte. |
| Negative verify | Changed only temporary S01-empty runtime record count `0` to `999`; rejected exactly `S01-empty.tsv`. |
| Positive verify | All 17 reviewed baseline files pass. |
| Expected-file immutability | Hashes of checked-in expectations and of the deliberately changed temporary expectation set remain unchanged by verification. |

Final capture A/B raw stdout files also match byte-for-byte before normalization. All 12 runtime classpath directory fingerprints match after independent clean builds. Current harness/tool and input hashes match the recorded metadata; all four runs regenerated the same 17 normalized observations (68 comparisons with baseline), including the negative verify run whose temporary expectation alone was wrong.

The 17 cases include **13 successful runtime constructions**, **three model-success/runtime-rejection cases** (both disabled indexes, BYTE, SHORT), and **one rejection at both loader APIs** (no URLs). Expected rejection does not mean infrastructure failure. All case stderr streams are empty. Two separate direct loads per case distinguish model and runtime outcomes, but both occur within that case's JVM; distinct cases never share a JVM.

## Reproduction and evidence layout

Use the commands in the [harness instructions](../../../tools/upstream/schema-oracle/README.md). Final execution used run directories `.work/final-a`, `.work/final-b`, and `Test-Verify.ps1 -TestDirectory .work/final-verify`, with the external checkout `$env:TEMP/qdcxx-003-qd` and the installed tool paths recorded in metadata. Each `metadata.json` contains exact native executable/argument arrays and working directories. The negative self-test uses the same full launcher, rebuild and fresh-JVM generation as positive verify; it does not merely test a mock comparison function.

- [Capture A](captures/final-a/metadata.json) and [capture B](captures/final-b/metadata.json): raw stdout/stderr for every case, environment/provider inventory, version/compiler/build logs and metadata copied unchanged from the corresponding run directories. Generated Java classes, Maven cache and duplicate normalized files are not checked in.
- [Negative verify](captures/mismatch/metadata.json) and [positive verify](captures/baseline/metadata.json): metadata, environment/version/build logs and exact result messages. Their regenerated per-case file hashes are retained in validation.json; duplicate raw/normalized streams remain in ignored working directories.
- [Self-test proof](captures/verify-proof.json): mutation, exact observed failure, baseline success and before/after expectation hash assertions. The modified expectation itself remains only in the ignored test directory.
- validation.json: per-run raw/normalized output hashes, counts, baseline comparison and classpath comparisons. It can be checked against the preserved raw files and current expectations.
- [expected](expected/): reviewed normalized output, explicitly promoted from the first reviewed capture. No Java source behavior was replaced by hand-authored expected values.

Normalization replaces only the exact input-directory `file:/.../inputs/` prefix with `<INPUT>/` and CRLF with LF. No semantic ordering, exception message, alias, type, slot or line number is changed. Metadata retains machine paths; it is not a golden semantic expectation. Maven itself ran in its inherited ru_RU/Cp1251 environment; the **oracle JVMs** explicitly used en/US, UTC and UTF-8, as their environment inventory records.

## Review and limits

Reviewed outputs against `SchemeModel.Loader.load`, `DXScheme.Loader.load/buildOneRecord/addIndexField`, `SchemeRecord.Field.copyFrom/getMainAlias`, `SerialFieldType.createDefaultIntInstance`, and `DefaultRecord` constructor/name installation at the locked revision; see [source index](../../../spec/schema/SOURCES.md) and [measured findings](../../../spec/schema/README.md#measured-evidence-qdcxx-005).

The captured failures confirm the source interpretation: BYTE/SHORT factory returns null, which DefaultRecord dereferences; all-disabled indexes explicitly throw SchemeException. Copied `main=true` metadata survives while the main-alias pointer does not, producing the observed fallback. Model HashMap traversal is preserved as an observation and clearly separated from generation/runtime order. No unexplained runtime disagreement was normalized away.

The [coverage manifest](COVERAGE.md) lists deferred portions of every exercised family. This is not full schema coverage, dxFeed provider/property parity, parser-edge validation, an upstream test-suite run, QTP evidence, a normalized-model specification, or C++ conformance. QDCXX-006 and C++ were not started.
