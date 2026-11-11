# Владеющие значения SpreadOrder и AnalyticOrder

[English version](ORDER-VARIANTS.md). Английский текст является нормативным.

Подключите `qd/events/spread_order.hpp` или `qd/events/analytic_order.hpp`. Оба значения владеют строками
и numeric snapshots. Сохранённая копия остаётся действительной после уничтожения исходного cursor,
callback batch, subscription или runtime. `symbol` SpreadOrder обозначает underlying instrument,
`spreadSymbol` — nullable описание из события. Сохраняйте различие между null и пустой строкой.

```cpp
namespace q = org::ttldtor::qdcxx;
void handle(const q::events::SpreadOrder &order) {
    consumeUnderlying(order.symbol, order.getSourceId(), order.getIndex());
    if (order.spreadSymbol) {
        consumeSpreadDescription(*order.spreadSymbol);
    }
}
void handle(const q::events::AnalyticOrder &order) {
    consumeIceberg(order.getPrice(), order.getIcebergPeakSize(), order.getIcebergHiddenSize(),
                   order.getIcebergExecutedSize(), order.getIcebergType());
}
```

Функции `consume...` обозначают обработку в приложении. Для сохранения iceberg-полей используйте точный
тип AnalyticOrder: присваивание обычному `Order` копирует только базовые поля. `flags`, `eventFlags` и
`icebergFlags` независимы. Reserved iceberg type3 читается как UNDEFINED, исходные raw flags остаются
доступными. Отсутствующие optional iceberg sizes читаются как NaN. После изменения публичного wide-значения
его numeric getter использует новое значение; snapshots других исходных business-чисел сохраняются.

Для явного schema-bound mapping подготовьте `PreparedSpreadOrder::prepare` или
`PreparedAnalyticOrder::prepare` с immutable local record ID и конечными MappingLimits. Затем вызовите
`map`/`mapView` с точным context этого record и полными canonical values. `mapView` копирует весь borrowed
text до возврата. Задайте EventValueLimits для symbol, каждой nullable строки и их суммы. Mapper сначала
проверяет весь вход и учитывает полный text до выделения памяти результата. Проверяйте `expected`:
resource/context/type/allocation отказы не публикуют частичное событие, mapper остаётся пригодным к повторному использованию.

`defaultSpreadOrderSchema`/`defaultAnalyticOrderSchema` создают явные native DEFAULT projections; можно
предоставить более полную validated schema. SpreadSymbol обязан быть объявлен, даже если значение null.
У Analytic поля MMID/MarketMaker и iceberg optional. Имя immutable record определяет обычный source.
`OrderSource::isPublishable` описывает возможность прямой публикации и не определяет допустимость
receive record. Например, явный AnalyticOrder#NTV может проецироваться при false analytic publication flag NTV.

[Контракт mapping](../spec/events/ORDER-VARIANTS.ru.md) определяет numeric types, source identity,
независимые business/FIELD/removal times и гарантии владения/ошибок.
[Production-oracle tools](../tools/upstream/order-variants-oracle/README.ru.md) описывают воспроизведение reference.