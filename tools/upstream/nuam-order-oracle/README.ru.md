# Генератор фикстур QD NuamOrder

[English version](README.md).

`NuamOrderOracle.java` записывает HISTORY QTP и наблюдения Java-делегата из [закреплённой ревизии QD](../../../upstream.lock) для источников DEFAULT, `NUAM` и `nuam`. [Фикстуры](../../../tests/fixtures/nuam-order/provenance.json) проверяют source/index, отсутствующие и пустые строки, числовые значения и зарезервированные коды флагов.

Для сборки нужен JDK 8, а `$qdClassPath` должен содержать собранные модули закреплённого QD:

```powershell
javac -encoding UTF-8 -cp $qdClassPath -d classes NuamOrderOracle.java
java -Duser.timezone=UTC -cp "classes;$qdClassPath" qdcxx.oracle.NuamOrderOracle capture
```

Укажите новый выходной каталог. Без аргументов или с `--help` программа выводит английскую справку без создания файлов, с `--help=ru` — русскую. Захват работает офлайн и создаёт три пары QTP/TSV. Код 0 означает успех; неверные аргументы, существующий каталог и ошибки захвата дают ненулевой код. После ошибки возможны частичные файлы. Сохранённые файлы совпадают в UTC и Pacific/Honolulu.
