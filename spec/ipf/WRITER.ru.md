# Локальный writer instrument profiles

[English version](WRITER.md). Английская версия является основной.

## Наблюдаемый контракт

`composeInstrumentProfiles(span<const InstrumentProfile>, options)` готовит один полный
владеющий несжатый UTF-8 документ без I/O. Expected содержит все bytes, включая
`##COMPLETE\r\n`, либо fixed-size типизированную диагностику; частичный документ не возвращается.
Profiles и getter views заимствуются только на вызов. Вызывающая сторона сериализует изменения;
callbacks, сохранённых spans, очередей, фоновой работы и глобальных настроек нет.

Совместимость означает canonical `getField` text, точные type/custom names, порядок profiles и
CSV escaping по фактическим pinned Java writer/composer и reader. Raw presence, биты signed zero
и исходное написание чисел не сохраняются: пустые/default fields следуют модели значений при
чтении. Java writer сортирует headers по UTF-16 type, стандартные fields — по enum с обязательным
SYMBOL, затем custom names — по UTF-16. Fields объединяются для type, profiles сохраняют входной
порядок. CSV заключает cell в кавычки ровно при comma, quote, CR или LF; кавычки удваиваются,
встроенные newline bytes точны, разделители records — CRLF.

Пустой input даёт только marker завершения. Поддержаны unknown, empty и quoted types, включая
обычные names с начальным '#'. Непустые custom names и пустое custom name чувствительны к регистру
и отличаются от стандартных names.

## Явная native-граница точности

Модель значений сохраняет представимость всех прежних значений. Writer до публикации отклоняет
только фактически наблюдаемые классы:

- INVALID_TYPE: точные types `##`, `##COMPLETE` либо type с начальным '#' и окончанием '::=TYPE'.
  Результат actual Java writer для них отклоняет его production reader.
- UNREPRESENTABLE_UTF16: непарный high/low surrogate в type, name или value. Actual Java UTF-8
  output заменяет его на '?', теряя canonical text. Корректные surrogate pairs сохраняются точно.
- REMOVED_FIELDS: REMOVED с непустыми данными помимо TYPE/SYMBOL. Actual Java composer отбрасывает
  дополнительные fields. Обычные REMOVED tombstones и пустые/default extra fields работают.

Это согласованные ограничения native writer, а не утверждение об отказе Java для всех этих inputs.
Нет тихой normalization/filtering или эквивалентности для каждого состояния модели значений.
Actual writer bytes и Java reader outcomes сохраняет
[writer oracle](../../tests/fixtures/ipf-writer/README.ru.md).

## Границы и владение при ошибке

Инженерные defaults настраиваются на вызов:100000 profiles,256 types,256 fields на format,
включая TYPE,1Mi retained format UTF-16 units,16Mi суммарных input retained UTF-16 units,
64Mi output bytes и32Mi logical work units. Это инженерные значения, не предпочтения нагрузки
владельца. Для profile до копирования custom names учитываются actual retained standard text и
custom names/values, посещения standard/custom fields. Сначала проверяется actual nonempty custom
count. Копия names ограничена учтёнными retained units и count profile; format copies имеют свой
aggregate ledger. Overflow-safe проверки предшествуют росту. Logical work учитывает просмотренные
UTF-16 units, field visits и emitted bytes, но не задаёт точный instruction count, capacity/RSS
allocator или deadline блокирующей I/O. Неуспешная работа не возвращается. Allocation failures дают
ALLOCATION_FAILURE; частичный владеющий документ не публикуется.

Диагностика указывает zero-based profile и field, где они известны; для её сохранения не нужно
выделять строки. Input values не меняются при успехе/ошибке. Public metadata `retainedUnits()`
измеряет standard text плюс custom names/values в UTF-16 units без config/container overhead;
`customFieldCount()` считает retained nonempty custom values, включая возможное пустое name.
Эти getters ничего не выделяют.

## Локальная публикация

`writeInstrumentProfiles(ostream&, profiles, options)` готовит body и marker целиком до первой I/O.
Заимствованный stream используется только во время вызова, не закрывается/seeks/откатывается;
маска исключений сохраняется. Body записывается и flush выполняется до попытки marker, затем
записывается/flush marker. Успех означает успех всех операций. После каждого write/flush проверяется
fail/bad, включая исключения, подавленные iostream. Вышедший наружу bad_alloc даёт
ALLOCATION_FAILURE; подавленное исключение, видимое только как stream failure, даёт IO_ERROR.
Состояние stream не очищается ради сокрытия ошибки.

`confirmedBytes` учитывает лишь полностью успешные ostream.write calls. Неуспешный вызов уже мог
произвести внешние side effects: это нижняя граница, не точный physical cursor. Любая ошибка даёт
complete=false. Поздняя ошибка marker/flush может оставить внешние bytes с полным marker;
rollback, transaction и atomic external-stream publication не обещаны.

`writeInstrumentProfilesFile(path,...)` также готовит всё до open, использует собственный binary/
truncating ofstream и закрывает только его. После успешного open прежнее содержимое может быть
потеряно. Ошибки open/write/flush/close наблюдаемы; complete=true только после успешного owned close.
Ошибка подготовки не открывает/не обрезает файл. Нет temporary-file rename или filesystem atomicity.
URLs, HTTP, compression, live collection и writer callbacks не входят в API.

