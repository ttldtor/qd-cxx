# Проверка production-свидетельств исходящего output

[English](VALIDATION.md). Английский текст является нормативным.

Локальная проверка QDCXX-031 2026-10-11 использовала неизменённый QD 3.355,
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK 8 `1.8.0_422`, CMake и PowerShell 7.4+.
Отчёт касается нового Java tooling/corpus; C++ writer здесь не реализован и не проверен.

| Выполненная проверка | Фактический результат |
| --- | --- |
| Origin/HEAD/tag/clean свежего checkout; clean reactor build `qds-file -am` | PASS; build сообщил 1:08, сохранены четырнадцать фактических classpath/resource trees. |
| Pinned google-java-format 1.36.1, formatter JDK 23.0.2, Format затем Check | PASS; проверены 25 Java sources репозитория. |
| Два новых Capture, 20:54:20–20:54:24 UTC | PASS; одиннадцать production cases, по восемь записей, ноль parser errors. |
| Строгое сравнение capture | PASS; все 43 артефакта совпали побайтово, включая provenance/receipt и ordered observations. |
| Свежий production Verify, 20:54:57–20:55:00 UTC | PASS; все 43 артефакта совпали с сохранёнными ожиданиями; Verify их не переписывал. |
| Byte/type/order/completion/provenance/file-set mutants | PASS; все шесть отвергнуты, затем успешно проверен неизменённый baseline; order переставляет допустимые строки и требует ошибку порядка. |
| Лишние скрытые файл/каталог | PASS; оба отвергнуты проверкой точного набора, включая скрытые элементы. |
| Недопустимый full type в обоих comparison peers | PASS; отвергнут typed grammar независимо от равенства peers. |
| Неизменённые real Quote/Profile default witnesses | PASS; шесть старых `.qtp` совпали побайтово, все полные observation lines совпали. |
| Повторённые production Verify, comparator, Java Check и mutation suite | PASS; девять mutants и неизменённый baseline; после исправлений проверок изменён только актуальный tool provenance, остальные 42 артефакта сохранены. |
| Прежний CTest baseline Windows MSVC Release | 44/45 прошли внутри sandbox; создание alias control форматтера запрещено. Отдельный повтор вне sandbox прошёл 1/1. Этот baseline не проверяет новый C++ writer. |

Source/provider/input hashes и bytecode receipt опубликованы в
[сохранённых ожиданиях](../../../fixtures/protocol/tape-write/expected/provenance.tsv).
[Capture](../../../fixtures/protocol/tape-write/CAPTURES.ru.md) и
[покрытие/исходники](../../../fixtures/protocol/tape-write/COVERAGE.ru.md) объясняют свидетельства.
[Launcher](../../../../tools/upstream/tape-write-oracle/README.ru.md) сохраняет полные raw logs и
run metadata в явно переданном run directory.

CRLF старого текстового корпуса против
свежего LF допускается только при сравнении полных исторических observation lines. Новые артефакты
сохраняют точные bytes. Diagnostic scans и constructed omission inputs не считаются production
parser acceptance. Disk failure injection, private tape, transport, общие durability/publication
и будущая C++ writing/rewriting support здесь не доказаны.
