# Адреса endpoint и эффективная конфигурация

[English](ENDPOINT-CONFIGURATION.md) · [Feed API](FEED-API.ru.md) · [Архитектура конфигурации](CONFIGURATION.ru.md)

Подключите qd/api/endpoint_configuration.hpp и цель qdcxx_feed. Этот профиль настраивает
существующий принимающий endpoint с plain binary QTP. Разрешение копирует явные входы,
проверяет их и не выполняет DNS, чтение файлов, активацию SDK или callbacks приложения.

## Настройка и подключение

Фрагмент предполагает api::ApiRuntime с именем runtime, которым приложение владеет на main.
Создайте подписки и callbacks через [Feed API](FEED-API.ru.md), затем вызовите connect явно.
Прогрессом владеет библиотека; опрашивать конфигурацию для продвижения работы не требуется.

~~~cpp
#include "qd/api/endpoint_configuration.hpp"
#include <array>

namespace api = org::ttldtor::qdcxx::api;

const std::array defaults{
    api::EndpointProperty{"qdcxx.maximumAttempts", "3"},
    api::EndpointProperty{"qdcxx.connectTimeoutMillis", "5000"}};
const std::array properties{
    api::EndpointProperty{"dxfeed.address", "localhost:7300[reconnectDelay=250]"},
    api::EndpointProperty{"qdcxx.name", "quotes"}};
auto configuration = api::EndpointConfiguration::create({}, defaults, properties);
auto endpoint = runtime.createEndpoint(configuration);
auto feed = endpoint->getFeed();
// Install the application's subscriptions and callbacks here.
endpoint->connect();
auto observation = endpoint->getEffectiveConfiguration();
// During application shutdown, on the runtime creator thread:
auto terminal = endpoint->closeAndAwaitTermination();
~~~

Конструирование никогда не подключает endpoint. Endpoint::connect() использует настроенный
маршрут и отклоняется, если его нет. Прежняя перегрузка connect(address) принимает plain
маршрут и может выбрать другой первый адрес. После admission фиксируется его точное написание;
disconnect/reconnect сохраняют его. Inline-свойства этой перегрузки отклоняются, а неизменяемая
политика сохраняется. Действуют прежние правила lifecycle, retirement callbacks и ожидания на creator thread.

## Источники и момент чтения

Приоритет: типизированный fallback EndpointOptions, явные defaults, явные properties,
затем поддерживаемые свойства адреса. Последний дубликат в слое побеждает. Явные false и
пустое нативное имя сохраняются. Ни один слой не читает system properties, environment,
файлы конфигурации или глобальные настройки процесса. Разбирается только выбранный адрес;
каждая входная запись, включая перекрытые, расходует конечный бюджет, и её ключ должен
поддерживаться. Выбранные значения проверяются полностью.

Views EndpointProperty заимствуются только на время create(). Результат владеет всеми
сохранёнными данными и необязательным immutable registry. Последующие изменения или уничтожение
входов на него не влияют. Разные удерживающие handles можно одновременно читать/копировать;
замена или уничтожение одного и того же handle требует синхронизации вызывающего. Moved-from
handle допускает только уничтожение или присваивание.

## Поддерживаемая политика

Ключи чувствительны к регистру. Только inline alias connector reconnectDelay сравнивается
без учёта ASCII-регистра и означает миллисекунды, как в pinned Java consumer.

| Ключ | По умолчанию | Значение |
| --- | --- | --- |
| dxfeed.address | отсутствует | Проверенный plain маршрут; при конструировании допустимы flat-блоки свойств |
| qdcxx.name | пустое | Локальная metadata-метка printable ASCII до 128 байт; потоки SDK не переименовывает |
| qdcxx.deliveryContract | ticker | ticker или stream |
| dxfeed.wildcard.enable | false | true/false без учёта ASCII-регистра; true требует STREAM |
| qdcxx.maximumAttempts | 3 | Полное unsigned decimal от 1 до 4294967295 |
| qdcxx.connectTimeoutMillis | 5000 | Полное unsigned decimal от 1 до 2147483647 миллисекунд |
| qdcxx.dnsTimeoutMillis | 5000 | Тот же диапазон миллисекунд |
| qdcxx.readTimeoutMillis | 30000 | Тот же диапазон миллисекунд |
| qdcxx.writeTimeoutMillis | 5000 | Тот же диапазон миллисекунд |
| qdcxx.pollIntervalMillis | 1 | Тот же диапазон; внутренняя частота worker |
| qdcxx.retryDelayMillis | 250 | Тот же диапазон; ноль допустим только при одной попытке |
| qdcxx.drainTimeoutMillis | 1000 | Тот же положительный диапазон миллисекунд |

