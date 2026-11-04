# Наблюдения атрибутов Candle

[English version](README.md). Английская версия нормативная.

`Invoke-Oracle.ps1` без аргументов, `-Help`, `-HelpRu` и `Get-Help` показывают справку
без рабочих путей, файлов и Java. PowerShell7 использует готовые production-классы
CandlePrice/Session/Alignment/PriceLevel/DataType/Exchange закреплённого QD3.355 и JDK8.
SDK не собирается и не загружается; форматтеру нужен отдельный JDK21+.

```powershell
pwsh -File tools/upstream/candle-attributes-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-attributes-check
```

Verify используется по умолчанию. Для работы обязательны четыре пути; RunDirectory должен
быть внутри .work этого checkout, собственные логи могут заменяться. Одновременно работает
один процесс, heap128MiB, timeout60 секунд, включая version/javac. Останавливается только
собственное дерево при timeout. UTC/Pacific-Honolulu должны совпадать побайтно, production-классы
не должны изменяться. Каталог хранит полные логи, raw streams, class inventories и receipt.
Exit0 — успех/справка; exit1 — ошибка входа, provenance/наблюдений, timeout или процесса.

Capture заменяет только [attributes-oracle.tsv](../../../tests/fixtures/events/candle/attributes-oracle.tsv)
и [attributes-provenance.json](../../../tests/fixtures/events/candle/attributes-provenance.json)
кандидатами для ревью. Verify сравнивает повторные наблюдения и точную provenance.

Пять столбцов TSV: тип, операция, UTF-16 hex-вход, UTF-16 hex-операнд, результат.
NULL означает null; каждая кодовая единица записана четырьмя hex-цифрами нижнего регистра.
Результат — UTF-16 hex, NULL, ERROR для ошибок argument/range reference или hex-биты числа/код,
двоеточие и UTF-16 hex-текст для level/exchange. Числовой raw-вход — hex-кодированный ASCII.
В fixture 265722 строки: все одиночные UTF-16 units трёх парсеров/exchange, null/malformed-символы,
defaults, псевдонимы, числовой синтаксис, отвергаемые -0/бесконечности, граничные биты и 2048
детерминированных случайных binary64. Приём Candle, расписания и агрегация не проверяются.
См. [контракт значений](../../../spec/events/CANDLE-ATTRIBUTES.ru.md).
