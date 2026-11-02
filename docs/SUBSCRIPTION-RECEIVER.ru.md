# Получение generic-записей и подготовленных событий

[English version](SUBSCRIPTION-RECEIVER.md). Нормативный текст — EN.

Используйте `qd/client/subscription_receiver.hpp`, когда приложению нужны независимые
логические подписки на существующей очереди STREAM, TICKER или HISTORY.
Generic-записи сохраняются полностью; явно выбранные записи также можно проецировать
через существующие prepared-отображения Quote/Profile. Приложение предоставляет схему,
очередь и сериализованный цикл выполнения. [Recovery](../spec/transport/RECOVERY.ru.md)
обеспечивает существующий жизненный цикл TCP/TLS-соединения и replay; получатель задаёт
границу callback.

Полное консольное приложение находится в
[исходнике подписчика Quote](../examples/subscriptions/receiver/quote.cpp), его явные
[ограниченные настройки](../examples/subscriptions/receiver/settings.hpp) и
[обработка аргументов/справки](../examples/subscriptions/receiver/arguments.hpp), а также
[отдельный CMake-проект](../examples/subscriptions/receiver/CMakeLists.txt) доступны рядом.
Приложение использует plain TCP и полную встроенную восьмиполевую схему Quote.
Явный canonical exact-symbol профиль не меняет значения по умолчанию библиотеки.
TLS и custom indexed-схемы используют собственные существующие контракты настройки.

## Сборка и запуск консольного приложения

Сначала соберите и установите библиотеку с `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` и
`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON` по [инструкции сборки](CPP-BUILD.ru.md).
Затем настройте пример как независимого потребителя установленного пакета.
Например, в PowerShell из дерева исходников замените каталог установки своим:

```powershell
cmake -S examples/subscriptions/receiver -B build/quote-subscriber -Dqdcxx_DIR=C:/dev/qdcxx/install/lib/cmake/qdcxx
cmake --build build/quote-subscriber --config Release
```

Используйте каталог executable выбранного генератора. Генератор с одной конфигурацией
помещает executable прямо в `build/quote-subscriber`, Visual Studio обычно использует
подкаталог `Release`. Установленному пакету со включённым TLS также нужен квалифицированный
`ORG_TTLDTOR_QDCXX_OPENSSL_ROOT`, даже для этого приложения с plain linkage;
см. [настройку пакета/TLS](CPP-BUILD.ru.md).

`qdcxx-quote-subscription --help` и отсутствие аргументов показывают одинаковую английскую
справку без загрузки схемы, bootstrap процесса и сети. `--help-ru` показывает русскую справку.
Для получения данных от уже запущенного контролируемого QTP-источника:

```text
qdcxx-quote-subscription --host 127.0.0.1 --port 19123 --symbol IBM --duration-ms 5000
```

Порт приведён для примера; встроенного сервиса на нём нет. Источник должен объявлять
поддержанный binary QTP-контракт и совместимое описание Quote. Именованный host требует
явного числового `--dns-server`; приложение не выбирает resolver из окружения или системной
конфигурации. Пределы runtime и значения timeout/retry явно заданы в настройках примера
и встроенной справке. Пример локального запуска не утверждает наличие аккаунта,
охват поставщика или доступность публичного live-feed.

Для именованных host пример добавляет документированный DNS SDK envelope 80 MiB
к начальной логической резервации requested payload. Отдельные DNS work credits
соответствуют консервативной оценке из [ограничений DNS](../spec/transport/DNS-RESOURCES.ru.md).
Эти величины не являются измерениями RSS, инструкций CPU или гарантией задержки.
Числовые адреса обходят этот DNS-профиль. Обе настройки выбираются до создания endpoint;
при завершении используется тот же профиль работы за один poll.

Напечатанные `bid.raw`, `bidSize.raw`, `ask.raw` и `askSize.raw` — точные биты
представления `WideDecimal`. Raw zero обозначает NaN, а не нулевую цену или объём.
Для отображения бизнес-значений используйте `WideDecimal::toDouble()` с приблизительным
результатом floating-point или `toString()` для ограниченного текстового представления.

