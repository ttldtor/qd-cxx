# Требования к архитектуре конфигурации

## Явный источник wire-схемы

Параметр `--wire-schema ordinary-v1` явно выбирает источник схемы для инспектора tape
и несовместим с `--schema PATH` и `--profile NAME`. Выбор разрешается на границе
CLI до чтения tape; core/protocol не читают переменные окружения, свойства или
файлы схем. [WIRE-INVENTORY](../spec/schema/WIRE-INVENTORY.ru.md) ограничивает
обнаружение, а [TAPE-INSPECT](TAPE-INSPECT.ru.md) определяет метаданные источника
в версии 7. Прежние параметры записей, полей, ключей поиска и имён схемы задают
предварительную проверку полного объединения; ограничения размера и глубины
JSON-документа применяются только при загрузке JSON. Внешний загрузчик и адаптер
событий не выбираются неявно; обнаружение wire-схемы работает и в сборках без JSON.

[English version](CONFIGURATION.md). Английская версия является нормативной.

Проекту нужны аналоги относящихся к нему настроек Java QD/dxFeed в разных подсистемах, а не только параметры схем событий.
Это по-прежнему широкая исследовательская задача: общей загрузки конфигурации и полного контракта совместимости нет. Выбранный native API параметров endpoint ниже принимает явные типизированные снимки в ограниченных границах.

## Источники и базовая версия

Реализованное в QDCXX-025 [подмножество настроек схемы](../spec/schema/CONFIGURED-EVENTS.ru.md) добавляет
production-наблюдения в новых JVM для выбранных startup, endpoint file/API, mutation и XML inputs.
Оно фиксирует измеренные приоритет и момент чтения каждого параметра, включая явный выбор полей
против nanoTime. Наблюдения дополняют исходные факты ниже; они не устанавливают универсальный приоритет
и не реализуют C++ settings loader. Подготовленные отображения принимают только явные проверенные схемы.

- [QD and dxFeed Java API Project Settings](https://github.com/dxFeed/dxfeed-graal-cxx-api/wiki/QD-and-dxFeed-Java-API-Project-Settings), прочитано 2026-09-23: исходный список для поиска, не полная и не закреплённая спецификация.
- [Текущие release notes](https://github.com/devexperts/QD/blob/master/ReleaseNotes.txt): источник для поиска и истории изменений; master изменяем.
- [Закреплённые release notes](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/ReleaseNotes.txt), код потребителей и production-наблюдения определяют базу согласно `upstream.lock`.

Закреплённые заметки QDS 3.353 описывают переименование настроек схемы, сохранение устаревших aliases и приоритет настроек схемы из endpoint-файла над JVM system properties.
Это документированное правило для этих настроек, а не доказательство единого порядка приоритетов всех компонентов.
Закреплённый исходник `SchemeProperties` различает явный выбор полей и сокращённую настройку nano-time, а также XML defaults и программные defaults.
Это наблюдения по исходникам и документации, а не новые runtime captures.

## Архитектурные требования

1. Разделять получение источников, разрешение приоритетов, типизированную проверку и применение к компонентам. Программно заданные properties, properties-файлы и параметры запуска требуют документированного пути в явную конфигурацию; поддержка переменных окружения и точный синтаксис CLI C++ требуют отдельных решений.
2. Допускать совместимые внешние имена properties там, где это осмысленно, предоставляя потребителям типизированные значения. Документировать намеренные переименования и отклонения; не принимать молча настройки без эффекта и не создавать впечатление, что JVM-only параметры работают в C++.
3. Определять область действия и момент чтения каждой настройки: default процесса, endpoint, connector, схема или другой компонент; снимок при инициализации, перестроение компонента либо явно поддержанное обновление во время работы. Вызов setter после инициализации сам по себе не меняет существующий Java- или C++-компонент.
4. Допускать сосуществование независимо настроенных экземпляров. Глобальные defaults совместимости могут быть явным входом разрешения; core, кодеки и mappings событий не обращаются к неявному изменяемому состоянию. Снимки не могут молча заменять upstream-контракт обновления во время работы; такие случаи исследуются и решаются отдельно.
5. Сохранять различия отсутствия, пустого значения и явного false/zero, где они существенны. Типы, единицы, диапазоны, разбор, неверные значения, повторные ключи, aliases и конфликты источников определяются по доказательствам. Не выдумывать универсальный приоритет вроде «командная строка выше файла».
6. Показывать действующую конфигурацию и происхождение значений в диагностике с сокрытием секретов и ограничением вывода. Не включать credentials, адреса с аутентификацией и частные конфигурационные файлы в fixtures, manifests или логи.
7. Отделять конфигурацию от реализации транспорта. Разрешение схемы выдаёт артефакт из [ADR-0003](decisions/ADR-0003-configurable-schemas-and-event-mappings.ru.md); настройки сети, collectors и логирования применяют соответствующие компоненты, а не построитель схемы.

## Инвентаризация и реестр совместимости

Создать общий реестр отдельной ограниченной задачей до реализации общей загрузки конфигурации и настройки рабочих утилит.
Для каждого ключа или семейства ключей записывать: имя Java и aliases; потребляющий исходник и закреплённую ревизию; подсистему; грамматику, тип, единицы, default и диапазон значения; допустимые источники и приоритеты; момент чтения и кэширование; историю версий и устаревания; чувствительность; решение для C++ и подтверждающие тесты.
Состояния: реализовано и проверено, запланировано, не поддержано, JVM-specific/неприменимо либо не выяснено. Найденный ключ не становится автоматически поддерживаемым.
Сопоставлять wiki с реальными потребителями, искать составляемые в коде ключи наряду с литералами обращений к properties; release notes объясняют изменения, но не доказывают полноту перечня.

Охватить настройки схем/providers, endpoint feed/publisher, connectors/адресов, heartbeat/reconnect, файлов/replay, подписок/collectors/буферов, потоков, логирования/мониторинга и тестовые параметры.
Приоритет определяется реализованными компонентами и [планом qds-tools](upstream/QDS-TOOLS.ru.md), а не воспроизведением всех внутренних параметров Java.

## Граница ближайшей реализации

QDCXX-025 берёт часть, относящуюся к схемам: пробы источников, приоритетов и момента чтения в свежих JVM, происхождение действующей конфигурации и явные разрешённые схемы для Quote/Profile.
Использовать поля реестра выше, не создавая отдельную политику properties в каждом адаптере события.
Выбранный native API [EndpointOptions](CONFIGURATION.ru.md) теперь предоставляет явные снимки параметров отдельных endpoint. Общий загрузчик properties-файлов C++, полный каталог настроек, properties рабочих утилит и правила live-update остаются будущей работой в [плане](NEXT-STEPS.ru.md).
Настройки, требующие неподдержанных типов или подсистем, становятся явными неподдержанными/отложенными случаями; нельзя подменять их значения или менять `upstream.lock` ради успешного теста.

## Выбранный native API снимка параметров endpoint (QDCXX-073)

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
