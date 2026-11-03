# Консольные примеры подписок

[English version](README.md). Английская версия — нормативная.

Начните с готового [консольного приложения Quote](receiver/quote.cpp). Оно получает настоящий binary
QTP через публичные `TcpRecovery` и `SubscriptionConsumer`, проецирует полную встроенную восьмиполевую
Quote и печатает bid/ask raw price/size. Это приложение-потребитель установленной библиотеки;
в его executable нет тестового peer или test framework.

## Сборка и запуск Quote-приёмника

Сначала соберите/установите CLIENT, RUNTIME и RECOVERY с точными зависимостями из
[инструкции сборки](../../docs/CPP-BUILD.ru.md). Затем используйте существующий отдельный CMake-проект:

```powershell
cmake -S examples/subscriptions/receiver -B build/quote-subscriber '-Dqdcxx_DIR=C:/dev/qdcxx/install/lib/cmake/qdcxx'
cmake --build build/quote-subscriber --config Release
```

Подставьте свой prefix/package library directory. Установка с TLS дополнительно требует точный
`ORG_TTLDTOR_QDCXX_OPENSSL_ROOT` при конфигурации потребителя; это не включает TLS в данном plain
приложении. Visual Studio помещает executable в `Release`; одноконфигурационные генераторы — обычно
прямо в каталог сборки. Для Visual Studio запустите из корня checkout:

```powershell
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --help
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --help-ru
./build/quote-subscriber/Release/qdcxx-quote-subscription.exe --host 127.0.0.1 --port 19123 --symbol IBM --duration-ms 5000
```

Для одноконфигурационной Windows-сборки используйте `./build/quote-subscriber/qdcxx-quote-subscription.exe`;
для Linux обычно `./build/quote-subscriber/qdcxx-quote-subscription`. При запуске из каталога executable
PowerShell требует `./qdcxx-quote-subscription.exe`. Короткие имена в остальных примерах предполагают
наличие в `PATH`; иначе укажите фактический полный путь.

Без аргументов выводится та же английская справка без schema/bootstrap/network work. Совместимый
источник должен уже слушать выбранный порт и поддерживать Quote/символ. Сам пример сервис не запускает.
Для DNS-имени задайте явный numeric IPv4 `--dns-server` и при необходимости `--dns-port`; системный
resolver не обнаруживается. Named route добавляет описанные 80 MiB логического резерва запрошенной
памяти и отдельную консервативную DNS work; numeric route это обходит. Это не предел RSS/времени.

Конечные исходные настройки находятся в [quote.cpp](receiver/quote.cpp), [settings.hpp](receiver/settings.hpp)
и [arguments.hpp](receiver/arguments.hpp). Приложение создаёт schema и endpoint snapshot, открывает
receiver на STREAM queue, передаёт desired set, затем создаёт/запускает/продвигает recovery на потоке
владельца. Progress и shutdown используют одинаковый конечный work profile.

Явный mapping budget допускает 128 точных альтернатив типов, покрывая текущие 72 Quote
альтернативы без изменения default восьмиполевой схемы. Это low-level приложение сохраняет
диагностический raw-decimal вывод; обычный [пример Feed](../feed_quote/README.ru.md) показывает
business getters по [контракту преобразования полей](../../spec/events/FIELD-CONVERSIONS.ru.md).

В настоящем `QuoteConsumer::onBatch` параметр `batch` — заимствованный публичный `SubscriptionBatch`;
цикл проецирует записи с явными конечными credits:

```cpp
auto work = projectionWork();
auto projected = batch.project(index, work);
```

Это фрагмент полного приложения: `index` — индекс текущей записи, `projectionWork()` — объявленный
помощник из `settings.hpp`. Полный callback проверяет ошибку projection и Quote variant до чтения,
проверяет stdout и возвращает существующее whole-batch action. Projected Quote владеет значениями;
batch/projection service действуют только внутри callback.
[Руководство API приёмника](../../docs/SUBSCRIPTION-RECEIVER.ru.md) описывает сохранение generic batches,
добавление независимых logical clients и выбор других семейств без удержания заимствованного фасада.

`--maximum-records N` необязательно ограничивает печать 1–1000000 строками; duration остаётся
безусловной границей. Пример может перестать печатать внутри callback batch. У отдельного CLI
`--max-records` задаёт цель с сохранением целого опубликованного batch. Максимум три новые попытки
используют фиксированную задержку 250 мс. Duration/row cutoff запрашивает local drain (`--drain-ms`,
default 1000); сигналы и ошибки — abort. Exit: 0 для завершённого drain/справки, 1 для ошибки,
2 для неверных аргументов, 130 для interruption. Local drain не доказывает peer ACK или whole-network flush.

Напечатанные `.raw` — точные WideDecimal bits; raw zero означает NaN. Для business values
используйте `toString()` либо `toDouble()`. Пример остаётся plain TCP/STREAM. Для Quote/Profile/HISTORY,
external JSON, проверяемого TLS и необязательных Basic-файлов есть
[руководство recovery CLI](../../docs/SUBSCRIPTIONS.ru.md) и [справочник](../../tools/subscribe/README.ru.md).
Машинные форматы и границы TLS-окружения определены в
[recovery output](../../spec/cli/RECOVERY-OUTPUT.ru.md) и
[замкнутом TLS-профиле](../../spec/cli/TLS-HOST-PROFILE.ru.md).

