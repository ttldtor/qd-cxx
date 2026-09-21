# Binding Java validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Author execution, 2026-09-21 UTC; not an independently rerun review. QD 3.355 / locked commit
0ddc0a09779fb61d74cc0b37c6449ff47dcb715f, Zulu JDK 8.0.422 and Maven 3.9.16. Native clean reactor builds
ran sequentially, cache disabled, upstream tests skipped, no source/POM patch. Only CRLF was normalized.

- [capture-a](captures/capture-a/metadata.json) and [capture-b](captures/capture-b/metadata.json)
  independently reproduced all 13 scenarios; normalized binding.tsv files are byte-identical.
- [mismatch](captures/mismatch/result.txt) detected a temporary delivered x value change 101 to 999.
- [baseline](captures/baseline/result.txt) passed exact Verify with unchanged expectations.
- [proof](captures/proof.json), [first capture log](captures/binding-capture-a.log),
  [sequential verification log](captures/binding-evidence.log) preserve the executed workflow.
- Historical hashes cover every pre-existing fixture file and
  upstream.lock. Audit result checks immutability,
  current hashed probe/input/helper sources, capture equality and projection agreement with delivery.

Each run retains raw stdout/stderr, environment.tsv, metadata, build/compiler logs and normalized
observations. Environment profiles and helper hashes are honest new provenance; historical sources,
captures and expectations remain untouched. The first development attempt failed compilation because
of an incorrect ProtocolOption constant; the corrected, formatted source was used by all four retained
runs. Initial compiler diagnostic
is troubleshooting evidence only, not a successful capture.

No errors were detected in these completed checks. The undescribed case intentionally reports one
corrupted-message callback, and unknown records intentionally log skips. Ordinary type-8 production
observations support the [bounded claims](COVERAGE.md); duplicate intermediate writes remain
source-supported, not externally observed. No upstream suite, provider/property matrix, metadata,
conversions, defaults, reset, transport or C++ event decoder was tested or introduced.
