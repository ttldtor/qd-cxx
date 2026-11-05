# Приём индексного Order

[English version](ORDER.md). Английский текст нормативный.

Подключите `qd/api/feed.hpp`, свяжите `qdcxx::feed` либо совместимый `qdcxx_feed` и явно выберите HISTORY:

```cpp
namespace q = org::ttldtor::qdcxx;
q::api::ApiRuntime runtime;
q::api::EndpointOptions options;
options.contract = q::api::DeliveryContract::HISTORY;
auto endpoint = runtime.createEndpoint(options);
auto orders = endpoint->getFeed()->createSubscription<q::events::Order>();
orders->addEventListener([](std::span<const q::events::Order> batch) {
    for (const auto &order : batch) {
        consume(order.symbol, order.getSourceId(), order.getIndex(), order.getPrice(), order.eventFlags);
    }
});
orders->addSymbols("AAPL");
```

Это локальная настройка demand. Подключение и проверенное завершение показаны в [полном примере Feed](../examples/feed_quote/README.ru.md).
`consume` обозначает собственную синхронную обработку приложения. Span действителен до возврата callback.
Копия Order владеет symbol, полями и nullable MarketMaker независимо от subscription/runtime.

HISTORY без явного registry использует стандартную схему default Order внутри SDK. Source-specific схемы
передаются через существующий immutable EventRegistry, каждый выбранный record регистрируется как
EventType::ORDER. Native indexed layout явно выбирает точный COMPACT_INT Index через IndexedTimeLowInt32.
Source определяется из имени записи; имена и дополнительные поля схемы остаются явной конфигурацией.
Приложение не создаёт collector, queue или per-turn work. TICKER остаётся контрактом endpoint по умолчанию.
Типизированный или смешанный Order отклоняет несовместимый контракт до публикации subscription.
Обычный STREAM-приём Order не выводится из существования publication delegates.

Plain selector требует все зарегистрированные Order sources. Явный indexed selector требует свой source;
отрицательный source ID выбирает доступный набор, сохраняя отдельную логическую identity. Например:

```cpp
const auto source = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
const std::array selectors{q::api::SubscriptionSelector::plain(u"AAPL"), source};
orders->setSelectors(selectors);
orders->removeSelector(selectors[0]);
```

Удаление plain selector сохраняет NTV demand. Перекрывающиеся selectors не дублируют physical record
в одной subscription; отдельные subscriptions независимы. Sources с одинаковыми low indexes сохраняют
независимые HISTORY partitions и прогресс transaction/snapshot. Публичный eventFlags содержит настоящие
collector delivery flags, бизнес flags остаётся отдельным словом. Полный контракт полей, индекса и границы
native/JSON описан в [Order](../spec/events/ORDER.ru.md).
