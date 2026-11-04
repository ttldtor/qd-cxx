# Production oracle Summary

[English version](README.md).

SummaryOracle.java вызывает закреплённые production QD3.355 SummaryMapping, SummaryDelegate и
Summary getters. Полные composite/regional records берутся из DXFeedScheme, с physical long slots
и VOID tails. Копия converter/delegate не используется. OpenInterest getAsInt наблюдается отдельно
от long/double/wide accessors.

Get-Help ./Invoke-Oracle.ps1 -Full, вызов без аргументов, -Help и -HelpRu не требуют рабочих путей,
не создают workload files и не запускают Java, Git, SDK или сеть.

```powershell
pwsh -File tools/upstream/summary-oracle/Invoke-Oracle.ps1 -Mode Verify `
  -Upstream C:/qd-reference -JavaHome C:/jdk8 `
  -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/summary-verify
```

Mode по умолчанию Verify: fixture/provenance сравниваются без замены. Capture заменяет только
tests/fixtures/summary/oracle.tsv и provenance.json. Обязательны четыре пути: закреплённый checkout
с готовыми production classes, JDK8, UTC logger и новый RunDirectory внутри .work этого checkout.
Redirecting ancestors и повторное использование каталога запрещены. Upstream/classes/SDK/global
configuration не меняются, public endpoint не открывается.

Subprocesses последовательны, timeout60секунд; Java heap128MiB. Останавливается только собственное
timed-out process tree. Git HEAD/diff, реальная JDK8 version, javac и каждый observation run имеют
полные UTC command/exit/stdout/stderr logs. Git discovery выбирает один application path. Version
извлекается из настоящей строки JDK8, не из logger COMMAND. Сохраняются class/source inventories.

Фиксированные входы включают Summary exchanges Q/z (fixture), default wide=true, wide=false tiny
fallback и явные price/volume int profiles. UTC/Pacific-Honolulu должны дать одинаковые байты.
Public provenance содержит source/harness/launcher/JDK hashes, actual Java version, class inventory
digest и raw fixture digest, без локальных путей.

Exit0 — успешная справка/Capture/Verify; exit1 — неверные пути/reference, ошибка/timeout процесса,
несовпадение observations или I/O. Capture пишет candidate fixture/provenance и новые run artefacts,
Verify — только новые run artefacts. Сохраняйте failed runs и используйте новый каталог для повтора.
Public feed и SDK build не запускаются.

Перед Capture примените закреплённый Java formatter, после final source freeze — Verify.
Raw TSV хранится без Git text conversion, provenance — UTF8безBOM/LF.
[Summary](../../../spec/events/SUMMARY.ru.md), [формат корпуса](../../../tests/fixtures/summary/CAPTURES.ru.md).

## Исправленный OpenInterest long reference

[ADR-0010](../../../docs/decisions/ADR-0010-summary-open-interest-width.ru.md) исправляет доказанную
потерю конечного представимого OI в pinneddelegate. Исходные helper, `oracle.tsv` и provenance
не меняются. Отдельная сопровождаемая команда:

```powershell
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -Help
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -HelpRu
pwsh -File tools/upstream/summary-oracle/Invoke-OpenInterest.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/oi-reference-check
```

Get-Help/noargs/EN/RU не требуют рабочих входов и не запускают Java. Действуют те же четыре пути,
Verify по умолчанию/Capture, heap128MiB, subprocess timeout60s, новый каталог внутри .work,
готовые offlineproductionclasses и правила exit0/1. Capture пишет только отдельные
`open-interest-long-v1.tsv`, `open-interest-boundary-v1.tsv` и provenance; исходный capture read-only.
Verify создаёт только новые run artifacts.

Harness восстанавливает полные физические поля через productionfactory, сверяет actualdelegateOI
с каждым исходным успешным sample и независимо вызывает actual mapping-long accessor. Reference
содержит оба значения, исправленное ожидание и явный класс IDENTICAL/CORRECTED_INT_ACCESSOR_LOSS.
Native reader требует ровно одну совпадающую строку для каждого sample; отсутствие, дубликат,
неверное raw значение или класс — ошибка. OI не игнорируется. Новые productionboundaries покрывают
все шесть numeric типов,2^31/2^32, signed64 границы, NaN/infinity и regionalabsence. UTC/Honolulu
должны совпасть; provenance связывает исходный immutablecapture и оба новых результата. Новый
harness форматируется отдельно прежним pinnedformatter до Capture; Verify выполняется на finalsource.
