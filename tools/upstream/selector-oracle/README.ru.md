# Production oracle selectors

[English version](README.md). Английский текст является основным.

`Invoke-Oracle.ps1` захватывает или проверяет реальные закреплённые Java8 selector equality/hash/construction, regional/source/wildcard selection production delegates и raw time-bound conversion. Используются готовые production classes; нет копии equality, диагностического mapping, сети, endpoint или SDK rebuild. Асимметричные Java результаты сохраняются без исправления. [ADR-0012](../../../docs/decisions/ADR-0012-decorated-subscription-identity.ru.md) классифицирует конкретное native исправление; сам capture не заявляет ошибку реального Feed-container.

No args, `-Help`, `-HelpRu` и `Get-Help ./Invoke-Oracle.ps1 -Full` не требуют путей и не запускают Java/создание рабочих результатов. Для работы обязательны четыре явных пути:

```powershell
pwsh -NoProfile -File tools/upstream/selector-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/selector-check-01
```

По умолчанию `Verify`. `Capture` заменяет только `tests/fixtures/selectors/oracle.tsv` и `provenance.json`, подготавливая кандидатов для ревью. RunDirectory должен быть новым каталогом внутри `.work` этого checkout без redirecting ancestors. Старые доказательства не перезаписываются. Результат привязан к source hashes, точной revision, полным class inventories, настоящей версии JDK и hashes executable/harness/launcher. Публичный provenance не содержит operational private paths.

Каждый subprocess журналируется с UTC command/exit и ограничен60s; Java/javac heap128MiB. Процессы последовательны; останавливается только собственное дерево timed-out процесса. Наблюдения UTC и Pacific/Honolulu должны совпадать побайтно. Exit0 — успех/справка; exit1 — неверные входы, отсутствующий/несовпавший pinned reference, process failure, timeout или observation/provenance mismatch. Ошибки указывают на справку; private raw stdout/stderr/command logs и receipts сохраняются в RunDirectory.

Корпус90 строк содержит20 pair comparisons, constructor/value/collection наблюдения, current Quote regional/source/time-series/wildcard delegate selections,13 signed time bounds и7 source dispatch cases. Покрыты source -1/-2/MIN/default/known/absent, MIN/MAX milliseconds, fromTime-excluded identity и explicit wildcard versus literal-star. Пример HashSet insertion отделён от непроверенного реального DXFeed subscription-container. Typed source/time event receiving и произвольный provider compatibility не входят в oracle. [Native контракт](../../../spec/subscriptions/DECORATED-SELECTORS.ru.md) и отдельный deterministic installed Feed consumer покрывают заявленную native границу.
