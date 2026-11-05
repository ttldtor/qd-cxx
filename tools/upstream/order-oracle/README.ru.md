# Production oracle Order

[English version](README.md). Английский текст нормативный.

Launcher захватывает или проверяет закреплённые стандартные схемы Order, исходные physical VOID и
long-tail slots, реальные HISTORY bytes BinaryQTPComposer/BinaryQTPParser и приём OrderDelegate.
Он не пересобирает QD, не открывает feed и не меняет общие production classes.

```powershell
pwsh -NoProfile -File tools/upstream/order-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/order-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/order-check
```

Без аргументов и с `-Help` выводится одинаковая EN-справка; `-HelpRu` — RU-справка. Рабочие пути для
неё не нужны, file/Java/SDK work не начинается. Mode по умолчанию Verify. Рабочим режимам нужны точная
[закреплённая ревизия](../../../upstream.lock), готовые production classes, JDK8, native UTC logger и
новый каталог внутри `.work` текущего checkout. Старые доказательства не перезаписываются. Каждый
Java child ограничен heap128 MiB и timeout60 секунд; останавливается только собственный зависший child.
Heap не равен process RSS. Сети, загрузок зависимостей и глобальных изменений нет. Exit0 — успех/справка;
exit1 — вход, reference, timeout, процесс, сравнение или ошибка вывода. При сбое остаются диагностики.

Четыре явных child scheme profiles: default wide, tiny decimal, integer price/size и FOB. Каждый
наблюдается в UTC и Pacific/Honolulu. Используются стандартные Order, Order#NTV и Order#BZX.
Контролируемые варианты сохраняют physical VOID high0/Index low1, остальные полные поля и source names.
Через настоящий parser/delegate проверяются numeric/raw alternatives, optional absence, отсутствие каждого
обязательного поля, flags, signed low-index границы и full-width removals. Отказы factory остаются
отрицательными доказательствами. Composer явно включает protocol option `hs`; parser обязан сохранить все значения flags от0 до31, включая snapshot begin/end. Возможность публикации source не означает разрешение подписки.

Capture заменяет только [Order TSV](../../../tests/fixtures/order/oracle.tsv), исходные binary fixtures
и [provenance](../../../tests/fixtures/order/provenance.json). Verify сравнивает их без изменения публичных
файлов. Логи, raw streams, exit codes и before/after inventories всех 14 class/resource roots сохраняются
в RunDirectory. Публичная provenance содержит стабильные хеши без локальных путей запуска.
[Контракт события](../../../spec/events/ORDER.ru.md) описывает native mapping и ограничения JSON layout.
