# Oracle возможностей источников заявок

[English](README.md). Английская версия является нормативной.

Invoke-Oracle.ps1 захватывает/сверяет actual production OrderSource и
IndexedEventSubscriptionSymbol закреплённого Java. Он не собирает SDK, не обращается
к серверу и не доказывает интеграцию выбора в Feed. [Контракт](../../../spec/events/EVENT-SOURCES.ru.md).

~~~powershell
pwsh -NoProfile -File tools/upstream/order-source-capability-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/order-source-capability-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/order-capability-check
~~~

Без аргументов и с -Help выводится одинаковая EN-справка; -HelpRu выводит RU.
Справка доступна без рабочих путей, создания каталогов и запуска Java; Get-Help -Full
задаёт тот же контракт. Mode по умолчанию Verify. Ошибочные/неполные рабочие вызовы
завершаются ошибкой. Upstream, JavaHome, Logger и RunDirectory обязательны.
Reference должен совпадать с upstream.lock, исходники — с HEAD; production classes
должны быть заранее собраны, JavaHome — JDK8. RunDirectory находится внутри .work
текущего checkout; его собственные логи/наблюдения могут заменяться.

Все Java-процессы, включая version, последовательны, ограничены 60 секундами и
heap128MiB. Останавливается только собственное дерево процесса по таймауту.
Полные command streams, хеши исходников, class inventory до/после и private receipts
остаются в RunDirectory. Публичная provenance содержит реальную Java version и
хеши без COMMAND/private paths. UTC и Pacific/Honolulu должны совпадать побайтно.
Capture заменяет только tests/fixtures/order-source-capabilities/oracle.tsv и
provenance.json кандидатами. Verify их не меняет. Exit0 — успех/справка; exit1 —
неверные входы, reference/provenance mismatch, timeout, ошибка процесса или I/O.

503 строки покрывают 64 встроенных и пять пользовательских источников для шести
семейств и полной книги, порядок отфильтрованных списков, ошибочные null/посторонние
типы, произвольные ID конкретного источника, ID-only равенство символов, отличие от
обычного символа и запрет null-источника. Семантика ALL обычного символа описана в
IndexedEventSubscriptionSymbol; чистый C++ селектор не доказывает работу Feed.
Исходные байты TSV сохраняются; форматтер исходников к ним не применяется.
