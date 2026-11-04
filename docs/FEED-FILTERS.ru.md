# Классификация символов и событий Feed

[English version](FEED-FILTERS.md). Нормативна английская версия.

`FeedFilter` проверяет существующий тип/символ или готовое событие по упорядоченным
категориям. Подписки он не меняет. Категории используют QD expressions; побеждает первая
категория, совпавшая через любую выбранную запись. Фильтр владеет неизменяемой конфигурацией
и остаётся действительным после уничтожения builder и исходных строк.

Для существующего endpoint подготовьте упорядоченные категории. Endpoint создаёт
классификатор на своём неизменяемом registry и delivery contract:

```cpp
#include <qd/api/feed_filter.hpp>
#include <array>

const std::array categories{
    org::ttldtor::qdcxx::api::FeedFilterCategory{u"equities", u"bs"},
    org::ttldtor::qdcxx::api::FeedFilterCategory{u"other", u"*"}};
auto filter = endpoint->createFilter(categories);
const auto category = filter.getCategory(
    org::ttldtor::qdcxx::api::EventType::QUOTE,
    org::ttldtor::qdcxx::api::SubscriptionSelector::plain(u"AAPL"));
```

`category` — необязательный UTF-16 view;
для его использования сохраните владеющую копию фильтра. `accept` возвращает boolean по
тому же пути координат подписки. `acceptEvent` и `getEventCategory` выбирают publisher-
координаты по `RegisteredEvent`. Обычная строка с начальной `*` не имеет делегатов подписки;
`SubscriptionSelector::all()` — отдельный wildcard-запрос. Текущие поддерживаемые типы —
Quote, Profile, Trade, TradeETH, Summary, TextMessage и TextConfiguration; у них нет
time-series-делегатов. Indexed wrapper не создаёт новых возможностей типа или источника.

Для advanced offline use есть `FeedFilter::Builder(registry, contract, limits)` и
`withCategory(name, expression)`. Registry владеет полными проверенными объявлениями
записей, поэтому используются реально настроенные имена и региональные маршруты.
Нет ambient process registry или setter фильтра подписки.

## Выражения

`!`, `&`, `,` и скобки задают NOT, AND, OR и группировку. Начальный `:` сопоставляет имена
записей. Неизвестные токены используют QD glob с классами и максимум двумя несмежными `*`.
Это не regex: `?`, `**`, третья `*` и неэкранированная начальная строчная буква запрещены.
Например, `:Quote*&[A-Z]*` сочетает критерии записи и символа; `[a]bc` экранирует первую
строчную букву.

Стандартные токены: `feed`, `compfeed`, `regfeed`, `chartdata`, `convrates`, `nwc`,
`hashMofN`, `range-A-B-` и семейства `bs`, `fut`, `opt`, `spread`, `cs`, `csopt`, `bsopt`,
`futopt`, `ind`, `prod`, `fx`. Суффикс `symbol` убирает неявное ограничение записей `feed`.
[Полный контракт](../spec/subscriptions/FEED-FILTERS.ru.md) описывает классы, UTF-16,
зарезервированные символы, range/hash и QD ALL projection. Принятие wildcard-
запроса не подтверждает получение или отображение события.

## IPF-ресурсы и обновления

Для IPF-выражения нужен явный resource context. Например:

```text
ipf[file:/data/instruments.ipf,update=1m]
```

Создайте один `FeedFilterIpfRuntime` на время приложения/endpoint с существующим
pre-thread `RuntimeBootstrap`, полными transfer settings и конечными лимитами. Передайте
`sources->context()` в `endpoint->createFilter(categories, context)` либо builder через
`withResourceContext`. Builder разрешает отсутствующие IPF-ресурсы с настроенным initial
deadline. Один session владеет worker и общей остановкой; клиент не запускает, не ждёт
и не опрашивает каждое выражение отдельно.

`update`, `schedule`, `user`, `password` сохраняют QD textual semantics. Адрес поддерживает
QD escaping. Отсутствующий или нулевой update period создаёт статический ресурс.
Schedule требует явных clock и schedule-query capability. Пустой schedule использует
TRADING_HOURS загруженных профилей, непустой — указанное описание. Отсутствующая optional
source/schedule capability даёт detached error; настройка не игнорируется. Credentials
не включаются в диагностику.

Каждое успешное полное чтение публикует неизменяемую замену. FLUSH и COMPLETE не публикуют
частичный фильтр; ошибочный хвост после COMPLETE отклоняется. При существующей default
warning policy чистый EOF без COMPLETE допустим. Равные множества символов и глобальные множества TRADING_HOURS сохраняют поколение;
порядок, дубликаты и привязка hours к отдельным символам не влияют.
Ошибка обновления сохраняет предыдущий успешный snapshot.

`filter.getTracker()` возвращает стабильный handle. Добавьте listener для ongoing tracking
и получайте обновления через `tracker.getCurrentFilter()`. Первое добавление догоняет
текущее состояние без initial callback. Без listeners current filter может устареть.
Удалённый listener может завершить один уже выбранный вызов. Исключения изолированы,
`callbackFailures()` показывает их число. Удаляйте ненужные tokens: retained captures и
старые поколения потребляют конечные lifetime allowances.

Сохранённый фильтр не меняется после обновления; его запросы не запускают I/O. Owning
session закрывается/join один раз на создающем потоке; source handles и immutable snapshots
могут пережить его как detached values. Creator owners нельзя уничтожать или join из
worker callback. Callback может удалить tracker listener, запросить source close и
освободить обычные filter/tracker handles.

## Ошибки и установка

Пустые/повторные имена, пустые выражения и неверная грамматика отклоняются до возврата
фильтра. Конечные лимиты категорий, текста, узлов/глубины, символов, координат, поколений,
listeners, источников, работы и времени не используют ноль как unlimited sentinel.
Отказ замены не инвалидирует старые snapshot. Public `Error` содержит статические stages
и числовые категории, без текста peer и credentials.

Подключите installed target `qdcxx_feed`. Статическая классификация доступна при OFF
optional IPF transfer backend; создание source runtime тогда сообщает об отсутствующей
capability. Для source-backed приложения включите существующий IPF transfer backend и
его prerequisites. [Feed API](FEED-API.ru.md) описывает владение endpoint/runtime;
[native filter contract](../spec/subscriptions/FEED-FILTERS.ru.md) — границы совместимости
и production-oracle evidence.
