# Измеренные доказательства QTP (QDCXX-006)

[English — основная версия](CAPTURES.md). Английский документ является основным; эта версия содержит полный русский перевод.

Метка доказательства **C** означает выполненный Java capture в пределах [перечня покрытия](../../tests/fixtures/protocol/COVERAGE.md) и [проверок/сведений о происхождении](../../tests/fixtures/protocol/VALIDATION.md). Метки исходников в других документах остаются доказательствами по исходному коду, если не связаны с этим документом. Baseline: QD 3.355 / `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Это описательные наблюдения, а не нормативные решения для C++.

## Разбиение на кадры и группировка callbacks

**C — P01:** [точный capture](../../tests/fixtures/protocol/expected/P01.tsv), [упорядоченные значения](../../tests/fixtures/protocol/expected/P01.semantic.tsv). В 31 байте содержатся описание [0,12), данные [12,23) и данные [23,31). Целый ввод даёт один callback с тремя записями; разбиение в позиции 23 даёт два callbacks с двумя, затем одной записью. Каждое разбиение (1..30) и побайтовая подача дают одни и те же упорядоченные значения. Байты неполного кадра остаются в ожидании без callbacks/ошибок для этого кадра; завершённое описание не вызывает callback записей. Это не устанавливает ошибку усечения при окончательном EOF: API окончательного EOF не вызывается.

Первый кадр данных использует маркер повторения символа `ff` для своей второй записи ABC; следующий кадр заново кодирует ABC (`0443`). Значения/позиции получены из формирования и разбора Java, а не из написанного вручную кодека. Источники: BinaryQTPParser.parseImpl, AbstractQTPParser.processPending, BinaryQTPComposer и PentaCodec Reader/Writer в [SOURCES](SOURCES.md).

## Отображения по направлениям и поля

**C — P03:** [A в B](../../tests/fixtures/protocol/expected/P03-A-B.tsv) отправляет ID 0 и доставляет ID 1; [B в A](../../tests/fixtures/protocol/expected/P03-B-A.tsv) отправляет ID 1 и доставляет ID 0. И данные, и STREAM_ADD_SUBSCRIPTION разрешают R по имени, не меняя локальный порядок R/S. В каждом направлении используются собственные новые parser/composer. Источники: BinaryQTPParser.remapRecord/parseSubscription и BinaryQTPComposer.describeRecord.

**C — P04:** [перестановка полей](../../tests/fixtures/protocol/expected/P04-order.tsv) отображает передаваемые x,y=(11,22) в локальные y,x,z=(22,11,0). У отсутствующего compact-int z сырые биты нулевые, и в этом конкретном классе поля он форматируется как 0; это ничего не говорит о значениях по умолчанию приложения, decimals, значениях по умолчанию XML или полях object. [Сценарий с псевдонимом](../../tests/fixtures/protocol/expected/P04-alias.tsv) сериализует локальное имя отправителя x.dot как свойство xdot и разрешает псевдоним получателя xdot в локальный слот 1 поля x. DefaultRecord.addFieldAlias также делает xdot именем свойства поля получателя; capture сохраняет эту деталь. Таблица псевдонимов не передаётся. Источники: поиск полей BinaryRecordDesc, AbstractDataField.getDefaultPropertyName, DefaultRecord.addFieldAlias.

## Запрошенные форматы полей через реальное соединение

**C — P05:** [none](../../tests/fixtures/protocol/expected/P05-none.tsv), [zero](../../tests/fixtures/protocol/expected/P05-zero.tsv), [x](../../tests/fixtures/protocol/expected/P05-x.tsv), [z](../../tests/fixtures/protocol/expected/P05-z.tsv). Входные описание/подписка peer проходят через MessageAdapterConnection.processChunks / ConnectionQTPParser. Получение ответа проходит через MessageAdapterConnection.retrieveChunks / ConnectionQTPComposer. Дескриптор запрошенного формата не внедряется напрямую. [Инструментирование драйвера](../../tools/upstream/protocol-oracle/README.md#driver-and-instrumentation) подавляет только недетерминированные metadata heartbeat/протокола и не является экспериментом с временным поведением или установлением протокольного взаимодействия.

| Входящий запрос до первого использования writer | Исходящее описание в передаче | Сырые x,y,z при контрольном чтении |
| --- | --- | --- |
| Отсутствует | x,y | 11,22,0 |
| Изначально ноль полей | x,y | 11,22,0 |
| Только x | x | 11,0,0 |
| Непустой запрос только z, отсутствующего локально | ноль полей | 0,0,0 |

Пустое пересечение не становится сигналом «все поля»: R/ABC по-прежнему доставляется, но без полезной нагрузки полей. Входящий R с ID 1 у peer разрешается в R с ID 0 у издателя; ответ описывает ID 0 издателя, а парсер peer доставляет локальный ID 1. Это подтверждает ограниченную указанной областью интерпретацию исходников. Источники: ConnectionQTPParser.remapRecord/getRequestedRecordDesc, MessageAdapterConnection.getRequestedRecordDesc, ConnectionQTPComposer.getRequestedRecordDesc, BinaryQTPComposer.getRecordDesc и BinaryRecordDesc.isEmpty. Динамические изменения формата полей, кэш/сброс, преобразования P06 и согласование протокола P09 остаются неизмеренными.
