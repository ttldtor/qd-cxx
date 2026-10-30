# Покрытие конечного каталога бинарных tape

[English version](COVERAGE.md). Английская версия нормативна.

[Матрица поддержки](../../../fixtures/protocol/real-tape/support-matrix.tsv)
содержит 71 строку. Каждая строка задаёт обе точные Java-версии, выбранный
контракт, положительные/отрицательные требования и фактические доказательства.
`not-applicable` означает явное исключение из профиля, а не реализованную
поддержку. Одной характеристики исходников недостаточно для runtime-проверки.

| Измерение | Строк | Ограниченное утверждение и доказательства |
| --- | ---: | --- |
| Поле | 21 | 19 конечных ID; reserved-альтернативы отвергаются. [Скалярные](../field-types/SCALAR-VALIDATION.ru.md), [объектные](../field-types/OBJECT-VALIDATION.ru.md) и [общие проверки типов](../field-types/VALIDATION.ru.md). |
| Схема | 8 | Явные indexed/time layout, снимки и конечное состояние схемы. [Indexed-проверки](../field-types/INDEXED-VALIDATION.ru.md). |
| Символ | 2 | Владеющее точное состояние символа, конечные repeat/default. [Проверки flags](../field-types/FLAGS-VALIDATION.ru.md). |
| Flag | 8 | Поддержанные raw flags и removal metadata; неизвестные биты отвергаются до публикации. [Проверки flags](../field-types/FLAGS-VALIDATION.ru.md). |
| Сообщение | 14 | Конечный raw dispatch и явные политики unsupported для control/family. [Dispatch-проверки](../field-types/DISPATCH-VALIDATION.ru.md); container markers выбирают отдельный слой оболочки. |
| Descriptor | 3 | Точные конечные свойства, extensions и успешные delta. [Dispatch-проверки](../field-types/DISPATCH-VALIDATION.ru.md). |
| Option | 3 | Явная грамматика options и неподдержанные альтернативы. [Dispatch-проверки](../field-types/DISPATCH-VALIDATION.ru.md). |
| Формат | 5 | Только BINARY; остальные форматы исключены. [Контракт каталога](../../../../spec/protocol/BINARY-TAPE-COVERAGE.ru.md). |
| Время | 4 | Явные NONE/MESSAGE/FIELD/LONG/TEXT; [FIELD-проверки](../field-types/FIELD-TIME-VALIDATION.ru.md), [контракт sidecar](../../../../spec/protocol/SIDECAR-TIME.ru.md), [проверки источника](VALIDATION.ru.md). |
| Container | 2 | Явный несжатый либо optional конечный GZIP/ZIP; [контракт контейнера](../../../../spec/protocol/TAPE-CONTAINERS.ru.md). |
| Источник | 1 | Конечные самодостаточные сегменты и согласованное закрытие; [контракт последовательности](../../../../spec/protocol/TAPE-SEQUENCE.ru.md). |

Окончательная матрица содержит 57 строк `implemented-bounded` и 14
`not-applicable`; pending не осталось. В прежних снимках четыре строки ожидали
проверки. Они закрыты только в следующих явных профилях:

| Строки | Положительные доказательства | Граница отказов и ошибок |
| --- | --- | --- |
| M75 / C1 | Production FileWriter/FileReader envelope observations и окончательные native `compressed_input`, `source_sequence_io`, `source_sequence_cli`; равенство логических записей при явных NONE/GZIP/ZIP. | Физические container markers не являются внутренними QTP message families. GZIP проверяет все concatenated members; ZIP32 допускает один nondirectory file и конечные пустые директории. CRC/trailer/directory/trailing-byte ошибки запрещают завершение. Произвольный ZIP, ZIP64/encryption/multiple files и недоступное сжатие отвергаются. |
| TIME3 | Production LONG/TEXT observations, native `sidecar_scanner`, `sidecar_policy`, `sidecar_paired_owner` и окончательные source sequence checks сохраняют точные позиции достигнутых отметок, raw timestamp и эффективное время. | Нужен явный companion; неявный поиск и relative/ambient-clock time отсутствуют. Missing/late malformed/read/close ошибки не подтверждают завершение. Логические позиции относятся к распакованным данным, не к физическим сжатым байтам. |
| CHAIN | Production split-file observations и окончательные native admission/I/O/CLI/three-million-record sequence checks сохраняют явный порядок вхождений, состояние схемы сегментов и source-local/aggregate counters. | Только самодостаточные сегменты и однородное время; совпадающие совместимые обновления descriptor поддержаны. Missing pieces, недоступный decoder, parser/scanner/trailer/close/flush/cancellation ошибки сохраняют конечный завершённый префикс и запрещают итоговый complete. Live follow/cycle и неявный globbing отсутствуют. |

[Отчёт о проверке](VALIDATION.ru.md) фиксирует окончательные наборы 74/74 на
Windows и Linux ASan/UBSan, шесть production-only profiles, 120 проверок Java
workflow на Windows и точные hash артефакта/помощников. Исторические частичные
отчёты сохраняют ограничения своих снимков; их hash не переписываются.

Для TIME3 нужны достигнутые companion position/time marks, отдельное сырое и
эффективное время, окончательный companion EOF и поздние malformed/read/close
ошибки. Для M75/C1 нужны равенство записей после распаковки, политика
member/entry, CRC/trailer/trailing bytes, exact/excess физических и логических
бюджетов и отказ compression-OFF без получения зависимостей. CHAIN требует
порядка источников, сброса схемы между сегментами, однородных деклараций времени,
одного завершения после всех закрытий и отказов missing/late сегментов. Каждая
строка сохраняет явные unsupported-политики для валидного Java-поведения вне
выбранного конечного профиля.

Ordinary-сравнение полного файла из [проверки](VALIDATION.ru.md) охватывает каждое
значение, поколение, source binding и порядок записей именованного корпуса.
Новый digest времени MESSAGE не доказывает FIELD, sidecar или compressed parity
для других входов. Источником воспроизводимых доказательств для этих измерений
остаются публичные production fixtures; приватные записи и сырые логи не
публикуются.
