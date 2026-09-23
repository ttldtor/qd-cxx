# Покрытие Java настраиваемых событий

[English version](COVERAGE.md). Английская версия нормативна.

Oracle использует неизменённую закреплённую QD `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
Это наблюдения Java, а не результаты C++. [Контракт](../../../../spec/schema/CONFIGURED-EVENTS.ru.md)
и [семантика Profile](../../../../spec/events/PROFILE.ru.md) задают принимаемые ограниченные проекции.

| Сценарии | Фактическая поверхность и наблюдение |
| --- | --- |
| default | Неизменённые полные default Quote/Profile; настоящие delegate putEvent/createEvent, FileWriter/FileReader в STREAM/NONE и RAW/MESSAGE; три различимых события каждого вида/режима |
| sequence, nano, both, explicit | Четыре независимые комбинации optional Quote; явный выбор поля перекрывает nano shortcut; subsecond packed Sequence отличается от независимого TimeNanoPart |
| endpoint-file, endpoint-api | Разрешение схемы FEED endpoint; конфликт file/system/API; файл выше system, явный builder выше файла |
| set-before, mutation | setProperty до инициализации влияет на singleton, после — нет; независимо настроенный endpoint отличается без изменения исходной схемы |
| xml, xml-system, xml-properties | Настоящий resource:dxlink.xml с imports; исходные options игнорируют JVM properties; включение options даёт приоритет явным loader properties |
| xml-extension | Отдельный совместимый XML overlay включает оба optional Quote и добавляет generic поля полных8/10; исходные схемы сохранены |
| reorder | Отдельные обычные программные записи разворачивают порядок логических полей, добавляют full8 поля и размещают Quote на local ID1 |
| minimal | Остаются только обязательные price/size Quote и Description Profile; все defaults при отсутствии optional доставлены через production delegates/files |
| numeric-int, integer-representation | Отдельные программная all-numeric8 и property-selected numeric8 схемы; проекция знакового int измеряется отдельно от wire conversion |
| rejected | Отсутствие обязательных локальных полей не даёт event delegate; положительный ChosenPrice alias и конфликтующий BidPrice проверены настоящим DefaultRecord |
| xml-unsupported, tiny-unsupported | Исходные XML string полного137 и tiny decimal полного24 сохранены; они не поддерживаются ограниченным C++ full-ID профилем |
| invalid-boolean | Java сообщает диагностическую ошибку неверного nanoTime и использует false |

Все 19 сценариев с файлами дополнительно содержат отдельные пустые wire descriptions через
production FileReader: обязательные локальные поля остаются, доставляются raw defaults.
Это отличается от отсутствия локально в minimal. Точно сохранены null, пустые строки, NUL, парные
и одиночные суррогаты; decimal inputs включают NaN, бесконечности и отрицательный ноль.
Даты и секунды включают отрицательные и граничные значения. Flags включают ноль, reserved bits
и -1; raw flags независимы от нулевых QTP event flags. Сохранены исходные input и delegate-put:
округление, усечение до секунд и отсутствие неподдержанного свойства не скрываются сравнением
только с идеальным input.

Каждое сравнение put/read проверяет все mapped properties относительно фактического production
delegate output с eventTime=0 для STREAM и 1600000000123 для RAW. Callback count фиксирует RAW
fanout; только STREAM callbacks задают ожидания одного события на запись. Отдельные прямые пробы
Sequence записывают INT_MIN/-1/0/1/4194303/4194304/INT_MAX в cursor и вызывают production delegate.
Они доказывают проекцию getter, а не допустимость setter или файловый production тест этих raw bits.

Source-resolution покрывает реестр CONFIGURED-EVENTS, а не произвольные providers, все aliases
конфигурации, environment sources, live updates и все события dxlink. C++ принимает явно разрешённые
данные; отсюда не следуют общий schema loader, новые типы, индексные записи, wire coercion, сжатие,
расширенные transport/session semantics и поддержка частного tape.
