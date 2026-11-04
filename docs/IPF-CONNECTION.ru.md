# Live-подключения к профилям инструментов

[English version](IPF-CONNECTION.md) является нормативной.

Включите ORG_TTLDTOR_QDCXX_BUILD_IPF_LIVE=ON с явными зависимостями IPF transfer и TLS;
подключайте qdcxx_ipf_live. Используется один фоновый worker. Независимая qdcxx_ipf
с collector/value/parser доступна при выключенных transport, transfer и live.

Создавайте InstrumentProfileConnectionRuntime на main до потоков приложения в owned
режиме либо передавайте явный pre-thread RuntimeBootstrap. Его moved-to владелец
должен пережить join/уничтожение runtime. Скрытый bootstrap ApiRuntime не заимствуется.
Поддерживается ровно один lifetime-слот connection на runtime без повторного использования.
Несколько runtime могут явно разделять bootstrap только при достаточном borrow capacity
и правдивом суммарном учёте TLS funding/history того же образа.

Проверяемый пример C++ дан в нормативной английской версии. Создайте runtime и shared
collector, зарегистрируйте owning update listener, вызовите createConnection и однократный
start, затем awaitCompleted с абсолютным steady-clock deadline. Приложению не нужен transport polling. После ожидания вызовите
request-only connection.close и creator-thread runtime.closeAndAwaitTermination.
Пример ожидает первый снимок до30секунд. Timeout/caller cancellation завершают только wait; connection продолжает работу до close. Creator join запрашивает shutdown и отделён от готовности снимка.

NOT_CONNECTED → CONNECTING → CONNECTED → COMPLETED → CLOSED отражает первоначальную фазу;
retry её не сбрасывает. initialSnapshotComplete остаётся sticky при следующих активных
или ошибочных попытках. Наблюдайте отдельно activeAttempt, lastAttempt, diagnostic, retired.
HTTP framing, целостность gzip/ZIP, IPF EOF, COMPLETE и native retirement различаются.
Marker перед ошибкой trailer/close сохраняет owning collector revisions и позднюю ошибку.

Запрос передаёт X-Live: yes с прежним Accept-Encoding: identity. Только точное значение
provided включает live. Без live чистый EOF завершает замену снимка; live EOF публикует
pending без создания COMPLETE. Flush, COMPLETE, размер batch и чистый EOF обрабатываются
по порядку. Неопубликованный pending после ошибки переносится в следующую попытку, как
в закреплённом QD. COMPLETE удаляет старые поколения; поколение одной попытки сохраняется
после marker.

Last-Modified фиксируется после успешного native retirement. Равная ненулевая дата
пропускает body без изменения capability/поколений. Fallback HTTP использует If-Modified-Since
и условный304; незапрошенный304 ошибочен. Update delay по умолчанию60000ms; retry начинается
с1000ms и после ошибок удваивается до текущего update delay, не сбрасываясь после успеха.
Изменение периода не сокращает уже начатое ожидание. [update=period] поддерживает QD
дни/часы/минуты/секунды:1m30s,0.5s,PT1S. Принимается полный неотрицательный signed64
диапазон миллисекунд, включая QD inf; это числовая задержка при независимых конечных
lifetime/attempt/work budgets. Ожидание ограничивается lifetime deadline без переполнения clock.

Connection/listener методы допускают конкурентных producers; обычное конкурентное
уничтожение самого facade требует синхронизации. State/auth callbacks выполняются
на worker вне owner/native locks и могут закрывать/удалять connection, снимать listeners,
менять collector. Для не запущенного подключения CLOSED dispatch происходит на creator
при shutdown. Выбранный callback может завершиться после удаления; исключения учитываются.
Authorization callbacks и освобождение captures также происходят вне locks на worker.
Произвольная работа callback должна быть конечной. Runtime нельзя уничтожать/join-ить
из worker/callback: creator владеет join и SDK cleanup. После join handles могут жить как
закрытые detached observations.

