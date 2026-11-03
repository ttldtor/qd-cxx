# Публичный feed API {#feed-api-ru}

[English version](FEED-API.md) · [Практические руководства](guides.ru.md) · [Справочник API](api.ru.md)

Подключите `qd/api/feed.hpp` и линкуйте `qdcxx_feed`, чтобы получать типизированные
`events::Quote` и `events::Profile` через рабочий поток библиотеки. Приложение создаёт
endpoint, обработчики и подписки на точные символы; библиотека продвигает соединение,
конечные переподключения, очереди и доставку. Подписки можно создавать до и после
`connect()`.

Поддержан обычный бинарный QTP поверх TCP со встроенными полными схемами Quote/Profile.
Endpoint выбирает TICKER (по умолчанию: последние значения с возможным объединением
обновлений) или STREAM (каждая принятая выбранная запись источника). TLS, credentials,
HISTORY, wildcard-селекторы и внешние схемы в этот API не входят.
[Низкоуровневые API получения](SUBSCRIPTIONS.ru.md) предоставляют отдельные профили.

## Получить Quote

Сначала [соберите пакет с включённым API](CPP-BUILD.ru.md#public-feed-api-build-ru).
[Полный консольный пример](../examples/feed_quote/main.cpp) проверяет аргументы,
обрабатывает прерывание и ошибки вывода, ограничивает время работы и возвращает
ошибку при отсутствии данных. [Инструкция по сборке и запуску](../examples/feed_quote/README.ru.md)
использует установленный пакет.

Основная последовательность приложения:

```cpp
#include "qd/api/feed.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    namespace q = org::ttldtor::qdcxx;

    try {
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();

        quotes->addEventListener([](std::span<const q::events::Quote> batch) {
            for (const auto &quote : batch) {
                std::cout << quote.bidPrice.toString() << '\n';
            }
        });
        quotes->addSymbols({"AAPL", "IBM"});
        endpoint->connect("demo.dxfeed.com:7300");

        std::this_thread::sleep_for(std::chrono::seconds(10));

        const auto closed = endpoint->closeAndAwaitTermination();
        runtime.closeAndAwaitTermination();

        if (closed.diagnostic.code != q::api::ErrorCode::NONE) {
            std::cerr << "Endpoint diagnostic=" << static_cast<int>(closed.diagnostic.code)
                      << " stage=" << closed.diagnostic.stage << " native=" << closed.diagnostic.nativeCode
                      << " detail=" << closed.diagnostic.detailCode << " drained=" << closed.drained
                      << " expired=" << closed.deadlineExpired << " gap=" << closed.hadGap << '\n';
        }

        return closed.drained && !closed.deadlineExpired && !closed.hadGap &&
                       quotes->getDiagnostic().code == q::api::ErrorCode::NONE
                   ? 0 : 1;
    } catch (const q::api::Error &error) {
        std::cerr << error.diagnostic().stage << '\n';
        return 1;
    }
}
```

Ожидание ограничивает интервал получения в приложении, а не продвигает feed.
Короткий пример не проверяет, поступил ли хотя бы один Quote; для этого используйте
полную программу. Demo-адрес служит примером входных данных; доступность, подписки
и задержка данных зависят от внешнего сервиса.

Для Profile создайте `createSubscription<q::events::Profile>()` и обработчик с
аргументом `std::span<const q::events::Profile>`. Для STREAM задайте
`EndpointOptions::contract = DeliveryContract::STREAM` перед `createEndpoint(options)`.
Выбранный контракт доставки действует для всех подписок этого endpoint.

## Runtime, обработчики и завершение

Создайте `ApiRuntime` в главном потоке приложения **до запуска потоков приложения**.
Он получает существующий bootstrap транспорта и удерживает его до присоединения
своего рабочего потока. Рабочий поток запускается при первом принятом `connect()`
и обслуживает endpoint одного runtime. Согласуйте жизненный цикл с остальными
пользователями того же экземпляра c-ares; потоки приложения должны завершиться
до уничтожения runtime. См. [контракт bootstrap](../spec/transport/TCP-RUNTIME.ru.md).

Уничтожайте runtime в создавшем его потоке. Он не копируется и не перемещается.
`Endpoint::closeAndAwaitTermination()` и `ApiRuntime::closeAndAwaitTermination()`
разрешены только в этом потоке. Другой поток, включая обработчик, получает
`ErrorCode::WRONG_THREAD` до ожидания. Нарушение требования к потоку деструктора
runtime завершает процесс.

Повторный await из деструктора освобождаемого захвата обработчика (включая циклы
между endpoint), а также await runtime при освобождении захватов или рекурсивном
shutdown отклоняется с `ErrorCode::INVALID_STATE`.

Producer-методы endpoint и подписки можно вызывать из обработчиков. `close()`
запрашивает завершение без ожидания; исчерпание обычной ёмкости не блокирует этот
запрос. Освобождение endpoint/подписки запрашивает закрытие; завершением продолжает
владеть runtime. Дескрипторы могут пережить runtime как закрытые отсоединённые
порты и не удерживают нативный рабочий поток.

Обработчики последовательно вызываются рабочим потоком. Их span действителен только
до возврата из вызова; скопируйте событие, чтобы сохранить владеющие строки и значения.
Такие копии не удерживают очередь или runtime; хранение копий приложением не входит
в конечные резервы библиотеки. Не блокируйте обработчики надолго: это задерживает
другие endpoint и завершение. Deadline drain и деструктор не могут прервать код
приложения и не обещают жёсткой границы времени завершения операций ОС.

Исключение обработчика переводит его подписку в ошибку `CALLBACK_FAILURE`.
Вызов не повторяется, другие подписки продолжают работу. Удаление обработчика
запрещает будущий выбор, но уже выбранный вызов может завершиться. Захваты
обработчиков освобождаются вне управляющих блокировок. Ожидание закрытия endpoint
подтверждает завершение вызовов и освобождение захватов.

`CloseResult::drained` описывает реально принятый локальный префикс. Проверяйте также
`deadlineExpired`, `hadGap` и `diagnostic`. Результат не подтверждает ACK источника,
полноту ещё не прочитанных сетевых данных или непрерывность истории upstream.

## Принятые изменения и ошибки

`addSymbols`, `removeSymbols`, `setSymbols` и `clearSymbols` асинхронно принимают
ограниченное желаемое состояние. Дубликаты объединяются; `setSymbols({})` отзывает
все интересы. Возврат producer-вызова не означает отправку ADD/REMOVE или ACK peer.
У подписок независимые интересы и доставка, в том числе для пересекающихся символов.

Синхронный отказ бросает `api::Error`; `diagnostic()` содержит независимые статические
и числовые сведения. `getState()` и `getDiagnostic()` endpoint/подписки показывают
последнее наблюдение рабочего потока и первую асинхронную ошибку. Для продвижения
соединения и доставки приложению не требуется polling.

`connect()` принимает ASCII `host:port` или `[IPv6]:port`. Первый принятый адрес
фиксируется; повтор того же адреса у открытого endpoint ничего не меняет. Другой
адрес или попытка подключить закрытый/ошибочный endpoint отклоняются. Автоматические
конечные переподключения используют политику endpoint. Селекторы — непустые точные
строки printable ASCII в пределах лимита runtime, без ведущего `*` и wildcard/All.

## Конечные значения по умолчанию

Настройте `RuntimeOptions` до создания runtime и `EndpointOptions` до создания
endpoint. Лимиты учитывают ожидающие, активные и завершающиеся объекты. Это логические
резервы, а не предел RSS процесса. Увеличение лимитов может потребовать значительной
памяти; неподдержанное вычисление ёмкостей завершается явным отказом.

| Runtime option | По умолчанию |
| --- | --- |
| `maximumEndpoints` | 4 |
| `maximumSubscriptionsPerEndpoint` | 8 |
| `maximumSymbolsPerSubscription` | 16 |
| `maximumListenersPerSubscription` | 4 |
| `maximumSymbolUnits` | 128; настраивается в 1..128 |
| `dnsServers` | Пусто: до 4 системных IPv4-адресов DNS-серверов |
| `dnsPort` | 53 |

| Endpoint option | По умолчанию |
| --- | --- |
| `contract` | `TICKER` |
| `connectTimeout`, `dnsTimeout`, `writeTimeout` | По 5000 мс |
| `readTimeout` | 30000 мс |
| `pollInterval` | 1 мс |
| `maximumAttempts` | 3 всего, включая первую попытку |
| `retryDelay` | 250 мс |
| `drainTimeout` | 1000 мс |

Лимиты количества должны быть положительными. Deadline и период продвижения endpoint
задаются положительными миллисекундами до `INT_MAX`; retry delay положителен при числе
попыток больше одной. Конечное время drain может истечь с переходом к abort; это
не жёсткий timeout блокирующего await.

## Настройка DNS

Пустой `RuntimeOptions::dnsServers` считывает ограниченный системный снимок при
создании runtime: Windows `GetNetworkParams`, Linux `/etc/resolv.conf`. Сохраняются
до четырёх уникальных пригодных IPv4-адресов DNS-серверов в порядке платформы.
Ошибка обнаружения или отсутствие таких адресов даёт `DNS_CONFIGURATION`;
публичный DNS-сервер автоматически не подставляется. Снимок считывается даже для
будущего числового адреса endpoint; после создания числовой маршрут обходит lookup.

Для явной настройки добавьте в `dnsServers` от одного до четырёх ненулевых числовых
IPv4-адресов `transport::DnsServer` и при необходимости задайте `dnsPort` (1..65535).
Например, если адрес DNS-сервера получен из настроек вашей сети:

```cpp
q::api::RuntimeOptions options;
options.dnsServers.push_back(q::transport::DnsServer{{192, 0, 2, 53}});
q::api::ApiRuntime runtime(options);
```

`192.0.2.53` — адрес-заполнитель документации; замените его реальным DNS-сервером.
Эта настройка выбирает транспорт DNS-сервера, а не TLS или политику символов/доставки.

## Планируемая статистика

[Контракт послойной статистики](../spec/subscriptions/CLIENT-STATISTICS.ru.md) определяет
будущие счётчики endpoint/подписки, эпохи и доступность. Текущий фасад не предоставляет
эти счётчики; наблюдения состояния и диагностики остаются доступны.
