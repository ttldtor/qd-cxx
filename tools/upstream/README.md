# Upstream analysis tools

This directory is reserved for reproducible tools that inspect the locked Java QD checkout or capture oracle output for fixtures.

Tools must:

- require or verify the expected upstream commit;
- avoid mutating the upstream checkout by default;
- produce deterministic output where possible;
- record invocation and relevant environment details;
- keep generated artifacts separate from hand-maintained specifications.

[schema-oracle](schema-oracle/README.md) implements QDCXX-005: verified locked-source Java builds, 17 bounded schema subcases, raw/normalized captures, provenance metadata and read-only golden verification. QTP tooling and production C++ remain outside this task.

QDCXX-006 adds the [protocol oracle](protocol-oracle/README.md), reusing schema-oracle build/provenance conventions. Never run clean-build harnesses concurrently on one upstream checkout.

The [container oracle](tape-container-oracle/README.md) captures production FileWriter/FileReader
and StreamCompression observations at either accepted baseline/current pin using existing read-only
class directories. Capture/Verify, explicit native UTC logging and built-in help require no upstream
rebuild. The [developer catalog](../dev/README.md#optional-tape-containers-and-production-oracle)
documents the separate manual help regression and optional native compression component.
