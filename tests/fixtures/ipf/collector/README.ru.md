# Java-наблюдения коллектора

[English](README.md).

`CollectorOracle.java` снимает конечные фактические наблюдения production classes
закреплённого `InstrumentProfileCollector`. `provenance.json` фиксирует source/class/helper
bytes и JDK8; `outcomes-v2.tsv` содержит 16 строк; прежний v1 capture из 14 строк сохранён без изменений. Существующие production classes не
пересобираются; совпадение байтов при компиляции этих source не заявлено.

Компилируйте helper JDK8 с явным classpath закреплённых QD modules и logging dependencies:

```powershell
New-Item -ItemType Directory -Path $outputClasses | Out-Null
javac -encoding UTF-8 -cp $qdClasspath -d $outputClasses tests/fixtures/ipf/collector/CollectorOracle.java
java "-Dfile.encoding=UTF-8" -cp "$outputClasses;$qdClasspath" CollectorOracle --run
```

`$qdClasspath` задаёт вызывающий, `$outputClasses` — его новая папка вывода classes.
Без аргументов, `--help` и `--help-ru` выводят безопасную справку без входов, создания
collector и запуска oracle. Неверные аргументы завершаются ошибкой. `--run` печатает
TSV в stdout, не читает файлы/сеть и не создаёт executor threads: выбирает direct или
конечный queued executor. Циклы ограничены 32 элементами/turns. Код 0 — успех,
исключение/неверные аргументы — ненулевой код.

Две новые строки различают публикацию неизменённого same-instance в reference-retaining base и подавление равного повтора actual defensive-copy hook. C++ следует immutable value equality независимо от адреса. Проверяются пустое/непустое начальное уведомление, standard/custom values, generation/time
для равных значений, отсутствующее удаление, минимальный tombstone, полное переданное
удаление и пропуск вытесненных queued revisions. C++ conformance tests дополнительно
проверяют владение, admission, allocation failure, reentry и concurrency.
[Контракт](../../../../spec/ipf/COLLECTOR.ru.md) определяет owning snapshot и границу
планирования. Connection и completion wait этим capture не квалифицируются.
