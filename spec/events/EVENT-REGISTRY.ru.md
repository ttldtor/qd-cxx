# Неизменяемый реестр принимаемых событий

[English version](EVENT-REGISTRY.md). Английская версия является основной.

## Граница и владение

`api::EventRegistry` владеет полной неизменяемой `core::LocalSchema` и явными
маршрутами `EventRegistration`. Он не разрешает свойства процесса, окружение,
файлы, providers или wire descriptions. Создание не выполняет native-работу,
сетевые операции или callbacks приложения. Реестр относится к опциональному
компоненту `qdcxx_feed`; core, protocol и адаптеры событий не получают зависимости
от транспорта.

`create` копирует валидированные объявления в заданных пределах и сохраняет их
точную идентичность схемы. Копия реестра разделяет неизменяемое хранилище без
аллокации. Исходная схема и контейнеры регистрации могут исчезнуть сразу после
успеха. Независимо созданная равная схема имеет другую идентичность и не может
помечать значения для mapping, подготовленного из этого реестра. Перемещение
передаёт владение. Для перемещённого исходного значения допустимы только `valid()`,
`storageBytes()`, уничтожение и присваивание; `storageBytes()` возвращает для него ноль.

Const-наблюдение реестра допускается одновременно из разных потоков. Возвращаемые
ссылки на схему и spans маршрутов заимствуют общее неизменяемое хранилище и требуют
живой владеющей копии реестра. Реестр не удерживает callable или captures приложения.

## Явные mappings

`EventType::QUOTE`, `EventType::PROFILE`, `EventType::TRADE`, `EventType::TRADE_ETH`, `EventType::SUMMARY`, `EventType::TEXT_MESSAGE` и `EventType::TEXT_CONFIGURATION`, `EventType::MESSAGE` и `EventType::CONFIGURATION` выбирают существующие prepared adapters.
Имя записи не определяет тип события. Каждый маршрут задаёт явный local record ID,
конечные `MappingLimits` и `EventTimePolicy`. Сохраняется требование `LocalSchema`:
каждый ID равен позиции записи в полном каталоге.

Разные записи могут проецироваться в один тип события. На физическую запись допускаются максимум один native route и одна совместимая явная EventProjection family. Два native любого business type, две derived groups либо несовместимая пара отклоняются атомарно. Quote совместим с QUOTE_ORDERS, MarketMaker — с MARKET_MAKER_ORDERS.
Наблюдение маршрутов сохраняет порядок входа. Подписка feed на один тип выбирает
зарегистрированные маршруты этого типа с учётом
[региональных exchange-правил Quote](EVENT-TIME.ru.md); несколько выбранных типов используют
[контракт смешанной подписки](MIXED-SUBSCRIPTIONS.ru.md).

Подготовка использует существующие точные допустимые lookup keys, полные
альтернативы типов, обязательные/опциональные поля и бизнес-значения по умолчанию
адаптеров. Явные lookup aliases вызывающего кода могут связывать поддержанный
business key с любой совместимой позицией поля. Все дополнительные поля и
незарегистрированные generic records сохраняются в удерживаемой схеме; проекция
не заменяет полное generic-представление. Пользовательский wire alias может быть
первым lookup поля, а поддержанный business alias — выбирать это же поле.
Псевдонимы, преобразования и произвольные C++-классы событий не выводятся автоматически.

Исчерпывающая [таблица преобразований полей](FIELD-CONVERSIONS.ru.md) принадлежит каждому
подготовленному адаптеру. Default registration допускает 128 точных альтернатив типов и
консервативно оплачивает сравнения; Quote требует72, Profile108, Trade110, TradeETH99 и Summary75. Явные меньшие границы
по-прежнему дают атомарный отказ. Полные receive caps составляют42 записи и284 поля; публичный builtIn сохраняет caller limits.

`FIELD_METADATA` выбирает фактический исходный FIELD event time при его наличии;
`UNSPECIFIED` не утверждает timestamp события. Ни одна политика не использует
indexed identity как время. Пределы владения символом/текстом каждого результата
остаются явно выведенными пределами receiving profile, отдельно от подготовки регистрации.

## Атомарные ошибки и конечные пределы

