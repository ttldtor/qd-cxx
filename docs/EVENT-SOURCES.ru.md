# Использование источников индексированных событий и заявок

[English](EVENT-SOURCES.md). Английская версия является нормативной.

Подключите независимую цель qdcxx_event_sources без SDK либо qdcxx::feed,
которая экспортирует её как публичную зависимость.
~~~cmake
find_package(qdcxx CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE qdcxx_event_sources)
~~~

~~~cpp
#include <qd/events/order_source.hpp>

namespace events = org::ttldtor::qdcxx::events;
auto source = events::OrderSource::valueOf(u"NTV");
auto restored = events::OrderSource::valueOf(source.id());
bool same = restored == source; // true; ID-only equality
events::IndexedEventSource base(source.id(), std::nullopt);
bool sameFamilyIndependent = base == source; // true
~~~
Имена используют точный UTF-16 с учётом регистра. Базовые значения принимают
любой знаковый ID и nullable-имя, которым владеют. Источники заявок сначала
разрешают встроенные значения, затем проверяют имя из одной–четырёх ASCII-букв
или цифр. Null и некорректные входы вызывают std::invalid_argument;
выделение памяти может вызвать стандартные ошибки строк/аллокации.
[Полный контракт](../spec/events/EVENT-SOURCES.ru.md) задаёт проверку ID.

Значения владеют именами. Представления имён и ссылки indexedSource() заимствуют
владельца до присваивания, перемещения из него или уничтожения.
Идентичность ссылок Java не обещается. hashCode() возвращает знаковый ID;
std::hash преобразует его в size_t. Строчные имена Java получают константы с
суффиксом _LOWER, например NTV_LOWER для ntv. ID источника отличается от кода биржи
и физического маршрута. API не добавляет маршрутизацию подписок или отображение записей Order.

## Проверка возможностей и выбор источника

~~~cpp
#include <qd/events/order_source.hpp>
#include <qd/events/source_selection.hpp>

namespace events = org::ttldtor::qdcxx::events;
auto publishable = events::OrderSource::publishable(events::OrderEventType::ORDER);
bool fullBook = events::OrderSource::NTV.isFullOrderBook();
auto all = events::SourceSelection::all();
auto defaultOnly = events::SourceSelection::specific(events::IndexedEventSource::DEFAULT);
auto custom = events::SourceSelection::specific(events::IndexedEventSource(-1, std::nullopt));
bool acceptsCustom = all.matches(*custom.source()); // true
bool same = all == defaultOnly; // false
~~~

[Матрица возможностей и контракт селектора](../spec/events/EVENT-SOURCES.ru.md#возможности-публикации-и-выбор-источника)
задают шесть семейств, порядок списков и ошибки. Конкретный селектор владеет
источником, допускает произвольные base ID и сравнивает только ID. source()
заимствует владельца до присваивания, move-from или уничтожения; nullptr означает
ALL. Флаги публикации не ограничивают выбор. Этот чистый API значений не создаёт
подписки, не принимает Order и не интегрирует выбор в Feed.
