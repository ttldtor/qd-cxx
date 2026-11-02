# Инструмент подключения по подпискам

[English version](README.md). Английская версия — нормативная.

`qdcxx-connect` получает binary QTP records от существующего совместимого источника. По умолчанию
сохраняется контролируемый plain STREAM-драйвер. Явный `--mode recovery` объединяет существующие
TCP/TLS recovery owner и receiver для одной STREAM, TICKER или HISTORY записи/клиента на запуск.
Начните с [руководства приёма](../../docs/SUBSCRIPTIONS.ru.md);
[консольный C++ пример](../../examples/subscriptions/README.ru.md) использует публичный API приложения.

## Сборка и справка

Включите CLIENT, RUNTIME и CONNECT; для нового маршрута также RECOVERY. Внешний JSON требует
`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=ON`; TLS — `ORG_TTLDTOR_QDCXX_BUILD_TLS=ON` и точный
квалифицированный статический SDK. [Инструкция сборки](../../docs/CPP-BUILD.ru.md) описывает
поддерживаемые компиляторы, явные SDK inputs и установленные targets. Target `qdcxx_connect`
создаёт `qdcxx-connect`. Отключённая возможность отвергается до файлов, signals/bootstrap или сети.
Короткие имена ниже требуют executable в `PATH`; иначе используйте полный путь.
При вызове из каталога executable PowerShell требует `./qdcxx-connect.exe`.

Без аргументов и с отдельным `--help` выводится одинаковая английская справка; отдельный
`--help-ru` выводит русскую. Endpoint и файлы не нужны, активации приложения нет. Смешивание
справки и рабочих аргументов — usage error. Ошибка записи справки остаётся видимой.

## Вызов recovery и параметры

```text
qdcxx-connect --mode recovery --host 127.0.0.1 --port 19123 --symbol IBM --family stream --schema-profile quote --output quote --duration-ms 5000
```

Нужен уже запущенный совместимый источник; утилита не запускает сервис на этом порту.
Параметры передаются отдельными парами имя/значение. Повторяется только `--symbol`.
Неизвестные, неполные, повторные или противоречивые параметры отвергаются. Нет stdin commands,
неявных файлов или скрытых defaults.

| Параметр | Значение recovery, диапазон и default |
| --- | --- |
| `--mode recovery` | Обязательный явный выбор маршрута |
| `--host HOST`, `--port N` | Обязательные numeric IP либо ASCII/A-label DNS name без URI/zone; порт 1–65535 |
| `--symbol SYMBOL` | Обязателен; 1–128 printable ASCII units, первый не `*`; до 16 различных символов |
| `--family stream\|ticker\|history` | По умолчанию `stream`; одно семейство/запись/клиент на запуск |
| `--schema-profile quote\|profile` | Полная встроенная схема; default `quote`, local record ID 0 |
| `--schema-json PATH` | Явный native binary JSON, до 1 MiB; несовместим с `--schema-profile` |
| `--record-id N` | Local ID 0–2147483647; обязателен для внешней схемы |
| `--history-from-raw N` | Обязателен только для HISTORY с внешней индексированной схемой; полное знаковое int64 decimal |
| `--selectors exact-only\|canonical-exact-v1` | Default `exact-only`; All/wildcard недоступны в обоих профилях |
| `--output generic\|quote\|profile` | Default `generic`; prepared-строки дополняют все generic-поля |
| `--dns-server IPv4`, `--dns-port N` | Явный numeric resolver для имён; порт 1–65535, default 53 |
| `--dns-timeout-ms N` | 1–2147483647 мс; default равен connect timeout |
| `--attempts N` | Положительный uint32 с учётом первой попытки; default 1, также ограничен epoch-label table размером 1 MiB |
| `--retry-delay-ms N` | 1–2147483647 мс для attempts > 1; default 250; с одной попыткой отвергается |
| `--duration-ms N` | Admission interval 1–600000 мс, default 5000, включая negotiation |
| `--remove-after-ms N` | Одно удаление начальных ключей; строго положительно и меньше duration |
| `--max-records N` | Необязательная цель 1–1000000 завершённых записей; default target отсутствует |
| `--drain-ms N` | Local retirement allowance 1–60000 мс, default 1000 |
| `--connect-timeout-ms N` | 1–2147483647 мс, default 1000 |
| `--read-timeout-ms N`, `--write-timeout-ms N` | 1–2147483647 мс; default равен admission duration |
| `--poll-interval-ms N` | Caller cadence 1–2147483647 мс, default 1 |
| `--security plain\|tls` | Default `plain`; TLS требует отдельного build/trust/reference profile |
| `--tls-trust-pem PATH` | Обязателен для TLS; явный ordinary CERTIFICATE PEM, без system-trust fallback |
| `--tls-reference NAME` | Обязателен для TLS; проверяемый numeric IP или ASCII/A-label identity независимо от route |
| `--tls-handshake-ms N`, `--tls-close-ms N` | TLS deadlines 1–2147483647 мс, каждый default 1000 |
| `--auth-basic-token-file PATH` | Необязательный TLS-only полный ASCII Basic token, до 4096 байтов, без newline |
| `--auth-timeout-ms N` | Только с token file; 1–2147483647 мс, default 1000 |

