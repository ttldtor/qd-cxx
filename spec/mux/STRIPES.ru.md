# Полосы в локальной композиции MUX

[English version](STRIPES.md). Английская версия нормативна.

Подключайте [`qd/mux/striped.hpp`](../../include/qd/mux/striped.hpp) и optional target
`qdcxx_mux_queue`. `mux::StripedCollector` владеет настоящим [MuxQueue](QUEUES.ru.md) на каждую
физическую [полосу символов](../core/SYMBOL-STRIPES.ru.md). Каждый child содержит свой native
STREAM, TICKER или HISTORY collector и независимые регистрации потребителей. Каждая точная
входная строка попадает в свой child; внутри полосы сохраняется относительный порядок строк.
Порядок callbacks разных полос не определён. Полоса независима от канала, event source,
connection epoch и executor domain. Композиция не добавляет wire format, транспорт, worker,
часы, внешнюю зависимость или обещание линейного масштабирования.

## Создание и независимый admission

`createStream`, `createTicker` и `createHistory` выбирают одно неизменяемое семейство всей
композиции. Передайте валидный владеющий `SymbolStriper`, схему, ровно один полный набор native
options на каждую полосу, явные `StripedLimits` и оставшийся `StripedWork`. Все children создаются
до публикации root. Неуспешная фабрика не возвращает частичный root. Child QueueOptions,
collector limits, retention options, demand limits и `ClientLimits` независимы: общий минимум
квот и неявное умножение квот не применяются.

Root резервирует собственные object/shared controls, удержание immutable schema/partition,
массивы children/results и хранилище живых логических ports. `residentBytes` показывает эти
запрошенные либо консервативно учтённые root bytes; физические child ledgers отдельны. Detached
pure control счётчика байтов исключён из data capacity по существующей модели учёта.
`operationBytes` ограничивает одну скопированную команду, регистрацию либо операцию source плюс
текущий slice, включая собственные descriptors, capacities строк и удерживаемые client metadata.
Три явных slice bounds покрывают operational metadata, скопированные legacy values и удерживаемый
pure payload. Ноль означает ноль. Allocator bookkeeping, runtime exception machinery, непрозрачные
captures потребителя и принадлежащие вызывающему inspection copies в эти scopes не входят.
Возвращаемые копии оплачиваются явным work и ограничены `operationBytes`; их удержание не
удерживает operational root.

`StripedWork` содержит root operation/visit/text/byte credits и существующие child, submission и
event work objects. Реализация их не пополняет. Попытки и неуспешная подготовка расходуют уже
проделанную работу. Отказ до приёма `beginStage` оставляет исходный event пригодным к использованию.
Дополнительной очереди source нет; фиксированного потолка полос, clients или строк сверх явных
существующих grants и представимого размера allocation тоже нет.

## Одна возобновляемая операция

Вызовите `beginOpen`, `beginAdd`, `beginRemove`, `beginReplace`, `beginClear`, `beginStage` либо
`beginReset`, затем повторяйте `advance` с явными оставшимися credits. Текущая операция только одна.
Следующий begin возвращает `BUSY`, в том числе пока открытый client ещё не забран. `takeClient`
однократно передаёт полный новый логический client после открытия всех нужных регистраций.
Operation IDs локальны для root и не переиспользуются.

Действие каждого child сохраняет существующий атомарный контракт. Root хранит точный результат
по полосам: required, submitted, completed, настоящий ticket status, input admission, reset identity,
source/admitted/staged row counts и исходную типизированную ошибку. `COMPLETE` требует завершения
локального барьера всех нужных children. `APPLIED` команды не означает source synchronization или
доставку. Staged row counts описывают успешные native input calls, включая строки, проигнорированные
настоящими правилами eligibility коллектора. Это отдельная величина от числа доставленных копий.

