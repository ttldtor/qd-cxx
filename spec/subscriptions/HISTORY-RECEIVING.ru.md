# HISTORY: владение приёмом и наблюдения

[English version](HISTORY-RECEIVING.md).

`EndpointOptions::contract = DeliveryContract::HISTORY` выбирает существующий индексируемый
HISTORY collector, независимые представления подписок и очередь доставки. SDK worker владеет
регистрациями, mapping desired-набора, replay, сетевым progress, reset источника и ACK reset.
Приложение не создаёт collector и не передаёт бюджеты отдельных тактов. По умолчанию остаётся
TICKER; STREAM и HISTORY выбираются явно как разные контракты.

Order — первый поддерживаемый типизированный HISTORY route. Без явного registry SDK выбирает
свою стандартную схему Order; записи отдельных sources задаются через неизменяемый EventRegistry.
Order требует HISTORY и остаётся индексируемым событием с отдельной source identity. Девять обычных
Feed-типов (Quote, Profile, Trade, TradeETH, Summary, TextMessage, TextConfiguration, Message и
Configuration) отклоняют HISTORY: создание подписки возвращает `UNSUPPORTED_PROFILE` со стадией
`subscription-history-event-type` до native-регистрации. Сырой индексируемый layout и заявленная
вызывающей стороной time-series capability не разрешают бизнес-mapper. Общий HISTORY receiver
сохраняет полные пользовательские схемы через существующий нижний receiver API; это не добавляет
публичную raw-record Feed-подписку или типизированную TimeAndSale facade.

## Пассивные снимки с собственным владением

`Subscription<Event>::getHistorySnapshot()` копирует владеющий `HistorySubscriptionSnapshot`.
Getter не выполняет I/O, queue progress, ожидание callback или запрос источника. Копия ограниченных
метаданных может выделять память; ошибка проходит существующую границу ошибок facade.

Worker перечисляет авторитетные применённые interests регистрации и запрашивает настоящий
`QueueRangeStatus` для каждого физического record/symbol/selector. Один логический selector может
породить несколько routes. Элементы владеют текстом символа и неизменяемым schema provenance;
payload, callback, collector и native owner не удерживаются. Порядок вектора публично не задан.
Непрозрачная identity владельца, проверяемый serial публикации и монотонное время наблюдения
определяют локальный срез. Это не глобальные часы, сравнимые между владельцами ID или ACK peer.

`CURRENT` означает, что последняя успешная публикация соответствует последним принятым logical,
connection и clear fences и наблюдённому срезу retirement callback. Это не обещает отсутствия
непрочитанных сетевых данных и будущей свежести. Одного source coverage недостаточно: только
`completeDeliveredRange` элемента подтверждает его ограниченный синхронизированный, чистый и
доставленный локальный диапазон. Узкий диапазон может завершиться, пока широкий диапазон другой
подписки остаётся частичным. Snapshot END и нулевые event flags эти условия не заменяют.

Неизвестный mapping, неактивный источник, reset, pause/retirement callback, отказ публикации и
закрытие/retirement имеют явные состояния. Любой возвращённый неактуальный срез сбрасывает
`completeDeliveredRange`. До первой публикации serial равен нулю. Для другого контракта getter
возвращает `UNSUPPORTED`. Сохранённые копии остаются историческими значениями AS-OF после смены
источника и native retirement и сами не обновляются. Final означает настоящий retirement
логической регистрации и всех выбранных callback captures.

## Ограничения, единицы диапазона и reset

Изменение только нижней границы существующего физического ключа HISTORY передаётся как ADD с
новой границей, без предварительного REMOVE. Удалённый ключ передаётся через REMOVE; пересекающиеся
логические представления сохраняют минимальную общую границу. Знаковый MAX остаётся реальной
границей demand с семантикой отключения, а не молча исключается. Эти переходы соответствуют
зафиксированному пути time-series subscription в QD.

`HistoryReceivingOptions` независимо ограничивает общие points, удерживаемые frames/bytes,
resident backing collector, pending updates каждого view и два банка публикации наблюдений.
Значения по умолчанию: 4096 points, 256 frames, 64MiB общих frames, 128MiB resident collector,
128 pending updates и 4MiB backing целых source owners на подписку, а также 8MiB наблюдений. Это логические резервы, не RSS процесса и не
обещание задержки. Проверяемая арифметика и существующие конечные runtime capacities отклоняют
несовместимый профиль до активации. Один внешний такт worker делит единый observation work seed
между всеми подписками и ключами, без пополнения на каждый ключ. Неудачная полная публикация
даёт явное unknown, а не частичный вектор с устаревшим complete.

`requestedFrom` и `rawTime` — знаковые сырые координаты индексируемой записи, а не Unix-миллисекунды.
Стандартная TimeAndSale явно связывает физические Time/Sequence slots; её проверенный Java delegate
преобразует миллисекунды в эту координату. Общие indexed-запросы используют full-index bound своего
delegate. Order запрашивает сырую границу `0` и сохраняет независимые разделы записей sources;
бизнес-поля Time/Sequence не выбирают эту границу. См. [контракт Order](../events/ORDER.ru.md).
Логическая time-series identity/capability отделена от доступности приёма; живая мутация
диапазона остаётся отдельным контрактом.

Принятые disconnect/clear и desired-изменения немедленно инвалидируют current completion через
отделённые facade fences. Новая полнота требует настоящего reset/mapping/ACK пути и retirement
callback. Старые неизменяемые batches сохраняют независимую валидность. Getter не выполняет ACK
и не продвигает reconnect. Pinned Java oracle и native controlled-peer test проверяют общий
receiver стандартной схемы. [Руководство Order](../../docs/ORDER.ru.md) показывает поддерживаемый типизированный публичный приём.

См. [oracle](../../tools/upstream/history-receiver-oracle/README.ru.md),
[логическую time-series identity](TIME-SERIES-IDENTITY.ru.md) и [статистику](CLIENT-STATISTICS.ru.md).
