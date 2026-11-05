# Публичный feed API {#feed-api-ru}

[English version](FEED-API.md) · [Практические руководства](guides.ru.md) · [Справочник API](api.ru.md)

Подключите `qd/api/feed.hpp` и линкуйте `qdcxx_feed`, чтобы получать типизированные
`events::Quote`, `events::Profile`, `events::Trade`, `events::TradeETH`, `events::Summary`, `events::TextMessage`, `events::TextConfiguration`, `events::Message`, `events::Configuration` и `events::Order` через рабочий поток библиотеки. Приложение создаёт
endpoint, обработчики и подписки на точные символы; библиотека продвигает соединение,
конечные переподключения, очереди и доставку. Подписки можно создавать до и после
`connect()`.

Поддержан обычный бинарный QTP поверх TCP со встроенными полными схемами Quote/Profile/Trade/TradeETH/Summary
и текстовых событий либо явными валидированными схемами вызывающего кода и зарегистрированными mappings.
Endpoint выбирает TICKER (по умолчанию: последние значения с возможным объединением
обновлений), STREAM (каждая принятая выбранная запись источника) либо явный HISTORY
(индексируемое общее хранилище и независимые views). Order требует HISTORY; стандартная схема
Order принадлежит SDK и не требует схемы от приложения. Остальные девять типов сохраняют
существующие профили TICKER/STREAM и отклоняют HISTORY до native-регистрации. TLS и credentials
остаются отдельными профилями приёма. Wildcard-селекторы
требуют существующего явного разрешения STREAM. Загрузка схем является отдельной задачей.
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
#include <stop_token>

int main() {
    namespace q = org::ttldtor::qdcxx;

    try {
        std::stop_source stop;
        q::api::ApiRuntime runtime;
        auto endpoint = runtime.createEndpoint();
        auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();

        quotes->addEventListener([](std::span<const q::events::Quote> batch) {
            for (const auto &quote : batch) {
                std::cout << quote.bidPrice.toString() << '\n';
            }
        });
        quotes->addStateChangeListener([source = stop](auto, auto current) mutable {
            if (current == q::api::SubscriptionState::FAILED) {
                source.request_stop();
            }
        });
        quotes->addSymbols({"AAPL", "IBM"});
        endpoint->connect("demo.dxfeed.com:7300");

        const auto cutoff = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED, cutoff, stop.get_token()));

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

## Получить Trade

Built-in registry добавляет composite Trade2, сохраняя Quote0/Profile1. Используйте owning
`events::Trade` с Java business getters; LONG может отличаться от wide-to-double на1ULP.
Для реального regional Trade&Q передайте явный экспортированный registry с соответствующим
`EventType::TRADE`. Selector AAPL&Q тогда отправляется как physical(Trade&Q,AAPL); простого
переименования символа после generic decoding недостаточно. Полные fields/types/defaults, flags,
packed time и явная граница однослотового TradeId описаны в [Trade](../spec/events/TRADE.ru.md).

```cpp
auto trades = endpoint->getFeed()->createSubscription<q::events::Trade>();
trades->addEventListener([](std::span<const q::events::Trade> batch) {
    for (const auto &trade : batch) {
        std::cout << trade.getTime() << ' ' << trade.getPrice() << ' '
                  << trade.getSizeAsDouble() << '\n';
    }
});
trades->addSymbols({"AAPL"});
```

Runtime/endpoint прогрессируют через SDK; клиент не опрашивает queue или work budgets. Копия Trade
владеет символом и inline snapshot, callback span действует только во время вызова. Непустые наборы зарегистрированных типов с учётом выбранного delivery contract можно
выбирать mixed-подпиской; предел полной receiving-схемы16 records сохраняется.

## Явный реестр событий

Для значений по умолчанию настраивать реестр не требуется. Для валидированной
пользовательской `schema` зарегистрируйте явные маршруты записей до создания endpoint:

