# Наблюдение принятых подписок

[English version](OBSERVABLE-SUBSCRIPTIONS.md). Английская версия основная.

## Владеющее чтение состояния

`Subscription::getSnapshot()` возвращает независимый `SubscriptionSnapshot`: одну принятую desired `revision`, точные типизированные логические `symbols` и неизменяемый нормализованный набор `eventTypes`. Копирование атомарно под mutex detached-подписки. Результат сохраняется после изменений, close, завершения endpoint и разрушения runtime. `getEventTypes()` копирует неизменяемый набор типов; `containsEventType()` возвращает false для неизвестных значений enum. Эти операции не продвигают native или сетевую работу.

Принятая ревизия отличается от применения worker, MUX command ticket, физического спроса, отправки wire и peer acknowledgement. Действуют прежние [барьеры команд и применения](CONTRACTS.ru.md). Перестановка одинакового эффективного логического набора, повторные add, remove отсутствующего символа и clear пустого набора не меняют ревизию. Некорректный ввод, allocation failure, отказ конечного capacity/work и исчерпание identity сохраняют прежний набор/ревизию.

Чтение сохраняет логические decorators, даже когда физический receiving demand склеивается. PLAIN и INDEXED остаются разными identity; имя source не является identity. Действуют [идентичность decorators](DECORATED-SELECTORS.ru.md) и [ADR-0012](../../docs/decisions/ADR-0012-decorated-subscription-identity.ru.md). Чтение состояния не включает неподдерживаемую typed history activation.

Close закрывает допуск мутаций, но сохраняет последний принятый логический snapshot для detached-чтения. Это локальное значение, а не продолжающийся спрос к источнику.

## Фазы наблюдателей

`addChangeListener()` устанавливает непустой callable и возвращает непереиспользуемую identity, отдельную от event/state listeners. Начальная ADDED выбирается для существующего непустого набора; пустая подписка не порождает начальную фазу. Native listener ID обозначает регистрацию; одинаковые callable не удаляются как дубликаты автоматически.

Успешная эффективная мутация готовит полный snapshot после изменения и фактическую delta до публикации нового desired набора/ревизии. Replace резервирует REMOVED и ADDED до видимости любой из них. REMOVED предшествует ADDED с той же ревизией; пустая фаза пропускается. Фаза предоставляет borrowed `SubscriptionChange`: неизменяемый полный snapshot и символы живут до возврата/exception. Для независимого хранения копируйте snapshot или selector values.

Выбранный fanout удерживает неизменяемую selection слушателей до завершения. Remove или close внутри fanout не разрушает его callable/аргументы. Reentrant registration входит в ordinary fanout только после собственной initial state phase. Следующая фаза выбирает текущий initialized registry: self-removal внутри REMOVED подавляет последующую ADDED этой регистрации. Ожидающая начальная фаза отменяется, если наблюдатель удалён до selection. CLOSED выбирается ровно один раз при remove регистрации или завершении подписки.

Завершение может подавить невыбранные ordinary phases; зарезервированный CLOSED snapshot содержит последнюю принятую ревизию. После CLOSED этого наблюдателя не начинается ordinary callback. Уже выбранный fanout может завершиться до terminal dispatch.

## Исполнение и конечный повторный вход

Незанятый producer доставляет принятые уведомления синхронно после commit и вне mutation/container locks. Это работает до connect и после disconnect без worker, socket, caller polling и прикладного event loop.

Вся change delivery одного runtime использует общий detached finite dispatch domain. Callback или конкурентный producer присоединяется к текущему turn без ожидания и рекурсивного вызова. Текущий owner дренирует допущенные selections после возврата callback. Порядок commit/phases каждой подписки детерминирован; глобальный порядок ревизий между подписками не обещается.

Изменения из существующего data callback немедленно сохраняют high-level accepted revision, но change delivery откладывается до возврата всего выбранного data-listener fanout. Физическое применение команды остаётся за прежним owner barrier. Borrowed data batch нельзя отозвать задним числом.

