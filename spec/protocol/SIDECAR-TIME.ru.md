# Ограниченный scanner LONG/TEXT companion

[English](SIDECAR-TIME.md). Английский текст нормативный.

Tool-private `SidecarTime` разбирает конечный logical `.time` stream, владеет ограниченным line
storage и возвращает владеющие timestamp/position marks. Он не открывает файлы, не распаковывает
containers, не планирует replay, не вызывает callbacks, не выбирает schemas и не меняет время
protocol reader. CLI integration — отдельная работа; protocol-header admission выбирается отдельно
описанной ниже политикой. Scanner сам по себе
не включает LONG/TEXT в существующих readers и не меняет profiles/defaults.

## Представление и конечная грамматика

Java `TimestampedPosition.writeTo` пишет LONG как decimal absolute milliseconds, `:`, signed decimal
logical data position и платформенный перевод строки. LONG — текст без binary byte order. TEXT
использует `YYYYMMDD-HHmmss.SSS+HHMM` либо `-HHMM`, кроме эпохи0 в виде `0`; за пределами calendar
range Java может вывести decimal milliseconds. Обе native format labels принимают одно объединение:

- canonical nonnegative decimal milliseconds от0 до INT64_MAX без знака и лишнего начального нуля;
- ровно24 calendar bytes: год0001..9999, корректная proleptic Gregorian дата, час00..23,
  минуты/секунды00..59, миллисекунды000..999 и обязательная numeric zone hour00..23/minute00..59;
- position в полном signed64 domain с необязательным `+`/`-`, без окружающих пробелов.

Absolute preepoch TEXT допустим: `19691231-235959.999+0000:0` означает-1ms. Numeric negative time —
`UNSUPPORTED_TIME`: Java трактует его относительно ambient clock, а не как absolute signed timestamp.
Scanner не использует ambient clock/timezone/locale. Relative durations, даты без зоны, прочая
Java-friendly грамматика, пробелы и BOM вне профиля. Неверное time/position и integer overflow дают
typed errors, не floating-point values.

Последнее двоеточие разделяет time и position. Строка без двоеточия пропускается, но ограничивается.
CR/LF/CRLF завершают строки; CRLF считается одной строкой между любыми feed boundaries. Последняя
строка без terminator разбирается один раз на logical EOF. Empty input/empty valid companion
допустимы. Calendar arithmetic ограничена годами профиля. `SidecarMark::packed` отдельно сохраняет
точный `timestampMs`, проецируя Java cursor time через saturated signed32 seconds и floor-mod1000
milliseconds с нулевыми оставшимися sequence bits.

## Позиции и хронологическое применение

Позиции относятся к inflated logical DATA bytes, включая protocol header/descriptions до сжатия.
Companion logical bytes имеют отдельную координату. Ни одна не имеет точного обратного отображения
на произвольный compressed byte; physical read-ahead, compressed consumption и inflation — отдельные
container diagnostics.

Caller владеет максимум одним pending mark, подаёт существующие data bytes до его позиции,
публикует complete parser results и применяет достигнутые marks по source order. Negative/equal/
decreasing positions применяются сразу; negative absolute TEXT и decreasing times допустимы.
`reached` безопасно сравнивает signed position без unsigned subtraction.

Effective time следует событиям: достигнутый mark заменяет external time; более поздний MESSAGE
heartbeat заменяет его; новый mark снова меняет время. Mark внутри incomplete frame влияет на
весь frame при завершении, включая физически предыдущие записи этого frame. Mark на frame end
влияет на следующие frames. Nonzero external packed заменяет delivered FIELD packing; external zero
возвращается к отдельно владеющим raw FIELD metadata. Обычные business fields `Time` независимы.
Scanner сам эту parser/sink integration не выполняет.

## API, владение и terminal states

`SidecarTime(format, limits)` — `noexcept`, без allocation/I/O, с немедленной проверкой enum и
representation constraints. Invalid config сохраняет fixed sticky diagnostic. Нулевые limits —
допустимые нулевые ёмкости. Line storage выделяется лениво после input/line/retained/work allowance
checks; allocation failure становится `ALLOCATION`.

`feed(span)` заимствует bytes только на время вызова, возвращает consumed prefix и максимум один
owning mark; caller повторно подаёт непринятые bytes до следующего read. Scanner не сохраняет caller
span/stream/decoder/schema/callback. Marks переживают scanner/source destruction. Empty feed до EOF
не работает с input. Statuses: `NEED_INPUT`, `MARK`, `END`, `FAILED`; `FAILED` избегает Windows macro
`ERROR`, header допустим после `windows.h` без изменения caller macros.

`endInput()` объявляет caller-confirmed logical EOF без I/O. Final unterminated mark возвращается
один раз, затем `END`; повторный успешный END идемпотентен. Feed после EOF — sticky INVALID_ARGUMENT.
`finish(dataLogicalBytes)` требует END, сверяет maximum nonnegative position с actual data length и
фиксирует первую data EOF length. Повторные успешные finish не работают с input и игнорируют новый
length. Ранний finish — sticky NOT_FINISHED. Поздний запрещённый feed всё равно отменяет scanner
success. Первая ошибка sticky: дальнейшие feed/endInput/finish принимают0bytes, не публикуют mark,
не выделяют память и не работают с input. Destructor не выполняет drain/close. END и scanner finish
подтверждают только syntax/position.

