# Выражения и классификация Feed-фильтров

[English version](FEED-FILTERS.md). Нормативна английская версия.

Feed-фильтр — неизменяемый классификатор запросов. Он не устанавливает фильтр подписки,
не меняет желаемые символы, не открывает источник и не выполняет сетевую работу. Builder
владеет копиями упорядоченных имён категорий и выражений и сохраняет неизменяемый
`EventRegistry`. Побеждает первая категория, совпавшая через любой выбранный делегат,
независимо от порядка делегатов. Принятие и классификация используют один набор координат.

## Существующие координаты

Native registry поддерживает Quote, Profile, Trade, TradeETH, Summary, TextMessage,
TextConfiguration, Message и Configuration.
Запрос использует реальные настроенные имена записей и физические символы; тип события не выводится из
имени записи. При наличии точного регионального маршрута выбирается он, иначе composite.
Региональный маршрут убирает суффикс биржи символа, сохраняя атрибуты. Существующий native
Profile adapter использует текст без регионального преобразования. Это границы текущего
registry; поддержка произвольных Java provider/delegate plugins не заявляется.

Time-series проверяется раньше indexed. У текущих проекций нет time-series-делегатов.
Явный indexed source не создаёт новых возможностей источников рыночных записей. Обычная
строка, начинающаяся с `*`, не имеет делегатов подписки; явный ALL — отдельный тип запроса.
Запрос готового события использует publisher-координаты без этого ограничения подписки.

TextMessage, TextConfiguration, Message и Configuration используют generic delegate coordinates: полное имя
символа сохраняется, включая `&Q`, даже если пользовательская запись похожа на региональную.
В indexed-запросе source0 и отрицательные ID выбирают доступные делегаты, положительный ID
даёт no-match. Это отличается от MarketEventDelegateSet и не меняет его прежние правила.
Payload null/empty/body, version и metadata времени не участвуют в классификации.
Message и Configuration сохраняют непрозрачные сериализованные байты attachment;
классификация не декодирует их и не вызывает codec.
Владеющий C++ symbol имеет тип `std::u16string`: пустая строка соответствует явному пустому
символу Java, а null symbol/model Java не представимы этим value API. Raw production Java
возвращает NullPointerException для null, включая события без заданного символа; null
не преобразуется в пустую строку или no-match.

## Грамматика выражений

Приоритет: унарный `!`, затем пересечение `&`, затем объединение `,`. Скобки группируют
выражения. Ключевые слова `not`, `and`, `or`, `record`, `any` с учётом разделителей
разворачиваются в `!`, `&`, `,`, `:`, `*`. Квадратные скобки экранируют составные операторы;
фигурные скобки вне квадратных тоже экранируют их. Обратная косая черта экранирует
следующую кодовую единицу. `\Q...\E` экранирует блок на стадии токенизации; полученный
токен затем проходит собственный leaf parser. Начальные ASCII-пробелы пропускаются.
Остальные пробелы значимы, кроме окружения распознанных ключевых слов, как в Java parser.

Начальный `:` задаёт шаблон имени записи. Неизвестные токены используют QD glob, не
регулярные выражения. Начальная строчная буква зарезервирована, если не экранирована:
например, `[a]bc`. Допустимы не более двух несмежных `*`, каждая означает ноль или более
кодовых единиц UTF-16. `**` и третья `*` запрещены. Классы, `^` сразу после `[`, включающие
диапазоны, backslash и block quotes повторяют QD bit-class semantics. Обратный диапазон
не добавляет элементов диапазона, но уже добавленная левая граница остаётся. Пустые
классы синтаксически допустимы. Незакрытые классы/диапазоны, конечный backslash и
незакрытые block quotes — ошибки. Зарезервированные неэкранированные операторы вне
классов запрещены. Текст шаблона допускает ASCII32..126. Входные единицы выше126
попадают в bucket127; supplementary-символ занимает две позиции UTF-16. NUL не совпадает
с инвертированным классом. `?` зарезервирован, а не означает один произвольный символ.

Поддерживаемые стандартные токены: `feed`, `compfeed`, `regfeed`, `chartdata`, `convrates`,
`nwc`, `hashMofN`, `range-A-B-` и семейства `bs`, `fut`, `opt`, `spread`, `cs`, `csopt`,
`bsopt`, `futopt`, `ind`, `prod`, `fx`. Форма с суффиксом `symbol` убирает неявное
ограничение записей `feed`. M и N — неотрицательные десятичные Java-int; N — степень
двойки не меньше2, M<N. Используется Java hash UTF-16, умноженный по модулю2^32 на
0xB46394CD, и старшие log2(N) бит. Границы range содержат до8 ASCII букв/цифр;
левая включается, правая исключается. Пустые границы открыты. Сравнение пропускает
необязательный числовой множитель spread и последующий недопустимый словарю префикс.