`EventRegistryLimits` ограничивает полную удерживаемую схему, число маршрутов и
удерживаемые schema/container/shared allocation requests в байтах.
`storageBytes()` сообщает тот же консервативный объём, включая capacity контейнеров
и фактический запрос shared state/control. Аллокатор отклоняет превышающий предел
полный запрос до аллокации. Финальная деаллокация не обращается к локальному учёту
создания. Накладные расходы аллокатора, входы вызывающего кода, независимо созданные
копии и RSS процесса не входят
в эту байтовую координату. Пределы схемы/числа элементов независимо ограничивают
все элементы реестра и временные планы адаптеров.

`EventRegistryWork` предоставляет visits и консервативные кредиты подготовки mapper.
Visits покрывают попытки копирования элементов/единиц имён схемы и проверки
маршрутов/дубликатов. Перед каждой попыткой адаптера списывается проверенная сумма
объявленных координат `MappingLimits`. Кредиты неуспешной попытки не возвращаются;
создание не добавляет их внутри. Ноль — действительный предел. Сложения и
умножения проверяют переполнение.

Ошибка не публикует реестр и сохраняет входы вызывающего кода. Результат удерживает
исходный `SchemaError`/`MappingError` либо отдельную ошибку реестра invalid-input,
resource, work или allocation. Ошибка маршрута содержит его входную позицию.
Неизвестные event/time enums, недопустимые ID, повторные local ID, перемещённая
исходная схема, отсутствие обязательных keys и несовместимость точных типов
отклоняются до публикации. Создание реестра преобразует исключения аллокации.

## Значения по умолчанию и пределы receiving

`builtIn` использует ту же границу регистрации: полный default Quote record0
с восемью полями, Profile record1 с шестнадцатью, composite Trade record2 с тринадцатью полями,
composite TradeETH record3 с двенадцатью, Summary record4 с десятью полями, TextMessage record5 с тремя,
TextConfiguration record6 с четырьмя, Message record7 с одним SERIAL_OBJECT и Configuration record8
с Version и SERIAL_OBJECT, с существующими lookup
aliases и бизнес-поведением. Endpoint без явного реестра выбирает SDK-owned каталог своего receiving contract; состав описан ниже.
Глобальной регистрации процесса и изменения живого реестра нет.

Высокоуровневый plain receiving profile сохраняет квалифицированный предел полной
схемы:42 записи,284 поля суммарно,724 lookup entries, 128 UTF-16 units на имя
и16384 units всех имён. Реестр может быть допустим при больших явно выбранных
пределах и всё же отклоняться этим receiving profile на `profile-schema`, до
native activation. Это явное ограничение receiving, а не усечение или отклонение
общей границей схемы.

В этих пределах подготовка сохраняет точную идентичность схемы и группирует
маршруты по типу. Она проверяет и суммирует координаты полной формы mapping,
selections/work, пределы callback projection и ёмкости desired commands.
Каждый принятый символ требует одного interest на distinct matching physical key. Native/derived overlap объединяет bits до полного desired submit. Native regional Quote использует suffix-free physical symbol, derived Order сохраняет literal symbol; один context может требовать двух keys. Checked capacity равна max(registered contexts, eligible contexts + наибольшая same-exchange native/derived regional bucket) с тем же contract guard. Generic unregistered records её не увеличивают. Default 35 domains дают 560 slots на подписку/4480 на 8 подписок; custom 42 paired contexts — до 84 domains и 1344/10752 slots, без изменения schema ceiling 42. Максимальная полная форма записи задаёт field allowance кадра с прежним минимумом16. Четыре физических callback rows дают максимум20 owning outputs, включая полную оплату group metadata/projection.

Региональная маршрутизация удерживает84 exchange/source slots и42 optional version gates. Конечные2047 selection plans с максимум84 indices каждый находятся в одном owning vector; его фактическая capacity/storage проверяются и оплачиваются до publication, без большого объекта на worker stack.
Каждая desired revision дополнительно владеет логическим
снимком и не более чем одной физической строкой без суффикса на логический символ.
Совпадающие региональные маршруты используют эту общую физическую строку; Profile
или composite fallback могут одновременно использовать исходную логическую копию.

