# QDCXX-013 validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Executed 2026-09-21 UTC. [Java evidence](../../../fixtures/protocol/p08/VALIDATION.md) preceded C++.
[Coverage](COVERAGE.md), [negative proof](negative-proof.json), audit, UTC logs.

MSVC 19.42 and ClangCL 19.1.5 Debug/Release, WSL Ubuntu 24.04 GCC 13.3 Debug/Release and GCC
ASan+UBSan all pass three CTest targets: compact_java_vectors, protocol_java_frames and
protocol_java_descriptions. The new target passes 3 cases / 50,841 assertions / 81 Java observations.
Compact/UTFString regressions retain 832+314 observations; framing retains its previous selected coverage.
Initial sandbox access to Windows SDK and WSL was denied; authorized elevated runs passed. No compiler
or semantic failure was detected in the executed checks. BUILD_TESTING=OFF configures/builds both production libraries in a fresh
WSL directory without creating _deps; FetchContent is not reached and no doctest acquisition occurs.
No new production dependency was added.

Pinned Uncrustify 0.83.0 then clang-format 20.1.8 Format/Check and Java google-java-format 1.36.1
Format/Check pass. Eight negative fixture checks detect mutations and invalid/empty coverage, then the
baseline passes unchanged. Source review checked Doxygen, ownership, range-versus-operation boundaries,
subtraction-based aggregate arithmetic and no reserve/allocation indexed by declared counts or sender ID.
Final evidence audit checks old fixture/upstream.lock hashes, independent capture equality, current probe
source/input hashes, historical payload origins, all strict prefixes, EN/RU artifact targets and relative
Markdown links. git diff --check passes.

Reproduce with the existing [build commands](../../../../docs/CPP-BUILD.md); they include the new target.
Use QD_DESCRIPTION_FIXTURES only for temporary negative selection, or run
`description_tests tests/fixtures/protocol/p08/expected/descriptions.tsv` directly. Run
`python tests/conformance/check_description_negative.py <executable> --report <new-report>` and
`python tests/conformance/protocol/descriptions/audit.py` for bounded audits. Log build/test commands
through `python tools/run_utc.py --log <path> -- <command>`.

Executed sanitizers are GCC ASan+UBSan only. Windows sanitizer configurations, allocator-failure
injection, huge allocations, 32-bit/macOS, other Java buffer/profile combinations, local binding,
events and transport were not run or implemented. The API is provisional and full QTP compatibility
is not claimed. Historical evidence remains unchanged; new results are additive.

## Follow-up review, 2026-09-21

Review covered compact, UTFString, framing and raw descriptions, tests and the Java description harness.
MSVC Debug/Release rebuilds and all three CTest targets were rerun successfully. The encoding evidence
audit, P08 audit and eight description negative checks passed, followed by the unchanged baseline.
P08 audit verified 534 historical files. Its report write was suppressed to preserve the existing audit.json.
These review runs are recorded in the review session; they do not replace the committed UTC logs above.
ClangCL, GCC/WSL, sanitizers and Java captures were not rerun in this review. No blocking defect was found;
this is not proof of absence of defects.

Raw intermediate results/positions come from a source-checked diagnostic walk using Java primitives,
not raw callbacks of the production parser. Agreement with that walk alone is not an independent
end-to-end proof. QDCXX-014 requires binding evidence through observable
production-parser results; diagnostic and internal inspection evidence must remain separately labeled.
