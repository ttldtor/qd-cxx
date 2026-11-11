# Production-oracle SpreadOrder и AnalyticOrder

[English version](README.md). Английский текст является нормативным.

Два launcher проверяют закреплённые production-схемы, BinaryQTPComposer, BinaryQTPParser и HISTORY-delegate
SpreadOrder/AnalyticOrder. Они используют готовые Java classes: без подключения к feed, сборки QD,
загрузки зависимостей и изменения глобальных свойств.

```powershell
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-FieldTimeOracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/order-variants-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/variants-check
```

Оба скрипта принимают одинаковые параметры. `Mode` по умолчанию равен `Verify`. Для Capture/Verify
обязательны `Upstream`, `JavaHome`, `Logger` и `RunDirectory`. Reference должен соответствовать
[upstream.lock](../../../upstream.lock), содержать готовые production classes и использовать JDK 8.
Форматтер использует отдельно закреплённый современный JDK. Каталог запуска должен быть новым и
находиться внутри `.work` этого checkout; прежние доказательства не перезаписываются. Вызов без
аргументов и `-Help` показывают одинаковую полную английскую справку, `-HelpRu` — русскую. Справка
не требует рабочих входов, не запускает Java и не изменяет fixtures.

Дочерние процессы выполняются последовательно: Java heap 128 MiB, timeout 60 секунд. Heap не ограничивает
RSS всего процесса. При timeout останавливается только дерево собственного дочернего процесса.
Сохраняются аргументы/коды команд, raw streams и before/after inventory всех 14 production class/resource
roots. Exit 0 означает успех/справку; exit 1 — неверные входы, reference, ошибку процесса/timeout, вывода
или сравнения. Неудачный захват сохраняет диагностику. Capture заменяет только fixtures/provenance
выбранной утилиты. Verify читает публичные fixtures, не заменяя их.

`Invoke-Oracle.ps1` отдельно наблюдает default-wide, tiny-decimal, configured-integer и FOB профили
в UTC и Pacific/Honolulu: 200862 строки TSV и 200 исходных QTP-файлов. Проверяются publication capabilities,
фактическая доступность standard records, optional-поля конкретных источников, отказы обязательных
полей, defaults отсутствующих optional-полей, raw/numeric границы, flags, full-width removal times и
точные nullable UTF-16 spread strings. FIELD-значения этого корпуса нулевые/default. Результаты:
`tests/fixtures/order-variants/oracle.tsv`, `wire/*.qtp` и
[provenance.json](../../../tests/fixtures/order-variants/provenance.json).

`Invoke-FieldTimeOracle.ps1` отдельно включает/выключает существующие FIELD write/read switches.
Его 51 строка содержит 48 типизированных наблюдений, отрицательные/full-width packed значения и независимое
business time. Результаты: [field-time-oracle.tsv](../../../tests/fixtures/order-variants/field-time-oracle.tsv),
`field-wire/*.qtp` и [field-time-provenance.json](../../../tests/fixtures/order-variants/field-time-provenance.json).
Каждый TSV и wire-файл должен совпадать в UTC и Pacific/Honolulu. Stable provenance связывает harness,
launcher, upstream source hashes и неизменный compiled-class inventory без локальных путей запуска.

[Контракт вариантов](../../../spec/events/ORDER-VARIANTS.ru.md) разделяет publication capability,
доступность standard scheme и приём явно объявленного record. Эти oracle не проверяют сетевой transport,
пользовательские callback executors и native Feed lifecycle.