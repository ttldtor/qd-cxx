# Offline-фикстуры отдельных producer

[English version](README.md). Английский текст является определяющим.

[Генератор](../../../../../tools/upstream/real-tape-corpus/README.ru.md) использует отдельно
закреплённые Java QD baseline/current producers. В каждом каталоге producer находятся
`expected/` с семью неизменёнными полученными артефактами и `provenance.json` с переносимыми
хешами исходников, runtime, classpath и артефактов. Фактические пути запуска и raw-логи остаются
приватными как полные текстовые UTC-логи. `corpus.tsv` описывает файлы этих producer; родительский общий corpus inventory не
меняется.

Launcher producer квалифицирован для Windows с явным нативным UTC runner. Его логи объединяют
декодированные потоки Java с timestamps; отдельные сырые потоки Java не заявлены. Переносимые
артефакты не получают зависимости от Windows runtime. Потребление фикстур нативным Linux —
отдельная интеграционная проверка; producer в Linux/macOS и POSIX-очистка потомков не квалифицированы.

| Артефакт | Происхождение и ожидаемый результат |
| --- | --- |
| `compact-none.qtp` / `.tsv` | Production BINARY/NONE/EMPTY_SET/STREAM_DATA, время NONE; три compact integer записи, точные значения и cipher/raw символы |
| `quote-none.qtp` / `.tsv` | Production writer и полные стандартные Quote delegate/scheme, время NONE; три точные записи |
| `quote-message.qtp` / `.tsv` | Тот же полный стандартный Quote, фиксированное время MESSAGE; три точные записи и event-time sequence |
| `quote-truncated.qtp` | Получен из `quote-message.qtp` удалением двух последних байтов; отказ production reader и префикс из двух записей, без утверждения о завершении |

TSV использует UTF-8/LF. Строки record содержат ordinal, event-time sequence, record/symbol в
UTF-16 hex, event flags и каждый локальный integer slot как `UTF16-field-name:full-type:raw-int`.
Широкие значения включают фактические локальные void-tail slots; это локальные наблюдения, а не
дополнительные wire-поля. Для `complete` нужны точные ожидаемые записи, один descriptor, ноль
parser errors и равенство обработанных байтов физическому размеру файла. Object-полей в выбранных
сценариях нет. Символы и байты не получены из приватных рыночных записей.

`provenance.json` фиксирует точную QD identity, хеши исходников с нормализацией LF и фактических
байтов, наблюдения JDK/Maven, хеши executable/configuration, исходников/helper, содержимого
classpath, инвентаризацию зависимостей сборки и хеши артефактов. Classpath digest — хеш
отсортированных строк `relative-path<TAB>sha256`, объединённых LF без конечного LF. Runtime
metadata приватно сохраняет хеш каждого файла classpath. Сборки используют class-файлы, поэтому
descriptor tape сообщает `QDS-UNKNOWN`; provenance устанавливают выбранный commit и
контролируемая сборка, а не эта строка.

Capture не меняет эталоны. Продвижение требует проверки свежего приватного capture, копирования
точного набора артефактов сюда, создания переносимого provenance и свежего Verify для обоих
producer pins. Отрицательные проверки отклоняют пять изменённых наборов и разбирают производную
обрезанную запись production FileReader. [Валидация](../../../../conformance/protocol/real-tape/producer/VALIDATION.ru.md)
фиксирует выполненный объём. Эти файлы подтверждают только именованные Java production-сценарии;
поддержка произвольного QD input и равенство декодирования C++ из них не следуют. Необязательное
получение live demo не реализовано и не запускалось.
