# Формат корпуса Summary

[English version](CAPTURES.md).

Raw dataset — `oracle.tsv`; provenance.json связывает байты с locked source, actual production-class
inventory и [bounded helper](../../../tools/upstream/summary-oracle/README.ru.md). Наблюдаются настоящие
SummaryMapping, SummaryDelegate и Summary getters.

Composite имеет10logical fields, regional9без OpenInterest. Default wide120 относится ко всем шести
double fields и composite OpenInterest. Отдельные свежие Java процессы фиксируют wide=false tiny
fallback и price/volume int. Logical order обращён при сохранении physical two-slot storage/VOID
tails. HasTime=false; независимые FIELD metadata сохранены, включая профиль без metadata mode.

record задаёт scenario/recordID/name; field — position/fulltype/local/property keys; symbol — physical
UTF-16 hex и исходную FIELD time sequence; value — полные scalar raw bits. После scenario/record/sample
строка summary содержит20cells:

|Cells|Actual production значение|
|---|---|
|0|logical event symbol UTF-16 units|
|1..7|eventTime, dayId, prevDayId, business OpenInterest, полные flags, day/previous close PriceType|
|8..13|wide raw open/high/low/close/previous-close/previous-volume|
|14..19|шесть event business double raw bits|

interest дополнительно хранит mapping getOpenInterest int, getOpenInterestLong, double bits и wide
raw. Они могут различаться; event delegate использует int. Flags прочитаны настоящим package-private
getter через reflection. Native формулы не создают expected results. Reject/conversion-reject
сохраняют actual production failures.

Families: шесть numeric типов, tiny128low patterns/пять mantissa boundaries, LONG tie/carry/extremes/
1ULP,11raw-int типов/high word, более1000wide OpenInterest rank/tie/overflow probes,16PriceType
комбинаций/high/negative flags, шесть required removals, optional alone/all missing, default/tiny/int,
metadata absence, suffix/attributes, неподдерживаемые double/OpenInterest representations.
Региональный отсутствующий OpenInterest остаётся0 при отказе composite с неподдерживаемым present OI.

Native owning/borrowed проверяет полные shapes и20cells; только OI следует отдельно
классифицированному actual long reference по ADR-0010. Installed/moved consumers отдельно доказывают
demand→physical binding→значения,31набор пяти типов, Trade/ETH interop, negative routes, remove/throw/
callbackclose/reconnect. ADD marker ограждает последний desired replacement. Только callback-close
case посылает дополнительный five-kind business probe после physical REMOVE, доказывая жизнь всех31
других подписчиков. Final REMOVE — точное множество десяти tuples по полным frames, с отказом при
пропусках/дубликатах/лишних. Ordinary close требует drained; reconnect gap и subscription ABORT history
классифицируются отдельно. Локальные детерминированные TCP проверки не являются public live run.

Отдельный `open-interest-long-v1.tsv` содержит reference/scenario/record/sample/delegateOI/mappingLong/correctedOI/classification. `open-interest-boundary-v1.tsv` сохраняет прежний layout record/field/symbol/value/summary. Оба результата и immutable `oracle.tsv` связаны `open-interest-long-v1.provenance.json`. Для каждого успешного sample обязательны exactrawOI и полное referencecoverage; необъяснённые отличия — ошибка. См. [ADR-0010](../../../docs/decisions/ADR-0010-summary-open-interest-width.ru.md).
