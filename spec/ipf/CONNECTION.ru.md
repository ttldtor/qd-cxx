# Контракт live IPF connection

[English version](CONNECTION.md) является нормативной.

## Источники совместимости

Используется закреплённый QD0ddc0a09779fb61d74cc0b37c6449ff47dcb715f:
[InstrumentProfileConnection](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/live/InstrumentProfileConnection.java),
[TimePeriod](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxlib/src/main/java/com/devexperts/util/TimePeriod.java).
[Actual JDK8 capture](../../tests/fixtures/ipf/live/connection-outcomes.tsv) различает
private Handler instrumentation и отдельный public asynchronous lifecycle. Универсальное
source/API/concurrency соответствие Java не заявляется.

## Выполнение и владение

Явный creator-runtime владеет одним lazy worker и одним непереиспользуемым lifetime-слотом;
maximumConnections≠1 отклоняется до SDK preparation. Owned bootstrap создаётся до потоков;
borrowed профиль хранит stable BootstrapLease, а не указатель на movable facade. Moved-to
владелец bootstrap живёт до creator join/cleanup. Native child, parser, sink и постоянный
transfer host создаются, исполняются и освобождаются на worker; process SDK cleanup — на creator.

Connection facade владеет stable Port. Callback может закрыть/удалить facade, поскольку
worker/operation/selections удерживают своё состояние. Close не выделяет память, не join-ит
и не вызывает SDK: это request-only cutoff. Каждый конечный внешний turn проверяет cutoff
до нового native/provider/output progress. Уже выбранная пользовательская работа может
завершиться. Hard latency для OS/filesystem/SDK/произвольного callback не обещается.

Start атомарно публикует CONNECTING после успешного создания worker; ошибка сохраняет
NOT_CONNECTED. Shutdown закрывает admission перед join. State/auth callbacks идут
на worker вне owner/native locks; для никогда не запущенного подключения финальный CLOSED
вызывается на creator при shutdown. Captures уничтожаются вне locks. Runtime нельзя
удалять/join-ить из worker или callback. Listener ledger резервируется до allocation;
удалённые selected nodes остаются учтены до освобождения последних ссылок, без цикла
Port↔ledger. Tokens не переиспользуются.

## Incremental и поколения

Обычный snapshot pipe сохраняет отдельную инстанциацию и layout/поведение. Live wrapper
использует IpfReadWork.records=1 на вызов, включая headers/comments/controls, без изменения
IpfReadLimits.records. Между feed/finish turns передаётся не больше одного owned profile
или control; handoff и дополнительный backing учтены. Whole-response/expanded-payload
накопления нет.

Pending сохраняет все standard/custom значения defensive copy. Source/request и входные
объекты неизменны до возврата create; shared provider соблюдает прежний callback/lifetime
контракт. Collector владеет immutable revisions. Flush выполняется по batch size, `##`,
`##COMPLETE` и чистому EOF. Ошибка не публикует pending: suffix переносится в следующую
попытку. Generation создаётся при первом flush и не меняется после COMPLETE; в конце
попытки попадает в old даже после ошибки. COMPLETE публикует pending, удаляет old и
отмечает первоначальное завершение. Nonlive EOF также завершает снимок; live EOF только
публикует pending. Ошибка не создаёт EOF/COMPLETE.

## HTTP, metadata, retry

Передаются X-Live: yes, прежний Accept-Encoding: identity и bounded source-reader headers.
Только точное provided означает live. Сохраняются прежние redirect/origin/auth/container
правила; authorization идёт на worker/native creator, секреты в observations не попадают.

Только fallback HTTP с прежним ненулевым Last-Modified передаёт If-Modified-Since и принимает304.
Равная ненулевая дата в200 пропускает body до изменения capability/CONNECTED и сохраняет
поколения. Metadata фиксируется после успешного native retirement: сохраняется более
строгая гарантия source reader; Java присваивает её после process, до resource close.
См. [ADR0009](../../docs/decisions/ADR-0009-live-ipf-connection-ownership.ru.md).

Первое completion sticky при reconnect; отдельно наблюдаются active attempt и последние
HTTP/native/container/IPF evidence. Marker перед поздней ошибкой не стирает owning prefix.
Первичная ошибка сохраняется перед cleanup; collector admission отмечается отдельно.

