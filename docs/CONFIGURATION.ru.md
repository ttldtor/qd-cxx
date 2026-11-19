# Владение конфигурацией и приоритет источников

[English](CONFIGURATION.md) · [Справочник настроек](CONFIGURATION-REFERENCE.ru.md) · [Свойства endpoint](ENDPOINT-CONFIGURATION.ru.md)

Конфигурация принадлежит использующему её компоненту. Разрешайте источники на явной границе,
проверяйте типизированные значения и передавайте их компоненту. Core, кодеки и отображения событий
не ищут файлы приложения, переменные окружения и изменяемые свойства процесса.

## Выбор границы конфигурации

| Граница | Текущее поведение | Подробности |
| --- | --- | --- |
| Публичный Feed API | `api::EndpointOptions` задаёт типизированную политику; `api::EndpointConfiguration` разрешает поддержанные текстовые слои до создания endpoint. Типизированный проверяемый TLS поддерживается квалифицированной сборкой с TLS. | [Конфигурация endpoint](ENDPOINT-CONFIGURATION.ru.md), [Feed API](FEED-API.ru.md) |
| Низкоуровневый контролируемый STREAM-пример | `client::EndpointSettings` преобразуется в неизменяемый снимок `client::EndpointOptions`. Его драйвер принимает числовой host, plain TCP и одну попытку. | Разделы низкоуровневого снимка ниже |
| Получение схемы | Явная загрузка нативного JSON или обнаружение wire-схемы создаёт проверенную владеющую схему. Потребители получают её отдельно от загрузки источника. | [Обычный JSON](../spec/schema/NATIVE-JSON.ru.md), [binary JSON](../spec/schema/NATIVE-BINARY-JSON.ru.md), [wire-инвентаризация](../spec/schema/WIRE-INVENTORY.ru.md) |
| Остальные компоненты и утилиты | Типизированные настройки, вложенные ёмкости и явные рабочие кредиты имеют собственных владельцев. Наличие поля не включает другую подсистему. | [Справочник полей](CONFIGURATION-REFERENCE.ru.md), [инструменты разработчика](../tools/dev/README.ru.md) |

Два типа `EndpointOptions` относятся к разным API и пространствам имён. Ограничения низкоуровневого
plain-примера не описывают высокоуровневый Feed API. Общего загрузчика Java properties,
единой цепочки приоритетов процесса и универсального изменения настроек во время работы нет.

## Разрешение источников до активации

Для `api::EndpointConfiguration::create` приоритет от низшего к высшему таков:

1. Типизированный исходный `api::EndpointOptions`, переданный явно или созданный по умолчанию.
2. Явный слой defaults вызывающего кода.
3. Явный слой properties вызывающего кода.
4. Поддержанные свойства выбранного адреса в порядке блоков.

Внутри слоя побеждает последнее повторение. Разбирается только выбранный адрес. Каждая переданная
запись, включая затенённый дубликат, расходует входной бюджет и должна иметь поддержанный ключ
и ограниченный текст; выбранные значения проходят типизированную семантическую проверку.
False, ноль, отсутствие и пустой текст сохраняют смысл конкретного ключа. Например, пустое нативное
имя задано явно, а пустой inline `requestedAggregationPeriod` сбрасывает запрос. Этот порядок
относится к данному resolver и не устанавливает приоритет всех семейств Java properties.

Создание разрешает и проверяет вход до публикации слота endpoint и допуска подключения.
Оно не выполняет DNS, сетевой I/O, поиск файлов или callback приложения. `Endpoint::connect()`
отдельно допускает активацию. Inline-свойства в последующем `connect(address)` отклоняются
и не заменяют неизменяемую политику создания. См. [ключи и точные ошибки](ENDPOINT-CONFIGURATION.ru.md).

## Владение, момент чтения и наблюдения

Заимствованные строки свойств должны жить до возврата `create`; успешная конфигурация владеет
сохранёнными значениями. `options()` возвращает заимствованный const-view, пока жив удерживающий
не moved-from handle. `snapshot()` владеет копиями и не удерживает runtime или соединение.
Независимые handles допускают параллельное чтение и копирование; замена или уничтожение одного
handle требует синхронизации. Moved-from handle можно только присвоить заново или уничтожить.