```cpp
const std::array routes{q::api::EventRegistration{q::api::EventType::QUOTE, 1},
                        q::api::EventRegistration{q::api::EventType::QUOTE, 2}};
q::api::EventRegistryWork work;
auto registry = q::api::EventRegistry::create(schema, routes, {}, work);

if (!registry) {
    // Inspect the typed cause and optional registrationIndex before continuing.
    return 1;
}

q::api::EndpointOptions options;
options.eventRegistry = std::move(*registry);
auto endpoint = runtime.createEndpoint(options);
auto quotes = endpoint->getFeed()->createSubscription<q::events::Quote>();
```

Этот расширенный фрагмент предполагает существующий runtime и подключение `<array>`
и `<utility>` вместе с `qd/api/feed.hpp`. Local record IDs остаются плотными позициями
каталога; пример требует совместимых записей 1/2 после записи 0. Имя записи не
выбирает класс события. Существующие адаптеры разрешают поддержанные явные business
lookup aliases и точные типы, сохраняя иной порядок полей, дополнительные поля и
generic records. Участвуют зарегистрированные composite-записи запрошенного типа;
региональные Quote-маршруты подчиняются выбору exchange ниже.
Повторные маршруты local ID отклоняются, отсутствующий тип отклоняется синхронно.
Для нескольких типов в одной логической подписке используйте перегрузку ниже.

Endpoint копирует неизменяемый владеющий снимок реестра. Последующие присваивание
или уничтожение исходных schema/registry/options не могут заменить его. Создание
имеет явные конечные пределы схемы, маршрутов, allocation requests и работы; оно не
вызывает callbacks приложения, loader или сеть. Receiving profile поддерживает до
16 записей, 128 полей суммарно, 256 lookups, 128 UTF-16 units на имя и 16384 units
всех имён. Реестр, допустимый при больших пределах, отклоняется на `profile-schema`
до native activation; усечения нет. См. полный
[контракт владения, ошибок и пределов реестра](../spec/events/EVENT-REGISTRY.ru.md).

Подготовленные Quote/Profile/Trade/TradeETH/Summary/Message/Configuration используют исчерпывающие
[правила преобразования полей](../spec/events/FIELD-CONVERSIONS.ru.md): допустимые числа
сохраняют WideDecimal поля и предоставляют прямые Java business getters вроде
`quote.getBidPrice()`/`profile.getBeta()` без LONG-to-wide округления. Raw int свойства сохраняют Java physical-slot поведение,
UTF_CHAR_ARRAY/STRING сохраняют nullable UTF-16 текст. Неверные присутствующие типы отвергаются
при подготовке registry. Default registration допускает 128 альтернатив типов; меньшие custom
границы остаются явными resource limits.

Для региональной доставки Quote передайте настоящее проверенное объявление региональной
записи и зарегистрируйте его как `EventType::QUOTE`. Например, если выбранная схема содержит
`Quote&Q`, обычная подписка `quotes->addSymbols("AAPL&Q")` отправляет физический interest
`Quote&Q`/`AAPL`. Принятый владеющий Quote имеет символ `AAPL&Q`; отсутствующие exchange-поля
получают суффикс записи. Правило одинаково для любого зарегистрированного exchange.
Composite и regional interests различаются в смешанной подписке. Совпадающий региональный
маршрут выбирается отдельно для каждого вида событий; иначе composite-маршруты сохраняют
точный логический символ. Завершающие атрибуты сохраняются. Remove и явный reconnect
используют ту же выбранную запись и физический символ.

Выбранный региональный маршрут должен восстанавливать точное логическое написание.
Необратимые формы вроде `AAPL&N&Q` или `AAPL{x=y}&Q` вызывают `INVALID_ARGUMENT` на
`subscription-symbol-representation` из `addSymbols`/`setSymbols`; отклоняется полное
изменение, прежний desired set сохраняется. Composite fallback оставляет эти буквальные
символы без изменений. Обычная форма с атрибутами `AAPL&Q{x=y}` представима.