Явные timeout values имеют приоритет независимо от порядка аргументов. Пути ограничены 4096 байтами;
Windows native UTF-16 arguments преобразуются в UTF-8 без изменения process codepages. Только явно
указанные файлы читаются один раз после проверок возможностей/аргументов. Ошибки не повторяют пути,
identity или tokens.

Фиксированный входной профиль допускает до 16 записей схемы, 128 logical fields, 256 lookups,
128 UTF-16 units на имя/16384 total name units и 64 records на frame. Несовместимый prepared
mapping или превышение профиля отвергаются до активации. Живые данные могут исчерпать фиксированные
queues/caches; это видимая ошибка без скрытого увеличения бюджетов. Numeric route обходит DNS;
имя требует явного resolver и дополнительных 80 MiB логического резерва запрошенной памяти.

## Схемы, TLS и вывод

[Руководство](../../docs/SUBSCRIPTIONS.ru.md) содержит команды Quote, Profile и HISTORY.
HISTORY требует явный indexed/time layout; знаковая raw bound не является Unix- или business-event
time. См. [native binary JSON](../../spec/schema/NATIVE-BINARY-JSON.ru.md) и
[индексированные схемы](../../spec/schema/INDEXED-SCHEMAS.ru.md). Выбор схемы не добавляет peer поддержку.

TLS проверяет явные trust/reference до использования необязательных credentials и не переходит на
plaintext. Ordinary trust input ограничен 524288 байтами/256 certificate blocks. Basic-файл
содержит точный `Basic ` prefix и канонический base64 без newline, trimming или нормализации.
Credentials передаются файлом, не argv. TLS ограничен
[замкнутым профилем](../../spec/cli/TLS-HOST-PROFILE.ru.md): точный статический image, последовательный
единственный SDK owner, новый процесс и отсутствующая `OPENSSL_ia32cap`; чужое произвольное SDK use не покрыто.

Recovery stdout начинается с `qdcxx-subscriptions<TAB>1`. Generic schema/record/field values
и исходные metadata сохраняются всегда; prepared-строки добавляют выбранные business values.
`null` отличается от присутствующего пустого значения. UTF16HEX/BYTEHEX сохраняют точные
units/bytes; raw numeric bits не преобразуются в цены или timestamps. Фактические commands,
reset/epoch transitions, HISTORY observations и local terminal evidence идут в stderr.
Для parser используйте точный [контракт строк/кодирования version 1](../../spec/cli/RECOVERY-OUTPUT.ru.md).

