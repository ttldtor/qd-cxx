# MarketMaker и производные источники Order

[English version](MARKET-MAKER-ORDER.md). Английский текст является определяющим.

Подключите `qd/api/feed.hpp` и свяжите `qdcxx::feed`. Стандартными схемами, collectors, выбором
источников и завершением callbacks владеет SDK. MarketMaker — индексное событие без бизнес-возможности
TimeSeriesEvent: наличие indexed layout само по себе её не предоставляет.

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::ApiRuntime runtime;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::HISTORY;
auto endpoint = runtime.createEndpoint(options);
auto makers = endpoint->getFeed()->createSubscription<q::events::MarketMaker>();
makers->addEventListener([](std::span<const q::events::MarketMaker> batch) {
    for (const auto &maker : batch) {
        // Copy a value when retaining it after this callback.
        const auto bid = maker.getBidPrice();
        const auto count = maker.getBidCount();
        (void) bid;
        (void) count;
    }
});
makers->addSymbols("AAPL");
```

Владеющий DTO сохраняет indexed flags, независимый FIELD eventTime, полные миллисекунды обеих сторон,
четыре wide-значения и их прямые business-double представления, знаковые counts. Публичный index:
char16 exchange в high32 и unsigned MMID в low32; source DEFAULT0. Исходный raw64 индекс collector
сохраняется отдельно. Поэтому REMOVE может иметь другой полный ключ collector при том же суженном
публичном index. Nullable maker-текст декодируется из текущего low32 без удержания исходной записи.

TICKER endpoint поддерживает Order, производные от composite Quote и всех реальных региональных
Quote&A..Z. Явный HISTORY поддерживает обычный Order и aggregate Order от MarketMaker.
STREAM publication/wildcard delegates не дают разрешения обычного приёма. Стандартный endpoint
по-прежнему использует TICKER.

```cpp
auto endpoint = runtime.createEndpoint();
auto orders = endpoint->getFeed()->createSubscription<q::events::Order>();
const auto composite = q::api::SubscriptionSelector::indexed(
    u"AAPL", q::events::OrderSource::COMPOSITE.indexedSource());
orders->setSelectors(std::array{composite});
```

| Физическая запись | Раздельные bid / ask sources | Unitary source | Контракт приёма |
| --- | --- | --- | --- |
| Quote | COMPOSITE_BID1 / COMPOSITE_ASK2 | COMPOSITE7 | TICKER |
| Quote&A..Z | REGIONAL_BID3 / REGIONAL_ASK4 | REGIONAL8 | TICKER |
| MarketMaker | AGGREGATE_BID5 / AGGREGATE_ASK6 | AGGREGATE9 | HISTORY |

Plain и отрицательные sources по умолчанию выбирают обе раздельные стороны. Неизменяемая настройка
`EndpointOptions::unitaryOrderSources=true` выбирает unitary-пару. Явные source IDs независимо выбирают
соответствующие delegates; source0 выбирает обычный native Order, если он зарегистрирован. Raw Quote
сохраняет market source selection. Raw MarketMaker принимает plain/negative/default0 и исключает
положительные IDs. All требует существующего явного STREAM-разрешения и не меняет контракт delegates.

Производный Order сохраняет буквальный QD symbol, включая имеющийся суффикс. Raw regional Quote
сохраняет прежнее преобразование логического/физического суффикса. Bid идёт перед ask. Unitary bid
ставит TX_PENDING и снимает END/SNIP; ask снимает BEGIN. Раздельный Quote Order имеет flags0,
раздельный aggregate Order сохраняет cursor flags. Sequence и nano remainder производного Order
начинаются с нуля, поля Quote Sequence/TimeNanoPart игнорируются. Время стороны использует production
Order packing с насыщением signed32 seconds. Exchange должен помещаться в ASCII после char16 cast.
Отрицательный MMID сохраняет signed-декодирование: до восьми byte-valued units в производном Order
и до четырёх в raw MarketMaker.

Логические selectors объединяют физический спрос. Физическая строка даёт максимум один native output
и четыре производных. Каждый captured output хранит исходную eligibility; withdrawal снимает только
его ещё не выбранные contributions. Remove/readd не оживляет старое событие FIFO. Расширение source
cache повторно выдаёт всю текущую delegate-группу и conflate только прежнюю source-owned dirty-группу.
Переданные FIFO-записи сохраняют исходные identities. Выбранный custom-executor packet неизменяем
до фактического завершения.

Delivery statistics считают бизнес-результаты: pending, in-flight, delivered, discarded, conflated
и ошибки. QueueCounts и transport сохраняют единицу физической записи. Unitary-пары проецируются
атомарно. Четыре физических строки callback дают максимум двадцать owning outputs. Полные text/body/
fixed credits оплачиваются до выделения outputs; listener не получает частичную группу. Несколько
listeners не умножают event counters.

SDK TICKER catalog содержит35 физических записей,225 логических полей,607 lookup keys и62 native/
derived declarations. Receiving bounds:42 records, 284 fields, 724 lookups,128 units на имя,16384 суммарно,
84 совместимых маршрута. На физическую запись допускается максимум один native и одна совместимая
derived family. Публичный девятимаршрутный `EventRegistry::builtIn` и стандартные caller limits
сохраняются. Расширенный проверенный каталог предоставляет SDK. Стандартные8 подписок×16 symbols
резервируют максимум 4480 физических request slots. Native regional Quote убирает exchange suffix,
а derived Order сохраняет literal symbol: один context может требовать двух физических keys.
Checked capacity учитывает максимальную same-exchange regional pair bucket. Custom 42-context paired
profile допускает 84 coordinate domains и 10752 aggregate slots. Registry сохраняет потолок 8 MiB.
Packet/pool credits настраиваются через
существующий [callback execution](CALLBACK-EXECUTION.ru.md).

[Production oracle](../tools/upstream/market-maker-order-oracle/README.ru.md) сохраняет исходные полные
types/slots, оба default source режима, nano-поля, все32 flags и full64 removals. Native tests сравнивают
owning mappings и реальные QTP bytes. [Обычный Order](ORDER.ru.md) описывает native qualified sources,
[Feed API](FEED-API.ru.md) — lifecycle и наблюдения ranges.
Protocol text seed заранее покрывает общий64-poll worker burst через проверенные полные descriptor/schema scans; credits не пополняются при retry. Прежняя snapshot arena64KiB сохраняется. Проверяется полный35-record prefix и exact/one-under text-credit boundary.
