# Подписки на несколько зарегистрированных типов событий

[English](MIXED-SUBSCRIPTIONS.md). Английская версия является основной.

## Выбор типов и публикация

`Feed::createSubscription(std::span<const EventType>)` и перегрузка с initializer-list
возвращают одну `Subscription<RegisteredEvent>`. `RegisteredEvent` — это
`std::variant<events::Quote, events::Profile, events::Trade, events::TradeETH, events::Summary, events::TextMessage, events::TextConfiguration, events::Message, events::Configuration>`. Существующие конкретные
`createSubscription<events::Quote>()` и `createSubscription<events::Profile>()`
сохраняют типы обработчиков и поведение.

Создание копирует и нормализует полный набор типов. Повторы объединяются; порядок
входных типов не задаёт порядок доставки. После возврата вызывающий код может
изменить или уничтожить вход. Выбранный набор фиксирован до закрытия подписки.
Зарегистрированные local record routes каждого выбранного типа участвуют с учётом
[регионального exchange-выбора Quote](EVENT-TIME.ru.md).
[Неизменяемый реестр](EVENT-REGISTRY.ru.md) сохраняет полную идентичность схемы,
плотные local ID, параметры mapping и generic records. Имена записей и wire sender
ID не выбирают тип C++.

Число исходных аргументов ограничено положительной настройкой runtime
`maximumEventTypeArgumentsPerSubscription` с инженерным значением по умолчанию 64.
Она ограничивает число просмотренных элементов и работу, включая повторы, независимо
от числа поддержанных типов. `{QUOTE, PROFILE, QUOTE}` принимается в этих пределах.
То же значение остаётся осмысленным при добавлении типов.

Поддержаны QUOTE, PROFILE, TRADE, TRADE_ETH, SUMMARY, TEXT_MESSAGE, TEXT_CONFIGURATION, MESSAGE и CONFIGURATION. Выбор TEXT_MESSAGE или MESSAGE требует STREAM. Неизвестные enumerators явно отклоняются.

Порядок проверок:

1. Слишком длинный исходный span отклоняется с `RESOURCE_LIMIT` на
   `subscription-event-type-arguments` до просмотра элементов.
2. Пустой вход или неизвестный enumerator отклоняется с `INVALID_ARGUMENT` на
   `subscription-event-types`.
3. Закрытый endpoint/runtime отклоняет admission с существующей категорией `CLOSED`.
4. Отсутствие любого выбранного поддержанного типа в реестре endpoint даёт
   `UNSUPPORTED_PROFILE` на `subscription-unregistered-event`.
5. Исчерпание ёмкости подписок даёт `RESOURCE_LIMIT`.

Ошибка выделения памяти может независимо отклонить создание. Отказ не публикует
подписку, не удерживает slot и не отправляет частичный desired selection. Принятый
набор занимает один slot подписки и один общий набор символов, независимо от числа
типов событий.

## Исполнение, доставка и время жизни

Рабочий поток открывает один существующий receiver и одну регистрацию multiplexer
для полного span выбранных маршрутов. Desired additions/removals содержат по одному
interest на символ и совпадающую выбранную local record с физической региональной
базой символа, где это применимо. Перекрывающиеся конкретные и смешанные
подписки делят upstream interest через существующий multiplexer. Закрытие или очистка
одной отзывает только interests, больше не нужные другим активным подпискам.

Каждый принятый batch receiver полностью проецируется во владеющие variant values
до вызова обработчиков приложения. Отказ projection идёт по существующему пути
ошибки подписки; частичный проецированный batch не доставляется. Порядок и границы
batch следуют выбранному существующему контракту TICKER/STREAM. Вызов обработчика
может содержать не все типы; сетевой кадр не задаёт границу callback. Дополнительного
порядка между записями нет.

Span обработчика действителен только во время вызова. Копии `RegisteredEvent`
владеют строками и значениями событий и пригодны после завершения регистрации,
endpoint и runtime. Копии, удерживаемые приложением, не входят в резервы библиотеки.

Сериализация рабочего потока, время жизни выбранных обработчиков, завершение captures
вне control locks, изоляция исключений, ожидания в создающем потоке и отделённые
закрытые handles совпадают с конкретными подписками. Исключение обработчика завершает
только его подписку с `CALLBACK_FAILURE`; перекрывающиеся подписки могут продолжать
работу. Ожидание закрытия endpoint устанавливает завершение callbacks и captures.

## Конечные резервы receiving

Подготовка профиля резервирует полное число зарегистрированных маршрутов для каждой
возможной подписки. Это позволяет создавать подписки после `connect()` без роста
native reservations. При `S` максимальных подписках, `N` максимальных символах
подписки и `R` маршрутах desired capacity подписки равна `N * max(1, R)`, endpoint —
`S * N * max(1, R)`, с проверкой арифметики и соответствующими пределами symbol units.
Поэтому встроенный реестр по умолчанию резервирует девять возможных interest на символ.

Полные mixed mapping limits, visits подготовки с проверкой повторов маршрутов и
пределы callback projection суммируются до активации. Пределы receiving schema
остаются 16 записей, 128 полей суммарно, 256 lookups, 128 UTF-16 units на имя и 16384 units
всех имён. Больший generic registry может быть допустим, но получать явный
`UNSUPPORTED_PROFILE` до native activation. Эти пределы не обещают одновременное
размещение будущего полного каталога типов.

## Первичный источник совместимости

Закреплённый QD3.355
[DXFeed.createSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeed.java)
создаёт и присоединяет одну подписку для массива классов.
[DXFeedSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedSubscription.java)
отклоняет пустой массив и хранит неизменяемый набор с объединёнными повторами.
Владеющий native variant выражает типобезопасную доставку для девяти поддержанных
prepared adapters; он не добавляет ABI произвольных классов приложения, reflection,
wire messages, source families или schema loaders.

Trade входит в RegisteredEvent и typed createSubscription<events::Trade>(). Поддержанные selections имеют конечные plans; fixed511 selection slots выбирают непоследовательные native kinds без лишних record routes. Региональный fallback Trade независим от Quote, см. [Trade](TRADE.ru.md). Прежние callback/ownership/error guarantees сохраняются.

TradeETH регистрируется отдельно как EventType::TRADE_ETH; см. [TradeETH](TRADE-ETH.ru.md). Raw ETH bit и отсутствие Tick fallback отличают его от обычного Trade.
