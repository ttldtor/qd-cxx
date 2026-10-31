# Формирование живых сообщений подписки

[English version](WRITER.md). Английская версия нормативная.

`qd/protocol/subscription_writer.hpp` и `qdcxx_protocol` предоставляют ограниченный composer
без I/O. Реализован вывод подписок/описаний записей; live session, транспорт, согласование,
коллекторы и подтверждение peer этим не реализованы. Профили recording `TapeWriter` не менялись.

## Layout и wire-профиль

`SubscriptionWriter::create` владеет ограниченной копией схемы с сохранением RecordContext identity,
полными явными `OutgoingRecord`, limits и одним фиксированным владельцем выходных кредитов.
Объявления сопоставляют local ID уникальным sparse sender ID в 0..INT32_MAX-1; имена записей
и полные типы полей соответствуют схеме, каждое логическое поле покрыто один раз через явный lookup.
Чужие схемы одинаковой формы не взаимозаменяемы. Большой допустимый sender ID не гарантирует,
что Java peer хватит памяти для его отдельного dense remapping.

Живой validator допускает indexed BINARY_FIELDS_V1 **только для формирования описаний**.
Он не включает новые recording data types и не расширяет старую фабрику OutgoingLayouts.
`describe(senderIds, work)` выдаёт один DESCRIBE_RECORDS/type 2 envelope в заданном порядке,
сохраняя повторы известных ID. Состояние announced/sent/readiness не ведётся.

`compose(message, requests, work)` сохраняет порядок и повторы tuples. Шесть идентичностей:
TICKER_ADD (11), TICKER_REMOVE (12), STREAM_ADD (16), STREAM_REMOVE (17), HISTORY_ADD (21), HISTORY_REMOVE (22).
Tuple содержит точный symbol, затем compact sender ID. Только HISTORY_ADD требует/разрешает
знаковую raw64 compact-long границу. Обе HISTORY формы требуют явных indexed metadata;
остальные сообщения и HISTORY_REMOVE отвергают переданную границу. Преобразование бизнес-времени не выводится.

Symbol state сбрасывается на каждом сообщении. Выбранный encoder выдаёт raw FD CharArray
и локальные для сообщения FF repeats; кратчайшая PentaCodec-форма Java не обещается.
Сохраняются ненулевая пустая строка, встроенный ноль и непарные UTF-16 units. Null data pointer
и точные селекторы с начальным `*` отвергаются; All/glob и неявных converters нет.
Описания используют прежние ограниченные UTFString/compact primitives.

## Жизнь выхода и частичная запись

Успешный вызов возвращает move-only `PendingSubscriptionOutput` с целым envelope.
`bytes()` — неизменяемый оригинал; `pendingBytes()` — текущий хвост.
`consume(n)` меняет лишь локальный cursor; выход за хвост даёт INVALID_CONSUMPTION без изменения.
Транспорт должен продвигать cursor на фактически завершённое число байтов. Полное consume не означает peer ACK.

Выход переживает вход/composer и переносится через move. Полные byte views следуют за allocation
к новому владельцу; прежние views назначения инвалидируются при замене.
Backing остаётся учтён даже после полного consume до фактического освобождения при destruction/replacement.
Фиксированный общий владелец кредитов переживает composer, пока есть выходы.
Наличие оригинала позволяет механически повторить байты; эпохи соединений и допустимость retry
обязан определять будущий session, а не факт доступности байтов.

Вызовы composer, доступ к sibling outputs **и уничтожение outputs** сериализуются и не реентерабельны.
Cross-thread release этим API не поддерживается. Limits относятся к исходному credit pool;
это не глобальная квота для независимо созданных composers/connection epochs.

## Ограничения, работа и отказ

Все ёмкости явные, ноль означает ноль. frameBytes ограничивает тело вместе с message identity
(и Java INT_MAX); operationBytes — целый envelope; operationEntries считает входные ID/tuples,
включая повторы. symbolUnits ограничивает один symbol, text — один description UTFString.
LayoutLimits отдельно ограничивают собственные metadata схемы/объявлений.