## Явный protocol-header admission

`TapeExternalTimePolicy::{DISABLED,SIDECAR}` выбирает только admission. Overloads с обязательным
последним аргументом:

```cpp
TapeReader(schema, limits, mode, fieldTimePolicy, externalTimePolicy);
TapeReader(limits, discovery, fieldTimePolicy, externalTimePolicy);
```

`SIDECAR` требует `BINARY_CATALOG_V1` и принимает только `LONG`, `long`, `TEXT`, `text` в initial
headers и active descriptor deltas. Admission NONE/MESSAGE неизменён; FIELD независимо требует
`FieldTimePolicy::READ`. Mixed case и другие tokens сохраняют typed TIME refusal. Raw time text
сохраняется во владении без normalization. Все старые constructors делегируют `DISABLED`, сохраняя
call signatures/defaults; посторонние factories неизменны. Invalid enum/mode отвергаются до owning
schema/framing allocation.

Policy хранит только fixed enum tag, не добавляя source/provider, clock, I/O, external timing state
или setter. Raw FIELD values, MESSAGE heartbeat timing, partial-frame publication и parser finish
не меняются. Owning tool применяет хронологические sidecar/heartbeat events и проверяет companion/
container/physical closure. Сам LONG/TEXT header ничего из этого не подтверждает.

## Учёт ресурсов

| Предел | Конечный default | Учёт |
| --- | --- | --- |
| Input | 4MiB | Consumed logical companion bytes, включая CR/LF |
| Line | 128bytes | Тело одной строки, включая ignored lines |
| Lines | 262144 | Attempted terminated/final unterminated lines |
| Marks | 262144 | Returned owning marks, включая repeated positions |
| Work | 8MiB | Input examination и admitted parse-character work |
| Retained | 128bytes | Configured line scratch; fixed object state исключён |

Все нули буквальные, не unlimited/default. Empty source может завершиться с нулевыми ёмкостями;
nonempty input с нулевым line/retained allowance отклоняется до scratch allocation/examination.
Counters не возвращают skipped/failed/returned work. Untrusted line/position не задаёт reserve size.
Decimal accumulation проверяет overflow до multiplication/addition. Unsigned charges проверяют
remaining allowance до addition; signed subtraction не сравнивает data/mark positions. Counters
различают consumed bytes, work, attempted lines, returned marks и maximum nonnegative position.
Diagnostics владеют scalar line/column/logical companion offset checkpoints и static stage.

Ошибки различают configuration, input/line/count/mark/work/retained/resource bounds, allocation,
malformed time/position, unsupported time, position beyond data и premature finish. Physical source
I/O, container CRC/trailer, physical close и sink flush остаются отдельными owner errors.

## Граница владельца закрытой пары

Closed-pair owner проверяет ВЕСЬ companion syntax и каждый nonnegative mark против final data
length. Missing required companion — source failure; marks за data EOF и malformed unused tail не
допускают complete. Это намеренно строже Java FileReader, который может игнорировать future mark tail
и логировать часть open/close errors без исключения. Empty valid companion допустим. Нормальный
возврат FileReader не подтверждает завершённость обоих файлов.

Owner уничтожает оба borrowed container adapters до закрытия любого physical stream. Каждый
opened member получает close attempt даже при ошибке первого close/второго open. Первая observed
parse/source/cancellation cause остаётся primary при cleanup. Separate bounded close/flush
diagnostics сохраняют secondary failures; cleanup не заменяет прежнюю причину/не возвращает успех.
Без прежней причины первая cleanup failure становится primary.

COMPLETE требует parser finish, обеих container integrity/physical EOF проверок, scanner END/position
finish, required segment closure, обоих physical closes и sink flush. Container может вернуть bytes
вместе с late error: сначала подаётся valid prefix, найденная source error сохраняется. Cancellation/
sink refusal запрещает дальнейшие reads/callbacks, companion drain и file transition. Cleanup всё
равно освобождает adapters и закрывает opened streams без скрытых reads. Узкий native owner harness
проверяет этот lifecycle отдельно от будущей CLI integration.

Java выводит имена `a.qtp`=>`a.time`, `a.qtp.gz`=>`a.time.gz`, `a.qtp.zip`=>`a.time.zip`; у каждого
companion свой container/integrity/budget state. Native owner path resolution вне scanner. Настоящее
Windows/POSIX Java naming использует native separator: slash на Windows/backslash на POSIX не
взаимозаменяемы в extension helper. Remote URLs, wildcard discovery и общая Java filename
compatibility вне explicit local closed-pair profile.

Тесты: [scanner](../../tests/conformance/sidecar_time_tests.cpp),
[protocol admission](../../tests/conformance/sidecar_policy_tests.cpp),
[Windows include order](../../tests/conformance/sidecar_windows_include_test.cpp),
[allocation ordering](../../tests/conformance/sidecar_allocation_test.cpp),
[paired owner](../../tests/conformance/sidecar_owner_tests.cpp). Production-source basis:
`TimestampedPosition`, `FileReader.process`, `FileWriterImpl.writeData`, `FileUtils`, `TimeFormat`,
`TimeSequenceUtil` в версиях [upstream.lock](../../upstream.lock).
