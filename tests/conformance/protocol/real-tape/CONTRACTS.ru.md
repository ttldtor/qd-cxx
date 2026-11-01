# Контракты проверки общего бинарного каталога

[English version](CONTRACTS.md). Английская версия нормативная.

Статус: обязательные проверки [конечного бинарного каталога](../../../../spec/protocol/BINARY-TAPE-COVERAGE.ru.md).
Документ задаёт приёмку, а не сообщает об исполнении этих проверок.
[Матрица](../../../fixtures/protocol/real-tape/support-matrix.tsv) разделяет существующие доказательства,
ожидаемую реализацию, выбранные исключения и обязательные production positive/negative observations.
[Решение](../../../../docs/decisions/ADR-0004-broad-binary-tape-reading.ru.md) задаёт границу.

## Правила эталонов и результатов

Каждый новый применимый production case выполняется с QD3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и QD3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Использовать реальные FileWriter/BinaryQTPComposer и
FileReader/BinaryQTPParser с явными schemas/settings. Изолированная primitive probe или diagnostic
decoder не заменяют production message-path observation. Идентичный source code может объяснять
наблюдение, но не превращает pending-тест другой версии в выполненный.

Для каждой строки сохранять переносимые deterministic inputs, точные source/helper/runtime/classpath
hashes, canonical production observations и C++ expected status/value/order/flags/time. Сохранять
исторические captures и `upstream.lock`. Частные recordings, symbols, paths и полные локальные логи
не входят в публичные fixtures. Различать immutable source facts, planned tests, Native execution
и независимо повторённые проверки. В actual-evidence не указывать ещё не существующий будущий путь.

[Producer validation](producer/VALIDATION.ru.md) — существующее Windows Java evidence для конкретных
compact/Quote ordinary STREAM_DATA cases и усечения. Оно не квалифицирует C++ parity и прочие строки.
Новые expectations требуют review, переноса неизменённых bytes и свежего Verify.

Для Java-valid alternative вне выбранного профиля фиксировать Java behavior и ожидаемый C++
UNSUPPORTED. MALFORMED оставлять для ошибочной grammar/state выбранного контракта. Неизвестные
types/flags/messages не принимаются через masking младших битов или пропуск поля.

## Общие проверки каждой реализованной строки

1. Положительный production fixture и совпадающие generic C++ canonical values, sender ID, source
   field order/full identities, symbol units/cipher, flags, family и поддерживаемое время. Raw/stream
   и новые family paths совпадают для общей physical grammar; business conversion или collector
   transformation не скрывают расхождения.
2. Отрицательная production mutation на значимых prefix/body/field/frame boundaries, включая каждую
   точку усечения конечного fixture. Раздельно фиксировать primitive examined position и production
   message-relative position. Проверять C++ typed cause/stage/offset и completed-prefix behavior;
   production error и отказ выбранной unsupported policy — разные наблюдения.
3. Конечные budgets проверяются в zero и L-1/L/L+1 для input, counts, construction, output, units и
   owned storage по применимости. Admission предшествует count-sized reserve, пропорциональному
   copy/allocation, построению lookup/action/default и publication. Overflow проверяется вычитанием.
   Отвергнутая работа не admitted; admitted work не возвращается после skip, replacement или error.
4. Сравнивать defaults, repeated assignments, unknown fields/records и следующее sentinel field/record.
   Malformed skipped values отвергаются без угадывания следующей границы. Сохранённые outputs
   переживают input/source/schema/reader и последующие успешные generations.
5. Фиксировать final source/artifact hashes, фактические mode/profile/toolchain и точные commands.
   После source fix повторять затронутые проверки на final source. Прежние default profiles,
   admission tokens и serialized output остаются byte/behavior compatible.

## QDCXX-041: недостающие scalar identities

Целевые строки T00/T01/T03/T04/T24/T104/T152 и неизменные прежние scalar rows.
Ожидаемые тесты: `tests/conformance/field_type_value_tests.cpp` и
`tests/conformance/field_type_tape_tests.cpp`; расширить production field-types oracle.

- BYTE/SHORT/INT: signed extrema, -1/0, big-endian exact widths, все incomplete widths и следующее
  sentinel field. BYTE/SHORT factories не создают default instance; нужен production requested-wire
  descriptor path с подходящим consumer. Diagnostic-only field factory не становится наблюдением FileWriter.
- DECIMAL сохраняет signed32 raw bits, включая special encodings; LONG/TIME_MILLIS — signed64.
  Compact-overlong/high32 inputs различают physical read и int32 storage projection. WIDE_DECIMAL
  сохраняет all64 bits; существующие Quote/extended fixtures не меняются.
