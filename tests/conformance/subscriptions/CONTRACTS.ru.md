# Матрица контрактов подписок и сценарии квалификации

[English version](CONTRACTS.md). Английская версия нормативная.

Документ квалифицирует [native design contract](../../../spec/subscriptions/CONTRACTS.ru.md). Это конечный реестр support/pending/excluded и таблица ожидаемых локальных traces. [Производственные Java-захваты](../../fixtures/subscriptions/CAPTURES.ru.md) подтверждают выбранный профиль; нативные подписки/коллекторы и harness локальных traces ещё не реализованы. Прежние [binary coverage](../protocol/real-tape/COVERAGE.ru.md) и [validation](../protocol/real-tape/VALIDATION.ru.md) сохраняют исходные профили.

## Матрица объёма

SUPPORTED foundation обозначает только существующую lower-layer capability. Новые subscription/runtime rows остаются pending implementation; design fixed закрывает локальное решение, а не implementation/upstream compatibility gate. Обязательные pending rows нельзя переименовать в excluded ради заявления полной поддержки.

| Область | Состояние | Смысл / prerequisite |
| --- | --- | --- |
| Schema/record provenance и indexed layout | SUPPORTED foundation | Только прежние immutable identity и явная проверка time layout. |
| Generic binary data families/flags | SUPPORTED foundation | Прежнее selected wire decoding; строка не означает collector delivery. |
| Desired mutations и membership barriers | PENDING implementation; дизайн зафиксирован | Реализовать local atomic/upsert/revision contract после compatibility qualification. |
| Callable/lease/host lifetime | PENDING implementation; дизайн зафиксирован | Реализовать guards, nonblocking handle release и quiescence в верхних слоях. |
| Resources, command/control admission и live-frame policy | PENDING implementation; дизайн зафиксирован | Реализовать resident/turn limits, reserved credits/control storage и whole-frame admission. |
| Desired/sent state и epoch ownership | PENDING implementation; дизайн зафиксирован | Fresh connection state/reset barriers зафиксированы; наблюдения выбранного peer/auth приведены в профиле захватов, нативная реализация ещё нужна. |
| Шесть subscription messages и readiness/auth/heartbeat | OBSERVED Java profile; PENDING native implementation | Захвачены шесть сообщений, requested layouts, protocol deltas, 16 масок heartbeat, клиентская/серверная auth и capability readiness. Только modern plain ordinary read-side профиль; ограничения PART/compression/converters/legacy явны в профиле захватов. |
| STREAM collector | OBSERVED Java profile; PENDING native implementation | Захвачены every-event/two-client/overlap traces; нативные cutover, ownership и resource tests ещё обязательны. |
| TICKER collector | OBSERVED Java profile; PENDING native implementation | Захвачены initial/latest/re-add и явный retention; универсальный cached re-add snapshot не обещается. |
| HISTORY collector и raw lower bound | OBSERVED Java profile; PENDING native implementation | Захвачены raw bound/snapshot/transaction/removal/aggregation traces; сырой QDAgent output отличается от business-event model. Нативные completeness/reset tests ещё обязательны. |
| Wildcard family/peer combinations | OBSERVED Java profile; PENDING native implementation | Fanout/overlap доказан для включённого STREAM, но не TICKER/HISTORY. Нативный All ограничен доказанным профилем; Java при этом может принимать литеральный star в aggregate subscriptions. |
| TCP/executor/TLS/configured authentication | PENDING runtime decision и implementation | Выбрать один supported backend/profile, trust/cancellation policy и OFF dependencies до adapter code. |
| Java object/business conversion и полный event catalog | EXCLUDED | Сохранить прежние opaque/generic data boundaries; не заявлять все business-event API. |
| Server/publisher/RMI/multi-endpoint routing | EXCLUDED | Это отдельные возможности, а не prerequisites выбранного read-side client. |
| Exactly-once journals и callback-side-effect retry | EXCLUDED | Нет подтверждённого end-to-end acknowledgement/resume contract для этих механизмов. |

## Каталог локальных traces

Trace IDs — стабильные имена сценариев. Числа малых примеров — logical test inputs, а не API defaults, измерения или process-memory claims. Native/Java tests должны выполнить применимые traces на final source, отделяя intentional native policy от measured upstream behavior.

