# Генератор фикстур QD NuamTimeAndSale

[English version](README.md).

`NuamTimeAndSaleOracle.java` использует рабочие делегаты NuamTimeAndSale из
[закреплённой ревизии QD](../../../upstream.lock). Он записывает QTP для STREAM
и HISTORY и наблюдаемые значения для составных и региональных записей.
[Фикстуры](../../../tests/fixtures/nuam-time-and-sale/provenance.json) содержат
исходные и интерпретированные флаги сделки, флаги индексированной доставки,
время, индекс, десятичные и необязательные текстовые поля.

Для сборки нужен JDK 8. В примере PowerShell переменная `$qdClassPath` содержит
пути к собранным классам закреплённой версии QD. Запускайте команды из каталога
генератора.

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes NuamTimeAndSaleOracle.java
$javaArgs = @('-Duser.timezone=UTC', '-Duser.language=en', '-Duser.country=US',
    '-Ddxscheme.exchanges.NuamTimeAndSale=Q', '-cp', "classes;$qdClassPath",
    'qdcxx.oracle.NuamTimeAndSaleOracle', 'capture-wide')
java @javaArgs
```

Для каждого запуска указывайте новый выходной каталог. По умолчанию
используется wide decimal. Для tiny decimal перед `-cp` укажите
`-Ddxscheme.wide=false`. Чтобы записать необязательные поля, добавьте перед
`-cp` следующие параметры JVM.

- `-Ddxscheme.enabled.TimeNanoPart=NuamTimeAndSale`
- `-Ddxscheme.enabled.Buyer=NuamTimeAndSale`
- `-Ddxscheme.enabled.Seller=NuamTimeAndSale`

Для сохранённых профилей файлы совпадают побайтно между
`UTC` и `Pacific/Honolulu`. Успешный запуск создаёт четыре пары QTP/TSV.
Справка выводится без аргументов или по `--help` на английском и по
`--help=ru` на русском. Справка не запускает захват. Код 0 означает успех.
При неверных аргументах, существующем выходном каталоге или ошибке захвата
код завершения ненулевой. Ошибка захвата может оставить неполный каталог.

Сохранённые составные файлы получены с обычным wide-профилем, региональные — с тремя дополнительными полями. Оба профиля совпадают побайтно в UTC и Pacific/Honolulu.
