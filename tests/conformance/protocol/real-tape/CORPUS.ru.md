# Квалификация корпуса реальных tape

[English version](CORPUS.md). Английская версия нормативная.

[Переносимый manifest](../../../fixtures/protocol/real-tape/corpus.tsv) ссылается по SHA-256 на
существующие неизменённые сгенерированные bytes. Пути разрешаются от корня репозитория.
`mixed-raw` содержит поддерживаемые полные типы полей; `unsupported-identities` сохраняет
типы 24/137 как явные неподдерживаемые C++ случаи. Это доказательства полей/профиля, а не
записи реального рынка. Production provenance остаётся в [корпусе полей](../../../fixtures/encoding/field-types/CAPTURES.md)
и [корпусе writer](../../../fixtures/protocol/tape-write/CAPTURES.md).

Запустите [Check-Inventory.ps1](../../../../tools/upstream/raw-tape-oracle/Check-Inventory.ps1) с
`-UpstreamRoot $lockedUpstream -JavaHome $oracleJdk -WorkDir $privateWorkDir`.
Launcher проверяет каждый хеш manifest и выполняет структурную инвентаризацию каждого входа.
Также выполняются generated assertions для opaque unknown kind 117, неподдерживаемого полного
типа 255, поздних descriptions, каждого обрезанного префикса frame/description, неверных
заголовков и конечных лимитов. Unknown types остаются наблюдениями inventory; структурный
проход до конца не означает принятия payload.

Для образца владельца добавьте `-PrivateFile $privateTape -ReplayDeliveryLimit 10000000`
и при необходимости `-Inspector $tapeInspector`. Допустим только ignored output внутри `.work`
репозитория. Образец открывается только для чтения, SHA-256 проверяется до/после. Его bytes,
пути, symbols и подробные логи нельзя включать в manifest или публичные captures. Недоступный
образец — явный локальный пробел; переносимые проверки выполняются и без него.

Production replay использует неизменённые locked FileReader/DXFeedScheme, автоматические
format/time/compression, MAX_SPEED, одно literal filename, без cycle/resync, по умолчанию
128 MiB heap и 30 секунд на процесс. Лимит доставок учитывает fanout callbacks.
`replay-observed` считает попытки доставок, включая первую отклонённую; `accepted-deliveries`
её исключает. STREAM_DATA — проекция одной wire-записи для этого RAW-образца, а не число
уникальных бизнес-событий.

`returned-byte-match` требует нормального возврата, отсутствия corruption callbacks и
равенства обработанных bytes физическому размеру файла. Также проверяйте stderr: FileReader
может записать I/O error в лог и поглотить исключение. Этот предикат не квалифицирует сжатые,
парные и последовательные входы. C++ наблюдения используют явно заданные локальный профиль
`oracle-field-types-v1`, `default-quote-v1`, `default-profile-v1` и режим `raw-message-v1` с неизменёнными конечными defaults; отказ
определяет первый этап данного профиля, а не совместимость с любой схемой вызывающего кода.

См. [агрегированные результаты и ранжированные пробелы](../../../../docs/REAL-TAPE-GAPS.ru.md).
Равенство C++/Java values/order для всего файла остаётся отдельным milestone; счётчики inventory
и возврат Java его не доказывают.

Manifest также закрепляет unsupported-message (C++ exit 4, этап type) и truncated-heartbeat (exit 5, этап heartbeat). При заданном Inspector проверяются все объявленные C++ exits. Вход unsupported-identities возвращает 4 на этапе binding/TYPE даже с unmapped record; его full types не принимаются.

`returned-byte-match` — нейтральное наблюдение, а не классификация source/container или доказательство
полного семантического декодирования. Размер распакованных данных может совпасть с физическим:
generated checks читают production FileReader 4 096 нулевых marker bytes в обычном файле и gzip,
дополненном до 4 096 bytes. Оба возвращаются без corruption callbacks с 4 096 processed bytes.
Launcher требует одинаковый нейтральный token: совпадение размеров не доказывает отсутствие сжатия.

Поля manifest cpp_stage/cpp_reason закрепляют negative diagnostics (- означает не задано); cpp_emitted/cpp_processed закрепляют positive completion counts. Mixed positive требует 3 mapped и 3 processed records. Отказы должны содержать один подходящий diagnostic и не содержать completion.