<a id="программный-пример-управляемого-stream"></a>
<a id="example-subscriptions-legacy-stream-ru"></a>

## Прежний вызываемый STREAM-пример

Остальные разделы описывают существующий `runStream` и вызываемую библиотеку примера;
их configuration/output/terminal rules отделены от приложения-приёмника выше.

[example.cpp](example.cpp) — небольшой вызываемый пример без второго операционного CLI.
`runSubscriptionExample` принимает заранее созданный main-thread `RuntimeBootstrap`, валидированную
схему вызывающей стороны, явный порт управляемого IPv4 loopback peer и синхронный output stream.
Он запрашивает точный `ALPHA` у local record zero и прекращает новый приём после callback target.
Callback примера печатает local/sender identities и число полей. Все исходные generic fields доступны
через `BatchRecord`; prepared standard-event projection выбирается отдельно.

[stream_client.hpp](stream_client.hpp) / [stream_client.cpp](stream_client.cpp) показывают полную
композицию приложения поверх реальных runtime и dispatcher. Это исходный код приложения/примера,
а не новый установленный библиотечный facade. Точные UTF-16 selectors, включая пустые/NUL/непарные
units, и ordinary схемы вызывающей стороны отделены от
[ограниченного ASCII/built-in профиля CLI](../../tools/subscribe/README.ru.md).

## Сборка и выполнение реального примера

Соберите/установите production package с явно включёнными CLIENT и RUNTIME и переданными
квалифицированными SDK roots согласно [контракту runtime](../../spec/transport/TCP-RUNTIME.ru.md).
Перемещённая установка экспортирует `qdcxx_client`, `qdcxx_transport`, `qdcxx_event_mapping`,
`qdcxx_quote` и `qdcxx_profile`; сами pure event targets не приобретают зависимости runtime.

Пример независимо компилируется против такой установки:

```text
cmake -S examples/subscriptions -B build/subscription-example -Dqdcxx_DIR=/path/to/prefix/lib/cmake/qdcxx
cmake --build build/subscription-example
```

Это собирает библиотеку вызываемого примера. Для выполнения его реального generic callback и обоих
существующих prepared standard-event путей с детерминированным собственным TCP peer запустите
независимого consumer:

```text
cmake -S tests/conformance/connect_consumer -B build/subscription-consumer -Dqdcxx_DIR=/path/to/relocated/prefix/lib/cmake/qdcxx
cmake --build build/subscription-consumer
ctest --test-dir build/subscription-consumer --output-on-failure
```

Если install library directory отличается от `lib`, используйте настроенный каталог. Consumer не
подключает исходный `include/`, не ищет test framework, не получает SDK и не подключается наружу.
Он компилирует tracked application/example и controlled peer source против production installed
headers/targets. Его внутренний runner не является публичной CLI-утилитой.

## Владение, работа и завершение

Создайте `RuntimeBootstrap` на application main thread до других application threads. Завершите
владельцев runtime и test/application threads до его уничтожения. Работа driver/runtime/dispatcher
и уничтожение связанных protocol owners выполняются в вызывающем потоке-владельце.

`runStream` проверяет изначально пустой выходной slot `std::optional<ClientHandle>` до driver setup.
После fallible owner setup он без исключения публикует копию control до progress/callbacks.
Сохраните объект slot до return. Callbacks могут вызвать `requestStop`; для cross-thread вызовов
нужно публиковать отдельные копии с собственной синхронизацией. Очистка slot не останавливает driver,
который держит собственную public control reference. После return retained handle содержит только
detached command port/terminal, без dispatcher, callable или socket.

Каждое полное runtime event принимается до последующих ordinary commands. Общий owner-turn бюджет
охватывает pre-poll local work и post-admission retry. `ownerOperationsPerTurn` явно настраивается
в 1..10000, по умолчанию 10000; исчерпание повторяет локальную работу до нового poll/removal, при этом
cutoff и interrupt продолжают проверяться. Factory/setup credits отдельные. Остальные finite
schema/storage/work bounds видны в исходном коде; это не гарантии RSS, throughput или жёсткой задержки.

Duration, data target и interrupt прекращают новый source admission и явно отменяют TCP run/output.
Graceful local retirement заканчивает уже принятый допустимый префикс, который может содержать больше
records, чем target. Local и native terminal evidence остаются отдельными в `StreamResult`; pending
output, discarded input и Windows unknown write acceptance не скрываются за local drained outcome.
Native code 1236 после Windows abort остаётся видимой ошибкой. Sink/callback failure не повторяется.
Для произвольно блокирующих callback/sink и OS completion нет жёсткой гарантии времени.

Пример не реализует TLS/authentication, reconnect, wildcard, MUX, TICKER/HISTORY или полную
subscription lifecycle. Поддерживаемый этап описан в [руководстве](../../docs/SUBSCRIPTIONS.ru.md).