Встроенный registry содержит composite Quote/Profile/Trade/TradeETH/Summary и оба текстовых события. Этот путь через явный ограниченный
registry не загружает полный региональный каталог и не реализует более широкие selectors.
[Контракт времени и вариантов](../spec/events/EVENT-TIME.ru.md) задаёт необязательные
Sequence/nanoseconds, секунды/миллисекунды Quote, halt-секунды Profile, независимые метаданные
событий и границы детерминированных локальных wire-доказательств. Новых утверждений
о доступности публичного сервиса нет.

## Получение нескольких типов событий

Фрагмент предполагает существующий endpoint и подключение `<type_traits>`:

```cpp
auto mixed = endpoint->getFeed()->createSubscription(
    {q::api::EventType::QUOTE, q::api::EventType::PROFILE, q::api::EventType::QUOTE});
mixed->addEventListener([](std::span<const q::api::RegisteredEvent> batch) {
    for (const auto &event : batch) {
        std::visit([](const auto &value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, q::events::Quote>) {
                // Read the owning Quote value.
            } else {
                // Read the owning Profile value.
            }
            (void)value;
        }, event);
    }
});
mixed->addSymbols({"AAPL", "IBM"});
```

Создаётся одна `Subscription<RegisteredEvent>` с одним набором символов. Повторы
типов объединяются; зарегистрированные маршруты каждого выбранного типа используют
описанный выше exchange-выбор. Набор типов копируется и остаётся фиксированным для подписки. Массив runtime
можно передать перегрузке `std::span<const EventType>`. Пустой список или неизвестный
enum даёт `INVALID_ARGUMENT`; отсутствие поддержанного типа в реестре endpoint —
`UNSUPPORTED_PROFILE`, без публикации подписки.

`RegisteredEvent` — владеющий variant всех поддержанных зарегистрированных событий, включая TextMessage и TextConfiguration.
Скопируйте значение, чтобы сохранить его после callback span. Конкретные и смешанные
подписки могут независимо перекрываться; закрытие одной отзывает только её оставшиеся
исключительные interests. Границы callback batch следуют очереди; один batch может
содержать не все выбранные типы. См. полный
[контракт смешанного выбора и доставки](../spec/events/MIXED-SUBSCRIPTIONS.ru.md).

## Runtime, обработчики и завершение

Создавайте владеющий `ApiRuntime(RuntimeOptions)` на главном потоке **до потоков
приложения**; они должны завершиться до его уничтожения. Хост, заранее создавший
`RuntimeBootstrap` на главном потоке до запуска потоков, позже может вызвать явный
заимствующий конструктор на том же создавшем потоке, сохраняя владельца хоста.
Обе формы удерживают bootstrap lease до уничтожения самого объекта runtime,
включая период после закрытия. Worker лениво запускается при первом принятом
`connect()` и обслуживает endpoint одного runtime. Согласуйте всех пользователей
того же состояния c-ares. См. [контракт встраивания и владения](../spec/transport/RUNTIME-EMBEDDING.ru.md).

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

После полного завершения запросы состояния/диагностики и освобождение последней
ссылки на терминальный дескриптор разрешены на другом потоке. `awaitState` остаётся
привязанным к создавшему потоку даже при уже достигнутом терминальном состоянии;
закрытая подписка отклоняет новый приём с `CLOSED`. Дескрипторы не удерживают lease
SDK и не могут возобновить native-работу после уничтожения runtime. Сохранённая
native-ошибка остаётся `FAILED` с исходной диагностикой, а не превращается в успешное `CLOSED`.

Обработчики последовательно вызываются рабочим потоком. Их span действителен только
до возврата из вызова; скопируйте событие, чтобы сохранить владеющие строки и значения.
Такие копии не удерживают очередь или runtime; хранение копий приложением не входит
в конечные резервы библиотеки. Не блокируйте обработчики надолго: это задерживает
другие endpoint и завершение. Deadline drain и деструктор не могут прервать код
приложения и не обещают жёсткой границы времени завершения операций ОС.

Исключение обработчика событий переводит его подписку в ошибку `CALLBACK_FAILURE`.
Вызов не повторяется, другие подписки продолжают работу. Удаление обработчика
запрещает будущий выбор, но уже выбранный вызов может завершиться. Захваты
обработчиков освобождаются вне управляющих блокировок. Ожидание закрытия endpoint
подтверждает завершение вызовов и освобождение захватов.

