# Реестр сообщений и состояния QTP

[English version](MESSAGE-STATE-REGISTRY.md). Английский текст является нормативным.

Реестр помогает выбрать существующий профиль декодирования/кодирования и найти его контракт
состояния и отказов. Он объединяет реализованные границы, не включая новые сообщения, параметры
или коллекторы. Эталон совместимости Java задан в [upstream.lock](../../upstream.lock): QD 3.355,
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Прежние [обзор границ](README.md)
и [каталог закреплённых исходников](SOURCES.md) сохраняют различие между анализом кода и захватами.

## Идентификаторы сообщений и направления live

ID относится к оболочке сообщения и не зависит от ID записи отправителя.
[FrameReader](FRAMING.ru.md) извлекает любой структурно допустимый неотрицательный signed32 type
и непрозрачный payload; успешное извлечение не подтверждает поддержку payload.
У пустой оболочки нет типа; она отличается от type 0.

В таблице **вход клиента** означает `LiveSession::create` с `feed`, **вход сервера** —
`createServer` с `feedServer`. Управляющие сообщения обрабатываются одинаково.
Отдельные кодеки не проверяют согласование сессии. Методы вывода явно формируют байты без I/O.

| ID / сообщение | Декодирование и допуск live | Кодирование live / отдельного компонента | Состояние и исход при отсутствии поддержки |
| --- | --- | --- | --- |
| Без типа / пустой heartbeat | Оба профиля возвращают EMPTY_HEARTBEAT. | Tape `marker()` выдаёт такую оболочку; отдельного live-метода для неё нет. | Не меняет протокол, привязки, аутентификацию или время записей. Не подтверждает EOF. |
| 0 HEARTBEAT | Оба профиля возвращают HEARTBEAT; известные биты приведены ниже. | `composeHeartbeat` всегда выдаёт type 0, в том числе без payload; tape `heartbeat` имеет более узкую грамматику. | Присутствие и сырые значения остаются метаданными: нет чтения часов, автоматического расписания или неявной установки времени прикладной записи. Неизвестные биты завершают live-ввод ошибкой. |
| 1 DESCRIBE_PROTOCOL | Оба разбирают полный DXP3 descriptor и объединяют полный/дельта-кандидат. | `composeProtocol`; `authorize` — отдельный путь type 1 с учётными данными. | Атомарная публикация обновляет эффективные свойства/возможности, аутентификацию и `hs`; пропущенные ключи/сообщения сохраняются. Неподдерживаемые расширения отклоняются до публикации. |
| 2 DESCRIBE_RECORDS | Оба атомарно заменяют эффективные описания/привязки ID записей peer. | `SubscriptionWriter::describe` / `LiveSession::describe`. | Входящая и исходящая карты независимы. Live-вывод не ведёт состояние объявленных ID; вызывающая сторона ставит описания перед DATA. Некорректный пакет не публикует кандидата. |
| 3 DESCRIBE_RESERVED | Не поддерживается обоими live-профилями. | Live-метода нет. | Live-ввод завершается UNSUPPORTED; только явный binary catalog tape-профиль игнорирует такую оболочку. |
| 4 PART | Не поддерживается, даже если peer объявил эту возможность. | API формирования/сборки PART нет. | Live-ввод завершается UNSUPPORTED. Фрагментация внешнего потока обрабатывается framer и не включает PART. |
| 5 RAW_DATA | Не поддерживается обоими live-профилями. Отдельный `decodeRawData` читает полный payload с нулевыми флагами. | Live-публикации RAW нет; `TapeWriter::data` выдаёт RAW в своём профиле записи. | Общее tape-декодирование RAW не подтверждает fanout по коллекторам или live-готовность. Фактический live RAW завершается UNSUPPORTED. |
| 10 TICKER_DATA; 15 STREAM_DATA; 20 HISTORY_DATA | Клиент: точная send-возможность peer, полученный протокол и разрешающая аутентификация; DATA допускается целой оболочкой. Сервер отклоняет DATA на входе. | `composeData`; сессионный `publishData` дополнительно требует соответствующую receive-возможность peer и разрешающую аутентификацию. | Семейство сохраняется в `LiveEvent::messageType`, включая пустое DATA. Политика чтения флагов/времени явная; семейство само не создаёт коллектор. DATA на входе сервера — UNSUPPORTED. |
| 11 TICKER_ADD; 12 TICKER_REMOVE | Сервер: точная send-возможность peer и разрешающая аутентификация, затем один целый владеющий пакет подписок. Клиент отклоняет входящие подписки. | `compose` / `subscribe`; кортеж TICKER: symbol + sender ID. | Сессионный вывод требует receive-add peer и конкретный запрошенный add/remove ID. Нет возможности — NOT_READY; неподдерживаемый вход клиента — UNSUPPORTED. |
| 16 STREAM_ADD; 17 STREAM_REMOVE | То же разделение сервера/клиента, что для 11/12. | `compose` / `subscribe`; кортеж STREAM: symbol + sender ID. | Та же направленная проверка; отдельная явная политика selector разрешает All. |
| 21 HISTORY_ADD; 22 HISTORY_REMOVE | То же разделение сервера/клиента; известным целям нужны явные индексированные метаданные. | `compose` / `subscribe`; только ADD несёт сырой signed64 bound. Обе формы HISTORY требуют индексированных метаданных. | Bound — координата записи, без предположения о миллисекундах. REMOVE отклоняет переданный bound. Неявных бизнес-преобразований нет. |
| 49–56 RMI; 61 TEXT; 75 ZIP; 139 GZIP; любой иной type | Live-реализации payload нет. | Соответствующего метода формирования нет. | Live-ввод завершается UNSUPPORTED. Наличие Java ID или compression marker не предоставляет native-поддержку; tape-отказы приведены ниже. |

