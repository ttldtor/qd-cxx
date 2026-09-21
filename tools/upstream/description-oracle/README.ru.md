# Oracle descriptions

[English version](README.md). Английский документ является основным.

QDCXX-013 добавляет отдельный ограниченный driver поверх собранного из исходников QD 3.355. Старые
Java harnesses, launchers, эталоны и captures неизменны. [Покрытие](../../../tests/fixtures/protocol/p08/COVERAGE.md)
и [валидация](../../../tests/fixtures/protocol/p08/VALIDATION.md) определяют измеренный объём.

Запускайте последовательно на общем закреплённом checkout; oracle использует JDK 8, formatter — JDK 21+.
Пути Upstream, JavaHome и Maven берутся как у [protocol launcher](../protocol-oracle/README.md).
`Invoke-Oracle.ps1 -Mode Capture -Upstream ... -JavaHome ... -Maven ... -RunDirectory <new-directory>`
выполняет независимую чистую reactor build. Повторите для второго capture, сравните normalized outputs,
проверьте их, затем явно перенесите descriptions.tsv в эталоны. Capture эталоны не записывает.
`Test-Verify.ps1 -Upstream ... -JavaHome ... -Maven ... -TestDirectory <new-directory>` меняет только
копию типа сырого descriptor с 8 на 9, требует точного mismatch, затем проверяет оригинал и оба хеша.
`Invoke-Oracle.ps1 -Mode Verify` также принимает отдельный ExpectedDirectory. Логи запускайте через tools/run_utc.py.

Driver читает [inputs.tsv](../../../tests/fixtures/protocol/p08/inputs.tsv): 81 payload,
максимум 256 байт, до 16 полей и sender ID до 1024 в ограниченном диагностическом профиле.
Входы — проверенные доверенные probes, а не публичный parser произвольных вредоносных counts/lengths.
Реальный parser использует пустую DefaultScheme/PentaCodec и maxMessageSize=1024; для каждого входа
новый parser, возвращающие error handlers MessageConsumerAdapter, без событий, metadata, overrides,
transport и EOF signal. Каждый payload заново обрамляется Java ByteArrayOutput, включая строгие
префиксы payload. Старые положительные входы вырезаны по захваченным Java frame offsets;
исторические эталоны не переписываются.

Отдельный диагностический обход вызывает реальные readCompactInt/readUTFString и выполняет сверенные
с исходником структурные проверки в порядке parseDescribeRecords. Он фиксирует позиции primitives,
этап, причину и завершённые сырые descriptions по порядку. Это инструментализация, а не список callbacks
реального parser. remapRecord вызывает super и фиксирует успешно установленные ID; reflection читает
recordMap и names/types/nDesc после разбора. Так различаются исключение VOID, отказ построения,
замена и сохранение предыдущего mapping. Upstream не патчится и не копируется. Новый driver не вызывает
фабрики локальной схемы; отказы локальных BYTE/SHORT остаются историческими доказательствами S15.
Java diagnostics сохранены в stderr.

Выход содержит 14 столбцов: `case, id, payloadHex, completeFrameHex, diagnosticStatus, stage,
consumed, errorOffset, rawDescriptions, primitiveExceptionClass, installedIds, constructedMap,
parserErrors, frameInputPosition`. Дефис означает отсутствие/пустоту; descriptors имеют вид
`id/UTF16hex/name:type,...`, записи разделяются точкой с запятой. Имена — четыре hex цифры на UTF-16 unit.
Последняя строка — `complete<TAB>descriptions`. Нормализация меняет только CRLF на LF; текст exceptions
не является C++ контрактом. Metadata содержит текущие хеши исходников/входов/environment,
classpath outputs, зависимости и команды.