`CloseResult::drained` описывает реально принятый локальный префикс. Проверяйте также
`deadlineExpired`, `hadGap` и `diagnostic`. Результат не подтверждает ACK источника,
полноту ещё не прочитанных сетевых данных или непрерывность истории upstream.

## Наблюдатели состояния

Зарегистрируйте наблюдателя endpoint или подписки до нужных переходов:

```cpp
const auto listener = endpoint->addStateChangeListener(
    [](q::api::EndpointState previous, q::api::EndpointState current) {
        // Observe the transition; keep this callback short.
        (void)previous;
        (void)current;
    });
endpoint->removeStateChangeListener(listener);
```

Те же методы `Subscription<Event>` используют `SubscriptionState`. Регистрация
**не вызывает начального уведомления**; `getState()` отдельно возвращает текущий
снимок. Наблюдатели получают различные фактические старое и новое состояния
в порядке публикации, включая ошибку и терминальное состояние. Полный снимок
наблюдателей выбирается атомарно с переходом; вызовы идут в порядке регистрации
вне управляющих блокировок. Удаление запрещает будущий выбор; уже выбранный вызов
может состояться. Удаление идемпотентно и доступно после close. Добавление после
отсечения приёма отклоняется с `CLOSED`; обычная ёмкость не блокирует close.

После первого `connect()` этого runtime вызовы выполняет SDK worker. Если runtime
ещё не запускал worker, завершение endpoint/runtime в создавшем потоке вызывает
наблюдателей состояния без запуска worker. Из обработчика можно читать состояние,
вызывать close, удалять наблюдателей и работать с другим endpoint. Await из worker
отклоняется с `WRONG_THREAD`; повторный await того же endpoint или runtime из
создающего потока при завершении отклоняется с `INVALID_STATE`. Разные неактивные
endpoint могут завершаться рекурсивно; завершение неактивного endpoint в создающем
потоке может идти одновременно с worker другого активного endpoint.

Наблюдатели состояния носят уведомительный характер. Если один бросает исключение,
вызывается весь выбранный снимок. `getStateChangeDiagnostic()` отдельно сохраняет
первую статическую ошибку `CALLBACK_FAILURE`; состояние владельца, `getDiagnostic()`
и `CloseResult` не меняются. `CLOSED` и `FAILED` остаются поглощающими. Как и раньше,
`CLOSED` подтверждает завершение нативной регистрации и освобождение всех захватов
**обработчиков событий**. Только новые явно добавленные наблюдатели состояния ещё
могут выполняться или освобождать захваты; await endpoint включает обе категории
до возврата и освобождения слота runtime.

Отдельные положительные лимиты `maximumStateListenersPerEndpoint` и
`maximumStateListenersPerSubscription` учитывают принятые, выбранные и завершаемые
объекты наблюдателей. Удаление освобождает ёмкость лишь после последнего выбранного
вызова и уничтожения захватов приложения. Очередь состояний не растёт с переходами:
первый опубликованный producer-потоком `CONNECTING` занимает один отложенный слот,
а остальные наблюдения сразу доставляются владеющим контекстом исполнения.
Синхронный отказ запуска worker не публикует переход и не вызывает наблюдателя
в producer-потоке. См. [полный контракт](../spec/subscriptions/FEED-STATES.ru.md).

## Принятые изменения и ошибки

`addSymbols`, `removeSymbols`, `setSymbols` и `clearSymbols` асинхронно принимают
ограниченное желаемое состояние. Дубликаты объединяются; `setSymbols({})` отзывает
все интересы. Возврат producer-вызова не означает отправку ADD/REMOVE или ACK peer.
У подписок независимые интересы и доставка, в том числе для пересекающихся символов.

Синхронный отказ бросает `api::Error`; `diagnostic()` содержит независимые статические
и числовые сведения. `getState()` и `getDiagnostic()` endpoint/подписки показывают
последнее зафиксированное состояние и первую асинхронную ошибку. Для продвижения
соединения и доставки приложению не требуется polling.

