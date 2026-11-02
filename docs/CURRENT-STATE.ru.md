# Текущие возможности и ограничения

[English version](CURRENT-STATE.md). Английский документ является основным.

Проект реализует отдельные ограниченные профили относительно Java QD 3.355 на ревизии из
[upstream.lock](../upstream.lock). Полная совместимость QD, dxFeed, QTP или tape не заявляется.

## Реализованные компоненты

[Выбранный опциональный профиль runtime](decisions/ADR-0006-live-runtime.ru.md) имеет воспроизводимые
[проверки реализуемости DNS/TCP/TLS](../tests/conformance/transport/VALIDATION.ru.md).
Отдельный production TCP-адаптер реализует описанный ниже ограниченный plain-профиль без credentials.
Отдельные ресурсный модуль и API TLS runtime описаны ниже; у TLS есть ограниченные native-доказательства; итоговая интеграционная приёмка учитывается отдельно, а полный сквозной клиент подписок не реализован.
Ограниченный локальный HISTORY-коллектор обрабатывает допущенные события источника, не владея
upstream demand, сессией источника или callbacks. Чистый ограниченный компонент MUX агрегирует
желаемые интересы локальных клиентов; он не владеет сессиями источников, согласованием wire-подписок,
доставкой callback или очередями медленных потребителей.