`getEffectiveConfiguration()` копирует локальную политику и допущенное намерение подключения
под mutex endpoint. Запрошенная и применённая retry delay могут различаться: при одной попытке
применяется ноль. Источник свойства обозначает выигравший слой, а не подтверждение сервера.
Ревизии подключения и запрошенной агрегации обозначают соответствующие локальные изменения
управления. Исходная конфигурация остаётся неизменной, когда `setRequestedAggregationPeriod`
меняет текущий запрос endpoint. Снимок создания не является обновляемым представлением.
Действующая локальная политика не доказывает DNS resolution, согласование, доставку или
фактически применённый сервером период агрегации.

Настройки с provider или зависимостью требуют собственного контракта времени жизни. Shared ownership
credential provider удерживает его handle, но не обеспечивает синхронизацию состояния приложения,
ограниченное время callback или secure erasure. Низкоуровневый контракт ниже и [проверяемый Feed TLS](FEED-API.ru.md)
описывают свои правила вызова и завершения. Разрешение конфигурации не вызывает credential provider.
Секреты не должны попадать в обычные метки, диагностику отклонённого ввода или fixtures.

## Чтение полного нативного каталога

[Справочник настроек](CONFIGURATION-REFERENCE.ru.md) указывает декларацию, тип, единицы, исходное
значение, момент чтения, изменяемость и ошибки каждого учтённого поля. Семантика отдельных полей
отделена от сведений только по декларациям; недостающие сведения обозначены явно.
Вложенные настройки являются составными входами; рабочие кредиты расходуются; оценки требуемых
ресурсов и константы не являются настройками. Ёмкость ограничивает удерживаемое состояние,
рабочий квант — операцию или ход. Они не превращаются неявно в квоту записей за всё время работы
или предел RSS процесса.

Текущие исходные 64 входных свойства и 4/128 слотов записей Feed являются настраиваемой политикой
или ограничениями профиля, а не QTP. Число в декларации само по себе не даёт рекомендации
по производительности и не доказывает поддержку другого значения. Зарезервированные неактивные
поля, включая бюджет текста диагностики attachment, нельзя выдавать за действующие ограничения.

## Принятое развитие; реализация учитывается отдельно

Контракт для будущих загрузчиков и изменений компонентов:

- Каждая настройка имеет владельца-потребителя, точные тип/единицу/исходное значение, проверку,
  источник и ревизию. Запрошенное и применённое состояния наблюдаемы; секреты скрываются.
  Настройка объявляет применение сразу, при переподключении или после перезапуска.
  Неподдержанная настройка не выдаётся за применённую.
- Приложение явно подключает загрузчики файлов, launch options или окружения. Получение источника,
  приоритеты, типизированная проверка и применение разделены. Приоритет endpoint сохраняется;
  другое семейство свойств требует собственных доказательств и объявленного порядка. Синтаксис
  CLI или файлов здесь не вводится.
- Структурная проверка предшествует активации. Неизвестное значение enum отклоняется;
  совместимая обработка неизвестных свойств выбирается явно. В частности, принятое правило
  IPF completion policy отклоняет неизвестное enum до чтения или публикации; текущая резервная
  ветка потребителя описана отдельно в справочнике.
- Уменьшение бюджета ниже занятого состояния и действующих резервов отклоняется. Поддержанное
  отложенное уменьшение является отдельной наблюдаемой операцией. Связанные изменения во время
  работы требуют согласованного применения и явных результатов отказа; создание снимка само
  по себе этот механизм не реализует.
- Допуск развивается к байтовым бюджетам с учётом владения payload и служебных объектов.
  Кванты планирования, смысловые ограничения запросов и границы wire-формата разделены.
  Текущие ограничения 4/128 и длины символа сохраняются до квалифицированной замены их гарантий.
- Квалифицированный профиль TLS Certificate/CertificateRequest сохраняет предел 65 536 байт.
  Расширение требует проверки владельцев зависимости и ресурсов; повышения поля недостаточно.
  Обычный режим утилиты восстановления должен использовать общий байтовый профиль; сохранённый
  старый режим числовых ограничений явно называется и описывается в справке.

