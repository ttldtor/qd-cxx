# Исполнение event callback

По умолчанию сохраняется SDK worker. `Endpoint::setEventExecutor` задаёт default для event listeners; `Subscription<Event>::setEventExecutor` задаёт override. Null у подписки наследует endpoint, null у endpoint возвращает исполнение на worker. Методы не меняют immutable endpoint configuration, transport properties и wire descriptors. Getter показывает принятый policy intent, а не завершение старой выбранной доставки. После close admission отклоняется с `CLOSED`. State и subscription-change listeners сохраняют прежние native-каналы.

## Выбор и последовательность

Набор listeners/policy неизменен для полного admitted batch. SDK worker готовит owning typed events, приостанавливает следующие selections этой подписки и передаёт move-only `CallbackTask` методу `CallbackExecutor::post`. На подписку приходится не больше одного пакета в preparing/posting/queued/running/retiring. Принятие post не означает завершение callback. Executor может исполнить задачу inline, на другом потоке, изменить порядок разных подписок или удержать задачу. Callback одной подписки последовательны и упорядочены. Concurrent/repeated run получает право исполнения только один раз; moved-from, cancelled и старые поколения инертны. Результат run описывает обработку задачи; фактический исход listeners показывает статистика.

Move/destruction самой задачи не должны пересекаться с её member calls. Executor обязан быстро возвращаться из post и при обычной работе исполнять либо уничтожать принятые задачи. SDK владеет пакетом, но заимствует workers и не останавливает чужую работу приложения. Смена policy влияет на последующий выбор; старая selection удерживает policy до post, вызова и последнего capture destructor. Application calls и уничтожение policy/captures происходят вне runtime/endpoint/subscription/pool locks. Бросающий destructor нарушает обычный C++-контракт уничтожения.

## Исходы и shutdown

| Исход | Учёт и диагностика |
| --- | --- |
| Reject до run | Selected events discarded ровно один раз; `EXECUTOR_REJECTED`; подписка fails. |
| Post throws до run | Discarded один раз; `EXECUTOR_FAILURE`; подписка fails. Текст исключения не сохраняется. |
| Принятая задача уничтожена без run | Discarded один раз; `EXECUTOR_DROPPED`; подписка fails, без retry. |
| Reject/throw после начала run | Фактический callback outcome сохраняется; `EXECUTOR_CONTRACT` дополнительно завершает подписку с ошибкой. |
| Listener throws | Существующие `CALLBACK_FAILURE`, статистика и изоляция подписки сохраняются. |
| Queued cancellation при close/reset | Discarded один раз; advisory `CALLBACK_CANCELLED` и gap, без выдуманной transport/listener failure. |

Close остаётся неблокирующим запросом. SDK worker отменяет not-started пакеты без исполнения/освобождения удержанной host-задачи. Run, получивший право до cancellation, не прерывается. Running callback сохраняет существующие close-cutoff rules. Await использует condition wait создателя и включает SDK-owned post/run/capture retirement; чужие host jobs не join-ятся. Произвольно заблокированные post, callback и destructors нельзя preempt; жёсткого deadline их завершения нет. Creator affinity и отказы self/reentrant-await сохраняются. Polling клиента для завершения retirement не требуется.

## Retirement и смена source

Пакет удерживает completion от queue pending→in-flight до результата post, фактической функции и уничтожения последних selected listener/policy captures. In-flight не превращается в delivered из-за принятия post или возврата worker adapter. Calls/durations учитывают только реальные вызовы listeners, исключая ожидание queue/post. Delivered/discarded/callback-failed списываются ровно один раз.

Automatic reset сначала запрашивает cancellation queued/not-started пакетов; facade выполняет её на существующем worker, затем reset ждёт pure outstanding-packet fence. Explicit reconnect/clear/close также отменяют not-started работу. RESET и новый положительный source epoch не обгоняют старые callbacks, последние captures и completion статистики. Embedded fence заимствуется только worker-owned recovery и counted packets. Quiescence и освобождение fence публикуются под одним pool lock до возможной замены registry-владельца endpoint. Поздний task token не удерживает runtime/native owners.

