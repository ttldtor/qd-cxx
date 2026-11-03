# Oracle значений IPF

[English version](README.md). Английская версия нормативная.

`value-oracle.tsv` содержит 22 738 фактических наблюдения закреплённого Java QD 3.355, ревизия
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, JDK 8. [Provenance](provenance.json)
связывает fixture с исходниками upstream, инвентаризацией production classes и отформатированными
harness/launcher. Fixture сохраняет захваченные Windows CRLF bytes; строки UTF-16 представлены
шестнадцатеричными кодовыми единицами.

Строка: `operation<TAB>nameHex<TAB>input<TAB>result`. `text`, `number`, `date` создают новый профиль,
записывают текст и наблюдают нормализованный текст, binary64 bits или day ID. `format` задаёт
binary64 bits и наблюдает текст; `date-format` задаёт знаковый day ID и наблюдает текст.
Вход — UTF-16 hex, кроме bits/day ID числовых setters. Результат — UTF-16 hex, numeric bits,
знаковый day ID либо `ERROR` для Java IllegalArgumentException. Последний пустой столбец значим.

Покрыты все стандартные поля/defaults, пользовательские поля и ошибки текстовой категории,
числовой синтаксис/переполнение/потеря малости, насыщение целого, знаковый ноль, неограниченные
значения, границы точности, 16 384 детерминированных случайных binary64, каждый binary exponent,
десятичные степени/соседи, subnormal, исторические/lenient/ошибочные даты, цифры Unicode 6.2 и
переполняющиеся компоненты дат. UTC и Pacific/Honolulu дают одинаковые bytes. Нативные conformance
тесты дополняют проверку точным UTF-16 владением, нормализацией null/empty, независимостью
copy/move/edit, идентичностью символа, равенством/представлением и отказами конечных лимитов
экземпляра. Parser/writer/network не покрыты.

[Воспроизводимый oracle](../../../tools/upstream/ipf-value-oracle/README.ru.md) использует готовые
закреплённые classes без загрузки зависимостей и пересборки SDK upstream. Runtime/tool hashes —
provenance, а не публичные бинарные предпосылки обычного потребителя C++.
