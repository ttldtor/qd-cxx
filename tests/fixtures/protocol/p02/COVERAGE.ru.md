# Исполненные доказательства framing P02

[English version](COVERAGE.md). Английская версия является нормативной.

QDCXX-012 добавляет P02 отдельно от исторических P01/P03/P04/P05. Закреплённые исходники Java QD
3.355 собираются JDK 8; [FramingOracle.java](../../../../tools/upstream/protocol-oracle/FramingOracle.java)
расширяет существующий source-built driver. [Точные наблюдения](../expected/P02.tsv) содержат 17
подслучаев. [Валидация](VALIDATION.ru.md) фиксирует воспроизводимость и неизменяющий Verify.

| Подслучай | Воздействие и наблюдение |
| --- | --- |
| empty | `00`: нулевой body, позиция продвигается на один, heartbeat callback отсутствует. |
| explicit-zero | `0100`: явный type 0, пустой payload, продвижение на два, callback отсутствует. |
| tiny-heartbeat | `020000`: type 0/content bits 0, один heartbeat callback. |
| concatenated | Три предыдущих кадра вместе; границы различаются, один callback. |
| negative-length | `7f` и tiny heartbeat; stream error, диагностика потребляет доступный хвост. |
| limit-minus / limit-at / limit-plus | Body 7/8/9 при maxMessageSize=8; type 3 и нулевые непрозрачные байты, затем tiny heartbeat. Первые два принимаются; третий даёт stream error до обработки payload. |
| partial-length / partial-body | `80` / `0300`: позиции возвращаются на 0, соответственно 1/2 pending-байта, ошибок нет. |
| truncated-type | `0180` и tiny heartbeat: stream error, байты следующего кадра не заимствуются; возврат consumer позволяет следующий callback. |
| negative-type / large-type | Type -1 / 2147483648 и tiny heartbeat: stream error и следующий callback. |
| overlong | Двухбайтовые length=2 и type=0, затем tiny heartbeat; принимаются, следующий callback. |
| unknown | Type 62 и tiny heartbeat: уведомление unknown, затем heartbeat callback. |
| bad-payload | Type 0 с content bits 1 без значения, затем tiny heartbeat: message corruption(type=0), затем callback. Кадры структурно допустимы. |
| good-before-error | Tiny heartbeat, кадр truncated-type, tiny heartbeat: callback, stream error, callback. |

Каждый вход подаётся целиком, при каждом непустом разбиении на две части и побайтно с пустыми
вызовами до/между/после байтов. Точные байты и расписания находятся в p02.input/p02.replay;
p02.feed содержит размер части и суммарное число поданных байтов. p02.state фиксирует totalPosition,
available, число callbacks и ошибок. Сохранённый суффикс равен input[totalPosition:supplied].
Явный записывающий consumer возвращается из всех уведомлений об ошибках и не закрывает соединение.

Защищённые наблюдательные hooks фиксируют позиции перед длиной, после type и после успешного payload.
Они не меняют ввод и не создают callbacks. After-body не всегда является концом кадра: reserved
type 3 игнорирует payload, затем внешний parser переходит к объявленному концу. Before-length и
конечные позиции устанавливают эти границы. Hook-события пустых кадров не являются heartbeat callbacks.

Диагностика ошибки заголовка вызывает appendBytes, читающий дополнительные доступные байты. В этом
малом профиле после неверной длины потребляется весь цельный ввод; при фрагментации последующие
байты могут разбираться. Этот измеренный побочный эффект объясняет зависимость восстановления Java
от расписания. Ошибки type/body ограничены body, затем parser переходит к его концу. Терминальные
[проекции C++](../../../conformance/protocol/frames.tsv) намеренно отличаются от восстановления Java.

Одно минимальное сравнение adapter запускает настоящий подкласс стандартного MessageAdapter и
передаёт type 62 через BinaryQTPParser: alive/open меняется на not-alive/closed после unknown.
Socket и connection в этом сравнении не используются. Полная матрица флагов политики отложена.
Semantic-файл P02 не содержит record-строк и не является источником ожиданий кадров.

Final-EOF, огромные входы/выделения, реализация payload, сборка PART, transport, timing, другие
подклассы буфера и полная матрица error policy не заявляются. Прежние входы, captures и ожидания
P01/P03/P04/P05 неизменны; старые утверждения о невыполненном P02 являются историческими.