Это принятые требования, а не утверждение о реализации общих загрузчиков, атомарных изменений,
замены на байтовые бюджеты или миграции утилиты восстановления. Неизвестные свойства справочника
остаются неизвестными до установления контракта владельца и его проверок.

## Доказательства совместимости с Java

[Базовая проверка настроек событий](../spec/schema/CONFIGURED-EVENTS.ru.md) содержит production-
наблюдения в свежих JVM для выбранных startup, endpoint file/API, mutation и XML входов.
Явный выбор полей, nano-time shortcuts, XML defaults и программные defaults имеют разные правила.
[Oracle endpoint](../tools/upstream/endpoint-configuration-oracle/README.ru.md) описывает ещё одно
ограниченное семейство. Ни один из них не устанавливает общий приоритет Java или изменяемость
всех настроек во время работы.

Java-имена, aliases, исходные значения, единицы, устаревание и момент чтения должны прослеживаться
до закреплённых потребителей и captures под `upstream.lock`. [Wiki настроек](https://github.com/dxFeed/dxfeed-graal-cxx-api/wiki/QD-and-dxFeed-Java-API-Project-Settings)
и release notes служат для поиска. [Закреплённые release notes](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/ReleaseNotes.txt)
описывают приоритет endpoint-file scheme properties над JVM properties для этого семейства;
правило не переносится автоматически на connectors, logging, monitoring или collectors.
Нереализованные Java-семейства остаются явной работой по совместимости, а не молча принятыми
нативными свойствами.

## Явные источники схем

`loadBinaryJsonSchema` выбирает [binary profile](../spec/schema/NATIVE-BINARY-JSON.ru.md) с явными
логическими полями, lookups и indexed/time contributors. Результат владеет схемой `BINARY_FIELDS_V1`
с независимым происхождением; копии сохраняют identity, отдельные загрузки — нет. Обычный
`loadJsonSchema` сохраняет профиль v1 и его пределы. `BinaryJsonSchemaLimits` пересекает ограничения
input/count/name с parser-request bytes, логической работой и байтами двух копий модели схемы;
model bytes не являются точным пределом heap/RSS. Вход вызывающего кода, сохранённые схемы
и последующие копии остаются отдельными областями.

Первый выбор backend закреплённого simdjson может читать `SIMDJSON_FORCE_IMPLEMENTATION`.
Выбор зависимости не разрешает properties приложения; изменение чужого SDK-global состояния
остаётся вне ресурсной квалификации. XML imports, вывод units/event mappings и замена схемы
во время работы не добавляются. [Режим проверки](TAPE-INSPECT.ru.md) завершается без tape/network
работы и не добавляет HISTORY subscription facade.

Параметр inspector `--wire-schema ordinary-v1` взаимоисключается с `--schema PATH` и `--profile NAME`.
Выбор разрешается на границе CLI до tape I/O. Параметры record/field/lookup/name ограничивают
полную предварительную проверку объединения; JSON document/depth относятся только к загрузчику.
Обнаружение wire-схемы остаётся доступным без JSON. [WIRE-INVENTORY](../spec/schema/WIRE-INVENTORY.ru.md)
ограничивает обнаружение; [TAPE-INSPECT](TAPE-INSPECT.ru.md) задаёт метаданные источника версии 7.
Внешний загрузчик или адаптер событий не выбирается неявно.

## Выбранный native API снимка параметров endpoint (QDCXX-073)

Эти разделы описывают низкоуровневый `client::EndpointOptions` и драйвер контролируемого примера.
Публичный `api::EndpointOptions` с работающими TLS/reconnect имеет отдельный
[Feed-контракт](FEED-API.ru.md). Упоминания будущего потребителя ниже относятся к этому handoff.

`client::EndpointOptions` создаётся из проверенной `core::LocalSchema` и явного `client::EndpointSettings`. При создании копируются схема и переданные строки/настройки; неизменяемый снимок сохраняет provenance схемы и не удерживает объекты настроек или схемы вызывающего кода. Изменение или уничтожение входов после успешного создания не меняет снимок.

API объявлен в заголовке [`qd/client/endpoint_options.hpp`](../include/qd/client/endpoint_options.hpp) как часть опциональной возможности `qdcxx_client`. Заголовок собирается при включённом client и выключенном runtime; ему не нужны заголовки TCP runtime, TLS SDK или JSON loader. Параметры сборки и пакет описаны в действующем [руководстве по C++ сборке](CPP-BUILD.ru.md). Этот слой не ищет настройки в environment, файлах, JSON, Java properties или глобальном состоянии. Каждый endpoint получает собственный снимок явных значений: слияния приоритетов и live-переконфигурации нет.

`EndpointOptions::settings()` и `schema()` возвращают заимствованные константные представления, действительные, пока жив соответствующий удерживающий снимок и он не перемещён. Скопируйте снимок, прежде чем освобождать объект, который его удерживает. Разные handles можно читать, копировать и освобождать в разных потоках; один и тот же handle нельзя одновременно изменять или уничтожать. Перемещённый объект допускает только присваивание или уничтожение.

Выбор схемы остаётся явным: нативный вызывающий код передаёт проверенную `LocalSchema`; CLI продолжает выбирать существующую схему Quote или Profile на своей границе. API не выводит схему и не добавляет загрузку JSON.

## Настройки и ограничения

`EndpointSettings` объединяет идентификаторы endpoint, runtime/security policy, необязательные TLS handoff-данные, retry policy, timeouts и `EndpointBudgets`. Ёмкости и бюджеты работы компонентов остаются отдельными типизированными значениями; конфигурация не складывает их в общий byte total и не меняет единицы.

| Группа | Контракт |
| --- | --- |
| Host и port | Явные копируемые значения. Host непустой, не длиннее 254 ASCII bytes; запрещены NUL, `%`, URI/user-info, brackets, zone IDs и address-property syntax. Форма конфигурации принимает числовой host и ограниченные ASCII/A-label DNS-имена. Port: 1–65535. Текущий STREAM driver по-прежнему принимает только числовой host. |
| Runtime и security | `CALLER_DRIVEN_TCP` и `CREDENTIAL_FREE_PLAIN` — выбранные caller-driven значения по умолчанию. `TLS` только запрашивает policy; это не означает, что TLS runtime уже есть или peer проверен. |
| Длительность операции | Текущий controlled helper использует 5000 ms; допустимый диапазон — 1–600000 ms. Это конечный интервал операции STREAM, а не жёсткий сетевой deadline. |
| Timeouts | Раздельные DNS, connect, read, write и poll. Текущие controlled defaults: DNS/connect 1000 ms, read/write равны длительности операции, poll 1 ms. Указанные значения должны быть положительными и помещаться в `INT_MAX` milliseconds. DNS-настройки являются только handoff-данными для driver с числовым endpoint. |
| Retry | Текущие defaults — одна попытка и нулевая задержка, то есть reconnect нет. Больше одной попытки — только handoff; требуется положительная задержка, проходящая checked duration arithmetic. Текущий driver отвергает retry более одной попытки. |
| TCP capacities | `inputBytes` и `writeBytes` — byte capacities; `addresses` — количество адресов; `cnameDepth` — число CNAME hops; `contextBytes`, `handlerBytes` и `residentBytes` — logical byte capacities. Текущий controlled example использует `{8192, 8192, 16, 4, 65536, 8192, 8388608}` соответственно. |
| Runtime-turn work (`EndpointRuntimeWork`) | Семь отдельных осей: `steps` и `handlers` (количество work items), `socketBytes` (bytes), `dnsCalls` (calls), `dnsPackets` (packets), `dnsBytes` (bytes) и `dnsUnits` (DNS work units). Текущий numeric-only controlled example использует `{128, 128, 10000000, 0, 0, 0, 0}` в этом порядке; нулевые DNS credits не включают DNS. |
| Другие ограниченные области | `EndpointBudgets` содержит существующие schema, layout, writer, session, desired-state, collector, client и TCP capacities, а также writer-preparation, submit, protocol, dispatch и runtime-turn work. Значения и единицы сохраняют контракты соответствующих компонентов. |

`controlledStreamSettings()` возвращает заполненные значения текущего controlled example, но без port. Перед созданием снимка вызывающий код должен задать правильные host и port и передать схему. Текущие C++ defaults не обещают совместимость с Java. Настройка системного socket send buffer остаётся у ОС и не становится новой опцией endpoint.

Снимок фиксирует настройки при создании. Driver читает неизменяемые лимиты при открытии owner turn. Work credits пополняются только на существующей явной границе application turn, а не при каждом poll или dispatch. Для продолжающегося feed нет lifetime byte/record quota. Память конфигурации и схемы относится к отдельной логической области и не является измерением runtime/session storage или RSS процесса.

## Использование снимка с controlled driver

Новая перегрузка `runStream` принимает schema, host и port из снимка. Поля выбора операции в `StreamOptions` — record/symbol selection, duration, removal, target и signal — остаются в этой структуре. Существующие сигнатура `runStream`, layout aggregate `StreamOptions`, defaults, порядок проверки и обработка ошибок старой перегрузки сохраняются.

В новой перегрузке пустой `StreamOptions::host` и нулевой port означают, что вызывающий код не дублировал эти значения. Если host или port указаны, они должны точно совпасть со снимком. Несовпадение отклоняется при проверке конфигурации до обращения этого driver к переданному bootstrap или выполнения им собственного I/O; это ничего не говорит о действиях, начатых вызывающей стороной раньше. Старая перегрузка продолжает требовать и использовать прежние host/port.

Кредит operations для dispatch turn из снимка ограничивается неизменённым `StreamOptions::ownerOperationsPerTurn`: текущий диапазон controlled driver — 1–10000, default — 10000. Результат — меньшее из двух значений; legacy cap не может увеличить кредит снимка. Все остальные work axes и вложенные desired/collector credits остаются заданными в снимке, в том числе на применимых stop, drain и failure путях. Второй настройки owner-operation budget нет; незаметного возврата к helper defaults тоже нет. Если обязательный начальный кредит не позволяет выполнить подготовку, новый путь должен завершиться до своего I/O. Малые последующие credits остаются фактическим результатом работы компонента, их нельзя молча повышать.


Достаточный retirement profile текущего driver также требует outer commands >= 1; outer records >= max(1, client.commandEntries, collector.frames + collector.records + schema.records().size() + desired.entries) с проверкой суммы; outer bytes >= max(collector.residentBytes, session.residentBytes); вложенные collector operations >= 1, visits >= collector.records + collector.frames, bytes >= collector.frames * session.residentBytes с проверкой умножения; runtime steps и handlers >= 1; outer operations >= 1. Для полного runtime quantum socketBytes >= max(inputBytes, writeBytes) — достаточный guard driver. Write floor консервативен для максимального допустимого write quantum и не является универсальным минимумом TCP. Эти проверки не делают недопустимыми generic handoff/partial-progress snapshots.

Для выбранной записи до своего I/O driver также требует проверенный на переполнение dispatch field credit >= 2 * selectedLocalRecord.fields.size(). Только фактический WORK_LIMIT при первом dispatcher.progress нового turn без предыдущего успешного продвижения явно возвращается как ошибка и запрашивает ABORT с неизменёнными credits. Последующий остаточный WORK_LIMIT, IDLE и native YIELD сохраняют обычную семантику.

Новый путь принимает только существующий controlled profile. DNS endpoint, TLS, credential handoff и retry больше одной попытки отклоняются через `requirePlainStream()` до настройки bootstrap/I/O этого driver. Эти значения не включают HISTORY collector, MUX delivery, reconnect, TLS или другую интеграцию источника.

## Граница TLS и credential handoff

Настройки могут владеть TLS handoff-данными, не реализуя TLS. При выборе TLS вызывающий код передаёт reference identity и PEM trust bytes; слой конфигурации проверяет только ограниченную форму и размер. Identity занимает 1–254 ASCII bytes, имеет ограниченную форму numeric-IP или ASCII/A-label DNS; запрещены NUL, wildcard, URI/user-info, brackets, zone IDs и address-property syntax. PEM input не превышает 512 KiB. Конфигурация не открывает файлы, не разбирает сертификаты, не строит цепочку и не подтверждает SAN/identity/cryptographic verification. Это остаётся обязанностью будущего TLS-адаптера по [принятой runtime/TLS policy](decisions/ADR-0006-live-runtime.ru.md); принятие конфигурации не подтверждает проверку сертификата.

Неявных TLS timeout или token limit нет. Запрос TLS требует явно заданных положительных handshake и close timeout. Если передан credential provider, для него также требуются положительный authentication timeout и token capacity не менее семи UTF-16 units, в пределах session text/output envelope. Без provider эти authentication settings противоречивы. TLS без Basic authentication допустим как форма handoff, но текущий driver не выполняет ни TLS, ни authentication. Credential eligibility появляется только после того, как будущий адаптер действительно проверит TLS; значение конфигурации никогда само не переводит session в `VERIFIED_TLS_BASIC`.

provider записывает токен в фиксированный заимствованный UTF-16 destination и возвращает число записанных units либо статическую ошибку. Результат должен быть непустым, не превышать размер destination и начинаться с точного префикса `Basic `. Provider не должен сохранять destination. Фабрика конфигурации его не вызывает, не выделяет token buffer и не материализует credentials. Будущий runtime вызывает его в своём сериализованном owner.

Handle provider удерживается через shared ownership, но состояние provider остаётся у вызывающего кода и требует его синхронизации при совместном использовании несколькими endpoint. Бюджеты конфигурации учитывают конечный shared handle, но не произвольное состояние provider, captures, удержанные ресурсы, время callback или работу деструктора. Освобождение последнего снимка может запустить деструктор provider вызывающего кода в потоке освобождения; деструктор должен быть там допустим. Гарантии secure erasure для обычной памяти C++ нет.

## CLI и диагностика

Существующий CLI `qdcxx-connect` сохраняет выбор схемы/вывода, текущие help, parsing, no-argument и exit-status правила. Добавленные параметры — одиночные positive-decimal `--connect-timeout-ms`, `--read-timeout-ms`, `--write-timeout-ms`, `--poll-interval-ms` (не более `INT_MAX`) и `--security plain|tls`. Исполняемый режим остаётся plain-only. `--security tls` распознаётся, чтобы сразу сообщить о неподдерживаемом профиле до настройки schema, signals, bootstrap или сети; параметры trust/секретов не добавляются, I/O не запускается. Retry-флага и дублирующего owner-operation budget нет.

Configuration errors и diagnostics используют статические error/stage labels. В них не включаются host, PEM, token, состояние provider или сырые аргументы. Автоматического дампа настроек, сериализации или CLI-параметра для передачи секрета нет.

## Установленный consumer

Точный исходник consumer — [tests/conformance/endpoint_options_consumer/main.cpp](../tests/conformance/endpoint_options_consumer/main.cpp). Его [CMake project](../tests/conformance/endpoint_options_consumer/CMakeLists.txt) связывает qdcxx_client и qdcxx_quote. Пример создаёт два снимка, меняет входные настройки между созданиями и проверяет, что первый сохранил свои port и timeout. Runtime не запускается, сетевого I/O нет.

```cpp
// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Native installed-package example: explicit immutable configuration only, without runtime/JSON or I/O activation.
#include "qd/client/endpoint_options.hpp"
#include "qd/events/quote.hpp"

#include <iostream>

/** Construct two independent immutable snapshots without any runtime or ambient configuration.
 * @return Zero for preserved independent effective values; nonzero for preparation/output failure.
 */
int main() {
    namespace qdcxx = org::ttldtor::qdcxx;
    auto schema = qdcxx::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto input = qdcxx::client::controlledStreamSettings(std::chrono::milliseconds{2000});

    if (!schema || !input) {
        return 1;
    }

    input->host = "127.0.0.1";
    input->port = 7301;
    auto first = qdcxx::client::EndpointOptions::create(*schema, *input);
    input->port = 7302;
    input->timeouts.read = std::chrono::milliseconds{750};
    auto second = qdcxx::client::EndpointOptions::create(*schema, *input);

    if (!first || !second || !first->requirePlainStream() || !second->requirePlainStream() ||
        first->settings().port != 7301 || second->settings().port != 7302 ||
        first->settings().timeouts.read != std::chrono::milliseconds{2000}) {
        return 1;
    }

    std::cout << "Independent endpoint snapshots: " << first->settings().port << ", " << second->settings().port
              << '\n';
    std::cout.flush();

    return std::cout ? 0 : 1;
}
```

Ожидаемый stdout: Independent endpoint snapshots: 7301, 7302. Программа завершается кодом 0. Установленный consumer прошёл на Windows и Linux.