residentBytes и peakBytes учитывают старый удерживаемый backing вместе с новым кандидатом;
outputs ограничивает одновременно удерживаемых владельцев результата. Кредиты резервируются
до allocation и откатываются при ошибке. Байты выделяются точным запрошенным массивом;
allocator/exception overhead и фиксированная identity — не лимит RSS процесса.

Один SubscriptionWriterWork переиспользуется в явно выбранном turn. Creation и все sizing,
metadata/comparison, primitive и encoding passes расходуют явные кредиты; успешно списанная
работа не возвращается при ошибке. Свежие кредиты следующего turn позволяют продолжать feed
без неявного накопительного лимита на всю жизнь.

Валидация, арифметика, работа, allocation и encoding завершаются до публикации.
Типизированный SubscriptionWriterError сохраняет прежние outputs/configuration и не отравляет
здоровый composer. Частично закодированный результат наружу не выходит.

## Пример

Числа — иллюстративные входы. Полная программа C++23 линкуется с qdcxx_protocol
и проверяет владение cursor без открытия сокета:

```cpp
#include "qd/protocol/subscription_writer.hpp"

namespace core = org::ttldtor::qdcxx::core;
namespace protocol = org::ttldtor::qdcxx::protocol;

int main() {
    constexpr std::int32_t COMPACT_INT_FIELD_TYPE = 8;
    const core::LocalRecord record{
        .id = 0, .name = u"Alpha", .hasTime = false, .fields = {{COMPACT_INT_FIELD_TYPE}}, .lookups = {{u"x", 0}}};
    const core::SchemaLimits schemaLimits{.records = 1, .fields = 1, .lookups = 1, .nameUnits = 5, .totalNameUnits = 6};
    auto schema = core::LocalSchema::create(std::span(&record, 1), schemaLimits);

    if (!schema) {
        return 1;
    }

    const protocol::OutgoingRecord declarations[] = {{41, 0, u"Alpha", {{u"x", COMPACT_INT_FIELD_TYPE, 0}}}};
    const protocol::LayoutLimits layoutLimits{
        .schema = schemaLimits, .records = 1, .fields = 1, .nameUnits = 16, .totalNameUnits = 32};
    const protocol::SubscriptionWriterLimits limits{.frameBytes = 512,
                                                    .operationBytes = 1024,
                                                    .operationEntries = 8,
                                                    .symbolUnits = 64,
                                                    .text = {.bodyBytes = 64, .codeUnits = 32},
                                                    .residentBytes = 4096,
                                                    .peakBytes = 4096,
                                                    .outputs = 4};
    protocol::SubscriptionWriterWork turn{.operations = 16, .visits = 4096, .textUnits = 65536, .outputBytes = 65536};
    auto writer = protocol::SubscriptionWriter::create(*schema, declarations, layoutLimits, limits, turn);

    if (!writer) {
        return 2;
    }

    const std::int32_t senderIds[] = {41};
    auto description = writer->describe(senderIds, turn);
    const core::SubscriptionRequest interest[] = {{{schema->recordContext(0), u"AAPL"}, std::nullopt}};
    auto subscription = writer->compose(protocol::SubscriptionMessage::STREAM_ADD, interest, turn);

    if (!description || !subscription) {
        return 3;
    }

    const auto retained = writer->retainedBytes();

    // Cursor demonstration only: a real transport advances by its actual write result.
    if (!subscription->consume(1) || !subscription->consume(subscription->pendingBytes().size()) ||
        !subscription->pendingBytes().empty() || writer->retainedBytes() != retained) {
        return 4;
    }

    return 0;
}
```

[Производственный разбор захватов](../../tests/fixtures/subscriptions/writer/README.ru.md)
проверяет выбранный нативный вывод закреплённой Java. [Проверки](../../tests/conformance/subscriptions/WRITER-VALIDATION.ru.md)
отделяют выполненные профили от будущей квалификации live session/runtime.
