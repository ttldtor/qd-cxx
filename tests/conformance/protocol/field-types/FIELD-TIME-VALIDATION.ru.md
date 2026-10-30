# Валидация времени события FIELD

[English version](FIELD-TIME-VALIDATION.md). Английская версия нормативная.

Явная policy READ задана в [TAPE-TIME](../../../../spec/protocol/TAPE-TIME.ru.md).
Прежние сигнатуры выбирают DISABLED, старые NONE/MESSAGE bytes остаются нормативными expectations,
а прежние formatter versions отклоняют engaged FIELD metadata. CLI grammar FIELD и writer view не добавлены.

2026-10-30 Windows x64 MSVC19.42 Release и Linux GCC13.3 Debug ASan/UBSan прошли полную
CTest suite из62 тестов каждая. Linux leak detection отключён. Все шесть production-only profiles
прошли: minimal, wire-discovery-OFF и JSON-ON обеих платформ, без получения новых зависимостей.
Финальный Doxygen завершился без предупреждений. Независимое ревью приняло контракты владения,
времени жизни, публикации, учёта метаданных и ошибок финального семантического изменения.

## Production-корпус

Отдельные capture/repeat на финальном исходнике исполнили QD3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`, с JDK8 и соответствующими доказанными production
classpath из14 корней. Before/after inventories сохраняют все1317 production files каждого pin.
Repository Java прошла закреплённый google-java-format1.36.1 Format, затем Check до финальных captures.

[Capture manifest](../../../fixtures/encoding/field-types/field-time-captures.json) сохраняет
source/runtime/class/harness и исходные/repeat artifact hashes.
[Baseline](../../../fixtures/encoding/field-types/expected/field-time/) и
[current](../../../fixtures/encoding/field-types/current-field-time-expected/) добавляют по29 артефактов:
25 общих tape с616 observation rows и115 canonical source records, а также фактическую стандартную
индексную TimeAndSale tape и27 строк наблюдений физических slots/projections. Стандартная tape
добавляет пять исходных записей. Каждый capture содержит469 помеченных production record deliveries,
включая отдельные parser/config/FileReader projections; это не469 исходных записей FileWriter.

Пять исходных FileWriter tape покрывают четыре семейства и стандартную индексную TimeAndSale.
Общая writer matrix использует пять packed values и direct parser read-enable false/true с внешним
packed time zero/nonzero, затем FileReader default/NONE/FIELD/MESSAGE. Три сконструированные heartbeat
tape разделяют descriptor time NONE/MESSAGE/FIELD и нулевое/ненулевое/отрицательное/насыщенное крайнее
состояние MESSAGE. Сконструированные raw inputs покрывают отсутствие, first-only/full pair, неверные
полные типы, перестановку, alias, регистр, VOID перед/между/после metadata, полные signed64 physical
words и намеренно неканонические девятибайтовые compact encodings с границами sequence bits.

Шесть настоящих BinaryQTPComposer tape с requested description содержат normal/REMOVE/normal:
распознанную пару, неверные типы sequence/time и VOID перед/между/после. Ведущий обычный EventPrice
и EventSequence неверного типа сохраняют переданное значение на REMOVE; последующий Value получает
нулевой default. Измеренный неизвестный FIELD consumer выбрасывает Java NullPointerException без
callback. C++ сохраняет намеренно более сильную гарантию безопасного пропуска неизвестной записи.
Companion использует production readers/writers; известные физические входные слова обозначены
отдельно от фактической проекции Java cursor.

Полные сравнения638 артефактов capture/repeat и двух pins прошли. Прежние609 expectations не изменены.
Старые57 диапазонов synthetic endpoint UID остаются в установленном in-memory comparison view;
новые FIELD files сравниваются побайтно, ни один raw capture не редактируется.

## Native-проверки владения и ресурсов

Целевой Linux GCC13.3 Debug native run прошёл FIELD и затронутые прежние message-family tests.
FIELD executable читает оба неизменяемых producer sets, сравнивает общие исходные metadata и
доступные обычные business values, восстанавливает явно доказанную по source slots схему/layout
TimeAndSale для сравнения всех logical fields. Владеющие результаты проверяются после уничтожения reader.

Сконструированные native cases также проверяют совместимость адреса прежней фабрики, конфликтующие
business names до lookup, точные packed/millisecond projections, все64 допустимых набора флагов,
недопустимые flags, каждую границу усечения текущей записи, zero/L-1/L/L+1 выходные и field-work budgets,
SIZE_MAX limits, unknown skips и source ordinals всех четырёх families, terminal completed prefix,
allocation failure rollback с сохранением borrowed addresses, каждую промежуточную замену,
фиксированную работу строительства планов и границы snapshot peak old-plus-candidate, сохранённые
обычные имена/типы между prefix/business/prefix generations. TSV/HUMAN и все восемь прежних formatter
profiles отказывают engaged zero metadata до нулевого row budget.

Закреплённые Uncrustify0.83.0, затем clang-format20.1.8 Format/Check прошли для авторского C++.
Clean locked full-reactor governed Verify прошёл все638 артефактов и прежний tape baseline.
Все62 мутации верификатора отклонены:50 прежних и12 FIELD для raw seconds, optional sequence,
projection, order, business values, completion, unknown consumer, raw bytes, extra files,
стандартного indexed time и физических slots. Неизменённая положительная база прошла после мутаций.
Независимые копии Windows и Linux sanitizer binaries также прошли FIELD19 cases/6546 assertions,
message-family14/1266 и session12 cases (4223 Windows/4101 Linux assertions; allocation checkpoints
различаются по платформам). Ревьюер независимо проверил хэши сохранённых production executions,
public fixtures, неизменённых classpaths и финального source. Независимый JVM или повторный запуск
verifier negatives не заявлен. Финальное уточнение комментария заголовка проверено отдельно от
исполненных семантических binaries.

## Ограничения

READ — явный выбор library/protocol. Из него не следуют scheduling, replay, source callback framework,
sidecar, compression, chains, business adapter, conversion, Java object execution или совместимость
с произвольной версией. Discovery не выводит hasTime. Состояние MESSAGE и владение физическим FIELD
остаются отдельными. Пустые timed messages не подтверждают physical EOF или завершение source close.
