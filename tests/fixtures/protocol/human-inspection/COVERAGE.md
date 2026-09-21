# Designed human fixtures

[Русская версия](COVERAGE.ru.md). English is authoritative.

[Manifest](designed.json) is designed C++ presentation policy, not Java human-output evidence.
It contains 84 exact cases: 69 historical input references and 15 additive raw-QTP Unicode stimuli.
Historical Java/TSV files are reused byte-for-byte. Names, labels, empty values, unsafe escapes,
frame offsets and completion are compared exactly; [full coverage](../../../conformance/protocol/human-inspection/COVERAGE.md).
[Builder](../../../conformance/protocol/human-inspection/design_fixtures.py) writes only to an explicitly
requested new directory; verification never invokes it or rewrites fixtures.
[Validation](VALIDATION.md) identifies author results and limitations.
