# Профили инструментов

[English](ipf.md) · [Оглавление справочника](../CONFIGURATION-REFERENCE.ru.md)

Контракты потребителей отдельных полей и сведения деклараций/исходников с оговорённой доказательностью явно различаются ниже. НЕИЗВЕСТНО обозначает недостающее сведение. Известные объявленные сведения сохраняются; их семантическая проверка ещё не завершена. Ссылки указывают декларацию или потребителя и не означают нового запуска тестов. Значения описывают текущий API; принятые изменения перечислены в [архитектурном контракте](../CONFIGURATION.ru.md).

- [ProfileCollectorLimits](ipf.ru.md#cfg-0345-owner-ru) — `include/qd/ipf/instrument_profile_collector.hpp`
- [IpfConnectionLimits](ipf.ru.md#cfg-0355-owner-ru) — `include/qd/ipf/instrument_profile_connection.hpp`
- [IpfConnectionRuntimeOptions](ipf.ru.md#cfg-0365-owner-ru) — `include/qd/ipf/instrument_profile_connection.hpp`
- [IpfReadLimits](ipf.ru.md#cfg-0368-owner-ru) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfReadOptions](ipf.ru.md#cfg-0382-owner-ru) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfReadWork](ipf.ru.md#cfg-0378-owner-ru) — `include/qd/ipf/instrument_profile_reader.hpp`
- [IpfEnvelopeLimits](ipf.ru.md#cfg-0385-owner-ru) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfTransferLimits](ipf.ru.md#cfg-0392-owner-ru) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfTransferOptions](ipf.ru.md#cfg-0415-owner-ru) — `include/qd/ipf/instrument_profile_url_reader.hpp`
- [IpfWriteLimits](ipf.ru.md#cfg-0432-owner-ru) — `include/qd/ipf/instrument_profile_writer.hpp`
- [IpfWriteOptions](ipf.ru.md#cfg-0439-owner-ru) — `include/qd/ipf/instrument_profile_writer.hpp`
- [ProfileLimits](ipf.ru.md#cfg-0440-owner-ru) — `include/qd/ipf/instrument_profile.hpp`
- [OptionChainLimits](ipf.ru.md#cfg-0445-owner-ru) — `include/qd/ipf/option_chains.hpp`

## ProfileCollectorLimits {#cfg-0345-owner-ru}

[include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp)

### `ProfileCollectorLimits::modelProfiles` — CFG-0345 {#cfg-0345-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: активные профили текущей модели
- Допустимые значения: Ноль допускается create; добавление нового активного профиля требует staged.size()&lt;modelProfiles. Замена/удаление существующего профиля не обязательно увеличивает размер.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::liveRevisions` — CFG-0346 {#cfg-0346-ru}

- Тип: `std::size_t`. Исходное значение: `= 200000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно удерживаемые ревизии профиля
- Допустимые значения: Любой установленный бюджет, включая 0. Каждая копия нового профиля требует 1 REVISIONS; старые ревизии остаются учтёнными через внешние снимки/пакеты до освобождения.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::retainedUnits` — CFG-0347 {#cfg-0347-ru}

- Тип: `std::size_t`. Исходное значение: `= 16 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 удерживаемых профилей
- Допустимые значения: Каждая новая ревизия резервирует Profile.retainedUnits() в общем счётчике владения. Ноль допускает только нулевой расход; старые ревизии и новые кандидаты могут одновременно занимать бюджет.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::customFields` — CFG-0348 {#cfg-0348-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: пользовательские поля копируемого профиля
- Допустимые значения: CollectorProfileAccess::fields(profile)&lt;=customFields до копирования. Ноль разрешает профиль без пользовательских полей fields; стандартные поля имеют собственные значения/размер.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::stagingProfiles` — CFG-0349 {#cfg-0349-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные/промежуточные профили одной атомарной операции
- Допустимые значения: Для обновления число рассматриваемых профилей равно input.size(), для удаления — model.size(). И это число, и tags.size() должны быть &lt;=stagingProfiles. Ноль допускает ранний выход при пустом входе, но может запретить удаление из непустой модели.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::listeners` — CFG-0350 {#cfg-0350-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: зарегистрированные слушатели коллектора
- Допустимые значения: listeners.size()&lt;listeners перед добавлением. Ноль запрещает слушателей; отдельны конечные идентификаторы токенов, работа снимка и квота deliveryCalls.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::liveBatches` — CFG-0351 {#cfg-0351-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно живые пакеты ревизий
- Допустимые значения: Создание пакета резервирует 1 BATCHES независимо от числа элементов. Ноль не допускает даже пустой объект пакета; внешнее удержание сохраняет резерв.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::batchReferences` — CFG-0352 {#cfg-0352-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные ссылки на ревизии во всех живых пакетах
- Допустимые значения: Создание пакета списывает source.size() REFERENCES. Ноль допускает лишь пустой пакет при отдельном доступном BATCHES; ссылка удерживается до уничтожения пакета.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::workUnits` — CFG-0353 {#cfg-0353-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: единицы работы одной операции коллектора
- Допустимые значения: Остаток workUnits заново берётся для каждой операции apply/listen. Пустое обновление может завершиться до списания;0 запрещает получение области подключения и операции с положительной ценой.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

### `ProfileCollectorLimits::deliveryCalls` — CFG-0354 {#cfg-0354-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: допущенные задачи обратного вызова одного цикла диспетчеризации
- Допустимые значения: Проверяется amount&lt;=deliveryCalls и already&lt;=deliveryCalls-amount. Ноль разрешает операции без задач обратного вызова; повторный вход при обновлениях расходует общий cycleReserved.
- Момент чтения: InstrumentProfileCollector::create сохраняет значения без общего запрета нуля. apply/listen/batch резервируют ресурсы до публикации кандидата; учтённый ресурс освобождается после завершения внешнего владения.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileCollectorError::LIMIT_EXCEEDED при превышении конкретного бюджета. Ошибки выделения бросают std::bad_alloc до публикации; кода ALLOCATION_FAILURE у ProfileCollectorError нет.
- Исходники потребителя: [include/qd/ipf/instrument_profile_collector.hpp](../../include/qd/ipf/instrument_profile_collector.hpp), [src/ipf/collector.cpp](../../src/ipf/collector.cpp).

## IpfConnectionLimits {#cfg-0355-owner-ru}

[include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp)

### `IpfConnectionLimits::batchProfiles` — CFG-0355 {#cfg-0355-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: профили ожидающего пакета
- Допустимые значения: Строго &gt;0 в Runtime::validate. pending.reserve(batchProfiles) резервирует массив; при достижении batchProfiles накопленный пакет передаётся методом flush. Вместимость входит в проверяемый размер памяти владельца; коллектор имеет собственные ограничения.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / connection-profile возвращается напрямую из Runtime::create до публикации при нулевом batchProfiles. Достижение положительного batchProfiles запускает flush; ошибки коллектора при flush относятся к работе Sink и попадают в наблюдаемое состояние подключения.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::pendingUnits` — CFG-0356 {#cfg-0356-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 ожидающих профилей
- Допустимые значения: Ноль допускается. Каждый целый профиль должен помещаться по retainedUnits в остаток pendingUnits; связанный байтовый резерв также учитывается residentBytes и appendPendingProfile.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfConnectionDiagnostic.transfer: IpfTransferError::RESOURCE_LIMIT / connection-pending-units либо connection-pending-backing. Рабочая ветвь передаёт отказ в наблюдаемую диагностику через Port::fail.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::oldGenerations` — CFG-0357 {#cfg-0357-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: сохранённые поколения источника
- Допустимые значения: Ноль допускается в validate, но первая передача нового поколения методом flush отказывает при old.size()&gt;=oldGenerations. Резерв oldGenerations*sizeof(uint64_t) входит в память владельца.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfConnectionDiagnostic.transfer: IpfTransferError::RESOURCE_LIMIT / connection-generations. Рабочая ветвь передаёт отказ в наблюдаемую диагностику через Port::fail.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::workUnits` — CFG-0358 {#cfg-0358-ru}

- Тип: `std::size_t`. Исходное значение: `= 64 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительная работа подключения
- Допустимые значения: Ноль запрещает запуск подключения с connection-domain-work. Затем списание проверяет amount&lt;=workUnits и spent&lt;=workUnits-amount; расход сохраняется между повторными попытками.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Нулевой workUnits: отдельный возврат WORK_LIMIT / connection-domain-work из create подключения до его публикации. Позднее исчерпание копирования: WORK_LIMIT / connection-copy-control-work, Sink вызывает Port::fail и сохраняет диагностику наблюдения.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionLimits::attempts` — CFG-0359 {#cfg-0359-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: попытки за жизнь подключения
- Допустимые значения: Ноль не отвергается Runtime::validate, но рабочий поток останавливается до первой попытки при attempts&gt;=лимита. Попытки суммируются по повторным попыткам одного подключения.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfConnectionDiagnostic.transfer: IpfTransferError::WORK_LIMIT / connection-lifetime. Рабочая ветвь передаёт отказ в наблюдаемую диагностику через Port::fail.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::listenerSlots` — CFG-0360 {#cfg-0360-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: слушатели состояний, включая завершаемые захваченные объекты
- Допустимые значения: Ноль допускается в validate и запрещает первый ObserverLease. Место освобождается после уничтожения захваченного объекта, включая уже удалённый из списка обработчик. Резерв массивов учтён в residentBytes.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / connection-listener-slots возвращается вызывающему listen как отдельный unexpected. Ошибка регистрации не меняет Port::observed.diagnostic. std::bad_alloc при создании Observer может выйти до публикации регистрации.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::residentBytes` — CFG-0361 {#cfg-0361-ru}

- Тип: `std::size_t`. Исходное значение: `= 16 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты собственного состояния подключения
- Допустимые значения: Должно покрыть размеры Runtime, Port, Control, Sink и ObserverLedger, массивы по completionWaiters, batchProfiles, pendingUnits, oldGenerations, listenerSlots и копии данных доверия, источника и авторизации. Каждое произведение и сложение проверяется до выделения; ноль не вмещает состояние.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / connection-owner-backing возвращается из проверки Runtime::create до публикации подключения. Это ошибка результата вызова; наблюдаемое состояние подключения ещё не создано.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionLimits::lifetime` — CFG-0362 {#cfg-0362-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{86400000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды срока жизни подключения
- Допустимые значения: &gt;0 и &lt;=duration_cast&lt;milliseconds&gt;(steady_clock::duration::max()/4). Начало фиксируется при запуске; абсолютный дедлайн насыщается у time_point::max. Истечение проверяется между ограниченными циклами продвижения.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Неположительный/слишком большой lifetime: отдельный INVALID_OPTIONS / connection-profile до публикации runtime. Истечение допустимого lifetime в рабочем цикле: WORK_LIMIT / connection-lifetime в диагностике наблюдения подключения.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_control.hpp](../../src/ipf/connection_control.hpp).

### `IpfConnectionLimits::completionWaiters` — CFG-0363 {#cfg-0363-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременные ожидающие завершения
- Допустимые значения: Ноль запрещает блокирующее ожидание незавершённого подключения; уже готовый результат возвращается до проверки места. Объект ожидания учитывается до снятия регистрации и завершения использования.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: RESOURCE_LIMIT / connection-wait-capacity возвращается блокирующему await как отдельная IpfConnectionDiagnostic. Отказ допуска ожидающего не записывается в Port::observed.diagnostic и не завершает подключение.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_wait.hpp](../../src/ipf/connection_wait.hpp).

### `IpfConnectionLimits::completionWaitTurns` — CFG-0364 {#cfg-0364-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: обороты ожидания одного завершения
- Допустимые значения: Ноль возвращает WORK_LIMIT, если результат ещё не готов. Положительный остаток уменьшается перед каждым ожиданием переменной условия; дедлайн, отмена и готовность проверяются отдельно.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: WORK_LIMIT / connection-wait-work возвращается ожидающему при нуле или исчерпании его цикла ожидания. Это отдельная диагностика await; Port::observed.diagnostic остаётся состоянием самого подключения.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp), [src/ipf/connection_wait.hpp](../../src/ipf/connection_wait.hpp).

## IpfConnectionRuntimeOptions {#cfg-0365-owner-ru}

[include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp)

### `IpfConnectionRuntimeOptions::maximumConnections` — CFG-0365 {#cfg-0365-ru}

- Тип: `std::size_t`. Исходное значение: `= 1`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: подключения одной среды выполнения
- Допустимые значения: Допускается только 1: ноль и значения &gt;1 получают INVALID_OPTIONS. Реализация содержит одно место для подключения на весь срок жизни runtime; повторное занятие возвращает connection-lifetime-slot.
- Момент чтения: InstrumentProfileConnectionRuntime::create проверяет форму настроек и достаточность памяти до создания владельца. Port сохраняет лимиты; Sink, рабочий поток и методы listen/await используют их в описанных для поля фазах.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: maximumConnections!=1: отдельный INVALID_OPTIONS / connection-profile из Runtime::create. Повторное использование занятого/завершаемого runtime: отдельный INVALID_STATE / connection-lifetime-slot из create подключения. Эти отказы возвращаются вызывающему и не записываются в существующий Port::observed.diagnostic.
- Исходники потребителя: [include/qd/ipf/instrument_profile_connection.hpp](../../include/qd/ipf/instrument_profile_connection.hpp), [src/ipf/connection.cpp](../../src/ipf/connection.cpp).

### `IpfConnectionRuntimeOptions::transfer` — CFG-0366 {#cfg-0366-ru}

- Тип: `IpfTransferOptions`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Существующие конечные бюджеты источника, native/TLS/HTTP/decoder, накопительные между повторными попытками.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Создание отдельного runtime соединения; бюджеты transfer накопительны между попытками источника; модель, listener и waiter имеют собственные пределы времени жизни.
- Изменяемость и время жизни: Один слот соединения на время жизни; обычные lifecycle API источника/listener не пополняют попытки и работу.
- Ошибка: Перечень ошибок владельца: IpfCompletionWaitStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfCompletionWaitStatus`.

### `IpfConnectionRuntimeOptions::limits` — CFG-0367 {#cfg-0367-ru}

- Тип: `IpfConnectionLimits`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Независимые пределы ожидающего состояния и управления моделью.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Создание отдельного runtime соединения; бюджеты transfer накопительны между попытками источника; модель, listener и waiter имеют собственные пределы времени жизни.
- Изменяемость и время жизни: Один слот соединения на время жизни; обычные lifecycle API источника/listener не пополняют попытки и работу.
- Ошибка: Перечень ошибок владельца: IpfCompletionWaitStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfCompletionWaitStatus`.

## IpfReadLimits {#cfg-0368-owner-ru}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadLimits::inputBytes` — CFG-0368 {#cfg-0368-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: принятые входные байты за время работы парсера
- Допустимые значения: Каждый байт требует progress.inputBytes&lt;inputBytes до принятия. Ноль запрещает непустой вход, но не сам конструктор/EOF. Разбиение на feed-вызовы не пополняет бюджет.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::INPUT_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::fieldUnits` — CFG-0369 {#cfg-0369-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 декодированного поля
- Допустимые значения: cell.size()&lt;fieldUnits перед добавлением очередной единицы;0 запрещает непустой текст поля. CSV-разделители/кавычки учитываются отдельным recordUnits.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::FIELD_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::recordUnits` — CFG-0370 {#cfg-0370-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: декодированные единицы синтаксиса и текста строки
- Допустимые значения: recordUnits ограничивает полную текущую строку; stagedUnits также проверяются по этому пределу. Ноль запрещает положительный расход, предел сбрасывается на новой строке.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::RECORD_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::fieldsPerRecord` — CFG-0371 {#cfg-0371-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: поля одной строки
- Допустимые значения: Индекс каждого нового поля должен быть&lt;fieldsPerRecord. Ноль запрещает поле; точное число по объявленному формату проверяется независимо.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::RECORD_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::records` — CFG-0372 {#cfg-0372-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: принятые логические строки
- Допустимые значения: Перед принятием строки требуется progress.records&lt;records; ноль запрещает первую строку. Управляющие строки и строки формата учитываются согласно диспетчеризации строк; профили имеют отдельный счётчик.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::RECORD_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::formats` — CFG-0373 {#cfg-0373-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно сохранённые форматы
- Допустимые значения: Новый формат требует formats.size()&lt;formats. Переопределение уже имеющегося ключа не требует нового слота;0 запрещает первый формат.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::FORMAT_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::headerUnits` — CFG-0374 {#cfg-0374-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имён сохранённых форматов/полей
- Допустимые значения: Новый stagedUnits должен помещаться в headerUnits вместе с удерживаемыми заголовками. Замена формата учитывает освобождаемое прежнее описание. Ноль запрещает положительный объём текста заголовка.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::HEADER_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profiles` — CFG-0375 {#cfg-0375-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: выданные профили за время работы парсера
- Допустимые значения: Перед выдачей требуется progress.profiles&lt;profiles. Ноль допускает чтение контроля/форматов, но запрещает первый выдаваемый профиль.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::OUTPUT_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profileUnits` — CFG-0376 {#cfg-0376-ru}

- Тип: `std::size_t`. Исходное значение: `= 16777216`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 выданных профилей
- Допустимые значения: currentUnits должен помещаться в остаток profileUnits. Накопительный progress.profileUnits не сбрасывается между вызовами feed. Ноль допускает лишь профили с нулевым retainedUnits при достаточном отдельном допуске числа профилей.
- Момент чтения: Reader сохраняет IpfReadOptions; парсер с последовательным приёмом входа проверяет остатки при принятии байта, строки, формата или целого профиля.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: Терминальный IpfDiagnostic с IpfReadError::OUTPUT_LIMIT и позицией источника; ранее выданный префикс сохраняется. bad_alloc даёт ALLOCATION_FAILURE.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadLimits::profile` — CFG-0377 {#cfg-0377-ru}

- Тип: `ProfileLimits`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Независимые пределы каждого владеющего значения профиля.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Вложенные пределы парсера IpfReadOptions захватываются при создании reader; контракт чтения накопительный.
- Изменяемость и время жизни: Вложенный явный вход следует настройкам владельца; изменение исходного aggregate не меняет текущую загрузку или parser; новой операции нужны явно новые настройки.
- Ошибка: Перечень ошибок владельца: IpfReadError, IpfReadStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

## IpfReadOptions {#cfg-0382-owner-ru}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadOptions::limits` — CFG-0382 {#cfg-0382-ru}

- Тип: `IpfReadLimits`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Независимые пределы ввода, хранения и вывода.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] IpfReadResult readInstrumentProfiles(std::istream &amp;input, IpfReadOptions options = {}) / [[nodiscard]] IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &amp;path, IpfReadOptions options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: IpfReadError, IpfReadStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

### `IpfReadOptions::completion` — CFG-0383 {#cfg-0383-ru}

- Тип: `IpfCompletionPolicy`. Исходное значение: `= IpfCompletionPolicy::ALLOW`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: политика завершения на чистом EOF
- Допустимые значения: ALLOW принимает чистый EOF без &#35;&#35;COMPLETE; REQUIRE выдаёт MISSING_COMPLETE при отсутствии маркера. Код проверяет только равенство с REQUIRE: неизвестное значение перечисления сейчас следует ALLOW-ветке; отдельного INVALID_OPTIONS для него нет.
- Момент чтения: При явном завершении входа после обработки последней строки.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfReadError::MISSING_COMPLETE только для REQUIRE без принятого маркера завершения; неправильное значение перечисления не даёт собственного отказа.
- Исходники потребителя: [include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp), [src/ipf/reader.cpp](../../src/ipf/reader.cpp).

### `IpfReadOptions::turnWork` — CFG-0384 {#cfg-0384-ru}

- Тип: `IpfReadWork`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительные квоты одного хода, используемые только синхронными convenience-readers.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Положительные квоты одного хода, используемые только синхронными convenience-readers.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] IpfReadResult readInstrumentProfiles(std::istream &amp;input, IpfReadOptions options = {}) / [[nodiscard]] IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &amp;path, IpfReadOptions options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: IpfReadError, IpfReadStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

## IpfReadWork {#cfg-0378-owner-ru}

[include/qd/ipf/instrument_profile_reader.hpp](../../include/qd/ipf/instrument_profile_reader.hpp)

### `IpfReadWork::inputBytes` — CFG-0378 {#cfg-0378-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Байты, принятые этим вызовом, включая некорректные UTF-8-байты.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::decodedUnits` — CFG-0379 {#cfg-0379-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Единицы UTF-16, обработанные этим вызовом, включая замены и синтаксис.
- Единицы: единицы определяются контрактом поля; UTF-16 там, где это указано
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::fields` — CFG-0380 {#cfg-0380-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Завершённые операции преобразования полей и метаданных этого вызова.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

### `IpfReadWork::records` — CFG-0381 {#cfg-0381-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Классификация: рабочий кредит: расходуемая работа, отдельно от удерживаемой ёмкости. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Операции допуска полных строк этого вызова.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Конечные явные кредиты. Семантика исчерпания определяется контрактом владельца; скалярный диапазон ёмкости эта запись не устанавливает.
- Момент чтения: Явный рабочий вход вызова или хода; сброс только на документированной границе владельца.
- Изменяемость и время жизни: Вызывающий код передаёт оставшиеся кредиты операции или ходу; автоматический возврат и пополнение на каждом poll не подразумеваются.
- Ошибка: Зависящие от компонента WORK_LIMIT/YIELD/status; точное соответствие определяется владельцем.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfReadError, IpfReadStatus`.

## IpfEnvelopeLimits {#cfg-0385-owner-ru}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfEnvelopeLimits::inputBytes` — CFG-0385 {#cfg-0385-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: принятые кодированные байты тела/источника
- Допустимые значения: Ноль разрешён в конверте и запрещает первый входной байт. Счётчик включает игнорируемые байты после конца данных; фрагментация входа не пополняет остаток.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-input. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::outputBytes` — CFG-0386 {#cfg-0386-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: произведённые декодированные байты
- Допустимые значения: Ноль запрещает положительный выход. Учитываются также пропускаемые данные каталога ZIP. Распаковщик может получить окно 1 байт для распознавания конца, но produced&gt;remaining всё равно отвергается.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-output. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::calls` — CFG-0387 {#cfg-0387-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: вызовы продвижения декодера
- Допустимые значения: Перед feed требуется calls&lt;лимита; ноль запрещает первый рабочий вызов. Вызов без продвижения и неуспешный вызов расходуют одну единицу, кроме раннего выхода при уже установленной ошибке или ожидающем выходе.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::WORK_LIMIT / envelope-calls. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::workBytes` — CFG-0388 {#cfg-0388-ru}

- Тип: `std::size_t`. Исходное значение: `= 1073741824`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: посещения входных/выходных байтов и обнулённой памяти
- Допустимые значения: workBytes расходуется монотонно; каждое списание проверяет used&lt;=limit и amount&lt;=limit−used. Ноль допускает лишь маршрут без положительного расхода. Работа временных выделений памяти также учитывается.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::WORK_LIMIT / envelope-work. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::inflaterBytes` — CFG-0389 {#cfg-0389-ru}

- Тип: `std::size_t`. Исходное значение: `= 131072`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: одновременно принадлежащие zlib байты с заголовками
- Допустимые значения: Проверяются items*size+sizeof(Allocation) и allocated+bytes&lt;=inflaterBytes. free возвращает байты в счётчик текущего владения; расход работы не возвращается. Ноль допускает несжатый ввод, но запрещает память распаковщика.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-inflater-backing либо envelope-allocation-overflow; ALLOCATION_FAILURE / envelope-allocation. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::metadataBytes` — CFG-0390 {#cfg-0390-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные байты локальных заголовков ZIP, имён и дополнительных полей
- Допустимые значения: Суммируются header_.size()+nameSize+extraSize для каждого рассматриваемого элемента ZIP. Ноль не вмещает локальный заголовок ZIP. GZIP и несжатый маршрут этот бюджет не расходуют.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / zip-metadata. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

### `IpfEnvelopeLimits::entries` — CFG-0391 {#cfg-0391-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: потоки GZIP либо каталог ZIP или выбранный файл
- Допустимые значения: Каждый принимаемый поток GZIP или элемент ZIP требует entries&lt;лимита. Ноль запрещает первый такой элемент и допускает несжатый IPF. Эта проверка не расширяет поддержку следующих файлов ZIP.
- Момент чтения: SourceEnvelope сохраняет лимиты на одну загрузку. метод feed, распаковщик и парсер ZIP списывают их до принятия соответствующего объёма; среда потокового выполнения сохраняет использованный расход между продолжениями.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferDiagnostic: IpfTransferError::RESOURCE_LIMIT / envelope-entries. Первый отказ сохраняется в результате.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/source_envelope.cpp](../../src/ipf/source_envelope.cpp).

## IpfTransferLimits {#cfg-0392-owner-ru}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfTransferLimits::sourceBytes` — CFG-0392 {#cfg-0392-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Начальный предел источника, не более фиксированной памяти в 8192 байта.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Начальный предел источника, не более фиксированной памяти в 8192 байта.
- Момент чтения: Вложенная конечная подготовка framing/envelope/native IpfTransferOptions относится ко всей загрузке; redirects и попытки не сбрасывают бюджеты.
- Изменяемость и время жизни: Вложенный явный вход следует настройкам владельца; изменение исходного aggregate не меняет текущую загрузку или parser; новой операции нужны явно новые настройки.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferLimits::locationBytes` — CFG-0393 {#cfg-0393-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Предел каждого redirect-поля, не более фиксированной памяти источника.
- Единицы: байты; область определяется контрактом поля
- Объявленные ограничения: Предел каждого redirect-поля, не более фиксированной памяти источника.
- Момент чтения: Вложенная конечная подготовка framing/envelope/native IpfTransferOptions относится ко всей загрузке; redirects и попытки не сбрасывают бюджеты.
- Изменяемость и время жизни: Вложенный явный вход следует настройкам владельца; изменение исходного aggregate не меняет текущую загрузку или parser; новой операции нужны явно новые настройки.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferLimits::authorizationBytes` — CFG-0394 {#cfg-0394-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты полного значения Authorization
- Допустимые значения: 1..8192 в validateOptions. Значение выделяет secret buffer; Basic/явный токен/provider дополнительно должны соблюдать синтаксис и помещаться в лимит. Общий requestBytes может оказаться меньше.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::AUTHORIZATION_FAILURE / authorization-provider; INVALID_OPTIONS / transfer-options. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::headerBytes` — CFG-0395 {#cfg-0395-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты строки состояния и секции заголовков HTTP
- Допустимые значения: 1..UINT32_MAX. Передаётся Beast header_limit(uint32_t); реальный статус и поля должны целиком укладываться. Ноль отвергается до I/O.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options; превышение Beast → HTTP_FAILURE / http-parse. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::headerFields` — CFG-0396 {#cfg-0396-ru}

- Тип: `std::size_t`. Исходное значение: `= 128`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: поля заголовка одного ответа HTTP
- Допустимые значения: Строго &gt;0. on_field_impl увеличивает fields_ и требует &lt;=headerFields. Счётчик нового ответа сбрасывается; общая работа и промежуточные ответы ограничены отдельно.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-header-fields. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::trailerBytes` — CFG-0397 {#cfg-0397-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты завершающей секции заголовков с завершающим CRLF
- Допустимые значения: 2..UINT32_MAX-2. После заголовков Beast получает trailerBytes-2, потому что его счётчик добавляет CRLF последнего блока. Минимум 2 допускает лишь пустую завершающую секцию заголовков по правилам парсера.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options; превышение Beast → HTTP_FAILURE / http-parse. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::trailerFields` — CFG-0398 {#cfg-0398-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: поля завершающей секции одного ответа HTTP
- Допустимые значения: Ноль допускает отсутствие завершающей секции заголовков. Перед каждым её полем требуется ++trailers_&lt;=trailerFields; заголовки маршрутизации и авторизации в этой секции запрещены отдельно.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-trailer-fields. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::bodyBytes` — CFG-0399 {#cfg-0399-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные байты полезных данных HTTP
- Допустимые значения: Ноль допускается настройкой. Beast body_limit и общий work.body ограничивают тело;0 отвергает непустое тело. Данные 200 идут в IPF, прочие допустимые тела всё равно расходуют счётчик.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-payload-bytes; ранний Beast body_limit → HTTP_FAILURE / http-parse. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::wireBytes` — CFG-0400 {#cfg-0400-ru}

- Тип: `std::size_t`. Исходное значение: `= 83886080`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: принятые входные байты транспорта или файла
- Допустимые значения: Ноль допускает только отсутствие входных байтов. HTTP и локальный файл списывают result.wireBytes; перенаправления сохраняют общий счётчик.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-input-capacity для HTTP; RESOURCE_LIMIT / source-input-limit для локального файла. Недопустимая обязательная конфигурация даёт INVALID_OPTIONS / transfer-options до ввода-вывода.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::framingBytes` — CFG-0401 {#cfg-0401-ru}

- Тип: `std::size_t`. Исходное значение: `= 8388608`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: просмотры байтов структуры HTTP
- Допустимые значения: Для каждого put списывается consumed−body; ноль запрещает ненулевую структуру HTTP. Полезные данные ограничены независимо; consumed&lt;body означает отдельную ошибку учёта.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::WORK_LIMIT / http-framing-work. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::chunkExtensionBytes` — CFG-0402 {#cfg-0402-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты расширений заголовка одного блока HTTP
- Допустимые значения: extensions.size()&lt;=chunkExtensionBytes. Ноль допускает блок HTTP без расширений; одновременно расходуется totalChunkExtensionBytes.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-chunk-extensions. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::totalChunkExtensionBytes` — CFG-0403 {#cfg-0403-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные байты расширений блоков HTTP
- Допустимые значения: До принятия выполняется charge(work.extensions, extensions.size(), totalChunkExtensionBytes). Ноль допускает лишь расширения нулевой длины; повторные заголовки расходуют общий остаток.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-chunk-extensions. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp), [src/ipf/transfer_response.cpp](../../src/ipf/transfer_response.cpp).

### `IpfTransferLimits::redirects` — CFG-0404 {#cfg-0404-ru}

- Тип: `std::size_t`. Исходное значение: `= 5`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: переходы перенаправления HTTP
- Допустимые значения: Ноль запрещает первый переход и допускает прямую загрузку. Каждый переход расходует 1; адрес назначения, происхождение ресурса и понижение защиты проверяются независимо. Автоматических повторов подключения в этой ветви нет.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::REDIRECT_REJECTED / redirect-count. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::informationalResponses` — CFG-0405 {#cfg-0405-ru}

- Тип: `std::size_t`. Исходное значение: `= 8`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: неокончательные HTTP ответы
- Допустимые значения: Ноль допускает окончательный ответ без промежуточных. Каждый промежуточный ответ расходует 1, после чего создаётся новый парсер. Общий бюджет не пополняется.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-informational-count. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::attempts` — CFG-0406 {#cfg-0406-ru}

- Тип: `std::size_t`. Исходное значение: `= 6`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: HTTP-подключения всей загрузки
- Допустимые значения: Ноль прекращает загрузку перед первым соединением. Каждый запуск соединения, включая перенаправление, расходует 1; число перенаправлений ограничено отдельно. Локальный файл не создаёт HTTP-подключение.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-attempts. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::parserCalls` — CFG-0407 {#cfg-0407-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: вызовы HTTP-парсер, включая EOF
- Допустимые значения: Ноль запрещает первый вызов HTTP-парсера put или завершение входа EOF. work.calls накапливается через перенаправления и промежуточные ответы; вызовы IPF и декодера конверта имеют отдельные бюджеты.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::WORK_LIMIT / http-parser-work. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::presentedBytes` — CFG-0408 {#cfg-0408-ru}

- Тип: `std::size_t`. Исходное значение: `= 268435456`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные просмотры байтов запроса, копий, парсера и профилей
- Допустимые значения: Ноль запрещает положительный расход подготовки или копирования. Один входной байт может учитываться многократно: при копировании, вызове put, сохранении суффикса и публикации профиля. Счётчик отражает суммарную работу с байтами.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / http-request-capacity или http-input-capacity; RESOURCE_LIMIT / source-input-limit при чтении локального файла; WORK_LIMIT / http-parser-work или http-framing-work при повторном предъявлении/сдвиге данных. Недопустимая обязательная конфигурация даёт INVALID_OPTIONS / transfer-options до ввода-вывода.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::cleanupTurns` — CFG-0409 {#cfg-0409-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: обороты резерва нативного завершения
- Допустимые значения: Ноль допускается. cleanupWork получает min(nativeWork.steps/handlers, cleanupTurns); этот ресурс вычитается из общего nativeWork. Если завершению не хватает продвижения, фиксируется WORK_LIMIT и выполняется резервное освобождение ресурсов.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::WORK_LIMIT / transfer-cleanup-work; возможна первичная TLS/TRANSPORT_FAILURE. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::turns` — CFG-0410 {#cfg-0410-ru}

- Тип: `std::size_t`. Исходное значение: `= 1000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: циклы продвижения всей ограниченной операции
- Допустимые значения: Каждая контрольная точка расходует 1 единицу turns; ноль запрещает первую такую точку. Отмена и дедлайн проверяются до списания; calls и attempts имеют отдельные бюджеты.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::WORK_LIMIT / transfer-turns. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::inputBytes` — CFG-0411 {#cfg-0411-ru}

- Тип: `std::size_t`. Исходное значение: `= 131072`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты фиксированного входного HTTP окна
- Допустимые значения: Строго &gt;0; размер выделения входит в residentBytes. Копия должна поместиться в остаток inputBytes−inputSize. Слишком короткое окно может отказать для неделимого заголовка или входа парсера.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options; RESOURCE_LIMIT / http-input-capacity. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::requestBytes` — CFG-0412 {#cfg-0412-ru}

- Тип: `std::size_t`. Исходное значение: `= 32768`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты целого HTTP запроса
- Допустимые значения: Строго &gt;0. Каждый фрагмент GET, Host, даты, авторизации и CRLF должен поместиться в остаток requestBytes до отправки. Размер фиксированного выходного места входит в residentBytes.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options; RESOURCE_LIMIT / http-request-capacity. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::residentBytes` — CFG-0413 {#cfg-0413-ru}

- Тип: `std::size_t`. Исходное значение: `= 16 * 1024 * 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты собственного хранение передачи
- Допустимые значения: Покрывает собственные состояния узла, управления, ответа, источника и пути; входной буфер, запрос, секрет, копии доверенных данных, DNS, распаковщик и остальные явно учитываемые резервы. Проверяется в validateOptions и повторно полной формулой резервирования. Ноль не вмещает обязательные структуры.
- Момент чтения: validateOptions выполняется до копирования/SDK. Operation и BasicTransferResponse используют сохранённые лимиты при одной загрузке; потоковые продолжения сохраняют использованный расход.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::RESOURCE_LIMIT / transfer-backing либо transfer-resident; дополнительный stream хранение → connection-stream-backing. Невалидная обязательная конфигурация получает INVALID_OPTIONS / transfer-options до I/O.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferLimits::envelope` — CFG-0414 {#cfg-0414-ru}

- Тип: `IpfEnvelopeLimits`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Независимые пределы encoded/decoded/work/backing одной загрузки.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Вложенная конечная подготовка framing/envelope/native IpfTransferOptions относится ко всей загрузке; redirects и попытки не сбрасывают бюджеты.
- Изменяемость и время жизни: Вложенный явный вход следует настройкам владельца; изменение исходного aggregate не меняет текущую загрузку или parser; новой операции нужны явно новые настройки.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

## IpfTransferOptions {#cfg-0415-owner-ru}

[include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp)

### `IpfTransferOptions::ipf` — CFG-0415 {#cfg-0415-ru}

- Тип: `IpfReadOptions`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Существующие независимые пределы parser/output и положительные квоты хода.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Существующие независимые пределы parser/output и положительные квоты хода.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::limits` — CFG-0416 {#cfg-0416-ru}

- Тип: `IpfTransferLimits`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Пределы источника, framing, копирования и памяти передачи.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::dnsServers` — CFG-0417 {#cfg-0417-ru}

- Тип: `std::vector<transport::DnsServer>`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явно заданные числовые IPv4 DNS-серверы, не более четырёх.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Явно заданные числовые IPv4 DNS-серверы, не более четырёх.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::dnsPort` — CFG-0418 {#cfg-0418-ru}

- Тип: `std::uint16_t`. Исходное значение: `= 53`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: UDP/TCP порт DNS
- Допустимые значения: 1..65535 по uint16_t и явной проверке ненулевого значения. Задаётся только для явных DNS-серверов; настройки TLS.dns.servers должны быть пусты, чтобы не создать второй список.
- Момент чтения: validateOptions, затем перенос в байтовые настройки открытого или защищённого транспорта при соединении.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при нуле.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::nativeLimits` — CFG-0419 {#cfg-0419-ru}

- Тип: `transport::TcpLimits`. Исходное значение: `{4096, 4096, 64, 16, 65536, 8192, 96 * 1024 * 1024}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Нативная память.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::overallTimeout` — CFG-0420 {#cfg-0420-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{300000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Общий положительный срок всей загрузки. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: Operation при создании фиксирует Clock::now()+overallTimeout; контрольные точки проверяют этот дедлайн. Он охватывает загрузку HTTP и локального файла.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::connectTimeout` — CFG-0421 {#cfg-0421-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{30000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Положительный срок отдельно для DNS и TCP-соединения. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: connect передаёт одно connectTimeout в два поля transport::TcpOptions: dnsTimeout и connectTimeout. Транспорт запускает их по отдельности в фазах RESOLVING и CONNECTING; общий срок загрузки задаёт overallTimeout.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::readTimeout` — CFG-0422 {#cfg-0422-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{60000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Положительный срок отсутствия продвижения чтения транспорта. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: connect передаёт readTimeout в transport::TcpOptions для нативной фазы чтения. Настройка не применяется к чтению локального файла; общий срок загрузки задаёт overallTimeout.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::writeTimeout` — CFG-0423 {#cfg-0423-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{60000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Положительный срок отсутствия продвижения ожидающей записи транспорта. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: connect передаёт writeTimeout в transport::TcpOptions для нативной фазы ожидающей записи. Локальный файл не использует нативную отправку; общий срок загрузки задаёт overallTimeout.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::pollInterval` — CFG-0424 {#cfg-0424-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{1}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..100 включительно. Ноль и отрицательные значения запрещены. Интервал продвижения применяется также между проверками отмены и завершения.
- Момент чтения: validateOptions до I/O; общий дедлайн фиксируется при создании Operation, фазовые значения передаются нативным настройкам.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::trustPem` — CFG-0425 {#cfg-0425-ru}

- Тип: `std::string`. Исходное значение: `{}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явные обычные PEM anchors для HTTPS, не более tls.limits.trustBytes.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Явные обычные PEM anchors для HTTPS, не более tls.limits.trustBytes.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::handshakeTimeout` — CFG-0426 {#cfg-0426-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{30000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Положительный срок TLS-handshake защищённого маршрута. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: validateOptions проверяет значение при создании настроек. connect копирует его в EndpointTls только для защищённого источника; общий срок загрузки задаёт overallTimeout.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::closeTimeout` — CFG-0427 {#cfg-0427-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{1000}`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды
- Допустимые значения: 1..INT_MAX включительно. Положительный срок протокольного закрытия TLS. validateOptions требует допустимую длительность при любой загрузке, включая маршруты, которые позднее не используют эту фазу.
- Момент чтения: validateOptions проверяет значение при создании настроек. connect копирует его в EndpointTls только для защищённого источника; завершение также ограничено общими ресурсами очистки и загрузки.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: IpfTransferError::INVALID_OPTIONS / transfer-options при неверной длительности; истечение общей границы DEADLINE / transfer-deadline, фазовые отказы сохраняют TRANSPORT_FAILURE/TLS_FAILURE и нижнюю причину.
- Исходники потребителя: [include/qd/ipf/instrument_profile_url_reader.hpp](../../include/qd/ipf/instrument_profile_url_reader.hpp), [src/ipf/url_reader.cpp](../../src/ipf/url_reader.cpp).

### `IpfTransferOptions::tls` — CFG-0428 {#cfg-0428-ru}

- Тип: `transport::TlsOptions`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Явные квалифицированные резервы host/shared/private одного образа.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::tlsWork` — CFG-0429 {#cfg-0429-ru}

- Тип: `transport::TlsWorkVector`. Исходное значение: `{}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Работа TLS всего reader, списываемая между загрузками и redirects, без сброса.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::nativeWork` — CFG-0430 {#cfg-0430-ru}

- Тип: `transport::TcpWork`. Исходное значение: `{ 1000000, 1000000, 536870912, 1000000, 1000000, 134217728, (std::numeric_limits<std::uint64_t>::max)()}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Нативная работа всей загрузки между операциями; DNS-единицы следуют конечному консервативному профилю, без unlimited sentinel.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

### `IpfTransferOptions::byteWork` — CFG-0431 {#cfg-0431-ru}

- Тип: `protocol::LiveWork`. Исходное значение: `{1000000, 1000000000, 1000000000, 64ULL * 1024 * 1024 * 1024}`
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Логическая работа подготовки/копирования всей загрузки, включая настройку DNS; не выделенные байты; никогда не возвращается.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(const IpfTransferOptions &amp;options = {}) / [[nodiscard]] static std::expected&lt;std::unique_ptr&lt;InstrumentProfileReader&gt;, IpfTransferDiagnostic&gt; create(transport::RuntimeBootstrap &amp;bootstrap, const IpfTransferOptions &amp;options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: HttpAuthorizationError, IpfTransferError, IpfTransferStatus; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `HttpAuthorizationError, IpfTransferError, IpfTransferStatus`.

## IpfWriteLimits {#cfg-0432-owner-ru}

[include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp)

### `IpfWriteLimits::profiles` — CFG-0432 {#cfg-0432-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные профили
- Допустимые значения: profiles.size()&lt;=profiles до чтения содержимого. Ноль допускает пустой список, который может всё равно породить служебный вывод согласно options.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::PROFILE_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::types` — CFG-0433 {#cfg-0433-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: типы/форматы выдаваемого файла
- Допустимые значения: Добавление нового формата требует formats_.size()&lt;types; существующий формат переиспользуется. Ноль допускает только сценарий без создаваемого формата.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::TYPE_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::fieldsPerType` — CFG-0434 {#cfg-0434-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: поля одного формата
- Допустимые значения: Для непустых профилей требуется &gt;=2 и customFieldCount&lt;=fieldsPerType−2. При добавлении каждого нового поля проверяется format.count&lt;fieldsPerType. Значения 0 и 1 могут обслужить только пустой ввод.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::FIELD_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::headerUnits` — CFG-0435 {#cfg-0435-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имён сформированных заголовков
- Допустимые значения: Сумма headers_+units должна быть &lt;=headerUnits; add проверяет переполнение. Ноль запрещает положительный заголовок. UTF-8-вывод и экранирование ограничены отдельным outputBytes.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::HEADER_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::inputUnits` — CFG-0436 {#cfg-0436-ru}

- Тип: `std::size_t`. Исходное значение: `= 16777216`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 исходных профилей
- Допустимые значения: Суммируются retainedUnits() входных профилей. Первый профиль проходит предварительную проверку до compose, следующие проверяются при обходе. Ноль допускает только нулевой расход.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::INPUT_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::outputBytes` — CFG-0437 {#cfg-0437-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты полного сформированного CSV/IPF документа
- Допустимые значения: Перед каждым выходным байтом требуется bytes_.size()&lt;outputBytes. Ноль запрещает даже первый байт маркера или перевода строки. Бюджет включает экранирование и служебный текст.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::OUTPUT_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

### `IpfWriteLimits::work` — CFG-0438 {#cfg-0438-ru}

- Тип: `std::size_t`. Исходное значение: `= 33554432`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: логические посещения/байты работы формирования
- Допустимые значения: Предварительная проверка списывает удерживаемые единицы текста, число стандартных и число пользовательских полей. compose дополнительно учитывает обход и каждый выходной байт. Ноль допускает лишь маршрут без положительной работы.
- Момент чтения: composeInstrumentProfiles выполняет предварительную проверку и строит полный документ до записи в поток/файл; опции передаются на один вызов.
- Изменяемость и время жизни: Параметры задаются каждым вызовом; следующий вызов может передать другой инициализированный набор. Они не меняют уже возвращённый результат.
- Ошибка: std::expected с IpfWriteDiagnostic{IpfWriteError::WORK_LIMIT,profileIndex,fieldIndex}; bad_alloc → ALLOCATION_FAILURE. Непредставимый UTF-16 и невозможная точность имеют отдельные причины.
- Исходники потребителя: [include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp), [src/ipf/writer.cpp](../../src/ipf/writer.cpp).

## IpfWriteOptions {#cfg-0439-owner-ru}

[include/qd/ipf/instrument_profile_writer.hpp](../../include/qd/ipf/instrument_profile_writer.hpp)

### `IpfWriteOptions::limits` — CFG-0439 {#cfg-0439-ru}

- Тип: `IpfWriteLimits`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: составной вход: вложенные типизированные настройки, без новой скалярной единицы. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Пределы ввода, формата, работы и полного вывода.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] std::expected&lt;std::string, IpfWriteDiagnostic&gt; composeInstrumentProfiles(std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {}) / [[nodiscard]] IpfWriteResult writeInstrumentProfiles(std::ostream &amp;output, std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {}) / [[nodiscard]] IpfWriteResult writeInstrumentProfilesFile(const std::filesystem::path &amp;path, std::span&lt;const InstrumentProfile&gt; profiles, IpfWriteOptions options = {})
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: Перечень ошибок владельца: IpfWriteError; точное соответствие поля ветви ошибки НЕИЗВЕСТНО и отдельно не установлено.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `IpfWriteError`.

## ProfileLimits {#cfg-0440-owner-ru}

[include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp)

### `ProfileLimits::customFields` — CFG-0440 {#cfg-0440-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: пользовательские непустые поля профиля
- Допустимые значения: Новое пользовательское поле с непустым значением требует custom_.size()&lt;customFields. Ноль не ограничивает стандартные поля. Присваивание пустого значения удаляет пользовательское поле и не требует нового места.
- Момент чтения: InstrumentProfile сохраняет лимиты без числовой валидации конструктора. методы установки и чтения численных значений проверяют их до принятия нового значения/разбора.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileError::LIMIT_EXCEEDED. Выделение памяти для std::u16string и таблицы полей может отдельно выбросить std::bad_alloc. Разбор численного и текстового значения имеет собственные ошибки.
- Исходники потребителя: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::nameUnits` — CFG-0441 {#cfg-0441-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имени поля
- Допустимые значения: name.size()&lt;=nameUnits в методах установки и чтения численного значения. Ноль допускает лишь пустое имя до дальнейшей логики типа; обычные имена стандартных полей не поместятся.
- Момент чтения: InstrumentProfile сохраняет лимиты без числовой валидации конструктора. методы установки и чтения численных значений проверяют их до принятия нового значения/разбора.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileError::LIMIT_EXCEEDED. Выделение памяти для std::u16string и таблицы полей может отдельно выбросить std::bad_alloc. Разбор численного и текстового значения имеет собственные ошибки.
- Исходники потребителя: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::valueUnits` — CFG-0442 {#cfg-0442-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 канонического значения поля
- Допустимые значения: Канонический text.size()&lt;=valueUnits, включая форматирование численного значения при установке. Ноль допускает пустое текстовое значение. Исходный численный текст имеет отдельный inputUnits.
- Момент чтения: InstrumentProfile сохраняет лимиты без числовой валидации конструктора. методы установки и чтения численных значений проверяют их до принятия нового значения/разбора.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileError::LIMIT_EXCEEDED. Выделение памяти для std::u16string и таблицы полей может отдельно выбросить std::bad_alloc. Разбор численного и текстового значения имеет собственные ошибки.
- Исходники потребителя: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::totalUnits` — CFG-0443 {#cfg-0443-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: суммарные кодовые единицы UTF-16 удерживаемых имён/значений
- Допустимые значения: Для стандартного поля учитывается text; для непустого пользовательского — name+text. При замене вычитается прежний расход; новый должен поместиться в totalUnits. Ноль допускает нулевое удержание.
- Момент чтения: InstrumentProfile сохраняет лимиты без числовой валидации конструктора. методы установки и чтения численных значений проверяют их до принятия нового значения/разбора.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileError::LIMIT_EXCEEDED. Выделение памяти для std::u16string и таблицы полей может отдельно выбросить std::bad_alloc. Разбор численного и текстового значения имеет собственные ошибки.
- Исходники потребителя: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

### `ProfileLimits::inputUnits` — CFG-0444 {#cfg-0444-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 входа разбора/преобразования
- Допустимые значения: setField ограничивает исходное value.size(); метод чтения численного значения ограничивает текст, который будет разбирать. Ноль разрешает пустой текст; канонический результат имеет независимый valueUnits/totalUnits.
- Момент чтения: InstrumentProfile сохраняет лимиты без числовой валидации конструктора. методы установки и чтения численных значений проверяют их до принятия нового значения/разбора.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: ProfileError::LIMIT_EXCEEDED. Выделение памяти для std::u16string и таблицы полей может отдельно выбросить std::bad_alloc. Разбор численного и текстового значения имеет собственные ошибки.
- Исходники потребителя: [include/qd/ipf/instrument_profile.hpp](../../include/qd/ipf/instrument_profile.hpp), [src/ipf/instrument_profile.cpp](../../src/ipf/instrument_profile.cpp).

## OptionChainLimits {#cfg-0445-owner-ru}

[include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp)

### `OptionChainLimits::profiles` — CFG-0445 {#cfg-0445-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: рассмотренные входные профили за время работы построителя
- Допустимые значения: build проверяет входной диапазон. Каждый addProfile расходует 1 profiles, включая не являющийся опционом профиль, принятый с пропуском. Ноль допускает лишь отсутствие добавлений.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::chains` — CFG-0446 {#cfg-0446-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: созданные цепочки по символу
- Допустимые значения: Создание нового ключа цепочки списывает 1 chains. Уже имеющаяся цепочка переиспользуется; один опцион может попасть в несколько цепочек по метаданным. Ноль не допускает новую цепочку.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::series` — CFG-0447 {#cfg-0447-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: серии во всех цепочках
- Допустимые значения: Новая серия списывает 1 series, существующая переиспользуется по сравнению метаданные. Ноль допускает лишь операции без новой серии.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::options` — CFG-0448 {#cfg-0448-ru}

- Тип: `std::size_t`. Исходное значение: `= 200000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: позиции опционов call/put во всех сериях
- Допустимые значения: Новая позиция расходует 1 options. Замена существующей цены исполнения вычитает прежние retainedUnits без роста options. Ноль запрещает новую позицию опциона.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::retainedUnits` — CFG-0449 {#cfg-0449-ru}

- Тип: `std::size_t`. Исходное значение: `= 16777216`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 ключей/метаданных/профилей графа
- Допустимые значения: Учитываются ключи цепочек, метаданные серий и копии профилей по позициям. Проверяемое списание ограничивает их общую сумму. При замене старый профиль освобождает соответствующий расход. Ноль допускает пустой граф.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::stagingUnits` — CFG-0450 {#cfg-0450-ru}

- Тип: `std::size_t`. Исходное значение: `= 50331648`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: пиковые логические кодовые единицы подготовки нового графа
- Допустимые значения: До копии учитываются старый граф, новый граф, старые/новые symbol keys и три расхода входного профиля по указанной формуле. Ноль допускает лишь нулевые промежуточные данные. Это отдельная граница от retainedUnits.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).

### `OptionChainLimits::workUnits` — CFG-0451 {#cfg-0451-ru}

- Тип: `std::size_t`. Исходное значение: `= 1073741824`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительная работа построителя
- Допустимые значения: Каждый профиль списывает 1+retainedUnits, затем обход старого графа, chains/series/options и дальнейшие сравнения. workUnits_ сохраняется между addProfile;0 запрещает первый вход.
- Момент чтения: OptionChainsBuilder принимает лимиты при создании; addProfile готовит отдельный кандидат и публикует его операцией swap только после всех проверок.
- Изменяемость и время жизни: Снимок принимается при создании владельца и далее неизменяем; изменение требует нового экземпляра.
- Ошибка: std::expected: OptionChainError::LIMIT_EXCEEDED. Аллокации графа/строк могут отдельно бросить std::bad_alloc; прежний граф остаётся до успешной публикации.
- Исходники потребителя: [include/qd/ipf/option_chains.hpp](../../include/qd/ipf/option_chains.hpp), [src/ipf/option_chains.cpp](../../src/ipf/option_chains.cpp).
