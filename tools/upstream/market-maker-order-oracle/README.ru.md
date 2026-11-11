# Production oracle MarketMaker и производного Order

[English version](README.md). Английский текст является определяющим.

Утилита сохраняет реальные MarketFactory delegates, исходные logical/full types и integer slots,
результаты MarketMaker/Order и bytes BinaryQTPComposer/BinaryQTPParser закреплённого [upstream](../../../upstream.lock).
Она использует готовые production classes без пересборки QD и подключения к feed.

```powershell
pwsh -NoProfile -File tools/upstream/market-maker-order-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/market-maker-order-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/market-maker-check
```

Mode по умолчанию Verify. Capture заменяет только отдельные TSV/QTP/provenance кандидаты в
`tests/fixtures/market-maker-order`; Verify сравнивает их без замены. Нужны существующий точный checkout,
14 готовых production class/resource roots, JDK8, UTC logger и новый каталог внутри `.work` этого checkout.
Там сохраняются полные logs/raw streams/exits и inventories classes до/после. Существующий каталог
запрещён. Каждый процесс имеет heap128MiB и timeout60s; завершается только собственный зависший child
утилиты. Сеть, сборка SDK, глобальные настройки и запись shared classes не используются. Exit0 — успех/
справка; exit1 — неверный вход, reference/provenance/comparison, процесс, timeout либо запись результата.
При ошибке сохраняется доступная диагностика.

Без аргументов и с -Help выводится одинаковая EN-справка, -HelpRu даёт RU; Get-Help -Full читает штатную
comment help. Эти режимы не требуют рабочих входов, не запускают Java и не создают файлы.

Separate/unitary defaults запускаются в свежих JVM. Третий профиль включает реальные поля
`dxscheme.nanoTime`. Все профили сравниваются в UTC и Pacific/Honolulu по observations и binary bytes.
Отдельные строки содержат реальные contract/SUB/PUB/WILDCARD/time-series/source facts. Corpus включает
Quote, Quote&Q, MarketMaker, полные numeric/raw альтернативы, optional absence, missing required fields,
все32 flags и пять full64 removal words. Composer явно включает hs, сохраняя snapshot flags. Native tests
сравнивают original/parsed delegates и пропускают настоящий QTP через C++ production parser/mappers.
NaN сравниваются по классификации, остальные double — по точным bits. Неиндексный cursor имеет getTime0,
его REMOVE compact-long равен нулю независимо от первых обычных полей.

[Контракт приёма](../../../docs/MARKET-MAKER-ORDER.ru.md) описывает публичный API. Публичный provenance
содержит стабильные hashes sources/classes/harness/launcher/fixtures без локальных evidence paths.