## Регистрация callback приложения

1. Создайте одну проверенную неизменяемую `LocalSchema` для endpoint. Используйте копии
   того же происхождения схемы при создании очередей, prepared-привязок и subscription keys.
2. Создайте существующую очередь выбранного семейства и selector policy. Выберите конечные
   общие и поклиентские ёмкости под ожидаемую нагрузку.
3. Реализуйте `SubscriptionConsumer::onBatch`. Читайте generic-значения через `batch.records()`;
   для явно подготовленного Quote/Profile вызывайте `batch.project(index, remainingWork)`.
   Используйте `std::get_if` для владеющего result variant. Невыбранные записи дают `monostate`.
4. Вызовите `openSubscriptionReceiver` с явными bindings, limits и work подготовки.
   Неверные typed-отображения отвергаются до открытия регистрации. Generic-only получатель
   использует пустой список bindings и не требует бизнес-отображения.
5. Передайте полный начальный набор через `replace` возвращённого handle. Наблюдайте
   фактический command ticket, затем используйте обычные `add`, `remove`, `replace` или
   `clear` для последующих изменений. Каждая логическая подписка хранит собственный handle.
6. Продвигайте существующую очередь или объединяющий recovery owner на его сериализованном
   потоке-владельце. Получатель не запускает поток, endpoint или цикл планирования.

Существующий `MuxHandle` также предоставляет pause/resume и stop. Typed-отображение
сохраняет дополнительные поля и generic metadata. Для HISTORY читайте знаковый `rawTime`,
produced flags и необязательные trigger/source metadata самой записи: биты индекса независимы
от времени бизнес-события. Только явный `FIELD_METADATA` может дать необязательное время
проекции из фактических исходных FIELD metadata.

## Сохранение результатов и безопасная остановка

Фасад пакета и служба prepared-проекции действительны только внутри callback.
`batch.retain()` сохраняет существующий независимый lease generic-пакета.
Возвращённый Quote/Profile уже владеет проекцией и может пережить callback, очередь и recovery.
Сохранение generic backing не продлевает жизнь фасада пакета или mapping plan.
Для последующей проекции явно сохраните существующий prepared mapper и вызовите его `mapView`.

Один callback подтверждает целый пакет существующим `BatchAction`; приложение управляет
своим обходом внутри него. Stop, уничтожение и замена публичного recovery owner внутри callback
сохраняют существующий callback barrier. Исключения сохраняют поклиентскую изоляцию очереди.
Служба проекции не добавляет callback.

`requestDrain(deadline)` recovery отсекает native-приём/output и пытается доставить уже принятый
очередью локальный префикс при последующих явных poll. Проверяйте фактический terminal outcome
после завершения принадлежащей работы. Deadline может повысить остановку до abort; это не
доказательство получения peer, сброса wire-output или полного upstream HISTORY snapshot.
`requestStop()` и деструктор сохраняют немедленную abort-семантику.

Если HISTORY-очередью владеет Recovery, вызывайте его `historyRangeStatus(handle, key,
work)` в создающем потоке после возврата `poll()`. Успешное detached-наблюдение содержит
`completeDeliveredRange` существующей очереди для точных bound и epoch. Удержанный вход,
reset/reconnect/cutoff или недоступная аутентификация запрещают текущее наблюдение;
сообщайте unknown вместо повторного использования прежнего complete. Доступность и work
определяет [контракт наблюдения recovery](../spec/transport/RECOVERY.ru.md). Raw progress,
флаги события и локальный drain сами по себе не доказывают полноту диапазона.

[Контракт получателя](../spec/subscriptions/RECEIVER.ru.md) описывает точные ошибки,
расход work и совместимость. [Контракт очереди](../spec/mux/QUEUES.ru.md),
[prepared-отображения](../spec/events/EVENT-MAPPING.ru.md) и
[контракт recovery](../spec/transport/RECOVERY.ru.md) задают нижележащие границы.
