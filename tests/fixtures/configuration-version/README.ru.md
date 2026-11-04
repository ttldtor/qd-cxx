# Наблюдения версий Configuration

[English version](README.md). Английская версия определяет контракт.

`capture-v1/oracle.tsv` содержит481 raw input/output/count наблюдение
[production Java helper](../../../tools/upstream/configuration-version-oracle/README.ru.md).
`provenance.json` закрепляет revision, Java executables, harness, launcher, выбранные upstream sources,
точный class inventory и байты fixture. Измеренный classpath:14 корней,1290 class-файлов и27 ресурсов;
до/после захвата он неизменен. Helper не пересобирает Java artifacts.

Стандартная и явные production absent/wide VersionedRecord-схемы проходят через настоящие collectors
TICKER/STREAM и ConfigurationDelegate. Первая отрицательная версия сравнивается с default-zero cache
и даёт version0/null attachment/eventTime0 с символом CONFIG. Равная версия допускает изменение данных;
меньшая не заменяет TICKER. Сохранены signed extrema, выбор staged-кандидатов batch и порядок STREAM.
Отсутствие Version отключает фильтр. Поздний replay cache при подписке отдельно покрывает native Feed
regression в `misc_feed_tests.cpp`. Версия события независима от кодеков вложений.
Неизменённые [attachment fixtures](../attachments/README.ru.md) проверяют serialization/delegate/wire pipeline.

Постоянные native regressions находятся в `ticker_collector_tests.cpp`, `misc_event_tests.cpp` и
`misc_feed_tests.cpp`: signed/default-границы, владеющие проекции и настоящий публичный Feed/replay.
[Контракт](../../../docs/MISC-EVENTS.ru.md) разделяет нормализованные поля проекции и сохранённый raw
provenance. Capture и Verify побайтно сравнивают UTC и Pacific/Honolulu.
