# Capture исходящего production output

[English](CAPTURES.md). Английский текст является нормативным.

QDCXX-031 добавляет 43 артефакта к неизменённым свидетельствам QDCXX-022/023/024/025/030. Закреплён
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` (QD 3.355); проверены origin, HEAD и tag свежего чистого
checkout. Один clean reactor build `qds-file -am` завершился успешно за 1:08. Capture использовал
JDK 8 `1.8.0_422`, UTF-8, English locale и UTC дочернего процесса без scheme overrides.

[Таблица входов](inputs.tsv) сверяется с точными восемью samples harness до создания output.
[Provenance](expected/provenance.tsv) сохраняет фактические hashes tool/input/provider sources и lock;
[classpath receipt](expected/classpath-receipt.json) — четырнадцать фактических деревьев upstream
bytecode/resources. Пути машины, полные журналы и run metadata отделены от deterministic expectations.
Новые артефакты не нормализуются.

Два свежих capture 2026-10-11, 20:54:20–20:54:24 UTC, совпали по всем 43 артефактам. Отдельный
production Verify выполнен 20:54:57–20:55:00 UTC. Hashes сохранённых ожиданий принадлежат этим
фактическим запускам, а не скопированным историческим metadata.

| Свидетельство | Production-наблюдение |
| --- | --- |
| [Custom RAW/MESSAGE](expected/extras/extras-raw-message.tsv) | Восемь записей, ноль parser errors, 24 fanout callbacks; timestamped batch создаёт три RAW data frames. |
| [Custom STREAM/NONE](expected/extras/extras-stream-none.tsv) | Восемь записей, ноль errors, восемь callbacks; один STREAM data frame. |
| [Custom RAW/NONE](expected/extras/extras-raw-none.tsv) | Явные raw-string symbols; восемь записей, 24 callbacks, один RAW frame; event timing отсутствует. |
| [Singleton visits](expected/extras/extras-raw-singletons.tsv) | Те же восемь входов по одному дают восемь RAW frames вместо трёх batch frames. |
| [Regular heartbeat input](expected/extras/extras-heartbeat-regular.tsv) | Non-timestamped buffer использует явное heartbeat time; восемь записей в одном RAW frame. |
| [hs](expected/extras/extras-raw-hs.tsv) / [null requested options](expected/extras/extras-raw-default-options.tsv) | Нулевые flags и точные raw values сохраняются; null requested options разрешаются в пустой descriptor `opt`. |
| [FIELD timing](expected/extras/extras-raw-field-time.tsv) | Java принимает его; диагностический descriptor добавляет EventTime/full56 и EventSequence/full72 перед обычными полями. |
| [Original Quote](expected/extras/quote-raw.tsv) / [original Profile](expected/extras/profile-raw.tsv) | Исходные схемы, ID 0/5, 8/16 logical fields; raw extremes сохраняются через FileReader. |
| [Исходные event API/delegates](expected/events/observations.tsv) | Свежий неизменённый QDCXX-025 `default`: все полные observation lines совпадают с историческим корпусом, все шесть `.qtp` совпадают побайтово. |
| [Unsupported identities](expected/extras/unsupported-full-identities.tsv) | Java принимает full24/full137. Это сохранённое negative admission witness для существующего точного C++ подмножества ID. |
| [Failures и empty writer](expected/extras/failures.tsv) | Null symbol/cipher0 отвергается с IllegalArgumentException; data extension `.time` при TEXT time — с InvalidFormatException; пустой закрытый writer не создаёт tape. |

`*-wire.tsv` фиксируют envelopes и descriptions фактического output, но являются диагностическими
scans. Два `*-wire-omitted.qtp` прежнего oracle событий остаются сконструированными diagnostic inputs.
Production parser acceptance устанавливается отдельно наблюдениями FileReader. RAW callbacks
10/15/20 — три проекции одной wire record; сравнение использует STREAM_DATA (15).

Историческое текстовое сравнение явно допускает CRLF checkout против свежего LF; полные строки
обязаны совпадать. Оговорка касается только прежних event observations. Новый корпус, включая
его event observations, между capture/Verify всегда сравнивается побайтово. Private input,
compression, endpoint, transport и новые event/field adapters не включены.
