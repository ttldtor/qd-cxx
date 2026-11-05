# Значения time-series подписки и логические диапазоны

[English version](TIME-SERIES-SUBSCRIPTIONS.md) · [Справочник API](api.ru.md)

Подключите `qd/api/time_series_subscription.hpp` и цель `qdcxx_subscription_selectors`.
Заголовок и цель доступны независимо от опциональных Feed и транспорта. Компонент владеет
идентичностью бизнес-типов, декорированными символами и изменяемым логическим диапазоном.
Он не создаёт действующую HISTORY-подписку. У нынешних девяти native-типов нет бизнес-маркера
TimeSeriesEvent и HISTORY-получателя Feed.

```cpp
#include <qd/api/time_series_subscription.hpp>
#include <array>

namespace api = org::ttldtor::qdcxx::api;

auto type = api::TimeSeriesEventDescriptor::declared(
    u"example:MyTimeSeriesEvent", api::EventSubscriptionCapability::TIME_SERIES);
if (!type) return 1;
auto model = api::TimeSeriesSubscriptionModel::create(std::span(&*type, 1));
if (!model) return 2;
const std::array symbols{api::SubscriptionSelector::plain(u"IBM")};
if (!model->setSymbols(symbols) || !model->setFromTime(0)) return 3;
auto cut = model->snapshot();
if (!cut) return 4;
// cut owns one coherent range, raw set, decorated set and logical revision.
```

Стабильный идентификатор явно задаёт вызывающая сторона. Это не поиск Java-класса и не
регистрация схемы или декодера. Разным бизнес-типам нельзя назначать один идентификатор.
Дескриптор допускает пользовательские и унаследованные time-series семейства, без закрытого
списка Candle и рыночных имён. Отдельный запрос `hasTimeSeriesFeedReceiver` возвращает false.
`isTimeSeriesEventType` проверяет существующий native-реестр: все нынешние типы и неизвестные
значения enum дают false. Объявления INDEXED и NON_INDEXED не допускаются при создании модели.

`TimeSeriesSubscriptionSymbol<Symbol>` владеет исходным значением. Идентичность и хеш не
включают внешний `fromTime`; `sameDemand` дополнительно сравнивает диапазон и атрибуты
исходного запроса. Стандартный домен `SubscriptionSelector` сохраняет точный UTF-16,
идентичность источника и тип декоратора. Вложенные обёртки сохраняются. Пользовательские
типы символа должны обеспечивать симметричную эквивалентность и одинаковые стабильные хеши
равных значений; concept проверяет только синтаксис. Исправление типизированной native-
идентичности из [ADR-0012](decisions/ADR-0012-decorated-subscription-identity.ru.md)
действует и внутри вложенных значений.

Начальное состояние — пустой набор и `fromTime == INT64_MAX`, отключающий sentinel Java.
Все знаковые миллисекундные значения сохраняются точно, включая MIN и MAX. Изменение диапазона
не меняет идентичность исходных символов. Снимок декорирует каждый символ своим диапазоном.
`setSymbols` заменяет набор, `addSymbols` объединяет по типизированной идентичности, сохраняя
последние переданные атрибуты запроса; удаление не учитывает исключённые из идентичности атрибуты.
Перестановка равного набора ничего не меняет и сохраняет прежний порядок. Эффективное изменение
получает одну проверяемую локальную ревизию модели; это не эпоха транспорта или источника.

Конечные лимиты ограничивают входные и сохранённые элементы и единицы UTF-16. Неверные
capability, конфликтующие объявления, исчерпание ресурсов и ошибки выделения памяти возвращают
типизированный отказ. Неуспешная мутация сохраняет весь диапазон, набор и ревизию. Снимки
выделяют независимые копии и могут отказать, не меняя модель. Под единственным mutex нет
callback или I/O. Обычные конкурентные методы сериализуются; перенос и уничтожение требуют
внешнего исключения доступа. Перенесённая модель отказывает в операциях. Снимки переживают
перенос и уничтожение владельца. У модели нет close lifecycle, доставки событий, callback-
уведомлений, обещания полноты или доставки.

[Нормативный контракт](../spec/subscriptions/TIME-SERIES-IDENTITY.ru.md) описывает наблюдения
Java и отличия native. [Production oracle](../tools/upstream/time-series-oracle/README.ru.md)
запускает закреплённые классы Java, включая наследование capability и обход через raw generic.
