# Покрытие настраиваемых событий и просмотра Profile

[English version](COVERAGE.md). Английская версия является нормативной.

QDCXX-025 реализует только точное подмножество ordinary records из
[EVENT-MAPPING](../../../../spec/events/EVENT-MAPPING.ru.md), [QUOTE](../../../../spec/events/QUOTE.ru.md)
и [PROFILE](../../../../spec/events/PROFILE.ru.md). [Java-покрытие](../../../fixtures/events/mapping/COVERAGE.ru.md)
разделяет production delegate/file наблюдения, прямые граничные пробы и доказательства разрешения источников.
[проверка](VALIDATION.ru.md) записывает выполненные прогоны; независимое ревью ожидается.

| Требование | Исполняемое покрытие |
| --- | --- |
| R1–R3 явные схемы, ключи и типы | `event_mapping_contract`: aliases, перестановки полей, ненулевой local ID, отсутствующие/неоднозначные/несовместимые запросы, optional absence и повреждённые keys/types. `configured_java_events` строит полные выбранные записи по захваченным local/property keys и точным identities. |
| R2/R5 конфигурация и XML | 21 Java-сценарий в новых JVM; 18 принимаемых C++ вариантов, включая неизменённый полный default и выбранные dxlink Quote/Profile, extension, конфликты источников, optional-комбинации и numeric-int. Rejected, full24 и full137 остаются явными случаями; полный dxlink не заявлен. |
| R4 контекст и владение | Независимые схемы одинаковой формы не обмениваются отображениями; копии сохраняют идентичность; уничтожение/перемещение источников, публикация STREAM/RAW и перестроение снимков TapeReader сохраняют правильные контексты. Контекст не владеет объявлениями. |
| R6 семантика событий | Прямые Java cursor/event пары и 108 исходных/omitted tape-файлов, каждый подан целиком и побайтно. Сравниваются все raw values, точные UTF-16 symbols/strings, доставленные свойства и timing. Границы Sequence, optional defaults, integer/decimal defaults, отрицательные времена и business flags различаются. |
| R7 совместимость | Все 25 прежних CTest, 41 исторический Quote-файл, прежнее отображение и профили; 18 отдельно захваченных process outputs покрывают три профиля, два формата, оба режима и характерные ошибки. Изменения help описывают новые параметры; проверяется эквивалентность no-argument/help. |
| R8 generic records | Тесты общего helper и декодера сохраняют extra fields и custom records; проекции не изменяют generic values. Одного имени события недостаточно для совместимости; несовместимые пары CLI schema/display отклоняются до IO. |
| R9 бюджеты и ошибки | Нулевые/точные/превышенные mapping counts/name/comparison бюджеты и бюджеты string/symbol/aggregate одного события. Profile IO проверяет накопленные копии, атомарность пары относительно output, input bytes, field units, records/fields, усечение, read/write failure и разные ошибки первого/финального flush. |
| R10 строгие доказательства | Java comparator отклоняет 14 мутаций; configured C++ comparator — 13; Profile CLI comparator — 84 и сравнивает шесть полных выводов независимо по Java cells и исходным границам frames. |

Шесть новых CTest: `event_mapping_contract`, `profile_mapping_contract`, `configured_java_events`,
`configured_event_fixture_negatives`, `profile_inspection_io` и `profile_inspection_cli`.
Проверки Prepared Quote расширяют `quote_java_tape`. CLI comparator не вызывает event mappings,
TapeReader или Formatter; чтение примитивных envelopes и независимо перечисленные правила decimal/quoting
строят полные ожидания. Изменяется каждая event cell и raw field; отклоняются unknown, trailing,
duplicate и missing rows, null/empty, flags, time, decimals и небезопасные UTF-16.
Оба формата также покрывают default Profile, присутствующий локально, но пропущенный в wire.

Конечные тесты не исчерпывают расписания allocation failure или все Unicode/numeric values. Не добавлены
общий source loader, полная совместимость настроек, regional/indexed события, новые full types, wire coercion,
сжатие, ненулевые QTP flags, replay scheduler или transport. Частный tape и live endpoint не используются.
Сохранение нескольких библиотечных событий требует накопленных бюджетов вызывающей стороны. Самопроверка
и исходные проверки не закрывают независимое ревью.
