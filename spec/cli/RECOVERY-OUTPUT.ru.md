# Вывод подписок с восстановлением, версия 1

[English](RECOVERY-OUTPUT.md) — нормативная версия.

Явно выбранный режим recovery начинает stdout строкой `qdcxx-subscriptions<TAB>1`. Каждый режим
сохраняет все точные generic-поля. Явные `--output quote` и `--output profile` добавляют существующую
подготовленную проекцию события; тип не угадывается по имени записи. Этот формат относится только
к выводу recovery. Прежний режим подписок сохраняет существующий формат вывода.

## Кодирование и публикация

- В потоке C++ строка заканчивается LF. Нативный текстовый stdout может преобразовать его в CRLF;
  записи проверки должны сохранять фактические байты своей платформы. Между ячейками ровно один TAB,
  без кавычек, пробельного выравнивания и BOM. Конечная пустая ячейка сохраняется перед концом строки.
- Целые числа записываются точно десятичными ASCII-цифрами; минус появляется только у отрицательных
  знаковых значений. Нет плюса, разделителей разрядов, hex-префикса или преобразования в floating point.
  Enum сохраняют свои числовые коды. Булевы значения — `0`/`1`. Locale, boolalpha, base и width потока
  не меняют содержимое ячеек.
- `null` означает отсутствие optional. Присутствующий пустой текст/массив — пустая ячейка. Числовой
  ноль — `0`. Отсутствующие метаданные не заменяются нулями.
- `UTF16HEX` — четыре заглавные hex-цифры на каждую исходную кодовую единицу UTF-16, включая NUL
  и непарные суррогаты. Нормализация, замена суррогатов и декодирование символов отсутствуют.
  `BYTEHEX` — две заглавные hex-цифры на каждый непрозрачный байт. У пустого значения нет цифр.
- Raw decimal/long/time сохраняют знаковые целочисленные биты. В частности, decimal raw=0 выводится
  как raw=0, без замены на числовой ноль, NaN, текст или значение floating point.
- Потоки синхронные. Единица публикации — весь callback batch либо один вызов функции вывода
  свидетельства. До успешного возврата выполняются flush и проверка состояния потока. Заблокированную
  запись нельзя прервать лимитом длительности запуска. Исключение получает статический текст;
  произвольный текст исключения потока наружу не переносится.
- При сбое могут остаться часть строки, часть единицы публикации и более ранние внешние байты.
  Нет отката, повтора, транзакционного файла или подтверждения чтения получателем. Исключение callback
  остаётся ошибкой callback очереди; его побочные эффекты повторно не воспроизводятся.

Stdout содержит только строки выбора и данных ниже. Stderr содержит прямые свидетельства из публичных
типов. Их функции не опрашивают владельца, не подают команды, не продвигают восстановление, не вычисляют
полноту и не выбирают код завершения. Сборки с TLS дополнительно предоставляют числовую строку
`tls-diagnostic`. Ошибки до активации и финальные сведения приложения о cutoff/target/signal/exit
имеют отдельные контракты исполнения вне этой спецификации строк.

## Строки stdout

В таблице перечислены ячейки после имени строки, строго в указанном порядке. Английские имена
обозначают публичные поля или уже проверенные настройки приложения; это не дополнительные текстовые ячейки.

| Строка | Ячейки после имени |
| --- | --- |
| `qdcxx-subscriptions` | `1` |
| `selection` | family (`stream`, `ticker`, `history`), selectedLocalId, output (`generic`, `quote`, `profile`) |
| `record` | ordinal, family, selectedLocalId, epochOrdinal, revision, admissionRevision, symbolUTF16HEX, rawTime-or-null, producedEventFlags, fieldCount |
| `source` | ordinal, present, senderId, cipher, originalFlags, bindingGeneration, removalTime, fieldRawSeconds, fieldRawSequence, fieldSourceOrdinal |
| `trigger` | ordinal, present, senderId, cipher, originalFlags, bindingGeneration, rawTime, fieldRawSeconds, fieldRawSequence, fieldSourceOrdinal |
| `field` | ordinal, zeroBasedLocalFieldIndex, exactFullSerialType, rawSigned64, unitsUTF16HEX-or-null, bytesBYTEHEX-or-null |
| `quote` | ordinal, symbolUTF16HEX, bidTimeMs, bidExchangeCodeUnit, bidPriceRaw, bidSizeRaw, askTimeMs, askExchangeCodeUnit, askPriceRaw, askSizeRaw, timeMs, sequence, timeNanoPart, eventTimeOrigin, eventTimeMs-or-null |
| `profile` | ordinal, symbolUTF16HEX, betaRaw, earningsPerShareRaw, dividendFrequencyRaw, exDividendAmountRaw, exDividendDayId, high52WeekPriceRaw, low52WeekPriceRaw, sharesRaw, freeFloatRaw, highLimitPriceRaw, lowLimitPriceRaw, haltStartTimeMs, haltEndTimeMs, businessFlags, descriptionUTF16HEX-or-null, statusReasonUTF16HEX-or-null, eventTimeOrigin, eventTimeMs-or-null |

