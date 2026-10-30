# Production fixtures контейнеров

[English version](README.md). Английская версия является нормативной.

`baseline/` соответствует Java QD 3.355 `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`;
`current/` — отдельному pin 3.356 `f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`.
В каждом каталоге — фактический закрытый FileWriter BINARY/STREAM_DATA/time=NONE output:
NONE (`none.qtp`), GZIP (`tape.qtp.gz`) и ZIP (`tape.qtp.zip`). Обычная синтетическая схема содержит
record Container с compact integer полем Container.F0=913 и symbol FLAGS. Частных рыночных данных
и timestamp sidecar bytes нет. Raw bytes помечены `-text`.

Ожидаемые inflated bytes в точности равны `none.qtp`. Production FileReader выдаёт одну STREAM
record и один descriptor для каждого valid file. `compressed_input_tests` проверяет побайтовое
равенство при одно-байтовом source и destination chunk. Metadata production ZIP, включая entry
timestamps, сохраняются; regeneration не обязана воспроизводить эти physical bytes в точности.
Критерии совместимости — payload/integrity outcomes; SHA-256 сохраняет identity каждого artifact.

[Спецификация контейнеров](../../../../spec/protocol/TAPE-CONTAINERS.ru.md) определяет более строгий
профиль закрытого контейнера. Принятие Java только первого ZIP entry, отсутствующего central
directory или trailing bytes не расширяет профиль. Native tests создают в памяти malformed
headers, CRC/ISIZE, truncated descriptor/directory/EOCD, дополнительные entries и граничные бюджеты.
Production observations и generated native negatives остаются разными видами доказательств.

Native regression также оборачивает существующие production standard-Quote bytes baseline/current
`real-tape/producer/*/expected/quote-none.qtp` независимым test compressor, затем проверяет точное
сохранение байтов обоими envelopes. Это standard-schema byte-preservation checks; они отделены
от новых production FileWriter compressed Container fixtures и итоговой CLI/parser/companion
integration acceptance.
