# Java style and automatic formatting

[Русская версия](README.ru.md). English is authoritative.

Repository-owned Java follows [Google Java Style](https://google.github.io/styleguide/javaguide.html).
[google-java-format](https://github.com/google/google-java-format) is the authoritative automatic
formatter, in its default Google mode: 2-space block indentation and a 100-column limit with the
guide's exceptions. AOSP mode is not used. [.editorconfig](../../.editorconfig) supplies basic editor
settings; it does not replace the formatter. AGENTS.md carries these instructions
into future sessions, and [CONTRIBUTING.md](../../CONTRIBUTING.md) documents the review workflow.

## Commands

PowerShell 7.4+, Git and a JDK 21 or newer are required. From the repository root:

```powershell
$formatterJdk = 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
pwsh -File tools/format/Format-Java.ps1 -Mode Format -JavaHome $formatterJdk
pwsh -File tools/format/Format-Java.ps1 -Mode Check -JavaHome $formatterJdk
```

`Check` is the default. It reports files that would change and fails without changing Java sources.
`Format` rewrites sources, then `Check` must pass. The same Check command can be used in CI.
If `-JavaHome` is omitted, the wrapper uses JAVA_HOME, then java on PATH when JAVA_HOME is unset.
It rejects JDK 8 with an explicit diagnostic: the formatter JDK is separate from the pinned QD
oracle's JDK 8. No global JAVA_HOME, Git hooks, IDE settings, production dependencies or upstream
build configuration are changed.

## Reproducible tool and scope

[google-java-format.lock.json](google-java-format.lock.json) pins version **1.36.1**, the official
release asset URL and SHA-256. On first use, the wrapper downloads the jar into ignored `.work/` and
checks its hash before executing it. It also checks cached jars on every run. A mismatching download
or cache is rejected. Download requires network access; subsequent runs use the verified local cache.
Updating the formatter is an explicit lock change followed by Format, Check and affected validation.

File discovery uses `git ls-files --cached --others --exclude-standard -- '*.java'`: tracked and new
nonignored repository Java files are included. Captures, expected outputs, target and `.work`
directories are excluded. The external pinned Java QD checkout is outside this scope. No golden
files are formatted. Do not use a floating IDE formatter version as the final check.

The formatter controls layout and imports, but it is not a complete style checker or semantic
validator. Review the guide's other rules, including named packages, explicit imports, mandatory
control-flow braces and naming. Keep literal Unicode escapes where they specify raw oracle inputs.
All three existing harnesses were migrated: schema and encoding now use package `qdcxx.oracle`;
protocol keeps `com.devexperts.qd.qtp` for upstream package access. Launchers use the corresponding
qualified entry points. No style exceptions are introduced for these harnesses.

## Oracle provenance after source edits

Formatting changes source hashes and can change bytecode debug metadata. Run Format before final
oracle captures. After editing a harness or a launcher, rerun affected Verify workflows and preserve
the actual new metadata and logs. Never replace a hash in historical metadata by hand and never
update expectations merely to accommodate formatting. SchemaOracle is also the environment
inspector for protocol and encoding, so changing it affects all three provenance chains.

For this migration, previous schema/protocol evidence remains historical and unchanged; new Verify
evidence is retained under evidence. Encoding's four current captures were refreshed
from two independent clean builds and negative/positive Verify. Every existing expectation remains
unchanged. See [executed validation](VALIDATION.md) for exact checks and links.