Типизированный fallback сохраняет свои API defaults, включая identity custom registry.
При одной попытке consumer получает нулевую задержку повтора; observation отдельно сохраняет
выбранную requested delay. Знаки, частичная конверсия и окружающие пробелы в endpoint values
отклоняются. Inline cells удаляют ASCII-пробелы вокруг ключа и значения.
Некорректный boolean отклоняется.

Обычные host names используют bounded ASCII/A-label DNS labels; IDNA-конверсию Unicode выполняет
вызывающий явно. Numeric IPv4 содержит четыре decimal octets без ведущих нулей. IPv6 заключается
в квадратные скобки. Порт — полное decimal от 1 до 65535. Сохраняется high-level предел:
253 байта host и 261 байт маршрута; DNS label содержит не более 63 байт.

При конструировании поддержаны завершающие square-bracket blocks, например:

~~~text
localhost:7300[reconnectDelay=250,qdcxx.name=quotes][qdcxx.maximumAttempts=3]
~~~

Блоки сохраняют исходный порядок. В них допустимы нативные name/timeouts/attempts и проверенный
alias reconnectDelay. Address, delivery-contract и wildcard в блоках отклоняются.
Nested blocks, parentheses, escaping, URI/codec prefixes, списки hosts, proxy/listen/file
connectors и credentials отклоняются. Этот профиль не добавляет TLS или negotiated authentication.

## Просмотр, ошибки и ограничения

snapshot() возвращает одиннадцать владеющих canonical policy rows и необязательный адрес без
inline-текста. getEffectiveConfiguration() дополнительно копирует первое принятое написание
маршрута, текущий принятый connect intent и control revision под mutex endpoint. Локальные policy,
metadata и запросы не подтверждают DNS, доставку peer или согласованные сервером значения.
Снимок читается и после retirement endpoint/runtime.

EndpointConfigurationLimits ограничивает совокупное число записей (64), длину ключа (96),
value/address (4096) и просмотренные байты ключей/значений (16384). Inline-записи дополнительно
расходуют тот же бюджет поверх содержащего их адреса. Выход содержит одиннадцать policy rows
и не более двух строк маршрута. Это логические пределы, а не точный allocator/RSS cap.
Runtime, registry, network и retained copies сохраняют свои независимые ограничения.

Неподдерживаемые ключи/формы дают UNSUPPORTED_PROFILE, некорректные поддерживаемые значения —
INVALID_ARGUMENT, исчерпание входной ёмкости/памяти — RESOURCE_LIMIT или ALLOCATION_FAILURE.
В ошибках только static labels; исходные ключи, адреса и значения не форматируются.
Credential keys и userinfo routes отклоняются и не сохраняются в effective snapshot.
Используйте отдельно поддержанный secure handoff, когда он доступен; хранение секретов в обычной
метке или hostname выходит за границы профиля. Secure erasure входов вызывающего не обещается.
Ошибка проверки не публикует endpoint slot или connection intent и не запускает worker.

## Java baseline и границы возможностей

[Pinned DXEndpoint API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L688-L868)
и [44 production observations](../tests/fixtures/endpoint-configuration/oracle.tsv)
описаны в [oracle runner](../tools/upstream/endpoint-configuration-oracle/README.ru.md).
Raw outcomes сохраняют поведение Java: её socket parser допускает нулевые/отрицательные порты
и multi-host lists; connector принимает parentheses и дополнительные свойства.
Native проверка отклоняет недоступные формы до активации.

Java FEED активирует выбранный адрес при build; STREAM_FEED этого не делает.
Inline connector properties Java применяет при создании connector. Native разрешает свой subset
при конструировании immutable configuration и требует явной активации.
Изменение Java builder видно через map свойств ранее построенного endpoint; native snapshot
остаётся неизменяемым. Java игнорирует неизвестные builder keys и читает malformed boolean как
false в boolean consumer; native отклоняет оба случая. Java file defaults предшествуют system
defaults, explicit builder values побеждают; native получает слои явно, без ambient discovery.
Это явные native compatibility boundaries, а не объявление поведения Java ошибкой.

Полный configuration catalogue, TLS/authentication handoff, failover/multiple hosts,
proxy/listen/file connectors и server aggregation остаются отдельными возможностями.
Этот профиль не объявляет полную веху принимающего клиента завершённой.