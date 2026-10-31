# Ограниченный TICKER-коллектор

[English version](TICKER-COLLECTOR.md). Английская версия нормативная.

Подключите `qd/collectors/ticker.hpp` и линкуйте `qdcxx_collectors`. Чистый C++23
коллектор хранит одно последнее значение для точного ключа запись/символ и конечное
число независимых локальных представлений выдачи. Он принимает целые допущенные
[события live-session](LIVE-SESSION.ru.md) и использует
[желаемое состояние](STATE.ru.md) каждого представления. Runtime, callbacks,
мультиплексирование upstream-спроса и HISTORY относятся к отдельным компонентам.

[Квалификация](../../tests/conformance/subscriptions/TICKER-VALIDATION.ru.md)
фиксирует исполненные native/Java профили и границы сравнения.

## Общий кеш и локальные представления

Коллектор владеет неизменяемой локальной схемой, фиксированным хранилищем кеша
и конечным числом слотов представлений. Каждое представление владеет своим TICKER
desired state и метаданными initial/update очередей. Токен определяет исходный
коллектор, слот и неповторяемую активацию: чужой или устаревший токен не обращается
к новому представлению в том же слоте. Сохранённый токен удерживает только
фиксированный маркер происхождения.

`storeEverything=false` удерживает значение, пока хотя бы одно локальное
представление имеет точный интерес к ключу. Удаление последнего интереса вытесняет
значение. Ввод до подписки без заинтересованных представлений не сохраняется.
При `storeEverything=true` допущенные значения могут сохраняться без интересов
и обеспечивать последующую initial-выдачу. Ни один режим не обещает кешированный
снимок при каждом remove/re-add.

Каждому представлению отдельно резервируется pending-ёмкость, равная числу
cache-ключей. На ключ приходится не более одного ожидающего значения с
фиксированными initial/update связями. Остановившийся потребитель не занимает
очередь другого представления и не удерживает старые версии payload отдельно
от общего кеша.

Эти локальные интересы определяют хранение и выдачу. Они не экспортируют
объединённую wire-подписку и не означают применённую подписку peer.

## Допуск целого кадра и равенство

Принимаются только целые события DATA/TICKER_DATA типа 10 с точным происхождением
схемы. Session проверяет framing до передачи события: декодированный префикс
ошибочного кадра не допускается. Первое валидное событие закрепляет исходную
эпоху даже без сохранённых ключей. Binding generation может меняться внутри
эпохи; другая эпоха требует явного reset.

Stage планирует весь кадр до публикации. Каждое вхождение сравнивается с последним
значением кандидата в порядке источника. Изменение и возврат внутри одного кадра
могут пометить ключ dirty, даже если итоговые поля равны ранее выданным. Наличие
записи со всеми нулями отличается от отсутствия. Равные поля не меняют dirty
и позицию ожидающего ключа.

Выбранный профиль обычных полей сравнивает точные скалярные представления
и поддержанное nullable-содержимое текста, байтов и opaque payload. Одни флаги
не являются изменением полей. Raw REMOVE не командует стирать ключ. Коллектор
использует уже материализованные декодером поля: повторного декодирования payload,
бизнес-конвертации и вывода о равенстве произвольных Java-объектов здесь нет.

Выдача различает сохранённый исходный `sourceRecord` и `deliveryFlags`. Источник
сохраняет raw-флаги, indexed removal time, метаданные полей, typed/null/opaque
значения и binding provenance. Выходные TICKER-флаги выбранного профиля равны нулю,
как в измеренной default-field FLAGGED_DATA проекции. Равный ввод не заменяет
исходные метаданные последнего эффективного значения.

## Initial и live выдача

Новый интерес к доступному кешированному значению планирует initial-выдачу.
Обновление ожидающего ключа заменяет видимое последнее значение без перемещения
в очереди. Обновление после подтверждения initial попадает в live/update порядок.
Remove отменяет прежнюю membership; re-add создаёт новую активацию и позицию.
Явный положительный retrieval quantum задаёт баланс initial/update; значение
по умолчанию 100 следует зафиксированному производственному исходнику.

