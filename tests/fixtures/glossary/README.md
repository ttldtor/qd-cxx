# Glossary fixtures

[Русская версия](README.ru.md). English is authoritative.

`oracle.tsv` is exact production Java output, with raw UTF-16 units encoded as four-digit
lowercase hex, `-` for empty and `~` for null. D rows observe each distinct reachable
attribute's name, description and ordered values; C/I rows observe CFI text/packed factories
and all decipher metadata; A/Q rows observe AU text parsing and unchecked static lookup;
M rows observe map formatting and the resulting parsed binary64 SPC. Numeric bits use
lowercase unpadded hex. Errors retain the production message prefixed with `!`.

The [maintained oracle](../../../tools/upstream/glossary-oracle/README.md) captures/verifies
this file. [Provenance](provenance.json) records the exact pin, sources, production classes,
JDK8 executables and harness/launcher hashes. The native test also exercises C++ model
bounds, lifetime and allocation failures; those checks are distinct from Java evidence.
