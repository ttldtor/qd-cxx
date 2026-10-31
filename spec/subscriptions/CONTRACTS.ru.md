# Контракт native subscriptions и жизненного цикла

[English version](CONTRACTS.md). Английская версия нормативная.

## Статус и границы

Это выбранный проектный контракт будущих native subscriptions. Он задаёт требования к локальным API/state, но не заявляет реализованные client/collector/transport или Java compatibility сверх указанных прежних доказательств. Новые C++ declarations, runtime dependency или wire profile не добавляются. Обоснование дано в [архитектурном решении](../../docs/decisions/ADR-0005-live-subscriptions.ru.md).

Выбранная цель — read-side client для явно поддержанных binary records и caller-supplied immutable schemas: STREAM, TICKER и HISTORY, общий upstream demand, изолированные локальные потребители, finite resources и явные recovery/stop. [Текущие возможности](../../docs/CURRENT-STATE.ru.md) остаются источником реализованного поведения. [Матрица и сценарии](../../tests/conformance/subscriptions/CONTRACTS.ru.md) различают current support, fixed design, pending evidence и exclusions. Совместимость collectors/capabilities/wildcard подтверждается до зависимой реализации; выбор runtime/TLS/backend — отдельная предпосылка.

Чистый [API целевого состояния](STATE.ru.md) и [исходящий composer](WRITER.ru.md) реализованы. Остальной client/live-session/collector lifecycle здесь остаётся проектным контрактом. Наблюдения выбранного Java-профиля приведены в [захватах](../../tests/fixtures/subscriptions/CAPTURES.ru.md).

## Границы владения

| Граница | Чем владеет | Чего не содержит и не выводит |
| --- | --- | --- |
| Pure core | Validated schema/record identity, exact selectors, desired maps, mutation revisions и finite operation/resident accounting | Соединения, clocks, callbacks, runtime threads, I/O, JSON и process settings |
| Protocol session | Directional sender tables, immutable binding snapshots, negotiation state, partial input/output и одна connection epoch | Socket library, callback scheduling или неподтверждённое peer acknowledgement |
| Collectors/MUX | Выбранное delivery state, aggregate interests, membership eligibility и bounded per-client staging | Business conversions по именам или неограниченная история |
| Client dispatcher | Registrations, guards исполняемого callable, owning deliveries, command/control admission и completion barriers | Borrowed caller buffers после submission или nested delivery в user code |
| Runtime adapter | Execution host, socket/DNS/TLS operations, timers, progress и quiescence | Обратную зависимость core/codecs/events от runtime |

Pure core вызывается без concurrent/reentrant access. Сериализацию обеспечивает caller; проверка OS thread identifier не нужна. Upper client имеет один serialized owner domain. Отдельный bounded command port доступен другим потокам и callbacks; direct concurrent mutation owner state не поддерживается. Это контракт сериализации, а не выбор event loop или OS threading library.

Application сохраняет execution host до quiescence всех использующих его runs. Нельзя уничтожать/join execution host из собственного callback; запросить stop можно. Caller-driven host требует продолжать pumping после stop request. Pure driver не подразумевает background progress или real-time deadline.

## Идентичность и запрошенное состояние

Run использует один immutable local schema context. Каждая registration связывает этот context и одно contract family. Context и family не меняются; для другого family нужна новая registration, для другого context — отдельный run. Ключ desired entry — record identity этого context и selector. Local record IDs валидируются внутри своего context; одинаковые числовые IDs или schema shapes не делают независимо созданные contexts взаимозаменяемыми. Копии схемы сохраняют прежнюю [RecordContext identity](../../spec/events/EVENT-MAPPING.ru.md).

Exact selectors владеют UTF-16 code units. Case folding, trimming, normalization, currency conversion и sender-ID identity не вводятся. Null не является exact selector; пустая non-null строка остаётся отдельным ключом. Well-formedness symbol text не выводится: выбранное encoding сохраняет точные UTF-16 units, включая embedded zero и unpaired surrogates, в заданных пределах. Для record names действуют прежние schema validation rules.

Wildcard — отдельный явный selector без автоматического расширения строк. Exact strings с зарезервированным префиксом `*` этот native profile отклоняет, чтобы случайно не отправить wildcard. Literal escape, glob и regex syntax не придумываются. Поддержка wildcard family/peer ещё требует production evidence; неподдержанные варианты явно отклоняются до отправки интереса.