Normal wait использует updatePeriod; retry сначала1000ms, затем удваивается до текущего
updatePeriod и не сбрасывается после успеха. Изменение периода не меняет уже начатое
ожидание. Ноль означает конечную немедленную попытку при lifetime/attempt/work budgets.
Сохранены QD TimePeriod grammar и signed64 numeric delay, включая inf и проверенные
JDK8 Unicode decimal integer parts. Clock wait ограничивается lifetime deadline без
сложения огромного delay с clock. Отрицательные/синтаксически неверные значения отклоняются.

## Admission и ошибки

Независимо ограничены pending profiles/UTF-16 units, old tags, listener nodes/selections,
requested backing, attempts и lifetime. workUnits необратимо списывает1 за profile плюс
его copied units,1 за collector update и1 плюс old tags за generation removal; failed
copy/поздняя ошибка не возвращают credits. Collector model/comparison/delivery work и
retained revisions ограничены его независимыми настройками.

Native/byte/TLS work, HTTP/parser/copy counters, wire/redirect/attempt counts и decoder
input/output/call/work/member/metadata consumption накоплены между попытками. Новый parser
может начать formats заново, но grants/shared TLS history не обновляются. Cleanup reserve
каждой попытки отделяется от исходного native grant, без refill. После первой native error
допустим только существующий cache-only raw owned-prefix/verified TLS plaintext drain,
без ordinary poll, output, provider или придуманного EOF.

Resident admission учитывает requested objects/vectors/strings/workspace и явные control
allowances; это не точный STL allocator overhead/RSS и не бюджет произвольных captures.
Overflow, exact/one-below, OOM, reentry, cross-thread close и stable moved bootstrap —
отдельные проверки. Ошибка не объявляет завершённый снимок; owning revisions сохраняются,
а pending suffix остаётся private до следующей admission.

## Границы сборки и проверок

Опциональный BUILD_IPF_LIVE требует явного BUILD_IPF_TRANSFER и прежних qualified TLS/native/SDK;
новых зависимостей/скрытого startup нет. Pure IPF/collector остаётся SDK-free; OFF configure
не получает transfer dependencies. Installed consumer использует фактическую exported archive closure.

Targeted TSan проверяет actual private connection control без native/TLS archives. Проверка
полного facade/HTTPS требует qualified native/sanitizer profiles и lifetime review; helper-only
TSan не выдаётся за проверку всего SDK.

Connection резервирует непереиспользуемый nonzero domain под collector mutex с конечным identity exhaustion. Публичные integer tags сохраняют domain0. Removal сравнивает domain и generation; равное значение обновляет оба без notification. Разные runtime не удаляют profiles друг друга или manual domain0. Внутренний model entry дополнен8-byte domain; collector limits остаются counts/UTF-16/work, а не точными allocator bytes/RSS. Резервирование domain списывает одну lifetime connection work unit до публикации.

Уточнение потоков: state/auth callbacks выполняются на worker своего connection. Collector notifications следуют его независимому outside-lock dispatcher и при concurrent/nested update могут выполняться на другом текущем updating thread.

До allocation pending copy резервирует общий requested backing: возможный empty-map sentinel, custom node/key/value objects, capacities всех standard/custom strings с terminators и консервативной library alignment-request добавкой. Для поддержанных tree headers заложены четыре указателя; actual allocation-counting regressions связывают оценку с профилем MSVC/GCC. Inline string capacity/padding консервативно учтены повторно. Fixed vector objects и прежний logical pending-unit reserve входят в resident admission один раз; остаток resident backing вместе с этим резервом финансирует одновременно живущие копии. Dense custom data может исчерпать backing раньше logical limits. Flush освобождает copy bytes после уничтожения values; failed copy откатывает storage reserve без возврата work. Это новый connection backing, без изменения logical-budget контрактов обычного reader/collector.

Request-only close публикует cutoff под port mutex, освобождает его и синхронизирует notification через runtime control mutex, используемый при retry wait. Поэтому уведомление не теряется между проверкой false predicate и переходом в ожидание. Stop request выполняется вне обоих owner locks. Завершение worker не зависит от дополнительного уведомления при creator shutdown.