Конечные retries создают новые attempts и воспроизводят текущий desired set. Reset публикуется
перед acknowledgement с видимыми gaps/epoch changes; delivery не является exactly-once. HISTORY
range evidence локально и AS-OF; refusal/ineligibility означает unknown. Duration/record target
запрашивает bounded local drain. Целый callback batch может пересечь target; суффикс сохраняется.
Drain ограничен допущенным локальным префиксом и не доказывает peer ACK либо whole-network flush.
SIGINT/SIGTERM запрашивает abort; progress/retirement используют один work profile. У блокирующего
sink либо OS call нет hard preemption deadline. См. [recovery-контракт](../../spec/transport/RECOVERY.ru.md).

| Recovery exit | Значение |
| --- | --- |
| `0` | Фактический DRAINED без expiry с выполненными readiness/command/target requirements |
| `1` | Setup, protocol, transport, output либо retirement failure |
| `2` | Usage error либо недостигнутый readiness/target |
| `130` | Обработанный сигнал с фактическим ABORTED; ошибка имеет приоритет |

<a id="управляемый-инструмент-подключения-stream"></a>
<a id="tool-subscribe-legacy-stream-ru"></a>

## Прежний маршрут без выбора mode

Все разделы ниже применяются только без `--mode recovery` и сохраняют исходный
`qdcxx-stream`/summary output и контролируемую STREAM shutdown semantics.

`qdcxx-connect` — нативное необязательное приложение STREAM по обычному TCP без credentials поверх
[квалифицированного runtime](../../spec/transport/TCP-RUNTIME.ru.md) и
[локального диспетчера](../../spec/subscriptions/CLIENT-DISPATCH.ru.md). Это первый ограниченный
принимающий набор из [пункта Connect каталога](../../docs/upstream/QDS-TOOLS.ru.md), с собственными CLI
и выводом; совместимость с аргументами Java не заявляется.

## Сборка и справка