Первые две строки записываются один раз после подготовки, до активации источника. Для каждой записи
порядок следующий: `record`, `source`, `trigger`, все `field` в локальном порядке полей, затем
необязательная выбранная строка `quote`/`profile`. Пустой batch не создаёт строк данных. После успешной
публикации sink возвращает CONTINUE. Он не знает целевого числа записей и не обрезает callback,
пересёкший target: runtime запрашивает cutoff после возврата всего callback.

`ordinal` начинается с единицы и увеличивается для каждой записи этого запуска. `completedRecords()`
изменяется только после успешного flush всего callback batch. Это счётчик публикации приложения,
не последующего ACK очереди, приёма native write или чтения получателем. При поздней ошибке записи,
проекции или flush прежние байты могут уже существовать, хотя весь callback завершился ошибкой.

Контекст каждой записи проверяется через `matches` с известным RecordContext выбранной схемы.
Печатается заранее проверенный локальный ID; он не извлекается из токена и не угадывается по sender ID.
Typed-поля сохраняют полный serial type, scalar и optional payload, включая поля, не выбранные
подготовленным mapper. Единственный legacy fallback — фактически полученный полный compact-only view
для выбранной схемы, все поля которой имеют тип COMPACT_INT; полный тип таких полей равен 8.
Смешанная, неполная или противоречивая форма typed/legacy отклоняется без выдумывания пропущенных полей.

При `source.present=0` все следующие восемь ячеек метаданных равны `null`. При present=1 sender, cipher,
исходные flags и binding generation взяты из настоящего source; removal/FIELD остаются отдельно
необязательными. Отсутствующий FIELD даёт три null-ячейки; у существующего FIELD sequence всё ещё
может отсутствовать. У `trigger` такое же правило, но пятая ячейка метаданных содержит настоящий
HISTORY rawTime, а не removalTime. Синтезированная запись коллектора может не иметь source, но иметь
настоящий trigger. Полученные delivery flags и исходные source/trigger flags не подменяют друг друга.

Подготовленные строки явно вызывают `batch.project(index, work)`. Результат владеет данными и живёт
лишь во время синхронного вывода этой записи. Один заранее заданный вектор projection work копируется
в начале callback и расходуется всеми его project-вызовами, без пополнения на каждую запись.
Ошибка, generic monostate или другой подготовленный variant дают статическую ошибку callback.
В успешном подготовленном выводе сохраняются все generic-поля. Exchange-коды Quote — реальные младшие
единицы UTF-16 mapper, выведенные беззнаковыми десятичными числами. Остальные числа — настоящие поля
проекции; decimal представлены raw-битами. Generic-строки сохраняют исходные значения, которые могут
быть шире проекции.

`eventTimeOrigin` равен `field` только при наличии SubscriptionProjection.eventTimeMs, иначе
`unspecified`. Соседняя ячейка содержит реальные optional миллисекунды FIELD, без придуманного нуля.
Quote.timeMs — результат бизнес-времени mapper. HISTORY rawTime, исходные FIELD seconds/sequence,
бизнес-время и optional eventTimeMs — разные координаты. Unix/calendar-смысл по имени поля не выводится.

## Локальные обозначения эпох

Обозначение эпохи — порядковый номер первого наблюдения, начиная с единицы, стабильный лишь внутри
этого запуска/таблицы. Это не wire generation, provider ID, маршрут, глобальная хронология или
доказательство пути от источника. Непрозрачные токены сравниваются только через `matches`; указатели,
закрытые поколения и membership ID не печатаются. Невалидный токен даёт `null`; запись данных
с невалидным токеном отклоняется.

