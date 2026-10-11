# Production-свидетельства исходящей tape

[English](README.md). Английский текст является нормативным.

Этот конечный добавочный корпус наблюдает неизменённый закреплённый QD через `FileWriter`, затем
`FileReader`. Он добавляет исходящие property names и ID, физический и wire layout, точные raw values,
nullable type-10 `String`/`char[]`, представления символов и явные варианты timing/mode. Неизменённый
[oracle событий](../event-mapping-oracle/EventMappingOracle.java) предоставляет `default`-свидетельства
делегатов Quote/Profile. Прежние исходники, capture и expectations не изменяются.

Oracle использует JDK 8; formatter — JDK 21+. Нужны CMake 3.25+ и PowerShell 7.4+. Передайте явно
чистый checkout из [upstream.lock](../../../upstream.lock), executable Maven и cache зависимостей.
Preparation выполняет clean reactor build закреплённого исходника; capture проверяет подготовленную
classpath receipt по фактическим байтам provider до и после запуска. Все run directories должны
быть новыми. Capture не использует endpoint, private tape или сетевой транспорт.

Точные команды с явно заданными путями находятся в [English README](README.md). Capture проверяет
структуру, второй capture — детерминированность байтов. Verify сравнивает свежий production output
с сохранённым корпусом из 43 артефактов и не записывает expectations. Comparator проверяет точный
набор файлов/каталогов, запрещает символические пути, ограничивает размеры, проверяет типизированные
числа/UTF-16, порядок raw values и символов, нулевые flags, production counts, completion и актуальные
tool/input hashes. Каждый новый артефакт сравнивается побайтово.

Только исторический `default/observations.tsv` сравнивается по полным строкам: автоматическое
преобразование текста в существующем checkout использует CRLF, свежий Java output — LF. Шесть старых
`.qtp` сравниваются побайтово. Эта оговорка не нормализует новый корпус.

Каждый Java запуск ограничен 30 секундами и heap 256 MiB; compilation/comparison тоже ограничены
timeout. Дочерние negative comparisons имеют предел 30 секунд, освобождают process handles и при
timeout останавливают только собственные process trees. FileReader закрывается в `finally`, writer
закрывается до replay; cursor рассматривается внутри callback и не сохраняется после него.

`extras/*-wire.tsv` — диагностические envelope/descriptor scans, а не parser acceptance. Прежние
`*-wire-omitted.qtp` oracle событий — сконструированные диагностические omissions. Production
acceptance устанавливается FileReader callbacks и явными проверками count/zero errors. Принятие
Java полных identities 24/137, FIELD/NONE timing или STREAM output не допускает их в будущий
RAW/MESSAGE writer. Java close может журналировать I/O failures; checked publication и durability
здесь не доказаны.

См. [capture](../../../tests/fixtures/protocol/tape-write/CAPTURES.ru.md),
[покрытие](../../../tests/fixtures/protocol/tape-write/COVERAGE.ru.md) и
[проверки](../../../tests/conformance/protocol/tape-write/VALIDATION.ru.md).
