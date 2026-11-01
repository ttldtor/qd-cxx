# Production captures контейнеров

[English version](CAPTURES.md). Английская версия является нормативной.

[Сопровождаемый oracle](../../../../tools/upstream/tape-container-oracle/README.ru.md) выполнил Capture,
отдельный повторный Capture и Verify с первым capture для каждого production pin 2026-10-30,
19:54:40–19:55:32 UTC. Это повторные captures, не независимое ревью кода.
Исходный [locked reference](../../../../upstream.lock) не изменён.

| Producer | Commit | Runtime/classpath |
| --- | --- | --- |
| Java QD 3.355 | `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` | Zulu OpenJDK 1.8.0_422-b05, четырнадцать существующих production roots |
| Отдельная Java QD 3.356 | `f71e4cb390d1586f2d4e3a83d9b9067e3326ef19` | Тот же JDK, отдельные четырнадцать production roots |

Каждый запуск сравнивал все 1317 classpath file entries каждого pin (1290 `.class` files и 27 resources)
до/после: изменений файлов нет. Maven и upstream
source build/write не выполнялись. Helper/launcher заморожены после Java Format/Check и PowerShell
syntax/help checks. Их фактические hashes, JDK hashes и безопасные capture intervals находятся
в [baseline provenance](baseline/provenance.json) и [current provenance](current/provenance.json).
Полные локальные command/stream receipts сохраняются отдельно от relative provenance summaries.

[manifest.json](manifest.json) идентифицирует точные production NONE/GZIP/ZIP bytes. ZIP timestamps
сохранены. Canonical [baseline](baseline/production.tsv) и [current](current/production.tsv) outcomes
разделяют FileReader callbacks и direct production StreamCompression results. Helper проверяет
ожидаемые direct byte counts/failure classes, Verify сравнивает canonical bytes в точности. Physical
ZIP hashes при regeneration могут отличаться из-за metadata timestamps.

Valid NONE/GZIP/ZIP дают по одной STREAM record; два gzip members дают два ordinary-file callbacks.
Усечённый header не даёт record. CRC/trailer damage может выдать complete record перед logged
I/O failure; parser-error count FileReader остаётся нулевым. ZIP читает лишь первый файл, обычный
buffered gzip объединяет members. Одно-байтовый direct gzip read при available=0 может остановиться
после первого member. Java принимает trailing bytes и усечённый ZIP EOCD. Эти наблюдения не
подтверждают успешное строгое container closure.

Primary source paths каждого pin: `dxlib/.../StreamCompression.java` и
`qds-file/.../FileReader.java`/`FileWriterImpl.java`. Например,
[locked StreamCompression](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/io/StreamCompression.java)
определяет first-file ZIP wrapper. [Профиль C++](../../../../spec/protocol/TAPE-CONTAINERS.ru.md)
намеренно проверяет весь конечный контейнер и отвергает unsupported/malformed closure.
