# Покрытие примеров контракта QDCXX-008

[English version](COVERAGE.md). Английский документ является основным.

Эти 17 файлов JSON — спроектированные проекции неизменных TSV QDCXX-005, а не JSON, произведённый Java, новые oracle captures или результат importer. На каждый подслучай дан отдельный пример без дедупликации, чтобы явно сохранить порядок provenance входов и попытки отказов. Их значение определяет [контракт](../JSON-CONTRACT.ru.md).

| Подслучай | Пример | Существующий expectation | Проекция |
| --- | --- | --- | --- |
| S01-empty | [JSON](S01-empty.json) | [TSV](../../../tests/fixtures/schema/expected/S01-empty.tsv) | Успех: нет записей; один пустой вход XML. |
| S01-no-urls | [JSON](S01-no-urls.json) | [TSV](../../../tests/fixtures/schema/expected/S01-no-urls.tsv) | Отказ: две диагностики no-source-inputs, разные попытки загрузчиков. |
| S04-order-disabled | [JSON](S04-order-disabled.json) | [TSV](../../../tests/fixtures/schema/expected/S04-order-disabled.tsv) | Успех: Alpha=0, Beta=1; disabled entities сохранены через ссылки на исходники; порядок объявлений отличается от порядка категорий Java. |
| S05-both | [JSON](S05-both.json) | [TSV](../../../tests/fixtures/schema/expected/S05-both.tsv) | Успех: heads First/Second в int 0/1, Ordinary в 2. |
| S05-first | [JSON](S05-first.json) | [TSV](../../../tests/fixtures/schema/expected/S05-first.tsv) | Успех: First в 0, placeholder в 1; Second остаётся обычным полем. |
| S05-second | [JSON](S05-second.json) | [TSV](../../../tests/fixtures/schema/expected/S05-second.tsv) | Успех: placeholder в 0, Second в 1; First остаётся обычным полем. |
| S05-disabled-first | [JSON](S05-disabled-first.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-first.tsv) | Успех: disabled First исключён; placeholder в 0. |
| S05-disabled-second | [JSON](S05-disabled-second.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-second.tsv) | Успех: disabled Second исключён; placeholder в 1. |
| S05-disabled-both | [JSON](S05-disabled-both.json) | [TSV](../../../tests/fixtures/schema/expected/S05-disabled-both.tsv) | Отказ: all-index-fields-disabled после успешного распознавания модели. |
| S07-explicit | [JSON](S07-explicit.json) | [TSV](../../../tests/fixtures/schema/expected/S07-explicit.tsv) | Успех: effective name Chosen.Name; сохранены property Value и record lookup ChosenName. |
| S07-fallback | [JSON](S07-fallback.json) | [TSV](../../../tests/fixtures/schema/expected/S07-fallback.tsv) | Успех: fallback First.Name; Plain использует semantic name. |
| S08-before | [JSON](S08-before.json) | [TSV](../../../tests/fixtures/schema/expected/S08-before.tsv) | Успех: base, copy, overlay; Copy имеет только Original. |
| S08-after | [JSON](S08-after.json) | [TSV](../../../tests/fixtures/schema/expected/S08-after.tsv) | Успех: base, overlay, copy; Copy имеет Original и Added. |
| S08-main-alias | [JSON](S08-main-alias.json) | [TSV](../../../tests/fixtures/schema/expected/S08-main-alias.tsv) | Успех: Base использует Chosen.Name, Copy — First.Name; explicit-main declaration сохранено ссылками на исходник/доказательство. |
| S15-slots | [JSON](S15-slots.json) | [TSV](../../../tests/fixtures/schema/expected/S15-slots.tsv) | Успех: шесть логических полей, шесть int slots и два obj slots; LONG/WIDE_DECIMAL сохраняют принадлежащие им VOID tails. |
| S15-byte | [JSON](S15-byte.json) | [TSV](../../../tests/fixtures/schema/expected/S15-byte.tsv) | Отказ: распознан BYTE=1; утверждённая диагностика unsupported-local-type на стадии локального построения. |
| S15-short | [JSON](S15-short.json) | [TSV](../../../tests/fixtures/schema/expected/S15-short.tsv) | Отказ: распознан SHORT=3; утверждённая диагностика unsupported-local-type на стадии локального построения. |

Итог: 13 успешных схем и четыре случая отказа (пять записей диагностики, поскольку no-URLs содержит две попытки). Все примеры ссылаются на одни и те же исходные входы/доказательства в обеих языковых версиях. Scoped lookup probes включают найденные и отсутствующие ключи; исходные метаданные сохранены ссылками, без дублирования.

Покрыты только существующие подслучаи, а не целые S-семейства, provider parity, wire conformance или общая семантика XML. Примеры protocol/session не добавлены: [доказательства P05](../../protocol/CAPTURES.ru.md) ограничивают разделение requests и selection, а не тело локальной схемы. Правила LONG/time-index wire остаются выводами по исходникам и требуют последующих probes. См. [выполненную валидацию и ограничения](VALIDATION.ru.md).
