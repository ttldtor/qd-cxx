# Оракул Series

[English version](README.md). При расхождении применяется английский текст.

`SeriesOracle` фиксирует запись `Series` закреплённой версии Java QD и публичные
значения `SeriesDelegate` после записи и чтения штатными двоичными composer/parser.
[Происхождение](../../../tests/fixtures/series/provenance.json) содержит ревизию и
контрольные суммы исходников, программы и фикстур.

Используйте штатные классы ревизии из [upstream.lock](../../../upstream.lock) и JDK 8.
В PowerShell из корня проекта задайте в `$upstream` корень сборки закреплённой
версии, в `$jdk` корень JDK, в `$run` новый каталог внутри `.work` проекта.

```powershell
$modules = 'dxlib','mars','dxfeed-promise','dxfeed-api','dxlib-qd-sandbox','auth','proto',
           'qd-core','proto-ssl','qd-rmi','dxfeed-impl','dxfeed-scheme','dxfeed-ipf-filter','qds-file'
$cp = ($modules | ForEach-Object { Join-Path $upstream "$_/target/classes" }) -join ';'
New-Item -ItemType Directory -Path "$run/classes" -Force
& "$jdk/bin/javac.exe" -encoding UTF-8 -cp $cp -d "$run/classes" tools/upstream/series-oracle/SeriesOracle.java
if ($LASTEXITCODE) { throw 'javac' }

foreach ($profile in 'standard','tiny','optional','field') {
    foreach ($zone in 'UTC','Pacific/Honolulu') {
        $options = if ($profile -eq 'tiny') { @('-Ddxscheme.wide=false') } else { @() }
        $name = $zone.Replace('/','-')
        $settings = @("-Duser.timezone=$zone", '-Duser.language=en', '-Duser.country=US') + $options
        & "$jdk/bin/java.exe" @settings -cp "$run/classes;$cp" qdcxx.oracle.SeriesOracle "$run/$profile-$name" $profile
        if ($LASTEXITCODE) { throw "$profile / $zone" }
    }
}
```

В Linux используйте исполняемые файлы Java без `.exe` и `:` как разделитель
classpath. Свойство `-Ddxscheme.wide=false` нужно только профилю `tiny`.

Справка выводится при вызове без аргументов, с `--help` или с `--help=ru`.
Программа создаёт один новый каталог и пишет `series.qtp`, `oracle.tsv` и `text.tsv`.
Профиль `optional` также пишет `optional.tsv`. Успех даёт код 0. Ошибочные
аргументы, существующий каталог и ошибки захвата дают ненулевой код. Сеть
не используется. При ошибке каталог может содержать неполные результаты.

- `standard` использует штатную схему wide decimal.
- `tiny` использует штатную схему decimal.
- `optional` проверяет пользовательскую схему и результат для отсутствующих полей.
- `field` использует штатную схему с явным временем события FIELD.

Результаты в обеих зонах совпадают побайтово, кроме файлов представления событий.
Сверьте каждый текстовый файл `field` с фикстурой для соответствующей зоны.
