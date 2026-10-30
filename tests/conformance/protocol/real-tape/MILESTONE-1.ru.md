# Первый этап полного чтения реальных tape

[English](MILESTONE-1.md). Английская версия нормативная.

**Результат: полное каноническое равенство именованного частного и переносимого обычного корпуса.**
Окончательный binary-catalog/output8 artifact прочитал STREAM/MESSAGE записи обоих producer
pins и полный частный RAW/MESSAGE файл. Семь прежних обычных случаев сохраняют проверку
через output7. Неподдерживаемые записи молча не пропускаются. Приёмка относится к указанной
обычной проекции и не подтверждает каждую ячейку binary catalog.

Мост output8 строго проверяет обычное подмножество до нормализации и отделяет `CppMode`
от времени Java. Финальный набор содержит **93 проверки: 44 прежние workflow-проверки,
47 DESIGNED grammar/generation controls и два НОВЫХ фактических C++8 успеха STREAM/MESSAGE**.
Designed-строки получены из реальных v7 captures и не являются production C++8 исполнением.
Полный частный output8-файл проверен отдельным фактическим запуском. Все четыре прежних
отказа STREAM/MESSAGE в старых режимах остаются обязательными регрессиями.

## Выполненное сравнение

Проверки Windows x64 от 2026-10-30 использовали C++ inspector `ordinary-v1`/output7 и
`binary-catalog-v1`/output8,
Java QD 3.355 по [upstream.lock](../../../../upstream.lock), Zulu JDK 8 `1.8.0_422-b05`,
Java heap 128 MiB и ограничение 60 секунд на дочерний процесс. Исходная граница схемы описана
в [проверках обычной схемы](../wire-inventory/VALIDATION.ru.md). Native build/integration
учитываются отдельно от этого Java/PowerShell workflow.
Этот отчёт относится к хешам фактического inspector и исходников утилит в частной metadata.

Финальный Windows MSVC 19.42 Release artifact имеет SHA-256
`a199a657cc75bc6d19486dfa692422f60bf51c3fed6a65e5a4ae4229ef0ce6a1`.
Хеш Java-компаратора:
`fb3f54cd89e00251aa605b02eb825e48144d1df5ba6dfd869963054bcff43764`.
Набор из 93 проверок наблюдался с 18:14:00.214 до 18:15:29.362 UTC, отдельный фактический
частный запуск — с 18:14:00.444 до 18:14:30.401 UTC. Это перекрывающиеся календарные
интервалы, не активные инженерные часы и не performance benchmark. Хеши исходников,
классов, runner, inspector и входа сохранялись до/после.

[Compare-RealTapes.ps1](Compare-RealTapes.ps1) и
[production Java oracle](../../../../tools/upstream/real-tape-acceptance/README.ru.md)
сравнивают каждое поколение схемы, локальные имена/ID записей и полей, исходный порядок
описаний, sender ID, wire-имена/полные типы/порядок/targets, frame и порядок записей,
точный UTF-16 символ, raw scalar и nullable точные UTF-16 значения. Для RAW/MESSAGE
сравниваются наличие/исходные миллисекунды timestamp, cursor sequence и event time.
Явно проверяется нулевое значение каждого наблюдаемого event flag. Native Java descriptors
должны использовать прямые INT/LONG/OBJ; бизнес-преобразования исключены.

Проверяются порядковые номера/число frames и контекст публикации schema/data. Вспомогательные
frame offset/type/payload columns, header properties и отдельные heartbeat/timing/marker
diagnostics не проецируются для равенства; перечисленные timing columns записей точны.

| Именованная запись | Результат |
| --- | --- |
| Частный обычный RAW/MESSAGE образец | Полное совпадение отображённой проекции без пропусков. |
| Producer 3.355 `compact-none` и полный `quote-none` | Полное равенство схемы/source/значений/символов/порядка, по три записи |
| Producer 3.356 `compact-none` и полный `quote-none` | Те же проверки закреплённым parser 3.355, по три записи; версия производителя закреплена отдельно |
| Прежние `mixed-stream` / `mixed-raw` | Восемь полных типов 2/8/10/40/56/72/88/120; nullable/пустой/непустой UTF-16, raw scalars и присутствующее MESSAGE-время |
| Прежний `mixed-order` | Перестановка wire-полей, sparse sender 77 и поздний 78 одной записи; сохранены текущие defaults объединения и исходные identities |
| Полный `quote-message` обоих producer pins | Фактическое полное output8-равенство, по три записи, включая все три timing columns. Legacy descriptor/TIME и raw-message-v1 descriptor/MESSAGE сохраняют отказ native exit 4 |

