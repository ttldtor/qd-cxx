# Программный пример управляемого STREAM

[English version](README.md). Английская версия — нормативная.

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