- VOID не потребляет value bytes. Только explicit wire VOID может давать raw zero; physical VOID tail
  long storage не является отдельным logical value. Отдельно проверять descriptor writer elision и
  reader alignment. До реализации закрыть точное правило logical schema.
- Exact-ID negatives: reserved168, identities с общими serialization bits, неизвестные high bits,
  отрицательные и >255 descriptor IDs. Различать profile refusal и descriptor grammar failure.
- Прежние profiles принимают ровно прежние IDs. Добавляется только новый явный field profile;
  scalar progress не означает готовности расширенного каталога19типов.

## QDCXX-042: byte/string/opaque-object payloads

Цели T09/T137/T233/T249 и неизменный T10. Ожидаемые тесты:
`tests/conformance/object_field_value_tests.cpp`, `tests/conformance/field_type_tape_tests.cpp`.

- Проверять nullable owned bytes и точные text tags: raw zero, различие null/empty/content, отсутствие
  payload у scalar, equality/defaults/replacement/invalid manually constructed values. Прежние
  aggregates и profiles сохраняют смысл.
- Проверить четыре grammar: byte-array compact-long length; marshalled compact-int projected length;
  UTFString byte length; character-array unit count. Для249 нужны wide/overlong prefixes с low32 count,
  отличным от полного compact long. Сравнить consumed bytes и sentinel alignment. Одинаковый byte-vector
  result не разрешает объединить правила length validation.
- Покрыть null, empty, NUL, FF bytes, большие bounded contents; malformed negative/oversized lengths,
  truncated prefix/body и allocation failure. Для137 — BMP/supplementary/CESU/unpaired units,
  accepted overlong forms и malformed continuations через production consumers.
- Наблюдать Marshalled bytes без getObject. Неизвестные class/tag contents остаются opaque;
  malformed outer length/framing дают ошибку. Java class loading/deserialization и semantic execution
  PlainObjField не становятся обязательными возможностями.
- До пропорционального allocation/copy проверять per-value/cumulative examined bytes, admitted payload
  work и retained bytes завершённых/текущего/временных значений. Skips, overwritten values, unknown
  records, failures и replacement не возвращают работу. Независимо проверить allocation, exception
  translation и owned-result lifetime во всех принятых data paths.

## QDCXX-043: indexed layout и immutable generations

Цели S1–S8. Ожидаемые тесты: `tests/conformance/indexed_schema_tests.cpp` и
`tests/conformance/tape_session_tests.cpp`; нужны production schema exporter/binding observations.

- Доказать Java slot indices, widths, aliases и hasTime по явной source schema. Проверить PAIR_INT32
  и SINGLE_INT64, ordinary/indexed records, reordered/missing fields и explicit VOID. Не выводить
  indexed layout из record names или wire-only descriptions. Нехарактеризованный layout остаётся
  pending/unsupported, а не автоматически принятым special case.
- Наблюдать equal/new/redefined sender IDs: same ID/equal descriptor, reorder/subset/new fields,
  record-name/type changes и несколько replacements в batch. Явно определить active table ordering
  и local-ID stability. Сохранять sender identity/full types; не применять Java conversion/fallback
  representation rewrite ради совпадения.
- При ошибке посреди parse/validation/admission/allocation прежние descriptions/schema/context и
  borrowed views сохраняются. При успехе все компоненты публикуются nonthrowing operations без user
  callbacks между публикациями. Старые owned records и RecordContext provenance валидны; borrowed
  views истекают только на документированных boundaries.
- Ограничить current active state, old+candidate peak storage и cumulative construction. Повторные
  replacement/rebuild достигают точных limits без unbounded generation history и возврата work.
  Sender storage sparse, включая большие valid sender IDs.
- Для независимых закрытых sources механизм reset — новый reader. Старые outputs переживают его;
  новый reader не использует старый sender description. Production reset характеризуется отдельно.
  При необходимости in-place reset проверить incomplete-frame/terminal-state и каждый компонент;
  reset не обходит source/sequence cumulative limits. Header/footer не сбрасывают состояние автоматически.
- Прежнее ordinary discovery сохраняет repeated-ID refusal. До закрытия обязателен независимый
  critical state/lifetime/resource review.

## QDCXX-044: options, flags и symbol state

Цели O1–O3, F0/F1/F2/F4/F8/F16/F64/FU, SYM и MRU. Ожидаемые тесты:
`tests/conformance/event_flags_tests.cpp`, `tests/conformance/symbol_tests.cpp`.