| Trace | Граница | Условие / действие | Обязательное наблюдение |
| --- | --- | --- | --- |
| SC01 | Идемпотентность | Пустая map, revision 7: add A; повтор identical add A; remove отсутствующего B. | Revisions 8, 8, 8; одна membership activation, remove отсутствующего ключа без side effect. |
| SC02 | Атомарный replacement | Опубликован A; replace на B и invalid context/selector либо противоречивый duplicate. | Отклонить команду целиком; A/revision/activation неизменны, temporary owners освобождены, attempted work списана. |
| SC03 | Пиковое выделение | Условные charged payload units: old 4, candidate 6, limit 8; остальные charges неотрицательны. | Отказ при пике не менее 10, хотя final 6 помещается; old 4 сохраняется. Нельзя освобождать old до validation. |
| SC04 | Self-remove из callback | Callback получил borrowed batch, поставил self-remove и вернулся; ожидает ещё matching batch. | Callable/batch живы до возврата. Затем remove; последующего callback для retired activation нет. Текущий batch не отзывается задним числом. |
| SC05 | Remove/re-add ABA | У STREAM A есть queued activation 10; remove A завершён; add A создаёт activation 11. | Элемент activation 10 withdrawn, а не доставлен как activation 11; неизменённые другие keys сохраняют eligibility. |
| SC06 | Ошибочный текущий frame | Прежний frame admitted; следующий frame декодирует record 0 и ошибается на record 1. | Не публиковать ошибочный frame; прежние admission/callback effects сохраняются. Decoder prefix на этом пути только diagnostic. |
| SC07 | Reset соединения | В epoch 17 ожидает A и удерживается owning lease; epoch 18 использует прежний sender ID для B. | Reset/gap до epoch 18 data; old pending data отброшены с учётом, old lease остаётся epoch 17 value и удерживает quota. |
| SC08 | Обновление binding | Один connection/context: normalized A из binding 3 queued; binding 4 меняет sender mapping. | Owning A остаётся A при valid membership; не переинтерпретировать через binding 4 и не отбрасывать лишь из-за смены binding. |
| SC09 | Медленный consumer | A/C быстро освобождают данные; B удерживает owning leases до своего data-credit limit. | B overflow/gap/terminal видимы через reserved controls; A/C сохраняют свои credits. Leases B учитываются до release. |
| SC10 | Control request при saturation | Ordinary command/data queues полны; отпустить последний public handle или request abort. | Allocation-free control request принят один раз; owner progress безопасно завершает работу. Completion не объявляется до quiescence. |
| SC11 | Приоритет failure | Наблюдена non-cancellation parse failure; затем graceful stop; затем close failure. | Parse cause остаётся primary, close cause — secondary; successful drain не заявлен; terminal ровно один после quiescence. |
| SC12 | Graceful cutoff | Stop применяется при admitted data и 10 bytes неполного input frame. | Drain eligible admitted prefix; учесть 10 unadmitted discarded bytes; не придумывать successful EOF/frame. Required sink-flush failure даёт failure. |
| SC13 | Непрерываемый callback | Callback ещё не вернулся; supervising clock достиг deadline. | Запросить escalation, но не заявлять quiescence/terminal completion до возврата. Bounded test harness отпускает barrier и убирает процессы. |
| SC14 | Продолжающаяся работа | Суммарная работа многих bounded turns превышает quota одного turn. | Продолжать при соблюдении operation/resident limits; без implicit finite-file lifetime exhaustion. Same-turn failure/retry не возвращает работу. |
| SC15 | Изоляция contexts | Независимые схемы имеют одинаковые local IDs/shapes; ключ run A передан run B. | Отклонить несовместимый context без mutation/emission; identity копии схемы совпадает с исходной. |
| SC16 | Изменение HISTORY attribute | Add прежнего HISTORY key с другим raw lower bound; затем повторить его. | Одна смена revision/activation, затем no-op; без conversion в milliseconds. Полнота old view инвалидирована; точная peer snapshot trace pending. |
| SC17 | Coalescing reset | Несколько resets до наблюдения consumer pending control interval. | Сохранить earliest/newest epoch и skipped-reset count; finite storage, при необходимости unknown-loss marker; данные не обходят нераскрытый reset. |
| SC18 | Accepted и applied | Port принял команду; close применился раньше неё; peer acknowledgement отсутствует. | Completion — cancelled-closing, не applied/emitted/peer-accepted; результат читается без удержания endpoint operations. |

## Воспроизводимая приёмочная нагрузка

Fixture profile `SUBSCRIPTIONS_ACCEPTANCE_V1` фиксирует исходную нагрузку будущей квалификации. Это configurable test input, а не universal production defaults, throughput SLA или доказательство уже работающей реализации. Каждый run записывает actual limits, source/schema/fixture hashes, compiler/build profile, seed и simulated/real clocks/peers.

