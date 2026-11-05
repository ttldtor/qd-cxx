# Индексные события Order

[English version](ORDER.md). Английский текст нормативный.

`Order` владеет данными индексного рыночного события. Его бизнес-индекс содержит ID source в high32
и беззнаковый wire Index в low32. Он независим от бизнес-полей Time/Sequence и необязательного
FIELD eventTime. Обязательны Index, Time, Sequence, Price, Size и Flags. Отсутствующие необязательные
поля сохраняют production defaults. Бизнес Flags и индексные eventFlags — отдельные слова.
Адаптер обычного Order не проецирует поля AnalyticOrder, SpreadOrder или OtcMarketsOrder.

`PreparedOrder` связывает явно выбранную локальную запись неизменяемой схемы и проверяет полную
provenance, число, точные полные типы и канонические значения до проекции. Source определяется как
в PreparedOrderSource: после последнего `#` от одной до четырёх регистрозависимых ASCII букв/цифр;
без разделителя — DEFAULT. Это явное связывание схемы, а не поиск или проверка возможностей подписки.
Результат не удерживает схему, cursor, receiver, runtime, callback span или чужой payload. Nullable
MarketMaker декодируется raw-accessor SHORT_STRING с отдельным от symbol ограничением.

Настоящий стандартный Order имеет physical VOID в integer slot0 и COMPACT_INT Index в slot1.
Нативный `IndexedTimeLowInt32{field}` явно задаёт младший contributor и не создаёт логическое VOID-поле.
Он принимает только точный COMPACT_INT. Обычный collector key — беззнаковый low32 с нулевым high32.
Заданный raw removalTime, включая ноль, сохраняет полный знаковый 64-битный collector key; в логический
Index восстанавливается только low32. Удаление с ненулевым high32 не удаляет обычный ключ с нулевым
high32, даже если публичные индексы Order совпадают. Source принадлежит partition выбранной записи,
его ID не добавляется в collector time. Layout доступен только нативному API; отдельная версия JSON
по-прежнему принимает лишь pair-int32 и single-int64.

Нормативные upstream-источники закреплены в [upstream.lock](../../upstream.lock): MarketFactoryImpl.java
(physical fields и контракты delegates), OrderDelegate.java (приём), impl/OrderMapping.java (accessors),
OrderBase.java и Order.java (бизнес-значения). Обычная подписка Order использует HISTORY; STREAM SUB
принадлежит отдельной stream-only схеме. Публичный API сохраняет TICKER по умолчанию и требует явно
выбранный HISTORY для типизированного Order.
