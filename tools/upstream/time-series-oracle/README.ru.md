# Production oracle идентичности time-series

[English version](README.md).

`Invoke-Oracle.ps1` захватывает или проверяет 123 наблюдения закреплённых production-классов Java8:
`TimeSeriesSubscriptionSymbol`, `DXFeedTimeSeriesSubscription` и бизнес-маркеров событий.
Покрыты точный UTF-16, знаковые границы времени, независимые от диапазона хеш/равенство,
вложенные обёртки, мутации набора/диапазона, начальный MAX, обход raw generic и наследуемые
пользовательские маркеры. Сырое асимметричное равенство сохранено;
[ADR-0012](../../../docs/decisions/ADR-0012-decorated-subscription-identity.ru.md) задаёт native-исправление.
Наблюдение Java-свойства после close не добавляет lifecycle чистой native-модели.

Без аргументов, с `-Help`, `-HelpRu` и через `Get-Help ./Invoke-Oracle.ps1 -Full` пути не нужны,
Java и основная работа не запускаются.

```powershell
pwsh -NoProfile -File tools/upstream/time-series-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/time-series-check-01
```

Verify выбран по умолчанию и сравнивает fixtures. Capture заменяет только
`tests/fixtures/time-series/oracle.tsv` и `provenance.json`. Все четыре рабочих пути обязательны.
Каталог результата должен быть новым внутри `.work` checkout, без перенаправляющих предков;
существующие доказательства не перезаписываются. Используются готовые закреплённые классы,
без сборки и записи upstream. Полные перечни классов/ресурсов до и после, хеши исходников,
исполняемых файлов и harness, UTC-команды и exit codes связывают запуск с входами.
Лимит каждого процесса — 60s, heap javac/Java — 128MiB, процессы последовательны.
Останавливаются только собственные дочерние процессы с истёкшим сроком. UTC и Pacific/Honolulu
должны дать побайтно одинаковый результат. Exit0 означает успех/справку; Exit1 — неверный ввод,
несовпадение reference, ошибку процесса/таймаут или изменение наблюдений. Полные логи остаются
в RunDirectory. Сеть и подготовка SDK не используются.

[Native-контракт](../../../spec/subscriptions/TIME-SERIES-IDENTITY.ru.md) разделяет логическую
capability и доступность HISTORY Feed, сохраняя обязательства вызывающей стороны по хешу/равенству.
