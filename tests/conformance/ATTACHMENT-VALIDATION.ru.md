# Проверка решения о переносимых вложениях

[English](ATTACHMENT-VALIDATION.md). Нормативна английская версия.

Запись квалифицирует [переносимую политику](../../spec/events/ATTACHMENTS.ru.md) и текущее обобщённое
сохранение opaque-данных. Типизированные проекции Message/Configuration и регистрация/декодирование
кодеков остаются отдельной реализацией. Независимое ревью контракта и сгенерированной документации
ожидаются; завершение, выпуск и приёмка не утверждаются.

## Выполненные наблюдения

Закреплённый production [Java oracle](../../tools/upstream/attachment-oracle/README.ru.md) прошёл Capture
и отдельный повторный Verify с Java 8u422, heap 128 MiB и пределом 60 секунд на подпроцесс.
14 cases стандартных событий и две идентичности Text дали 29 точных артефактов.
UTC/Pacific-Honolulu совпали; все 1290 classes и 27 resources в 14 явных корнях classpath
и выбранные исходники остались неизменными до/после. [Provenance](../fixtures/attachments/provenance.json)
связывает артефакты и хеши tools/runtime/source. Пересборки QD и сетевого источника не было.
Noargs/EN/RU/Get-Help работают без runtime-входов и рабочих выходных файлов; неполный Verify дал exit 1.

Windows x64 MSVC 19.42 Release/MD и Linux x64 GCC 13.3 Debug ASan/UBSan прошли пять выбранных
CTest: `attachment_serialized_contract`, `attachment_fixture_negatives`, `object_field_owned_values`,
`field_type_java_tape`, `field_type_java_values`. На Linux для этих native-only тестов включён
и LeakSanitizer; покрытие стороннего SDK или JVM санитайзерами не заявляется. Использован один
compiler job, сборки OS последовательны.

Новый тест читает настоящие FileWriter tapes с точными локальными объявлениями стандартных записей.
Он проверяет body/null/empty, отличие сериализованного Java-массива, benign custom serialization,
сохранение повреждённых opaque-байтов, знаковую версию Configuration, уничтожение input/schema/reader,
независимые копии, chunks 1/7/65536, неизвестные записи, zero/exact/limit-minus-one пределы тела,
типизированный RESOURCE_LIMIT и повреждённый усечённый data frame. Подмена null на empty в копии
oracle отклоняется настоящим consumer assertion. Прежние object-регрессии проверяют грамматики
полных идентификаторов и более широкие ошибки allocation/work/ownership.

Отдельные public-header consumers без тестового framework собраны и выполнены MSVC и GCC
с санитайзерами только через публичные include paths. Они проверяют независимые владеющие копии,
различие null/empty и некорректное смешанное представление. Заглушка нативного кодека не выдаётся
за production-реестр. Закреплённые Java formatting и C++ Uncrustify 0.83/clang-format 20.1.8 проверки прошли.

## Ограничения

Custom Java-класс — безвредный fixture-класс, доступный этому oracle JVM. Представление не доказывает
совместимости с произвольными классами. Ошибки getter отделены от успешного wire parsing.
Text-наблюдения подтверждают только идентичность поля. TICKER-фильтрация Configuration, выбор
зарегистрированного кодека, перевод исключений кодека и callback-гарантии — нормативные требования
будущего API, а не реализованное или runtime-квалифицированное поведение этого решения.
Грамматика wire, исходящая поддержка и зависимости не меняются.
