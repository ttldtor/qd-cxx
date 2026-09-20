# Java QD upstream

The reference implementation is `devexperts/QD`. The exact baseline is stored in `upstream.lock`; changes to that file are deliberate compatibility events, not routine dependency updates.

## How upstream is used

- As a behavioral oracle for wire, schema, encoding, and state-machine questions.
- As a source of tests, resources, and examples that reveal contracts.
- As historical evidence when behavior is undocumented.
- Not as an architecture template that must be copied into C++.

## Traceability convention

Research notes and specifications should cite stable evidence where practical:

- repository and commit;
- module and relative file path;
- class, method, resource, or test name;
- relevant line range or symbol;
- observation command or fixture-generation procedure.

If a conclusion is inferred rather than explicit, label it as an inference and describe how it can be verified.

## Updating the lock

An upstream update should be performed in a dedicated task. Review release notes and relevant diffs, regenerate affected observations or fixtures, update specifications, and document compatibility impact before accepting the new commit.
