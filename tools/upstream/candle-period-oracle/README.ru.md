# Наблюдения периодов Candle

[English version](README.md). Английская версия нормативная.

`Invoke-Oracle.ps1` без аргументов, `-Help`, `-HelpRu` и `Get-Help` показывают справку
без рабочих путей, создания файлов и запуска Java. Launcher PowerShell 7 использует
готовые production-классы CandleType/CandlePeriod закреплённого QD 3.355 и JDK8;
он не собирает и не загружает SDK. Форматтеру Java нужен отдельный JDK21+.

```powershell
pwsh -File tools/upstream/candle-period-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-period-check
```

`Mode` по умолчанию равен Verify. Для работы обязательны все четыре пути.
`RunDirectory` должен находиться внутри `.work` этого checkout; собственные логи
в нём могут заменяться. Одновременно запускается один процесс Java, heap128MiB,
тайм-аут60 секунд, включая проверку версии и javac. Останавливается только собственное
дерево процесса при превышении срока. Вывод UTC и Pacific/Honolulu должен совпадать,
production-классы не должны изменяться. Каталог сохраняет команды, коды завершения,
raw-потоки, инвентаризации классов и receipt.

Capture заменяет только [period-oracle.tsv](../../../tests/fixtures/events/candle/period-oracle.tsv)
и [period-provenance.json](../../../tests/fixtures/events/candle/period-provenance.json)
кандидатами для ревью. Verify сравнивает новые наблюдения и точное происхождение
исходников/harness/launcher без замены fixtures. Exit0 — успех/справка; exit1 —
ошибка входа, происхождения, наблюдений, тайм-аут или ошибка процесса.

Восемь столбцов TSV: операция, hex-вход UTF-16, ordinal типа для конструктора,
raw-биты binary64 для конструктора, канонический hex-выход UTF-16 либо ERROR,
ordinal типа результата, raw-биты binary64 результата, оценка интервала.
Неиспользуемые поля содержат пустой вход, -1, ноль или дефис в зависимости от операции.
Каждый точный кодовый элемент UTF-16 записан четырьмя строчными hex-цифрами;
файл сохраняет исходные байты UTF-8 TSV production Java.

Охвачены все типы, префиксы enum и регистр множественного числа, все одиночные
кодовые элементы UTF-16, различие минуты/месяца и неоднозначность price-имён,
ошибки числового префикса, субнормальные значения, нули с разными знаками, NaN
и бесконечности, насыщение при сужении, представление 2^63, границы записи и
512 детерминированных raw double на тип. Harness также напрямую проверяет
равенство NaN/нулей. Это наблюдения значений; CandleSymbol, агрегация и приём
событий Candle не проверяются. См. [контракт](../../../spec/events/CANDLE-PERIOD.ru.md).
