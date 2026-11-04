# Свидетели стандартных сериализованных вложений

[English](README.md). Нормативна английская версия.

`capture-v1` содержит оригинальные production-наблюдения закреплённого QD3.355.
[Provenance](provenance.json) связывает upstream sources, harness, launcher, Java runtime,
production class/resource inventory и все 29 артефактов.
[Oracle](../../../tools/upstream/attachment-oracle/README.ru.md) воспроизводит их без пересборки QD.

Каждая из 14 `event` TSV строк содержит case, настоящий wire record ID, имя поля, полный тип 249,
nullable hex тела, результат явного Java getter, версию Configuration и точный composer wire hex.
`null` и `empty` различаются. `.qtp` — настоящий composer output стандартной схемы, проверенный
parser и делегатом; `.tape` — отдельный production FileWriter output из того же event/delegate
cursor. Две строки `text-field` фиксируют только независимую стандартную идентичность Text 10.

Оба события проверяются с null, String `hello`, пустым/непустым Java byte[], безвредным custom
Serializable, engaged-empty raw и повреждёнными raw `010203`. Raw формы сохраняются при production
wire parsing, но явный Java Object access выдаёт MarshallingException. Пустой Java byte[] занимает
27 сериализованных байт с началом `aced0005`, отличаясь от engaged-empty. Версия Configuration `-7`
проходит через настоящий делегат; фильтрация не проверяется. Custom-класс oracle — свидетель
представления, а не production-нативный кодек или обещание произвольного Java Object.

Постоянный CTest `attachment_serialized_contract` читает FileWriter tapes публичным обобщённым
TapeReader. Он проверяет точные владеющие значения после уничтожения reader/schema, chunks
1/7/65536, независимое изменение копии, неизвестные записи, точные пределы тела и усечённый
data frame. Composer-артефакты дают точное усечение независимо от завершающих маркеров FileWriter.
Типизированные проекции, реестр и Java-декодирование не входят в доказанный результат теста.
См. [контракт](../../../spec/events/ATTACHMENTS.ru.md).

[Запись проверки](../../conformance/ATTACHMENT-VALIDATION.ru.md) отделяет выполненные обобщённые
проверки от требований будущего типизированного API.
