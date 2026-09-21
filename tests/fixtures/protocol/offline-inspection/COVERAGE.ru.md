# Доказательства офлайн-композиции и разбора Java

[English version](COVERAGE.md). Английская версия является нормативной.

[Входы](inputs.tsv), [наблюдения Java](expected/offline-inspection.tsv),
[сырой бинарный поток](expected/composed.qtp), [валидация](VALIDATION.ru.md).

Дополнительный [harness](../../../../tools/upstream/offline-inspection-oracle/OfflineInspectionOracle.java)
использует production BinaryQTPComposer и BinaryQTPParser locked QD 3.355, не вывод C++ и не
диагностическую переимплементацию. Composer создаётся с отключённым автоматическим чередованием
описаний; harness вызывает его настоящий protected describeRecord для всех четырёх записей
отправителя внутри beginMessage/endMessage, затем дважды использует production visitData.
Пустые области композиции сворачиваются production composer. Это явное офлайн-управление,
не инструментирование parser. Upstream-исходники не меняются. Записи отправителя:
R=0(x,unknown,y,xdot), S=1(v), Unknown=2(a,b), E=3(). Локальный профиль — S=0(v),
R=1(y,x,z) с xdot→x, E=2(), все поля обычного type 8.

Восемь входных записей двух сообщений дают семь наблюдаемых локальных записей в одном callback;
неизвестная запись пропускается. Четыре начальных кадра описаний и два data занимают 116 байтов.
Измерены remapped ID, пропуск неизвестных поля/записи, порядок y/x, итоговая запись alias,
отсутствующее z=0, E без полей, повтор ABC, сброс символов между сообщениями, NUL, изолированные
старший/младший суррогаты, пустой символ и границы int32. Сырой .qtp не содержит tape header/container.
EMPTY_SET задан явно; metadata, field replacers и внешнее event time отключены. Пустой вызов
visitHeartbeat(new HeartbeatPayload()) не выдаёт байтов в этом composer; покрытие пустого heartbeat
в CLI использует спроектированные fixtures, не выдуманный Java callback.

Существующие доказательства [STREAM_DATA](../stream-data/COVERAGE.ru.md),
[binding](../binding/COVERAGE.ru.md) и [symbols](../../encoding/symbols/COVERAGE.ru.md) неизменны
и дают более широкие наблюдения type-8/malformed/symbol. Их семантика используется повторно;
это не новые исполнения здесь. Промежуточные повторные записи подтверждены исходниками;
callbacks показывают только итоговое значение.

[designed.json](designed.json) и [спроектированные потоки](designed/) — явно авторские ожидания
политики CLI, не Java captures. Они покрывают строгий порядок, ошибки/позиции, границы ресурсов,
вывод завершённого префикса, неканонические заголовки и heartbeat-конверты. Начальные 53 случая
предшествовали реализации CLI; ещё 19 граничных случаев добавлены при валидации. Ожидаемая
семантика записей не получена исключительно из проверяемого executable.

Не заявляются importer, transport, dynamic session, conversions, поддержка ненулевых flags,
огромных аллокаций, timing или полная совместимость протокола. Успех пустого файла — проверка
политики, не содержательное семантическое покрытие.
См. [conformance инструмента](../../../conformance/protocol/offline-inspection/COVERAGE.ru.md).