`connect()` принимает ASCII `host:port` или `[IPv6]:port`. Первый принятый адрес
фиксируется; повтор того же адреса при логически включённом подключении ничего
не меняет. После `disconnect()` тот же адрес возобновляет работу в свежей эпохе. Другой
адрес или попытка подключить закрытый/ошибочный endpoint отклоняются. Автоматические
конечные переподключения используют политику endpoint. Селекторы — непустые точные
строки printable ASCII в пределах лимита runtime, без ведущего `*` и wildcard/All.

`disconnect()` немедленно публикует `NOT_CONNECTED`, сохраняя желаемые интересы и
спроецированный кэш. После физической quiescence паузы новые/изменённые подписки
могут получить этот кэш. `reconnect()` запрашивает свежую эпоху при логически
включённом подключении; после явного disconnect ничего не делает.
`disconnectAndClear()` немедленно запрещает новый выбор старых данных и запрашивает
асинхронную настоящую очистку, в том числе при уже отключённом endpoint.
`NOT_CONNECTED` не является барьером завершения clear. Уже выбранный снимок
listeners может завершиться; собственные копии событий действительны. Управление
не пополняет конечные попытки/работу. См.
[полный контракт lifecycle и admission](../spec/subscriptions/FEED-LIFECYCLE.ru.md).
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
| `maximumStateListenersPerEndpoint` | 4 |
| `maximumStateListenersPerSubscription` | 4 |
| `maximumEventTypeArgumentsPerSubscription` | 64 исходных аргумента типов, включая повторы; положительное |
| `maximumPendingStateChangesPerEndpoint` | 8; ожидающие/исполняемые/освобождаемые снимки |
| `maximumSymbolUnits` | 128; настраивается в 1..128 |
| `dnsServers` | Пусто: до 4 системных IPv4-адресов DNS-серверов |
| `dnsPort` | 53 |

| Endpoint option | По умолчанию |
| --- | --- |
| `contract` | `TICKER` |
| `eventRegistry` | Отсутствует: встроенные Quote0/Profile1/Trade2/TradeETH3/Summary4/TextMessage5/TextConfiguration6; явное значение копируется как неизменяемый снимок |
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

## Статистика

[Контракт послойной статистики](../spec/subscriptions/CLIENT-STATISTICS.ru.md) определяет
описанные ниже счётчики endpoint/подписки, эпохи и доступность. Методы скоростей и
baseline остаются планируемыми; диагностика жизненного цикла независима.

## Ожидание текущего состояния

`endpoint->awaitState(target, deadline, stopToken)` и
`subscription->awaitState(target, deadline, stopToken)` блокируют только
поток-создатель runtime без опроса и выполнения native-работы.
`StateWaitResult<State>` возвращает текущее состояние и первую диагностику
жизненного цикла. Приоритеты условий заданы явно: целевое `REACHED`, другое
поглощающее `TERMINAL` (`CLOSED`/`FAILED`), `CANCELLED`, затем `DEADLINE_EXPIRED`.
По умолчанию срок равен steady-clock `max()`, stop token пустой. Прошедший переход
не запоминается.

Чужие потоки/SDK callbacks получают `api::Error` / `WRONG_THREAD`; любой контекст
callback/освобождения захватов на потоке-создателе получает `INVALID_STATE`,
в том числе для другого объекта и уже совпавшего состояния. Неверное значение enum
даёт `INVALID_ARGUMENT`. Проверки предшествуют быстрому возврату готового результата.
Ожидание состояния не меняет жизненный цикл и не расходует ёмкость наблюдателей.
`awaitState(CLOSED)` может вернуться до освобождения подключённых наблюдателей
состояния; полный барьер quiescence — `closeAndAwaitTermination()`.
См. [результаты ожидания, отмену и время жизни обработки сигналов](../spec/subscriptions/FEED-WAITS.ru.md).

## Получить TradeETH