| Параметр | Fixture value / единица |
| --- | --- |
| Synthetic data | 1,000,000 records; deterministic seed 0x51444358; exact symbols S000…S063 |
| Schemas | Явные ordinary Quote/Profile и synthetic indexed record с high/low signed32 time fields и одним signed32 payload; time/index semantics не выводятся из имён |
| Clients | 3: быстрый A, удерживающий credits B, меняющий memberships C; не более 3 reserved registrations |
| Desired capacity | 256 keys/client, 768 aggregate; symbol capacity 64 UTF-16 units/selector |
| Per-command/turn work | 65,536 key visits; 1,048,576 examined/copied UTF-16 units; 16 commands, 4 frames, 32 callbacks/turn; codec-specific bounds записываются отдельно |
| Input/staging | 1,048,576 bytes/frame, 256 records/frame, 64 fields/record; 8,388,608 aggregate charged resident bytes; одновременное достижение всех максимумов не обязано быть допустимым |
| Data credits | 32 items и 65,536 bytes/client; 96 items и 196,608 bytes aggregate, включая retained library leases |
| Control credits | 2 slots/client, 6 aggregate; отдельная bounded completion capacity 16/client |
| Retired metadata | До 16 outstanding binding/epoch metadata generations/context; held leases сохраняют нужные reservations |
| Churn/recovery | 100 scripted resets с sender-ID reassignment; C меняется 256 раз; logical interests/leases явно освобождаются между reusable registrations |
| Virtual timing | 1,024 records/virtual second; heartbeat 1,000 ms; inactivity 3,000 ms; retry delays 10/20/40/80 ms с cap 80 ms, до 8 attempts/episode; только fixture parameters |

Выполнить отдельные STREAM, TICKER и HISTORY episodes со свежим состоянием и 1,000,000 input records в каждом. Fixture client — одна registration; все три registrations используют family своего episode. STREAM чередует Quote/Profile/indexed records, TICKER — ordinary Quote/Profile, HISTORY — явно indexed synthetic record с raw lower bound zero. Для ordinal i от нуля record выбирается по floor(i / 64) modulo числу допустимых records, symbol — по (i + (seed modulo 64)) modulo 64. В indexed record high time word равен zero, low time word — floor(i / 64) modulo 32, payload — (i + seed) modulo 1024. Повторение bounded raw-time keys проверяет continuing updates без неограниченного history cache; business time conversion не подразумевается.

A/B запрашивают все допустимые keys. C чередует первые/последние 32 symbols; replacement count увеличивается при увеличении floor((i + 1) * 256 / 1,000,000). Reset count аналогично использует factor 100. Применять scripted controls после этого record и до следующего; при совпадении membership change предшествует reset, чтобы resubscribe использовал новый desired set. Outstanding C leases освобождаются до retirement/reuse registration. Конкретные ordinary-field values и точные framing/flags записываются в captured fixture manifest, а не получаются из придуманного protocol encoder.

Использовать synthetic values/credentials и controlled Java/scripted peer. Codec/capability snapshots и exact wire expectations получаются в compatibility qualification, а не придумываются из этой нагрузки. Real public endpoint — дополнительная квалификация с отдельными access/data permissions; запись контрактов к нему не подключается.

A/C продолжают в reserved credits, когда B достигает queue/lease bound. Shared-state exhaustion проверяется отдельно как видимый shared-scope failure. Повторить zero/exact/exceeded limits и failed candidate peaks; длинный traffic не останавливается лишь из-за превышения cumulative input над per-turn quota. Сначала fake-clock control/state cases; real socket/TLS cancellation/closure проверяются выбранным adapter.

## Доказательства для qualification реализации

Записывать production parser/composer/collector inputs/outputs на locked revision, options/auth/runtime settings, native actual results и точный comparison scope. Покрыть empty/duplicate/removal/membership cases, TICKER retained/absent snapshots, HISTORY lower-bound и transaction/snapshot/remove/empty/snip, wildcard restrictions, raw sender reassignment, credential/certificate failures, stop/flush и resource recovery.

Когда код появится, обязательны independent ownership/state review и affected Windows/Linux checks. Synthetic captures не содержат private original tape data, secrets или application paths. Static document/link checks доказывают только consistency спецификации. Doxygen сейчас читает headers, поэтому standalone Markdown specifications не считаются новой собранной API reference page.
