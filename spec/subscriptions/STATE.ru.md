# Ограниченное целевое состояние подписок

[English version](STATE.md). Английская версия нормативная.

`qd/core/subscription.hpp` в `qdcxx_core` реализует чистое целевое состояние из
[контракта подписок](CONTRACTS.ru.md). Здесь нет I/O, парсинга, callback dispatch, доставки
коллекторов, часов или фонового планирования; JSON/runtime зависимости не нужны.
`SubscriptionChange` — синхронный локальный результат применения, не wire emission и не
подтверждение peer. Барьеры callbacks и общие runtime-кредиты остаются отдельной работой.

## Идентичность, текст и владение

Move-only `SubscriptionState` создаётся из валидированной `LocalSchema`, одного
`SubscriptionFamily` и явных `SubscriptionLimits`. Сохраняются точные record provenance и
факты indexed layout, а не объявления схемы. Входная схема не обязана переживать состояние.
Ограниченная копия схемы сохраняет provenance; независимо созданная схема той же формы — нет.

Ключ содержит `RecordContext` и заимствованный точный UTF-16 `string_view`. Текст нельзя менять
во время вызова; при эффективном изменении он копируется в собственное упакованное хранилище.
Null data pointer недопустим, включая view по умолчанию. Ненулевой указатель с нулевой длиной
допустим. Встроенный ноль и непарные суррогаты сохраняются. Нормализации и glob expansion нет;
точный текст с начальным `*` отклоняется. Явный All относится к будущему API.

HISTORY-ключ требует явных indexed-time метаданных записи для add, replace **и remove**,
даже если удаление отсутствующего ключа иначе было бы no-op. Add/replace HISTORY требуют
знаковую raw64 границу; STREAM/TICKER отклоняют переданную границу. Remove старое значение
не требует. Любое представимое signed64 — сырое record time, не миллисекунды; граница — атрибут ключа.

`entries()` возвращает заимствованные неизменяемые views. Эффективное изменение, move,
присваивание или уничтожение их инвалидируют; no-op и отказ сохраняют.
Копируйте только `membership`, чтобы удержать владеющий токен допуска. Он сохраняет фиксированную
непрозрачную идентичность владельца, а не текст/записи. `isCurrent()` отвергает default,
чужие и отозванные активации. Move переносит идентичность; прежние токены назначения становятся
неактуальны. У moved-from entries/revision пустые/нулевые, family сохраняется; отказ команд описан ниже.

## Команды и ошибки

Add выполняет upsert, одинаковые дубликаты схлопываются; изменённая граница HISTORY создаёт новую
активацию. Remove принимает только ключи; отсутствующие валидные ключи — no-op. Replace публикует
ровно проверенную карту; пустой вход и clear очищают её. Противоречивые дубликаты атрибутов
отклоняют команду целиком. Неизменённые memberships сохраняют активации.
Remove/re-add никогда не делает старый токен актуальным.

Эффективная команда увеличивает revision один раз. No-op/отказ сохраняет карту, revision,
активации и borrowed views. Проверки, резервы работы/пика и построение кандидата завершаются
до публикации через swap. Вход может заимствовать текст текущего состояния.
No-op не выделяет память, но проверяет вход и расходует работу. Успешный clear не выделяет
хранилище кандидата; отказ использует обычный путь ошибки, включая служебную память механизма исключений.

Возвращается `std::expected<SubscriptionChange, SubscriptionError>`. Различаются ошибки
state/context/selector/history, противоречивый вход, пределы ресурсов/работы, естественное
исчерпание ID и отказ памяти. Сначала проверяется/списывается кредит команды: moved-from с
нулевым кредитом получает WORK_LIMIT, иначе INVALID_STATE без чтения входов.
`create()` отдельно отвергает неверное family, избыток метаданных и отказ памяти.

