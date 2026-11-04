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

- [Oracle источников событий](../tools/upstream/event-source-oracle/README.ru.md): захват/проверка идентичности источников закреплённого Java, справка и ограниченные процессы.
- [Oracle возможностей источников](../tools/upstream/order-source-capability-oracle/README.ru.md): шесть семейств, полная книга и выбор конкретного источника.

- [CFI и дополнительные базовые активы oracle](../tools/upstream/glossary-oracle/README.ru.md): Production oracle CFI/AdditionalUnderlyings: справка без входов на EN/RU, JDK8, heap128MiB/60s, Git30s.
