# Upstream open questions

Record questions about Java QD behavior that cannot yet be answered confidently. Link each question to the task investigating it and close it with source or fixture evidence.

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