## Проекция wildcard-запроса

ALL использует QD wildcard semantics. Критерии записей остаются точными; обычные
символы и данные проверяются по результирующему точному предикату. Статические шаблоны
без звёздочек и non-ASCII bucket классифицируются как symbol sets при мощности до
закреплённого порога10000, как в Java. Этот порог совместимости отделён от конечных
настроек `finiteSymbols` и `normalizationWork`.

Если AND имеет такой конечный seed, остальные статические symbol-only критерии
проверяются по точным кандидатам. Пустое пересечение превращается в NOTHING и
отклоняет ALL: `A&B`, `A&!A`, `(A,B)&!A&!B` ложны. Record/mixed и dynamic критерии
не подменяются выводами о символах. Cofinite NOT проверяется по конечному seed,
без перечисления бесконечного дополнения. Symbol-set membership для `*` отличается
от принятия QD wildcard cipher: `[*]&A` ложно, `[*]&![*]` принимает ALL.

Шаблоны выше порога и шаблоны со звёздочкой не перечисляются. Поэтому `A*&!A*`
принимает ALL, повторяя реальное Java поведение; аналогично пересечение шаблона
из14641 комбинации с его отрицанием остаётся glob projection. Меньший бюджет
перечисления или работы даёт типизированный RESOURCE_LIMIT/WORK_LIMIT до публикации,
а не приблизительный ответ. Неподтверждённое TODO в Java не считается дефектом.
Текстовая `*` подписки отбрасывается до projection; текстовая `*` данных проходит
publisher path и кодируется как QD wildcard cipher.

## Граница ресурсов

Явно ограничены категории, единицы текста, узлы, глубина дерева, позиции шаблона,
координаты, длина символа запроса и консервативный объём хранения. Отказ не публикует
фильтр. Отказ builder сохраняет прежние категории. Фильтр владеет строками и registry
после уничтожения builder и входов. Для category views нужна живая владеющая копия
фильтра. Ошибки выделения памяти возвращаются через detached public error; диагностика
не содержит credentials источника.

IPF-фильтру нужен явный владелец resource context. Разбор классификатора не запускает
native networking. Неизменяемый snapshot, tracker и необязательный source service имеют
разные обязанности владения. Snapshot публикуется после успеха полной source operation
с явной политикой завершения; COMPLETE не отменяет ошибку последующего текста. При
warning policy отсутствие маркера допустимо. Старые snapshot не меняются после обновления
или ошибки источника. Service владеет ограниченными попытками, clock/schedule и остановкой;
клиент не выполняет цикл опроса.

## Доказательства совместимости

Первичные классы: `DXFeedFilter`, `DXFeedFilterImpl`, `DXFeedFilterTrackerImpl`,
`CompositeFilters`, `PatternFilter`, `FilterFactoryImpl`, `HashFilter`, `HashStriper`,
`RangeFilter`, `RangeUtil`, `IPFSymbolFilter`, `IPFUpdater`; версия закреплена в
[`upstream.lock`](../../upstream.lock).
[Production oracle](../../tools/upstream/feed-filter-oracle/README.ru.md) сохраняет
[raw classifier observations](../../tests/fixtures/feed-filter/oracle.tsv) и
[IPF observations](../../tests/fixtures/feed-filter/ipf-oracle.tsv), хеши исходников и
class inventory. Чтение исходников, выполнение Java и native regression — разные доказательства.


Идентичность IPF-содержимого включает точное множество символов и глобальное множество
TRADING_HOURS. Порядок, повторения и привязка hours к отдельным профилям не влияют;
изменение только множества hours создаёт новое поколение и уведомляет tracker.


Первичные исходники закреплённой версии: [DXFeedFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedFilter.java), [DXFeedFilterImpl](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedFilterImpl.java), [CompositeFilters](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/CompositeFilters.java), [PatternFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/PatternFilter.java), [standard factory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/FilterFactoryImpl.java), [IPFSymbolFilter](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-ipf-filter/src/main/java/com/dxfeed/ipf/filter/IPFSymbolFilter.java), [tracker](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedFilterTrackerImpl.java).
