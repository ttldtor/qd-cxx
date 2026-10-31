# Проверка STREAM-коллектора

[English version](STREAM-VALIDATION.md). Английская версия нормативная.

Native test проверяет [контракт коллектора](../../../spec/subscriptions/STREAM-COLLECTOR.ru.md)
через настоящие полные события `LiveSession` и изменения desired state. Decoder,
dispatcher и socket не добавляются. Окончательный pure-profile CTest: **41/41**
на Windows x64 MSVC 19.42 Release и Linux x86-64 GCC 13.3 Debug ASan/UBSan;
основной LSan OFF. У коллектора 15 cases, включая полную standard Quote.

## Поведение

- Полностью одинаковые последовательные записи остаются разными, interleaved
  records сохраняют порядок. Unrequested/pre-add input не становится latest cache.
- Effective revisions отзывают queued activations; failed/no-op сохраняют offers.
  Remove/re-add не оживляет прежние данные. Два коллектора с независимой provenance
  не получают чужой интерес.
- Whole-frame record/frame/backing limits проверены на exact/exceeded с reserved
  terminal evidence. Work exhaustion сохраняет input/queue/offer для retry.
- Последнее освобождение event/snapshot предварительно оплачивается. Старые binding
  generations и значения переживают session destruction; drain сохраняет epoch pin.
- Проверены empty/control/wrong-family/moved events, zero capacities, точный необычный
  UTF-16, повторное использование rings и traffic за пределами одного turn.
- Factory allocation failure сохраняет desired ownership; stage/front/pop не выделяют
  память. Malformed current frame не публикует completed decoder prefix.
- Полная восьмиполевая default Quote schema читает исходные `time-1.qtp` type2/type15
  envelopes. После session destruction проверяются все raw/type/context values,
  prepared Quote mapping и независимо сохранённый результат после pop.

## Сравнения с production

`CollectorOracle` выполняет закреплённые Java QD STREAM/QDAgent/QDDistributor/RecordBuffer
пути. Выбраны `stream-every-event-order`, `stream-duplicate-two-clients`
и `stream-identical-filter`. Последний фиксирует три одинаковых ALPHA values,
посторонние record/symbol и отсутствие pre-add replay. Прежние collector rows
и wire/session artifacts сохранены.

Native test выводит actual admitted input, subscription и offered/popped output
semantic tuples/counts. Windows/GCC traces побайтно одинаковы: 12 723 bytes,
SHA256 `ba2aa451c2a265a51c93534a102f749009dba3871d8d4a15c08d3331cfa3e1b3`.
Независимое сравнение совпало по 185 coordinates в 20 phases трёх cases.
Cipher/raw-symbol representation и QD pull-loop `more` исключены явно. Queued
local membership barriers остаются нормативным локальным поведением, не новым
Java observation или доказательством общего upstream MUX.

Fresh production Capture/Verify покрывают 76 cases/5 681 observations полного
subscription profile: 22 wire, 22 collector и 32 session cases. Отдельная новая
native-control verification принимает Windows/GCC live-session output; controls
344 bytes и production callback artifact 4 778 bytes прежние. Оба native captures
проверяются Windows JDK8.

## Сборка, владение и документация

Production-only builds и перенесённые installed consumers проходят на обеих
платформах, включая factory/add, реальный feed/stage и явный front/pop. У каждого
пакета 27 hash-preserved files, runtime/JSON SDK не нужен. Совместимость с optional
TCP проверена отдельным MSVC configure/export tree; SDK там не собирался и не
запускался. Существующие пути исполнения runtime не изменены.

Независимое исполнение покрывает прежние 13 contract cases и новый Quote case,
а также два protected session-contract cases. Pinned C++ format/check/idempotence,
Java format/check и Doxygen 1.18 без предупреждений проходят. Generated API pages
и собираемый пример installed consumer проверяются отдельно.

Requested capacities и byte-work allowances не включают allocator overhead,
CPU instructions и RSS. Destruction/move assignment вне turn credits. GCC Release
также выдаёт прежнее предупреждение conversion-buffer в `wide_decimal.cpp`;
исходник не изменён, полный sanitizer regression suite проходит. Callback,
wildcard, TICKER/HISTORY, MUX и full subscriptions этим не квалифицируются.
