# Покрытие свидетельств tape

[English version](COVERAGE.md). Английская версия является нормативной.

QDCXX-019 использует open/write/close закреплённого Java QD FileWriter и production replay FileReader.
[Спецификация/инвентаризация](../../../../spec/protocol/TAPE-FILES.ru.md),
[инструменты](../../../../tools/upstream/tape-oracle/README.ru.md), [валидация](VALIDATION.ru.md).
Это исследовательские свидетельства, а не C++ tape conformance или обещание общей поддержки форматов.

## Входы и случаи

[inputs.tsv](inputs.tsv) содержит семь записей в трёх сообщениях с явными временами входного heartbeat,
точными UTF-16 символами и нулевыми флагами. Отправитель: R=0(x,y,z), S=1(v), E=2(); локально:
S=0(v), R=1(y,x,z), E=2(), везде обычный type 8, hasTime=false. Harness проверяет локальные
идентификаторы и перестановку R, оба предела int32, повтор ABC/U+2603, пустой символ и E без полей.

| Случай | Исходный файл | Production replay | Диагностический структурный scan | Текущий инспектор |
| --- | --- | --- | --- | --- |
| default | [BINARY, 165 байт](expected/default.qtp), без setter-вызовов writer | [7 записей, 3 callbacks](expected/default.tsv), три времени heartbeat, нулевые флаги | [11 конвертов](expected/default-frames.tsv), заголовок type 1, поздние описания, время type 0 и пустой footer | [stdout](expected/default-inspect.stdout), [stderr](expected/default-inspect.stderr), [exit 4](expected/default-inspect.exit) |
| constrained | [BINARY, 137 байт](expected/constrained.qtp), явные BINARY/сжатие NONE/время NONE/пустой opt/STREAM_DATA | [7 записей, 1 callback](expected/constrained.tsv), time sequence 0 | [8 конвертов](expected/constrained-frames.tsv), заголовок и поздние описания остаются | [stdout](expected/constrained-inspect.stdout), [stderr](expected/constrained-inspect.stderr), [exit 4](expected/constrained-inspect.exit) |
| text | [TEXT, 432 байта](expected/text.txt), только setter формата | [7 записей, 1 callback](expected/text.tsv), те же значения/символы/time sequences, что у default; callback пустого heartbeat | Java-сканер конвертов не применяется | [stdout](expected/text-inspect.stdout), [stderr](expected/text-inspect.stderr), [exit 4](expected/text-inspect.exit), начало текста трактуется как type 61 |
| truncated | [Производный файл, 135 байт](expected/truncated.qtp), из constrained удалены последние два байта (последний байт данных + footer) | [4 записи, 1 callback, 1 stream error](expected/truncated.tsv); reader возвращается, в логе 13 оставшихся байт | Производный повреждённый вход, не случай успешного writer | [stdout](expected/truncated-inspect.stdout), [stderr](expected/truncated-inspect.stderr), [exit 4](expected/truncated-inspect.exit) на заголовке до обрезания |

Полные бинарные результаты не исправлялись для инспектора. Все отказы инспектора — ожидаемые
unsupported type exits, а не успешные семантические сравнения. Сканер конвертов диагностический;
корректность записей проверена настоящими callbacks FileReader. Пробелы, известные лишь по исходникам,
явно выделены в спецификации. В четырёх replay 25 наблюдений доставленных записей; это число
не является числом уникальных входных записей.

## Грамматика наблюдений

TSV наблюдений использует LF и непустые ячейки. Hex-строки — UTF-16 units в нижнем регистре,
четыре цифры на единицу; `-` означает пустую последовательность, отсутствие значения или null endpoint
согласно ячейке. Десятичные целые без ведущих нулей, знака плюс и отрицательного нуля.
Нормализации Unicode нет.

| Строка | Порядок ячеек, включая тег строки | Арность |
| --- | --- | --- |
| profile | profile, случай, single-file, MAX_SPEED, auto | 5 |
| descriptor | descriptor, ID отправляемых типов или -, ID принимаемых типов или -, units endpoint или - | 4 |
| property | property, units ключа, units значения | 3 |
| heartbeat | heartbeat, время в мс или -, boolean isEmpty | 3 |
| record | record, индекс, индекс callback, тип сообщения, локальный ID записи, units локального имени, units символа, список int32 в локальном порядке или -, флаги, event-time sequence | 10 |
| error | error, stream/message/unknown, тип сообщения или - | 3 |
| end | end, число записей, число data callbacks, число ошибок, returned | 5 |
| complete | complete, случай | 2 |
| frame | frame, индекс, начало, исключающая конечная позиция, длина конверта, тип (-1 для пустого heartbeat) | 6 |
| complete (конверты) | complete, envelopes, число кадров, всего байт | 4 |

Verifier закрытого профиля принимает только наблюдённую stream error, один дескриптор, ожидаемые
ключи/число свойств, форму времени/heartbeat, точную арность локальных полей, порядок и счётчики
complete/end, известную последовательность конвертов и диагностику инспектора. Точное побайтовое
сравнение проверяет остальные значения против raw-ожиданий. Отсутствие или пустота наблюдений
не считается успехом. TSV инспектора имеет собственную неизменённую
[грамматику](../../../../spec/protocol/OFFLINE-INSPECTION.ru.md).

## Ограничения и исключения

Нет выполнения CSV/BLOB/сжатия/sidecar, replay коннектора, ненулевых флагов, произвольных метаданных,
динамических переопределений, converter/importer, follow, resync, проб предельных ресурсов или расширения C++.
Версия runtime — QDS-UNKNOWN из классов; provenance настоящего commit/JDK/classpath хранится отдельно.
Не добавлены production-зависимости либо Python-скрипты/зависимости. Существующий UTC logger использован
только для свидетельств build/format; capture эталонов и Verify запускаются через Java/PowerShell.