Runtime одной семьи до активации резервирует таблицу из maximumAttempts+1 элементов: начальная эпоха
очереди и не более одного source reset на вошедшую попытку. Отдельный проверяемый бюджет байт
ограничивает запрошенную ёмкость массива токенов; фактическая зарезервированная capacity vector тоже
проверяется. Переполнение или нехватка бюджета отклоняются до активации. Таблица не растёт сверх
начального лимита и хранит только detached QueueIdentity: ни очередь, batch, receiver, callback,
event, runtime, ни credentials она не удерживает. Доступ сериализован приложением. В таблицу поступают
только реальные наблюдения одной выбранной очереди; другие произвольные очереди/токены вне этого frontend.

## Прямые строки свидетельств stderr

Необязательная причина очереди занимает три соседние ячейки: queueError, nestedKind, nestedError.
При отсутствии причины все три null; при имеющемся внешнем error без вложенной причины null только
две последние. `nestedKind` — исходный индекс QueueSourceError: 0 MuxError, 1 SubscriptionError,
2 StreamError, 3 TickerError, 4 HistoryError. Остальные error/enum сохраняют коды текущих публичных enum;
этот формат не назначает им новый смысл.

| Строка | Ячейки после имени |
| --- | --- |
| `recovery-status` | phase, attempts, demandedFamilies, convergedFamilies, stopped, terminalPresent |
| `recovery-diagnostic` | scope (`first`, `secondary`, `turn`, `history-query`), recoveryError, tcpError, nativeCode, protocolError, queueError-or-null, nestedKind-or-null, nestedError-or-null, endpointError-or-null, writerError-or-null |
| `tls-diagnostic` | scope (`first`, `secondary`, `turn`, `history-query`), tlsError, sdkCode, verifyCode, tcpError, nativeCode, protocolError; доступна только в сборке recovery с TLS |
| `command-accepted` | command (`initial-replace`, `remove-initial`), actualNonzeroTicketId |
| `command-result` | command, id, status, revision, changed, muxError-or-null |
| `reset` | sequence, fromEpochOrdinal-or-null, toEpochOrdinal-or-null, knownDiscardedQueueRecords, knownDiscardedSourceRecords-or-null, skippedResets, unknownEligibility, unknownUpstreamLoss, countsSaturated, sourceCommitted, globalAckPending, firstCause(3 cells), secondaryCause(3 cells) |
| `reset-ack` | exactAttemptedSequence, actualControlAdmission |
| `history` | selectedLocalId, queriedSymbolUTF16HEX, epochOrdinal-or-null, authoritativeRevision, clientDesiredRevision, mappingDesiredRevision, requestedFrom, mappingCurrent, acceptedUnstagedData, paused, resetPending, hadGap, queuedForKey, executingForKey, completeDeliveredRange, sourcePresent |
| `history-source` | selectedLocalId, queriedSymbolUTF16HEX, sourceRevision, observationSerial, partitionActivation, requestedFrom, sourceFrontier, knownFrontier, snipFrontier-or-null, snapshot, sourceTransactionPending, initialPending, viewTransactionDirty, completeRequestedRange |
| `history-unavailable` | selectedLocalId, attemptedSymbolUTF16HEX, actualRequestedFrom, actualRecoveryError |
| `recovery-terminal` | outcome, deadlineExpired, heldDataDiscarded, nativeOperationsAtCutoff-or-null, wirePresent |
| `wire-terminal` | primaryTcpError, primaryNativeCode, primaryProtocolError, secondaryTcpError, secondaryNativeCode, secondaryProtocolError, aborted, peerEnded, writeAcceptanceUnknown, reportedWriteBytes, discardedInputBytes, pendingOutputBytes |
| `queue-terminal` | family, epochOrdinal-or-null, outcome, authoritativeRevision, hadGap, unknownEligibility, unknownUpstreamLoss, failedRegistrationCount, primaryCause(3 cells), secondaryCause(3 cells) |
| `queue-counts` | family, acceptedFrames, acceptedRecords, collectorAcknowledged, queueAdmitted, withdrawn, discarded, sourceRecordsDiscarded, callbackStartedBatches, callbackStartedRecords, callbackCompletedBatches, callbackCompletedRecords, callbackFailedBatches, callbackFailedRecords, commandsApplied, commandsRejected, commandsCancelled, saturated |