Пассивный private cut — `{quiescent, retirementRevision}`. Preparing/posting/queued/running/retiring не quiescent. Revision увеличивается после статистики и последних captures; смена policy её не сбрасывает. Вместе с endpoint clear/logical/mapping fences он квалифицирует history publication. Чтение snapshot не запускает progress.

## Reentry и конечные ресурсы

Реальные data scopes образуют counted cohort runtime: первый вход начинает fence, только последний выход снимает его. Queued-задачи не считаются исполняющимися. Notifications из post/run/selected destruction дополнительно зависят от точного pool/slot/generation barrier пакета. Выход отдельного thread scope не освобождает их раньше полного retirement.

Run внутри другого callback/capture резервирует fixed after-return continuation существующего subscription-change driver и возвращает `DEFERRED`. Задача выполняется итеративно после exact source retirement: без recursive event delivery, ожидания другого callback, нового worker, repost или host polling. A→B→A и cross-runtime mutations делят конечные turn credits. Исчерпание явно даёт `WORK_LIMIT`, очередь не растёт молча. После отмены target остаются только инертные pure identities. Пересекающиеся parallel data callbacks могут консервативно задержать subscription-change delivery до завершения cohort.

Pure pool создаётся лениво при первом custom policy. Число физических reusable identities равно `maximumEndpoints * maximumSubscriptionsPerEndpoint`; поколения не оборачиваются. Pool/control-block allocation остаётся во владении до фактической deallocation, включая stale host tasks. Stale task удерживает fixed detached metadata, но не пакет, callable, endpoint, bootstrap, socket или thread. Удержание задач разных runtime сохраняет отдельный pool каждого; process-global quota не обещается.

`maximumCustomCallbackSubscriptions` ограничивает simultaneous custom/selected reservations, `maximumCallbackBytes` — logical pool storage и packet reservations. Пакет резервирует `1 MiB + 4096 + 20 * sizeof(RegisteredEvent)` logical bytes. 1 MiB покрывает полную owning projection текущего four-record profile (aggregate text/opaque fields и record arena 64 KiB); control allowance покрывает packet/vector/policy-reference metadata. Existing listener ledgers отдельно удерживают charge selected callables до destruction. Pool metadata: `sizeof(pool) + slotCount * sizeof(slot) + 4 * sizeof(void*)` logical bytes. Это консервативные credits, а не измерение allocator overhead/RSS. Opaque capture graphs и host scheduler allocations принадлежат приложению.

Endpoint change предварительно проверяет все inheriting subscriptions; overrides независимы. Новая inheriting subscription резервируется до publication. Отключение custom сохраняет selected reservation до exact retirement. Pool initialization — O(slot count), endpoint change — O(subscription count), event work ограничена profile/listener/turn limits. Новый SDK thread pool и unbounded task queue не добавляются.

Медленный default callback по-прежнему блокирует общий SDK worker. Custom callback приостанавливает свою подписку, позволяя worker и siblings продолжать работу. Source change endpoint ждёт его старые пакеты. Общий serial host executor может задерживать соседние подписки; отдельные policies помогают только при наличии host resources. Не меняются queue conflation, finite overload и transport flow control; throughput/callback latency не гарантируются.

## Сопоставление с Java

[Закреплённый Java endpoint contract](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L657) охватывает все asynchronous user interactions и запрещает reject. Native `EventExecutor` намеренно относится только к событиям. [Subscription API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedSubscription.java#L600) даёт override/null inheritance; [actual raw observations](../../tests/fixtures/callback-execution/oracle.tsv) сохраняют эти поддерживаемые selection/serial behaviors.

Native null у endpoint возвращает worker; Java его отклоняет. Native отклоняет policy admission после close, Java присваивает executor закрытой подписке. Native reject/throw/drop/cancel diagnostics — явные расширения Java-требования nonrejecting executor. Эти timing/error отличия не означают полную эквивалентность Java endpoint executor. [Runner](../../tools/upstream/callback-execution-oracle/README.ru.md) проверяет actual supported Java behavior; C++ causal tests подтверждают native failure/lifetime rules.

After-return task/target identities используют существующую конечную causal-driver allocation, размер которой задаёт `maximumSubscriptionChangesPerTurn`. Дополнительные default custom pool и per-task control block не выделяются.
