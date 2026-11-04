# Формат захвата Trade

[English version](CAPTURES.md).

Raw dataset — файл oracle.tsv в этом каталоге. provenance.json связывает его точные байты с locked
production source/class inventory и [воспроизводимым helper](../../../tools/upstream/trade-oracle/README.ru.md).
Helper вызывает actual generated TradeMapping/TradeDelegate без диагностической копии converter.
Включены nano time и Trade exchanges Q/z. Сценарии сохраняют полные physical record fields,
переставляя logical order для проверки lookup identities. Java сохраняет synthetic VOID tails;
только native logical fields не содержат эти искусственные слоты.

record задаёт scenario, record ID и точное имя Trade/Trade&Q. field добавляет logical position,
full serial type ID, local/property lookups. symbol задаёт physical UTF-16 hex units и исходный FIELD
event-time sequence. value сохраняет raw scalar каждого поля; nullable text marker равен null,
поскольку в этих Trade profiles нет object fields.

После scenario/record/sample строка trade содержит25 cells:

| Cells | Actual production result |
|---|---|
|0 | logical event symbol в точных UTF-16 hex units |
|1..6 | eventTime milliseconds, timeSequence, business milliseconds/nanoseconds, sequence, nano part |
|7..12 | exchange unit, TradeId, dayId, полные post-fallback flags, direction code, extended-hours0/1 |
|13..17 | wide raw price, size, change, day volume, day turnover |
|18..22 | соответствующие actual business getter double bits как signed64 |
|23..24 | actual business integer size/day volume |

Flags читаются actual package-private production getter через bounded harness reflection; native
формула fallback не генерирует expected flags. reject/conversion-reject сохраняют actual factory/accessor
failure. native-reject обозначает явный отказ native one-slot TradeId, сохраняя успешное Java
наблюдение с известным соседним physical slot.

Families охватывают шесть numeric representations (все128 tiny low patterns на пяти mantissa
boundaries, LONG tie/carry/extremes и1ULP business witness), одиннадцать raw-int representations,
три raw64 TradeId, one-slot TradeId с известным соседом, missing optional fields, missing required
price/size, unsupported numeric flags/time types,128 flags/tick combinations, physical symbols с уже
существующими suffix/attributes. Полные positive/negative shapes сохранены; неподдерживаемые typed
варианты не скрываются.

Native conformance сравнивает owning/borrowed projections. Standalone public Feed consumer отдельно
проверяет точные ADD/REMOVE record/symbol tuples, composite/regional binding, семь mixed type sets,
attributes, wrong-record/unsubscribed drops, явный reconnect и изоляцию listener fault. Это
детерминированные локальные TCP наблюдения, отдельно от public live-service run.
Последний mixed demand добавляет отдельный символ без начальных данных: его wire ADD подтверждает
backend membership до первой доставки. После первого удаления live marker следует за удалённым
composite в том же DATA frame; его callback предшествует удалению оставшихся подписок. После final
REMOVE новые DATA не отправляются, поэтому обычный local drain проверяется без гонки с искусственным
поздним пакетом при завершении.
Последняя REMOVE group требует ровно шесть record/symbol tuples в полных frames, допуская разный
порядок асинхронной отправки; пропуски, дубликаты и лишние tuples остаются ошибками.