Создайте `createSubscription<q::events::TradeETH>()` с getPrice()/getSizeAsDouble(), как в примере
Trade. Builtin TradeETH3 — composite; явная TRADE_ETH registration включает настоящий TradeETH&Q.
ETH bit может быть false при переходе в RTH. Nano key — Last.TimeNanoPart, Tick fallback отсутствует.
Полные поля, defaults и конечные ограничения: [TradeETH](../spec/events/TRADE-ETH.ru.md).

## Получить Summary

Создайте `createSubscription<q::events::Summary>()`; getDayOpenPrice()/getPrevDayVolume() возвращают
business double, getOpenInterest() — полный mapping-long OI с исправлением потери pinneddelegate
по [ADR-0010](decisions/ADR-0010-summary-open-interest-width.ru.md).
Builtin Summary4 — composite, явная SUMMARY registration включает настоящий Summary&Q. Day IDs —
signed дни, не business timestamps. PriceType берётся из flags. Поля, defaults и региональное
отсутствие OpenInterest: [Summary](../spec/events/SUMMARY.ru.md).

## Явные decorated selectors

Owning `SubscriptionSelector` и `addSelectors`/`setSelectors`/`removeSelector` сохраняют typed kind/source identity отдельно от physical demand. Quote/Profile/Trade/TradeETH/Summary игнорируют IES source ID как production Java. Order направляет явные source IDs в зарегистрированные записи Order; plain-селекторы и отрицательные source IDs выбирают доступное объединение sources. См. [получение индексируемого Order](ORDER.ru.md). TSS typed Feed атомарно неподдержан. `EndpointOptions::enableWildcard=true` разрешает explicit All только для STREAM и по умолчанию выключен. Literal leading-star не включает wildcard; прежние string/Candle API остаются plain exact. [Полный контракт, пример, конечное хранение и known-binding source adapter and time value conversion](../spec/subscriptions/DECORATED-SELECTORS.ru.md).

TextMessage требует endpoint STREAM. TextConfiguration использует STREAM или TICKER; владеющий nullable-текст и политика версии описаны в [контракте](../spec/events/TEXT-EVENTS.ru.md).

## Наблюдение принятых подписок и query classifiers

`getSnapshot()` копирует одну полную принятую desired revision, точные типизированные логические selectors и неизменяемый нормализованный набор event types. Результат переживает мутацию и завершение runtime. `getEventTypes()` и `containsEventType()` читают этот неизменяемый набор. Accepted revision отличается от worker application, физического спроса и peer acknowledgement.

`addChangeListener()` сообщает начальное непустое состояние и committed REMOVED/ADDED phases. Replace сначала удаляет, затем добавляет с одной ревизией. Callback заимствует аргументы; для независимого хранения копируйте `change.snapshot` или selectors. Вызовы происходят вне control locks. Reentry и concurrent calls используют конечное after-return continuation, включая разные runtime и выход всего выбранного data callback. Caller не опрашивает состояние, не управляет очередями и не продвигает сеть.

Observer exceptions являются advisory и доступны через `getChangeDiagnostic()`; прежняя политика ошибок event listeners сохраняется. `close()` остаётся nonblocking fence. Фактический worker retirement либо creator `closeAndAwaitTermination()` доставляет terminal CLOSED и включает выбранные callbacks и разрушение captures. Перед зависимостью от native timing differences относительно Java прочитайте точный [контракт observation/lifetime/budgets](../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.ru.md).

`Endpoint::createFilter(orderedCategories, context)` строит независимый classifier с фактическим registry и delivery contract endpoint; advanced builder также принимает явный registry и finite limits. Он не меняет subscription demand. Optional context задаётся один раз для явных resource capabilities. [Filter queries, categories, trackers и владение IPF source](FEED-FILTERS.ru.md) описывают отдельные lifetimes и optional backend.

Installed/relocated [public consumer](../tests/conformance/subscription_observer_consumer/CMakeLists.txt) собирается только через `find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)` и `qdcxx::feed`. Он независимо включает каждый публичный header и проверяет detached snapshots, callback phases, classifier queries, refusal и offline retirement без private headers и source paths.

