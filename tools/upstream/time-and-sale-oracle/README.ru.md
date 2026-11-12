# Генератор фикстур QD TimeAndSale

[English version](README.md).

`TimeAndSaleOracle.java` использует рабочие делегаты TimeAndSale из
[закреплённой ревизии QD](../../../upstream.lock). Он записывает QTP для STREAM
и HISTORY и наблюдаемые значения для составных и региональных записей.
[Фикстуры](../../../tests/fixtures/time-and-sale/provenance.json) содержат
исходные и интерпретированные флаги сделки, флаги индексированной доставки,
время, индекс, десятичные и необязательные текстовые поля.

Для сборки нужен JDK 8. В примере PowerShell переменная `$qdClassPath` содержит
пути к собранным классам закреплённой версии QD. Запускайте команды из каталога
генератора.

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes TimeAndSaleOracle.java
$javaArgs = @('-Duser.timezone=UTC', '-Duser.language=en', '-Duser.country=US',
    '-Ddxscheme.exchanges.TimeAndSale=Q', '-cp', "classes;$qdClassPath",
    'qdcxx.oracle.TimeAndSaleOracle', 'capture-wide')
java @javaArgs
```

Для каждого запуска указывайте новый выходной каталог. По умолчанию
используется wide decimal. Для tiny decimal перед `-cp` укажите
`-Ddxscheme.wide=false`. Чтобы записать необязательные поля, добавьте перед
`-cp` следующие параметры JVM.

- `-Ddxscheme.enabled.TimeNanoPart=TimeAndSale`
- `-Ddxscheme.enabled.Buyer=TimeAndSale`
- `-Ddxscheme.enabled.Seller=TimeAndSale`

Для всех четырёх комбинаций сохранённые файлы совпадают побайтно между
`UTC` и `Pacific/Honolulu`. Успешный запуск создаёт четыре пары QTP/TSV.
Справка выводится без аргументов или по `--help` на английском и по
`--help=ru` на русском. Справка не запускает захват. Код 0 означает успех.
При неверных аргументах, существующем выходном каталоге или ошибке захвата
код завершения ненулевой. Ошибка захвата может оставить неполный каталог.