Фазы Recovery: 0 CONFIGURED, 1 PREPARING, 2 STARTING, 3 NEGOTIATING, 4 REPLAYING, 5 ACTIVE,
6 RETIRING, 7 RESETTING, 8 BACKOFF, 9 STOPPING, 10 CLOSED, 11 EXHAUSTED, 12 FAILED.
Биты family mask — позиции RecoveryFamily: TICKER 0, STREAM 1, HISTORY 2.
Command status: 0 APPLIED, 1 REJECTED, 2 CANCELLED_CLOSING. Control admission: 0 RECORDED,
1 COALESCED, 2 STALE, 3 CLOSED. Terminal outcome: 0 DRAINED, 1 ABORTED, 2 FAILED.
HISTORY snapshot: 0 UNKNOWN, 1 RECEIVING, 2 COMPLETE, 3 TRUNCATED.

Принятая команда ещё не обязательно применена, синхронизирована, скомпонована, передана или подтверждена
источником. Runtime получает настоящий ticket result и передаёт функции вывода. ACTIVE и convergence
bits — реальные локальные наблюдения, не подтверждения peer.

Для reset runtime копирует настоящий notice, вызывает writeRecoveryReset, дожидается успешного
flush/check и лишь затем вызывает acknowledgeReset с точной sequence. В writeRecoveryResetAcknowledgement
передаётся только фактический результат. STALE означает повторное наблюдение нового notice в другом
ходе. Ошибка публикации требует abort; после неё нельзя подтверждать gap. Reset не превращается
в синтетическую DATA-запись. Неизвестные потери не печатаются как известный ноль.

Успешный HISTORY query создаёт соседние строки `history` и `history-source`. При sourcePresent=0
все двенадцать source-ячеек после ID/symbol равны null. При наличии source его поля копируются точно:
RECEIVING может сочетаться с полным верхним запрошенным диапазоном, пока другому view нужны нижние данные.
CompleteRequestedRange и completeDeliveredRange — разные реальные поля. Видны pending delivery, pause,
reset, gaps и актуальность mapping. Detached-результат — свидетельство AS-OF принятого префикса;
он не доказывает будущую свежесть, состояние непрочитанного wire, всю историю поставщика или peer ACK.

Недоступный/отклонённый query даёт `history-unavailable` и при необходимости отдельную typed-диагностику.
Старый успешный диапазон, старая эпоха, complete=true или выдуманное наблюдение complete=false не
повторяются. Недоступность не доказывает ни полноту, ни неполноту диапазона. Строки успешного и
отклонённого наблюдений самостоятельны; прежний успех не становится текущим подтверждением свежести.

Terminal-строки требуют наличия настоящего optional public terminal. Счётчики, истёкшая длительность,
нулевой pending bytes, stop request или пустой FIFO не создают DRAINED. Wire-строка появляется лишь
для фактического optional результата завершённой попытки. Reported write bytes — локальный приём;
pending/unknown остаются видимыми и не доказывают network flush, получение peer или TLS close-notify.
Queue counts описывают свои локальные области; складывать их в общий счётчик потерянных событий нельзя.

`failed()` сохраняет признак окончательного отказа публикации. Runtime проверяет его после возврата
owner turn и явно запрашивает завершение; функция вывода не обращается к владельцу и не останавливает его.

## Порядок вызовов runtime

Runtime владеет очередью, регистрацией receiver, tickets, retry/poll и решением о завершении. До
активации он готовит labels/sink, открывает существующий receiver с выбранным binding, вызывает
`sink.begin()` и запускает владельца. После вернувшегося owner turn он может проверить completedRecords()
и запросить последующий cutoff. Настоящие изменения status/ticket/reset/query/terminal передаются прямо
свободным функциям вывода. HISTORY query выполняется вне callback с явным неизменным бюджетом работы.
Sink и labels живут до ухода callbacks и финального sink.flush(). Ни одна функция вывода не сохраняет
work-ссылки, указатели владельца, секреты или прежнее успешное состояние диапазона.

Гарантии доставки, управления и времени жизни описаны в
[контракте приёмника подписок](../subscriptions/RECEIVER.ru.md),
[контракте очереди](../mux/QUEUES.ru.md) и [контракте восстановления](../transport/RECOVERY.ru.md).
