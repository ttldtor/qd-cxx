# ADR-0010 — Сохранение полной ширины Summary OpenInterest

[English version](ADR-0010-summary-open-interest-width.md). Английский текст нормативный.

Статус: **Принято для QDCXX-146.** Это явное исправление доказанной потери данных в закреплённом Java, согласно правилу повторять поведение Java без воспроизведения подтверждённых ошибок.

## Основание

QD commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` документирует OpenInterest как число открытых контрактов и предоставляет publiclong getter/setter. Реальная схема по умолчанию использует WIDE_DECIMAL120: wide=true по умолчанию, MarketFactoryImpl выбирает этот тип. При явном wide=false используется прежний COMPACT_INT8.

Long-accessor production mapping точно сохраняет конечное3000000000 в стандартной wide-записи. SummaryDelegate читает int-accessor и лишь затем расширяет повреждённый результат вlong; публикация явно сужает eventlong вint. Реальные JDK8 наблюдения показывают3000000000→-1294967296 и4294967296→0 при delegate read и при event put/get roundtrip. Сам Java Summary сохраняет исходныйlong. Физический default-формат способен точно хранить эти значения: потеря происходит в delegate, а не требуется wire-ограничением.

Генератор объявляет Summary OI как INT_DECIMAL с DecimalMapper(int), что объясняет accessor/cast. Это реализация, а не документированное publicint32 предусловие. Checkout shallow; время миграции, исторический замысел и признание ошибки upstream не утверждаются. Одного наличия long-accessor недостаточно для доказательства: основание — конечный, представимый и воспроизводимый roundtrip counterexample.

## Решение

Сохраняется существующее C++ поле `std::int64_t openInterest` и getter. Отображение следует actualSummaryMapping.getOpenInterestLong / RecordMapping.getAsLong:

| Допущенный источник | Исправленное значение C++ |
|---|---|
| INT2/4/8 | signed physicalint с расширением знака, без расширения domain физического хранения |
| LONG104 | полное signed64 значение |
| DECIMAL24 | actualDecimal.toDouble, Java усечение/насыщение/NaN→0 вlong |
| WIDE120 | точный WideDecimal.toLong: ближайшее целое, половины к плюс бесконечности, насыщение signed64, NaN→0 |
| Отсутствующее optional / региональное поле |0 |

Для LONG/WIDE нет double intermediate и последующего сужения вint. Реальное ограничение INT8 сохраняется: уже потерянные физическим32-битным полем данные восстановить нельзя. Неподдерживаемые present40/152 по-прежнему отклоняются при подготовке. Новые форматы, поля, publictypes, layout, defaults и receivingcapacities не вводятся; шесть business-double views и их inline snapshots не меняются. Wide precision/scaling ограничения также сохраняются: результат — long-accessor фактически сохранённого значения, а не обещание восстановить любой requestedlong после decimalquantization.

## Доказательства и совместимость

Исходные `oracle.tsv` и provenance остаются неизменными сырыми наблюдениями Java delegate. Отдельный `open-interest-long-v1.tsv` для каждого успешного исходного и добавленного boundary sample хранит delegateOI, настоящий long-accessorOI, исправленное ожидание и класс `IDENTICAL` либо `CORRECTED_INT_ACCESSOR_LOSS`. Native reader требует полного покрытия без дубликатов, совпадения исходного delegate и класса до применения единственного исправленного expectedcell. Остальные поля сохраняют прежние ожидания; несовпадения не игнорируются.

`open-interest-boundary-v1.tsv` добавляет реальные production значения около2^31/2^32, signed64 границы, все шесть numeric representations, NaN/infinities и regionalabsence. Tiny и wide сохраняют разные правила Java long. Capture/Verify связывает оба корпуса с исходным capture, productionclasses и sourcehashes; nativeформула не создаёт ожидания.

Обычные значения прежнего диапазона совместимы. Большой представимый OI намеренно отличается от закреплённого delegate, сохраняя64-битный публичный контракт. Зависимость приложения от отрицательной/нулевой порчи прежнего bridge не поддерживается. Aggregateinitialization, владение, маршрутизация, callbacklifetime и конечный preparation/accounting контракт не меняются. Wide precision/scaling ограничения также сохраняются: результат — long-accessor фактически сохранённого значения, а не обещание восстановить любой requestedlong после decimalquantization.

## Альтернативы

Точное повторение delegate int-narrowing сохраняло бы доказанную потерю данных и отвергнуто принятой политикой. Ограничение int32 также теряет информацию и не соответствует actual long mapping. Новый тип, compatibilityswitch и расширенный receivingprofile не нужны: publicfield уже имеет требуемую ширину.

Первичные закреплённые исходники перечислены в [английской версии](ADR-0010-summary-open-interest-width.md#primary-sources). См. [контракт Summary](../../spec/events/SUMMARY.ru.md) и [oracle helper](../../tools/upstream/summary-oracle/README.ru.md).
