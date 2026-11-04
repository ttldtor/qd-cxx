# Декорированные selectors на границе приёма

[English version](DECORATED-SELECTORS.md). Английский текст является основным.

## Значения и identity

[`api::SubscriptionSelector`](../../include/qd/api/subscription_selector.hpp) владеет точным UTF-16 текстом события и встроенными kind/source/time полями. Он не удерживает имя источника, схему, listener или представление входных данных. Создание принимает пустую строку и произвольный UTF-16; активация проверяет домен выбранного приёмника. Пунктуация строки сама по себе не создаёт декоратор или wildcard.

| Фабрика | Симметричная identity | Эффективный атрибут demand |
| --- | --- | --- |
| `plain(text)` | PLAIN и точный текст | Нет |
| `indexed(text, source)` | INDEXED, точный текст и знаковый source ID | Имя источника игнорируется и не сохраняется |
| `timeSeries(text, fromTime)` | TIME_SERIES и точный текст | Знаковые Unix milliseconds, включая MAX sentinel |
| `all()` | ALL | Нет текста события; требуется явное разрешение |

`operator==` исключает TIME_SERIES `fromTime`; `sameDemand` учитывает его. Для определения изменения полного desired snapshot используется второй метод. Равные значения имеют одинаковый hash. Совместимые с Java hashes отдельных kinds могут совпадать у неравных видов, включая ALL и plain `"*"`. Native сравнение между kinds симметрично; [ADR-0012](../../docs/decisions/ADR-0012-decorated-subscription-identity.ru.md) фиксирует подтверждённое расхождение закреплённой Java и явное исправление.

Copy assignment сохраняет всё прежнее значение при allocation failure: сначала создаётся полная replacement copy, затем выполняются только noexcept swaps. Self-copy не меняет значение. Move operations noexcept; moved-from и self-moved значения остаются валидными с неопределённым текстом, без гарантии прежнего содержимого. Producer snapshots используют copy construction в новом backing и noexcept move assignment при замене элементов, сохраняя прежнюю атомарную публикацию.
## Активация в typed Feed

`Subscription::addSelectors(span)`, `setSelectors(span)` и `removeSelector(value)` копируют проверенные владеющие значения. Прежние string и CandleSymbol API остаются plain exact identities. Удаление строкой удаляет только соответствующий PLAIN. Source-specific и plain identities могут иметь общий физический demand; удаление одной сохраняет другую. Весь пакет проверяется до публикации desired revision, включая региональную представимость, текст, количество и виды. Возврат producer-вызова и `ACTIVE` не подтверждают применение backend membership.

| Selector | Quote/Profile/Trade/TradeETH/Summary Feed |
| --- | --- |
| PLAIN | Прежний exact composite/regional routing, включая symbols с атрибутами |
| INDEXED | Тот же физический market route, что у PLAIN: production MarketEventDelegateSet игнорирует source ID этих неиндексированных типов. Logical kind/source identities различаются |
| TIME_SERIES | Атомарный `UNSUPPORTED_PROFILE` в `subscription-time-series`: эти typed events и endpoint не реализуют time-series/HISTORY delivery |
| ALL | Только явные `EndpointOptions::enableWildcard=true` и STREAM; по умолчанию выключен |

Включение wildcard для TICKER отклоняется до публикации endpoint. Это возможность controlled-peer профиля: дескрипторы, acknowledgements и строка `"*"` её не включают. PLAIN текст с ведущей звездой остаётся атомарным invalid argument. `AB*` и `A?` — обычные exact строки без glob-интерпретации. Canonical DATA `*` остаётся конкретным symbol; All получает его как данные.

All выбирает все допустимые выбранные record bindings, включая реальные региональные варианты, без расширения каталога. При Exact/All overlap каждая конкретная запись доставляется один раз через прежнюю captured eligibility. Удаление All сохраняет Exact; удаление одной typed source identity сохраняет другие logical identities. Неизвестные/невыбранные виды записей, конечные входные лимиты и изоляция callback failures сохраняют прежние контракты. Физический региональный base и возвращаемый logical exchange suffix используют прежнее обратимое отображение; source ID не является exchange code.

Пример при runtime, принадлежащем главному потоку, и явно настроенном composite/regional registry:

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::STREAM;
options.enableWildcard = true;
// Assign options.eventRegistry when actual regional records are required.
auto endpoint = runtime.createEndpoint(options);
auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
const std::array selectors{
    q::api::SubscriptionSelector::indexed(u"AAPL&Q", q::events::IndexedEventSource::DEFAULT),
    q::api::SubscriptionSelector::all()};
