# Оракул QD для Underlying

[English version](README.md).

UnderlyingOracle использует закреплённую версию QD из [upstream.lock](../../../upstream.lock).
Он записывает описание Underlying и значения Java-делегата после бинарной записи и чтения
QTP для TICKER и HISTORY. Каждый запуск создаёт новый каталог с двумя QTP-файлами
и двумя TSV-файлами. Код 0 означает успех. Неверные аргументы, существующий каталог
или ошибка захвата дают ненулевой код. При ошибке может остаться неполный каталог.
Программа работает офлайн и без аргументов выводит справку.

Соберите исходник JDK 8 с каталогами классов из
[инструкции к оракулу Series](../series-oracle/README.ru.md). Используйте
`tools/upstream/underlying-oracle/UnderlyingOracle.java` и класс `qdcxx.oracle.UnderlyingOracle`.
Передайте один аргумент — путь к новому каталогу. Для wide-decimal используйте
параметры по умолчанию, для tiny-decimal добавьте `-Ddxscheme.wide=false`.
В каждом режиме QTP и TSV совпадают побайтно в UTC и Pacific/Honolulu.