Общего rollback нет. Поздний отказ сохраняет завершённые children и точное продолжение текущего
child. Повторяемый отказ work/producer pressure возобновляет тот же child; постоянный оставляет
`FAILED` до явной отмены либо stop. Завершённые действия не отправляются заново. Progress и
заимствованные result spans истекают при следующем begin/advance или изменении owner.

`requestCancel(operationId)` фиксирует cutoff и не отправляет действия следующим children. Уже
принятая child команда обязана получить свой реальный ticket result; отмена поэтому может показать
этот child как `APPLIED` и completed. Принятый незавершённый source использует существующий путь
pending-source discard. Принятый reset сохраняет настоящие source commit/global ACK.
`CANCELLED` означает завершение этого продолжения и освобождения принадлежащего операции ввода;
завершённые ранее эффекты сохраняются. `unsubmittedSourceRecords` отдельно считает root-owned
строки, исключённые до child admission; потерянные per-client copies не выдумываются. Отмена source
отдельна от закрытия children.

## Demand, stripe hint и настоящие фильтры

Логический client содержит существующие независимые child ports. Mux fan-out сохраняет отдельные
demand, очереди, pause/stop callbacks и retained leases потребителей. Точные команды идут в одну
выбранную включённую полосу. Явный STREAM `ALL` идёт во все включённые полосы. Допустимость
Exact/ALL и поддержка семейств остаются в существующем selector profile каждого child.

Необязательный `SymbolStripeFilter` в `beginOpen` — **stripe hint**. Доказанное статическое
пересечение позволяет не создавать ненужные child registrations; неизвестное рассматривает все
полосы. Hint независим от жёсткого record predicate. Например, `byhash4` и hint `hash0of8` выбирают
один child; его STREAM ALL всё равно получает `SYM2` и `SYM5`. Production Java
`withStripe(hash0of8)` ведёт себя именно так. Отдельный `withFilter(hash0of8)` оставляет только
`SYM2`. Подмена настоящего предиката подсказкой выбора children изменила бы поведение.

`StripedCollector` не устанавливает `FeedFilter`. Сохраняйте настоящий hard filter на существующей
верхней record boundary, например publisher physical record admission. Полная policy может
пересекать своё выражение с сгенерированным physical stripe leaf, продолжая проверять настоящую
policy. Pure core и collector targets не зависят от event API. Production oracle и regression
`stripe_filter_boundary` проверяют эти разные обязанности.

## Владение, callbacks и удерживаемые данные

Сериализуйте owner calls, moves и destruction. Копии `StripeClient` содержат pure fixed provenance,
учёт и существующие producer ports. Они не удерживают operational root, queue, protocol event,
identity snapshot или consumer callback. Разные копии tokens и immutable delivery leases допускают
межпоточное использование по существующим port rules. Последний публичный логический token
фиксирует abort во всех своих child ports, даже когда текущая операция ещё удерживает port metadata.

Только root содержит исходный принятый source event и максимум один текущий operational slice.
Shared receiving rows удерживают pure immutable payload aliases. Legacy rows копируют выбранные
symbols, nullable text/opaque values и scalar representations в независимую учтённую eager arena.
Client delivery использует существующую pure alias/copy boundary без удержания parent event,
live identity либо live snapshot. Physical flags, times, removals и исходная metadata сохраняют
свой существующий смысл.

Callbacks выполняются настоящим child MuxQueue вне его locks. Рекурсивные begin, advance и native inspection calls
возвращают `REENTRANT`. Reserved controls и пассивные progress/resource views не запускают child operations. Root сохраняет один сериализованный operational lease на время callback;
замена либо уничтожение публичного root внутри него запрещает дальнейшую работу и освобождает
старые children после возврата callback. Это не меняет документированное требование времени жизни
owner у отдельного MuxQueue. Исключения callback учитываются без повторного вызова. Destructor
или detached release не запускают новый callback.

## Native inspection, snapshots и reset

`observations` возвращает настоящие capacity, текущие resources, queue state, authoritative desired
revision, desired size, reset state и необязательный terminal outcome каждого child. Children
наблюдаются последовательно по индексу; версии описывают отдельные моменты наблюдения, а не
один атомарный общий момент. `aggregateEntries(stripe)` заимствует authoritative desired union
конкретного child.