Ограничены pending profiles/units, поколения, попытки, lifetime, listeners, backing и
накопленная copy/control work. Native/TLS/HTTP/decoder grants не обновляются при retry/redirect;
shared TLS history не сбрасывается. Model/revision/delivery budgets collector независимы.
STL backing admission задаёт консервативный requested storage, а не allocator overhead/RSS;
произвольные captures/allocations приложения остаются его ответственностью.

См. [контракт](../spec/ipf/CONNECTION.ru.md), [collector](IPF-COLLECTOR.ru.md),
[source reader](../spec/ipf/URL-READER.ru.md), [решение о владении](decisions/ADR-0009-live-ipf-connection-ownership.ru.md).

Уточнение потоков: state/auth callbacks выполняются на worker своего connection. Collector notifications следуют его независимому outside-lock dispatcher и при concurrent/nested update могут выполняться на другом текущем updating thread.

До allocation pending copy резервирует общий requested backing: возможный empty-map sentinel, custom node/key/value objects, capacities всех standard/custom strings с terminators и консервативной library alignment-request добавкой. Для поддержанных tree headers заложены четыре указателя; actual allocation-counting regressions связывают оценку с профилем MSVC/GCC. Inline string capacity/padding консервативно учтены повторно. Fixed vector objects и прежний logical pending-unit reserve входят в resident admission один раз; остаток resident backing вместе с этим резервом финансирует одновременно живущие копии. Dense custom data может исчерпать backing раньше logical limits. Flush освобождает copy bytes после уничтожения values; failed copy откатывает storage reserve без возврата work. Это новый connection backing, без изменения logical-budget контрактов обычного reader/collector.

## Ожидание первого снимка

awaitCompleted(deadline, stopToken) возвращает expected IpfCompletionWaitStatus.
При каждой защищённой проверке приоритет: COMPLETED, CLOSED, CANCELLED, TIMED_OUT.
Первое completion сохраняется при reconnect, поздней ошибке и close. Метод не ожидает новое
поколение и не подтверждает HTTP framing, gzip/ZIP integrity, parser EOF или native retirement.
Валидный COMPLETE может завершить wait при ещё открытом live response; чистый fallback EOF
также завершает первый снимок. Owning revisions остаются действительными после позднего removal.

Передавайте абсолютный std::chrono::steady_clock deadline. Истёкший deadline даёт немедленную
проверку; minimum/maximum clock поддержаны. Необязательный std::stop_token позволяет существующему
потоку приложения вызвать stop_source.request_stop() без доступа к facade/SDK. Отмена завершает
только этот wait, не закрывает connection и не очищает его first error, metadata, generation/work
history. Уже наблюдаемое completion имеет приоритет над заранее запрошенной отменой.

Waiter удерживает stable control независимо от runtime/facade drain. Синхронизируйте уничтожение
facade с входом в метод; callback close/deletion после admission не инвалидирует retained wait state.
Не удерживайте application locks, необходимые worker callbacks. Worker callback может немедленно
прочитать готовый outcome; would-block self-wait возвращает INVALID_STATE, не останавливая своего
producer. Creator runtime join остаётся отдельным и никогда не выполняется awaitCompleted.

completionWaiters (по умолчанию16) ограничивает одновременно живущие calls, включая retirement stop callback.
completionWaitTurns (по умолчанию1024) ограничивает pending predicate/wait turns каждого call; spurious/посторонние
notifications расходуют turns без обновления grants. Ноль разрешает immediate outcomes, но
отказывает pending wait. Capacity/work refusal возвращается только caller и не закрывает connection
или переписывает его ошибку. Resident preflight консервативно включает
completionWaiters*sizeof(actual fixed wait frame); frame имеет automatic storage без listener-node
allocation/скрытой очереди. Caller thread stacks и allocator/RSS overhead остаются вне owner backing.