Подробная грамматика: [live-сессия](../subscriptions/LIVE-SESSION.ru.md),
[подписки и описания](../subscriptions/WRITER.ru.md),
[входящие кортежи подписок](INCOMING-SUBSCRIPTIONS.ru.md), [исходящие DATA](LIVE-DATA-WRITE.ru.md),
[полные DATA payload](STREAM-DATA.ru.md). Отдельный декодер подписок потребляет неизвестные
*описанные* локальные цели без выдачи, но отклоняет необъявленные sender ID. Live-сервер использует
направление описаний peer. Входящие кортежи не выбирают layout исходящих DATA.

Объявление peer может сохранять неподдерживаемый ID, не предоставляя реализацию его payload.
Локальный `composeProtocol` принимает только точные поддерживаемые пары ID/name выбранных
направлений: общие управляющие в обоих, DATA в send и клиентском receive, подписки в клиентском
send или серверном receive. На серверной сессии `subscribe` возвращает UNSUPPORTED.
`publishStreamPrefix` / `composeStreamPrefix` выбирают непустой префикс STREAM по явным мягкому
и жёсткому ограничениям байтов; результат по-прежнему одна полная оболочка с теми же правилами
согласования и владения.

## Профили записи

Tape-профиль выбирается явно, независимо от live-согласования. Таблица кратко описывает допуск;
точные грамматика descriptor, ошибки, ограничения и условия завершения находятся по ссылкам.

| Профиль | ID при декодировании / кодировании | Состояние и граница отказа |
| --- | --- | --- |
| [TapeReader LEGACY](TAPE-READER.ru.md) | Начальный 1, описания 2, пустые маркеры, DATA 15. | Ограниченный descriptor tape/NONE/пустой opt; описания только добавляются, флаги нулевые. Явный heartbeat 0 и другие фактические типы не поддерживаются. |
| [TapeReader RAW_MESSAGE_V1](RAW-TAPE.ru.md) | Начальный 1, описания 2, пустые маркеры, heartbeat 0, DATA 5. | Время MESSAGE, opt отсутствует/пуст/точно `hs`, флаги нулевые. `hs` не разрешает ненулевые флаги. Другие фактические семейства и повторные descriptor отклоняются. |
| [TapeReader BINARY_CATALOG_V1](BINARY-MESSAGES.ru.md) | Полный/дельта 1, описания 2, пустые маркеры, heartbeat 0, reserved 3, DATA 5/10/15/20. | Четыре семейства используют общий явно выбранный декодер известных флагов в порядке провода. Type 3 выдаёт ограниченное ignore-событие и ничего не сбрасывает. PART, подписки, RMI, TEXT, сжатие и остальные фактические ID — UNSUPPORTED/MESSAGE. |
| [TapeWriter RAW/MESSAGE v1](TAPE-WRITER.ru.md) | `begin` 1; `describe` 2; `heartbeat` 0; `data` 5; `marker` пустой; `finish` не выдаёт байтов. | READY → ACTIVE → FINISHED; любая ошибочная операция переводит в FAILED. Описания нужны перед DATA; объявления фиксируются только при успехе всей операции. Флаги нулевые; неявного footer нет. |