Для максимального числа символов `N`, максимальных units символа `U` и числа
зарегистрированных маршрутов `R` отдельный временный резерв включает `2*N*U`
текстовых units, `N` routing wrappers, `N*R` заимствующих request wrappers и
завершители строк. Символы, удерживаемые frontend, и владеющие копии команд очереди
сохраняют прежние отдельные пределы. Это логическое хранилище без неиспользованной ёмкости,
выбранной реализацией, allocator overhead и RSS. Helper резервирует полную итоговую строку
до копирования заимствованных prefix/attribute slices, поэтому growing-prefix allocation
не перекрывается с итоговым физическим буфером. Временное хранилище и освобождение списываются до
выделения снимка. Routing work содержит `N*(2*R+8)+R` visits, `11*N*U` проверяемых/
копируемых текстовых units и удвоенное временное хранилище плюс32 байта массива
exchange в byte work. Дополнительные grants endpoint для каждой логической
подписки выводятся с проверкой сумм/произведений до публикации. Новый допустимый
grant всегда покрывает routing attempt вместе с прежним submission allowance;
переполнение отклоняется на `profile-arithmetic`. Исчерпание текущих turn credits
откладывает revision без выделения памяти, изменения состояния и внутреннего
пополнения. Неудачные попытки не возвращают потраченное.

Строковые request views создаются после полного заполнения временного вектора
selectors и живут до конца синхронного `handle.replace`, который владеет успешной
копией команды. Эти views не переходят в captures слушателей или reconnect state.

Текст команд, которым владеет очередь, отдельно резервирует `N*R*(U+1)` units,
включая один завершитель каждого символа максимальной длины. Завершители не уменьшают
публичный предел128 печатных ASCII units. Непредставимый резерв отклоняется проверкой
арифметики до выделения памяти.

## Пример пользовательской схемы

Вызывающий код предоставляет валидированный плотный каталог с независимой generic
record0 и Quote-совместимой record1 с именем `ConfiguredQuote`. Порядок полей может
отличаться; допустимы дополнительные значения и явные поддержанные business aliases:

```cpp
const std::array routes{api::EventRegistration{api::EventType::QUOTE, 1}};
api::EventRegistryWork work;
auto registry = api::EventRegistry::create(schema, routes, {}, work);
```

Схема не зависит от loader. Её могут предоставлять существующий native JSON или
другие source adapters; создание реестра не требует loader. См.
[schema-bound mapping](EVENT-MAPPING.ru.md) и [ADR-0003](../../docs/decisions/ADR-0003-configurable-schemas-and-event-mappings.ru.md).

## Граница доказательств совместимости

В закреплённой Java QD перегрузки [DXFeed.createSubscription](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeed.java)
для одного класса и массива классов отделяют выбор типов от attachment.
[EventDelegateFactory.createDelegates(DataRecord)](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/EventDelegateFactory.java)
задаёт границу mapping для записи. Native-реестр воспроизводит явный schema-bound
выбор с существующими измеренными адаптерами; он не утверждает поддержку Java
reflection, произвольных delegate factories, всех классов событий или новых
wire semantics. Закреплённый reference и существующая provenance адаптеров находятся
в `upstream.lock` и [доказательствах configured events](../../tests/fixtures/events/mapping/COVERAGE.ru.md).

Trade добавлен как явный EventType::TRADE; см. [полный контракт](TRADE.ru.md). Finite stack copy84 bindings и её initialization/read/write work включены в preparation. Quote/Trade/TradeETH/Summary выбирают regional fallback независимо; Profile сохраняет exact logical symbol, derived Order — literal physical symbol.

TradeETH регистрируется отдельно как EventType::TRADE_ETH; см. [TradeETH](TRADE-ETH.ru.md). Raw ETH bit и отсутствие Tick fallback отличают его от обычного Trade.

TextMessage и TextConfiguration сохраняют точные nonmarket-символы. Поведение STREAM и TICKER описано в [контракте](TEXT-EVENTS.ru.md).

SDK-owned TICKER catalog:35 физических записей,225 полей,607 lookups,62 native/derived routes. HISTORY: native Order, native MarketMaker и его derived group; STREAM сохраняет9 native declarations. Контракт разрешается отдельно от регистрации. Physical overlap объединяет bits до desired submit; резерв учитывает distinct coordinate domains, включая native regional suffix-free/derived literal split и наибольшую same-exchange bucket: default 35→560/4480 slots, custom 42 paired contexts→до 84 domains/1344/10752 slots. Generic unregistered records его не увеличивают. Четыре физических callback rows дают максимум20 owning outputs с полным prepaid budget. См. [MarketMaker и derived Order](../../docs/MARKET-MAKER-ORDER.ru.md).
