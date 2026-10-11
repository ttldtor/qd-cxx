# Outgoing production captures

[Русская версия](CAPTURES.ru.md). English is authoritative.

QDCXX-031 adds 43 artifacts to the unchanged QDCXX-022/023/024/025/030 evidence. The upstream revision
is `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` (QD 3.355); a fresh clean checkout's origin, HEAD and tag
were checked. One clean `qds-file -am` reactor build succeeded in 1:08. Captures used JDK 8
`1.8.0_422`, UTF-8, English locale and child UTC, without scheme overrides.

The [reviewed input table](inputs.tsv) is checked against the harness's exact eight input samples
before output creation. [Provenance](expected/provenance.tsv) records actual tool/input/provider
source hashes and the lock; [classpath receipt](expected/classpath-receipt.json) records all fourteen
actual upstream bytecode/resource trees. Machine paths, raw logs and run-specific metadata are kept
separate from deterministic expectations. There is no normalization of new artifacts.

Two fresh captures, on 2026-10-11 at 20:54:20–20:54:24 UTC, compared equal across all 43 artifacts.
A separate production Verify ran at 20:54:57–20:55:00 UTC. The source and provider hashes in the
retained expectations belong to these actual executions, not copied historical metadata.

| Witness | Production observation |
| --- | --- |
| [Custom RAW/MESSAGE](expected/extras/extras-raw-message.tsv) | Eight records, zero parser errors, 24 RAW fanout callbacks; three RAW data frames for the timestamped batch. |
| [Custom STREAM/NONE](expected/extras/extras-stream-none.tsv) | Eight records, zero errors, eight callbacks; one STREAM data frame. |
| [Custom RAW/NONE](expected/extras/extras-raw-none.tsv) | Explicit raw-string symbols; eight records, 24 callbacks, one RAW frame; no event timing. |
| [Singleton visits](expected/extras/extras-raw-singletons.tsv) | Same eight inputs supplied individually; eight RAW frames, not the batch's three. |
| [Regular heartbeat input](expected/extras/extras-heartbeat-regular.tsv) | Non-timestamped buffer uses explicit heartbeat time; eight records in one RAW frame. |
| [hs](expected/extras/extras-raw-hs.tsv) / [null requested options](expected/extras/extras-raw-default-options.tsv) | Zero flags and exact raw values survive; null requested options resolve to empty descriptor `opt`. |
| [FIELD timing](expected/extras/extras-raw-field-time.tsv) | Java accepts it; diagnostic descriptor adds EventTime/full56 and EventSequence/full72 before regular fields. |
| [Original Quote](expected/extras/quote-raw.tsv) / [original Profile](expected/extras/profile-raw.tsv) | Original schemas, IDs 0/5, 8/16 logical fields; unchanged raw extremes survive production FileReader. |
| [Original event API/delegates](expected/events/observations.tsv) | Fresh unchanged QDCXX-025 `default` execution: all complete observation lines match the historical corpus and all six `.qtp` bytes match. |
| [Unsupported full identities](expected/extras/unsupported-full-identities.tsv) | Java accepts full24/full137. This is a retained negative admission witness for the existing exact-ID C++ subset. |
| [Failures and empty writer](expected/extras/failures.tsv) | Null symbol/cipher0 is rejected with IllegalArgumentException; `.time` data extension with TEXT time is rejected with InvalidFormatException; an empty closed writer creates no tape. |

`*-wire.tsv` record actual output envelopes and descriptions, but are diagnostic scans. The event
oracle's two `*-wire-omitted.qtp` remain constructed diagnostic inputs, not FileWriter output.
Production parser acceptance is established separately by FileReader observations. RAW's callbacks
10/15/20 are three projections of one wire record; comparison uses STREAM_DATA (15).

The historical text-only comparison explicitly permits that checkout's CRLF versus fresh LF;
the complete lines must match. This exception is confined to the old event observations. The new
corpus, including its event observations, always uses exact byte comparison between captures/Verify.
No private input, compression, endpoint, transport or new event/field adapter is included.
