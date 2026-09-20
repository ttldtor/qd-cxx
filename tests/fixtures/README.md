# Compatibility fixtures

Fixtures will store small, reviewable inputs and expected observations for schema, encoding, and protocol compatibility.

Each fixture set must document:

- upstream repository, version, and commit;
- generation tool or exact manual procedure;
- relevant options, locale, and environment assumptions;
- whether output is captured, normalized, or hand-authored;
- the specification rule and conformance test that consume it.

Avoid large opaque snapshots. Prefer focused cases with human-readable metadata and stable binary representations where applicable. Do not add upstream-derived content until provenance and licensing are understood.
