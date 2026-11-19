# Составные справочные значения

[English](glossary.md) · [Оглавление справочника](../CONFIGURATION-REFERENCE.ru.md)

Контракты потребителей отдельных полей и сведения деклараций/исходников с оговорённой доказательностью явно различаются ниже. НЕИЗВЕСТНО обозначает недостающее сведение. Известные объявленные сведения сохраняются; их семантическая проверка ещё не завершена. Ссылки указывают декларацию или потребителя и не означают нового запуска тестов. Значения описывают текущий API; принятые изменения перечислены в [архитектурном контракте](../CONFIGURATION.ru.md).

- [AdditionalUnderlyingLimits](glossary.ru.md#cfg-0341-owner-ru) — `include/qd/glossary/additional_underlyings.hpp`
- [PriceIncrementLimits](glossary.ru.md#cfg-0343-owner-ru) — `include/qd/glossary/price_increments.hpp`

## AdditionalUnderlyingLimits {#cfg-0341-owner-ru}

[include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp)

### `AdditionalUnderlyingLimits::textUnits` — CFG-0341 {#cfg-0341-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 канонического текста
- Допустимые значения: Фактическая длина&lt;=min(textUnits,INT32_MAX). Null/пустой вход дают пустое значение до checkText; число элементов и итоговый канонический вывод проверяются отдельно.
- Момент чтения: valueOf читает лимиты в каждом вызове разбора/создания канонического значения; готовое значение не перечитывает настройки.
- Изменяемость и время жизни: Параметры ограничивают один вызов valueOf. Неизменяемый результат содержит каноническое значение, без сохранённого бюджета; следующий вызов может выбрать другие лимиты.
- Ошибка: std::length_error("additional underlying text limit exceeded") при превышении; переполнение размера имеет отдельный length_error; неверная грамматика и bad_alloc не превращаются в успешное значение.
- Исходники потребителя: [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp), [src/glossary/additional_underlyings.cpp](../../src/glossary/additional_underlyings.cpp), [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp).

### `AdditionalUnderlyingLimits::entries` — CFG-0342 {#cfg-0342-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: входные элементы дополнительных базовых активов
- Допустимые значения: Каждый элемент исходного списка расходует entries до разбора/дедупликации; конструирование из map проверяет map.size(). Ноль допускает пустое значение; непустой список отвергается.
- Момент чтения: valueOf читает лимиты в каждом вызове разбора/создания канонического значения; готовое значение не перечитывает настройки.
- Изменяемость и время жизни: Параметры ограничивают один вызов valueOf. Неизменяемый результат содержит каноническое значение, без сохранённого бюджета; следующий вызов может выбрать другие лимиты.
- Ошибка: std::length_error("additional underlying entry limit exceeded") при превышении; переполнение размера имеет отдельный length_error; неверная грамматика и bad_alloc не превращаются в успешное значение.
- Исходники потребителя: [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp), [src/glossary/additional_underlyings.cpp](../../src/glossary/additional_underlyings.cpp), [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp).

## PriceIncrementLimits {#cfg-0343-owner-ru}

[include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp)

### `PriceIncrementLimits::textUnits` — CFG-0343 {#cfg-0343-ru}

- Тип: `std::size_t`. Исходное значение: `= 1048576`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: кодовые единицы UTF-16 текста приращений
- Допустимые значения: Длина входа и канонического вывода&lt;=min(textUnits,INT32_MAX). Null/пустой вход возвращает пустое значение до проверки; специальные пустые представления проходят грамматику отдельно.
- Момент чтения: valueOf читает лимиты в каждом вызове разбора/создания канонического значения; готовое значение не перечитывает настройки.
- Изменяемость и время жизни: Параметры ограничивают один вызов valueOf. Неизменяемый результат содержит каноническое значение, без сохранённого бюджета; следующий вызов может выбрать другие лимиты.
- Ошибка: std::length_error("price increments text limit exceeded") при превышении; переполнение размера имеет отдельный length_error; неверная грамматика и bad_alloc не превращаются в успешное значение.
- Исходники потребителя: [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp), [src/glossary/price_increments.cpp](../../src/glossary/price_increments.cpp), [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp).

### `PriceIncrementLimits::ranges` — CFG-0344 {#cfg-0344-ru}

- Тип: `std::size_t`. Исходное значение: `= 16384`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: диапазоны приращений
- Допустимые значения: Число диапазонов&lt;=ranges; для текста счётчик=число точек с запятой+1 после обработки пустого представления. Дополнительно счётчик&lt;=(SIZE_MAX-1)/2 для хранения пар. Ноль допускает только пустое значение.
- Момент чтения: valueOf читает лимиты в каждом вызове разбора/создания канонического значения; готовое значение не перечитывает настройки.
- Изменяемость и время жизни: Параметры ограничивают один вызов valueOf. Неизменяемый результат содержит каноническое значение, без сохранённого бюджета; следующий вызов может выбрать другие лимиты.
- Ошибка: std::length_error("price increments range limit exceeded") при превышении; переполнение размера имеет отдельный length_error; неверная грамматика и bad_alloc не превращаются в успешное значение.
- Исходники потребителя: [include/qd/glossary/price_increments.hpp](../../include/qd/glossary/price_increments.hpp), [src/glossary/price_increments.cpp](../../src/glossary/price_increments.cpp), [include/qd/glossary/additional_underlyings.hpp](../../include/qd/glossary/additional_underlyings.hpp).