Подтверждение initial может сохранять update-позицию ключа для повторного
использования. Обновление подтверждённого ключа до границы quantum должно
сохранять измеренный порядок; немедленное удаление обеих связей меняет его.

`front` готовит одно заимствованное предложение последнего значения для одного
представления. `pop` подтверждает именно его; это не callback и не подтверждение
peer. API многозаписного batch в этом компоненте отсутствует.

Весь доступ, копирование/освобождение исходных владельцев, перемещения
и разрушение сериализуются и не допускают reentrancy. Фоновых потоков
и пользовательских callbacks нет. Заимствованные schema/desired state следуют
документированным срокам жизни владельца и мутаций. Публикация изменённого кеша,
соответствующее вытеснение, замена предложения, reset и перемещение владельца
инвалидируют затронутые delivery/latest ссылки до освобождения backing. До такой
операции вызывающая сторона должна закончить использование предложения.

## Отказы, stale и reset

Отказ preflight по работе сохраняет владение вводом, свежесть кеша и текущие
предложения. Отказавшая мутация или мутация без эффективного изменения сохраняет состояние
и предложения. Списанная работа не возвращается.

Исчерпание общего cache/resource отвергает целый непотреблённый ввод и явно
помечает общее состояние stale. Текущие предложения инвалидируются, даже если
backing остаётся для stale-инспекции. Все представления видят этот общий gap;
произвольные живые ключи не вытесняются ради видимости успешного допуска.
Отказ открытия слота или мутации core desired state сохраняет другие представления.

Reset заранее учитывает освобождение, инвалидирует предложения, удаляет старые
cache/pending данные и владельцев эпохи, сохраняя desired interests. Резервное
уведомление содержит локальное поколение, объединённые reset, прежнюю причину
и неизвестную upstream-потерю. До свежей выдачи нужно точно подтвердить текущее
уведомление. Это один барьер владельца коллектора: он не доказывает подтверждение
каждым представлением, callback, транспортом или peer. Старое подтверждение
не разблокирует новое уведомление. Замена схемы требует нового коллектора.

## Ограничения и доказательства

Лимиты конечны и неизменяемы; ноль означает нулевую ёмкость. Requested resident
backing включает фиксированные метаданные/кандидат, собственную схему и уникальный
backing целых удержанных событий, включая невостребованные записи в том же
событии. Core desired state каждого представления и общие session snapshot/epoch
имеют отдельные контролируемые бюджеты. Это не накладные расходы allocator и не RSS.

До публикации учитываются старые владельцы и входящий кандидат. Замена может
превысить пиковую ёмкость, хотя итоговый кеш поместился бы. Matching, равенство,
метаданные и консервативное возможное освобождение владельцев оплачиваются
до разрушительного commit. Work ограничивается вызывающей стороной на ход,
а не суммарно на файл или tape. Cleanup деструктора/move-assignment находится
вне этих кредитов и не обещает ограниченную задержку.

[Профиль захватов](../../tests/fixtures/subscriptions/CAPTURES.ru.md) разделяет
реальные direct-distributor Java-наблюдения и нативную политику. Прямой Java REMOVE
с ненулевым обычным значением не всегда представим wire REMOVE: декодер задаёт
обычным полям значения по умолчанию и indexed removal time. Нативное сравнение
должно связывать фактический декодированный ввод и явно выбранные представимые
фазы; оно не доказывает полную wire-эквивалентность каждого прямого операнда.
Профиль также исполняет 104 кешированных ключа через default initial/update quantum
100. После одной initial-записи, обновления полученного ключа и ещё 99 initial-записей
обновлённый ключ выдаётся перед оставшимися четырьмя initial-записями. Этот больший
случай дополняет небольшую частичную выдачу.

## Скомпилированный фрагмент драйвера

