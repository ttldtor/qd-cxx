# Оракул полей Candle

[English version](README.md).

`CandleOracle` сохраняет значения HISTORY закреплённой версии QD из
[upstream.lock](../../../upstream.lock). Он использует реальные делегаты Candle
и TradeHistory, а также бинарную запись и чтение QTP.

Соберите `tools/upstream/candle-payload-oracle/CandleOracle.java` через JDK 8 с
каталогами классов из [инструкции к оракулу Series](../series-oracle/README.ru.md).
Запустите `qdcxx.oracle.CandleOracle <новый-каталог> <профиль>`. Профили:
`standard`, `tiny`, `optional`, `long`, `numeric-long`, `trade` и `period`.
Для `tiny` задайте `-Ddxscheme.wide=false`. Для побайтовой сверки QTP и TSV
повторите захват в UTC и Pacific/Honolulu. Каждый запуск создаёт новый каталог.

Программа работает офлайн. Успешный захват или справка возвращают код 0.
Неверные параметры, существующий каталог или ошибка захвата дают ненулевой код.
При ошибке может остаться неполный вывод. Без аргументов, с `--help` или
`--help=ru` программа выводит справку.