| Компонент | Поведение и контракт |
| --- | --- |
| Generic/prepared-получатель подписок | Независимые логические регистрации на существующих STREAM/TICKER/HISTORY очередях, полные generic-записи и явные schema-bound Quote/Profile проекции, атомарная подготовка до регистрации, callback-only заимствованные пакеты и независимые владеющие результаты: [руководство приложения](SUBSCRIPTION-RECEIVER.ru.md), [контракт](../spec/subscriptions/RECEIVER.ru.md). Владение endpoint/recovery остаётся отдельным. |
| Контролируемый STREAM-клиент | Опциональный `qdcxx-connect` и программный пример соединяют настоящие plain TCP runtime и диспетчер: [руководство](SUBSCRIPTIONS.ru.md). Точный ограниченный выбор CLI, общие записи и явные Quote/Profile summaries; без заявления TLS/reconnect/полных подписок. |
| Параметры endpoint | Типизированные неизменяемые снимки схемы/настроек в опциональном `qdcxx_client`, независимые endpoint, конечные бюджеты/timeouts и передача runtime/TLS/auth/schema settings: [нативное руководство](SUBSCRIPTIONS.ru.md#endpoint-options), [конфигурация](CONFIGURATION.ru.md), [валидация](../tests/conformance/subscriptions/OPTIONS-VALIDATION.ru.md). Текущее приложение остаётся STREAM по числовому TCP без credentials; хранение TLS/DNS/retry settings не включает эти возможности. |
| Локальная доставка STREAM | Сериализованные callbacks, ограниченные потокобезопасные команды, независимые неизменяемые батчи и явное локальное завершение: [контракт](../spec/subscriptions/CLIENT-DISPATCH.ru.md). OFF по умолчанию; сессией/runtime и согласованием исходящих подписок владеет вызывающий код. Локальное завершение не подтверждает вывод транспортных операций. |
| STREAM-коллектор | Каждая принятая запрошенная запись в исходном порядке, включая одинаковые значения; ограниченная whole-event staging и локальные membership/revision barriers: [контракт](../spec/subscriptions/STREAM-COLLECTOR.ru.md). Без callbacks, latest cache и общего upstream multiplexing. |
| TICKER-коллектор | Общий ограниченный latest-кеш, независимые локальные представления, явное хранение, initial/live порядок и stale/reset evidence: [контракт](../spec/subscriptions/TICKER-COLLECTOR.ru.md). Чистая выдача под управлением вызывающей стороны; callbacks и upstream multiplexing относятся к отдельным компонентам. |
| HISTORY-коллектор | Чистая ограниченная per-view проекция целиком допущенных HISTORY-событий с общим signed-raw-index хранилищем и полнотой запрошенного диапазона: [контракт](../spec/subscriptions/HISTORY-COLLECTOR.ru.md), [проверки](../tests/conformance/subscriptions/HISTORY-VALIDATION.ru.md). Без владения сессией источника, callbacks и upstream demand. |
| Опциональный TCP runtime | Явные SDK/bootstrap, DNS/connect/read/write под управлением вызывающей стороны, владеющие буферы, конечная работа и close после вывода операций: [контракт](../spec/transport/TCP-RUNTIME.ru.md), [ресурсы DNS](../spec/transport/DNS-RESOURCES.ru.md), [проверки](../tests/conformance/transport/TCP-VALIDATION.ru.md). OFF по умолчанию; без TLS, прикладных callback, коллектора и reconnect. |
| Чистая live-сессия | Приём целого кадра, неизменяемые поколения другой стороны, управляющие сообщения протокола/heartbeat, readiness/auth gates и владеющий вывод: [контракт сессии](../spec/subscriptions/LIVE-SESSION.ru.md). Без сокетов, часов, коллекторов и клиента с переподключением. |
| Вывод подписок | Шесть сообщений и явные описания, точные символы, владеющие хвосты частичной записи и квоты удержанного вывода: [live composer](../spec/subscriptions/WRITER.ru.md). Без socket/session/peer acknowledgement. |
| Целевые подписки | Чистое ограниченное состояние по клиентам и точное агрегированное объединение, точные символы/schema identity, атомарные add/remove/replace/clear, revision и непереиспользуемые membership tokens: [API состояния](../spec/subscriptions/STATE.ru.md), [контракт MUX interests](../spec/mux/INTERESTS.ru.md), [валидация MUX](../tests/conformance/subscriptions/MUX-VALIDATION.ru.md). `qdcxx_mux` выдаёт полный snapshot желаемого спроса и predicate выбора клиента; согласование wire-подписок, сессии источников, callbacks, очереди, reconnect, HISTORY-коллектор и полная сквозная доставка остаются отдельными компонентами. |
| Кодировки | Принимаемые формы compact int/long, точные nullable UTF-16 строки, владеющие символы, конечные бюджеты и терминальные ошибки: [примитивы](../spec/encoding/PRIMITIVES.ru.md), [UTFString](../spec/encoding/UTFSTRING.ru.md), [символы](../spec/encoding/SYMBOLS.ru.md). |
| QTP | Инкрементальное выделение кадров, исходные описания, неизменяемые обычные локальные схемы, статическое связывание и декодирование полного STREAM_DATA: [кадры](../spec/protocol/FRAMING.ru.md), [описания](../spec/protocol/DESCRIBE-RECORDS.ru.md), [связывание](../spec/protocol/STATIC-BINDING.ru.md), [stream data](../spec/protocol/STREAM-DATA.ru.md). |
| Офлайн tape | Явные профили legacy STREAM и RAW_DATA/MESSAGE без сжатия, append-only описания, владеющие данные времени, конечные бюджеты работы/владения и терминальные ошибки с завершённым префиксом: [tape reader](../spec/protocol/TAPE-READER.ru.md), [RAW tape](../spec/protocol/RAW-TAPE.ru.md). |
| Writer tape в памяти | Ограниченная запись RAW_DATA/MESSAGE v1 для точных поддержанных layouts и типов полей, с владеющими фрагментами, конечными бюджетами и терминальными ошибками: [контракт writer](../spec/protocol/TAPE-WRITER.ru.md), [валидация writer](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.ru.md). |
| Строгий допуск tape | Предварительная проверка всего `RAW_MESSAGE_V1`, точное связывание inventory, владеющий move-only token полного источника, явный EOF и отдельные конечные бюджеты работы/логического scratch: [валидация допуска](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.ru.md). |
| Офлайн-воспроизведение | Необязательная синхронная библиотека `qdcxx_replay` для неизменяемых допущенных записей, явный режим `IMMEDIATE`/`RELATIVE`, конечные бюджеты и callbacks вызывающей стороны: [контракт](../spec/protocol/OFFLINE-REPLAY.ru.md), [валидация](../tests/conformance/replay/VALIDATION.ru.md). |
| Значения полей | Явные точные полные ID 2/8/10/40/56/72/88/120; nullable UTF-16 хранится с владением, исходные скалярные представления сохраняются: [типы полей](../spec/encoding/FIELD-TYPES.ru.md). Прежние более узкие профили остаются доступными. |
| Бинарный каталог | Явная конечная модель 19 full field ID, состояние indexed/time схемы, raw message families, flags и removal metadata: [покрытие каталога](../tests/conformance/protocol/real-tape/COVERAGE.ru.md). |
| Конечные tape-источники | Явная последовательность самодостаточных вхождений, однородное время, optional конечные контейнеры и согласованное завершение parser/source/close/flush: [контракт источника](../spec/protocol/TAPE-SEQUENCE.ru.md), [проверки](../tests/conformance/protocol/real-tape/VALIDATION.ru.md). |
| События | Default Quote и подготовленные отображения Quote/Profile, привязанные к схеме, с явными локальными ID и сохранением дополнительных/custom полей в общем пути: [default Quote](../spec/protocol/DEFAULT-QUOTE.ru.md), [настраиваемые события](../spec/schema/CONFIGURED-EVENTS.ru.md). |
| Native JSON | Необязательный загрузчик с отдельно закреплённой зависимостью, владеющие проверенные схемы, типизированные ошибки до I/O tape и общий просмотр: [native JSON](../spec/schema/NATIVE-JSON.ru.md). Core, codecs, protocol и отображения событий независимы от JSON. |
| Binary JSON configuration | Отдельно выбранный `qdcxx-native-binary` v1, явные pair/single indexed layouts, владеющее provenance, независимые конечные области и validate-only CLI: [контракт](../spec/schema/NATIVE-BINARY-JSON.ru.md). Прежняя HISTORY composition отделена от CLI-проверки конфигурации. |
| Экспорт Java-схем | Офлайн settings/XML источники, полный набор реальных lookup keys, явная логическая проекция storage slots и публикация native JSON без частичного результата при отказе: [контракт экспорта](../spec/schema/JAVA-EXPORT.ru.md), [использование](../tools/schema-export/README.ru.md). Существующие C++ потребители используют сохранённый результат без Java. |
| Утилиты | Необязательные `qdcxx-qtp-inspect`, `qdcxx-tape-inspect` и строгий офлайн `qdcxx-tape-write`, конечные бюджеты, явные профили и публикация результата только при отсутствии назначения: [сборка/API](CPP-BUILD.ru.md), [QTP просмотр](QTP-INSPECT.ru.md), [tape просмотр](TAPE-INSPECT.ru.md), [запись tape](TAPE-WRITE.ru.md), [Profile просмотр](PROFILE-INSPECT.ru.md). |
| Инструменты разработки | Нативные UTC-логи и переносимое форматирование через CMake; обычные configure/build/CTest не требуют Python или PowerShell: [инструкции](../tools/dev/README.ru.md). Тестовые зависимости и JSON-загрузчик сохраняют отдельные границы включения. |

## Доказательства проверок

[Валидация экспорта схем](../tests/conformance/schema-export/VALIDATION.ru.md) описывает
интеграцию schema-export. Итоговые интеграционные доказательства writer приведены в
[валидации writer](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.ru.md): Windows
MSVC x64 Release прошёл 47/47 CTest тестов; Linux GCC Debug ASan+UBSan на финальном исходнике
прошёл два затронутых теста writer и char-array, тогда как полный прогон 47/47 использовал
начальный snapshot до исправления `textUnits` после ревью. Финальные Linux production-only Release
сборки прошли с выключенными и включёнными tools (JSON выключен в обеих). Java 8 FileReader
прочитал все пять финальных native-emitted cases: 18 STREAM records, 54 callback projections,
152 rows и ноль ошибок parser. Эти результаты не подтверждают macOS, строгую публикацию файлов,
или поддержку tape целиком.

Отчёт разделяет production-наблюдения Java, диагностическую проекцию и C++ conformance.
Отчёты прежних компонентов сохраняют собственные выполненные профили и ограничения. Хеши
форматтера, captures и исходников относятся к записанным ревизиям, а не к будущим входам.

Доказательства реализации допуска и CLI задачи QDCXX-034 приведены в
[валидации допуска](../tests/conformance/protocol/tape-write/ADMISSION-VALIDATION.ru.md) и её
архиве с индексом хешей.
Windows MSVC x64 Release прошёл полный CTest: 50/50 тестов; прошли две Windows production-only
матрицы с JSON OFF, включая профиль с необязательными tape-write и inspection tools. Закреплённый
production Java FileReader прочитал пять итоговых native перезаписей (18 STREAM records, 54 callback
projections, 152 rows, без ошибок разбора). Целевые Linux GCC ASan+UBSan file-boundary тесты прошли
12/12; полный Linux Debug набор прошёл 50/50 под Ubuntu 24.04 / WSL2 с GCC 13.3, ASan и UBSan
(865,93 секунды по CTest). Оба Linux production-only Release-профиля собрались с JSON OFF: минимальный с tools OFF и с QTP inspection, tape inspection и tape-write ON. Итоговые проверки подтвердили отсутствие `_deps` в обоих профилях. В обеих сборках GCC выдал существующее предупреждение `-Wstringop-overflow` в `src/core/wide_decimal.cpp:68`; команды завершились успешно. Проверка
форматирования C++ прошла для 97 файлов. Doxygen создал страницу API допуска без предупреждений;
настройка `EXTRACT_ALL` не подтверждает полноту API-документации. Эти результаты не подтверждают
macOS, враждебные гонки файловой системы или сохранность после сбоя.

Replay target прошёл 9 случаев / 1 303 assertions и в итоговом целевом Windows-прогоне, и при отдельной независимой проверке. Полный Windows MSVC Release набор прошёл 51/51 за 311,19 секунды; прошли три Windows production-only профиля с JSON OFF: replay ON/tools OFF, replay ON/все tools ON и replay OFF/tools OFF. Проверка форматирования C++ прошла для 100 файлов, Doxygen создал страницу API replay. Полный Linux sanitizer-набор и production-профили прошли. Результаты и ограничения описаны в [валидации replay](../tests/conformance/replay/VALIDATION.ru.md).

## Конечный каталог бинарных tape и владеющие источники

Явный профиль `BINARY_CATALOG_V1` добавляет 19 конечных full field ID,
поддержанные raw message families, состояние indexed/time схемы, flags и removal
metadata. Он сохраняет полные ID, физические значения и владеющие payload вместо
бизнес-преобразований событий. Явный профиль источника
`SELF_CONTAINED_SEGMENTS_V1` добавляет конечный список вхождений, однородную
политику NONE/MESSAGE/FIELD/LONG/TEXT, optional конечный GZIP/ZIP и согласованное
закрытие источников. Core codecs и protocol не зависят от файлов, часов и
транспорта.

[Покрытие каталога](../tests/conformance/protocol/real-tape/COVERAGE.ru.md)
учитывает все 71 строки и сохраняет явные unsupported-альтернативы.
[Именованные проверки](../tests/conformance/protocol/real-tape/VALIDATION.ru.md)
разделяют production observations, native-тесты, полное ordinary-сравнение и
оставшиеся ограничения платформ/версий. Профиль output9 этого компаратора
ограничен одним несжатым ordinary-источником MESSAGE и не заменяет
FIELD/sidecar/container/sequence fixtures.

Новый source CLI требует явного выбора source/time/schema/mode. Он не ищет
companion или файлы, не следует live-последовательности и не смешивает политики
времени. Маркер полного завершения требует успеха всех слоёв, закрытий,
окончательных flush и процесса. Прежние профили и форматы вывода 1–8 сохраняют
отдельные контракты. Логические счётчики ресурсов отличаются от измеренного RSS.

Окончательные native-наборы прошли 74/74 на Windows MSVC Release и Linux GCC
Debug ASan/UBSan; прошли шесть production-only profiles и dependency gates.
Публичный Java workflow на Windows прошёл 120 проверок после отдельного нового
93-проверочного прогона legacy. Полные ordinary-сравнения прошли на обоих точных
parser pin. [Проверки](../tests/conformance/protocol/real-tape/VALIDATION.ru.md)
фиксируют hash исходников/артефакта и сохраняют узкие границы времени/ОС.

Полная совместимость QD/network/subscription/collector, дополнительные адаптеры
событий, десериализация объектов, произвольные архивы, live follow/cycle, macOS и
выпущенная версия остаются вне именованного результата. Исторические отчёты
сохраняют точные ограничения исходников/артефактов; окончательная интеграция
фиксируется отдельно.

## Известные ограничения

- Нативное построение XML-схем, общее разрешение Java properties и замена схем во время работы отсутствуют.
  Java-мост поддерживает только документированные settings и контролируемое замыкание источников.
- Indexed layouts, дополнительные конечные типы, options/flags и сжатие доступны только в явных catalog/source профилях. Восстановление, произвольные преобразования, более общая публикация файлов, интеграция с реальными часами, replay CLI и сетевой транспорт остаются вне реализованной границы.
- Адаптеры кроме документированных отображений Quote/Profile не предоставляются. Исходные данные
  времени и биты полей не подразумевают бизнес-преобразования, TICKER-доставку или широкую интеграцию источника. Ограниченная локальная HISTORY-проекция описана отдельно в [контракте коллектора](../spec/subscriptions/HISTORY-COLLECTOR.ru.md).
- Полное равенство приватного источника в [оставшихся пробелах](REAL-TAPE-GAPS.ru.md) ограничено именованным ordinary MESSAGE-профилем; остальные ячейки каталога проверяются отдельными production fixtures.
- macOS, отдельная standalone сертификация LeakSanitizer и hostile concurrent filesystem остаются
  непроверенными. Пути отказа allocation у writer покрыты focused fault injection; XML preflight не
  является общей песочницей для враждебного входа.
- Приоритет неверной конфигурации и одновременного отказа вывода не определён;
  см. [валидацию экспорта схем](../tests/conformance/schema-export/VALIDATION.ru.md).

[Открытые вопросы upstream](upstream/OPEN-QUESTIONS.ru.md), [требования конфигурации](CONFIGURATION.ru.md),
[политика совместимости](COMPATIBILITY.ru.md) и [план развития](NEXT-STEPS.ru.md) задают оставшиеся границы.

## Локальная MUX delivery composition

Необязательный CLIENT graph также предоставляет `qdcxx_mux_queue`: один настоящий MUX state и один
настоящий STREAM/TICKER/HISTORY collector с конечным per-client command/control/delivery storage,
callbacks и detached immutable leases. См. [контракт очередей](../spec/mux/QUEUES.ru.md) и
[ограниченную валидацию](../tests/conformance/subscriptions/MUX-VALIDATION.ru.md#mux-queue-validation).
Эта композиция отделена от чистого `qdcxx_mux`; текстовый glob expansion, wire reconciliation, полный subscription
facade, transport stop и новая Java HISTORY producer equivalence не добавлены.

<a id="selector-profiles"></a>

## Явные canonical selectors

Opt-in immutable profiles добавляют canonical Exact всех трёх subscription families и явный
known-record All для STREAM. См. [контракты селекторов и compiled examples](../spec/subscriptions/SELECTORS.ru.md)
и [ограниченную квалификацию](../tests/conformance/subscriptions/WILDCARD-VALIDATION.ru.md). Legacy Exact
остаётся default; All не является буквальным wildcard text. TICKER/HISTORY All, automatic peer
negotiation, полные network facades и общая provider/HISTORY-producer equivalence не добавлены.

<a id="tls-source-profile-current-state-ru"></a>

## Ресурсы и реализация TLS runtime

У чистого расчёта запрошенной памяти/работы TLS и атомарного допуска есть результаты native-тестов
ресурса. Реализация runtime, допускающая только перемещение, обеспечивает передачу проверенных учётных данных,
точное владение незавершённым I/O и достоверное локальное закрытие. Ограниченные native-проверки
отделены от итоговой интеграционной приёмки. См. [ресурсный контракт](../spec/transport/TLS-RESOURCES.ru.md),
[API runtime](../spec/transport/TLS-RUNTIME.ru.md) и [статус квалификации](../tests/conformance/transport/TLS-VALIDATION.ru.md).
Это не меняет поведение обычного runtime/CLI и не подтверждает выпуск TLS или полных подписок.

### Текущая граница доказательств TLS

Рабочий TLS runtime реализован. Целевой набор текущих исходников прошёл 39 native-тестов: Windows MSVC 19.42 x64 Release /MD — 11 486 проверок; Linux GCC 13.3 x64 Debug ASan+UBSan — 11 625 проверок, LSan выключен. Полная регрессия на каждой ОС прошла 58/58 тестов с JSON и 57/57 без JSON. Проверка сгенерированной справки учитывается отдельно. Обновлённый установленный пакет прошёл четыре TLS 1.2/1.3 запуска на двух ОС. Результаты отдельных проверок не означают выпуск, полную поддержку подписок или сертификацию безопасности. Актуальный перечень выполненных и только обоснованных исходниками случаев находится на странице квалификации TLS, ссылка на которую приведена выше.
