# Публичный API

[English](api.md) · [Оглавление справочника](../CONFIGURATION-REFERENCE.ru.md)

Контракты потребителей отдельных полей и сведения деклараций/исходников с оговорённой доказательностью явно различаются ниже. НЕИЗВЕСТНО обозначает недостающее сведение. Известные объявленные сведения сохраняются; их семантическая проверка ещё не завершена. Ссылки указывают декларацию или потребителя и не означают нового запуска тестов. Значения описывают текущий API; принятые изменения перечислены в [архитектурном контракте](../CONFIGURATION.ru.md).

- [EndpointConfigurationLimits](api.ru.md#cfg-0001-owner-ru) — `include/qd/api/endpoint_configuration.hpp`
- [EventRegistryLimits](api.ru.md#cfg-0005-owner-ru) — `include/qd/api/event_registry.hpp`
- [EventRegistryWork](api.ru.md#cfg-0008-owner-ru) — `include/qd/api/event_registry.hpp`
- [FeedFilterContextLimits](api.ru.md#cfg-0022-owner-ru) — `include/qd/api/feed_filter.hpp`
- [FeedFilterIpfLimits](api.ru.md#cfg-0032-owner-ru) — `include/qd/api/feed_filter.hpp`
- [FeedFilterLimits](api.ru.md#cfg-0010-owner-ru) — `include/qd/api/feed_filter.hpp`
- [BasicTlsCredentials](api.ru.md#cfg-0065-owner-ru) — `include/qd/api/feed.hpp`
- [EndpointOptions](api.ru.md#cfg-0080-owner-ru) — `include/qd/api/feed.hpp`
- [HistoryReceivingOptions](api.ru.md#cfg-0058-owner-ru) — `include/qd/api/feed.hpp`
- [HistorySnapshotOptions](api.ru.md#cfg-0109-owner-ru) — `include/qd/api/feed.hpp`
- [RuntimeOptions](api.ru.md#cfg-0042-owner-ru) — `include/qd/api/feed.hpp`
- [TapeSourceOptions](api.ru.md#cfg-0103-owner-ru) — `include/qd/api/feed.hpp`
- [VerifiedTlsOptions](api.ru.md#cfg-0070-owner-ru) — `include/qd/api/feed.hpp`
- [IndependentSubscriptionLimits](api.ru.md#cfg-0110-owner-ru) — `include/qd/api/independent_subscription.hpp`
- [IndexedEventModelLimits](api.ru.md#cfg-0113-owner-ru) — `include/qd/api/indexed_event_model.hpp`
- [OrderBookCorrectionOptions](api.ru.md#cfg-0122-owner-ru) — `include/qd/api/order_book_model.hpp`
- [OrderBookViewOptions](api.ru.md#cfg-0118-owner-ru) — `include/qd/api/order_book_model.hpp`
- [PublisherCatalogOptions](api.ru.md#cfg-0129-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherDemandPolicy](api.ru.md#cfg-0131-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherOptions](api.ru.md#cfg-0145-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherPeerDemandPolicy](api.ru.md#cfg-0134-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherRecordLimits](api.ru.md#cfg-0136-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherSourceOptions](api.ru.md#cfg-0126-owner-ru) — `include/qd/api/publisher.hpp`
- [PublisherStreamByteLimits](api.ru.md#cfg-0141-owner-ru) — `include/qd/api/publisher.hpp`
- [TimeSeriesModelLimits](api.ru.md#cfg-0157-owner-ru) — `include/qd/api/time_series_subscription.hpp`

## EndpointConfigurationLimits {#cfg-0001-owner-ru}

[include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp)

### `EndpointConfigurationLimits::properties` — CFG-0001 {#cfg-0001-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные свойства во всех слоях, включая затенённые
- Допустимые значения: Положительное значение. Ноль вызывает INVALID_ARGUMENT до чтения входа. Число входных записей &lt;= properties; каждый дубликат расходует запись. Отдельного численного потолка сверху в проверке нет; ограничения keyBytes/valueBytes/totalBytes действуют одновременно.
- Момент чтения: EndpointConfiguration::create: проверка нуля и последовательное применение значений по умолчанию, явных свойств и блоков адреса.
- Изменяемость и время жизни: Лимиты задаются одним вызовом EndpointConfiguration::create и расходуются только InputEnvelope. Результат сохраняет разрешённые EndpointOptions и снимок эффективных свойств; четыре входных лимита в State не сохраняются. Следующий create может передать другие лимиты.
- Ошибка: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits при нуле; RESOURCE_LIMIT / endpoint-property-input при превышении. Некорректный ключ/значение получает отдельную ошибку; bad_alloc при создании переводится в ALLOCATION_FAILURE / endpoint-configuration-storage.
- Исходники потребителя: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::keyBytes` — CFG-0002 {#cfg-0002-ru}

- Тип: `std::size_t`. Исходное значение: `= 96`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты одного ключа
- Допустимые значения: Положительное значение; ноль отвергается. Длина каждого ключа &lt;= keyBytes и доступного totalBytes; пустой либо непечатный ключ отдельно вызывает INVALID_ARGUMENT. Фиксированного верхнего потолка лимита нет.
- Момент чтения: EndpointConfiguration::create: проверка нуля и последовательное применение значений по умолчанию, явных свойств и блоков адреса.
- Изменяемость и время жизни: Лимиты задаются одним вызовом EndpointConfiguration::create и расходуются только InputEnvelope. Результат сохраняет разрешённые EndpointOptions и снимок эффективных свойств; четыре входных лимита в State не сохраняются. Следующий create может передать другие лимиты.
- Ошибка: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits при нуле; RESOURCE_LIMIT / endpoint-property-input при превышении. Некорректный ключ/значение получает отдельную ошибку; bad_alloc при создании переводится в ALLOCATION_FAILURE / endpoint-configuration-storage.
- Исходники потребителя: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::valueBytes` — CFG-0003 {#cfg-0003-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты одного значения свойства, включая строку адреса
- Допустимые значения: Положительное значение; ноль отвергается. Значение может быть пустым, если его потребитель допускает это; длина &lt;= valueBytes и остатка totalBytes. Адрес далее проверяется собственной грамматикой и пределами. Численного верхнего потолка лимита нет.
- Момент чтения: EndpointConfiguration::create: проверка нуля и последовательное применение значений по умолчанию, явных свойств и блоков адреса.
- Изменяемость и время жизни: Лимиты задаются одним вызовом EndpointConfiguration::create и расходуются только InputEnvelope. Результат сохраняет разрешённые EndpointOptions и снимок эффективных свойств; четыре входных лимита в State не сохраняются. Следующий create может передать другие лимиты.
- Ошибка: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits при нуле; RESOURCE_LIMIT / endpoint-property-input при превышении. Некорректный ключ/значение получает отдельную ошибку; bad_alloc при создании переводится в ALLOCATION_FAILURE / endpoint-configuration-storage.
- Исходники потребителя: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

### `EndpointConfigurationLimits::totalBytes` — CFG-0004 {#cfg-0004-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные байты ключей и значений всех рассмотренных свойств
- Допустимые значения: Положительное значение; ноль отвергается. Каждый ключ и значение должны помещаться в оставшиеся totalBytes; затенённые значения также расходуют бюджет. Вычитание выполняется до сложения. Дополнительного верхнего потолка нет; возможность выделить фактическую память проверяется отдельно.
- Момент чтения: EndpointConfiguration::create: проверка нуля и последовательное применение значений по умолчанию, явных свойств и блоков адреса.
- Изменяемость и время жизни: Лимиты задаются одним вызовом EndpointConfiguration::create и расходуются только InputEnvelope. Результат сохраняет разрешённые EndpointOptions и снимок эффективных свойств; четыре входных лимита в State не сохраняются. Следующий create может передать другие лимиты.
- Ошибка: api::Error: INVALID_ARGUMENT / endpoint-configuration-limits при нуле; RESOURCE_LIMIT / endpoint-property-input при превышении. Некорректный ключ/значение получает отдельную ошибку; bad_alloc при создании переводится в ALLOCATION_FAILURE / endpoint-configuration-storage.
- Исходники потребителя: [include/qd/api/endpoint_configuration.hpp](../../include/qd/api/endpoint_configuration.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp).

## EventRegistryLimits {#cfg-0005-owner-ru}

[include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp)

### `EventRegistryLimits::schema` — CFG-0005 {#cfg-0005-ru}

- Тип: `core::SchemaLimits`. Исходное значение: `{16, 128, 256, 128, 16384}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Полная удерживаемая схема, включая generic-записи.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;EventRegistry, EventRegistryFailure&gt; create(const core::LocalSchema &amp;schema, std::span&lt;const EventRegistration&gt; registrations, EventRegistryLimits limits, EventRegistryWork &amp;work) / [[nodiscard]] static std::expected&lt;EventRegistry, EventRegistryFailure&gt; builtIn(EventRegistryLimits limits, EventRegistryWork &amp;work)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: EventRegistryError; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `EventRegistryError`.

### `EventRegistryLimits::registrations` — CFG-0006 {#cfg-0006-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: регистрации типизированных маршрутов
- Допустимые значения: Ноль допускает только пустой список регистраций; положительное значение ограничивает registrations.size(). Отдельного верхнего потолка нет. Даже пустой реестр должен поместить схему и состояние в storageBytes; work и лимиты схемы действуют отдельно.
- Момент чтения: EventRegistry::create: проверяется до копии схемы и повторно при расчёте удерживаемого состояния.
- Изменяемость и время жизни: EventRegistryLimits передаются одному create по значению. Реестр сохраняет схему, регистрации, versionFields и измеренный storageBytes; запрошенные потолки не становятся политикой последующих операций. Новый create может использовать другой допуск.
- Ошибка: std::expected с EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT,index?} при превышении/переполнении; отдельные ошибочные схема/маршрут и выделение имеют собственные причины.
- Исходники потребителя: [src/api/event_registry.cpp](../../src/api/event_registry.cpp), [include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp).

### `EventRegistryLimits::storageBytes` — CFG-0007 {#cfg-0007-ru}

- Тип: `std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: учитываемые байты удерживаемой схемы, маршрутов, их ёмкостей и общего состояния
- Допустимые значения: Ноль является реальным бюджетом и не позволяет создать непустое состояние. Требуется минимум для State, схемы, регистраций и их массивов; затем учитываются фактические ёмкости и блок allocate_shared. Проверки сложения/умножения могут отказать. Единого постоянного минимума/максимума нет: они зависят от схемы и регистраций.
- Момент чтения: EventRegistry::create: проверяется до копии схемы и повторно при расчёте удерживаемого состояния.
- Изменяемость и время жизни: EventRegistryLimits передаются одному create по значению. Реестр сохраняет схему, регистрации, versionFields и измеренный storageBytes; запрошенные потолки не становятся политикой последующих операций. Новый create может использовать другой допуск.
- Ошибка: std::expected с EventRegistryFailure{EventRegistryError::RESOURCE_LIMIT,index?} при превышении/переполнении; отдельные ошибочные схема/маршрут и выделение имеют собственные причины.
- Исходники потребителя: [src/api/event_registry.cpp](../../src/api/event_registry.cpp), [include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp).

## EventRegistryWork {#cfg-0008-owner-ru}

[include/qd/api/event_registry.hpp](../../include/qd/api/event_registry.hpp)

### `EventRegistryWork::visits` — CFG-0008 {#cfg-0008-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Просмотры элементов схемы, единиц имён, маршрутов и дубликатов.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `EventRegistryError`.

### `EventRegistryWork::preparation` — CFG-0009 {#cfg-0009-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000000`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Сумма объявленных ограничений счётчиков, единиц и сравнений каждого испробованного mapper.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `EventRegistryError`.

## FeedFilterContextLimits {#cfg-0022-owner-ru}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterContextLimits::expressionUnits` — CFG-0022 {#cfg-0022-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 токена IPF
- Допустимые значения: Ноль допускается при создании контекста; любой непустой корректный токен IPF превысит его. Длина полного выражения &lt;= expressionUnits; грамматика проверяется отдельно.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-ipf-config-units. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_ipf_config.cpp](../../src/api/feed_filter_ipf_config.cpp).

### `FeedFilterContextLimits::resources` — CFG-0023 {#cfg-0023-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: зарегистрированные ресурсы контекста
- Допустимые значения: Ноль запрещает добавление нового ресурса; замена уже найденного ресурса не расходует новый слот. Число ресурсов &lt;= resources. Эта величина также входит в расчёт работы обновления.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-resource-count. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::symbols` — CFG-0024 {#cfg-0024-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: символы либо описания расписания во входном обновлении
- Допустимые значения: Символы и метаданные расписания проверяются отдельными счётчик &lt;= symbols. Ноль допускает пустые входы. До удаления дубликатов проверяется исходное число; общий размер ограничен totalSymbolUnits.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-resource-metadata-count либо filter-resource-symbols. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::symbolUnits` — CFG-0025 {#cfg-0025-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 одного символа
- Допустимые значения: Каждый символ должен иметь длину &lt;= symbolUnits; ноль разрешает только пустые символы. Для описаний расписания отдельная проверка длины этим полем не выполняется; они входят в totalSymbolUnits.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-resource-symbol-units. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::totalSymbolUnits` — CFG-0026 {#cfg-0026-ru}

- Тип: `std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 символов и описаний расписания
- Допустимые значения: Сумма исходных длин &lt;= totalSymbolUnits, сложение проверяется. Ноль допускает лишь нулевую сумму; ограничения числа и work продолжают действовать.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-resource-total-units; переполнение учётной арифметики RESOURCE_LIMIT / filter-storage. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::snapshots` — CFG-0027 {#cfg-0027-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно удерживаемые поколения ресурса/фильтра
- Допустимые значения: Ноль допускается при create, но первая reserveGeneration получает отказ. Каждое удерживаемое поколение занимает слот до уничтожения списание; общие байты ограничиваются storageBytes.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-live-snapshots. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::storageBytes` — CFG-0028 {#cfg-0028-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные расчётные байты живых поколений
- Допустимые значения: Поколение должно целиком помещаться вместе с прежними живыми поколениями. Ноль запрещает любое положительное удержание. Проверяется bytes &lt;= maximumBytes и previous &lt;= maximumBytes-bytes.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-live-storage. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::trackers` — CFG-0029 {#cfg-0029-ru}

- Тип: `std::size_t`. Исходное значение: `= 32`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые отслеживающие объекты контекста
- Допустимые значения: Ноль запрещает build с присоединённым контекстом. Перед проверкой удаляются истёкшие weak_ptr; число оставшихся объектов отслеживания &lt;= trackers. reserve может раньше отказать при чрезмерной величине.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-context-trackers; length_error в build → RESOURCE_LIMIT / filter-build-capacity. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::listeners` — CFG-0030 {#cfg-0030-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые слушатели одного динамического объекта отслеживания
- Допустимые значения: Ноль запрещает регистрацию слушателя динамического объекта отслеживания. Для статического объекта addListener возвращает 0 без регистрации. Исчерпание идентификаторов uint64_t также запрещает регистрацию. build использует величину для reserve.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: RESOURCE_LIMIT / filter-tracker-listeners; выделение ListenerNode → ALLOCATION_FAILURE / filter-tracker-listener-allocation. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterContextLimits::updateWork` — CFG-0031 {#cfg-0031-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: совокупные единицы работы обновлений/перекомпиляции
- Допустимые значения: Начальный remainingWork=updateWork, общего пополнения нет. Ноль обычно отвергает первое обновление из-за положительной цены контроля. Цена зависит от выражения, resources/expressionUnits, числа/длин символов и перестройки живых фильтров.
- Момент чтения: FeedFilterContext::create сохраняет лимиты без проверки чисел. replaceResourceSymbols, build и добавление слушателя потребляют их при работе; remainingWork расходуется за жизнь контекста.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error: WORK_LIMIT / filter-resource-work либо filter-resource-rebuild-work; нормализация может дать WORK_LIMIT / filter-expression. Аллокация контекста: ALLOCATION_FAILURE / filter-context-allocation.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

## FeedFilterIpfLimits {#cfg-0032-owner-ru}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterIpfLimits::schedulerInterval` — CFG-0032 {#cfg-0032-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{10}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Внутренний интервал активации listener; строго положительный.
- Единицы: миллисекунды
- Объявленные ограничения: Внутренний интервал активации listener; строго положительный.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::unique_ptr&lt;FeedFilterIpfRuntime&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const ipf::IpfTransferOptions &amp;transfer, FeedFilterIpfLimits limits = {}, FeedFilterIpfSchedule schedule = {}, FeedFilterContextLimits contextLimits = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp); точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `FeedFilterIpfLimits::schedulerTurns` — CFG-0033 {#cfg-0033-ru}

- Тип: `std::size_t`. Исходное значение: `= 10000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: обороты общего планировщика
- Допустимые значения: Строго &gt;0. Рабочий поток завершает работу при schedulerTurns &gt;= лимита; каждая итерация увеличивает счётчик. Лимит общий для всех источников владельца.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: WORK_LIMIT / filter-ipf-owner-lifetime-work. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::sources` — CFG-0034 {#cfg-0034-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: регистрации источников IPF
- Допустимые значения: Строго &gt;0. ports.size() должно быть &lt; sources перед добавлением нового источника; существующую регистрацию можно переиспользовать по правилам add. reserve зависит от ёмкости контейнера и памяти.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: RESOURCE_LIMIT / filter-ipf-source-registrations. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::initialTimeout` — CFG-0035 {#cfg-0035-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{300000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды ожидания начального снимка
- Допустимые значения: Строго положительная длительность. Дедлайн=min(дедлайн владельца, now+initialTimeout), сложение насыщается. Это общий дедлайн ожидания набора источников; ноль и отрицательное значение запрещены.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: INVALID_STATE / filter-ipf-initial-snapshot-deadline либо сохранённая диагностика источника. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::attempts` — CFG-0036 {#cfg-0036-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные попытки владельца
- Допустимые значения: Строго &gt;0. Попытки всех источников расходуют общий attempts; рабочий поток прекращается при attempts &gt;= лимита. Счётчики отдельного источника наблюдаются отдельно.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: WORK_LIMIT / filter-ipf-owner-lifetime-work. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::expressionUnits` — CFG-0037 {#cfg-0037-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 полного токена IPF
- Допустимые значения: Строго &gt;0. add передаёт лимит parseFilterIpfConfig; полный токен должен быть &lt;= expressionUnits и удовлетворять грамматике. Верхний потолок отдельно не задан.
- Момент чтения: FeedFilterIpfRuntime::create проверяет положительное значение. Control сохраняет его; каждый add передаёт expressionUnits в parseFilterIpfConfig до разбора выражения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Ноль: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. Превышение длины полного выражения: api::Error RESOURCE_LIMIT / filter-ipf-config-units непосредственно из parseFilterIpfConfig.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp), [src/api/feed_filter_ipf_config.cpp](../../src/api/feed_filter_ipf_config.cpp).

### `FeedFilterIpfLimits::snapshotSymbols` — CFG-0038 {#cfg-0038-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: символы одного копируемого снимка
- Допустимые значения: Строго &gt;0. Перед добавлением требуется symbols.size()&lt;snapshotSymbols. Ограничения snapshotUnits и copyWork действуют одновременно.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: IpfTransferError::RESOURCE_LIMIT / filter-ipf-snapshot-symbols. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::snapshotUnits` — CFG-0039 {#cfg-0039-ru}

- Тип: `std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 символов и расписаний снимка
- Допустимые значения: Строго &gt;0. Символы и расписания должны последовательно помещаться в общий остаток snapshotUnits; проверки вычитания предотвращают переполнение. Непустой символ может исчерпать его до добавления расписания.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: IpfTransferError::RESOURCE_LIMIT / filter-ipf-snapshot-symbols либо filter-ipf-snapshot-schedules. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::copyWork` — CFG-0040 {#cfg-0040-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные посещения/кодовые единицы копирования за жизнь владельца
- Допустимые значения: Строго &gt;0. До копирования списывается одна единица на запись и длина символа/расписания; copyWork накапливает расход. Проверяются amount&lt;=limit и spent&lt;=limit-amount.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: IpfTransferError::WORK_LIMIT / filter-ipf-snapshot-copy-work. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

### `FeedFilterIpfLimits::lifetime` — CFG-0041 {#cfg-0041-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{86400000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды времени жизни владельца
- Допустимые значения: Строго положительная длительность. Дедлайн вычисляется при создании Control сложением со steady_clock с насыщением. Рабочий поток и контрольные точки проверяют его; ожидание начального снимка также ограничено этим дедлайном.
- Момент чтения: FeedFilterIpfRuntime::create сначала проверяет положительность. Лимиты копируются в Control; рабочий поток и копирование снимка расходуют их за жизнь владельца.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевое/неположительное значение: api::Error INVALID_ARGUMENT / filter-ipf-runtime-limits. При потреблении: IpfTransferError::DEADLINE / filter-ipf-owner-lifetime; рабочий поток: WORK_LIMIT / filter-ipf-owner-lifetime-work. Результат передачи отображается transferDiagnostic в api::Error/сохранённую диагностику.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter_ipf_service.cpp](../../src/api/feed_filter_ipf_service.cpp).

## FeedFilterLimits {#cfg-0010-owner-ru}

[include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp)

### `FeedFilterLimits::categories` — CFG-0010 {#cfg-0010-ru}

- Тип: `std::size_t`. Исходное значение: `= 32`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: категории
- Допустимые значения: Ноль сохраняется в Builder, но запрещает первое withCategory; число категорий ограничено categories. build без категорий отдельно отвергается. Отдельного верхнего потолка нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-category-limit.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::categoryNameUnits` — CFG-0011 {#cfg-0011-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имени категории
- Допустимые значения: Имя должно быть непустым, уникальным и длиной &lt;= categoryNameUnits. Ноль не позволяет добавить категорию. Максимум лимита отдельно не задан.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-category-limit.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::expressionUnits` — CFG-0012 {#cfg-0012-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 выражения
- Допустимые значения: Непустое выражение должно иметь длину &lt;= expressionUnits; ноль исключает допустимую категорию. Те же лимиты передаются компилятору. Верхний потолок отдельно не задан.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-category-limit / filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::expressionNodes` — CFG-0013 {#cfg-0013-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: узлы синтаксического дерева выражения
- Допустимые значения: Ноль отказывает при создании первого узла; каждый узел увеличивает счётчик до expressionNodes. Связаны expressionDepth, storageBytes и normalizationWork. Фиксированного верхнего потолка нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::expressionDepth` — CFG-0014 {#cfg-0014-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: уровни глубины разбора/дерева
- Допустимые значения: Непустое выражение начинает разбор с глубины 1, поэтому expressionDepth=0 его отвергает; Builder не допускает пустое выражение. Глубина &lt;= expressionDepth проверяется также после нормализации. Отдельного предела нативного стека нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::patternPositions` — CFG-0015 {#cfg-0015-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: позиции автомата шаблона
- Допустимые значения: Каждая создаваемая позиция расходует patternPositions; ноль отказывает при первой позиции. Формы без создаваемых позиций определяются грамматикой. Отдельного максимума нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::finiteSymbols` — CFG-0016 {#cfg-0016-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: символы конечного множества нормализации
- Допустимые значения: Остаток finiteSymbols уменьшается при построении конечных множеств. Ноль запрещает расход хотя бы одного символа, но не все выражения требуют конечного множества. Верхний предел отдельно не задан.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::normalizationWork` — CFG-0017 {#cfg-0017-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: единицы работы нормализации
- Допустимые значения: Начальный остаток normalizationWork; для контекста ограничивается также общим remainingWork. Ноль допускает только операции без списания. Перерасход имеет WORK_LIMIT; потолка сверху нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с WORK_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-expression.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_expression.cpp](../../src/api/feed_filter_expression.cpp).

### `FeedFilterLimits::symbolUnits` — CFG-0018 {#cfg-0018-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 символа координаты
- Допустимые значения: Длина символа &lt;= symbolUnits при типизированной и универсальной классификации. Ноль допускает только пустой символ там, где грамматика маршрута его разрешает. Длина имён записей ограничена отдельно.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-symbol-units / filter-record-limit.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_coordinates.cpp](../../src/api/feed_filter_coordinates.cpp).

### `FeedFilterLimits::coordinates` — CFG-0019 {#cfg-0019-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: физические координаты одного запроса
- Допустимые значения: Ноль запрещает координату Candle и первую выбранную координату обычного маршрута. Число выбранных координат &lt;= coordinates; исходные маршруты/схема ограничивают фактическое число.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-coordinate-count.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp), [src/api/feed_filter_coordinates.cpp](../../src/api/feed_filter_coordinates.cpp).

### `FeedFilterLimits::storageBytes` — CFG-0020 {#cfg-0020-ru}

- Тип: `std::size_t`. Исходное значение: `= 16 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: расчётные удерживаемые байты фильтра
- Допустимые значения: Проверяется формула sizeof(State)+категории+UTF-16-тексты+2048*(длина выражения+1) по каждой категории. Ноль не вмещает состояние. Сложение/умножение проверяются; численного верхнего потолка нет.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-storage.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

### `FeedFilterLimits::recordNameUnits` — CFG-0021 {#cfg-0021-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имени универсальной записи
- Допустимые значения: acceptRecord требует непустое имя длиной &lt;= recordNameUnits; ноль исключает любой допустимый вызов. Верхний предел отдельно не задан.
- Момент чтения: Builder захватывает лимиты; withCategory и build проверяют вход, компилируют выражения и публикуют неизменяемый фильтр. accept использует сохранённые лимиты координат.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error с RESOURCE_LIMIT и меткой конкретной операции; ошибки синтаксиса выражения получают INVALID_ARGUMENT. bad_alloc при вызове build переводится в ALLOCATION_FAILURE; length_error в RESOURCE_LIMIT. Метка отказа этого условия: filter-record-limit.
- Исходники потребителя: [include/qd/api/feed_filter.hpp](../../include/qd/api/feed_filter.hpp), [src/api/feed_filter.cpp](../../src/api/feed_filter.cpp).

## BasicTlsCredentials {#cfg-0065-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `BasicTlsCredentials::MINIMUM_TOKEN_UNITS` — CFG-0065 {#cfg-0065-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 7`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Префикс схемы и непустое значение.
- Единицы: единицы определяются контрактом поля; UTF-16 там, где это указано
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::MAXIMUM_TOKEN_UNITS` — CFG-0066 {#cfg-0066-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 1024`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Помещается в квалифицированный управляющий бюджет Feed.
- Единицы: единицы определяются контрактом поля; UTF-16 там, где это указано
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::provider` — CFG-0067 {#cfg-0067-ru}

- Тип: `std::shared_ptr<client::CredentialProvider>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: разделяемое владение поставщиком Basic
- Допустимые значения: Если credentials отсутствует, поставщик не нужен. При наличии credentials provider должен быть ненулевым; shared_ptr удерживается в скопированной политике, вызов происходит после проверки TLS на рабочем потоке.
- Момент чтения: EndpointConfiguration::create проверяет параметры TLS верхнего API до копирования и I/O; подготовка принимающего профиля переносит данные, активация создаёт среду выполнения TLS.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-tls-credentials. При неподдержанном профиле tlsRequirements → UNSUPPORTED_PROFILE; без TLS-сборки UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `BasicTlsCredentials::timeout` — CFG-0068 {#cfg-0068-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительный кооперативный срок аутентификации после проверки.
- Единицы: миллисекунды
- Объявленные ограничения: Положительный кооперативный срок аутентификации после проверки.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `BasicTlsCredentials::tokenUnits` — CFG-0069 {#cfg-0069-ru}

- Тип: `std::size_t`. Исходное значение: `= MAXIMUM_TOKEN_UNITS`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 полного токена Basic
- Допустимые значения: Если credentials задан, 7..1024 включительно; учитывается префикс Basic и непустое значение. При отсутствии credentials не читается. Значение далее увеличивает протокольный бюджет control/token.
- Момент чтения: EndpointConfiguration::create проверяет параметры TLS верхнего API до копирования и I/O; подготовка принимающего профиля переносит данные, активация создаёт среду выполнения TLS.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-tls-credentials. При неподдержанном профиле tlsRequirements → UNSUPPORTED_PROFILE; без TLS-сборки UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

## EndpointOptions {#cfg-0080-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `EndpointOptions::MAXIMUM_BATCH_RECORDS` — CFG-0080 {#cfg-0080-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 4`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Квалифицированный предел физических записей callback.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::MAXIMUM_PENDING_RECORDS` — CFG-0081 {#cfg-0081-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 128`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Квалифицированный предел слотов отделённых записей.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::DEFAULT_RECEIVE_MESSAGE_BYTES` — CFG-0082 {#cfg-0082-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 8192`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Стандартный бюджет тела входящего QTP.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::MAXIMUM_RECEIVE_MESSAGE_BYTES` — CFG-0083 {#cfg-0083-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 65536`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Наибольший поддержанный бюджет тела входящего QTP.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::contract` — CFG-0084 {#cfg-0084-ru}

- Тип: `DeliveryContract`. Исходное значение: `= DeliveryContract::TICKER`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: значение DeliveryContract
- Допустимые значения: TICKER, STREAM, HISTORY или MIXED. Любое иное значение перечисления отвергается. Wildcard разрешён только STREAM; MIXED несовместим с явным eventRegistry и tape.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. Подготовка принимающего тракта выбирает семейство коллектора; MIXED строит три согласованных тракта.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options; UNSUPPORTED_PROFILE / endpoint-wildcard-contract либо endpoint-mixed-registry. Другие связанные поля проверяются до активации. При последующем Endpoint::connectTape на MIXED: UNSUPPORTED_PROFILE / endpoint-mixed-tape.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::connectTimeout` — CFG-0085 {#cfg-0085-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды попытки подключения
- Допустимые значения: 1..INT_MAX включительно; ноль/отрицательное запрещены. Целая нативная попытка имеет этот дедлайн.
- Момент чтения: Профиль передаёт connectTimeout в транспорт. При численном адресе start устанавливает connectDeadline; при имени сначала запускается dnsDeadline, затем успешное разрешение устанавливает новый connectDeadline при входе в CONNECTING.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/transport/runtime_core.cpp](../../src/transport/runtime_core.cpp).

### `EndpointOptions::dnsTimeout` — CFG-0086 {#cfg-0086-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды DNS-поиска
- Допустимые значения: 1..INT_MAX включительно; применяются к двухсемейному поиску в используемом DNS-профиле.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane копирует dnsTimeout в соответствующее поле settings.timeouts.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::readTimeout` — CFG-0087 {#cfg-0087-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{30000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды отсутствия входного продвижения
- Допустимые значения: 1..INT_MAX включительно. Положительная длительность используется как срок допустимого отсутствия продвижения в фазе чтения.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane копирует readTimeout в соответствующее поле settings.timeouts.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::writeTimeout` — CFG-0088 {#cfg-0088-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды отсутствия продвижения ожидающего выхода
- Допустимые значения: 1..INT_MAX включительно. Применяется только при ожидающем исходящем выводе.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane копирует writeTimeout в соответствующее поле settings.timeouts.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::pollInterval` — CFG-0089 {#cfg-0089-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{1}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды шага рабочий поток
- Допустимые значения: 1..INT_MAX включительно. Ноль запрещён; интервал задаёт продвижение общего рабочий поток.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane копирует pollInterval в соответствующее поле settings.timeouts.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::retryDelay` — CFG-0090 {#cfg-0090-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{250}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды задержки перед повторной попыткой
- Допустимые значения: При maximumAttempts&gt;1:1..INT_MAX. При maximumAttempts=1 исходное значение не проверяется как положительное и эффективная задержка нормализуется в 0; исходная запрошенная политика сохраняется отдельно.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane формирует settings.retry; при одной попытке эффективная задержка равна 0.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options при повторных попытках. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::drainTimeout` — CFG-0091 {#cfg-0091-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{1000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды локального завершения принятого префикса
- Допустимые значения: 1..INT_MAX включительно. По истечении срока завершения с доставкой разрешение эскалирует к прерыванию. Этот срок сам по себе не подтверждает доставку удалённой стороне.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. publishEndpointCutoff фиксирует admitted+drainTimeout один раз при принятии закрытия; тот же период используется в переходах источников.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::maximumAttempts` — CFG-0092 {#cfg-0092-ru}

- Тип: `std::uint32_t`. Исходное значение: `= 3`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: попытки подключения, включая первую
- Допустимые значения: Любое представимое ненулевое uint32_t без меньшего постоянного потолка. Значение 1 отключает повторы и приводит к эффективному retryDelay=0; при &gt;1 требуется положительный retryDelay&lt;=INT_MAX.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. prepareReceivingLane формирует settings.retry; при одной попытке эффективная задержка равна 0.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::eventRegistry` — CFG-0093 {#cfg-0093-ru}

- Тип: `std::optional<EventRegistry>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный владеющий EventRegistry
- Допустимые значения: nullopt выбирает встроенный реестр; присутствующий объект должен быть valid(). Для MIXED явный реестр запрещён. Это копия неизменяемого реестра, связанная с его схемой.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. selectRegistry выбирает заданный valid EventRegistry либо строит реестр по умолчанию; сохранённый реестр используется при привязке событий.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-registry; UNSUPPORTED_PROFILE / endpoint-mixed-registry. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::enableWildcard` — CFG-0094 {#cfg-0094-ru}

- Тип: `bool`. Исходное значение: `= false`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явное разрешение STREAM All для контролируемого peer; текст и peer никогда его не включают.
- Единицы: булево значение
- Объявленные ограничения: Объявленная булева область: false/true; ограничения совместимости профиля рассматриваются отдельно и независимо не перепроверены.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::requestedAggregationPeriod` — CFG-0095 {#cfg-0095-ru}

- Тип: `std::optional<std::chrono::milliseconds>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательные миллисекунды запроса серверной агрегации
- Допустимые значения: nullopt запрашивает стандарт сервера;0 означает отсутствие агрегации;1..86400000 включительно задают интервал. Отрицательное/большее значение запрещено. Наблюдаемый серверный интервал хранится отдельно.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. ApiRuntime копирует начальное намерение в EndpointPort; метод установки меняет его ревизию, а рабочий поток передаёт текущее значение recovery.requestAggregationPeriod.
- Изменяемость и время жизни: Начальная политика копируется при создании; текущее намерение агрегации меняется отдельным методом установки с ревизией и повторной проверкой диапазона.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-aggregation-period. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::history` — CFG-0096 {#cfg-0096-ru}

- Тип: `HistoryReceivingOptions`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Используется только явным HISTORY-контрактом; не является рабочим кредитом приложения.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::unitaryOrderSources` — CFG-0097 {#cfg-0097-ru}

- Тип: `bool`. Исходное значение: `= false`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: При true источники SDK default/plain/negative используют унитарные пары bid/ask.
- Единицы: булево значение
- Объявленные ограничения: Объявленная булева область: false/true; ограничения совместимости профиля рассматриваются отдельно и независимо не перепроверены.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::batchRecords` — CFG-0098 {#cfg-0098-ru}

- Тип: `std::size_t`. Исходное значение: `= MAXIMUM_BATCH_RECORDS`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: физические записи одного выбора обработчик
- Допустимые значения: 1..4 включительно и &lt;=pendingRecords. Это квалифицированный предел выбора целых физических записей; диапазон протокола из него не следует.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. Подготовка профиля переносит batchRecords в число физических записей выбора клиента, routeRows для сетевого STREAM и квант обработки в QueueLimits.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::pendingRecords` — CFG-0099 {#cfg-0099-ru}

- Тип: `std::size_t`. Исходное значение: `= MAXIMUM_PENDING_RECORDS`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: слоты отделённых записей подписки
- Допустимые значения: 1..128 включительно, &gt;=batchRecords. Задаёт очередь подписки; временное удержание/байты проверяются отдельно.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. Подготовка профиля переносит pendingRecords в очередь отделённых записей клиента и общий резерв clients*pendingRecords; размер результата HISTORY учитывает его отдельно.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-options. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp).

### `EndpointOptions::aggregationPeriod` — CFG-0100 {#cfg-0100-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{0}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды локальной доставки
- Допустимые значения: 0..86400000 включительно;0 выбирает немедленную доставку. Начальное значение EndpointOptions и отдельное изменение текущей агрегации проверяются независимо.
- Момент чтения: EndpointConfiguration::create проверяет исходную политику до создания конечной точки и I/O; конкретный потребитель указан ниже. Создание подписки сохраняет неизменяемую нижнюю границу EndpointOptions.aggregationPeriod в её статистике. Отдельный метод установки подписки выбирает собственное необязательное значение; поле конечной точки при этом не меняется.
- Изменяемость и время жизни: Значение EndpointOptions неизменяемо в созданной конечной точке. Каждая подписка может отдельно установить/сбросить свой aggregationPeriod через метод установки с проверкой; это не изменение поля конечной точки.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-local-aggregation-period. Другие связанные поля проверяются до активации.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `EndpointOptions::tls` — CFG-0101 {#cfg-0101-ru}

- Тип: `std::optional<VerifiedTlsOptions>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явный проверяемый сетевой транспорт. Отсутствие выбирает plain TCP.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `EndpointOptions::maximumReceiveMessageBytes` — CFG-0102 {#cfg-0102-ru}

- Тип: `std::size_t`. Исходное значение: `= DEFAULT_RECEIVE_MESSAGE_BYTES`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Байты типа и payload входящего QTP, 8192..65536, без префикса длины.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Байты типа и payload входящего QTP, 8192..65536, без префикса длины.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

## HistoryReceivingOptions {#cfg-0058-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `HistoryReceivingOptions::maximumPoints` — CFG-0058 {#cfg-0058-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: общие индексированные точки
- Допустимые значения: Ноль не запрещён валидатором EndpointOptions; points=0 передаётся HistoryCollector и не даёт удержать точку. Верхний предел зависит от проверяемых формул хранения и квадратичной предоплаченной работы.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение соответствующих производных сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic. maximumPoints влияет на запрошенную память коллектора; недостаточный residentBytes даёт HistoryError::RESOURCE_LIMIT при создании. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumFrames` — CFG-0059 {#cfg-0059-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: удерживаемые исходные кадры
- Допустимые значения: Ноль не отвергается локально. Значение задаёт cacheFrames и pendingFrames каждого представления; нужен свободный кадр для удержания нового владельца. Общие слоты вычисляются как cacheFrames+views*pendingFrames+1 с проверкой переполнения.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение соответствующих производных сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic. maximumFrames влияет на запрошенную память коллектора; недостаточный residentBytes даёт HistoryError::RESOURCE_LIMIT при создании. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumBytes` — CFG-0060 {#cfg-0060-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: пиковые байты исходных кадров кэша
- Допустимые значения: Ноль допустим как пустой бюджет. Для HISTORY проверяется maximumBytes+clients*pendingBytesPerSubscription&lt;=maximumResidentBytes; отдельные входные владельцы должны целиком поместиться в cacheBytes.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение проверяемых сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic; превышение общей разрешённой памяти даёт RESOURCE_LIMIT / profile-history-resident-capacity. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumResidentBytes` — CFG-0061 {#cfg-0061-ru}

- Тип: `std::size_t`. Исходное значение: `= 128 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты постоянного состояния коллектора и пикового удержания
- Допустимые значения: Ноль не вмещает реальное состояние. Бюджет должен покрывать фиксированное хранение вместе с maximumBytes+clients*pendingBytesPerSubscription; до аллокаций проверяется расчётный размер, после — реальные ёмкости.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение проверяемых сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic; превышение общей разрешённой памяти даёт RESOURCE_LIMIT / profile-history-resident-capacity. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295).
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::pendingUpdatesPerSubscription` — CFG-0062 {#cfg-0062-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: элементы FIFO обновлений одного представления
- Допустимые значения: Ноль не запрещён EndpointOptions и создаёт нулевой pendingItems; первая требующая FIFO доставка не может быть поставлена. Сумма maximumPoints+pendingUpdatesPerSubscription+pendingRecords и произведения по views проверяются.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение соответствующих производных сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic. pendingUpdatesPerSubscription влияет на запрошенную память коллектора; недостаточный residentBytes даёт HistoryError::RESOURCE_LIMIT при создании. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295). Переполнение FIFO конкретного представления записывает локальный HistoryViewFailure с RESOURCE_LIMIT и порядковым номером отказавшей записи.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::maximumObservationBytes` — CFG-0063 {#cfg-0063-ru}

- Тип: `std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: расчётные байты двух банков публикации наблюдений
- Допустимые значения: Для HISTORY publicationBytes=2*(sizeof(snapshot)+subscriptionEntries*rowBytes) должно быть &lt;=maximumObservationBytes. Ноль не покрывает даже заголовок; требуемый минимум зависит от числа подписок/длины символа.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error RESOURCE_LIMIT / profile-history-observation-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `HistoryReceivingOptions::pendingBytesPerSubscription` — CFG-0064 {#cfg-0064-ru}

- Тип: `std::size_t`. Исходное значение: `= 4 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: пиковые байты полных исходных владельцев FIFO одного представления
- Допустимые значения: Ноль задаёт пустой pendingBytes. Новый владелец должен помещаться целиком; отдельно ограничены pendingItems и pendingFrames. Резерв этого бюджета умножается на clients и входит в maximumResidentBytes.
- Момент чтения: EndpointOptions сохраняет поле; при подготовке трактов HISTORY/MIXED оно преобразуется в HistoryLimits, размер/работа проверяются до активации и при доставке. Для других семейств поле не используется.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: До активации переполнение проверяемых сумм или произведений даёт api::Error RESOURCE_LIMIT / profile-arithmetic; превышение общей разрешённой памяти даёт RESOURCE_LIMIT / profile-history-resident-capacity. При открытии очереди отказ HistoryCollector::create преобразуется в api::Error RESOURCE_LIMIT / endpoint-queue. При обработке принятого события отказ history-&gt;stage фиксируется как QueueError::SOURCE_FAILURE с исходной причиной и приводит к failAll; HistoryError::WORK_LIMIT обрабатывается отдельно как QueueError::WORK_LIMIT (src/mux/queue.cpp:5287–5295). Переполнение FIFO конкретного представления записывает локальный HistoryViewFailure с RESOURCE_LIMIT и порядковым номером отказавшей записи.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

## HistorySnapshotOptions {#cfg-0109-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `HistorySnapshotOptions::maximumResults` — CFG-0109 {#cfg-0109-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: события результата одного запроса снимка
- Допустимые значения: Строго &gt;0. Для обычного индексированного снимка максимум передаётся в HistoryRetainedQuery; коллектор сначала считает строки и отказывает до выделения результата. Для временного ряда исходные строки ограничены history.maximumPoints, затем отфильтрованный бизнес-результат проверяется по maximumResults. Ни этот счётчик, ни depth книги не задают общий бюджет всей предварительной памяти/работы.
- Момент чтения: Проверяется при подготовке запроса снимка; сохранённые опции применяются при проекции результата.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: Ноль: api::Error INVALID_ARGUMENT / indexed-snapshot-options. Обычный индексированный запрос передаёт maximumResults в HistoryRetainedQuery: превышение числа строк даёт HistoryError::RESOURCE_LIMIT до reserve, затем QueueError::RESOURCE_LIMIT → RecoveryDiagnostic через retainedHistoryRange/queueFailure → api::Error RESOURCE_LIMIT / recovery-queue через recoveryDiagnostic. Временной ряд сначала получает исходные строки с максимумом history.maximumPoints; после фильтрации selected&gt;maximumResults даёт отдельный RESOURCE_LIMIT / time-series-result-limit.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/collectors/history.cpp](../../src/collectors/history.cpp), [src/mux/queue.cpp](../../src/mux/queue.cpp), [src/transport/recovery_core.cpp](../../src/transport/recovery_core.cpp).

## RuntimeOptions {#cfg-0042-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `RuntimeOptions::maximumEndpoints` — CFG-0042 {#cfg-0042-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: конечные точки одного ApiRuntime
- Допустимые значения: &gt;0. Совместно с maximumSubscriptionsPerEndpoint проверяются произведение, резервы владения наблюдателей и размер таблицы. Верхний предел зависит от других полей и sizeof; свободный слот конечной точки должен существовать.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. RuntimeImpl создаёт таблицу конечных точек; ApiRuntime::createEndpoint ищет свободный элемент этой таблицы.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. Непредставимые связанные размеры: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases. При исчерпании допуска операции: RESOURCE_LIMIT / runtime-endpoint-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSubscriptionsPerEndpoint` — CFG-0043 {#cfg-0043-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: подписки одной конечной точки
- Допустимые значения: &gt;0. Число задаёт размер таблиц подписок/запросов и участвует в проверяемом произведении с maximumEndpoints. Реальные/выбранные/завершаемые подписки могут удерживать слот до освобождения.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание конечной точки выделяет таблицу подписок этого размера; при регистрации требуется свободный элемент, включая ещё завершаемые подписки.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. Непредставимые связанные размеры: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases. При исчерпании допуска операции: RESOURCE_LIMIT / endpoint-subscription-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSymbolsPerSubscription` — CFG-0044 {#cfg-0044-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: уникальные символы/селекторы одной подписки
- Допустимые значения: &gt;0. Проверяется верхняя граница max_size/(selectorBytes*selectorBanks), где selectorBytes зависит от maximumSymbolUnits, а число банков — от ожидающих изменений и слушателей. Добавление уникального символа и некоторые входные списки ограничиваются отдельно.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание подписки копирует значение в SubscriptionPort::maximumSymbols; mutateDesired и подготовка владеющих селекторов проверяют фактический набор.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. Непредставимые связанные размеры: RESOURCE_LIMIT / runtime-selector-storage. При исчерпании допуска операции: RESOURCE_LIMIT / subscription-symbol-count / subscription-selector-arguments. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumListenersPerSubscription` — CFG-0045 {#cfg-0045-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: удерживаемые слушатели данных подписки
- Допустимые значения: &gt;0. Лимит занятых резервов включает выбранные и завершаемые захваченные объекты; наличие удалённого из списка слушателя ещё не освобождает резерв. Отдельного постоянного максимума нет.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание подписки присваивает значение ListenerLedger::maximum; добавление слушателя резервирует место в этом счётчике владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. При исчерпании допуска операции: RESOURCE_LIMIT / subscription-listener-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumSymbolUnits` — CFG-0046 {#cfg-0046-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 одного символа
- Допустимые значения: 1..128 включительно. Предел используется до владения текстом при разборе и проверке селектора; также влияет на вычисление selectorBytes и общей ёмкости банков. Число 128 относится к квалифицированному верхнему API.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание подписки копирует maximumSymbolUnits; validateSelector проверяет длину и ASCII-грамматику до принятия символа.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options для значения вне 1..128; RESOURCE_LIMIT / runtime-selector-storage при непредставимом связанном резерве. Символ длиннее принятого maximumSymbolUnits либо вне допустимого печатного ASCII отвергается INVALID_ARGUMENT / subscription-symbol.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::dnsServers` — CFG-0047 {#cfg-0047-ru}

- Тип: `std::vector<transport::DnsServer>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явные IPv4-resolvers; пустое значение выбирает ограниченный снимок системных настроек.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании ApiRuntime; владеющие настройки реестра и рабочего потока.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `RuntimeOptions::dnsPort` — CFG-0048 {#cfg-0048-ru}

- Тип: `std::uint16_t`. Исходное значение: `= 53`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Общий порт DNS-сервера для UDP/TCP, в диапазоне 1..65535.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Общий порт DNS-сервера для UDP/TCP, в диапазоне 1..65535.
- Момент чтения: При создании ApiRuntime; владеющие настройки реестра и рабочего потока.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `RuntimeOptions::maximumStateListenersPerEndpoint` — CFG-0049 {#cfg-0049-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: удерживаемые слушатели состояния конечной точки
- Допустимые значения: &gt;0. Задаёт счётчики владения конечной точки и OnDemand; добавление требует свободного резерва, пространство идентификаторов токенов также конечно. Отдельного численного потолка нет. Слушатели EndpointState и OnDemand используют отдельные счётчики владения с этим максимумом; поле не образует один общий счётчик обоих видов.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. makeStateListeners создаёт отдельные счётчики владения для наблюдений EndpointState и OnDemand; добавление каждого вида использует свой maximum.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. При исчерпании допуска операции: RESOURCE_LIMIT / state-change-listener-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumStateListenersPerSubscription` — CFG-0050 {#cfg-0050-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: удерживаемые слушатели состояния подписки
- Допустимые значения: &gt;0. Копируется в отдельный счётчик владения каждой подписки; выбранные/завершаемые слушатели остаются учтёнными. Отдельного численного потолка нет.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание подписки вызывает makeStateListeners с данным максимумом; addStateListener резервирует выбранные и завершаемые наблюдения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. При исчерпании допуска операции: RESOURCE_LIMIT / state-change-listener-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumEventTypeArgumentsPerSubscription` — CFG-0051 {#cfg-0051-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: аргументы типов событий до удаления дубликатов
- Допустимые значения: &gt;0. types.size() проверяется до анализа содержимого/сортировки; пустой список отдельно запрещён. Повторы во входе занимают аргументы, даже если итоговых типов меньше.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Feed::createSubscriptionPort проверяет длину исходного списка types перед анализом и удалением дубликатов.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. При исчерпании допуска операции: RESOURCE_LIMIT / subscription-event-type-arguments. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumPendingStateChangesPerEndpoint` — CFG-0052 {#cfg-0052-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: ожидающие изменения состояния конечной точки
- Допустимые значения: &gt;0. Задаёт stateChanges.resize и квант dispatchStateChanges; при принятии нового управления требуется место в уведомлениях. Верхний предел зависит от памяти/контейнера.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Создание конечной точки выделяет stateChanges; операции управления резервируют записи уведомления до принятия изменения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. При исчерпании допуска операции: RESOURCE_LIMIT / endpoint-state-notification-count. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `RuntimeOptions::maximumChangeListenersPerSubscription` — CFG-0053 {#cfg-0053-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: слушатели изменений подписки
- Допустимые значения: &gt;0. Участвует в проверках хранения фаз, резервов владения и банков селекторов. Каждый выбранный/завершаемый слушатель сохраняет резерв; регистрация требует свободной ёмкости.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. initializeSubscriptionObservers копирует максимум в собственный ListenerLedger наблюдателей изменений.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. Непредставимые связанные размеры: RESOURCE_LIMIT / runtime-change-storage / runtime-change-leases / runtime-change-snapshots. При исчерпании допуска операции: RESOURCE_LIMIT / subscription-change-listeners. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumPendingSubscriptionChanges` — CFG-0054 {#cfg-0054-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: ожидающие фазы изменений подписки
- Допустимые значения: &gt;0. Проверяется размер массива фаз и сумма числа фаз, слушателей изменений и двух кадров подготовки; затем учитываются 3 банка селекторов. До принятия изменения требуется свободная фаза.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. initializeSubscriptionObservers выделяет массив phases; подготовка изменения требует свободную фазу перед публикацией символов.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нулевом значении. Непредставимые связанные размеры: RESOURCE_LIMIT / runtime-change-storage / runtime-change-snapshots. При исчерпании допуска операции: RESOURCE_LIMIT / subscription-change-pending. producerAction переводит bad_alloc в ALLOCATION_FAILURE / api-allocation, length_error в RESOURCE_LIMIT / api-capacity.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumSubscriptionChangesPerTurn` — CFG-0055 {#cfg-0055-ru}

- Тип: `std::size_t`. Исходное значение: `= 32`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: общие кредиты уведомлений за один цикл выполнения обработчика
- Допустимые значения: &gt;0. Проверяются sizeof(SubscriptionContinuation) и совместимость числа резервов владения; RuntimeImpl выделяет continuations такого размера и присваивает maximumWork. Это квант продвижения, не число подписок за всё время.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. RuntimeImpl выделяет continuations и устанавливает ChangeDomain::maximumWork; цикл внешнего принятия пополняет remainingWork этим значением.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / runtime-options при нуле. Размер continuations проверяется: RESOURCE_LIMIT / runtime-change-storage или runtime-change-leases. Внутри цикл выполнения обработчика исчерпание кредита даёт WORK_LIMIT / subscription-change-turn либо subscription-change-continuations; новое внешнее принятие начинает новый цикл по правилам домена.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/subscription_observer.cpp](../../src/api/subscription_observer.cpp).

### `RuntimeOptions::maximumCustomCallbackSubscriptions` — CFG-0056 {#cfg-0056-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно допущенные пользовательские пакеты обратного вызова
- Допустимые значения: Ноль разрешён при создании ApiRuntime и отключает допуск пользовательского обработчика: при первом opt-in вычисляется min(maximumCustomCallbackSubscriptions,(maximumCallbackBytes-storage)/PACKET_BYTES), результат должен быть&gt;0. Фактический предел связан с байтами.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. Только первый выбор пользовательского исполнителя вызывает callbackPool; здесь вычисляется доступное число пакетов. Общий конструктор не проверяет положительность этого поля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Число при создании ApiRuntime не отвергается. При первом пользовательском исполнителе: api::Error RESOURCE_LIMIT / callback-pool-budget или callback-packet-budget; переполнение таблицы отдельно callback-pool-bytes. Без этой возможности поле не создаёт собственного отказа.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/callback_execution.cpp](../../src/api/callback_execution.cpp).

### `RuntimeOptions::maximumCallbackBytes` — CFG-0057 {#cfg-0057-ru}

- Тип: `std::size_t`. Исходное значение: `= 32 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: логические байты общего пула и пакетов обратного вызова
- Допустимые значения: Ноль разрешён при создании ApiRuntime; первый выбор пользовательского исполнителя требует budget&gt;storageBytes(endpointSlots) и остаток &gt;= PACKET_BYTES. Пакетов min(лимит подписок, остаток/PACKET_BYTES). Захваты приложения в эту оценку не входят.
- Момент чтения: ApiRuntime проверяет применимые исходные условия в prepareRuntimeOptions. callbackPool при первом пользовательском исполнителе вычитает фиксированную таблицу из maximumCallbackBytes и делит остаток на PACKET_BYTES.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Число при создании ApiRuntime не отвергается. При первом пользовательском исполнителе: api::Error RESOURCE_LIMIT / callback-pool-budget или callback-packet-budget; переполнение таблицы отдельно callback-pool-bytes. Без этой возможности поле не создаёт собственного отказа.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/callback_execution.cpp](../../src/api/callback_execution.cpp).

## TapeSourceOptions {#cfg-0103-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `TapeSourceOptions::maximumFileBytes` — CFG-0103 {#cfg-0103-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: физические входные байты файла
- Допустимые значения: &gt;0. До открытия потокового чтения file_size&lt;=maximumFileBytes; то же значение передаётся читателю записи и хранится для дальнейшего счёта. Слишком большой файл отвергается; отдельного постоянного максимума лимита нет.
- Момент чтения: Endpoint::connectTape проверяет параметры до принятия команды. На рабочем потоке создаётся TapeFileSource; читатель записи и часы повторно потребляют сохранённые значения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error RESOURCE_LIMIT / tape-file-size. Нулевые лимиты файла и кадров и недопустимая политика → INVALID_ARGUMENT / tape-source-options.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::maximumFrames` — CFG-0104 {#cfg-0104-ru}

- Тип: `std::size_t`. Исходное значение: `= 65'536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кадры записи, включая маркеры и управление
- Допустимые значения: &gt;0. maximumFrames передаётся нескольким счётчикам читателя и умножает бюджеты DATA и снимков. Каждое умножение проверяется; допустимость определяется полученными произведениями.
- Момент чтения: Endpoint::connectTape проверяет параметры до принятия команды. На рабочем потоке создаётся TapeFileSource; читатель записи и часы повторно потребляют сохранённые значения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Ноль и недопустимая политика: api::Error INVALID_ARGUMENT / tape-source-options. Переполнение производных бюджетов: RESOURCE_LIMIT / tape-limit-product. При попытке читать следующий непустой ввод после maximumFrames TapeReader переходит в FAILED с TapeError::RESOURCE_LIMIT / frames; TapeFileSource возвращает api::Diagnostic RESOURCE_LIMIT с той же стадией frames.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp), [src/protocol/tape_reader.cpp](../../src/protocol/tape_reader.cpp).

### `TapeSourceOptions::playback` — CFG-0105 {#cfg-0105-ru}

- Тип: `TapePlaybackPolicy`. Исходное значение: `= TapePlaybackPolicy::IMMEDIATE`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: политика воспроизведения
- Допустимые значения: Только IMMEDIATE или SOURCE_TIME. IMMEDIATE не проверяет и не использует speedPercent/maximumSourceSpan; SOURCE_TIME требует отдельные допустимые скорость и диапазон.
- Момент чтения: Endpoint::connectTape проверяет параметры до принятия команды. На рабочем потоке создаётся TapeFileSource; читатель записи и часы повторно потребляют сохранённые значения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / tape-source-options. Нулевые лимиты файла и кадров и недопустимая политика → INVALID_ARGUMENT / tape-source-options.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::speedPercent` — CFG-0106 {#cfg-0106-ru}

- Тип: `std::uint32_t`. Исходное значение: `= 100`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: проценты обычной скорости
- Допустимые значения: Для SOURCE_TIME: 1..10000 включительно, ноль запрещён. Для IMMEDIATE поле не читается в validTapePlayback и не влияет на расписание. Задержка округляется вверх по накопленному максимальному исходному времени.
- Момент чтения: Endpoint::connectTape проверяет параметры до принятия команды. На рабочем потоке создаётся TapeFileSource; читатель записи и часы повторно потребляют сохранённые значения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / tape-source-options; ошибка представимости дедлайна → TAPE_TIMING_RANGE / tape-playback-time. Нулевые лимиты файла и кадров и недопустимая политика → INVALID_ARGUMENT / tape-source-options.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::maximumSourceSpan` — CFG-0107 {#cfg-0107-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{std::chrono::hours{24}}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды диапазона исходного времени от первой метки времени
- Допустимые значения: Для SOURCE_TIME: &gt;0 и &lt;=365 суток. Проверяется положительное прошедшее время от первой метки; обратный ход времени не уменьшает накопленный максимум. Отсутствующая метка не задаёт начало отсчёта. Для IMMEDIATE поле не ограничивается.
- Момент чтения: Endpoint::connectTape проверяет параметры до принятия команды. На рабочем потоке создаётся TapeFileSource; читатель записи и часы повторно потребляют сохранённые значения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / tape-source-options; превышение диапазона времени → TAPE_TIMING_RANGE / tape-playback-time. Нулевые лимиты файла и кадров и недопустимая политика → INVALID_ARGUMENT / tape-source-options.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/api/tape_playback_clock.hpp](../../src/api/tape_playback_clock.hpp), [src/api/tape_file_source.cpp](../../src/api/tape_file_source.cpp).

### `TapeSourceOptions::initiallyPaused` — CFG-0108 {#cfg-0108-ru}

- Тип: `bool`. Исходное значение: `= false`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Открыть файл и ждать resume перед чтением первого envelope.
- Единицы: булево значение
- Объявленные ограничения: Объявленная булева область: false/true; ограничения совместимости профиля рассматриваются отдельно и независимо не перепроверены.
- Момент чтения: Явный запуск tape-источника; управление воспроизведением имеет отдельные методы.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

## VerifiedTlsOptions {#cfg-0070-owner-ru}

[include/qd/api/feed.hpp](../../include/qd/api/feed.hpp)

### `VerifiedTlsOptions::referenceIdentity` — CFG-0070 {#cfg-0070-ru}

- Тип: `std::string`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты ASCII DNS/IP удостоверяемого имени
- Допустимые значения: При наличии TLS требуется validEndpointHost: непустое проверяемое DNS/IP-имя длиной&lt;=254 байт, с DNS-меткой&lt;=63 байт. Дополнительно длина &lt;= tls.limits.identityBytes; адрес подключения выбирается независимо. Без сборочного профиля TLS любое tls отвергается.
- Момент чтения: EndpointConfiguration::create проверяет параметры TLS верхнего API до копирования и I/O; подготовка принимающего профиля переносит данные, активация создаёт среду выполнения TLS.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-tls-options либо RESOURCE_LIMIT / endpoint-tls-allowance. При неподдержанном профиле tlsRequirements → UNSUPPORTED_PROFILE; без TLS-сборки UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/client/endpoint_host.hpp](../../src/client/endpoint_host.hpp).

### `VerifiedTlsOptions::trustPem` — CFG-0071 {#cfg-0071-ru}

- Тип: `std::string`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты явного PEM доверия
- Допустимые значения: При наличии TLS текст непустой, &lt;=512*1024 байт и &lt;=tls.limits.trustBytes. Синтаксис и пригодность сертификатов проверяются нижним TLS. Автоматическое чтение системных корней не задано.
- Момент чтения: EndpointConfiguration::create проверяет параметры TLS верхнего API до копирования и I/O; подготовка принимающего профиля переносит данные, активация создаёт среду выполнения TLS.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / endpoint-tls-options либо RESOURCE_LIMIT / endpoint-tls-allowance. При неподдержанном профиле tlsRequirements → UNSUPPORTED_PROFILE; без TLS-сборки UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp).

### `VerifiedTlsOptions::handshakeTimeout` — CFG-0072 {#cfg-0072-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительный срок проверяемого handshake.
- Единицы: миллисекунды
- Объявленные ограничения: Положительный срок проверяемого handshake.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::closeTimeout` — CFG-0073 {#cfg-0073-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{1000}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительный срок фазы закрытия TLS.
- Единицы: миллисекунды
- Объявленные ограничения: Положительный срок фазы закрытия TLS.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::limits` — CFG-0074 {#cfg-0074-ru}

- Тип: `transport::TlsResourceLimits`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Пределы ввода и проверки квалифицированного профиля.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::host` — CFG-0075 {#cfg-0075-ru}

- Тип: `transport::TlsHostLimits`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Конечные пределы одного образа, включая внешних пользователей и префиксы неудачных попыток.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

### `VerifiedTlsOptions::sdkInstanceBytes` — CFG-0076 {#cfg-0076-ru}

- Тип: `std::size_t`. Исходное значение: `= 0`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты частного SDK-резерва экземпляра TLS
- Допустимые значения: Должно быть &gt;=tlsRequirements(limits,host).sdk.instanceBytes. Значение 0 по умолчанию означает незаполненный допуск; вызывающая сторона должна предоставить рассчитанный объём. Нижний TLS повторно проверяет профиль и ресурсный допуск. Прохождение этих начальных условий не доказывает достаточность полного резерва конкретной попытки: он включает адаптер, снимок настроек, TCP и оба SDK-допуска.
- Момент чтения: EndpointConfiguration::create проверяет начальные минимумы до копирования и ввода-вывода. При активации feed передаёт три допуска в TlsOptions. TlsRecovery::prepare создаёт runtime каждой конкретной попытки; TlsRuntimeFactory::create повторяет минимальную проверку и рассчитывает полный резерв.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Проверка конфигурации: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. При активации конкретной TLS-попытки: RESOURCE_LIMIT / tls-sdk-allowance для минимума SDK; RESOURCE_LIMIT / tls-resident-arithmetic при переполнении формирования компонент; RESOURCE_LIMIT / tls-resident-allowance при переполнении их суммы либо превышении residentBytes. Стадии проходят TlsRecovery/common → recoveryDiagnostic. Неподдержанный tlsRequirements даёт UNSUPPORTED_PROFILE; сборка без TLS — UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::sdkSharedBytes` — CFG-0077 {#cfg-0077-ru}

- Тип: `std::size_t`. Исходное значение: `= 0`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты общего SDK-резерва одного образа
- Допустимые значения: Должно быть &gt;=tlsRequirements(limits,host).sdk.sharedBytes. Нижний уровень также учитывает общие пользователи/профиль host; один лишь ненулевой бюджет не удостоверяет достаточность. Прохождение этих начальных условий не доказывает достаточность полного резерва конкретной попытки: он включает адаптер, снимок настроек, TCP и оба SDK-допуска.
- Момент чтения: EndpointConfiguration::create проверяет начальные минимумы до копирования и ввода-вывода. При активации feed передаёт три допуска в TlsOptions. TlsRecovery::prepare создаёт runtime каждой конкретной попытки; TlsRuntimeFactory::create повторяет минимальную проверку и рассчитывает полный резерв.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Проверка конфигурации: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. При активации конкретной TLS-попытки: RESOURCE_LIMIT / tls-sdk-allowance для минимума SDK; RESOURCE_LIMIT / tls-resident-arithmetic при переполнении формирования компонент; RESOURCE_LIMIT / tls-resident-allowance при переполнении их суммы либо превышении residentBytes. Стадии проходят TlsRecovery/common → recoveryDiagnostic. Неподдержанный tlsRequirements даёт UNSUPPORTED_PROFILE; сборка без TLS — UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::residentBytes` — CFG-0078 {#cfg-0078-ru}

- Тип: `std::size_t`. Исходное значение: `= 0`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты общего логического резерва запуска TLS
- Допустимые значения: Верхний validate требует &gt;0. Точная достаточность относительно SDK, среды и удерживаемых буферов проверяется средой выполнения TLS при активации. Фиксированного универсального минимума нет. Прохождение этих начальных условий не доказывает достаточность полного резерва конкретной попытки: он включает адаптер, снимок настроек, TCP и оба SDK-допуска.
- Момент чтения: EndpointConfiguration::create проверяет начальные минимумы до копирования и ввода-вывода. При активации feed передаёт три допуска в TlsOptions. TlsRecovery::prepare создаёт runtime каждой конкретной попытки; TlsRuntimeFactory::create повторяет минимальную проверку и рассчитывает полный резерв.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Проверка конфигурации: api::Error RESOURCE_LIMIT / endpoint-tls-allowance. При активации конкретной TLS-попытки: RESOURCE_LIMIT / tls-sdk-allowance для минимума SDK; RESOURCE_LIMIT / tls-resident-arithmetic при переполнении формирования компонент; RESOURCE_LIMIT / tls-resident-allowance при переполнении их суммы либо превышении residentBytes. Стадии проходят TlsRecovery/common → recoveryDiagnostic. Неподдержанный tlsRequirements даёт UNSUPPORTED_PROFILE; сборка без TLS — UNSUPPORTED_PROFILE / endpoint-tls-unavailable.
- Исходники потребителя: [include/qd/api/feed.hpp](../../include/qd/api/feed.hpp), [src/api/endpoint_configuration.cpp](../../src/api/endpoint_configuration.cpp), [src/api/receiving_profile.cpp](../../src/api/receiving_profile.cpp), [src/api/feed.cpp](../../src/api/feed.cpp), [src/transport/tls_recovery.cpp](../../src/transport/tls_recovery.cpp), [src/transport/tls_runtime.cpp](../../src/transport/tls_runtime.cpp).

### `VerifiedTlsOptions::credentials` — CFG-0079 {#cfg-0079-ru}

- Тип: `std::optional<BasicTlsCredentials>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явно подключаемый Basic provider; вызывается сериализованным worker.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: При создании endpoint; применённая политика принадлежит worker; отдельные явные setters действуют там, где объявлены.
- Изменяемость и время жизни: Изменение входной структуры не перенастраивает владельца; доступны только отдельно документированные методы изменения настроек во время работы.
- Ошибка: api::ErrorCode и Diagnostic; точное соответствие поля ветви ошибки отдельно не перепроверено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ErrorCode, WaitStatus`.

## IndependentSubscriptionLimits {#cfg-0110-owner-ru}

[include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp)

### `IndependentSubscriptionLimits::maximumAttachments` — CFG-0110 {#cfg-0110-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые и завершаемые поколения прикрепления
- Допустимые значения: &gt;0 и &lt;=attachments_.max_size(). Повторное attach к тому же Feed возвращает прежний ID; истёкшие объекты gate удаляются до проверки. Лимит учитывает активные поколения и поколения, завершаемые обработчиком; отдельно конечны ID.
- Момент чтения: Конструктор IndependentSubscription проверяет лимиты; attach/setSelectors применяют сохранённые значения под mutex.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / independent-subscription-options при нуле. Превышение max_size соответствующего контейнера: RESOURCE_LIMIT / independent-subscription-options. Поздний допуск: RESOURCE_LIMIT / independent-subscription-attachments. std::bad_alloc от reserve/создания объектов может выйти отдельно.
- Исходники потребителя: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

### `IndependentSubscriptionLimits::maximumSelectors` — CFG-0111 {#cfg-0111-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные аргументы и сохранённые селекторы
- Допустимые значения: &gt;0 и &lt;=selectors_.max_size(). setSelectors проверяет исходный размер до удаления дубликатов; итоговые символы должны также укладываться в лимиты каждого прикреплённого Feed.
- Момент чтения: Конструктор IndependentSubscription проверяет лимиты; attach/setSelectors применяют сохранённые значения под mutex.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / independent-subscription-options при нуле. Превышение max_size соответствующего контейнера: RESOURCE_LIMIT / independent-subscription-options. Поздний допуск: RESOURCE_LIMIT / independent-subscription-selector-arguments. std::bad_alloc от reserve/создания объектов может выйти отдельно.
- Исходники потребителя: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

### `IndependentSubscriptionLimits::maximumSymbolUnits` — CFG-0112 {#cfg-0112-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 символа селектора
- Допустимые значения: &gt;0. У каждого аргумента eventSymbol().size()&lt;=maximumSymbolUnits до сохранения; отдельного потолка 128 здесь нет. При прикреплении Feed дополнительно применяет собственный maximumSymbolUnits/грамматику.
- Момент чтения: Конструктор IndependentSubscription проверяет лимиты; attach/setSelectors применяют сохранённые значения под mutex.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: api::Error INVALID_ARGUMENT / independent-subscription-options при нуле. Поздний допуск: RESOURCE_LIMIT / independent-subscription-symbol-units. std::bad_alloc от reserve/создания объектов может выйти отдельно.
- Исходники потребителя: [include/qd/api/independent_subscription.hpp](../../include/qd/api/independent_subscription.hpp).

## IndexedEventModelLimits {#cfg-0113-owner-ru}

[include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp)

### `IndexedEventModelLimits::partitions` — CFG-0113 {#cfg-0113-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: разделы по символу/источнику
- Допустимые значения: Ноль допустим конструктором. Новый непустой раздел требует partitions_.size()&lt;partitions; пустой результат может удалить раздел без нового слота. Верхний потолок отдельно не задан.
- Момент чтения: IndexedEventModel лишь сохраняет лимиты в конструкторе; processEventsChecked проверяет объём до копирования и при подготовке целого кандидата.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected с IndexedEventModelError::RESOURCE_LIMIT при превышении и length_error; ALLOCATION_FAILURE при bad_alloc. Нарушение ключа события отдельно WRONG_KEY; отказ оставляет прежнюю модель.
- Исходники потребителя: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::symbolUnits` — CFG-0114 {#cfg-0114-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 ключа символа
- Допустимые значения: Ноль допускает только symbol.size()==0 в этой проверке; непустой ключ откажет. Соответствие symbol/source каждого события проверяется отдельно.
- Момент чтения: IndexedEventModel лишь сохраняет лимиты в конструкторе; processEventsChecked проверяет объём до копирования и при подготовке целого кандидата.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected с IndexedEventModelError::RESOURCE_LIMIT при превышении и length_error; ALLOCATION_FAILURE при bad_alloc. Нарушение ключа события отдельно WRONG_KEY; отказ оставляет прежнюю модель.
- Исходники потребителя: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::inputEvents` — CFG-0115 {#cfg-0115-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные события одного вызова
- Допустимые значения: input.size()&lt;=inputEvents;0 допускает пустой вызов, возвращающий пустые Changes. Непустой вызов также связан с глобальными pendingEvents/retainedEvents.
- Момент чтения: IndexedEventModel лишь сохраняет лимиты в конструкторе; processEventsChecked проверяет объём до копирования и при подготовке целого кандидата.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected с IndexedEventModelError::RESOURCE_LIMIT при превышении и length_error; ALLOCATION_FAILURE при bad_alloc. Нарушение ключа события отдельно WRONG_KEY; отказ оставляет прежнюю модель.
- Исходники потребителя: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::pendingEvents` — CFG-0116 {#cfg-0116-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: ожидающие события незавершённых транзакций/снимков всех разделов
- Допустимые значения: Ноль принимается конструктором. Ёмкость текущего раздела=лимит минус ожидающие события других разделов; событие незавершённой транзакции требует свободный слот. Завершающее событие может временно добавляться перед атомарным применением и очисткой.
- Момент чтения: IndexedEventModel лишь сохраняет лимиты в конструкторе; processEventsChecked проверяет объём до копирования и при подготовке целого кандидата.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected с IndexedEventModelError::RESOURCE_LIMIT при превышении и length_error; ALLOCATION_FAILURE при bad_alloc. Нарушение ключа события отдельно WRONG_KEY; отказ оставляет прежнюю модель.
- Исходники потребителя: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

### `IndexedEventModelLimits::retainedEvents` — CFG-0117 {#cfg-0117-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: удерживаемые активные события всех разделов
- Допустимые значения: Ноль принимается конструктором и допускает результат без активных событий. Ёмкость текущего раздела уменьшается на активные события других разделов; после завершения транзакции next.active.size() должна помещаться.
- Момент чтения: IndexedEventModel лишь сохраняет лимиты в конструкторе; processEventsChecked проверяет объём до копирования и при подготовке целого кандидата.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected с IndexedEventModelError::RESOURCE_LIMIT при превышении и length_error; ALLOCATION_FAILURE при bad_alloc. Нарушение ключа события отдельно WRONG_KEY; отказ оставляет прежнюю модель.
- Исходники потребителя: [include/qd/api/indexed_event_model.hpp](../../include/qd/api/indexed_event_model.hpp).

## OrderBookCorrectionOptions {#cfg-0122-owner-ru}

[include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp)

### `OrderBookCorrectionOptions::policy` — CFG-0122 {#cfg-0122-ru}

- Тип: `OrderBookCorrectionPolicy`. Исходное значение: `= OrderBookCorrectionPolicy::DISABLED`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: политика исправления агрегированных заявок
- Допустимые значения: DISABLED либо SAME_SYMBOL; иное значение запрещено в конструкторе. DISABLED пропускает коррекцию; SAME_SYMBOL сравнивает подходящие заявки всех источников точного символа.
- Момент чтения: Конструктор OrderBookModel захватывает corrections; correct использует их при подготовке завершённых изменений.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::invalid_argument("Invalid order-book correction options") при неизвестной политике/отрицательном возрасте. Перерасход entries позднее возвращает IndexedEventModelError::RESOURCE_LIMIT.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::keepAge` — CFG-0123 {#cfg-0123-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `= std::chrono::hours(24)`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды бизнес-возраста несвязанной заявки
- Допустимые значения: keepAge&gt;=0;0 допускается. Возраст сравнивается по бизнес-времени заявки, без чтения системных часов; верхнего потолка меньше представимой chrono::milliseconds нет.
- Момент чтения: Конструктор OrderBookModel захватывает corrections; correct использует их при подготовке завершённых изменений.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::invalid_argument("Invalid order-book correction options") при неизвестной политике/отрицательном возрасте. Перерасход entries позднее возвращает IndexedEventModelError::RESOURCE_LIMIT.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::flipAge` — CFG-0124 {#cfg-0124-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `= std::chrono::minutes(1)`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды бизнес-возраста пересечения сторон
- Допустимые значения: flipAge&gt;=0;0 допускается. Порог применяется при crossed-условии отдельно от keepAge; проверка отношения flipAge&lt;=keepAge отсутствует.
- Момент чтения: Конструктор OrderBookModel захватывает corrections; correct использует их при подготовке завершённых изменений.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::invalid_argument("Invalid order-book correction options") при неизвестной политике/отрицательном возрасте. Перерасход entries позднее возвращает IndexedEventModelError::RESOURCE_LIMIT.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookCorrectionOptions::entries` — CFG-0125 {#cfg-0125-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные действующие и подавленные отметки коррекций
- Допустимые значения: Ноль допускается конструктором. При включённой коррекции доступная ёмкость текущего символа=entries минус отметки коррекций других символов; вставка новой отметки коррекции требует свободный слот. DISABLED поле не расходует.
- Момент чтения: OrderBookModel сохраняет entries в correctionOptions_. При включённой коррекции applyEvents вычитает отметки коррекций других символов из общей ёмкости и передаёт остаток stageCorrections.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: stageCorrections возвращает IndexedEventModelError::RESOURCE_LIMIT, если нужен новая отметка коррекции при заполненной ёмкости. Изменения готовятся в отдельном кандидате вместе с processEventsChecked; отказ не публикует ни кандидат коррекции, ни кандидат индексированной модели. std::length_error отображается в RESOURCE_LIMIT, std::bad_alloc — в ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

## OrderBookViewOptions {#cfg-0118-owner-ru}

[include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp)

### `OrderBookViewOptions::filter` — CFG-0118 {#cfg-0118-ru}

- Тип: `OrderBookModelFilter`. Исходное значение: `= OrderBookModelFilter::ALL`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: значение фильтра областей
- Допустимые значения: Только значения OrderBookModelFilter, для которых scopeMask возвращает маску: UNFILTERED, COMPOSITE, REGIONAL, AGGREGATE, ORDER, COMPOSITE_REGIONAL, COMPOSITE_REGIONAL_AGGREGATE, ALL. Иное значение перечисления даёт INVALID_OPTIONS.
- Момент чтения: buildSnapshot читает переданные настройки при построении одного представления книги. Эти настройки не изменяют сохранённые заявки.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected: IndexedEventModelError::INVALID_OPTIONS при неверном filter/lotSize; RESOURCE_LIMIT при недостаточной ёмкости/length_error; ALLOCATION_FAILURE при bad_alloc. sourceId/depth собственного отказа диапазона не имеют.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::sourceId` — CFG-0119 {#cfg-0119-ru}

- Тип: `std::optional<std::int32_t>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный знаковый ID источника
- Допустимые значения: nullopt выбирает все источники; любой int32_t выбирает точный раздел этого источника. Знак сам по себе не проверяется здесь; отсутствие раздела даёт пустое наблюдение по правилам модели.
- Момент чтения: buildSnapshot читает переданные настройки при построении одного представления книги. Эти настройки не изменяют сохранённые заявки.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected: IndexedEventModelError::INVALID_OPTIONS при неверном filter/lotSize; RESOURCE_LIMIT при недостаточной ёмкости/length_error; ALLOCATION_FAILURE при bad_alloc. sourceId/depth собственного отказа диапазона не имеют.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::depth` — CFG-0120 {#cfg-0120-ru}

- Тип: `std::optional<std::size_t>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: число строк на каждой стороне книги
- Допустимые значения: nullopt сохраняет все подходящие заявки;0 обрезает обе стороны до пустых; положительное значение обрезает уже отсортированный результат отдельно по каждой стороне. Дополнительного максимума нет; depth не ограничивает предшествующее копирование/сортировку.
- Момент чтения: buildSnapshot читает переданные настройки при построении одного представления книги. Эти настройки не изменяют сохранённые заявки.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected: IndexedEventModelError::INVALID_OPTIONS при неверном filter/lotSize; RESOURCE_LIMIT при недостаточной ёмкости/length_error; ALLOCATION_FAILURE при bad_alloc. sourceId/depth собственного отказа диапазона не имеют.
- Исходники потребителя: [include/qd/api/order_book_model.hpp](../../include/qd/api/order_book_model.hpp), [src/api/order_book_model.cpp](../../src/api/order_book_model.cpp).

### `OrderBookViewOptions::lotSize` — CFG-0121 {#cfg-0121-ru}

- Тип: `std::int32_t`. Исходное значение: `= 1`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительный множитель для неиндивидуальных размеров.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Положительный множитель для неиндивидуальных размеров.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] std::expected&lt;OrderBookSnapshot, IndexedEventModelError&gt; buildSnapshot(std::u16string_view symbol, const OrderBookViewOptions &amp;options) const / [[nodiscard]] std::expected&lt;OrderBookSnapshot, IndexedEventModelError&gt; getFilteredSnapshot(std::u16string_view symbol, const OrderBookViewOptions &amp;options) const
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp); точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherCatalogOptions {#cfg-0129-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherCatalogOptions::schema` — CFG-0129 {#cfg-0129-ru}

- Тип: `const core::LocalSchema&`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: явная зависимость или данные: время жизни должен устанавливать владелец. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Проверенный каталог бинарных записей.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherCatalogOptions::wireFieldNames` — CFG-0130 {#cfg-0130-ru}

- Тип: `std::span<const std::vector<std::u16string>>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: явная зависимость или данные: время жизни должен устанавливать владелец. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Порядок записей и полей, точные QTP-имена.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherDemandPolicy {#cfg-0131-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherDemandPolicy::ticker` — CFG-0131 {#cfg-0131-ru}

- Тип: `std::optional<FeedFilter>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный фильтр спроса TICKER
- Допустимые значения: nullopt не вводит ограничения этого семейства. Присутствующий FeedFilter должен быть valid; начальная политика запрещает динамический фильтр. Обновление через соответствующий API допускает динамический фильтр и применяет снимок по правилам источника.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Начальная политика копируется; явный метод обновления принимает новую политику и последующие снимки динамического фильтра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherDemandPolicy::stream` — CFG-0132 {#cfg-0132-ru}

- Тип: `std::optional<FeedFilter>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный фильтр спроса STREAM
- Допустимые значения: nullopt не вводит ограничения STREAM; фильтр должен пройти valid() при наличии. При создании разрешён статический фильтр; живое обновление отдельно допускает динамический фильтр. Проверка применяется к универсальной записи и символу.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Начальная политика копируется; явный метод обновления принимает новую политику и последующие снимки динамического фильтра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherDemandPolicy::history` — CFG-0133 {#cfg-0133-ru}

- Тип: `std::optional<FeedFilter>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный фильтр спроса HISTORY
- Допустимые значения: nullopt не вводит ограничения HISTORY. Фильтр должен быть valid и начально статическим; живое обновление отдельно разрешает dynamic. Это политика принятия запроса, ограниченная остальными ресурсами сервера.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Начальная политика копируется; явный метод обновления принимает новую политику и последующие снимки динамического фильтра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-demand-filter. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

## PublisherOptions {#cfg-0145-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherOptions::MAX_PEER_DEMAND_POLICIES` — CFG-0145 {#cfg-0145-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 32`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Максимум адресных переопределений на endpoint.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::MAX_STREAM_BATCH_RECORDS` — CFG-0146 {#cfg-0146-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 8`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Максимум локальных STREAM-строк одной партии.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::listener` — CFG-0147 {#cfg-0147-ru}

- Тип: `transport::TcpListenerOptions`. Исходное значение: `{"127.0.0.1", 0, 16, 4}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Plain bind по числовому адресу; адрес заимствуется на время create.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::pendingQuotes` — CFG-0148 {#cfg-0148-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Максимум запросов публикации в очереди по всем включённым контрактам.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: 1..1024; src/api/publisher.cpp:3335. Ёмкость очереди, а не квота записей за всё время работы.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::source` — CFG-0149 {#cfg-0149-ru}

- Тип: `std::optional<PublisherSourceOptions>`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Удалённый upstream вместо локальной публикации.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::mixed` — CFG-0150 {#cfg-0150-ru}

- Тип: `bool`. Исходное значение: `= false`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Включить Quote/TICKER, TimeAndSale/STREAM и default Order/HISTORY на каждом соединении.
- Единицы: булево значение
- Объявленные ограничения: Объявленная булева область: false/true; ограничения совместимости профиля рассматриваются отдельно и независимо не перепроверены.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::catalog` — CFG-0151 {#cfg-0151-ru}

- Тип: `std::optional<PublisherCatalogOptions>`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Локальный publisher записей или полный relay. Требует mixed=false.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::demand` — CFG-0152 {#cfg-0152-ru}

- Тип: `PublisherDemandPolicy`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Исходная политика. Фильтры должны быть статическими и созданы до create.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::peerDemand` — CFG-0153 {#cfg-0153-ru}

- Тип: `std::vector<PublisherPeerDemandPolicy>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Владеющие переопределения, выбираемые по числовому IP при подключении.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::recordLimits` — CFG-0154 {#cfg-0154-ru}

- Тип: `PublisherRecordLimits`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Пределы ожидающего хранилища для локальной публикации записей.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::streamBatchRecords` — CFG-0155 {#cfg-0155-ru}

- Тип: `std::size_t`. Исходное значение: `= 1`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Значения от 2 до 8 включают партии в локальном mixed или catalog режиме.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: При отсутствии streamBytes: 1..8; &gt;1 только для локального mixed/catalog; байтовый режим выводит выбор отдельно, src/api/publisher.cpp:3343.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherOptions::streamBytes` — CFG-0156 {#cfg-0156-ru}

- Тип: `std::optional<PublisherStreamByteLimits>`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Включить локальную STREAM-публикацию с байтовым ограничением.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherPeerDemandPolicy {#cfg-0134-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherPeerDemandPolicy::address` — CFG-0134 {#cfg-0134-ru}

- Тип: `std::string`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты числового IP-адреса политики участника
- Допустимые значения: Непустой IPv4/IPv6,&lt;=45 байт, без NUL/% и с успешным make_address. IPv4 приводится к v4-mapped IPv6 для сравнения; две политики одного нормализованного адреса запрещены.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-demand-peer-address либо publisher-duplicate-demand-peer. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherPeerDemandPolicy::demand` — CFG-0135 {#cfg-0135-ru}

- Тип: `PublisherDemandPolicy`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Неизменяемая политика фильтра peer, заменяющая политику endpoint при совпадении числового удалённого IP.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherRecordLimits {#cfg-0136-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherRecordLimits::MAX_FIELDS` — CFG-0136 {#cfg-0136-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 128`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Поддержанное число логических полей одной передаваемой строки.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::MAX_ROW_BYTES` — CFG-0137 {#cfg-0137-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 16 * 1024`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Наибольший настраиваемый резерв строки.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::MAX_PENDING_BYTES` — CFG-0138 {#cfg-0138-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Наибольший настраиваемый совокупный резерв.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::rowBytes` — CFG-0139 {#cfg-0139-ru}

- Тип: `std::size_t`. Исходное значение: `= MAX_ROW_BYTES`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Максимум для представлений полей, единиц UTF-16 и непрозрачных байтов одной строки.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Локальный catalog или байтовый режим: 1..16384; mixed byte mode дополнительно &gt;=sizeof(SourceRow), src/api/publisher.cpp:3350 и 3362.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherRecordLimits::pendingBytes` — CFG-0140 {#cfg-0140-ru}

- Тип: `std::size_t`. Исходное значение: `= 256 * 1024`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Совокупный максимум во время ожидания в очереди или обработки коллектором.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Локальный catalog или байтовый режим: 1..8388608; mixed byte mode дополнительно &gt;=sizeof(SourceRow), src/api/publisher.cpp:3350 и 3362.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## PublisherSourceOptions {#cfg-0126-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherSourceOptions::address` — CFG-0126 {#cfg-0126-ru}

- Тип: `std::string_view`. Исходное значение: `= "127.0.0.1"`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты числового IP-адреса источника
- Допустимые значения: При заданном source: непустой числовой IPv4/IPv6,&lt;=45 байт; NUL и % запрещены, DNS-имена не принимаются. asio::ip::make_address должен успешно разобрать адрес.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-source-options либо publisher-source-address. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherSourceOptions::port` — CFG-0127 {#cfg-0127-ru}

- Тип: `std::uint16_t`. Исходное значение: `= 0`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: TCP-порт источника
- Допустимые значения: При заданном source:1..65535 по uint16_t и проверке ненулевого значения. default0 не включает автоматический выбор удалённого порта; он недопустим для source.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-source-options. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

### `PublisherSourceOptions::retryDelay` — CFG-0128 {#cfg-0128-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{100}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды задержки переподключения к источнику
- Допустимые значения: 1..60000 включительно при заданном source. Значение используется при вычислении retryAt после отказа вышестоящего источника.
- Момент чтения: При создании Publisher проверяются source и configuredDemandPolicies; копия адреса/политики сохраняется сервером. Фильтр расходуется при допуске запросов участника.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: PublisherEndpoint::create возвращает std::expected&lt;std::unique_ptr&lt;PublisherEndpoint&gt;,Diagnostic&gt;; внутренний api::Error преобразуется в Diagnostic: INVALID_ARGUMENT / publisher-source-options. Ошибки настройки собираются до работы сервера. std::bad_alloc отображается в ALLOCATION_FAILURE / publisher-storage.
- Исходники потребителя: [include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp), [src/api/publisher.cpp](../../src/api/publisher.cpp).

## PublisherStreamByteLimits {#cfg-0141-owner-ru}

[include/qd/api/publisher.hpp](../../include/qd/api/publisher.hpp)

### `PublisherStreamByteLimits::MAX_DELIVERY_WINDOW_BYTES` — CFG-0141 {#cfg-0141-ru}

- Тип: `static constexpr std::size_t`. Исходное значение: `= 8 * 1024 * 1024`
- Классификация: константа: не пользовательская настройка. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Разрешение слоёв свойств неприменимо.
- Объявленное назначение: Максимальное настроенное окно.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Байтовый режим: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; только локальные mixed или catalog, src/api/publisher.cpp:3355.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::targetPayloadBytes` — CFG-0142 {#cfg-0142-ru}

- Тип: `std::size_t`. Исходное значение: `= 8000`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Мягкий порог закодированного payload перед получением следующей записи.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Байтовый режим: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; только локальные mixed или catalog, src/api/publisher.cpp:3355.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::maxFrameBytes` — CFG-0143 {#cfg-0143-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Жёсткий предел полного кадра с префиксом длины.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Байтовый режим: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; только локальные mixed или catalog, src/api/publisher.cpp:3355.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

### `PublisherStreamByteLimits::deliveryWindowBytes` — CFG-0144 {#cfg-0144-ru}

- Тип: `std::size_t`. Исходное значение: `= 256 * 1024`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Байты отделённых записей, выбранных для peer; одна крупная строка может пройти.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Байтовый режим: 0&lt;targetPayloadBytes&lt;maxFrameBytes&lt;=65536; 0&lt;deliveryWindowBytes&lt;=8388608; только локальные mixed или catalog, src/api/publisher.cpp:3355.
- Момент чтения: PublisherEndpoint::create; worker владеет конфигурацией; заимствованный текст catalog/source копируется до возврата.
- Изменяемость и время жизни: Изменение aggregate не обновляет endpoint; явная замена политики спроса peer имеет отдельный API.
- Ошибка: api::Diagnostic INVALID_ARGUMENT на стадии publisher-*; ошибки допуска и runtime остаются отдельными.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `api::ErrorCode / Error / Diagnostic (qd/api/feed.hpp)`.

## TimeSeriesModelLimits {#cfg-0157-owner-ru}

[include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp)

### `TimeSeriesModelLimits::eventTypes` — CFG-0157 {#cfg-0157-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: аргументы дескрипторов типов
- Допустимые значения: &gt;0 и &lt;=max_size/sizeof(TimeSeriesEventDescriptor). Непустой исходный eventTypes.size() должен быть &lt;=лимита до удаления дубликатов; возможности каждого дескриптора проверяются отдельно.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::typeIdentityUnits` — CFG-0158 {#cfg-0158-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 устойчивой идентичности типа
- Допустимые значения: &gt;0. stableIdentity().size() каждого дескриптора&lt;=typeIdentityUnits. Дополнительного постоянного потолка нет.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::symbols` — CFG-0159 {#cfg-0159-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: уникальные селекторы модели
- Допустимые значения: &gt;0 и &lt;=max_size/sizeof(SubscriptionSelector). При изменении желаемого набора каждый новый уникальный селектор требует свободного слота; входные дубликаты дополнительно ограничены inputSymbols.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::inputSymbols` — CFG-0160 {#cfg-0160-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: аргументы селекторов одного изменения
- Допустимые значения: &gt;0. input.size()&lt;=inputSymbols проверяется до подготовки;0 как настройка запрещён, пустой вход как операция допускается.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::symbolUnits` — CFG-0161 {#cfg-0161-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 одного символа
- Допустимые значения: &gt;0. Каждый eventSymbol().size()&lt;=symbolUnits до копирования. Отдельного ASCII/128-потолка эта модель не вводит.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).

### `TimeSeriesModelLimits::totalSymbolUnits` — CFG-0162 {#cfg-0162-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 уникальных символов
- Допустимые значения: &gt;0. Уникальные кандидаты последовательно помещаются в totalSymbolUnits-units; подсчёт идёт после нормализации набора. Число symbols ограничено независимо.
- Момент чтения: TimeSeriesSubscriptionModel::create проверяет лимиты и типы; операции изменения символов подготавливают целый кандидат по сохранённым лимитам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: TimeSeriesModelError::INVALID_LIMITS при нуле/переполнении базовой ёмкости; RESOURCE_LIMIT при превышении поля/length_error; ALLOCATION_FAILURE при bad_alloc. Отказ не публикует частичный набор.
- Исходники потребителя: [include/qd/api/time_series_subscription.hpp](../../include/qd/api/time_series_subscription.hpp), [src/api/time_series_subscription.cpp](../../src/api/time_series_subscription.cpp).