Явно включите `ORG_TTLDTOR_QDCXX_BUILD_CONNECT=ON`, `ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON` и
`ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON`. Все три параметра по умолчанию OFF. Передайте точные
квалифицированные исходные каталоги Asio и c-ares согласно
[инструкции runtime](../../spec/transport/TCP-RUNTIME.ru.md#граница-сборки).
CLI не добавляет получения зависимостей и не требует JSON-loader;
`ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF` поддерживается. Цель сборки — `qdcxx_connect`, имя файла —
`qdcxx-connect` (`.exe` в Windows).

Без аргументов и с отдельным `--help` выводится одинаковая английская справка в stdout, с пустым
stderr и кодом ноль, до разбора рабочих аргументов, схемы, bootstrap, изменения signals или сети.
Отдельный `--help-ru` выводит русскую справку. Ошибка публикации справки даёт ненулевой код.
Справка никогда не читает stdin.

Пример для явно управляемого IPv4 loopback peer:

```powershell
.\build\stream\qdcxx-connect.exe --host 127.0.0.1 --port 19000 --symbol IBM --schema-profile quote --output generic --duration-ms 5000
.\build\stream\qdcxx-connect.exe --host 127.0.0.1 --port 19000 --symbol IBM --schema-profile profile --output profile --max-records 1 --duration-ms 5000
```

В Linux используйте соответствующий путь без `.exe`. Peer должен реализовать выбранную готовность
DXP3, полные описания записей и данные STREAM; одного слушающего сокета недостаточно.

## Нативные параметры и границы

| Параметр | Смысл и значение по умолчанию |
| --- | --- |
| `--host IP` | Обязательный точный числовой IPv4/IPv6; без URI, zone identifier, hostname и обнаружения resolver. |
| `--port N` | Обязательный числовой порт 1..65535. |
| `--symbol SYMBOL` | Обязательный, повторяемый до 16 разных selectors по 1..128 печатных ASCII units; без начального `*`. Точное сравнение без glob/regex. |
| `--schema-profile quote\|profile` | Полная встроенная Quote из восьми полей или Profile из шестнадцати; по умолчанию `quote`. |
| `--output generic\|quote\|profile` | По умолчанию точные generic rows; prepared summary должна соответствовать схеме. |
| `--duration-ms N` | Интервал приёма 1..600000 миллисекунд, по умолчанию 5000. |
| `--remove-after-ms N` | Необязательное удаление всего исходного набора; положительное значение меньше duration. |
| `--max-records N` | Необязательная положительная цель по числу callback records до 1000000; прекращает новый приём после достижения цели принятым префиксом. |
| `--connect-timeout-ms N` | Deadline соединения, 1..2147483647 мс; по умолчанию 1000 |
| `--read-timeout-ms N` | Deadline неактивности ввода, 1..2147483647 мс; по умолчанию duration |
| `--write-timeout-ms N` | Deadline неактивности вывода, 1..2147483647 мс; по умолчанию duration |
| `--poll-interval-ms N` | Период прогресса вызывающего кода, 1..2147483647 мс; по умолчанию 1 |
| `--security plain\|tls` | По умолчанию `plain`; `tls` отклоняется до схемы/signals/bootstrap/сети |

Повторять можно лишь `--symbol`. Неизвестные, неполные, повторные или несовместимые аргументы
отклоняются до активации приложения. Нет stdin-команд, ambient configuration или загрузки схемы.
[Пример C++](../../examples/subscriptions/README.ru.md) отдельно принимает валидированную схему
вызывающей стороны и точные UTF-16 selectors, включая пустые/NUL/непарные units; ASCII-ограничение
CLI не меняет семантику символов библиотеки.

Driver использует один поток-владелец, одну сессию/decoder внутри `TcpRuntime` и один диспетчер.
Каждое полное событие после poll передаётся диспетчеру до последующих обычных изменений. Pending
локальная работа повторяется до нового source poll или удаления. Применение add/remove, композиция
вывода, локальная запись в сокет и подтверждение peer — разные этапы; локальные доказательства есть
лишь у первых трёх. Общего механизма согласования desired-to-sent нет.

Интервал приёма считается от запуска driver, включая negotiation. Цель по records не обрезает уже
принятый целиком frame: graceful локальное завершение может закончить его допустимый префикс.
Остановка по времени/цели/interrupt прекращает приём, явно отменяет native TCP/output, затем завершает
принятый локальный префикс. Возможны pending или uncertain native writes и частичный input. Ни local
terminal, ни код ноль не подтверждают полный network drain или peer ACK. Более ранняя остановка по
цели/interrupt может не дать выполнить `--remove-after-ms`.

SIGINT/SIGTERM запрашивают cooperative stop. Приложение временно устанавливает/восстанавливает свои
signal handlers и игнорирует POSIX SIGPIPE, чтобы broken working-output pipe стала проверяемой ошибкой
sink. Для произвольно блокирующих sinks/callbacks и native OS completion нет жёсткой гарантии времени.
TLS, credentials/authentication, reconnect, wildcard, общий MUX, TICKER/HISTORY и полные subscriptions
остаются вне этого этапа.

## Неизменяемая нативная конфигурация

После разбора всех аргументов CLI формирует тот же [типизированный снимок endpoint](../../docs/SUBSCRIPTIONS.ru.md#endpoint-options),
что и нативный overload. Явные timeout overrides имеют приоритет над defaults из duration
независимо от порядка аргументов. Все пять новых параметров однократные. TLS распознаётся
для статической ошибки неподдерживаемого профиля до рабочей подготовки; trust/secret argv,
конфигурационные файлы, environment и Java resolution не добавляются. Ошибки не повторяют
секреты вызывающего кода. Прежние рабочие exit codes и noargs/отдельная справка сохраняются.

Новый overload допускает независимое отсутствие legacy host/port, отклоняет заданные
конфликты и применяет минимум operations снимка и прежнего cap1..10000.
[Ограничения нативного driver](../../docs/SUBSCRIPTIONS.ru.md#endpoint-options) описывают
достаточные retirement/full-quantum guards и обработку WORK_LIMIT первого вызова свежего хода;
меньшие допустимые общие options/TCP-профили частичного продвижения остаются представимыми.
Передача TLS/auth, retry и DNS settings не включает исполнение.
См. [валидацию параметров](../../tests/conformance/subscriptions/OPTIONS-VALIDATION.ru.md).

## Версия вывода 1

Stdout содержит диагностические строки одного run/epoch, без строки об успешном завершении. Целые
числа десятичные. `UTF16HEX` — четыре прописные hex digits на точную UTF-16 unit; `BYTEHEX` — две
на byte. Литерал `null` означает отсутствие; non-null пустой payload — пустая ячейка.

Generic output начинается с `qdcxx-stream<TAB>1` и содержит такие tab-separated строки:

```text
schema  localId  recordNameUTF16HEX  fieldCount
layout  localId  fieldIndex  fullType  lookupNameUTF16HEX
record  ordinal  localId  senderId  revision  bindingGeneration  cipher  eventFlags  symbolUTF16HEX  removalTimeOrNull  fieldSecondsOrNull  fieldSequenceOrNull  fieldSourceOrdinalOrNull
field   ordinal  fieldIndex  fullType  raw  unitsNullOrUTF16HEX  bytesNullOrBYTEHEX
```

Пробелы здесь разделяют колонки; фактический вывод использует tabs. Ordinals начинаются с единицы.
Сохраняются все lookups, включая aliases, и все дополнительные typed fields. Legacy compact-only
values при отсутствии typed views используют полный тип 8. Provenance callback проверяется по
исходной bounded schema identity; local ID, sender ID, revision и исходная binding generation —
отдельные координаты. Это не сериализация полной `LocalSchema`/`DataScheme`, не формат wire/tape
roundtrip и не opaque identity между runs. Schema profile/indexed layout и peer descriptor metadata
не сериализуются.

Prepared summaries начинаются с `qdcxx-stream-summary<TAB>1` и содержат выбранную часть mapped
business event; для сохранения всех исходных полей используйте generic output:

```text
quote    ordinal  symbolUTF16HEX  bidTimeMs  askTimeMs  bidPriceRaw  askPriceRaw  timeMs
profile  ordinal  symbolUTF16HEX  flags  descriptionNullOrUTF16HEX  statusReasonNullOrUTF16HEX
```

Stderr отдельно сообщает `application`, `local` и `transport`: applied/composed команды, цель/cutoff,
локальные callback counts/outcome, native primary/secondary stage/code/protocol category, abort/peer end,
reported accepted write bytes, pending output, unknown write acceptance и discarded input. Ошибка
окончательного sink flush остаётся отдельной видимой ошибкой после завершения обоих владельцев.

## Коды завершения и границы проверок

- `0`: intentional cutoff или достигнутая цель без обнаруженной ошибки; проверьте native pending/uncertain output.
- `1`: неверный ввод, setup, peer/protocol/callback/sink failure, включая неоднозначную native close failure.
- `2`: интервал истёк до композиции начального add или до цели records; также применённый remove без композиции вывода.
- `130`: обнаруженный interrupt без предшествующей ошибки.

В Windows квалифицированное native close может сообщить `READ_FAILURE`, `socket-read`, native code
1236 после intentional abort. Источник неоднозначен; ошибка сохраняется и даёт `1` даже при drained
локальном префиксе. Она не переименовывается молча в cancellation или успешное завершение.

`connect_stream`, `connect_cli_contract` и `connect_process` — focused native CTests. Они проверяют
actual local TCP readiness/add/remove, точный выбор, byte sends, forced local work retry, полные
prepared Quote/Profile, peer loss/partial framing, sink failure и завершение собственных child processes.
[Независимый consumer](../../tests/conformance/connect_consumer/CMakeLists.txt) отдельно проверяет
линковку установленного пакета и программный пример. Описание сценариев не означает выполнения
каждой платформы/конфигурации; окончательная квалификация фиксируется в опубликованных доказательствах задачи.
