# Контракт текста событий

[English version](EVENT-TEXT.md). Английская версия является основной.

## Эталон и поля

[ADR-0011](../../docs/decisions/ADR-0011-java-event-text-reference.ru.md) определяет числовой профиль Java 25 и согласованные исправления дефекта кеша. [Provenance](../../tests/fixtures/event-text/provenance.json) указывает закреплённый исходник QD. qdcxx_event_text односторонне зависит от событий, value/source helpers и общего qdcxx_time_zone_data; event values не зависят от Schedule.

Порядок Quote: symbol, eventTime, time, timeNanoPart, sequence, bidTime, bidExchange, bidPrice, bidSize, askTime, askExchange, askPrice, askSize. Profile: symbol, eventTime, description, SSR, status, statusReason, haltStartTime, haltEndTime, highLimitPrice, lowLimitPrice, high52WeekPrice, low52WeekPrice, beta, earningsPerShare, dividendFrequency, exDividendAmount, exDividendDay, shares, freeFloat. Trade/TradeETH: symbol, eventTime, time, timeNanoPart, sequence, exchange, tradeId, price, change, size, day, dayVolume, dayTurnover, direction, ETH.

Числовые поля используют бизнес-accessors и действующие снимки source binary64. Для Profile shares/freeFloat применяется presentation-проекция Java double-to-WideDecimal. Текст не меняет хранимые decimal-биты и не добавляет raw fields. Reserved flags остаются в событии и отдельно выводятся formatEventFlags; бизнес-проекции enum сохраняют семантику существующих accessors.

Собственный C++ symbol не представляет Java null; сравнение передаёт Java ту же ненулевую строку, включая пустую. Nullable строки Profile выводятся с буквальным null в кавычках. Кавычки, controls и отдельные суррогаты буквальны. Exchange units 32..126 буквальны; NUL становится backslash-zero; остальные — backslash-u и четырьмя строчными hex-цифрами.

## Числа, даты и канонические значения

Double-текст сохраняет отрицательный ноль, NaN и signed infinities с записью Java. Обычная запись применяется при старшем exponent -3..6; научная использует E, не добавляет плюс к exponent и содержит дробную цифру. Выбор основан на стандартном кратчайшем scientific to_chars, явном выборе ближайших двух цифр при достаточности одной и раскладке Java. Фактическое Java-сравнение квалифицирует обе обязательные C++ реализации; эквивалентность библиотек не предполагается. Код алгоритма Java runtime и новая числовая зависимость не встроены.

Время использует yyyyMMdd-HHmmss.SSS; halt times Profile не содержат .SSS. Ноль даёт 0. Включительный pre-guard Java: [-62135683200000,253402387200000]; снаружи выводится исходный signed64 integer. Внутри локальная григорианская дата вне годов 1..9999 тоже даёт исходное число. Day IDs используют существующую signed32 DayUtil-совместимую арифметику независимо от timezone.

Для enum, точных nullable source names и беззнакового строчного flag hex есть отдельные overloads. Существующие API CandlePeriod/CandleSymbol/Candle attributes/decimal сохраняют канонический текст и гарантии parser. Общий parser событий и универсальный round trip не добавлены.

## Владение, ошибки и ограниченная стоимость

EventTextLimits обязателен. inputUnits считает суммарные копируемые/просматриваемые units symbol, nullable strings и source names. outputUnits ограничивает полный UTF-16 результат. utf8Bytes ограничивает UTF-8 байты и не используется при выводе только UTF-16. work списывает единицу за просмотренную source unit, единицу за скопированную UTF-16 output unit, 128 за форматируемый scalar integer/double/character/date/WideDecimal, а при кодировании — единицу за UTF-16 unit и каждый UTF-8 byte. Literal/enum text оплачивает копирование output units. Бюджет общий для вызова, внутренних повторов нет. Это логические ограничения, а не квоты CPU-инструкций, времени, allocator capacity или RSS.

Double занимает до 24 units, exchange — до шести. Временные числа ограничены диапазонами binary64/integer; даты используют фиксированные stack buffers. Композиция Profile shares выполняет до восьми сравнений rank и ограниченное преобразование decimal powers. UTF-8 convenience временно удерживает до outputUnits UTF-16 units вместе с utf8Bytes байт; стандартные строки могут резервировать дополнительную allocator capacity. Ошибка выделения даёт ALLOCATION, превышение логической/системной ёмкости строки — RESOURCE_LIMIT. Частичного результата нет.

Контекст владеет неизменяемым binding pin; исходные данные/provider handles можно уничтожить после привязки. UTC/fixed контекст не выделяет binding storage. Views удерживаются лишь во время синхронных вызовов. Независимые const calls реентерабельны и параллельны при неизменных входах; параллельная замена handle требует внешней синхронизации. Application callbacks, ленивые format caches, очереди, reload и изменение настроек процесса отсутствуют.

UTF-16 результат точно сохраняет malformed units. UTF-8 явно отклоняет непарный суррогат либо заменяет каждый на U+FFFD. Правильная пара становится одним стандартным UTF-8 scalar; NUL остаётся нулевым байтом. Ошибка не содержит байтов. Приложение пишет поток после получения полной строки.

Масштабирование Profile shares использует фиксированное произведение significands в двух словах и явное nearest-even округление умножения/сложения. Текст одинаков при всех четырёх поддержанных режимах округления вызывающей стороны; режим округления её потока не меняется. Сохранение флагов исключений плавающей точки этими проверками не установлено.

Порядок Summary: symbol, eventTime, day, dayOpen, dayHigh, dayLow, dayClose, dayCloseType, prevDay, prevDayClose, prevDayCloseType, prevDayVolume, openInterest. Цены/объём используют те же бизнес-accessors binary64; OpenInterest выводится как полное signed64 значение accessor. Production text oracle прямо задаёт этот long, включая 3000000000 и обе signed64 границы, отдельно от исправления ширины delegate в [ADR-0010](../../docs/decisions/ADR-0010-summary-open-interest-width.ru.md).

Фабрика именованного контекста заимствует необязательный указатель TimeZoneData только на время вызова; при успехе контекст владеет ZoneBinding и одним общим держателем. maxIdUnits ограничивает копируемый ID; общее неизменяемое содержимое провайдера подчиняется [контракту часовых поясов](../../docs/TIME-ZONE-DATA.ru.md), а не лимитам вывода. Создание может выделять память; форматирование только читает привязку и копирует скалярное смещение. Опорный момент явно передаётся вызывающей стороной, часы EventText не читает. Фактическое смещение, включая final-raw соглашение провайдера до 1900 года, запрашивается отдельно от опорных метаданных. Недоступность провайдера, неверный ID, диапазон и повреждение данных различаются; подстановки данных ОС нет.