Только catalog допускает документированный путь самоописания без заголовка и полный/дельта
descriptor. Его `FieldTimePolicy::READ` и `TapeExternalTimePolicy::SIDECAR` — независимые явные
расширения; прежние сигнатуры конструкторов оставляют их выключенными.
[FIELD/MESSAGE](TAPE-TIME.ru.md) и [сопутствующее время](SIDECAR-TIME.ru.md) не становятся
live-возможностями и не определяют индексированное/бизнес-время по догадке.

## Параметры, флаги и расширения

`hs` — поддерживаемый согласуемый параметр протокола. У следующих битов/настроек отдельные
владельцы; принятие синтаксиса не согласует новую возможность.

| Элемент | Декодирование / эффективное состояние | Кодирование / отсутствие поддержки |
| --- | --- | --- |
| `opt=hs` | Live узнаёт точный токен `hs` в списке через запятую; повторы идемпотентны, неизвестные токены информационные, пробелы/подстроки не совпадают. Изначально отсутствие, null или пустое значение выключает `hs` peer. `historySnapshotEnabled` — локальный `LiveLimits::historySnapshot` И эффективный `hs` peer. Дельта без `opt` сохраняет его. | Локальный непустой `opt` должен быть точно `hs`, включённым локально. Остальные локальные строки — UNSUPPORTED. Tape использует более узкие правила своей таблицы. |
| Биты событий DATA | Явная event policy BINARY_CATALOG_V1 принимает любое подмножество `0x5f`: TX_PENDING `0x01`, REMOVE_EVENT `0x02`, SNAPSHOT_BEGIN `0x04`, SNAPSHOT_END `0x08`, SNAPSHOT_SNIP `0x10`, SNAPSHOT_MODE `0x40`. Параметры descriptor не маскируют читаемые биты. | Live DATA разрешает эти биты только при включённом `hs`; неизвестные биты или ненулевые без `hs` отклоняются, тогда как Java маскирует неподдерживаемые выходные биты. ZERO_FLAGS и прежние payload API отклоняют все ненулевые флаги. |
| Неизвестные/резервные event bits | Проекция signed low32 проверяется после полного symbol, перед sender ID: отрицательные маски, `0x20`, REMOVE_SYMBOL `0x80` и остальные неизвестные биты не поддерживаются. | Native-вывод не маскируется молча. Общие флаги не подтверждают завершение транзакции/snapshot в коллекторе. |
| REMOVE_EVENT | Заменяет обычные значения на signed64 removal time; явная FIELD policy обрабатывает отдельный физический префикс Event*. Нулевое removal time остаётся присутствующим. | Writer требует пустой span значений; отклоняет первый не-VOID исходящий field `Event*`. Неиндексированный вывод использует нулевое removal time. См. точный [контракт удаления](LIVE-DATA-WRITE.ru.md). |
| Префиксы symbol repeat / MRU | FF повторяет прежний symbol; F8 повторяет текущие MRU-флаги; F9 вводит флаги. Состояние сбрасывается на каждом DATA/сообщении подписок. Префикс — синтаксис, не согласуемая возможность. | DATA использует canonical Penta/CESU-8/repeats. EXACT_ONLY подписок сохраняет FD/FF; явные canonical-политики определяет [контракт selector](../subscriptions/SELECTORS.ru.md). All разрешает только выбранная STREAM policy. |
| Биты содержимого heartbeat | Live: `1` timeMillis compact long, `2` timeMark, `4` deltaMark, `8` lagMark compact ints, в этом порядке. Отсутствие отличается от присутствующего нуля; неизвестные биты — UNSUPPORTED, усечение/лишние байты — MALFORMED. | Live формирует ту же конечную грамматику. RAW/catalog tapes и TapeWriter допускают лишь отсутствие payload, bits 0 без времени или bits 1 с timestamp. Остальные формы heartbeat там не поддерживаются. |
| `authentication` / `authorization` | Пропущенный ключ сохраняет эффективное свойство. Каждый принятый type 1 заново оценивает его: непустое значение означает REQUIRED, пустое — ACKNOWLEDGED; отсутствие/null сохраняет прежнее состояние аутентификации. Повторная оценка может заменить AUTHORIZATION_EMITTED, даже если дельта не содержит ключа. Текст authorization исключён из публичных snapshot views. | Обычное формирование descriptor запрещает оба credential-ключа. Только `authorize` выдаёт учётные данные при явном adapter-supplied VERIFIED_TLS_BASIC. Plain/отсутствие допуска — CREDENTIALS_FORBIDDEN. |
| Свойства сообщений / агрегация | Представления сообщений snapshot показывают локальные переопределения; наследование разрешается через свойства верхнего уровня. Peer SEND DATA `aggregationPeriodInfo` — независимый optional-отчёт о диапазоне, не эхо запроса. Пропущенные дельты сохраняют прежний отчёт; отсутствие/ошибка/UNKNOWN или отсечённый source не даёт текущего отчёта. | Peer RECEIVE DATA `requestedAggregationPeriod` несёт текущее локальное намерение; явно пустая строка сбрасывает к серверному default. Обновления — дельты свойств type 1, без нового ID или ACK. [Серверная агрегация](../transport/SERVER-AGGREGATION.ru.md) задаёт validation, отсечение старого source и документированное числовое исключение совместимости. Остальные сохраняемые свойства не включают неявно filter/stripe/aggregation. |
| EndpointId / расширения descriptor | Live принимает один структурно корректный optional endpoint tail и отбрасывает его; это не routing identity. Неизвестный хвост — UNSUPPORTED. Catalog tape проверяет структуру известного tail, затем отказывает. | У публичного live descriptor нет поля вывода endpoint. API произвольных расширений не подразумевается. [Endpoint witnesses](../../tests/fixtures/subscriptions/endpoint-identity-v1/README.md) задают точные live-случаи. |
| `version`, `time`, layout полей | Version text не включает wide-decimal fallback и не выбирает Java pin. FIELD-вход требует локальной policy; исходящий live `time` отсутствует/пуст/NONE/none. Локальный непустой `type` должен быть `qtp`. | Свойство не включает Java legacy schemeKnown/readAs/неявное преобразование типов. Точные layout и явные профили преобразования сохраняют собственный [контракт схемы](SCHEMA-EXCHANGE.md) (EN). |

