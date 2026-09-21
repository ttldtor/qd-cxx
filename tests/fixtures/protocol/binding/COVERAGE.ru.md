# Наблюдения штатного binding

[English version](COVERAGE.md). Английская версия является основной.

[Harness](../../../../tools/upstream/binding-oracle/README.md), [inputs](inputs.tsv),
[точные наблюдения](expected/binding.tsv), [валидация](VALIDATION.md).
13 сценариев с новым parser содержат 16 stimulus rows. Двенадцать описанных сценариев дают 15
независимых проекций планов; undescribed сравнивает ошибку parser, а не вход planner. Локальный порядок:
S=0, R=1, E=2; поля R: y=0, x=1, z=2. Явный alias xdot указывает на x и становится его Java property name.
Все поля — обычные COMPACT_INT, без time/object fields. Metadata/replacers/properties отключены.

| Сценарий | Штатное наблюдение |
| --- | --- |
| reorder | Sender 7 доставляет R=1, y=202, x=101. |
| alias / full | xdot / R.x находят x; доставлены x=303 / 505, y=404 / 606. |
| middle-skip | Неизвестное поле 808 между x=707 и y=909 не нарушает выравнивание. |
| missing-wire | x=111; Java storage y/z равен нулю, это не обещание defaults приложения. |
| zero | Описанный R без wire fields всё равно доставляет R с нулевыми Java slots. |
| empty-local / empty-both | У E=2 нет локальных полей; одно пропущенное значение / отсутствие значений доставляют E. |
| duplicate | x,xdot,y,x=(333,444,555,666) доставляет y=555,x=666. |
| sparse | Sender 4096 доставляет R=1 с y=888,x=777. |
| alignment | Неизвестная запись sender 31 потребляет 123,234; затем S=0(v=345), R=1(y=456,x=567) доставляются по порядку. |
| case-sensitive | Строчная r пропущена; прописная X отсутствует в R; y=890 сохраняется. |
| undescribed | Data ID 19 без описания вызывает corrupted-message callback типа 15 без записей. |

Все успешные сценарии заканчиваются без pending bytes/errors. Лог неизвестной записи сохранён в raw stderr.
Планы отдельно разработаны в [plans.tsv](../../../conformance/protocol/binding/plans.tsv), а не получены
как Java callbacks. Аудит сопоставляет raw payloads
и проецирует входные значения через планы на действительно доставленные ID/порядок/значения.
Промежуточные перезаписанные значения duplicate независимо не наблюдаются: BinaryRecordDesc.field2Desc/readFields
из [SOURCES](../../../../spec/protocol/SOURCES.md) подтверждают сохранение всех действий в wire order.
Пропуск неизвестной записи подтверждается дальнейшим выравниванием, а не прямым skip callback.

Исторические P03/P04 используются через [CAPTURES](../../../../spec/protocol/CAPTURES.md);
их файлы не переписаны и не перезапускались. XML importer, defaults, conversions, dynamic replacement,
reset, metadata, provider matrix, transport и полная protocol parity не подтверждаются.
