# Настраиваемые схемы Quote и Profile

[English version](CONFIGURED-EVENTS.md). Английская версия нормативна.

Этот контракт QDCXX-025 следует новым наблюдениям закреплённой Java и предшествует production C++.
[Oracle](../../tools/upstream/event-mapping-oracle/EventMappingOracle.java) использует production
`MarketFactoryImpl`, `QuoteDelegate`, `ProfileDelegate`, `FileWriter` и `FileReader`.
[Покрытие](../../tests/fixtures/events/mapping/COVERAGE.ru.md) разделяет воспроизведение файлов,
прямые пробы отображения и наблюдения конфигурации источников. Существующий профиль JSON неизменён.

## Граница разрешённой схемы

Получение источников находится вне core, protocol и адаптеров событий. Java разрешает properties
и XML; C++ получает явно проверенную неизменяемую обычную локальную схему. Совместимые записи
Quote/Profile из закреплённого `dxlink.xml` и отдельного расширения типа 10 сохраняют фактические
поля, локальные/property ключи, логический порядок и полные identities. Физические Java VOID tails
не являются логическими полями. Полные исходные Profile и Quote сохранены.
Reorder, minimal и numeric-int — отдельные программные примеры; они не заменяют defaults.
Полная схема dxlink содержит другие записи/типы и не является принимаемым профилем схемы C++.

Подготовка разрешает явные lookup keys без удаления точек, изменения регистра и вывода aliases.
Java `DefaultRecord` устанавливает полные/локальные/default property имена до создания mappings,
а специальные aliases mapping — после него. `addFieldAlias` принимает другой ключ того же поля
и отвергает ключ другого поля. Захваченный alias `ChosenPrice` разрешается в Bid.Price;
Bid.Time -> BidPrice отвергается. C++ сохраняет ограниченную проверку неоднозначности, когда
предоставленные ключи разрешаются в разные логические поля; имя записи не доказывает совместимость.
Идентичность/владение схемой и лимиты подготовки заданы в [EVENT-MAPPING](../events/EVENT-MAPPING.ru.md).

## Подмножество реестра конфигурации

Это наблюдения Java, а не C++ загрузчик properties или универсальный приоритет настроек.
Каждый сценарий запускается в новой JVM; только mutation намеренно меняет свойства после инициализации.

| Ключи / потребитель | Грамматика и default | Источники, приоритет и время чтения | Capture |
| --- | --- | --- | --- |
| `dxscheme.nanoTime`, `SchemeProperties` | Boolean; отсутствие false | Явные `dxscheme.enabled.Sequence` и `.TimeNanoPart` независимо перекрывают shortcut | default, sequence, nano, both, explicit |
| `dxscheme.enabled.<Property>` | Glob имени события; Quote включает только Quote, Profile исключает Quote | Копирование при создании SchemeProperties; явный выбор предшествует nano fallback | sequence, nano, explicit |
| `dxscheme.price`, `.size`, `.shares`, `MarketFactoryImpl` | int/wide/tiny/decimal без учёта регистра; отсутствие использует wide=true | Снимок при создании схемы; fixture явно запрашивает int | integer-representation |
| `dxscheme.wide`, `SchemeProperties` | Boolean, default true | false выбирает некоторые поля полного типа24 вне принимаемого C++ профиля | tiny-unsupported |
| Неверное `dxscheme.nanoTime` | Неверный boolean записывает ошибку в журнал и использует false | Поведение Java наблюдается, но не рекомендует молча принимать неверный C++ ввод | invalid-boolean |
| `dxfeed.properties`, FEED `DXEndpointImpl.Builder` | Явный путь локального properties-файла | Для проверенных ключей: builder withProperty > файл > JVM property; разрешение при build | endpoint-file, endpoint-api |
| `System.setProperty` / `DXFeedScheme` | Явное изменение до/после | До инициализации класса влияет на singleton; после не меняет снимок; новый явно настроенный endpoint отличается | set-before, mutation |
| XML options `DXScheme.Loader` | dxprops/sysprops по умолчанию false | resource:dxlink.xml игнорирует startup nanoTime; включённые options объединяют system, затем loader properties | xml, xml-system, xml-properties |

Properties-файл задаёт nanoTime=false и enabled.Sequence=Quote. Конфликтующие startup значения:
nanoTime=true и enabled.Sequence=Profile. Файл даёт только Sequence. API явно задаёт nanoTime=true
и enabled.Sequence=Profile: остаётся только TimeNanoPart. Mutation сохраняет исходный singleton,
но новый явно настроенный endpoint имеет оба поля. XML-properties включает options и явно выбирает
nanoTime=false/Sequence=Quote вместо startup true/Sequence=Profile. Переменные окружения, секреты
и частные файлы не используются. Aliases других настроек, повторные endpoint builds и live changes
остаются вне измеренного покрытия.

## Точные представления и отсутствие

Числовые price/size Quote и числовые проекции Profile принимают измеренные полные представления
120 и 8 с проверкой выбранного поля. Полный8 проецирует знаковое целое в double;
полный120 использует существующую семантику WideDecimal. Это локальная проекция события, а не
wire coercion: wire/local полные identities должны совпадать. Другие точные типы сохраняют свои роли.

Отсутствующее optional локальное числовое свойство даёт NaN; существующий wire-omitted полный120
имеет raw zero и также даёт NaN, а wire-omitted полный8 даёт числовой ноль. Совпадение/различие
результатов не объединяет состояния схемы. Отсутствующие строковые slots дают null; явные null
и пустая строка различаются. Отсутствующие обязательные локальные поля не дают совместимого
production event delegate. Существующее обязательное Description при wire omission может дать null.

Отдельное неподдерживаемое XML-расширение использует Java `string` полного137, а wide=false включает
полный24. Исходники сохранены как явно неподдерживаемые C++ варианты и не расширяют профиль полей.
Не добавляются новые serial identities, индексные записи, общие преобразования, XML loader,
транспорт и session semantics. Созданные файлы остаются STREAM/NONE или RAW/MESSAGE без сжатия,
с options EMPTY_SET и нулевыми QTP event flags. Business Flags Profile — независимые данные записи.
