# Production-наблюдения STREAM_DATA

[English version](COVERAGE.md). Английский документ является нормативным.

[Inputs](inputs.tsv), [фактический вывод Java](expected/stream-data.tsv), [валидация](VALIDATION.md),
[harness](../../../../tools/upstream/stream-data-oracle/StreamDataOracle.java),
[контракт](../../../../spec/protocol/STREAM-DATA.md).

17 входных сценариев дают 35 независимых наблюдений parser, включая 18 строгих префиксов. Каждый
префикс обёрнут новым полным кадром type-15; предшествующее описание type-2 остаётся полным.
Программная локальная схема: S=0(v), R=1(y,x,z), E=2(); явный alias xdot указывает R.x. Sender plans:
7=R(x,unknown,y,xdot,x), 9=S(v), 31=Unknown(a,b), 4096=E(), 11=R(), 12=R(x).
Все поля — точный type 8. Metadata, external event time и field replacers отключены.
Описания создаются Java writers; data payloads — вручную заданные воздействия. Oracle — доставка
настоящего BinaryQTPParser: ожидаемые записи не получены диагностической реимплементацией или C++.

| Сценарии | Наблюдаемая поверхность |
| --- | --- |
| mixed | Sender/local ID различаются; x/alias/duplicate writes заканчиваются x=44, y=22; отсутствующий z=0. Неизвестное среднее поле и неизвестная запись потребляют все values. ABC повторяется через unknown record и S; A/NUL/B повторяется через E. R без wire fields доставляет три raw-zero slots. |
| small, skip-prefix, их строгие префиксы | Truncation payload полного кадра внутри символа, ID, целевого и пропускаемого values; префикс после полной unknown record успешен без доставки. Изолированный high surrogate сохраняется через skip неизвестной записи и repeat в S. |
| empty | Пустое полное data message успешно без записей/callbacks. |
| late-failure | Валидная S предшествует обрезанному полю R; Java сообщает corruption и не доставляет записей этого сообщения. Сохранённая S в C++ — проектная политика результата. |
| undescribed, negative-id, invalid-symbol, truncated-id | Message corruption без записей; отдельные C++ категории/позиции — проекции, а не Java diagnostics. |
| nonzero-flags, remove-event | Java принимает raw flags 1 и 2. REMOVE_EVENT использует альтернативный time path. Оба явно не поддерживаются C++. |
| zero-flags | Явный нулевой flag prefix успешен. |
| reset-valid, reset-repeat | Два валидных сообщения доставляют две записи одним callback при whole input; repeat в начале второго сообщения ошибочен, запись первого сообщения доставляется. |
| string-forms | FC NUL, FD supplementary surrogate pair и изолированный low surrogate сохраняют точные units. |
| boundaries, overlong | Границы знака/ширины compact, int extrema, значения за пределами 32 бит и signed 64-bit extrema; overlong ID/value forms; наблюдаемое low-32-bit локальное хранение. |

Записи содержат UTF-16 hexadecimal без потерь вместо чувствительного к разделителям текста символа.
Числа callbacks и ошибки сохранены отдельно от записей и финальной внешней input position. Source evidence
BinaryRecordDesc.readFields устанавливает readCompactLong перед FLD_INT narrowing, включая skipped fields;
промежуточные перезаписанные values отдельно не наблюдаемы. P01/P03/P04 и binding captures остаются
неизменёнными историческими доказательствами. Полные optional flags/time, conversion, dynamic session,
provider, огромные ресурсы, transport и recovery profiles остаются открытыми.
