# Наблюдения источников событий

[English](README.md). Английская версия является нормативной.

[oracle.tsv](oracle.tsv) содержит 311 фактических наблюдений классов IndexedEventSource
и OrderSource закреплённого Java QD. [provenance.json](provenance.json) хранит ревизию,
хеши harness/launcher/upstream, inventory production-классов, JDK и точный SHA-256 fixture.
Формат: операция, вход, ID, UTF-16 hex имени, знаковый hashCode; строки ошибок содержат
имя исключения Java. EQUAL/CROSS/UNEQUAL содержат фактические boolean-результаты.
NULL отличается от пустой строки; каждые четыре hex-цифры кодируют одну единицу UTF-16.

Все встроенные поля отражены через reflection и проверены через оба valueOf.
Дополнительные случаи покрывают неизвестные имена, границы ASCII, внутренний NUL,
отрицательные/малые/неалфавитные ID, nullable/пустые/непарные UTF-16 имена базы,
разные имена одного ID и равенство между семействами. Это полезный ограниченный
корпус, а не исчерпывающий перебор всех 32-битных ID.

[Воспроизведение](../../../tools/upstream/event-source-oracle/README.ru.md).
Capabilities, index packing, record mappings и routing не наблюдаются этим fixture.
