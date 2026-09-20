# Upstream analysis tools

This directory is reserved for reproducible tools that inspect the locked Java QD checkout or capture oracle output for fixtures.

Tools must:

- require or verify the expected upstream commit;
- avoid mutating the upstream checkout by default;
- produce deterministic output where possible;
- record invocation and relevant environment details;
- keep generated artifacts separate from hand-maintained specifications.

No tools are implemented yet.
