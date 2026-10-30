# Finite binary tape catalog coverage

[Русская версия](COVERAGE.ru.md). English is authoritative.

The [support matrix](../../../fixtures/protocol/real-tape/support-matrix.tsv) has
71 rows. Each row identifies both exact Java pins, the selected contract,
positive/negative requirements and actual evidence. A row marked
`not-applicable` is an explicit exclusion from this profile, not implemented
support. A source characterisation alone does not satisfy a row's runtime check.

| Dimension | Rows | Bounded claim and evidence |
| --- | ---: | --- |
| Field | 21 | 19 finite identities; reserved alternatives refused. [Scalar](../field-types/SCALAR-VALIDATION.md), [object](../field-types/OBJECT-VALIDATION.md) and [type validation](../field-types/VALIDATION.md). |
| Schema | 8 | Explicit indexed/time layouts, snapshots and finite schema state. [Indexed validation](../field-types/INDEXED-VALIDATION.md). |
| Symbol | 2 | Owned exact symbol state, bounded repeat/default behavior. [Flag validation](../field-types/FLAGS-VALIDATION.md). |
| Flag | 8 | Supported raw flags and removal metadata; unsupported bits refuse before publication. [Flag validation](../field-types/FLAGS-VALIDATION.md). |
| Message | 14 | Finite raw dispatch and explicit unsupported control/family policies. [Dispatch validation](../field-types/DISPATCH-VALIDATION.md); container markers select the separate envelope layer. |
| Descriptor | 3 | Exact finite properties, extension and successful-delta policies. [Dispatch validation](../field-types/DISPATCH-VALIDATION.md). |
| Option | 3 | Explicit option grammar and unsupported alternatives. [Dispatch validation](../field-types/DISPATCH-VALIDATION.md). |
| Format | 5 | BINARY only; other formats excluded. [Catalog contract](../../../../spec/protocol/BINARY-TAPE-COVERAGE.md). |
| Time | 4 | Explicit NONE/MESSAGE/FIELD/LONG/TEXT; [FIELD validation](../field-types/FIELD-TIME-VALIDATION.md), [sidecar contract](../../../../spec/protocol/SIDECAR-TIME.md), [source validation](VALIDATION.md). |
| Container | 2 | Explicit uncompressed or optional bounded GZIP/ZIP; [container contract](../../../../spec/protocol/TAPE-CONTAINERS.md). |
| Source | 1 | Finite self-contained segments and coordinated close; [sequence contract](../../../../spec/protocol/TAPE-SEQUENCE.md). |

The final matrix has 57 `implemented-bounded` and 14 `not-applicable` rows;
none remains pending. Earlier catalog snapshots had four pending rows. They
are resolved only within the following explicit profiles:

| Rows | Positive proof | Refusal and failure boundary |
| --- | --- | --- |
| M75 / C1 | Production FileWriter/FileReader envelope observations plus final native `compressed_input`, `source_sequence_io` and `source_sequence_cli` checks; identical logical records for explicit NONE/GZIP/ZIP. | Physical container markers are not inner QTP message families. GZIP validates every concatenated member; ZIP32 admits one nondirectory file and bounded empty directories. CRC/trailer/directory/trailing-byte failures prevent completion. General ZIP, ZIP64/encryption/multiple files and unavailable compression remain refused. |
| TIME3 | Production LONG/TEXT observations, native `sidecar_scanner`, `sidecar_policy`, `sidecar_paired_owner` and final source sequence checks preserve exact reached mark positions, raw timestamps and effective time. | Explicit companion required; no implicit search or relative/ambient-clock time. Missing/late malformed/read/close failures cannot authenticate completion. Logical positions refer to inflated data, not compressed physical bytes. |
| CHAIN | Production split-file observations plus final native admission/I/O/CLI/three-million-record sequence checks preserve explicit occurrence order, per-segment schema state and source-local/aggregate counters. | Self-contained segments and homogeneous time only; compatible descriptor updates remain supported. Missing pieces, unavailable decoders, parser/scanner/trailer/close/flush/cancellation failures preserve a bounded completed prefix and prevent a final complete result. No live follow/cycle or implicit globbing. |

The [validation report](VALIDATION.md) records final 74/74 Windows and Linux
ASan/UBSan native suites, six production-only profiles, 120 Windows Java workflow
checks and the exact artifact/helper hashes. Historical partial reports remain
qualified to their original snapshots; this result does not rewrite their hashes.

For TIME3, acceptance includes reached companion position/time marks, separate raw
and effective time, final companion EOF and late malformed/read/close failures.
For M75/C1, acceptance includes inflated-record equality, member/entry policy,
CRC/trailer/trailing bytes, exact/excess physical and logical budgets, and
compression-OFF refusal without acquiring dependencies. CHAIN requires source
order, per-segment schema reset, homogeneous time declarations, one completion
after all closes and missing/late failing segments. Each row keeps explicit
unsupported policies for valid Java behavior beyond the selected finite profile.

The ordinary complete-file comparison in [validation](VALIDATION.md) covers every
value, generation, source binding and record order of its named corpus. Its fresh
MESSAGE time digest does not establish FIELD, sidecar or compressed parity for
unrelated inputs. Public production fixtures remain the reproducible authority
for those dimensions; private recordings and raw logs are not published.
