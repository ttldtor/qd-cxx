# ADR-0012: Симметричная identity декорированных подписок

[English version](ADR-0012-decorated-subscription-identity.md). Английский текст является основным.

Статус: принятая implementation policy QDCXX-136; независимая проверка реализации учитывается отдельно.

## Контекст и наблюдённый дефект

Закреплённая Java QD3.355 revision `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` определяет identity IndexedEventSubscriptionSymbol (IES) через symbol/source, а TimeSeriesSubscriptionSymbol (TSS) — только через symbol. TSS наследуется от IES. IES.equals принимает любой подтип IES, а TSS.equals — только TSS.

Реальные production Java8 наблюдения показывают `IES("AAPL", DEFAULT).equals(TSS("AAPL", 0)) == true`, но обратное сравнение `false`. Асимметрия также возникает при fromTime -1,1,MIN/MAX. Hashes проверенных DEFAULT-cross-class пар равны; нарушение hash-контракта не заявляется. Реальная вставка в HashSet сначала IES, затем TSS даёт size2; обратный порядок даёт size1. Это collection witness, а не доказательство поведения настоящего DXFeed subscription container или сетевого route.

Публичный Java Object.equals требует симметрии. Поэтому наблюдённое cross-class отношение — подтверждённый дефект equality-контракта, а не просто необычный sentinel или догадка о runtime. Исключение fromTime из TSS identity и source ID-based equality при разных/null именах документированы и рассматриваются отдельно. Explicit WildcardSymbol.ALL и String("*") неравны в обоих направлениях при одинаковом hash42: это допустимые typed distinction и hash collision.

Первичные закреплённые источники: [IES](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/IndexedEventSubscriptionSymbol.java), [TSS](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/TimeSeriesSubscriptionSymbol.java), [WildcardSymbol](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/osub/WildcardSymbol.java). Проверен контракт установленного JDK8 Object.java; [справка Object.equals](https://docs.oracle.com/javase/8/docs/api/java/lang/Object.html#equals-java.lang.Object-) описывает то же требование симметрии. [Raw observations](../../tests/fixtures/selectors/oracle.tsv) и [provenance](../../tests/fixtures/selectors/provenance.json) сохраняют upstream результаты без исправления.

## Решение

Используется плоский owning SubscriptionSelector с явными PLAIN, INDEXED, TIME_SERIES и ALL kinds. Equality включает kind и симметрично. В INDEXED source identity использует знаковый ID, а не имя; в TIME_SERIES fromTime исключён из identity, но включён в сравнение эффективного demand. ALL отличается от literal text, включая "*". Hash каждого kind совместим с Java; совпадения между неравными kinds допустимы.

Асимметрия закреплённой Java не воспроизводится. Поведение Java сохраняется там, где её контракт непротиворечив; этот подтверждённый дефект исправляется. Из решения нельзя выводить общее разрешение менять непривычные значения или считать любое расхождение upstream bug.

## Альтернативы и последствия

Копирование Java inheritance/instanceof equality сохранило бы некорректное отношение и зависимость native containers от порядка; отклонено. Объединение IES(DEFAULT), TSS и plain text в одну identity уничтожило бы различия kind/source/time и не обосновано их отдельными публичными контрактами; отклонено.

Это явное cross-kind отличие от Java. Прежней native decorated identity нет. Текущие string/Candle API сохраняют plain identity и exact routing. Изменение эффективного fromTime должно менять desired demand даже при равной identity. Текущий native typed Feed атомарно отклоняет неподдерживаемую TSS activation и не изображает приём будущего indexed/time-series family. Source demand использует существующие проверенные bindings; time conversion остаётся value-only операцией с explicit raw requests. [Контракт selectors](../../spec/subscriptions/DECORATED-SELECTORS.ru.md) определяет поддержанную границу, lifetime и конечные ресурсы.
