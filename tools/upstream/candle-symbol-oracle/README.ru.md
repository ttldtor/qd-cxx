# Наблюдения символов Candle

[English version](README.md). Английская версия нормативная.

Launcher Windows PowerShell7 использует готовые production-классы закреплённого QD3.355 и JDK8.
Без аргументов, -Help/-HelpRu/Get-Help показывают справку без путей, файлов, Java и SDK.
Форматтеру Java нужен отдельный JDK21+.

```powershell
pwsh -File tools/upstream/candle-symbol-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/reference/QD -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/candle-symbol-check
```

Verify — режим по умолчанию; для работы обязательны четыре пути. RunDirectory должен быть внутри
.work этого checkout; собственные observations/logs могут заменяться. Каждый Java subprocess работает
последовательно, heap128MiB, timeout60s, включая version/javac; останавливается только собственное
дерево при timeout. UTC/Pacific-Honolulu должны совпадать побайтно, production class inventories
не должны меняться. Exit0 — успех/справка, exit1 — ошибка входа, provenance/наблюдений, timeout
или процесса. Capture заменяет только
[symbol-oracle.tsv](../../../tests/fixtures/events/candle/symbol-oracle.tsv) и
[symbol-provenance.json](../../../tests/fixtures/events/candle/symbol-provenance.json) кандидатами
для ревью; Verify сравнивает без замены public fixtures. Полные логи, raw streams, инвентаризации
исходников/классов и receipt сохраняются в RunDirectory.

Шестнадцать столбцов TSV: операция, input UTF-16 hex, descriptors либо right-symbol hex, status,
canonical UTF-16 hex либо equals result, base hex, exchange hex, price ordinal, session ordinal,
raw period bits, period-type ordinal, alignment ordinal, raw price-level bits, data-type hex,
signed32 hash либо ERROR, exception class. Неиспользуемые столбцы — dash. NULL означает null;
одна кодовая единица — четыре lowercase hex-цифры. Equals ERROR означает null-domain failure.

Descriptors разделены semicolon: e + exchange hex; p/s/a + enum ordinal; l + raw double hex;
t + raw double hex / type ordinal; d + UTF-16 hex (пусто означает undefined).
Они создают настоящие production values перед вызовом настоящей CandleSymbol factory;
подмены parser/normalizer нет. Null-ссылки атрибутов не представимы закрытым C++ variant.

9822 строки включают combined attributes, различающиеся explicit getters, duplicate/unknown/invalid
keys, null/empty/malformed text, NaN payload/nonfinite/signed-zero periods, equality pairs,
4096 детерминированных key/value combinations и каждую семнадцатую UTF-16 unit внутри base symbols.
Native-тест читает все строки и отдельно проверяет явную total C++ operator== null adaptation.
См. [полный контракт](../../../spec/events/CANDLE-SYMBOL.ru.md).