## Статистика endpoint

`endpoint->getStatistics()` копирует последнюю согласованную публикацию в
владеющий `EndpointStatistics`. Чтение разрешено параллельно из потоков producer
и после завершения runtime. `ownerIdentity` удерживает только ограниченный
непрозрачный токен; сравнивайте его на равенство, не интерпретируя адрес. Разные
владельцы не совпадут, пока сохранено старое наблюдение. `generation` остаётся
равным единице в течение жизни владельца. `observedAt` — монотонное время
публикации, а не чтения. Чтение не выделяет память, не выполняет IO/DNS, не вызывает
callback и не сбрасывает работу. Заблокированный callback задерживает следующую
публикацию; читатели продолжают видеть предыдущий согласованный срез.

Поля `connection` разделяют физический IO ОС и завершение протокола.
`readBytes`/`writtenBytes` учитывают положительные результаты native receive/send
и completion, включая частичные передачи и впоследствии отброшенный вход. В plain
Feed TCP это октеты потока QTP. Полный структурно принятый входной кадр увеличивает
`readMessages` до обработки payload, поэтому отвергнутый payload тоже даёт один
кадр. DATA учитывается после проверки всего payload и построения события, включая
wire records, пропущенные локальной привязкой схемы, до проекции и fan-out.
Исходящие сообщения и записи ADD/REMOVE учитываются после принятия сокетом
последнего байта кадра. Очередь и desired selectors ничего не добавляют; реальный
replay считается повторно. Принятие сокетом не доказывает peer acknowledgement.
При неизвестном префиксе прерванной Windows write sticky `writeAcceptanceUnknown`
обозначает число байтов как нижнюю границу; полные кадры считаются по сообщённому
прогрессу.

`connectionAttempts` считает реальные входы в recovery factory, включая ошибки
DNS/connect. `attemptIdentity` — текущий порядковый номер factory этого владельца;
без владельца попытки значение отсутствует. `sourceEpoch` — номер установленного
источника; до установления и после retirement он отсутствует. Для ненасыщенных
итогов `establishedConnections = closedConnections + activeConnections`;
готовность протокола — отдельное состояние. Reconnect меняет источник, сохраняя
итоги и generation. Baseline — независимая копия потребителя; разрушительного
reset и метода расчёта скорости нет.

Каждый `StatisticCounter` содержит собственный `availability`. Доступные счётчики
в простое равны нулю; переполнение насыщается до `UINT64_MAX` со sticky `SATURATED`,
после чего точные дельты этого поля недоступны. Входящие подписки, исходящий DATA
и QD lag/RTT сейчас возвращают `UNSUPPORTED`. Метки событий и длительность callback
не заменяют wire timing. Метод инструментирует существующий plain Feed path;
будущий Feed TLS должен отдельно определить границы encrypted transport и plaintext QTP.

Native итоги сохраняются после возврата всех собственных completion. `final`
становится true лишь после retirement native, listeners и callable captures,
включая наблюдателей изменений подписки и состояния; после этого итоги и время неизменны.
Само CLOSED не является этим барьером. Сохранённый снимок не удерживает сокеты,
схемы, callback или runtime. Полный [послойный контракт](../spec/subscriptions/CLIENT-STATISTICS.ru.md).

[Конфигурация endpoint](ENDPOINT-CONFIGURATION.ru.md): явные адреса/свойства, неизменяемые снимки, эффективные локальные значения и границы совместимости Java.

## Статистика подписки

`subscription->getStatistics()` возвращает detached `SubscriptionStatistics` под
одним mutex учёта подписки. Реальные переходы collector/FIFO admission, selection,
cleanup и пользовательских вызовов обновляют этого же владельца. Чтение не выделяет
память, не выполняет IO, dispatch, reset и не ждёт callback. Пользовательский код
работает вне mutex учёта и управления. Для `ownerIdentity`, `generation`, `observedAt`
и `final` действуют описанные выше правила endpoint; чтения двух владельцев дают
разные cuts. `sourceEpoch` — последний наблюдённый установленный источник endpoint,
отсутствующий при неактивном соединении. Lifetime итоги и старые pending события
переживают смену источника без изменения внутреннего provenance. Final ждёт retirement
event, state и change-observer captures вместе с деструкторами; затем значения и время
замораживаются.

