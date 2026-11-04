# Fixtures сжатия IPF и JDK8 oracle

[English](README.md). Английская версия нормативная.

CompressionOracle.java — внутренний ограниченный fixture/oracle helper. Он вызывает фактические
production StreamCompression и InstrumentProfileReader закреплённой QD из upstream.lock на
существующем JDK8, без пересборки/получения QD, зависимостей и SDK. Зафиксирован Zulu
OpenJDK1.8.0_422. provenance.json содержит source/class identities и SHA256 fixtures. Наблюдение
cached production classes не объявляется новой воспроизводимой сборкой этих классов.

Генератор создаёт синтетический UTF-8 IPF, использует JDK GZIPOutputStream/raw Deflater, формирует
малые ZIP local-entry/ZIP64/descriptor records и обычный multi-entry ZipOutputStream archive.
Для обычного архива задана фиксированная дата1980; Java запускается с user.timezone=UTC.
Malformed cases намеренно повреждают/усекают headers, payload, sizes и CRC.
Реальных credentials, пользовательских данных и публичной сети нет.

Старые outcomes.tsv/outcomes-v2.tsv/outcomes-v3.tsv сохранены. outcomes-v5.tsv фиксирует40 cases
актуального helper. У исходного ordinary ZIP была дата наблюдения; текущий fixture использует
фиксированные metadata с тем же Java decode/IPF outcome. Captures не переписываются под C++.

[ADR-0007](../../../../docs/decisions/ADR-0007-ipf-container-interoperability.ru.md) описывает намеренные
отличия для корректных unsigned descriptors и малых ZIP64 с64-битными descriptor fields.
zip64-descriptor-small сочетает ZIP64 extra и32-битный descriptor и некорректен по PKWARE4.3.9.2:
C++ отклоняет его несмотря на принятие JDK8. Дефекты JDK8 не копируются. gzip-missing-marker
фиксирует Java default warning/allow; C++ явно выбирает ALLOW или REQUIRE.

## Запуск и справка

Команды приведены в английской версии. Нужен существующий classpath production QD:
module target/classes и runtime dependencies jsr305/slf4j/log4j. C++ tests читают committed
fixtures и не требуют Java. Formatter JDK21 отделён от oracle JDK8.

Без аргументов, --help и --help-ru выводят справку без file/network/native SDK работы.
Один новый output directory получает40 конечных .bin файлов и outcomes.tsv. Существующие файлы
отклоняются; decoded observation ограничен1MiB на case. Код0 означает успех; иной — ошибку
аргументов/output/classpath/tool/I/O либо превышение probe bound. Нельзя генерировать поверх
checked captures: используйте новый каталог и сначала сравните identities.
