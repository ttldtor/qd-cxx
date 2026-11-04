# Production glossary oracle

[English](README.md). Английский текст является нормативным.

`Invoke-Oracle.ps1` вызывает закреплённые production Java `CFI` и `AdditionalUnderlyings`
из `dxfeed-api`, используя готовые `dxlib`/`qd-core` classes. QD не пересобирается, сеть
не запускается, JDK/global settings не меняются. Checkout должен совпадать с
[upstream.lock](../../../upstream.lock), относящиеся к задаче исходники — с HEAD.

```powershell
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/glossary-oracle/Invoke-Oracle.ps1 `
  -Mode Verify -Upstream C:/qd -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/glossary-check
```

Без аргументов, `-Help`, `-HelpRu`, PowerShell `Get-Help` не нужны рабочие входы,
Java и обработка источников не запускаются. По умолчанию Verify. Capture заменяет только
`tests/fixtures/glossary/oracle.tsv` и `provenance.json` этого oracle; остальные записи —
свои логи, скомпилированный harness и receipts в указанном checkout-local `.work`.
Exit 0 — успех/справка; exit 1 — ошибка входов/reference/process/timeout/сравнения.
Неизвестные параметры отклоняются.

Фактический `java -version` должен идентифицировать JDK8. Java heap — 128 MiB, timeout —
60 секунд, read-only Git timeout — 30 секунд. Останавливаются только свои просроченные
деревья процессов. Formatter JDK21 используется отдельно. Инвентари production classes
до/после должны совпадать. Provenance содержит hashes исходников, Java/javac, harness,
launcher, полного class inventory, fixture и число строк. Полные команды, потоки и exits
остаются в локальных logs/receipts.

Детерминированный harness проверяет все 676 префиксов CFI, каждый достижимый атрибут и
полный порядок значений/описаний, короткие/неизвестные/некорректные коды и packed integers;
AU separators, null, дубликаты, signed zero, legacy exponent overflow, UTF-16 границы
и статический поиск. Рендеринг карты включает 8 192 seeded random binary64, соседей всех
десятичных степеней, первые 128 subnormal обоих знаков и normal/subnormal transitions.
UTC/en-US и Pacific/Honolulu/fr-FR сравниваются побайтово. Это production observations,
не заменяющий парсер и не доказательство полного перебора binary64. Дополнительные наблюдения
карты покрывают ошибки округления precision и соседние binary64, а также окрестности десятичных
середин обоих знаков от 1e15 до 1e18.

Native CTest `glossary` читает строки через публичные API, проверяет владение, constant
initialization, конечные пределы и allocation failures. `glossary_oracle_help` при наличии
PowerShell проверяет noargs/EN/RU help и некорректные вызовы. Отдельный запуск:

```powershell
cmake -DORG_TTLDTOR_QDCXX_PWSH=pwsh -P tests/conformance/glossary_oracle_help.cmake
```

См. [fixtures](../../../tests/fixtures/glossary/README.ru.md) и
[контракт](../../../spec/glossary/VALUES.ru.md). Live service/SDK здесь не проверяются.
