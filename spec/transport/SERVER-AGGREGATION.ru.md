# Запрошенная серверная агрегация

[English](SERVER-AGGREGATION.md) · [Feed API](../../docs/FEED-API.ru.md)

Endpoint разделяет текущий optional local request, локальную revision и optional peer min/max
текущего source. Отсутствующий request просит server default; ноль просит отсутствие агрегации.
Публичный setter принимает целые миллисекунды 0..86400000 включительно. Это фактически проверенная
через DXEndpoint публичная QDEndpoint/ChannelShaper validation, а не выведенная из scheduler clamp
граница. Неверный request отклоняется до изменения revision или native output.

Начальное typed значение — EndpointOptions.requestedAggregationPeriod. Канонический ключ
dxfeed.requestedAggregationPeriod принимает существующую QD TimePeriod grammar до 24 часов.
Inline connector alias requestedAggregationPeriod сравнивается без ASCII-регистра, как в Java.
Пустой inline alias означает reset к отсутствующему значению. Пустой канонический endpoint property
неверен, как TimePeriod.valueOf. Приоритет прежний: typed fallback, явные defaults, overrides,
затем упорядоченные inline connector properties. Construction не активирует соединение.

EndpointConfiguration остаётся immutable. После явного изменения setter
Endpoint::getEffectiveConfiguration копирует исходный snapshot и заменяет только requested-
aggregation row текущим локальным намерением, origin RUNTIME_UPDATE и requestedAggregationRevision.
Все остальные properties сохраняют исходные значения и timing. Peer min/max не попадает в
локальные properties. Равное значение не увеличивает revision; exhaustion отклоняется атомарно.

## Существующий wire и timing

У RECEIVE DATA текущего contract свойство requestedAggregationPeriod содержит канонический
TimePeriod text либо явно пустую строку для reset. Initial/fresh-attempt descriptor несёт текущее
значение; online update создаёт property-only DESCRIBE_PROTOCOL delta.
TICKER_DATA=10 и STREAM_DATA=15 используют одно свойство. Общий recovery helper также обрабатывает
уже настроенный HISTORY_DATA=20 без нового high-level delivery mode. Новых wire ID, ACK и версии нет.

Setter коммитит owned intent под endpoint mutex и будит существующий runtime. Он не стартует worker
и не переподключает endpoint. Pending updates сворачиваются в одно последнее fixed-size значение
и делят единственный существующий bounded control-output slot. Уже допущенный frame сохраняет
обычное владение до завершения; поэтому в полёте возможны промежуточные requests. Fresh attempt
отправляет последнее намерение. До connect, после disconnect и после close значение доступно и
изменяемо без native activity. Это соответствует Java local-intent setter после close.

Peer SEND DATA aggregationPeriodInfo — независимый report. Production Java отправляет compact
JSON seconds min/max, может ограничить request диапазоном channel/connector и до следующего
response показывать старое значение. Отдельного refusal ACK и correlation ID нет. Native API
никогда не выводит эффективный результат из request. Legacy omission, malformed/UNKNOWN,
offline/fenced/closed и fresh source без report возвращают std::nullopt. Известный 0..0 отличим
от UNKNOWN. Omitted delta сохраняет accumulated report, как production merge. Nullable local
property наследует top-level, явно пустой local property его перекрывает. Известные ranges
настроенных matching families объединяются minimum=min(minima), maximum=max(maxima).
Peer range — неотрицательные signed-64 миллисекунды без ограничения request cap.

Connection-control revision немедленно fencing-ует report при disconnect/reconnect; actual source
replacement также сбрасывает его. Поздняя публикация old source не проходит fence. Native access
остаётся на runtime owner thread, public reads копируют данные под endpoint mutex.
Пользовательские callbacks не исполняются под parser/endpoint/native container locks.

## Finite parser и ошибки

Parser повторяет pinned TimePeriodInfo pattern, а не общий JSON: точный compact min-then-max,
Java numeric syntax, округление seconds по Math.round и насыщение signed-64. Отрицательные или
инвертированные округлённые ranges дают UNKNOWN. Extension alternatives включают quoted strings,
flat objects/arrays и bare scalar; последний намеренно допускает некоторые nested-looking строки.
Raw Java outcomes не исправляются. Эти границы входят в 68-row oracle.

Один value ограничен 65536 UTF-16 units в рамках live property envelope. Текст, scan visits и scratch
initialization списывают caller work. Fixed stack scratch ограничен примерно 128 KiB, не удерживает
peer text и не выделяет heap. Рекурсии и JSON dependency нет. Out-of-profile даёт UNKNOWN.
При short turn-work owned protocol event сохраняется и повторяется до публикации/освобождения;
partial multi-family range не публикуется. Прежние finite event/resident/output limits действуют.
Нового worker, network service и SDK profile нет.

## Первичные доказательства

[DXEndpoint API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java),
[DistributorAdapter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/DistributorAdapter.java),
[AgentAdapter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/AgentAdapter.java),
[TimePeriodInfo](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimePeriodInfo.java)
привязаны к [production oracle](../../tools/upstream/server-aggregation-oracle/README.ru.md).
Raw fixtures включают full/delta/reset wire, clamp, legacy/invalid, inheritance, STREAM и настоящий
SocketConnector reconnect. Native tests читают эти bytes и сравнивают emitted deltas, не принимая
эхо request за negotiation.

## Намеренное исправление числовой несовместимости

При min и max, равных 10000000000000000000e2147483647 секундам, pinned Java FastDoubleUtil
переполняет внутреннее signed-int вычитание exponent и сообщает известный интервал 0..0.
Добавленная raw oracle row сохраняет этот фактический результат. Native намеренно использует
overflow-safe обработку exponent и сообщает INT64_MAX..INT64_MAX миллисекунд: насыщенный
положительный диапазон. Ошибка Java-арифметики не воспроизводится. Обычные numeric observations
сверяются отдельно; полная числовая эквивалентность для этого overflow case не утверждается.
Baseline 1e1000 насыщается в обеих реализациях.