Эта функция исполнялась в production-only installed consumer на Windows и Linux.
Вызывайте её из сериализованного драйвера с готовой `LiveSession`, соответствующей
неизменяемой схемой и одним целым TICKER_DATA envelope для `Sample/ALPHA/Value10`.
Локальная запись 0 — untimed `Sample` с одним compact-int полем `Value` (тип 8);
драйвер уже передал соответствующее peer description. Функция применяет локальные
интересы; wire-вывод подписок и жизненный цикл I/O принадлежат драйверу.

Конечные лимиты резервируют два представления, четыре cache-ключа, пять событий,
512 KiB cache backing и 1 MiB collector backing при отдельных desired-state бюджетах.
Work-кредитов достаточно для этого небольшого примера. Ноль означает первую выдачу,
initial-повтор позднему представлению и вытеснение после последнего интереса.
Ненулевой результат обозначает неудавшуюся проверку; копирование схемы может передать
исключение выделения памяти. Заимствованная запись читается до инвалидирующего
`pop`. Сборка и линковка описаны в [CPP build](../../docs/CPP-BUILD.ru.md).

```cpp
#include <qd/collectors/ticker.hpp>
#include <cstddef>
#include <span>
#include <utility>

namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;
/** Exercise one externally driven frame and two independent pure local interests.
 * The caller owns the ready serialized LiveSession and supplies exactly one whole TICKER_DATA
 * envelope with Sample/ALPHA/Value10. This function neither composes upstream interests nor closes I/O.
 * @param source Actual externally owned protocol session, serialized through this call.
 * @param schema Matching complete immutable local schema, copied with preserved provenance.
 * @param bytes One complete controlled data envelope.
 * @return Zero after both deliveries are acknowledged and both views close; nonzero on a failed check.
 */
int runTickerExample(p::LiveSession &source, const c::LocalSchema &schema, std::span<const std::byte> bytes) {
    auto copy = schema;
    s::TickerWork work{1000, 1000000, 1000000, 100000000};
    const s::TickerLimits limits{2, 4, 5, 512 * 1024, 1024 * 1024, {4, 8, 32, 128, 16, 256, 16}};
    auto ticker = s::TickerCollector::create(std::move(copy), limits, {false, 100}, work);

    if (!ticker) {
        return 1;
    }

    auto one = ticker->openView(work);

    if (!one) {
        return 2;
    }

    const c::SubscriptionRequest request{{schema.recordContext(0), u"ALPHA"}, {}};
    c::SubscriptionWork commands{16, 100000, 100000};

    if (!ticker->add(*one, std::span(&request, 1), commands, work)) {
        return 3;
    }

    // Desired application is local; the external driver decides any wire composition and output.
    p::LiveWork inputWork{16, 1000000, 1000000, 1000000};
    auto input = source.feed(bytes, inputWork);

    if (input.status != p::LiveStatus::EVENT || input.accepted != bytes.size() || !input.event) {
        return 4;
    }

    if (!ticker->stage(std::move(*input.event), work)) {
        return 5;
    }

    auto first = ticker->front(*one, work);

    if (!first || !*first) {
        return 6;
    }

    const auto &record = *(**first).value.sourceRecord;
    const auto value = record.typedValues.empty() ? record.values[0] : record.typedValues[0].raw;

    if (!record.context.matches(schema.recordContext(0)) || record.symbol != u"ALPHA" || value != 10) {
        return 7;
    }

    // Copy needed scalars before pop; borrowed record/view storage must not escape its invalidation boundary.
    if (!ticker->pop(*one, work)) {
        return 8;
    }

    auto late = ticker->openView(work);

    if (!late || !ticker->add(*late, std::span(&request, 1), commands, work)) {
        return 9;
    }

    auto initial = ticker->front(*late, work);

    if (!initial || !*initial || (**initial).kind != s::TickerDeliveryKind::INITIAL) {
        return 10;
    }

    if (!ticker->pop(*late, work) || !ticker->closeView(*one, work)) {
        return 11;
    }

    auto shared = ticker->latest(request.key, work);

    if (!shared || !*shared || !(**shared).current) {
        return 12;
    }

    if (!ticker->closeView(*late, work)) {
        return 13;
    }

    auto removed = ticker->latest(request.key, work);

    return removed && !*removed ? 0 : 14;
}
```