`desiredSelectors` считает принятые логические селекторы один раз независимо от типов
событий и квалифицируется `desiredRevision`. `activeSelectors` публикуется только после
полного фактического source mapping commit, с `activeDesiredRevision` и наблюдённым
`sourceEpoch`. Pending/failed mapping не объявляет следующую команду active; последующий
submit не переименовывает старый mapping candidate. Unchanged команды сохраняют
физический mapping и публикуют собственную логическую квалификацию. После фактического
clear/reset mapping и retirement active selectors ноль, active revision отсутствует.
Это локальное применение, не peer acknowledgement.
`queueCapacityEvents` — реально зарезервированные FIFO slots плюс pending slots TICKER
view; до native регистрации ноль. Это slots, не свободная ёмкость или память процесса.
`pendingEvents` включает pending значения source view и detached FIFO события;
`inFlightEvents` охватывает полный выбранный вызов адаптера, включая блокировку listener.

До насыщения выполняется lifetime равенство:

```text
admittedEvents = deliveredEvents + conflatedEvents + discardedEvents +
                 callbackFailedEvents + evictedOverflowEvents + pendingEvents + inFlightEvents
droppedEvents = rejectedOverflowEvents + evictedOverflowEvents
```

Совпадение одного события с двумя подписками даёт admission каждой. Fanout listeners
не умножает admission или delivered events. `deliveredEvents` требует нормального
завершения всей выбранной группы listeners. Отсутствие listeners, отказ projection,
source/clear fence и отмена относятся к `discardedEvents`. Исключение listener относит
весь выбранный batch один раз к `callbackFailedEvents`, даже если предыдущий listener
его видел; оставшиеся queued события discarded. Close может подавить ещё не начатые
вызовы, не объявляя dispatch завершённым.

`droppedEvents` считает только известный overflow. STREAM отклоняет входящие
per-registration FIFO candidates; TICKER может потерять уже admitted source-view
candidates как overflow victims. Замещённые pending TICKER значения относятся к
`conflatedEvents`, не dropped. Equal/filtered input и неклассифицированная потеря общего
STREAM source не превращаются в выдуманные admission/drop подписки: сохраняются
существующие gap/unknown-eligibility diagnostics для нематериализованной потери.

`callbackCalls`/`callbackFailures` считают реальные вызовы/исключения event listeners.
`callbackDurationSamples`, `callbackDurationTotal` и `callbackDurationMaximum` измеряют
завершённые вызовы steady clock в наносекундах, включая исключения и блокировки;
queue wait, state/change observers исключены. До завершения первого вызова timing имеет
`NO_SAMPLE`. Переполнение count/sum насыщает отдельное поле со sticky availability;
чтение и reconnect никогда не сбрасывают итоги.

[Серверная агрегация](SERVER-AGGREGATION.ru.md): отдельные local request и peer report, bounded descriptor updates и явный UNKNOWN.

## Наблюдения диапазонов HISTORY

Выбирайте `DeliveryContract::HISTORY` явно. Worker владеет collector, независимыми views, replay и ACK reset. `Subscription<Event>::getHistorySnapshot()` возвращает пассивный владеющий срез AS-OF; getter не выполняет I/O и не ждёт callback. Current completion требует настоящих данных о доставленном диапазоне и соответствия принятым logical/source/clear/callback fences. Order — первый поддерживаемый типизированный HISTORY route: endpoint без явного registry использует стандартную схему Order, принадлежащую SDK. Это индексируемое событие. Остальные девять бизнес-типов отклоняют HISTORY до native-регистрации. См. [поля Order, source-селекторы и владение](ORDER.ru.md). Точные состояния, конечные storage limits и сырые индексируемые координаты описаны в [контракте HISTORY](../spec/subscriptions/HISTORY-RECEIVING.ru.md).