Exact symbols сохраняют полную UTF-16 identity, включая пустой текст, встроенный ноль, непарные
кодовые единицы и региональные суффиксы; декодирование не удаляет и не нормализует их. Выбранная
грамматика symbol проверяет некорректные кодировки и ограничения. Selector All и резервный
начальный `*` подчиняются явной политике подписок; descriptor или написание symbol не включает их.

## Владение состоянием и ошибки

| Владелец состояния | Переход, срок жизни и ошибка |
| --- | --- |
| Framer | Хранит неполные внешние байты между feed; один владеющий frame на результат. Только `finish()` сигнализирует EOF. Ошибка терминальна; поиска/resync нет. `reset()` сбрасывает лишь отдельный framer. |
| Live epoch | Новая identity при создании, фиксированные локальная схема и исходящие layout. Reset с повторным epoch отсутствует. Ошибка feed терминальна; следующие feed принимают ноль байтов. `finish` требует полного framing и полученного protocol descriptor, затем CLOSED. |
| Эффективный snapshot peer | Полный кандидат descriptor/описаний успешно завершается до публикации. Ошибка кандидата сохраняет старые snapshot, generation и authentication/`hs`. Удержанные старые владельцы остаются действительными. Замены sender не переписывают уже владеющие записи. |
| Live authentication | Изначально UNCHALLENGED; успешный локальный `authorize` → AUTHORIZATION_EMITTED. Каждый принятый type 1 устанавливает REQUIRED при непустом эффективном authentication или ACKNOWLEDGED при пустом, в том числе сохранённом из прежнего descriptor; отсутствие/null эффективного значения сохраняет прежнее состояние. REQUIRED и AUTHORIZATION_EMITTED блокируют входящие DATA/подписки и исходящие DATA/подписки. Type 2 не переоценивает аутентификацию. |
| Операция live-вывода | Нет направленной возможности → NOT_READY; активный auth gate → AUTHENTICATION_REQUIRED. Типизированные ошибки validation/work/resource/allocation не возвращают output и сохраняют работоспособность здоровой сессии. Потраченное work сохраняется. Управляющий вывод не подтверждает получение peer. |
| Владелец pending output | Владеет целыми неизменяемыми байтами; `consume` двигает только локальный suffix. Полное потребление не освобождает backing credit до уничтожения/замены владельца. Наличие байтов не разрешает reconnect replay. |
| Результат отдельного payload decoder | DATA сохраняет префикс полных записей при ошибке; subscription decoder отклоняет весь пакет. Live атомарно допускает DATA и не показывает префикс ошибочной текущей оболочки. Tape сохраняет свой отдельно определённый завершённый префикс. |
| Receiver / collector | Получает нормализованные владеющие записи после protocol admission. Локальные membership, delivery, snapshot completion и source reset принадлежат [receiver](../subscriptions/RECEIVER.ru.md), [STREAM](../subscriptions/STREAM-COLLECTOR.ru.md), [TICKER](../subscriptions/TICKER-COLLECTOR.ru.md), [HISTORY](../subscriptions/HISTORY-COLLECTOR.ru.md) и [recovery](../transport/RECOVERY.ru.md). Декодированное семейство/флаг или локальный command ticket не означает peer ACK или завершённую доставку. |

