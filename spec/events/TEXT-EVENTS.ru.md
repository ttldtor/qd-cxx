# TextMessage и TextConfiguration

[English version](TEXT-EVENTS.md). Английский текст является нормативным.

`TextMessage` — событие STREAM; `TextConfiguration` хранит последнее значение. Оба типа владеют символом и nullable-текстом UTF-16. Отсутствующий текст отличается от пустой строки. Событие и текст живут независимо от декодированной записи и span обратного вызова.

Стандартное поле `Text` имеет полный идентификатор `UTF_CHAR_ARRAY` (10). Длина в протоколе считает единицы UTF-16, а тело использует кодирование массива символов QD, включая непарные суррогаты. См. [FIELD-TYPES](../encoding/FIELD-TYPES.ru.md). Подготовленный mapper также принимает точный идентификатор `STRING` в пользовательской схеме. Он проверяет полную форму записи, включая невыбранные дополнительные поля.

Для `TextMessage` обязательны `Time`, `Sequence` и `Text`; для `TextConfiguration` обязательно `Text`, а `Time`, `Sequence` и `Version` могут отсутствовать. Отсутствующие необязательные скаляры дают ноль. Старшие 32 бита `timeSequence` содержат знаковые секунды; младшее слово хранит миллисекунды в битах 22–31 и sequence в битах 0–21. `eventTime` независимо берётся согласно выбранной политике метаданных FIELD. Копии символа и текста ограничены конечными индивидуальными и общим лимитами единиц UTF-16.

Для записи конфигурации в TICKER с включённым полем `Version` общий collector принимает входящую знаковую физическую версию, если она не меньше версии в кэше; равная версия также может обновить изменённые данные. При отсутствии `Version` допускаются все обновления. В STREAM это сравнение версий не применяется. Подписка на TextMessage требует STREAM.

Источники: закреплённые Java [TextMessage](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/TextMessage.java), [TextConfiguration](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/TextConfiguration.java) и [misc factory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/misc/MiscFactoryImpl.java).