`summary` суммирует physical desired entries и наблюдавшиеся root admission/stage calls. STREAM
ALL считается один раз на выбранный physical child. Итоговые `QueueCounts` доступны только после
настоящего завершения всех children. До этого они отсутствуют; недоступные сведения о потерях не
подменяются нулём. Известная отмена root, child `hadGap`, неизвестная eligibility и насыщаемые
счётчики явны. Upstream continuity неизвестна независимо от чистой локальной доставки.

Snapshot/inspection API сохраняет native контракты семейств:

- HISTORY `rangeSnapshot(client)` возвращает владеющие per-stripe точные keys, текущие desired/mapping
  revisions, raw source coverage, состояние доставки и gaps. `retainedHistoryRange` направляет
  ограниченный точный интервал в нужный child и возвращает существующие заимствованные retained
  rows. Покрытие source interval отдельно от полной delivered coverage.
  Объединённый владеющий снимок выполняет два оплаченных прохода запросов дочерних очередей
  и один раз резервирует точный итоговый размер. Первый проход считает ключи и память и сразу
  освобождает временный вектор каждой очереди; второй формирует возвращаемые наблюдения.
  Работа дочерних очередей оплачивает оба запроса и уничтожение временных данных, работа корня —
  обходы и итоговый результат. Между проходами нет продвижения владельца или callback.
  Параллельный зарезервированный stop может изменить признаки состояния, но не применённые ключи
  без продвижения владельца. Очереди по-прежнему наблюдаются последовательно: общий атомарный
  момент наблюдения не обещается.
- TICKER `visitTickerLatest` направляет точный key и вызывает существующий синхронный nonthrowing
  borrowed-value visitor. Удержание либо reentry через заимствованное значение не поддерживается.
- STREAM не хранит snapshot data; Java snapshot provider пуст. Вызовы HISTORY/TICKER-specific
  методов на другом семействе явно возвращают `UNSUPPORTED_FAMILY`.

`beginReset` запрашивает настоящий существующий source reset каждого child, сохраняя desired demand.
Завершение требует source commit и соответствующего collector-global ACK всех запрошенных children.
Client reset notices и точный per-stripe `acknowledgeReset(stripe,sequence)` отдельны: callbacks
остаются закрыты до настоящего ACK notice. Устаревший ACK не подтверждает новый интервал.
Старые immutable batches переживают reset и закрытие root. Внешний transport reconnect может
использовать этот локальный reset barrier; сам барьер не доказывает socket reconnection.

`requestStop` сначала фиксирует stop во всех children, затем выполняет текущую отмену и fair child
cleanup. `closed` требует настоящего terminal каждого child. Graceful HISTORY closure возвращает
`ABORTED` и `hadGap` для неполного requested range даже после доставки всех известных строк;
полное SNAPSHOT_END coverage позволяет завершить drain чисто. Ошибки children и известные discard
counts сохраняются. Stripe-local quota failure одной регистрации не навязывает её квоту здоровым
потребителям или полосам.

## Доказательства совместимости

Закреплённая база Java — [upstream.lock](../../upstream.lock). Границы исходников:
[StripedCollector](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedCollector.java),
[StripedAgent](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedAgent.java),
[StripedDistributor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-stripe/src/main/java/com/devexperts/qd/impl/stripe/StripedDistributor.java)
и [matrix Agent](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/impl/matrix/Agent.java).
[Production oracle](../../tools/upstream/stripe-oracle/README.ru.md) записывает настоящую composition
трёх семейств, partial retrieval, replacement, sizes, drops, closure и различия stripe hint/filter.
Native tests проверяют настоящую Mux delivery, partial progress, reset/ACK, immutable retention,
независимый отказ квот и закрытие. Эти локальные контракты не добавляют автоматический transport
или parallel execution API.