Доступ сериализует вызывающая сторона. Потокобезопасного command port и reentrant callback API здесь нет.
Счётчики revision/activation не оборачиваются и не переиспользуются; естественное исчерпание uint64
защищено и проверено чтением кода, а не практическим выполнением до исчерпания.

## Конечный учёт

Все поля limits обязательны; ноль означает ноль. schemaRecords ограничивает фиксированные
сохранённые метаданные. entries/totalSymbolUnits ограничивают запрошенные resident slots/UTF-16
ёмкость; symbolUnits — один символ; commandEntries включает дубликаты заимствованного входа.
peakEntries/peakSymbolUnits учитывают старые и новые backing arrays одновременно.
Временных динамических карт/копий ключей нет. Пиковые настройки могут запретить даже уменьшающую
карту команду; clear второй массив записей не требует.

Фиксированная идентичность владельца и удерживаемая непрозрачная идентичность схемы отделены от
переменных буферов; токены могут продлевать их жизнь. Общие registration/lease кредиты будущего
runtime этим компонентом не ограничиваются. Slots/units — логические запросы хранилища, а не
служебная память allocator/exception runtime или измеренный RSS процесса.

Переиспользуйте один изменяемый `SubscriptionWork` для команд явно заданного turn. Успешно
списанные commands, key/metadata/comparison visits и просмотренные/скопированные UTF-16 units
не возвращаются при отказе/ошибке памяти. Свежие кредиты задаются на выбранной границе следующего
turn; накопительного лимита на всю жизнь feed нет. Консервативные сканирования могут быть
квадратичными по входу или resident×input; нехватка работы даёт отказ до публикации.

## Пример

Числа ниже — иллюстративные входы, не производственные defaults. Полная программа C++23
линкуется только с `qdcxx_core` и показывает идемпотентную revision и отзыв membership:

```cpp
#include "qd/core/subscription.hpp"

namespace core = org::ttldtor::qdcxx::core;

int main() {
    constexpr std::int32_t COMPACT_INT_FIELD_TYPE = 8;
    const core::LocalRecord record{
        .id = 0, .name = u"Quote", .hasTime = false, .fields = {{COMPACT_INT_FIELD_TYPE}}, .lookups = {{u"x", 0}}};
    const core::SchemaLimits schemaLimits{.records = 1, .fields = 1, .lookups = 1, .nameUnits = 5, .totalNameUnits = 6};
    auto schema = core::LocalSchema::create(std::span(&record, 1), schemaLimits);

    if (!schema) {
        return 1;
    }

    const core::SubscriptionLimits limits{.schemaRecords = 1,
                                          .entries = 4,
                                          .symbolUnits = 16,
                                          .totalSymbolUnits = 64,
                                          .peakEntries = 8,
                                          .peakSymbolUnits = 128,
                                          .commandEntries = 8};
    auto state = core::SubscriptionState::create(*schema, core::SubscriptionFamily::STREAM, limits);

    if (!state) {
        return 2;
    }

    core::SubscriptionWork turn{.commands = 8, .keyVisits = 512, .symbolUnits = 4096};
    const core::SubscriptionRequest request[] = {{{schema->recordContext(0), u"AAPL"}, std::nullopt}};
    const auto added = state->add(request, turn);

    if (!added || !added->changed || added->revision != 1) {
        return 3;
    }

    const auto membership = state->entries()[0].membership;
    const auto repeated = state->add(request, turn);

    if (!repeated || repeated->changed || repeated->revision != 1) {
        return 4;
    }

    const core::SubscriptionKey key[] = {{schema->recordContext(0), u"AAPL"}};
    const auto removed = state->remove(key, turn);

    if (!removed || removed->revision != 2 || state->isCurrent(membership)) {
        return 5;
    }

    return 0;
}
```

Отдельный CTest `core_subscription_state` проверяет изменения, identity, HISTORY, ёмкости,
работу и allocation rollback. Выполненные конфигурации и ограничения — в
[проверках](../../tests/conformance/subscriptions/STATE-VALIDATION.ru.md).
