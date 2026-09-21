# Покрытие conformance STREAM_DATA

[English version](COVERAGE.md). Английский документ является нормативным.

[Тесты](../../stream_data_tests.cpp), [negative checker](../../check_stream_data_negative.py),
[Java coverage](../../../fixtures/protocol/stream-data/COVERAGE.md), [контракт](../../../../spec/protocol/STREAM-DATA.md).

Четыре doctest cases выполняют 85 549 assertions. Выбраны все 35 наблюдений Java. Успешные сообщения
поддерживаемого профиля сравнивают фактические local ID, точные UTF-16, локальный порядок values и нулевые
flags. Ошибочные Java messages сравниваются по corruption outcome; C++ отдельно сохраняет завершённые
записи. Ненулевые flags 1/2 — явные rejection cases с отдельной проверкой Java delivery. Прямые проверки
задают C++ error stage, consumed/errorOffset и partial-prefix policy. Универсальная эквивалентность
исключений/callbacks не утверждается.

Whole, каждый two-chunk split (включая пустые края) и bytewise FrameReader feeds сравнивают точные
полные description/data payloads и одинаковые результаты decoder. Тестовый driver использует готовый
caller-supplied context, а не production session engine. Reset-repeat и обрезанный символ перед другим
полным валидным кадром подтверждают свежее состояние и отсутствие cross-frame borrowing.

Snapshot tests проверяют дубликаты, отрицательные ID, неверные names/types, неподдерживаемые типы,
некорректные local lookup targets, пустые контексты, INT_MAX sparse ID, aggregate action/name budgets и
точные границы schema record/field/lookup/name. Ownership checks уничтожают исходные schema/descriptions/
input/context. Decoder tests отдельно уменьшают каждый точный бюджет: payload, все processed records,
wire fields, emitted records/values/symbol units, symbol remaining-input/per-value/state. Repeats,
skipped и zero-field records учитываются соответствующими бюджетами. Missing slots, duplicate targets
и unknown middle fields/records сравниваются с Java. Primitive errors целевых и пропускаемых values,
resource-before-read precedence, undefined repeat, invalid lengths и nonzero flags проверяются напрямую.

## Строгая TSV schema

Допустимые rows и точные arities: profile(7), case(2), descriptions(2), payload(3), wire(2), record(6),
callback(4), error(3), end(6), complete(2). Profile точный; имена уникальны, обязательны все 35 cases.
Проверяется порядок description/payload/wire. Индексы payload/record/callback последовательны;
проверяются hex/unit cells и signed int32 values. Проверяются local ID и value arity. Числа callback
должны совпадать с предшествующими records; errors должны быть message/type-15. End проверяет полное
потребление wire, нулевой pending и точные record/callback/error counts. Ровно один complete завершает
файл; unknown rows, пустые/trailing данные, duplicate completion, неверная arity и missing coverage ошибочны.

Все row types либо сравниваются с decoder/frame behavior, либо проверяются как явная Java observation
structure/profile/batching. Ни одна row не игнорируется молча. Пятнадцать временных negative cases:
symbol, value, ID, порядок values, failure row/outcome, unknown row, arity, duplicate/trailing/missing completion,
отсутствующий обязательный case, value вне диапазона, wire type и missing file. Неизменный baseline затем успешен.

Не выполнялись: allocation-failure injection, огромные allocations, 32-bit targets, исчерпывающие Unicode/
penta/compact inputs, optional metadata/replacers/conversions, event/time flags, dynamic session,
transport, writer, importer и performance checks. Пять прежних suites остаются regression checks.
