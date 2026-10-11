# QDCXX-008 executed contract validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 in Windows/PowerShell with existing Python 3.11.1 (standard library only). [Contract](../JSON-CONTRACT.md), [case matrix](COVERAGE.md), [checker](../../../tests/conformance/check_json_contract.py).

## Executed checks

From the repository root:

```powershell
python tests/conformance/check_json_contract.py
git diff --exit-code -- tests/fixtures upstream.lock
git diff --check
```

The checker passed all 17 examples: 13 successful schemas and four failures (five diagnostic entries). It strictly parses JSON with duplicate-key/non-JSON-number rejection, checks format/profile, required/unknown members, integer domains, IDs, names, references, one head per logical field, Java category/ordinal order, LONG/WIDE_DECIMAL tail ownership, index positions/placeholders and lookup scopes. It compares ordered inputs, original input SHA-256, runtime outcomes, emitted generation order, active model field order/names/full type IDs, all Java slot names/types/counts and all captured positive/negative record/field lookup probes against unchanged TSV. Diagnostic attempts, recognition boundary, designed codes/types/subjects and evidence references match the four captured failures. Metadata/environment reference order and file existence are checked.

Thirteen in-memory negative checks passed: unknown format, unknown profile, unknown nested member, boolean ID, null fields collection, dangling field owner, incorrect tail owner, reversed slot order, duplicate lookup, incorrect index target, records on failure, incorrect recognition stage, and duplicate JSON object key. They do not modify examples or goldens. The checker validates the example corpus and selected mutations; it is not a complete validator for arbitrary documents. In particular extension payloads, all Unicode edge cases, arbitrary malformed shapes and resource limits are not exhaustively tested. No formal JSON Schema is supplied.

The fixture/upstream diff command passed without differences; schema and protocol inputs, expectations and captures remain unchanged. `git diff --check` passed. A local Markdown link audit over all changed/new Markdown files checked relative file/directory targets and reciprocal EN/RU links; all resolved. It did not fetch external URLs or validate heading fragments.

## Manual projection and bilingual review

Reviewed the unchanged [TSV corpus](../../../tests/fixtures/schema/expected/) against the [17-row matrix](COVERAGE.md) and generated JSON. S04 retains active declaration order separately from int/obj order and removes HiddenRecord/Hidden from the local artifact. S05 distinguishes real index heads, absent/disabled placeholders and both-disabled failure; undeclared index names can remain ordinary fields. S15-slots has six logical values, six int slots and two obj slots, with tails owned by Long and Wide. S07 keeps semantic/effective/full/local/property names and collapsed lookup keys separate. S08-before/after preserve input order and snapshot differences; S08-main-alias retains Copy.First.Name while source/evidence retains explicit Chosen.Name main metadata. No effective name is reconstructed from that declaration.

The no-URLs example preserves two distinct loader attempts, unlike successful empty XML. BYTE/SHORT retain successful model recognition and failed construction with the approved structured diagnostic; all-disabled indexes retain a distinct code and SchemeException evidence. Exact exception messages/classes/origins remain in linked TSV, not invented JSON observations. Profile metadata and the [environment inventory](../../../tests/fixtures/schema/captures/final-a/environment.tsv) retain the pinned provider context.

Reviewed EN/RU contract, coverage, validation, task/state/backlog updates for matching fields, constraints, conclusions, evidence, exclusions and executed-versus-pending distinctions. Both versions reference the same JSON, XML, TSV and capture metadata. Earlier task-history statements retain historical scope; the current approval/update notes supersede pending decision labels.

## Limits

These are designed projections prepared from existing observations, not fresh Java output. Manual review and script comparisons are document/evidence checks, not importer or C++ conformance. No Java run, importer, C++, build configuration, dependency selection, canonical identity/hash, protocol serialization or QDCXX-009 work was performed. Input SHA-256 identifies original bytes only. No full-session or three-format architecture is required.

Core consumption requires only resolved local contents; provenance auditing is separate. Native construction semantics, broader XML/provider profiles, alias collisions/removal, generators/imports, conversions and dynamic protocol/reset behavior remain deferred. [P05](../../protocol/CAPTURES.md) supports excluding requests/selection from local identity; it does not measure LONG/time-index wire projection or XML copy wire effects. Those probes and broader malformed-input validation remain later work.
