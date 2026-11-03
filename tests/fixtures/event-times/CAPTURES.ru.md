# Production-наблюдения общего времени и регионального Quote

[English version](CAPTURES.md). Английская версия является основной.

`oracle.tsv` содержит исходный вывод [oracle времени событий](../../../tools/upstream/event-time-oracle/README.ru.md).
`provenance.json` связывает его байты с harness, launcher, хешами production-исходников и неизменным
class inventory QD3.355 commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Production-вызовы JDK8
в UTC и Pacific/Honolulu дали одинаковый вывод из2152 строк.
[Контракт](../../../spec/events/EVENT-TIME.ru.md) отличает эти наблюдения от native-политики.

Девять сценариев отдельных JVM сохраняют настоящие сгенерированные объявления Quote, Quote&Q, Quote&z и Profile:

| Сценарий | Выбранная конфигурация/вариант |
| --- | --- |
| `default` | Стандартные секунды business time; Quote Sequence/TimeNanoPart отсутствуют |
| `sequence` | `dxscheme.enabled.Sequence=Quote` |
| `nano` | `dxscheme.enabled.TimeNanoPart=Quote` |
| `both` | `dxscheme.nanoTime=true` |
| `millis` | Оба необязательных поля Quote и `dxscheme.bat=millis` |
| `millis-default` | Миллисекунды business time без Sequence/TimeNanoPart |
| `minimal` | Клонированы только обязательные поля сгенерированных записей |
| `reordered` | Обратный порядок сгенерированных логических полей, оба необязательных поля Quote |
| `missing` | Клон без обязательных полей; настоящие ошибки factory/delegate |

Восемь принятых сценариев содержат по пять cursor samples каждой записи:160 полных cursor/delegate
наблюдений. Необязательные поля сохраняют границы signed32, отрицательные и слишком большие nano parts
и все десять битов упакованных миллисекунд. Business time включает границы signed32 секунд и signed64
миллисекунд, отрицательные доли секунды и разное время bid/ask. Независимые timestamp-sequence words
включают0, минус1, signed64 minimum/maximum и смешанный набор битов. Строки Profile имеют null, empty
и непустые значения. Отсутствие обязательных полей даёт четыре строки `reject`.

Сценарий default добавляет21 строку `route` через настоящие delegate и14 строк `selection` через
настоящие market delegate sets. Входы содержат существующие exchange-суффиксы и завершающие атрибуты;
наблюдаются региональный каталог и composite-only fallback. Строчный exchange предотвращает
специальную реализацию только для обязательного случая Q.

Отдельный `representability.tsv` добавляет четыре настоящих Quote&Q delegate roundtrips.
Написания с потерями `AAPL&N&Q` и `AAPL{x=y}&Q` отличаются от представимых символов с атрибутами.
Отдельный хеш записан в том же provenance; исходный корпус2152 строк не изменён.

Строки имеют точные TSV-ячейки: `record` задаёт scenario/local record; `field` сохраняет логическую
позицию, полный serial identity, local name и настоящее property name; `input` сохраняет UTF-16
units символа и исходный timestamp sequence; `value` сохраняет исходные scalar/text values;
`quote`/`profile` сохраняют результаты production getters. `route` хранит event-to-QD и QD-to-event
символы; `selection` — реально выбранную запись и QD-символ. Строки представлены hex UTF-16 units,
`-` означает пустую строку, `null` — отсутствие. Физические VOID tails не являются логическими
полями. Ожидаемое время не вычисляется диагностической копией.

Native-проверки `event_time_java`, `event_time_routing`, `event_time_feed` сравнивают владеющие/
заимствованные проекции, точный выбор маршрутов, границы конечного workspace и реальную loopback-
доставку Feed. Standalone Feed test также используется как consumer установленного и физически
перемещённого пакета только через публичные заголовки. Демонстрация публичного сервиса является
отдельным случаем и здесь не заявляется.
