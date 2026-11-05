# Серверная агрегация

[English](SERVER-AGGREGATION.md) · [Feed API](FEED-API.ru.md)

Начальная политика задаётся EndpointOptions.requestedAggregationPeriod, текущее намерение —
явным Endpoint::setRequestedAggregationPeriod. Значение optional: целые миллисекунды от нуля
до 24 часов. Ноль просит отсутствие агрегации; std::nullopt сбрасывает запрос к server default.
Канонический ключ immutable configuration — dxfeed.requestedAggregationPeriod с QD TimePeriod
text; в construction address допустим connector alias requestedAggregationPeriod.

~~~cpp
#include <qd/api/endpoint_configuration.hpp>
using namespace org::ttldtor::qdcxx;
using namespace std::chrono_literals;

api::ApiRuntime runtime;
api::EndpointOptions options;
options.requestedAggregationPeriod = 5s;
auto endpoint = runtime.createEndpoint(options);
endpoint->connect("localhost:7300");

// A later application action changes only server-aggregation intent.
endpoint->setRequestedAggregationPeriod(250ms);
const auto requested = endpoint->getRequestedAggregationPeriod();
const auto reported = endpoint->getAggregationPeriodInfo();
// reported may be absent or describe the previous request. It is never a local echo.

endpoint->setRequestedAggregationPeriod(std::nullopt);
static_cast<void>(endpoint->closeAndAwaitTermination());
runtime.closeAndAwaitTermination();
~~~

Обычному клиенту не нужны polling или собственный worker. Setter коммитит локальное намерение,
существующий SDK worker отправляет последнее pending значение. Setter не активирует offline
endpoint, не переподключает его и не ждёт response. Частые updates могут сворачиваться.
До connect, после disconnect и после close намерение доступно и изменяемо без network work.

getAggregationPeriodInfo возвращает owning optional AggregationPeriodInfo с minimum/maximum
миллисекундами текущего peer. Сервер может ограничить request; report может отставать.
Legacy peer, malformed/UNKNOWN дают отсутствие значения, как offline/fenced/closed и fresh
connection до информации. Известный нулевой интервал отличим от отсутствующего.
ACK и корреляции request/report нет. Requested нельзя считать effective.

EndpointConfiguration остаётся immutable. getEffectiveConfiguration возвращает новый owning local
snapshot: после явного setter только aggregation row содержит текущее намерение, RUNTIME_UPDATE
и новую requestedAggregationRevision. Остальные properties и timing не меняются. Peer report
в этот local snapshot не попадает. Неверные values отклоняются атомарно.

[Wire контракт](../spec/transport/SERVER-AGGREGATION.ru.md) описывает deltas, reset, reconnect,
finite parser и ошибки. [Production oracle](../tools/upstream/server-aggregation-oracle/README.ru.md)
содержит pinned Java outcomes и actual full/delta bytes. Используется существующий plain Feed
receiving path; новых TLS, authentication и server capabilities здесь нет.

## Намеренное исправление числовой несовместимости

При min и max, равных 10000000000000000000e2147483647 секундам, pinned Java FastDoubleUtil
переполняет внутреннее signed-int вычитание exponent и сообщает известный интервал 0..0.
Добавленная raw oracle row сохраняет этот фактический результат. Native намеренно использует
overflow-safe обработку exponent и сообщает INT64_MAX..INT64_MAX миллисекунд: насыщенный
положительный диапазон. Ошибка Java-арифметики не воспроизводится. Обычные numeric observations
сверяются отдельно; полная числовая эквивалентность для этого overflow case не утверждается.
Baseline 1e1000 насыщается в обеих реализациях.
