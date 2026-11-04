# Production-наблюдения текста событий

[English version](README.md). Английская версия является основной.

[provenance.json](provenance.json) указывает закреплённую ревизию QD, точные hashes Java/compiler/harness/launcher, production-class inventory и hashes результатов. [oracle.tsv](oracle.tsv) содержит 182957 фактических наблюдения существующих production methods через Java 25. Отдельный Java 8 capture сохранён с полными рабочими доказательствами; [numeric-version-differences.tsv](numeric-version-differences.tsv) содержит обе наблюдённые записи и точное доказательство decimal selection для каждого различия.

Входы включают deterministic seeded binary64; каждый exponent с граничными significands; положительные/отрицательные subnormals; соседние значения десятичных степеней; signed zero, NaN и infinities; каждый UTF-16 unit; все объявленные имена market enum; точные nullable/custom/built-in source names; raw flag words; полные Quote/Profile/Trade/TradeETH/Summary default/filled/raw-flag events; signed64/date-boundary timestamps. Явные профили: UTC, offsets +19800000/-27000000/1234/-2147483648/2147483647 миллисекунд и три локали.

Строка содержит profile, kind, input, result. Scalars записаны точным ASCII; полные event/source strings используют четырёхзначные строчные UTF-16 hex units через запятую. NUL и malformed UTF-16 сохраняются без платформенного transcoding. [Три cache-исключения](upstream-cache-differences.tsv) содержат точные кешированные и свежие Java-результаты; остальные строки остаются строгими.

Конечный корпус не исчерпывает binary64/signed64 и не доказывает ещё не реализованные типы событий или эквивалентность произвольных версий данных часовых поясов. См. [проверки](../../conformance/EVENT-TEXT-VALIDATION.ru.md), [инструмент](../../../tools/upstream/event-text-oracle/README.ru.md) и [решение об эталоне](../../../docs/decisions/ADR-0011-java-event-text-reference.ru.md).

Восемь именованных профилей: America/New_York, Europe/Berlin, Asia/Kathmandu, Australia/Lord_Howe, Europe/Dublin, Pacific/Apia, SystemV/EST5EDT и Asia/Tokyo. Их 1296 наблюдений отдельно сняты с Java 8 и побайтно совпали с Java 25. C++ использует отдельно квалифицированную IANA 2026f; [36 классифицированных различий данных](../schedule-query/tzdb2026f-provenance.json) провайдера сохраняются отдельно от трёх исключений кеша форматтера.