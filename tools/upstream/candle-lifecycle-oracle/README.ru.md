# Эталон Candle HISTORY

[English version](README.md). При расхождениях действует английская версия.

`CandleLifecycleOracle` фиксирует работу Java-делегата Candle и коллектора HISTORY в
стандартной схеме QD с wide decimal. Сеть не нужна. Сценарий подписывается на
`AAPL{=m}` от 1000 мс, завершает первоначальный снимок и отправляет коррекцию с тем же
индексом. Второй локальный подписчик получает первоначальный снимок из кеша, третий
получает уже исправленный. Провайдер добавленных записей выдаёт один физический ADD
на все три подписки.

Для захвата передайте один путь к новому каталогу. Программа запишет `oracle.tsv` и девять
бинарных фрагментов QTP. Без аргументов и с `--help` выводится английская справка,
с `--help=ru` выводится русская. Для справки не нужен путь, файлы при этом не создаются.
Код 0 означает успех. При ошибке программа возвращает ненулевой код и может оставить
часть выходных файлов.

## Воспроизведение

Понадобятся JDK 8 и собранные классы Java QD из ревизии, указанной в
[upstream.lock](../../../upstream.lock). В PowerShell перейдите в каталог с
`CandleLifecycleOracle.java`. Задайте в `$upstream` путь к собранному QD, в `$jdk`
путь к JDK, а в `$run` путь к новому каталогу для классов и двух захватов.

```powershell
$modules = 'dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto',
           'qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file'
$cp = ($modules | ForEach-Object { Join-Path $upstream "$_/target/classes" }) -join [IO.Path]::PathSeparator
$suffix = if ($IsWindows) { '.exe' } else { '' }
$javac = Join-Path $jdk "bin/javac$suffix"
$java = Join-Path $jdk "bin/java$suffix"
if (Test-Path -LiteralPath $run) { throw 'Choose a new run directory' }
New-Item -ItemType Directory -Path "$run/classes" -Force | Out-Null
& $javac '-J-Xmx128m' -encoding UTF-8 -cp $cp -d "$run/classes" ./CandleLifecycleOracle.java
if ($LASTEXITCODE) { throw 'javac failed' }
$class = 'qdcxx.oracle.CandleLifecycleOracle'
$classpath = "$run/classes$([IO.Path]::PathSeparator)$cp"
& $java '-Dfile.encoding=UTF-8' -cp $classpath $class --help
if ($LASTEXITCODE) { throw 'English help failed' }
& $java '-Dfile.encoding=UTF-8' -cp $classpath $class --help=ru
if ($LASTEXITCODE) { throw 'Russian help failed' }
foreach ($zone in 'UTC','Pacific/Honolulu') {
    $label = $zone.Replace('/','-')
    & $java '-Xmx128m' "-Duser.timezone=$zone" '-Duser.language=en' '-Duser.country=US' -cp $classpath $class "$run/$label"
    if ($LASTEXITCODE) { throw "Capture failed for $zone" }
}
```

Сравните SHA-256 всех десяти файлов по именам в `UTC` и `Pacific-Honolulu`.
Содержимое совпадает побайтово. Для получения исходного потока объедините
`source-begin.qtp`, `source-end.qtp` и `source-correction.qtp` в этом порядке.
Первый фрагмент содержит бинарное описание записи. Фрагменты каждого подписчика образуют
отдельный поток.

В TSV записаны физическое имя записи, символ, сырое время и флаги, а также полученные
от делегата логический символ, индекс, миллисекунды Unix, номер последовательности, число
сделок и биты цены закрытия в binary64. Сценарий проверяет локальную доставку и replay
HISTORY внутри одного коллектора QD.