HISTORY entries дополнительно содержат signed 64-bit raw record-time lower bound. Это атрибут ключа, а не дополнительная symbol/registration identity. Это не Unix milliseconds, MESSAGE/FIELD tape time или выведенный business timestamp. Представление соответствует явно валидированному [indexed layout](../schema/INDEXED-SCHEMAS.ru.md): high/low words либо одно logical signed64 value с сохранением всех битов. Native state хранит raw bound; collector comparison/inclusive-range и wire packing требуют выбранных compatibility evidence. Этот профиль требует явных indexed/time metadata для HISTORY и отклоняет отсутствующий layout.

Local schema неизменяема в течение run. Для replacement локальной конфигурации нужен новый context/run; live local-schema replacement не входит в дизайн. Изменения peer descriptions/bindings — отдельная protocol operation.

## Атомарные команды и barriers

| Команда | Успешный локальный эффект |
| --- | --- |
| Add | Upsert запрошенных ключей; identical entries — no-op. Новый HISTORY bound заменяет атрибут ключа и создаёт новую membership activation. |
| Remove | Удаление присутствующих ключей; отсутствующие — no-op. |
| Replace | Публикация ровно валидированной input map; пустой input очищает её. Противоречивые дубликаты в одной команде отклоняются; одинаковые схлопываются. |
| Clear | Публикация пустой map. |
| Close registration | Необратимое retirement memberships и запрос quiescence. Полнота обычной command/data queue не блокирует этот control path. |

Целиком валидировать candidate, зарезервировать peak resources и атомарно опубликовать. Неверные identities/selectors/layouts, arithmetic overflow, allocation/resource/work failure сохраняют прежние map, revision и membership activations. Temporary reservations освобождаются; уже списанная работа текущего turn не возвращается. Возвращается typed cause; rejected local command сам по себе не завершает исправное соединение.

Успешное реальное изменение увеличивает registration revision ровно один раз; no-op и failure её не меняют. Неизменённые memberships сохраняют activation IDs. Новые/повторно добавленные keys и изменённые HISTORY bounds получают новые неповторяемые IDs. Replacement bound инвалидирует старое subscription view и требует выбранного fresh/incomplete transition коллектора; точная peer snapshot semantics ещё требует evidence.

Submission acceptance означает только владение bounded command copy верхним слоем. Это отличается от owner application и wire emission. Принятая команда получает applied, rejected или cancelled-closing с identity и соответствующей revision; молча она не теряется. Full/closed command port немедленно отказывает в admission. Direct owner commands вне callbacks могут синхронно вернуть результат применения; callback commands ожидают возврата. Completed result state не удерживает endpoint, socket operations или callable.

Completion remove/clear/replace — owner application barrier: после неё новый callback не начинает доставку для retired memberships. Уже исполняемый user code может закончиться до barrier. Old queued items имеют eligibility tokens и фильтруются/учитываются как withdrawn by revision; повторный add того же ключа не делает их eligible. Команда, поставленная из callback, не отзывает записи уже выданного borrowed batch. Nested dispatch ради barrier не запускается.

## Admission, доставка и callable lifetime

Live admission сначала валидирует и строит весь текущий frame, затем публикует records в collectors/user-visible queues. Ошибочный frame не публикует даже completed decoder prefix. Ранее принятые frames учитываются отдельно; последующая terminal error не отменяет уже выполненные callbacks. Partial socket reads — не полные frames и не успешный EOF. Control-frame changes также публикуют только полный корректный candidate. Прежние [decoder completed-prefix semantics](../protocol/STREAM-DATA.ru.md) и offline readers/replay неизменны.

Queued delivery владеет immutable values, exact symbol/flags и provenance и содержит connection epoch, binding snapshot и membership activation. Sender IDs не задают delivery identity. Borrowed views действуют только во время callback. Retained explicitly owning delivery сохраняет quota reservation до последнего lease; самостоятельные deep copies caller принадлежат ему и не входят в library resident accounting. Копирование schema handle или callback span не создаёт совместимую provenance.

Dispatcher не вызывает user code под mutation/container lock или с borrowed iterator в удаляемое registration storage. Исполняемый callable, arguments и backing state защищены до возврата/исключения. Self-remove, request-stop и отпускание последнего public registration/endpoint handle разрешены; handles инициируют nonblocking retirement/abort, а run/execution host сохраняют progress ownership до quiescence. Release handle не означает синхронный close completion.

Callback mutation применяется после возврата; recursive delivery запрещена. Exception завершает затронутую registration с наблюдаемым callback-failure cause и pending counts; side effects не повторяются. Остальные registrations могут продолжать при целостном shared state. Exception/diagnostic text ограничен и sanitized; секреты не копируются в public errors.

