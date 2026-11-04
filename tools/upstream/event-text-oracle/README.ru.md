# Оракул текста событий

[English version](README.md). Английская версия является основной.

Launcher вызывает реальные закреплённые методы QD Quote/Profile/Trade/TradeETH/Summary, TimeFormat, EventUtil, enum, sources и WideDecimal. Java 25 служит числовым эталоном; наблюдения Java 8 классифицируются отдельно согласно [ADR-0011](../../../docs/decisions/ADR-0011-java-event-text-reference.ru.md). Сборка SDK/classes и сеть не используются.

~~~powershell
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/event-text-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk25 -LegacyJavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-text-check
~~~

Mode по умолчанию Verify. Рабочие пути обязательны; RunDirectory должен быть новым и находиться внутри .work этого checkout. Capture записывает только event-text fixture/proof/provenance кандидаты. Verify сравнивает без их изменения. Оба сохраняют полные команды, UTC-время/exits, исходные streams и source/class inventories в RunDirectory. Java запускается последовательно с heap128MiB и timeout60 секунд; останавливается лишь собственное просроченное дерево процессов. Код0: успех/справка; код1: неверный вход/reference, timeout/process, ошибка provenance или наблюдений.

Без аргументов и -Help выводится одинаковая справка EN; -HelpRu переводит её; Get-Help показывает штатную справку EN. Рабочий вход не нужен, выходные файлы работы не создаются. JDK25 подходит и для отдельно закреплённого Java formatter.

Fixture содержит profile, kind, input, result. Числа/characters/time/enum/flags записаны точным ASCII; остальные результаты — четырёхзначные строчные UTF-16 units через запятую с сохранением malformed units и NUL. Proof числовых версий требует одинакового обратного преобразования в биты и определённой классификации decimal selection. Три cache-исключения требуют точных кешированных/свежих Java-результатов; остальные строки сравниваются строго.

Проверяются шесть UTC/fixed-контекстов (включая обе границы signed32 миллисекунд и субсекундное смещение) и восемь точных именованных зон из [описания корпуса](../../../tests/fixtures/event-text/README.ru.md). Каждый отдельный Java-процесс задаёт зону по умолчанию до инициализации TimeFormat; общий форматтер не перенастраивается. C++ использует отдельно квалифицированный неизменяемый провайдер IANA 2026f.