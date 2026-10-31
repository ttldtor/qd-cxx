# Проверка нативных подписок производственным парсером

[English version](README.md). Английская версия нормативная.

Необязательный developer tool передаёт ограниченный синтетический нативный вывод закреплённому
Java BinaryQTPParser. Фактические callbacks и readSubscriptionTime сравниваются с независимыми
ожиданиями входов нативного теста. Сокеты не открываются. Обычная native сборка/CTest Java не запускает.
Границы заданы [wire/API контрактом](../../../spec/subscriptions/WRITER.ru.md) и
[профилем захвата](../../../tests/fixtures/subscriptions/writer/README.ru.md).

Нужны CMake 3.25+, Git, явный JDK 8, чистый checkout по [upstream.lock](../../../upstream.lock)
и доверенные подготовленные классы/квитанция [подготовки схем](../../schema-export/README.ru.md).
Подготовка может скачивать Maven зависимости; Verify.cmake не скачивает и не собирает upstream,
ничего не устанавливает глобально. Непустые JAVA_TOOL_OPTIONS, _JAVA_OPTIONS и JDK_JAVA_OPTIONS
запрещены. Настройка native тестов отдельно может скачивать закреплённый doctest.

## Воспроизведение

Из корня репозитория; замените иллюстративные пути к установленным инструментам/checkout:

```powershell
cmake -DHELP=ON -P tools/upstream/subscription-writer-oracle/Verify.cmake
cmake -S . -B .work/subscription-tests -DBUILD_TESTING=ON -DORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON=OFF
cmake --build .work/subscription-tests --config Release --target qdcxx_subscription_writer_tests
ctest --test-dir .work/subscription-tests -C Release -R '^protocol_subscription_writer$' --output-on-failure
$repo = (Get-Location).Path.Replace('\', '/')
cmake "-DJAVA_HOME=C:/tools/jdk8" "-DUPSTREAM=C:/work/QD-locked" `
  "-DRECEIPT=$repo/.work/schema-export/prepare/classpath-receipt.json" `
  "-DNATIVE=$repo/.work/subscription-tests/subscription-native-captures" `
  "-DRUN=$repo/.work/subscription-java-check" -DMODE=Verify `
  -P tools/upstream/subscription-writer-oracle/Verify.cmake
```

NATIVE содержит созданные тестом native.qtp, expected.tsv и expected-time-reads.tsv.
Тест пишет лишь в настроенный каталог своей сборки, а не в эталоны Git.
Новый test CLI не вводится; обычная справка doctest не выполняет случаи.

Для работы обязательны JAVA_HOME, UPSTREAM, RECEIPT, NATIVE и RUN. RUN новый и отделён от входов,
upstream и эталонов. MODE по умолчанию Capture, но всё равно сверяет оба Java-выхода с ожиданиями
нативных входов. Verify дополнительно сравнивает пять файлов с EXPECTED, по умолчанию
tests/fixtures/subscriptions/writer/expected. Публикации/изменения эталонов нет.
Без аргументов и с отдельным -DHELP=ON выводится одинаковая справка без providers/input/work;
это настройка скрипта, поскольку глобальный --help принадлежит CMake.
Код 0 — справка/успех, 1 — ошибка аргументов, происхождения, компиляции, парсера или сравнения.

RUN сохраняет классы, JVM/provider receipts и raw logs. Компиляция ограничена 60 секундами,
parse 30 секундами, help probes 15 секундами. Child heap 256 МиБ не является лимитом RSS.
До/после должны совпадать хеши source/provider/native input. Байты сравниваются точно,
без нормализации, включая генерируемые LF metadata и исходный нативный бинарник.

## Прямой helper

`java -Xmx256m -cp CLASSPATH qdcxx.oracle.NativeSubscriptionParser --input FILE --output NEW_DIRECTORY`
требует обе опции в любом порядке. Отдельный --help и отсутствие аргументов дают одинаковый
UTF-8 stdout, пустой stderr и exit 0 даже без QD providers. Неверные/повторные аргументы — exit 3;
I/O, существующий выход, parser/resource failure и отказ вывода справки — exit 1.
Вход и существующий выход не перезаписываются. Ошибка записи может оставить частичные новые файлы;
успешен только exit 0.

Receiver содержит Indexed 0 (timed hi/lo/x), Beta 1 (x/y), Alpha 2 (x), все поля compact int.
Разрешены только описания и шесть subscription types. Limits: input 1 МиБ, 4096 messages/tuples/
time reads, 64 descriptions, sender IDs 0..65535, 4096 units на symbol и 65536 суммарно.
Unknown records/types, corruption и incomplete EOF — отказ. Производственный descriptor parser
может выделять field arrays до проверки всего тела, поэтому heap bound нужен отдельно от byte bound.

observations.tsv содержит фактические message/index/receiver ID/name/symbol/raw time.
Имена/символы — lowercase UTF-16 hex по четыре цифры на unit; empty — пустая ячейка, не null.
time-reads.tsv содержит настоящие protected production reader calls. Оба файла UTF-8/LF/no BOM.
Фиксированный receiver — эталонная фикстура, не общий внешний schema loader и не полный live client.
