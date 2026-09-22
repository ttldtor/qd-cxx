# Покрытие Java для RAW tape

[English version](COVERAGE.md). Английская версия является нормативной.

94 ограниченных случая наблюдают закреплённый производственный FileReader QD; шесть исходных
файлов произведены FileWriter либо реальным путём TapeConnector. Остальные 88 явно помечены
как диагностические wire-входы и не выдаются за неизменённый вывод производителя. Для каждого
сохранены вход, полный TSV callback/значений/времени/ошибок и дополнительный TSV конвертов:
282 файла плюс настройки и независимые значения упаковки, всего 284. Частных байтов, символов
или значений здесь нет.

| Случаи | Измеренная область |
| --- | --- |
| raw-quote, raw-synthetic, raw-hs, stream-synthetic | FileWriter BINARY/NONE compression/MESSAGE, точные Quote либо S/R/E, нулевые flags; реальные RAW и STREAM |
| connector-quote, connector-default | DXEndpoint publisher → TapeConnector → FileWriterHandler, явные пустые либо стандартные hs; неизменённый полный Quote delegate и заданный eventTime |
| opt-* | Absent, empty, null, hs, unknown и hs,unknown в дескрипторе, действующие options вместе с реальной RAW-доставкой |
| matrix-* (24) | Время дескриптора absent/NONE/MESSAGE/FIELD × send list empty/RAW/STREAM × реальные RAW/STREAM |
| timing-sequence | Данные до timestamp; ноль; повторное/убывающее время; последовательные heartbeat; состояние через позднее описание и несколько сообщений; EOF после heartbeat |
| boundary-* (14), packing.tsv | Границы миллисекунд, отрицательное floor-деление, насыщение int32 seconds, оба предела int64 и точный packed sequence |
| empty-envelope, explicit-zero-empty, heartbeat-absent | Маркер, явный тип zero без payload и bits 0 различаются с сохранением наличия |
| heartbeat-fields, heartbeat-marks-no-time | Timestamp и все известные optional fields, границы signed-int, поля без timestamp |
| heartbeat-unknown-bit, heartbeat-negative-bits, heartbeat-extension | Неизвестные/отрицательные bits и хвостовое расширение: измерен допуск Java, ограниченная политика отказывает |
| heartbeat-truncated-0..20 | Каждый строгий префикс полного bits-15 heartbeat с каждым compact-примитивом; пустой префикс допустим как empty payload |
| timestamp-truncated-1..9 | Каждый непустой строгий префикс bits-1 и максимального int64 timestamp, включая только content |
| heartbeat-preserves-time | Bits 0 не стирает прежний timestamp/sequence |
| mixed-raw-stream, unsupported-message | Реальная диспетчеризация, Java callback/unknown callback и продолжение доставки |
| nonzero-flags, nonzero-flags-empty-opt | Реальный парсер доставляет flag 1 даже с пустым opt; C++ отказывает до полей записи |

RAW типа 5 размножается на callback 10/15/20; проекция записей для сравнения C++ — callback 15.
Записаны все доставленные raw fields, точные символы/flags и event-time sequences. Строки Quote
также содержат business bid/ask timestamps, exchange codes, точные mapped binary64 bits, mapped
eventTime, sequence и nano part. Исходные входы отделены: replay timestamps 1600000000123 + i*1000;
business times 1700000000000 + i*2000 / ask +1000; sizes 10.5+i / 12.25+i. Схема не упрощена.

Harness не реализует decoder вместо oracle parser. Сканирование конвертов доказывает лишь framing;
дополнительные вызовы `ProtocolOption.parseProtocolOptions` и `TimeSequenceUtil` показывают
публичные upstream-преобразования вместе с реальной доставкой записей. Разрешительная матрица Java
не является bounded acceptance matrix; [RAW-TAPE](../../../../spec/protocol/RAW-TAPE.ru.md)
задаёт более строгую политику и отличия terminal/atomic. Реализация C++, ресурсная политика и CLI
требуют отдельных проверок; отчёт не заявляет их результатов или общей/частной tape-совместимости.
