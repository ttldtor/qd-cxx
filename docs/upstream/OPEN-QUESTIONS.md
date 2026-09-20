# Upstream open questions

Record questions about Java QD behavior that cannot yet be answered confidently. Link each question to the task investigating it and close it with source or fixture evidence.

## Provenance

- **QDCXX-002 — GitHub repository authority**
  - **Status:** non-blocking provenance question.
  - The upstream root POM at the locked commit identifies the internal Devexperts Stash repository (`https://stash.in.devexperts.com/scm/mdd/qd.git`) as its SCM source, while `qd-cxx` uses the public `https://github.com/devexperts/QD.git` repository.
  - For the pinned QD 3.355 baseline, the public tag, commit SHA, root POM version, and release notes are consistent, so the locked source tree is reproducible and unambiguous.
  - `qd-cxx` therefore treats the pinned GitHub repository and immutable commit SHA as the **public reference source** and uses that Java source tree as the compatibility oracle.
  - Whether the GitHub repository is the canonical repository, an official mirror, or another form of synchronized public copy remains unverified.
  - This uncertainty does **not** block specification extraction, conformance work, or C++ implementation against the pinned baseline.

## Initial questions

- Which XML resources and Java loaders collectively define the effective DataScheme?
- Which schema rules are normative, conventional, or module-specific extensions?
- How are aliases, mappings, defaults, and record inheritance/composition resolved?
- Where are QTP frame boundaries established, and which layer owns message boundaries?
- Which QTP negotiation and state transitions are version- or option-dependent?
- Which encodings retain state across messages or connections?
- What malformed-input behavior is externally observable and relied upon?
- Which upstream tests are suitable as direct sources for conformance fixtures?

For each answer, distinguish explicit upstream contract from observed behavior and implementation accident.
