# Документация разработчика {#developer-documentation-ru}

[English version](developer.md) · [Главная](index.ru.md)

Этот раздел нужен для сборки необязательных компонентов, изменения библиотеки и проверки совместимости.
Подключение приложения и использование описаны в [начале работы](getting-started.ru.md)
и [практических руководствах](guides.ru.md).

## Сборка и инструменты

- [Подробные параметры сборки и компоненты](CPP-BUILD.ru.md)
- [Требования к изменениям](../CONTRIBUTING.md) (англ.), [соглашения C++](CPP-STYLE.ru.md) и [имена](NAMING.ru.md)
- [Каталог инструментов разработчика](../tools/dev/README.ru.md): UTC-журналы, форматирование и автономные проверки совместимости
- [Сборка этой документации](../tools/docs/README.ru.md)
- [Эталон upstream](UPSTREAM.md) (англ.) и [политика совместимости](COMPATIBILITY.ru.md)

## Результаты проверок и ограничения

Результаты относятся к точным входам и профилям инструментария. Анализ исходников, нативное исполнение
и независимые повторные проверки — разные доказательства; они не являются общей сертификацией безопасности
или гарантией RSS/жёсткого времени.

- [Квалификация TLS runtime и ресурсов](../tests/conformance/transport/TLS-VALIDATION.ru.md)
- [Квалификация TCP runtime](../tests/conformance/transport/TCP-VALIDATION.ru.md)
- [Наблюдение диапазона HISTORY в recovery](../tests/conformance/transport/HISTORY-STATUS-VALIDATION.ru.md)
- [Чтение tape и поддержанные входы](../spec/protocol/TAPE-READER.ru.md)
- [Проверки записи tape](../tests/conformance/protocol/tape-write/WRITER-VALIDATION.ru.md)
- [Проверки экспорта схем](../tests/conformance/schema-export/VALIDATION.ru.md)
- [Совместимость подписок](../tests/conformance/subscriptions/VALIDATION.ru.md)
- [Составные профили получения и проверки CLI](../tests/conformance/subscriptions/DELIVERY-VALIDATION.ru.md)
- [Получатель подписок и установленный консольный пример](../tests/conformance/subscriptions/RECEIVER-VALIDATION.ru.md)
- [Публичный feed API, установленный пакет и live Quote](../tests/conformance/subscriptions/FEED-VALIDATION.ru.md)
- [База знаний и связь решений](knowledge/README.ru.md)

[Текущие возможности](CURRENT-STATE.ru.md) отделяют готовые компоненты от
[дальнейшего развития продукта](NEXT-STEPS.ru.md). Эталонные данные conformance и технические спецификации
остаются в исходной поставке; навигация не превращает предложения в поддержанное поведение.

- [Java oracle текстовых событий](../tools/upstream/text-events-oracle/README.ru.md): конечные наблюдения стандартных делегатов и wire.
- [Oracle источников событий](../tools/upstream/event-source-oracle/README.ru.md): захват/проверка идентичности источников закреплённого Java, справка и ограниченные процессы.
- [Oracle возможностей источников](../tools/upstream/order-source-capability-oracle/README.ru.md): шесть семейств, полная книга и выбор конкретного источника.

- [CFI и дополнительные базовые активы oracle](../tools/upstream/glossary-oracle/README.ru.md): Production oracle CFI/AdditionalUnderlyings: справка без входов на EN/RU, JDK8, heap128MiB/60s, Git30s.

- [Ценовые шаги инструмента](PRICE-INCREMENTS.ru.md): диапазоны, точность и округление цены через `qdcxx_ipf`.

- [Контракт IPF collector](../spec/ipf/COLLECTOR.ru.md): SDK-free потокобезопасная модель, конечное удержание/работа и callback cutoff; [Java oracle](../tests/fixtures/ipf/collector/README.ru.md).

## Утилиты значений событий

[Oracle значений событий](../tools/upstream/event-values-oracle/README.ru.md) захватывает и проверяет точные закреплённые Java-преобразования без сборки SDK и endpoint. См. встроенную EN/RU справку и [публичные контракты](../spec/events/EVENT-VALUES.ru.md). [Installed consumer](../tests/conformance/event_values_consumer/CMakeLists.txt) отдельно компилирует каждый новый публичный заголовок и проверяет сохранение raw, преобразования и ошибки входов; он линкует SDK-free value target либо совместимый feed target.

- [Контракт live IPF connection](../spec/ipf/CONNECTION.ru.md): явный joined runtime, private incremental sink, конечные накопленные grants; [actual Java oracle](../tests/fixtures/ipf/live/README.ru.md).

- [Проверки текста событий](../tests/conformance/EVENT-TEXT-VALIDATION.ru.md): production Java profiles, точные различия эталона, числовое округление, ограниченное владение и установленные consumers.
- [Публичный пример live IPF](../examples/ipf_live/README.ru.md): отдельный installed consumer, EN/RU help без inputs, конечные duration/revision capture и creator shutdown; [shutdown oracle](../tests/fixtures/ipf/live/README.ru.md#shutdown-oracle) использует pinned JDK8 с конечным собственным cleanup.

- [Получение Message/Configuration и кодеки](MISC-EVENTS.ru.md): владеющие payload, явный decode и Java-совместимый version admission Configuration; [version oracle](../tools/upstream/configuration-version-oracle/README.ru.md).

- [Приём индексного Order](ORDER.ru.md): явный HISTORY, полные владеющие поля и перекрывающиеся source selectors.
- [Oracle MarketMaker/производного Order](../tools/upstream/market-maker-order-oracle/README.ru.md): полные типы, physical time, источники delegates и реальный binary parser.
- [Oracle SpreadOrder и AnalyticOrder](../tools/upstream/order-variants-oracle/README.ru.md): записи конкретных источников, физическое время FIELD и исходные байты QTP.
- [Oracle parser/delegate Order](../tools/upstream/order-oracle/README.ru.md): исходные physical slots и ограниченные наблюдения стандартных схем.
- [Оракул Series](../tools/upstream/series-oracle/README.ru.md): двоичная запись и чтение HISTORY закреплённой версией, варианты схем и значения опционных серий.
- [Oracle Candle HISTORY](../tools/upstream/candle-lifecycle-oracle/README.ru.md) записывает точные снимки Java, локальное воспроизведение, коррекции того же индекса и исходный QTP.