Cross-runtime mutations сохраняют прежнюю поддержку receiving API. До commit admission резервирует конечное after-return target continuation и полный target payload, списывая capacity causal root, target и активного continuation driver без ожидания. Точные weak source-callback barriers с отдельными data/change epochs не позволяют конкурентному foreign dispatcher выбрать новую фазу до возврата callback и его выбранных captures. Исполняемый source удерживает свой detached domain; истечение weak reference поэтому не означает ещё исполняемый source. Causal root lease переживает передачу foreign owner и запрещает пополнять бюджет, пока остаются его pending/executing phases. Continuations дренируются итеративно вне locks, включая существующий выход data callback, без нового worker и caller polling. Блокирующие creator wait/shutdown отклоняют callback/retirement reentry вместо ожидания собственных captures.

`maximumChangeListenersPerSubscription` включает active, selected, removed и capture-retiring callable. `maximumPendingSubscriptionChanges` включает pending/executing phases до окончательного освобождения аргументов/selection; двусторонний replace требует двух свободных slots. `maximumSubscriptionChangesPerTurn` ограничивает общие phase/registration admissions всего внешнего runtime turn. Reentrant/concurrent producers не пополняют его. Исчерпание отклоняет допуск до публикации revision/registry.

Каждая phase/registration ограничена прежними symbol count/units и неизменяемым числом поддерживаемых типов. Полное хранение candidate, delta и snapshot выделяется до commit. Terminal storage резервируется на каждого charged observer отдельно от ordinary capacity. Последнее освобождение callable происходит вне всех control locks; charge снимается только после разрушения прикладных captures. Произвольный blocking/looping application code остаётся непрерываемым.

## Close и ошибки

`Subscription::close()` и destructor сохраняют прежний nonblocking request: публикуют close fence и резервируют terminal selection без inline-вызова произвольного пользовательского кода. Фактический существующий worker retirement либо creator `closeAndAwaitTermination()` доставляет CLOSED, включая offline runtime без запуска worker. Caller ничего не дренирует и не опрашивает. Уже активный локальный callback owner может доставить зарезервированную terminal phase после возврата текущего callback.

Endpoint await включает выбранные change callbacks и окончательное разрушение captures. Одно наблюдение состояния не является этим quiescence barrier. Повторные remove/close не повторяют CLOSED. Заполнение ordinary notifications не отклоняет close и не теряет terminal selection.

Observer exceptions являются advisory: выбранный fanout продолжается, side effects не повторяются, `getChangeDiagnostic()` сохраняет первый статический `CALLBACK_FAILURE`. Они не ломают event delivery, не меняют desired demand и не заменяют lifecycle diagnostic. Политика ошибок event listeners прежняя.

## Pinned Java и явные native отличия

[Production capture](../../tests/fixtures/subscription-observer/README.ru.md) использует Java QD 3.355 из [upstream.lock](../../upstream.lock): `DXFeedSubscription`, `ObservableSubscription` и `ObservableSubscriptionChangeListener`. Фактическая membership и removal-before-addition напрямую сравниваются native tests. Raw Java traces, включая владение monitor, recursive removal и propagated exceptions, сохранены неизменными.

Java использует синхронные monitor-protected callbacks и может рекурсивно вызвать CLOSED при remove listener. Native callbacks идут вне container locks, откладывают reentry/contention, используют конечный admission и содержат advisory exceptions. Native close остаётся асинхронным retirement request. Это явные timing/error отличия receiving profile; Java monitor не объявляется дефектом. Symmetric decorated identity отдельно регулируется ADR-0012.

Observer API не добавляет publisher subscription, dynamic event family, `Subscription.setFilter`, source scheduler или network progress service. Отдельный [Feed filter classifier](FEED-FILTERS.ru.md) владеет query/category и dynamic-update semantics; прежние remove/clear и shared-demand barriers сохраняются.
