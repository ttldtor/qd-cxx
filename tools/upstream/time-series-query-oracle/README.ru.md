# Генератор границ запроса временного ряда

[English version](README.md).

`TimeSeriesQueryOracle.java` использует `TimeAndSale`, `TimeSequenceUtil` и `AbstractIndexedList` из [закреплённой ревизии QD](../../../upstream.lock). В [фикстуре](../../../tests/fixtures/time-series-query/provenance.json) записаны raw-индексы, бизнес-время, последовательности, порядок Java и расхождение реализации с документированной включительной верхней границей.

Соберите JDK 8, указав готовые production-классы QD в `$qdClassPath`:

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes TimeSeriesQueryOracle.java
java -Duser.timezone=UTC -cp "classes;$qdClassPath" qdcxx.oracle.TimeSeriesQueryOracle capture.tsv
```

Укажите новый выходной файл. Утилита работает офлайн; код 0 означает успех, неверные аргументы, существующий файл или ошибка записи дают ненулевой код. При ошибке может остаться частичный файл. Без аргументов или с `--help` выводится английская справка без записи файлов, с `--help=ru` — русская. Наблюдения совпадают в UTC и Pacific/Honolulu.
