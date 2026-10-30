# Приёмка обычных реальных tape

[English](README.md). Английская версия нормативная.

[Compare-RealTapes.ps1](../../../tests/conformance/protocol/real-tape/Compare-RealTapes.ps1)
сравнивает ordinary C++ output7 либо строго проверенное обычное подмножество output8
с закреплённым production parser Java. Проверяются все
snapshots обычной схемы, поколения, исходные sender ID, порядок и привязки wire-полей,
локальные ID/порядок объединения, порядок записей, точные UTF-16 символы, полные типы,
raw integers и nullable точные UTF-16 значения. Для RAW/MESSAGE сравниваются наличие и
исходное значение timestamp в миллисекундах, cursor sequence и event time. Оба пути
требуют нулевые event flags; поддержка ненулевых flags этим не подтверждается.

Явный профиль владеющего источника также сравнивает узкое ordinary MESSAGE
подмножество output9: identity источника, ресурсы/закрытие и отдельно учтённые
raw/effective time. `OracleProfile` выбирает точный baseline/current parser pin
без поиска версии. См. [сравнение владеющего источника](#явное-сравнение-владеющего-источника).

## Запуск

Нужны PowerShell 7.4+, JDK 8, чистый предварительно собранный checkout по
[upstream.lock](../../../upstream.lock) с зависимостями `qds-file`, явный
[native `qdcxx-run-utc`](../../dev/README.ru.md) и собранная утилита
`qdcxx-tape-inspect` с [`--wire-schema ordinary-v1`](../../../spec/schema/WIRE-INVENTORY.ru.md).
Запуск не собирает upstream, не устанавливает зависимости, не меняет исходники и не
подключается к сети. JDK 21+ для [форматтера](../../format/README.ru.md) используется отдельно.

Справка не требует рабочих входов и не создаёт файлов:

```powershell
pwsh -NoProfile -File tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 -Help
Get-Help ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 -Full
pwsh -NoProfile -File tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 -Help
```

Запускайте из корня репозитория. Задайте явные локальные пути `$tape`, `$inspector`,
`$lockedCheckout`, `$oracleJdk`, `$runner` (native runner) и новый каталог внутри `.work`.
Данный PowerShell workflow проверен на Windows; POSIX cleanup через него не квалифицирован
и явно отвергается:

```powershell
& ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 `
    -InputFile $tape -Inspector $inspector -UpstreamRoot $lockedCheckout `
    -JavaHome $oracleJdk -Runner $runner -RunDirectory .work/tape-acceptance -TapeMode raw-message-v1
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-regressions
```

Обязательный `TapeMode` выбирает только время Java: `legacy` — NONE,
`raw-message-v1` — MESSAGE. Необязательный `CppMode` по умолчанию равен `TapeMode` и
выбирает режим C++ `legacy`, `raw-message-v1` либо `binary-catalog-v1`. Последний задаёт
явные `--wire-schema binary-catalog-v1 --tape-mode binary-catalog-v1` и lossless output8.
Время Java не выводится из режима C++ или текста descriptor. Прежние ограничения ordinary-v1/v7
и четыре проверки отказа для STREAM/MESSAGE в старых режимах сохраняются.

[Отчёт этапа](../../../tests/conformance/protocol/real-tape/MILESTONE-1.ru.md) сохраняет
видимыми STREAM/MESSAGE файлы обоих producer pins. Реальный output8 artifact должен
успешно прочитать их и частный корпус до общей приёмки. Финальный отчёт именованного корпуса
фиксирует эти фактические успехи; спроектированная строка output8 остаётся отдельной от
доказательств исполнения production C++8.

## Мост обычного подмножества output8

Java-параметр `--cpp-projection ordinary-v1|binary-catalog-v1` по умолчанию равен ordinary-v1;
PowerShell выбирает его по `CppMode`. Версия8 требует точный header из пяти столбцов и
ровно 14 столбцов каждой строки `wire-data-v8`. Значение содержит ровно
`fullId:raw:units:bytes`. Мост принимает только прежние восемь полных ID, канонические
signed raw и UTF-16/null/empty, bytes `-`, flags строго `0`, removalTime строго `-`.
Проверка выполняется до построения прежнего comparison view. Новые полные типы, bytes,
ненулевые flags и removal metadata отвергаются даже для пустого payload или removal time 0.

Каждый descriptor-v8 содержит ровно девять ограниченных столбцов, канонические presence
bits/UTF-16 hex и send mask только из четырёх известных family bits. Это проверка грамматики,
а не новое равенство вспомогательных свойств descriptor. Для NONE до удаления трёх timing
columns обязательны отсутствующий timestamp и нулевые sequence/eventTime. MESSAGE точно
сравнивает все три. Source ID, frame/order/generation, полные schema snapshots и конечные
счётчики остаются точными. Пустой broad description batch не меняет поколение; совместимая
непустая замена sender публикует новое поколение и сохраняет union имён/типов/порядка/defaults.
Для старого v7 правила пустого batch и replacement не меняются.

Проверяйте designed-грамматику отдельно от приёмки фактического артефакта:

```powershell
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-designed -DesignedV8
# Requires an actual binary-catalog-v1/output8 C++ artifact:
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/tape-acceptance-binary -BinaryCatalog
& ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 `
    -InputFile $tape -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk `
    -Runner $runner -RunDirectory .work/tape-acceptance-binary-file `
    -TapeMode raw-message-v1 -CppMode binary-catalog-v1
```

Без этих switches тест сохраняет прежние 44 фактические v7 проверки. DesignedV8 добавляет
явно помеченные производные от реального v7 и строгие positive/negative controls; metadata
явно отмечает отсутствие production C++8 исполнения. BinaryCatalog добавляет два реальных
успешных producer STREAM/MESSAGE случая, сохраняя все четыре старых отказа.

| Параметр | Значение по умолчанию и ограничение |
| --- | --- |
| `CppMode` | `TapeMode`; legacy/raw-message-v1/binary-catalog-v1, независимо от времени Java |
| `MaxInputBytes` | 8 MiB; 1 байт..64 MiB |
| `MaxRecords` | 1 000 000; 1..10 000 000 wire-записей |
| `MaxOutputBytes` | 256 MiB; 1 байт..1 GiB на каждый поток процесса и C++ projection |
| `TimeoutSeconds` | 60; 1..300 секунд на корень native ownership, затем не более 1 секунды на подтверждённую очистку |
| `HeapMiB` | 128; 32..1024 MiB на Java runtime |

Java дополнительно ограничивает строку TSV 4 MiB, описания — 4096, исходные поля — 65 536,
суммарные units имён — 1 048 576. C++ получает явные конечные бюджеты входа, вывода, записей,
полей/значений, frames и timing; остальные ограничения библиотеки сохраняются. Размер
raw-файлов проверяется Java worker каждые 20 мс с возможным превышением между проверками.
Heap worker — 32 MiB, его bootstrap compiler — 128 MiB; runner diagnostics ограничены
отдельными 8 MiB. Deadline PowerShell действует до выхода владельца и завершения потоков.
Это логические
ограничения и размер JVM heap, а не измеренный пик RSS.

Код 0 сравнения требует полного равенства и наблюдаемого чистого EOF. Ошибки, timeout,
исчерпание вывода, изменение входа/исходника или неверная настройка дают исключение/ненулевой
код. После незавершённого запуска `metadata.json` содержит `INCOMPLETE`. Статус теста
`PASS-WITH-NAMED-GATES` означает прохождение положительных и отрицательных проверок старого
профиля; сам по себе он не подтверждает broad STREAM/MESSAGE. `PASS-WITH-BINARY-CATALOG-NAMED-CASES` добавляет только два
фактических переносимых v8 случая; полная частная приёмка остаётся отдельным требованием.
Каталоги не переиспользуются. При timeout/превышении вывода
останавливаются только собственные дочерние процессы.

Runner создаёт Windows job до исполнения JVM. Внутренний [RawChild.java](RawChild.java)
направляет реальные stdout/stderr C++ и Java напрямую в отдельные файлы через
`ProcessBuilder.redirectOutput/redirectError`, сохраняя точные байты. После выхода target
или исчерпания ресурса он записывает небольшой terminal status и завершается. Native
владелец убирает оставшихся потомков, включая grandchildren после выхода их родителя.
UTC diagnostics сохраняются отдельно. Только bootstrap compiler worker использует
объединённый UTC-текст, который никогда не считается raw C++ output. До/после записываются
и проверяются hashes обоих Java sources, worker/oracle classes, source/artifact runner.

## Независимость оракула и завершение

[RealTapeAcceptance.java](RealTapeAcceptance.java) использует неизменённые закреплённые
production `FileReader` и `BinaryFileQTPParser`. Protected hooks наблюдают описания и
исходные sender ID; имена и значения C++ не строят ожидания Java. Discovery использует
пустую схему и настоящее пропускание неизвестных записей production parser. В нём выбрано
NONE-время: при MESSAGE закреплённый parser может разыменовать null cursor пропущенной
записи. Discovery подтверждает только инвентарь схемы.

Проход значений создаёт собственную обычную Java-схему из наблюдённых описаний, точных
serial classes и физических VOID tails для WideDecimal. Каждая операция дескриптора
должна быть прямой INT/LONG/OBJ. Конвертеры, встроенные event fields и неподдерживаемые
полные типы явно отвергаются. Ordinary-v1 сохраняет отказ от replacement;
binary-catalog-v1 принимает только описанные выше совместимые обычные замены. Очередь дескрипторов сохраняет
wire-порядок для STREAM callback; borrowed cursors читаются только внутри callback.
Ранние записи используют число полей своего поколения. Отсутствующие исходные поля
получают production defaults raw-zero/null.

Production `FileReader` перехватывает часть IOExceptions; возврат и равенство размеров
недостаточны. URL handler внутри одного дочернего JVM подаёт один фиксированный поток
реального файла и наблюдает EOF `-1`, байты, открытия/закрытия и ошибки чтения/закрытия.
Явно выбраны BINARY, compression NONE, MAX_SPEED, режим времени; sidecar/cycle/resync и
сетевой источник исключены. Это **production parser с инструментированным источником**.
Отдельный `FileReader` по обычному файловому пути должен дать те же строки и schema/data
hashes; у этого прохода нет отдельно наблюдаемого EOF hook.

Native binary-catalog inspection подтверждает чтение, физический EOF, reader finish,
запись и финальный flush через borrowed stream APIs. Закрытие native-потока его владельцем
до завершения этим не подтверждается; интеграция owned-source close относится к дальнейшей
работе с источниками. Явное доказательство открытия/закрытия/ошибок Java-источника является
отдельным более сильным наблюдением и сохраняется обязательным.

Проверяются порядковые номера/число frames и контекст публикации schema/data. Вспомогательные
frame offsets, message-type/payload columns, header properties и отдельные диагностические
heartbeat/timing/marker rows не проецируются для равенства. Timing columns записей
сравниваются точно. Остальные envelope-контракты проверяются существующими library tests;
компаратор не доказывает самостоятельно каждую диагностическую строку.

Компаратор читает C++ как ограниченный ASCII TSV, точно сравнивает ожидаемые metadata/data,
отвергает лишние записи projection, проверяет конечные счётчики и отсутствие байтов после
`complete`. Хеши суммируют уже точно сравнённые строки. SHA-256 входа проверяется до/после;
metadata сохраняет хеши Java/помощника/launcher/inspector/JDK, commit upstream, полный
инвентарь файлов classpath, ограничения, команды, UTC-интервалы и исходные потоки.
Частные пути, имена и значения могут попасть в логи; храните их только внутри `.work`.

## Регрессии и источники

[Test-Acceptance.ps1](Test-Acceptance.ps1) сравнивает compact и полные Quote NONE обоих
producer pins, смешанные восемь типов STREAM/RAW и перестановку/поздний sender. Мутации
проверяют raw value/type, символ, sender/local ID, поколение, порядок, лишние записи,
тип схемы, исходную привязку и все три timing-столбца. Проверяются обрезания, отказ для
ненулевых flags, бюджеты входа/записей/вывода, настоящий закрытый native sink, одинаковая
справка без аргументов/с ключом без рабочих входов, поздние ошибки чтения и закрытия.
При поздней ошибке сначала возвращаются все физические байты файла, затем ошибка вместо
EOF: одно равенство размеров не может ошибочно пройти проверку.
[Test-ProcessOwnership.ps1](Test-ProcessOwnership.ps1) дополнительно воспроизводит выход
родителя с унаследованными потоками child, deadline работающего root в одну секунду и
исчерпание raw output. Все записанные собственные parent/child/worker/runner PID должны
отсутствовать после возврата. Намеренно закрытый native output pipe остаётся отдельным
прямым I/O-тестом inspector и не считается доказательством очистки дерева процессов.

Первичные источники закреплённого commit:

- [`FileReader.createParser`, `readInto`, `process`, `tryOpenFile`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-file/src/main/java/com/devexperts/qd/qtp/file/FileReader.java).
- [`BinaryQTPParser.parseDescribeRecords`, `remapRecord`, `parseData`, message hooks](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java).
- [`BinaryRecordDesc`: protected copy/fields, `readRecord` и прямые операции](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java).
- [`MessageConsumerAdapter.expandRawData`](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageConsumerAdapter.java)
  определяет TICKER/STREAM/HISTORY fanout; wire-записи сравниваются один раз через STREAM.

[Отчёт этапа](../../../tests/conformance/protocol/real-tape/MILESTONE-1.ru.md) перечисляет
выполненные проверки и ограничения. Общая поддержка indexed schemas, десериализации объектов,
ненулевых flags, сжатия, sidecar, split-file и бизнес-преобразований не заявляется.

## Явное сравнение владеющего источника

`-SourceProfile self-contained-segments-v1` выбирает output9 для одного несжатого
ordinary-источника MESSAGE. `TapeMode` обязан быть `raw-message-v1`; пропущенный
`CppMode` становится `binary-catalog-v1`, а явное конфликтующее значение
отвергается до работы с входом. Обвязка передаёт native `--time-policy message`
явно. Сравнение ограничено прежними восемью full field ID, нулевыми flags,
отсутствием FIELD/removal metadata и ordinary-схемами. Остальные профили
времени/значений/контейнеров проверяются отдельными production fixtures.

```powershell
& ./tests/conformance/protocol/real-tape/Compare-RealTapes.ps1 `
    -InputFile $tape -Inspector $inspector -UpstreamRoot $lockedCheckout `
    -JavaHome $oracleJdk -Runner $runner -RunDirectory .work/source-tape-acceptance `
    -TapeMode raw-message-v1 -SourceProfile self-contained-segments-v1 -OracleProfile baseline
& ./tools/upstream/real-tape-acceptance/Test-Acceptance.ps1 `
    -Inspector $inspector -UpstreamRoot $lockedCheckout -JavaHome $oracleJdk -Runner $runner `
    -RunDirectory .work/source-tape-regressions -DesignedV8 -BinaryCatalog -SourceProfile
```

`-OracleProfile baseline` используется по умолчанию и сохраняет проверку
`upstream.lock`. Явное `current` выбирает отдельно закреплённую точную версию
parser из [producer.lock.json](../real-tape-corpus/producer.lock.json); передайте
её чистый заранее собранный checkout как `UpstreamRoot`. Поиск версии и сборка
upstream не запускаются. Выбранный pin и hash его источника записываются и
проверяются после запуска.

Строгий source bridge проверяет все 27 ячеек данных, native identity источника,
порядок вхождений/записей, точный список ресурсов, source-end и итоговое завершение.
Резерв завершения 65 536 байт и точные байты format/native-identity строк
исключаются из per-source допуска вывода; `MaxOutputBytes` по-прежнему
ограничивает весь native-вывод. Source-profile требует положительного допуска
публикации после всех трёх резервов.

Production heartbeat callbacks задают истину для сырого timestamp; packed cursor
time Java и его production-проекция миллисекунд сравниваются отдельно. Новый
digest с числом записей включает явный выбор эффективного времени и отсутствие
FIELD. Фактический production descriptor обязан объявить точное
`message`/`MESSAGE` до данных; совместимые совпадающие обновления descriptor
остаются валидными. Инструментированный источник доказывает EOF и read/close
счётчики; проход с буквальным именем файла отдельно не инструментируется.
Нужен успех обеих полных проекций и процесса. Сырые пути/строки/логи остаются
приватными. См. [именованные проверки](../../../tests/conformance/protocol/real-tape/VALIDATION.ru.md).