subscription->setSelectors(selectors);
```

Подключите `<array>` и `qd/api/feed.hpp`; сохраняйте runtime на его creator thread до retirement endpoint/callback. Соединение устанавливается явно обычным Endpoint API. Этот фрагмент выбирает текущие Quote routes и не добавляет typed Order или Candle.

## Source/time demand существующих известных bindings

Независимый target `qdcxx_subscription_selectors` доступен при выключенных Feed и native модулях. `indexedSelectorRequest(selector, PreparedOrderSource)` использует уже проверенные context/source/Index binding и возвращает borrowed STREAM/TICKER request, отсутствие совпадения для другого неотрицательного source либо typed kind/context refusal. PLAIN и отрицательные explicit source IDs выбирают доступный подготовленный source set, как production source delegate dispatch. Отрицательные ID сохраняют отдельную typed identity. Helper не обнаруживает полный каталог и не выводит capability из `isPublishable`.

`SubscriptionSelector::qdTime()` выполняет только value conversion: преобразует удерживаемый fromTime milliseconds в production generic QD time-sequence value, сохраняя `Long.MAX_VALUE`. Он не утверждает record или TimeSeriesEvent business eligibility и не создаёт request. Проверенный raw indexed-time layout сам по себе не доказывает такую business capability. Lower-level tests явно составляют прежний raw SubscriptionRequest, а существующий HISTORY owner проверяет context/layout; это raw protocol capability, а не typed time-series activation.

Source adapter не выделяет память и не удерживает текст: owning selector должен жить до синхронного копирования возвращённого request в SubscriptionState/MuxQueue. Скопированный context сохраняет provenance после уничтожения схемы. Ни value conversion, ни raw request не реализуют и не разрешают будущий typed event family.
## Конечное хранение, прогресс и lifecycle

Количество symbols и units каждого symbol остаются конечными; defaults и caps16 записей/128 полей не изменены. Typed producer API проверяют не более `maximumSymbolsPerSubscription` аргументов, включая дубликаты. Удерживаемые logical identities учитываются отдельно, даже при общем физическом demand. Source ID, kind и time встроены; нет копирования имени источника или отдельной framework allocation для selector. Представимость четырёх банков проверяется до bootstrap. Они покрывают retained, parsed, candidate и conversion/retirement хранение; каждый банк запрашивает не более N wrappers и N*(U+1) UTF-16 units. Это логический requested backing, без implementation capacity, allocator overhead и process RSS.

Новая136 величина `frontendSelectorBytes` относится к одному in-flight producer mutation attempt, а не ко всему runtime. Параллельные caller attempts имеют собственные transient costs. Прежние aggregate limits core/MUX/collector/queue и сериализованного worker не ослаблены: это отдельный новый показатель, не global heap cap и не часть routing work grant. Прежний frontend также разбирал входы до port mutex. Для detached handles ранее допустимый All после retirement owner сохраняет `CLOSED`, а не превращается в неподдерживаемый profile.
Worker сохраняет ограниченный logical text и явный Exact/All tag. Региональный physical backing, borrowed requests и queue-owned copies имеют отдельные прежние лимиты. Routing byte/work reservations используют реальные размеры wrappers и оплачиваются до snapshot allocation/publication. Новый конечный turn обеспечивает прогресс; исчерпанный общий turn откладывает работу без refund. Exact/one-below prepay tests сохраняют все coordinates при отказе. Source preparation и explicit raw requests используют прежние независимо конечные owners.

Прежние selected-callback lifetime, listener/capture retirement, cancellation, remove/re-add и новые reconnect epochs сохраняются. Тесты различают обычный строгий `DRAINED/noGap/noExpiry`, именованные callback THROW/CLOSE local ABORT с причинной доставкой другим живым подписчикам и gap явного reconnect. Финальная native read cancellation после собственного cutoff фиксируется отдельно от здоровой pre-cutoff границы. Ошибки lifecycle/transport не игнорируются ради зелёного selector теста.

## Production reference и граница проверки

[Raw capture](../../tests/fixtures/selectors/oracle.tsv) и [provenance](../../tests/fixtures/selectors/provenance.json) содержат реальные закреплённые Java identity/hash, construction, delegate source/regional/wildcard selection и signed time-bound наблюдения. Raw Java асимметрия сохранена, а не переписана в corrected expectations. Focused C++ cases классифицируют только документированное отличие cross-kind identity. [Oracle launcher](../../tools/upstream/selector-oracle/README.ru.md) использует существующие production classes, а не копию реализации.

[Selector values](../../tests/conformance/subscription_selector_tests.cpp), [конечный profile](../../tests/conformance/selector_profile_tests.cpp) и [реальный Feed peer consumer](../../tests/conformance/selector_feed_tests.cpp) покрывают соответствующие границы. Peer использует opt-in один exact followup ADD/response для причинной overlap mutation, сохраняет строгие defaults и разрешает exact multiset final REMOVE в нескольких асинхронных frames. Доказательства native/compiler/package выполнения сообщаются отдельно; source coverage не означает успешное выполнение. Публичный сервис, произвольный provider, typed Order/Candle и whole-client readiness не заявлены.
