# Расписания

[English](schedule.md) · [Оглавление справочника](../CONFIGURATION-REFERENCE.ru.md)

Контракты потребителей отдельных полей и сведения деклараций/исходников с оговорённой доказательностью явно различаются ниже. НЕИЗВЕСТНО обозначает недостающее сведение. Известные объявленные сведения сохраняются; их семантическая проверка ещё не завершена. Ссылки указывают декларацию или потребителя и не означают нового запуска тестов. Значения описывают текущий API; принятые изменения перечислены в [архитектурном контракте](../CONFIGURATION.ru.md).

- [CalendarDefaults](schedule.ru.md#cfg-0640-owner-ru) — `include/qd/schedule/schedule.hpp`
- [ScheduleLimits](schedule.ru.md#cfg-0642-owner-ru) — `include/qd/schedule/schedule.hpp`

## CalendarDefaults {#cfg-0640-owner-ru}

[include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp)

### `CalendarDefaults::versionMillis` — CFG-0640 {#cfg-0640-ru}

- Тип: `std::int64_t`. Исходное значение: `= 0`
- Классификация: явная зависимость или данные: время жизни должен устанавливать владелец. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Типизированная версия сравнения; более старые версии игнорируются.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явная версионированная транзакция setDefaults; заимствованные entries копируются; старые версии игнорируются.
- Изменяемость и время жизни: Атомарное версионированное обновление поколения; ошибка сохраняет текущее поколение и удержания reader.
- Ошибка: ScheduleError; RESOURCE_LIMIT/ALLOCATION/CONCURRENT_CHANGE и типизированные ошибки parser/query; частичного поколения нет.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ScheduleError`.

### `CalendarDefaults::entries` — CFG-0641 {#cfg-0641-ru}

- Тип: `std::span<const CalendarDefaultsEntry>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: явная зависимость или данные: время жизни должен устанавливать владелец. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Полные заимствованные entries в порядке источника.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Скалярный диапазон неприменим. Вложенные ограничения или возвращаемые наблюдения сохраняют объявленный смысл.
- Момент чтения: Явная версионированная транзакция setDefaults; заимствованные entries копируются; старые версии игнорируются.
- Изменяемость и время жизни: Атомарное версионированное обновление поколения; ошибка сохраняет текущее поколение и удержания reader.
- Ошибка: ScheduleError; RESOURCE_LIMIT/ALLOCATION/CONCURRENT_CHANGE и типизированные ошибки parser/query; частичного поколения нет.
- Дополнительный перечень ошибок/статусов владельца (без соответствия полю): `ScheduleError`.

## ScheduleLimits {#cfg-0642-owner-ru}

[include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp)

### `ScheduleLimits::inputUnits` — CFG-0642 {#cfg-0642-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 определения инструмента и торговой площадки
- Допустимые значения: Длина instrumentDefinition и отдельно заданной торговой площадки venue должна быть &lt;=inputUnits до разбора и копирования. Ноль принимает только пустой текст там, где его допускает грамматика; встроенные значения по умолчанию имеют собственные проверки.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::defaultUnits` — CFG-0643 {#cfg-0643-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 имён и определений значений по умолчанию
- Допустимые значения: Сумма entry.name+definition типизированных записей&lt;=defaultUnits. Начальное создание также требует sizeof(CALENDAR_BYTES)&lt;=defaultUnits и&lt;=stagingBytes/sizeof(char16_t), поэтому 0 не создаёт стандартный контекст.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::defaultEntries` — CFG-0644 {#cfg-0644-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: именованные списки и торговые площадки одного поколения
- Допустимые значения: entries.size() типизированного входа&lt;=defaultEntries; встроенный загрузчик проверяет то же при добавлении. Ноль не вмещает встроенный непустой набор. Верхний максимум отдельно не задан.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::properties` — CFG-0645 {#cfg-0645-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: различные объединённые свойства календаря
- Допустимые значения: Перед вставкой props.size()&lt;properties. Прежний одинаковый ключ удаляется до проверки, поэтому замена не требует лишнего счётчик; память/работа подготовки расходуются отдельно. Ноль запрещает непустой набор.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dateValues` — CFG-0646 {#cfg-0646-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные создаваемые значения дат
- Допустимые значения: chargeDates требует счётчик&lt;=dateValues-construction.dateValues, затем резервирует счётчик*sizeof(int32_t). Ноль допускает лишь отсутствие построения дат; объединения/промежуточные значения также расходуют бюджет.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dayDefinitions` — CFG-0647 {#cfg-0647-ru}

- Тип: `std::size_t`. Исходное значение: `= 8192`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные абстрактные определения дня
- Допустимые значения: Каждый dayDefinition требует construction.dayDefinitions&lt;dayDefinitions и списывает 1 до создания модели. Ноль запрещает первый такой разбор; это накопительный счётчик за операцию.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::sessionsPerDay` — CFG-0648 {#cfg-0648-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: абстрактные сеансы одного дня, включая добавленные промежутки
- Допустимые значения: appendSession требует sessions.size()&lt;sessionsPerDay. Ноль не допускает первую сессию; промежутки, добавленные для полноты дня, тоже занимают записи.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::work` — CFG-0649 {#cfg-0649-ru}

- Тип: `std::size_t`. Исходное значение: `= 16000000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: логические посещения/копирования при построении
- Допустимые значения: Construction::inspect списывает amount из work по проверке остатка. Расход монотонен за операцию;0 не вмещает чтение встроенных значений по умолчанию при стандартном create.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::stagingBytes` — CFG-0650 {#cfg-0650-ru}

- Тип: `std::size_t`. Исходное значение: `= 33554432`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные логические байты подготовки
- Допустимые значения: Construction::allocate расходует каждый запрошенный размер без возврата при освобождении временного объекта. Ноль не вмещает фиксированные объекты контекста. Представимость размеров проверяется отдельно.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedBytes` — CFG-0651 {#cfg-0651-ru}

- Тип: `std::size_t`. Исходное значение: `= 67108864`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: общие логические байты удерживаемых моделей
- Допустимые значения: create требует &gt;=sizeof(Context)+sizeof(Ledger). acquire дополнительно проверяет bytes+sizeof(Lease) по остатку общего счётчика владения. Кандидаты строятся во временной памяти подготовки до acquire или во время него; retainedBytes не ограничивает все предварительные выделения памяти.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedGenerations` — CFG-0652 {#cfg-0652-ru}

- Тип: `std::size_t`. Исходное значение: `= 4`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые поколения значений по умолчанию
- Допустимые значения: acquire(GENERATION) требует свободного места в счётчике. Ноль запрещает первое create со встроенным поколением. Читатели старых данных удерживают поколение и его байты после обновления.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedSchedules` — CFG-0653 {#cfg-0653-ru}

- Тип: `std::size_t`. Исходное значение: `= 256`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: точные идентичности календарей
- Допустимые значения: acquire(IDENTITY) требует счётчик&lt;retainedSchedules. Ноль допускает контекст, но не новую идентичность Schedule; сборка неактивных записей кэша может освободить неактивные идентичности перед созданием.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedRuleModels` — CFG-0654 {#cfg-0654-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: модели правил для пар идентичности и поколения
- Допустимые значения: Каждая новая модель правил занимает RULES до освобождения последнего владельца. Ноль допускает пустой контекст и запрещает создание модели правил. Кандидат сначала расходует временную память подготовки.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::retainedBundles` — CFG-0655 {#cfg-0655-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: текущие, подготавливаемые и удерживаемые наборы
- Допустимые значения: makeBundle/acquire(BUNDLE) учитывает полный набор. Ноль запрещает создание начального набора. При обновлении может потребоваться одновременное место для прежнего и нового наборов.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::inactiveCacheTarget` — CFG-0656 {#cfg-0656-ru}

- Тип: `std::size_t`. Исходное значение: `= 64`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: целевое число неактивных идентичностей кэша
- Допустимые значения: Мягкая цель сборки при явных фабричных операциях: удаляется разность inactive−inactiveCacheTarget; нехватка retainedSchedules может потребовать дополнительное освобождение. Ноль запрашивает удаление всех доступных неактивных записей. Записи с активными владельцами сохраняются.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: Превышение числа неактивных записей над целью запускает сборку; отдельного RESOURCE_LIMIT за превышение самой цели нет. Сборка может получить ScheduleError::RESOURCE_LIMIT при исчерпании work/stagingBytes или других жёстких квот, ALLOCATION при std::bad_alloc. Активные записи пропускаются.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp).

### `ScheduleLimits::queryWork` — CFG-0657 {#cfg-0657-ru}

- Тип: `std::size_t`. Исходное значение: `= 100000`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: единицы работы одного запроса даты
- Допустимые значения: queryLimits заменяет work на queryWork. Поиск в кэше также вызывает inspect; ноль может вызвать отказ уже при этом поиске. Ресурсная ошибка возвращается отдельно от отсутствующего календарного результата.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::queryStagingBytes` — CFG-0658 {#cfg-0658-ru}

- Тип: `std::size_t`. Исходное значение: `= 4194304`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: накопительные логические байты одного запроса даты
- Допустимые значения: queryLimits заменяет stagingBytes на queryStagingBytes. Построение DayModel, сеансов и кэша расходует этот бюджет;0 может обслужить готовый результат из кэша только при отсутствии требуемой аллокации.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedDays` — CFG-0659 {#cfg-0659-ru}

- Тип: `std::size_t`. Исходное значение: `= 4096`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые неизменяемые модели дней
- Допустимые значения: acquire(DAY) списывает 1 при готовом кандидате. Ноль запрещает новое удерживаемое DayModel; отдельно действует queryStagingBytes до допуска. Повторный найденный результат кэша может разделять прежний объект.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedSessions` — CFG-0660 {#cfg-0660-ru}

- Тип: `std::size_t`. Исходное значение: `= 65536`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: сессии внутри всех живых моделей дней
- Допустимые значения: acquire(SESSION,...,result.sessions.size()) ограничивает общий счётчик. Ноль допускает только отсутствие сессий; предварительное построение ограничено queryStagingBytes и querySessionsPerDay.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::querySessionsPerDay` — CFG-0661 {#cfg-0661-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: физические записи сессий одного создаваемого дня
- Допустимые значения: append требует sessions.size()&lt;querySessionsPerDay; учитываются объединённые дни и промежутки. Ноль запрещает первую запись. Отдельно действуют предел абстрактных сеансов sessionsPerDay и общий retainedSessions.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::dayCacheEntries` — CFG-0662 {#cfg-0662-ru}

- Тип: `std::size_t`. Исходное значение: `= 1024`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: слабые записи одного кэша дней
- Допустимые значения: Ноль отключает публикацию кэша, но не построение дня. Положительное значение ограничивает сохранённые weak_ptr плюс новый результат; истёкшие записи опускаются. Это не сильное удержание DayModel.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: Ноль отключает публикацию слабого кэша после построения дня. Достигнутая ёмкость сокращает перенос прежних записей; отдельного RESOURCE_LIMIT за заполнение этого кэша нет. Построение дня/нового кэша расходует work/stagingBytes и жёсткие квоты удерживаемых объектов: возможны ScheduleError::RESOURCE_LIMIT и ALLOCATION. Неудачный compare_exchange публикации игнорируется, готовый день возвращается.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).

### `ScheduleLimits::retainedDayCaches` — CFG-0663 {#cfg-0663-ru}

- Тип: `std::size_t`. Исходное значение: `= 16`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: живые неизменяемые версии кэша дней
- Допустимые значения: Каждый новый кандидат кэша занимает DAY_CACHE в общем счётчике владения. Ноль запрещает публикацию при dayCacheEntries&gt;0. При dayCacheEntries=0 этот ресурс не требуется. Читатели и конкурирующие кандидаты могут удерживать прежнюю версию кэша.
- Момент чтения: ScheduleDataContext захватывает ScheduleLimits. Construction расходует отдельные бюджеты операции; acquire и счётчик владения учитывают опубликованные и удерживаемые модели. Запросы дат используют queryWork/queryStagingBytes.
- Изменяемость и время жизни: ScheduleDataContext::create копирует лимиты в Context и Ledger; существующий контекст не перенастраивается изменением исходного аргумента. Отдельный Schedule::getTradingVenues принимает лимиты только для текущего вызова и возвращает обычный вектор строк.
- Ошибка: std::expected: ScheduleError::RESOURCE_LIMIT при бюджете/переполнении/length_error; ALLOCATION при bad_alloc. Календарная грамматика, NOT_READY и диапазоны дат имеют отдельные ошибки.
- Исходники потребителя: [include/qd/schedule/schedule.hpp](../../include/qd/schedule/schedule.hpp), [src/schedule/queries.cpp](../../src/schedule/queries.cpp), [src/schedule/parser.cpp](../../src/schedule/parser.cpp), [src/schedule/schedule.cpp](../../src/schedule/schedule.cpp).