Callbacks должны возвращаться быстро. Удерживающий queue credits/owning leases consumer изолируется quotas; произвольно блокирующий user code останавливает serialized domain. Runtime не может принудительно прервать такой C++ код или объявить quiescence, пока он исполняется.

## Epochs, bindings и восстановление

Отдельные monotonic identities задают connection epoch, immutable binding publication, registration lifetime, registration revision и membership activation. Арифметика не переполняется с повторным использованием identity: операция отклоняется либо owning run завершается до исчерпания. Одинаковые shapes не означают одинаковые contexts.

Desired subscriptions находятся вне connection-local parser/composer/negotiation/output state. Новое соединение создаёт fresh directional state, отбрасывает старые raw input/unsent output и проходит authentication/negotiation до отправки актуального desired snapshot. Offline removals нельзя воскресить stale queued deltas. Локально applied/emitted subscription не является peer acknowledgement; selected binary profile не придумывает per-key acknowledgement без protocol evidence.

Reset — owner barrier. Дождаться возврата исполняемого callback, отбросить old-epoch pending deliveries с видимым gap/discard accounting и сбросить collector freshness/completeness до новых данных. Reset notice предшествует new-epoch data callback. Old caller-held owning values остаются валидными значениями со старой epoch, но не текущим collector state. Число потерянных upstream records может быть неизвестно; lost count, exactly-once и resumable history не придумываются.

В одном соединении новый peer binding snapshot не меняет уже owning normalized records. Они могут оставаться queued, если immutable local context и membership совместимы; одна publication binding не отбрасывает их молча. Queued record не переинтерпретируется по новой sender table. Peer-ID reuse, description replacement и readiness/auth order требуют выбранных production evidence до protocol implementation.

## Конечные ресурсы и работа

Limits задаются явно в immutable configuration с единицами и scope. Универсальные числовые defaults здесь не выбираются. Zero означает нулевую ёмкость, а не unlimited. Арифметика и обязательные соотношения валидируются до activation/I/O. При недостатке initial schema/reserved control storage activation невозможна.

| Измерение | Обязательный учёт |
| --- | --- |
| Desired keys/selectors | Distinct resident entries и вся owned UTF-16 capacity; input duplicates/work учитываются даже при collapse |
| Queues/deliveries | Items, owned payload capacity и leases, включая executing и externally retained library allocations |
| Schema/binding/staging | Old published, candidate и temporary owners вместе на пике; shared allocations считаются один раз в своём scope |
| Retired generations | Pending operations, outstanding leases, control/completion storage до реального освобождения |
| Per-command/turn work | Visited keys, examined/copied UTF-16 units, commands, frames и callbacks; codec-specific counters сохраняют смысл |
| Diagnostics/controls | Конечные causes/text/counts и reserved reset/terminal storage без неограниченного error list |

Учитывать запрошенную library containers backing capacity, а не только заполненное содержимое. Item/key/generation limits ограничивают metadata cardinality. Это logical bounds, а не гарантия allocator overhead/process RSS. Accounting counters не переполняются; только cumulative diagnostic counters могут saturate с явным overflow flag.

Cumulative file/frame/work ceiling tape reader не переносится на lifetime продолжающегося feed. Resident limits действуют до release owners. Per-operation/turn work сбрасывается только на документированной границе; failures/retries внутри turn не возвращают работу. Bounded frame decode может завершиться в собственном конечном лимите либо отказать до admission; делать его resumable контракт не требует. Scheduler проверяет conservative bounded cost до начала следующего frame и yield между ограниченными операциями. Optional lifetime quotas — отдельные явные настройки.

Registration резервирует свои data/control credits до activation. Сумма reservations не превышает context capacity; held leases сохраняют reservation и после retirement. Slow client не расходует зарезервированные credits другого active client. Исчерпание shared parser/cache остаётся явным shared-scope failure; isolation не обещает бесконечный progress при исчерпанном общем resource pool.

STREAM overflow не выполняет silent conflation/drop с продолжением как lossless. Consumer завершается с видимыми gap/counts; healthy consumers сохраняют reserved capacity. HISTORY overflow/interruption инвалидирует completeness до следующей доставки. TICKER conflation разрешён только measured contract. Ожидание consumer space с блокировкой shared owner не является overflow policy.

## Резервная доставка control

При создании registration резервируются как минимум один pending-reset slot и один terminal slot, а также bounded completion state принятых команд. Retirement/abort имеют allocation-free idempotent control path, не блокируемый обычной queue saturation. Ordinary data enqueue не расходует этот резерв.

