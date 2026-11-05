# Identity time-series и модель логического диапазона

[English version](TIME-SERIES-IDENTITY.md). Нормативна английская версия.

Публичный [`qd/api/time_series_subscription.hpp`](../../include/qd/api/time_series_subscription.hpp)
содержит owning decorators, явные business capability declarations и ограниченную изменяемую
`TimeSeriesSubscriptionModel`. Компонент `qdcxx_subscription_selectors` доступен без Feed, транспорта
и runtime bootstrap. Здесь нет HISTORY receiver, event mapping, wire requests, полноты диапазона,
event flags, dispatch listeners или жизненного цикла закрытия подписки.

## Факты pinned Java

[Actual production corpus](../../tests/fixtures/time-series/oracle.tsv) и
[provenance](../../tests/fixtures/time-series/provenance.json) связывают QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` с hashes Java8 executable/source/classes.
[Ограниченный oracle](../../tools/upstream/time-series-oracle/README.ru.md) наблюдает detached production
объекты: nested decorators, observer notices, raw generic bypass и наследуемые markers.

- `TimeSeriesSubscriptionSymbol` сохраняет event symbol и signed64 `fromTime`. Source — DEFAULT zero;
  equality/hash используют только event symbol, исключая внешний диапазон.
- `DXFeedTimeSeriesSubscription` начинает с `Long.MAX_VALUE`: события не запрошены до изменения свойства.
  Изменившийся `setFromTime` заново декорирует текущий raw set. То же значение — no-op.
  Signed extrema, отрицательные значения и ноль сохраняются без clamp.
- `getSymbols()` возвращает raw symbols, `getDecoratedSymbols()` — wrappers. Уже декорированные
  входы оборачиваются, не уплощаются. Смена диапазона сохраняет raw identity; range — атрибут demand,
  не ключ. Обычный текст star и явный wildcard остаются разными объектами.
- TimeAndSale, Candle, Greeks, TheoPrice, Underlying реализуют TimeSeriesEvent. NuamTimeAndSale и
  custom-наследник TimeAndSale из harness наследуют marker. Order и Series indexed, но не time-series.
  Ни один из девяти текущих native projection types не имеет этих markers.
- Generic type bound Java запрещает неподдержанный typed use при компиляции; raw-Class constructor
  принимает проверенные non-time-series классы. Этот обход не доказывает wire capability.
- Java принимает `setFromTime(42)` после close с пустым set. Факт сохранён отдельно; native logical
  model не имеет close и не заявляет жизненный цикл facade.

[Исправление ADR-0012](../../docs/decisions/ADR-0012-decorated-subscription-identity.ru.md) сохраняется.
Actual Java IES/TSS asymmetry распространяется на nested TSS event symbol: результат collection может
зависеть от порядка вставки. Raw rows сохраняют расхождение. Native typed-kind equality симметрична
и внутри time-series decorator, поэтому native deduplication не повторяет ошибочный collapse.
Reference-driven notices FilteredSubscriptionSymbol не являются callbacks модели: она сообщает
только effective value changes.

## Значения и capabilities

`TimeSeriesSubscriptionSymbol<Symbol>` владеет Symbol по значению. По умолчанию это существующий
`SubscriptionSelector`, сохраняющий PLAIN, INDEXED, TIME_SERIES, ALL и их атрибуты. Другой time-series
wrapper тоже может быть Symbol: nesting не ограничен фиксированной runtime глубиной или Candle-классом.
Concept проверяет синтаксис copy/equality/hash. Вызывающая сторона обеспечивает semantic laws:
симметричную эквивалентность, одинаковый hash равных identity и стабильность ключа. Владение и
исключения custom Symbol следуют его контракту. Native domain хранит только text/scalar metadata.

`TimeSeriesEventDescriptor::declared(stableIdentity, capability)` сохраняет непустую business identity
и явный NON_INDEXED, INDEXED или TIME_SERIES marker; TIME_SERIES включает indexed semantics.
Spelling identity, raw record-time layout и timestamp field не предоставляют capability. Разные
business types не должны alias один stable ID. Для custom/inherited types используется тот же
небольшой descriptor, без whitelist пяти Java-классов или новой иерархии. Declaration — metadata
вызывающей стороны, не verified wire mapper. `isTimeSeriesEventType(EventType)` возвращает false для
девяти текущих native types и неизвестных значений; `hasTimeSeriesFeedReceiver` — false даже для
валидной logical declaration. Текущий отказ Feed для TIME_SERIES selector не меняется.

## Изменяемая модель и атомарные cuts

Создание требует непустой конечный batch descriptors с TIME_SERIES. Точные дубликаты схлопываются
в first-seen order; конфликтующие markers одного ID отклоняются. Неверные descriptors, неподдержанная
категория, ошибочные limits и allocation failure не публикуют модель. Limits ограничивают counts
type/input, retained identities и точные UTF-16 units, не RSS процесса.

Начальный range — MAX, raw set пуст. Set/add/remove/clear используют существующую симметричную
typed identity. У дубликата остаются последние raw attributes. Только смена порядка не меняет set;
no-op сохраняет order и revision. Внутренний TIME_SERIES range может менять retained demand без смены
identity. Clear убирает symbols, сохраняя range/types. `setFromTime` меняет общее lower-bound property,
включая пустой set; не фильтрует events и не угадывает index encoding. Существующий
`SubscriptionSelector::qdTime()` остаётся отдельным generic value conversion, не доказательством
business/receiver capability.

Все методы сериализует один mutex. Snapshot одним cut копирует revision, range, types, raw values
и по одному внешнему decorator на raw value. Внешние decorators получают range этого snapshot;
старые copies не меняются. Они не удерживают model/schema/callback/socket/runtime. Чтение выделяет
ограниченные copies, без progress и ожидания callback. Move передаёт unique pure owner; moved-from
методы дают INVALID_STATE. Move/destruction не должны пересекаться с вызовами на том же объекте.
Detached snapshots переживают move и destruction владельца.

Effective change получает checked model-local revision; no-op сохраняет её. Exhaustion отклоняет
изменение до commit. Count/unit/length/allocation refusal сохраняет весь принятый range, raw set,
types и revision. Временная подготовка и caller-owned snapshots — отдельные copies; история и
monitoring map по symbols не накапливаются.

См. [руководство](../../docs/TIME-SERIES-SUBSCRIPTIONS.ru.md). Oracle подтверждает captured Java facts;
native builds/consumers/sanitizers и независимое ревью требуют отдельных execution evidence.
