# Значения и mapping SpreadOrder и AnalyticOrder

[English version](ORDER-VARIANTS.md). Английский текст является нормативным.

`SpreadOrder` владеет общими полями OrderBase и nullable `spreadSymbol`. Его event `symbol` обозначает
underlying subscription symbol; `spreadSymbol` описывает spread payload. Это соседний по иерархии тип
обычного Order, без поля MarketMaker. `AnalyticOrder` наследует `Order` и отдельно сохраняет три iceberg
числа и полный `icebergFlags`. Dispatch точного варианта обязан сохранять analytic identity: преобразование
в обычный Order теряет дополнительные поля.

Оба adapter требуют Index, Time, Sequence, Price, Size и Flags. SpreadSymbol также обязан быть объявлен,
но его значение может быть null. Допустимы точные типы UTF_CHAR_ARRAY и STRING; serialized objects
отклоняются. Optional nano/action/ID/executed/count/trade поля сохраняют [семантику Order](ORDER.ru.md).
Только Analytic содержит optional MMID/MarketMaker с тем же raw first-Java-int accessor и декодированием
SHORT_STRING, что у Order. Отсутствующие iceberg числа равны NaN, icebergFlags — нулю. Iceberg sizes
допускают те же шесть полных numeric types, что значения Order; IcebergFlags использует точные raw
first-int forms. Полная identity типа проверяется без маскирования.

Младшие два бита iceberg flags означают UNDEFINED 0, NATIVE 1 и SYNTHETIC 2; reserved 3 читается как UNDEFINED.
Остальные биты сохраняются в raw-поле. Три независимых iceberg business-number snapshots существуют
вместе с пятью унаследованными snapshots Order. Прямые исходные Java double сохраняются отдельно от
wide-decimal accessors. После изменения публичного wide raw соответствующий getter использует новое
wide-значение, не меняя snapshot другого поля. `Count` сохраняет signed32 getAsInt semantics с расширением
в signed64, включая saturation/truncation tiny-decimal входа.

`PreparedSpreadOrder` и `PreparedAnalyticOrder` связывают явный record ID и владеют immutable context,
полной shape, selections и source ID. Выбор required/optional полей и разбор source расходуют конечные
MappingLimits. Mapper не удерживает исходные schema declarations, cursor, payload view, callback span,
runtime или native resource. `map`/`mapView` сначала проверяют полные provenance, count, точные полные
типы и canonical payloads. Затем symbol и каждый selected nullable text учитываются в
symbolUnits/stringUnits/totalUnits до первого выделения памяти результата. Проверки через вычитание
исключают переполнение суммы. Отказ атомарно возвращает существующий MappingError; allocation/length
exceptions превращаются в ALLOCATION_FAILURE. Входы не меняются, подготовленный mapper можно использовать снова.

Source занимает high32 публичного index, unsigned wire Index — low32. Signed business Time и unsigned
Sequence образуют timeSequence независимо от FIELD eventTime. Native `IndexedTimeLowInt32` сохраняет
physical VOID/Index layout; engaged removalTime владеет полным signed64 collector identity, включая
независимый high word. Приём не переносит ограничения putEvent-only publication на отрицательные low
indices или сочетания business side/size.

Publication capability источника, доступность standard scheme и приём явно объявленного record различны.
DEFAULT публикует оба варианта; ISE публикует Spread, но не Analytic; NTV поддерживает full order book при
false publication flags обоих вариантов. Явно предоставленный AnalyticOrder#NTV record всё равно может
приниматься. Сохраняется optional shape конкретного standard source: нельзя автоматически копировать
поля DEFAULT на каждый suffix. `defaultSpreadOrderSchema` и `defaultAnalyticOrderSchema` создают явные
нормализованные native DEFAULT projections с Count и SpreadSymbol/MMID; optional iceberg fields отсутствуют.
Приложение может явно подготовить более полную immutable schema.

Закреплённые источники: [upstream.lock](../../upstream.lock), API classes SpreadOrder/AnalyticOrder,
их generated mappings/delegates и MarketFactoryImpl.
[Production oracle](../../tools/upstream/order-variants-oracle/README.ru.md) сохраняет все 200862 исходные
наблюдения и 200 QTP-файлов. Отдельный 51-строчный FIELD witness проверяет реальные nonzero/negative/full-width
metadata, нулевые/выключенные read/write modes и независимое business time. Native fixture loader
ограничивает вход 16 MiB и 250000 строками, не обрезая наблюдения. Оба семейства являются обычными native
indexed routes; Quote/MarketMaker derived-source permission и time-series classification не подразумеваются.