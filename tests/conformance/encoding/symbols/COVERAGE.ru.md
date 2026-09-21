# Покрытие conformance символов C++

[English](COVERAGE.md). Английская версия является нормативной.

[symbol_tests.cpp](../../symbol_tests.cpp) читает неизменяемые [наблюдения Java](../../../fixtures/encoding/symbols/expected/symbols.tsv).
Требуются точные числа: 3465 строк read / 693 основных сценария, 19 writer vectors, 114 чтений
последовательностей, семь integration deliveries, completion marker и уникальные read identities.
Отсутствующие/пустые fixtures не проходят. Сравниваются Java cipher, точные UTF-16, consumption,
исходные flags и flag-byte count.

После ошибки основного чтения продолжение C++ проверяется как FAILED/zero/no value, не как частично
сохранённое состояние Java. after-reset начинает новое сообщение; sentinel получает ещё один reset
C++, поскольку предыдущий undefined repeat Java был ошибкой. Успешные after-repeat/MRU сравниваются
с Java. Integration driver выбирает границы символов captured type-15 и пропускает ID пустой записи R;
декодированные значения сравниваются с реальной доставкой Java. Этот test-only driver не event decoder.

Дополнительные тесты покрывают input/output/state budgets, включая ноль и точные границы, отказ
supplementary по unit budget, приоритет length/resource, неподдерживаемую маску, усечение без
заимствования другого payload, terminal/reset, MRU reset, NUL/суррогаты и независимость результата
после изменения входа/reset/уничтожения. Production writer или C++ round-trip не служат oracle.

[Negative checker](../../check_symbol_negative.py) изменяет units, consumed count, repeat state, MRU,
cipher, удаляет сценарии/completion, подаёт неверные units, пустые и отсутствующие файлы. Требует
отказ каждого случая и успех неизменённого baseline. [Валидация](VALIDATION.ru.md) фиксирует финальную матрицу.
Не проверены огромные ресурсы, инъекция allocation failure, 32-битные targets, полный перебор
penta/Unicode/flags, кеш resolver, negotiation, семантика event metadata и транспорт.

## Доработка TSV по ревью

Каждая строка проверяется по схеме до обработки, включая Java-поля, исключённые из terminal-state
проекции C++. Закрытый набор типов и число колонок (включая тип строки):

| Тип | Колонки | Обработка |
| --- | --- | --- |
| profile | 5 | Только метаданные конфигурации; проверяются точные литералы ограниченного профиля |
| writer-error | 6 | Только отказ Java writer; сравнения с C++ writer нет |
| writer | 5 | Канонические доказательства Java writer, учитываются в счётчике; соответствующие main reads сравниваются с C++ |
| writer-sequence | 3 | Даёт captured bytes для последовательных чтений C++ |
| sequence | 6 | Сравнивает позиции, UTF-16 и флаги |
| read | 12 | Сравнивает документированную проекцию C++; raw Java string и состояние после ошибки остаются только доказательствами Java |
| integration | 2 | C++ читает captured symbol boundaries |
| replay | 3 | Только Java parser schedule одного chunk, production / [положительный count] |
| parse.begin | 2 | Только позиция входа Java parser |
| record | 3 | Сравнивает доставленные символы; проверяет числовой индекс и форму STREAM_DATA/0/R |
| callback | 4 | Только группировка callbacks Java; проверяет STREAM_DATA и счётчики |
| parse.end | 7 | Только сводка Java parser; проверяет все шесть счётчиков |
| complete | 3 | Требует symbols / 156 и ровно одну завершающую строку |

Отвергаются неизвестные типы, недостающие/лишние колонки (включая конечную пустую колонку), неверные
идентификаторы, hex bytes/UTF-16, неожиданные null, неизвестные литералы status, нечисловые/отрицательные
counts и Java ints вне знакового 32-битного диапазона. В hex запрещены знаки и пробельные символы.
Позиции sequence/read проверяются на границы. Ровно один complete должен быть последней строкой;
повторный complete, любые данные или пустая строка после него, отсутствие complete и ошибка потока
до EOF проваливают тест. Каждый диагностический тип имеет явный обработчик; общего игнорирования нет.

Прежние десять отрицательных проверок сохранены. Четыре новые мутации временных копий проверяют
unknown-row, wrong-column-count (лишняя пустая колонка profile), duplicate-complete и row-after-complete
(корректная строка parse.begin). Для каждой из четырёх требуется конкретная диагностика отказа и
ненулевой exit. Неизменённый baseline должен проходить, временные входы/эталоны не должны изменяться.
См. [результаты доработки](VALIDATION.ru.md#доработка-tsv-по-ревью).
