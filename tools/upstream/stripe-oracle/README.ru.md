# Production oracle полос символов

[English version](README.md). Английская версия нормативна.

`StripeOracle.java` наблюдает настоящие закреплённые Java `SymbolStriper`, фабрики
MONO/HASH/RANGE, листья полос и production-пересечения фильтров. Алгоритм выбора полос
не переписывается в эталоне. [Контракт полос символов](../../../spec/core/SYMBOL-STRIPES.ru.md)
определяет native строковый API. Наблюдения чистой классификации не доказывают доставку
striped-коллекторов. `StripedCollectorOracle.java` дополнительно наблюдает настоящие pinned qd-stripe children, agents и distributors. [Native MUX composition](../../../spec/mux/STRIPES.ru.md) сохраняет существующие family boundaries; fixtures не доказывают параллельное ускорение.

Запускайте из корня репозитория с PowerShell 7.4+, явно заданным существующим закреплённым
деревом QD с собранными production-классами, JDK8 и native UTC logger:

```powershell
pwsh -NoProfile -File tools/upstream/stripe-oracle/Invoke-Oracle.ps1 --help
pwsh -NoProfile -File tools/upstream/stripe-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/stripe-check-01
```

Вызовы без аргументов, отдельные `--help`/`-Help`, `--help-ru`/`-HelpRu` и `Get-Help -Full`
не требуют рабочих входов и не запускают нагрузку. `Verify` выбран по умолчанию и сравнивает
проверенный fixture и provenance. `Capture` заменяет только `tests/fixtures/stripes/oracle.tsv`, `collectors.tsv`
и `provenance.json`; используйте его для подготовки кандидатов на ревью. Все четыре рабочих
пути обязательны. Каталог запуска должен быть новым, находиться внутри `.work` этого дерева
и не иметь перенаправляющего предка. Старые доказательства не перезаписываются.
Exit0 означает успех/справку; exit1 — ошибку входов/эталона, timeout, компиляции/процесса
или сравнения.

Launcher компилирует оба своих harness и девять точных pinned production sources qd-stripe только в private run classes. Существующие production classes предоставляют зависимости; upstream не пересобирается и не изменяется. Одновременно работает один дочерний процесс, Java heap128MiB и deadline60s.
Останавливаются только собственные просроченные процессы. Два явно заданных часовых пояса —
UTC и Pacific/Honolulu. Сеть и подключение источника не запускаются, глобальные свойства
не изменяются.

Каждый захват связан с хешами production-исходников, точным commit, полными inventories
production/generated-классов, фактической версией JDK, хешами executable/harness/launcher и fixture.
Полные UTC/raw потоки, inventories и receipt хранятся в частном каталоге запуска;
публичный provenance не содержит рабочих путей. После изменений Java необходим pinned
Google Java Format с отдельным JDK21+, затем захват/проверка настоящего обновлённого provenance.

Фиксированные сценарии охватывают допустимую/недопустимую грамматику, канонические имена,
различия фабрики по числу, все листья полос, ASCII, точные NUL/непарные/дополнительные UTF-16,
символы около границ диапазонов, префиксы/множители спреда, Java-наблюдения cipher/string
и совместимые либо неизвестные пересечения. Сырой wildcard cipher Java проверяется как
допуск во все листья отдельно от детерминированного строкового индекса полосы. Полные
фильтры `A*` и `A,B` дают неизвестный поднабор и требуют настоящего предиката на верхнем уровне.
Отдельный корпус collectors.tsv наблюдает все три семейства с MONO/HASH/RANGE, независимые пересекающиеся agent demand, partial retrieval, snapshot providers, replacement, sizes, drops и child closure. Сохранившийся HISTORY subscription size после close записан как есть. STREAM ALL scenario различает stripe hint и настоящий hard filter: более мелкий hash hint внутри крупного child допускает оба representative symbols; отдельный predicate оставляет один.
