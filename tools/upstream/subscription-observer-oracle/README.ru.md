# Production oracle наблюдателей подписки

[English version](README.md). Английская версия основная.

Ограниченный offline runner вызывает реальные production classes закреплённой Java: `DXFeedSubscription` и `ObservableSubscriptionChangeListener`. Он записывает чтение символов/типов, начальное состояние, повторные/no-op мутации, removal-before-addition, фактически удалённые decorators, self-removal/close, глубину повторного входа и propagated observer exceptions. Содержимое наборов сортируется для детерминированного представления; порядок callbacks и операций остаётся наблюдаемым. Логика подписки не эмулируется, feed connection не открывается.

`Invoke-Oracle.ps1 -Mode Capture|Verify -Upstream <pinned-checkout> -JavaHome <jdk8> -Logger <qdcxx-run-utc> -RunDirectory <fresh-private-run>` требует уже собранный reference из [upstream.lock](../../../upstream.lock). Каталог запуска должен быть новым, contained, без redirecting ancestors, внутри закрытой рабочей области checkout. Зависимости, SDK и сеть не приобретаются. Production classes хешируются до/после и не пересобираются. Одновременно работает один child, heap128MiB, deadline60s; остановить можно только собственный child при timeout.

Capture заменяет только [новые fixture и provenance](../../../tests/fixtures/subscription-observer/README.ru.md). Verify сравнивает результат и source/tool/class provenance с этими файлами. Полные UTC commands/exits, raw streams, inventories и receipts остаются в переданном закрытом каталоге. Результаты UTC и Honolulu должны совпадать. Exit0 означает успех/справку; Exit1 — invalid input, reference/provenance mismatch, timeout, process или observation failure.

Без аргументов, с `-Help`, `-HelpRu` и `Get-Help -Full` доступны назначение, параметры, ограничения, результат, exits и побочные действия без рабочих путей и запуска Java/source work. Неполный вызов без help остаётся ошибкой. Repository Java использует pinned formatter с JDK21+, отдельно от oracle JDK8.

[Native observation contract](../../../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.ru.md) явно отделяет raw Java monitor/reentry/exception outcomes от native outside-lock finite callback semantics. Прежние selector identity fixtures и ADR-0012 не изменяются.