Для producer-случаев сохранена [отдельная неизменяемая provenance](../../../../tests/fixtures/protocol/real-tape/producer/README.ru.md).
Смешанные поля сохраняют прежние [исходники и captures](../../../../tools/upstream/field-types-oracle/README.ru.md).
Частные вход, путь, имена схемы, символы и декодированные строки остаются локальными.
SHA-256 входа совпал до/после; частные ожидаемые значения не переносились в fixtures.

## Завершение и регрессии

В проходе значений production Java нет unknown-data skips. Discovery — отдельный проход
production parser с действительно пустой схемой и NONE-временем; все пропуски
явно отражены и не считаются приёмкой декодирования. Java разворачивает RAW callback в
TICKER/STREAM/HISTORY; компаратор сравнивает каждую wire-запись один раз через STREAM,
разделяя fanout и wire counts.

EOF наблюдается на одном фиксированном потоке реального файла внутри дочернего JVM,
переданном неизменённым production FileReader/parser. Инструментированный проход требует
EOF `-1`, точные байты, одно открытие/закрытие, отсутствие ошибок чтения/закрытия/parser
и пустую очередь sender ID. Отдельный production FileReader по обычному файловому пути
даёт те же точные schema/data строки и hashes; у него нет независимого EOF hook. Оба
наблюдения квалифицированы явно: одно равенство размеров не доказывает EOF.

Windows launcher требует явный native runner, владеющий job до исполнения child. Небольшой
Java worker направляет реальные stdout/stderr target напрямую в raw-файлы; UTC diagnostics
runner сохраняются отдельно. Только diagnostics bootstrap compiler worker объединяются
в UTC-текст. Root deadline действует до выхода владельца и завершения потоков, затем
предусмотрено не более секунды на подтверждённую очистку. Регрессии требуют отсутствия всех
записанных собственных parent/child/worker/runner PID, включая случай выхода родителя, когда
его child унаследовал output handles. POSIX cleanup через этот launcher не квалифицирован.

Native broad inspection подтверждает read, физический EOF, reader finish, write и финальный
flush через borrowed streams. Закрытие native owner до завершения не входит в этот API
и относится к дальнейшей source integration. Это не ослабляет отдельное доказательство
закрытия/ошибок инструментированного Java-источника и не переносит его на native path.

Переносимая [команда регрессий](../../../../tools/upstream/real-tape-acceptance/README.ru.md#запуск)
проверяет положительное равенство и отказы при изменении raw value/type, символа,
sender/local ID, поколения, record/frame order, порядка публикации схемы, лишних записей,
типов схемы, исходных привязок и каждого timing-столбца. Проверяются выбранные обрезания
описаний/данных, отказ для ненулевых flags, бюджеты входа/записей/вывода и native output
pipe, закрытый во время записи inspector. Закрытый sink даёт I/O exit 3, исчерпанный
вывод — 6, malformed truncation — 5. Ни один случай не выдаёт `complete`.

Внедрённая поздняя ошибка сначала возвращает все физические байты входа, затем ошибку
вместо EOF. Production FileReader может перехватить её и вернуться; приёмка всё равно
отвергает результат. Отдельная ошибка после фактического закрытия handle тоже отвергается.
Справка Java/PowerShell проверяется без рабочих входов; неполные аргументы не запускают работу.

Java отформатирована закреплённым google-java-format 1.36.1 с отдельным JDK 23; общие
Format/Check 34 исходников прошли без изменения байтов. Независимое ревью замороженного
исходника и десять отдельных Java probes не нашли P1/P2; финальные runtime-доказательства
учитываются отдельно. Полные stdout/stderr, команды, UTC-интервалы, hashes
classpath/JDK/обоих-Java-sources/worker-и-oracle-classes/runner/inspector/входа и фактические ограничения сохраняются в ignored
локальной metadata. Настроенные heap/time/output limits не являются измеренным RSS или
производительностью.

## Границы приёмки

Обе STREAM/MESSAGE записи приняты явным binary-catalog профилем; четыре отказа старых
режимов остаются видимыми. Положительные случаи подтверждают только именованные обычные
профили. Indexed/time schemas, встроенные event fields, новые полные
типы, ненулевые flags, объекты, сжатие, sidecar/split sources и business projections
не входят в результат. Нулевое число записей или resource stop не закрывают populated
whole-file acceptance. Независимое ревью моста, hashes финальных доказательств и общие
native integration checks учитываются отдельно и не расширяют проверяемую проекцию
schema/data/order/symbol/raw-time.