- Наблюдать absent/empty/hs options и unknown/duplicate tokens обоих commits. Отдельно фиксировать
  composer masks и parser behavior: Penta reader игнорирует option set; обычный writer фильтрует flags.
  Broad decoding сохраняет известные raw data flags без hs advertisement; прежние modes отвергают
  nonzero data flags. Неизвестные features остаются явными profile refusals.
- Проверить all64 subsets0x5f в каждой RAW/TICKER/STREAM/HISTORY family, включая parser mutations для
  отфильтрованных writer combinations. Flags остаются metadata без collector semantics.
  Отдельно проверить0x20/0x80/unknown high bits и различать writer masking, raw Java parser acceptance
  и выбранный C++ UNSUPPORTED.
- Проверить F8 initialMRU1, F9 new flags, reuse, explicit zero, duplicate prefixes, flagged FF repeat,
  first-symbol repeat, message/frame reset, empty messages и truncation внутри flags/symbol.
  Сохранять точные eventFlagsBytes rules и symbol ownership/work accounting.
- REMOVE_EVENT читает builtin event fields, затем compact-long record time; обычные fields отсутствуют.
  Проверить ordinary/indexed, zero-field и следующий sentinel record; точный removalTime переживает
  reader. Настроенные local words совпадают с Java setTime. Unsupported FIELD metadata до046
  отвергаются, не потребляются как ordinary fields.
- Проверить completed-prefix counters/limits во всех families и combinations, меняющих payload layout.
  Не добавлять duplicate eventFlags member или snapshot/history scheduler.

## QDCXX-045: dispatch и descriptor policy

Цели M0–MU, D1–D3, H1–H5, прежние TIME0/TIME1 и C0. Будущие строки остаются отдельными.
Ожидаемые тесты: `tests/conformance/tape_message_family_tests.cpp`,
`tests/conformance/tape_reader_tests.cpp`.

- Production RAW/TICKER/STREAM/HISTORY tapes совпадают по generic source family, record/value sequence,
  flags и поддерживаемому времени. Использовать RawDataConsumer observation, не считать fallback
  fan-out Java трижды. Смешанные families и controls сохраняют wire event order.
- До реализации точной bounded policy проверить обычные DXP3 headers, endpoint/no-endpoint tails,
  unknown properties/message descriptors, deltas/repeated headers, bounds и malformed/truncated
  extensions. Сохранить acceptance/refusal и output прежних modes.
- Headerless self-described binary требует реального FileWriter positive с указанной writer setting
  подавления header/footer. Data IDs по-прежнему требуют descriptions. Проверить no-footer clean EOF
  и truncated last frame. До такого case headerless остаётся pending.
- Явно отвергать schemeKnown/readAs/resync/ignore/continue/cycle, CSV/BLOB, PART reassembly,
  subscription и RMI payload operations. Проверить каждый finite known message ID/group и примеры
  unknown IDs. Valid excluded Java behavior не называется malformed. Отказ должен предшествовать
  пропорциональной unsupported-feature work и публикации вводящих в заблуждение records.
- Finish успешен только при actual explicit EOF, полных frames и проверках выбранного mode. Valid
  prefix, NEED_INPUT, resource stop, непрочитанные bytes и late failure не дают full completion.
  Сохранять source/frame/payload coordinates и первую terminal cause. Source/callback/transport
  framework этим пакетом не требуется.

## Будущие gates и итоговые проверки

FIELD046, LONG/TEXT047, GZIP/ZIP048, split sources049 и operational streaming050 остаются pending.
До048 выбрать optional pinned dependency/license/platform/OFF acquisition policy, members/entries,
CRC/trailer checks и source/inflated/output budgets; OFF не скачивает dependency. Этот контракт её
не добавляет. Будущим sinks нужен review callback lifetime, stop/exception checkpoints, source/
sidecar/container close и layered completion. Нереализованные ячейки остаются pending.

Для изменённого C++/schema/protocol behavior выполнить focused doctest/CTest и production-oracle
comparisons, затем обязательную итоговую матрицу Windows MSVC Release и Linux GCC Debug ASan/UBSan
full CTest. Проверить применимые production-only optional profiles ON/OFF без unwanted dependency
acquisition. Windows-only qualification producer не отменяет Linux client checks. После source changes
запускать pinned C++/Java formatters и affected oracle Verify; фиксировать final source/provenance hashes.

Для документов/матрицы проверить EN/RU semantic parity, reciprocal/relative links, whitespace всех
changed/new files, row uniqueness/count, точные full-ID/message/flag dimensions, primary-source
references обоих commits и существование actual-evidence paths. Обязательный independent final-source
review охватывает ownership, generation state, partial failure, EOF и resource bounds. Самостоятельная повторная проверка не заменяет независимую. Публиковать только фактические checks и оставшиеся limits.
