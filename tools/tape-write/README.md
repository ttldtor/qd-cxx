# Offline tape normalization

[Русская версия](README.ru.md). English is authoritative.

`qdcxx-tape-write` implements [strict offline rewriting](../../docs/TAPE-WRITE.md).
It admits a complete supported RAW/MESSAGE recording, composes its original envelope
sequence through a fresh reader and the bounded writer, and publishes a fresh destination.
Source bytes, names, full field identities, raw values and timing distinctions remain
semantically represented; primitive encodings may normalize. No live connection is opened.

## Build and invocation

```text
cmake -S . -B build -DORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE=ON
cmake --build build --config Release --target qdcxx_tape_write
qdcxx-tape-write --input quote.qtp --output quote-normalized.qtp --tape-mode raw-message-v1 --profile default-quote-v1
qdcxx-tape-write --input input.qtp --output normalized.qtp --tape-mode raw-message-v1 --schema schema.json
```

The utility is optional and defaults OFF. Native JSON is privately linked only when
`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON`; otherwise `--schema` returns LOADER_DISABLED
before schema or tape I/O. Production libraries remain independent of JSON/file I/O.
The supported built-ins are `oracle-sre-v1`, `default-quote-v1`, `default-profile-v1`
and `oracle-field-types-v1`; they retain their existing exact local profiles.
No arguments and standalone `--help` print identical help without opening files.
Success is silent; the destination parent must exist and its final entry must be absent.

## Finite configuration

Help lists all **38** finite options and their exact defaults: the 31 tape/admission/writer
options in [TAPE-WRITE](../../docs/TAPE-WRITE.md#finite-limits), plus the seven unchanged
[native schema options](../../spec/schema/NATIVE-JSON.md).
Values are separate unsigned ASCII decimal tokens; zero is meaningful, duplicate options
and overflow are usage errors. Configuration is owned and resolved once before file I/O.

Reader and admission counters are independent for preflight and the fresh composition pass.
The second pass scans validated description IDs without allocating duplicate names and
guards temporary sender-ID/record-view/type-8 value containers before growth. Typed values
and symbols are borrowed only during the current writer call, before the owning feed result
is released. Failed reader results never contribute their completed record prefix to output.

The writer's shared text-visit allowance is the checked sum of `--max-descriptor-units`
and `--max-description-name-units`. Admission separately bounds descriptor text and retained
layout names. Writer comparison/sizing/copy/encoding visits consume this shared allowance,
so repeated visits can exhaust it before the retained-text bounds. Descriptor-string byte/unit
limits also apply to writer UTFString names; layout `--max-name-units` remains independent.
Overflow when deriving the shared allowance is resource exit 6.

Physical file work has separate derived finite allowances: `4*N+68` for source/schema reads
and `2*N+68` for output, with checked arithmetic and the respective byte limit `N`.
These cover byte transfers, worst-case one-byte positive progress, bounded read growth copies,
up to 64 exclusive-name attempts and checked completion operations. Retained native paths
are bounded to 4096 native code units. On Windows, option tokens use a checked UTF-8 bridge
without the ANSI code page; path operands retain their original native UTF-16 units after argument
validation. Unicode paths are exercised by the CLI regression; isolated-surrogate path execution
has not been tested.

## Completion and failures

Complete schema loading precedes alias checks and physical input EOF/checked close.
Strict admission precedes exclusive same-directory temporary creation. Each whole writer chunk
is written with checked partial-progress handling; reader finish, writer finish, flush and close
precede create-only hard-link publication. Existing/racing destinations are refused.
There is no overwrite or rename fallback. Only the exclusively owned temporary is removed.

Failures use the [documented exit categories](../../docs/TAPE-WRITE.md#outcomes-and-error-precedence)
and a content-free LF diagnostic. If temporary cleanup fails after publication, the destination
remains valid and `published=1` accompanies a nonzero exit. An additional bounded cleanup row
uses `owned-native-path-hex` to encode exact native path bytes without control-character injection.
No hostile parent replacement or crash/power-loss durability is promised.

The native CLI regression is [tape_write_cli.cmake](../../tests/conformance/tape_write_cli.cmake).
It checks help, all 38 defaults and number grammar, loader-OFF precedence, complete Quote/Profile
rewrites, all five native corpus cases when JSON is enabled, overlong compact-int normalization,
Unicode paths, existing/same destinations and finite input/output refusals. Actual platform acceptance,
production Java comparison and filesystem fault-injection results are separate verification evidence.