Reset/gap notices можно coalesce только в документированный interval от первой затронутой epoch до новейшей, со skipped-reset count, bounded cause fields и unknown-loss marker при необходимости. Они не исчезают и не превращаются в одну uninterrupted epoch. До new-epoch data клиент может наблюдать pending interval. Terminal notice сохраняет cause/integrity/counts и при полной data queue; для query terminal state не требуется освобождать её. Неограниченный callback/log на каждое промежуточное событие не требуется.

## Жизненный цикл и достоверное завершение

Это логические состояния, а не утверждённые C++ enum spellings.

| Состояние | Допустимый progress / выход |
| --- | --- |
| Configured | Принимать desired interests; start с validated limits/schema; без подразумеваемого I/O |
| Starting / negotiating | Bounded pending operations и desired changes; не отправлять интересы до selected readiness/auth |
| Active | Принимать valid frames, доставлять eligible memberships в source order |
| Resetting / retry-wait | Сохранять desired state, публиковать reset до новых данных; retry только по явной выбранной policy |
| Quiescing | Прекратить new intake/ordinary commands; drain определённого admitted prefix либо abort; разрешить idempotent/escalating stop |
| Terminal | Immutable outcome после quiescence; новые library callbacks/operations не начинаются; run не открывается заново |

Graceful cutoff — применение stop command в owner order. Прекратить new read admission/ordinary mutations, явно отменить not-yet-applied commands и drain eligible data, принятые до cutoff. Неполный input frame не admitted и не successful EOF; его discarded bytes считаются отдельно. Callback может закончить текущий borrowed batch; request-abort/handle retirement доступны во время drain. Дополнительные peer unsubscribe/close messages не придумываются.

Начатая outgoing work должна завершиться по configured stop policy либо abort с явными partial-write/cause result. Required sink flush и runtime/TLS closure принадлежат composing adapter/tool; их ошибка не превращается в успешное completion. До activation graceful stop отменяет pending connect/negotiation, не ожидая market data.

Для registration/run есть ровно один terminal outcome. Drained означает завершение заданного still-eligible admitted prefix и обязательных flush/closure, а не gap-free upstream lifetime. Возвращаются прежние gap/unknown-loss state, withdrawn/discarded counts, failed registrations и secondary cleanup errors. Forced drain discard/escalation — aborted или failed, не clean drained. User abort отличается от успешного drain.

Первая наблюдённая non-cancellation failure остаётся primary cause в serialized owner order; последующий stop не заменяет её. Expected cancellation из-за stop не является новой failure. Без прежней ошибки stop timeout — failure; поздние close/flush faults сохраняются как bounded secondary causes. Пустая очередь не стирает записанную failure.

Quiescence означает отсутствие исполняемого library-owned callback, pending operation с доступом к retired state и будущей запускаемой delivery. Caller-held immutable deliveries и detached completed result values могут пережить её; они не удерживают callable/live operation. Terminal completion публикуется после этой границы. Deadline усиливает owned cancellation/discard, но не заставляет произвольный callback вернуться. При прекращённом pumping или нарушенных execution-host/callback preconditions completion не придумывается.

## Предпосылки совместимости

Действующий [finite binary message contract](../protocol/BINARY-MESSAGES.ru.md) отклоняет subscription messages. [P03/P05 captures](../protocol/CAPTURES.ru.md) — ограниченные subscription/remapping evidence; P05 подавляет nondeterministic heartbeat/protocol metadata и не доказывает полный handshake. [Schema-exchange source study, EN](../protocol/SCHEMA-EXCHANGE.md) и [offline replay contract](../protocol/OFFLINE-REPLAY.ru.md) не реализуют live lifecycle.

До native state/wire/collectors получить selected peer evidence для add/remove families, HISTORY raw bound, exact/empty/reserved selectors, descriptor/readiness/auth/heartbeat и membership/initial/latest/snapshot/transaction/removal cases. Разделять source inspection, production observations и native tests. Будущий collector не считается supported из-за наличия generic decoder.

Дизайн фиксирует local ownership, mutation, work и epoch boundaries. Peer-dependent protocol/collector details, literal escapes, broader field conversions, PART/compression requirements и exact authentication options остаются blocking evidence rows для своей реализации. Отдельное runtime decision должно установить progress/cancellation/trust behavior на каждой supported platform. Дополнительные framework, persistent journal, business-event catalog и спекулятивные recovery mechanisms здесь не добавляются.
