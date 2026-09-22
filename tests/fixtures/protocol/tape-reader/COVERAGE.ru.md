# Покрытие Java для tape reader

[English version](COVERAGE.md). Английская версия является основной.

Все 51 вход используют production FileReader зафиксированного QD 3.355: явный BINARY, сжатие NONE,
MAX_SPEED, cycle=false и настоящую локальную DefaultScheme S/R/E. [Harness и команды](../../../../tools/upstream/tape-reader-oracle/README.ru.md).
Входы кроме original — диагностические байтовые стимулы; выходы — реальные production callbacks,
не диагностический парсер. [Валидация](VALIDATION.ru.md) идентифицирует чистые сборки и сравнение.

| Случаи | Наблюдение / покрытие |
| --- | --- |
| original | Неизменённая 137-байтовая FileWriter tape QDCXX-019; семь записей по порядку, поздние ID, перестановка локальных R-полей, крайние числа, Unicode и E без полей |
| empty, header-only, no-footer, missing-header | Java возвращается без ошибок; missing-header/no-footer доставляют семь, empty/header-only — ноль |
| repeated-header, duplicate-id, redefined-id | Java возвращается без ошибок и доставляет семь; замена не является политикой C++ только с добавлением |
| unknown-record, undescribed-id | Неизвестная описанная запись пропускает обе записи без ошибки; неописанный ID сообщает одну ошибку сообщения без доставки |
| partial-update | Повреждённое полное description-сообщение устанавливает первое завершённое описание; доставляются две последующие записи и сообщается одна ошибка сообщения |
| footer-then-data, explicit-zero | Девять и семь записей соответственно без ошибок; пустой маркер не подтверждает конец |
| properties-0..12 | Отсутствие opt/type/version; непустое opt; NONE/пустое/MESSAGE time; неизвестное свойство; повторы type/opt; null opt; пустые version/type. Все доставляют семь без ошибок. Повтор type оставляет последнее значение; null наблюдаем |
| send-empty, send-raw, receive-stream | Семь записей без ошибок с фактическими наборами дескриптора; это не требование поддержки этих наборов в C++ |
| extension | Одна ошибка сообщения, затем семь записей; хвостовой zero — намеренно повреждённое endpoint-расширение |
| negative-properties, negative-send, negative-receive, bad-magic | Одна ошибка сообщения, доставленных записей нет |
| truncated-1/2/20/57/59/65/68/74/80/100/135 | Частичные конверты header/description/data: одна ошибка потока. Префикс — ноль записей до 74, две на 80/100, четыре на 135 |
| malformed-header-0/1/4/5/49, malformed-data | Полные конверты с усечёнными примитивами payload: одна ошибка сообщения, доставок нет |

Свойства используют счётчик compact-long, списки сообщений — compact-int; элементы содержат ID,
UTFString имя и таблицу свойств. Исходники: BufferedInput.readProperties,
ProtocolDescriptor.parseFrom/parseMessageListFrom и MessageDescriptor.parseFrom из upstream.lock.
Runtime-пробы проверяют обычные элементы, наборы, свойства и отрицательные счётчики; экстремальное
выделение памяти, все варианты некорректного Unicode/ширины счётчиков и общая negotiation не заявлены
как выполненные Java-проверки. Прежние эталоны примитивов отдельно покрывают дополнительные кодировки.

Java callbacks не сообщают позиции примитивов дескриптора. `case` содержит точный размер всего входа;
индексы `record` — порядок доставки, не байтовые позиции. Проектные C++ consumed/error проверяются отдельно.
`complete` observer означает завершение ограниченного эксперимента, включая ожидаемые ошибки.
Совместимость восстановления, сжатия/временных sidecar/default connector/Quote/транспорта не установлена.