Единицы ограничений, смысл нулевых значений и поведение отказа определяет выбранный API/профиль.
Например, `LiveLimits::dataEventBytes=0` выбирает фиксированную арену событий. Запрошенная backing
память, логическая работа, число владельцев и результаты, удерживаемые вызывающей стороной,
учитываются отдельно; реестр не добавляет универсальных гарантий размера сообщения, RSS или задержки.

## Карта исходников и проверок

Ссылки указывают реализацию и существующие проверки, не утверждая, что проверены все Java
adapter, платформы и комбинации. Исторические отчёты сохраняют свои границы.

| Контракт | Точки в исходниках | Существующие проверки / доказательства |
| --- | --- | --- |
| Live dispatch, дельты, readiness, auth, heartbeat и epoch | [live_session.cpp](../../src/protocol/live_session.cpp): `dispatch`, `receiveSubscription`, `validateOutgoingProperties`, `subscribe`, `publishDataImpl`, `heartbeat`, `finish`; [публичный API](../../include/qd/protocol/live_session.hpp) | [live-тесты](../../tests/conformance/live_session_tests.cpp), [границы validation](../../tests/conformance/subscriptions/LIVE-VALIDATION.md) |
| Шесть subscription ID и отдельное владение выводом | [subscription_writer.cpp](../../src/protocol/subscription_writer.cpp); [incoming_subscription.cpp](../../src/protocol/incoming_subscription.cpp) | [writer-тесты](../../tests/conformance/subscription_writer_tests.cpp), [incoming-тесты](../../tests/conformance/incoming_subscription_tests.cpp), [Java writer capture](../../tests/fixtures/subscriptions/writer/README.md) |
| Флаги DATA, удаление и явные policy | [stream_data.cpp](../../src/protocol/stream_data.cpp); [live_decode.hpp](../../src/protocol/live_decode.hpp); [subscription_writer.cpp](../../src/protocol/subscription_writer.cpp) | [live writer tests](../../tests/conformance/live_data_writer_tests.cpp), [flags validation](../../tests/conformance/protocol/field-types/FLAGS-VALIDATION.md) |
| Tape dispatch семейств/descriptor и состояние записи | [tape_reader.cpp](../../src/protocol/tape_reader.cpp): `dispatch`; [tape_writer.cpp](../../src/protocol/tape_writer.cpp) | [family tests](../../tests/conformance/tape_message_family_tests.cpp), [RAW tests](../../tests/conformance/raw_tape_tests.cpp), [legacy reader tests](../../tests/conformance/tape_reader_tests.cpp), [writer tests](../../tests/conformance/tape_writer_tests.cpp) |
| Membership после протокола и срок жизни receiver | [subscription_receiver.cpp](../../src/client/subscription_receiver.cpp) | [receiver tests](../../tests/conformance/subscription_receiver_tests.cpp); [границы HISTORY receiving](../subscriptions/HISTORY-RECEIVING.ru.md) |
| Свойства агрегации и независимый отчёт peer | [Контракт агрегации и первичные источники](../transport/SERVER-AGGREGATION.ru.md) | [aggregation tests](../../tests/conformance/server_aggregation_tests.cpp), [production oracle](../../tools/upstream/server-aggregation-oracle/README.md) |

Точки в Java — `MessageConstants`, `MessageType`, `ProtocolOption`, `ProtocolDescriptor`,
`BinaryQTPParser`, `BinaryQTPComposer`, `EventFlag` и `PentaCodec`; [SOURCES](SOURCES.md) разрешает
их на закреплённой ревизии. Native-политики атомарного кандидата, завершённого префикса и отказа
неподдерживаемого вывода выше — намеренные документированные границы; они не объявляют более
широкое поведение Java parsing/masking некорректной wire-грамматикой.
