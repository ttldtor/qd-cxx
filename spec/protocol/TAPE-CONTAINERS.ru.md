# Входные контейнеры закрытых binary tapes

[English version](TAPE-CONTAINERS.md). Английская версия является нормативной.

Спецификация определяет необязательный инструментальный слой `CompressedInput`. Его логические
байты подаются существующему chunked reader; [wire-семантика tape](TAPE-READER.ru.md), целиком
владеющий admission и replay не изменяются. Новых transport, callbacks, threads, извлечения файлов
и рекурсивной распаковки нет. Выбор формата и владение физическим файлом принадлежат инструменту.
Заимствующий API инспектора без сжатия сохраняет прежний контракт.

Слой NONE и API доступны при сборке инспектора или его тестов, в том числе с
`ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION=OFF`. `containerAvailable` сообщает поддержку в сборке
без выделения памяти и I/O: NONE доступен всегда, GZIP/ZIP требуют эту опцию, некорректные значения
перечисления недоступны. Известный недоступный формат даёт `UNSUPPORTED` при первом чтении до
выделения памяти или обращения к источнику; конструктор остаётся ленивым. Владелец может проверить
доступность до копирования путей и открытия файлов. Без потребителя в инструменте или тестах target
не создаётся. Получение zlib и язык C включаются только для реального потребителя сжатия;
core остаётся независимым.

## Владение и завершение

`CompressedInput` заимствует binary `std::istream` до своего уничтожения и владеет только конечными
decoder, input scratch и ZIP metadata. Конструктор не выполняет input I/O. `read` заимствует
непустой writable span на время вызова и возвращает выданный префикс, признак физического завершения
и типизированную ошибку. Память вызывающего кода и callbacks не сохраняются. `finish` не выполняет
I/O и возвращает `NOT_FINISHED` до физического завершения. Destructor освобождает память без
чтения, дочитывания или закрытия источника. Владелец файла уничтожает decoder перед close.

Поздняя CRC/trailer/directory/source ошибка может сопровождаться непустым выданным префиксом.
Вызывающий код подаёт этот префикс через прежний feed/resubmit и затем обрабатывает ошибку.
Префикс не подтверждает успешное завершение. Ошибка терминальна; новые вызовы не читают и не
запускают inflater. Пустой read span даёт `INVALID_ARGUMENT` до работы, включая состояние после
завершения, и сохраняет ранее возникшую ошибку. Повторное чтение с непустым span после завершения
возвращает ноль байтов и признак завершения без I/O.

Физическое завершение контейнера требует integrity validation и фактического EOF. Итоговый COMPLETE
инспектора дополнительно требует parser finish, завершения выбранных companion/split источников,
успешных owner close и output flush. Отмена, отказ, предел ресурсов или ошибка слоя не подтверждают
COMPLETE. При отмене запрещено дочитывать источник ради проверки CRC.

## Профиль GZIP

Все concatenated members, включая пустые, образуют один поток логических байтов. Нужен минимум
один member. Проверяются каждый header, reserved flags, optional header checksum, deflate stream,
CRC32 и ISIZE. Физический EOF допустим только между завершёнными members. Посторонние хвостовые
байты, частичный следующий header и усечённый trailer дают отказ. Результат не зависит от разбивки
физического чтения при достаточных operational work/call limits.

Используются gzip wrapper zlib и ограниченная подача header. `Z_BLOCK` отделяет конец deflate block
от обработки wrapper trailer: ratio не учитывает headers/trailers как deflate input. Header names,
comments и extras не сохраняются, но расходуют metadata/work limits.

## Профиль ZIP

Поддерживается single-disk ZIP32 с ровно одним файлом и ограниченным числом пустых каталогов до
или после него. Методы: stored (0) и deflate (8). Для deflate поддерживается обычный signed ZIP32
data descriptor production writer. Stored требует известных local sizes; неизвестные stored sizes
и stored descriptors не поддерживаются. Распакованный payload каталога должен быть пустым.

Проверяются local headers, конец deflate, CRC, фактические размеры, signed descriptors, central
entries, соответствующие names/methods/flags/local offsets, EOCD counts/size/offset/comment и
физический EOF. Порядок central entries должен совпадать с local entries. Names — непрозрачные
байты: они не преобразуются и не используются для файловых операций. Проверяется length framing
extra fields; ZIP64 extra не поддерживается. Поиск сигнатуры descriptor внутри payload запрещён.

Отвергаются ноль файлов, несколько файлов, encryption, неподдержанные flags/methods, ZIP64,
multidisk/split, self-extracting prefixes, переставленные central entries, unsigned descriptors
и посторонний хвост. Нельзя игнорировать второй файл и объявлять первый успешно закрытым архивом.
Неподдержанные варианты и ошибки структуры отделены от resource refusals.

## Конечные пределы и координаты

Настройки явные и принадлежат экземпляру. Ноль означает ноль, не unlimited/default. Некорректная
конфигурация и представимость проверяются до state allocation/input work. Каждая dynamic allocation
сначала проверяет остаток owned capacity; variable metadata length берётся из bounded header до
рассмотрения allocation. Scratch положителен и
представим как `uInt` и `streamsize`. Physical/inflated allowances оставляют представимое место для
диагностического excess byte. Сложения счётчиков и произведения allocation проверяются. Charge
не возвращается после ошибки, пропуска или замены.

| Предел | По умолчанию | Учёт |
| --- | --- | --- |
| Physical bytes | 64 MiB | Каждый наблюдаемый байт, включая ограниченное чтение вперёд |
| Inflated bytes | 64 MiB | Каждый логический байт до публикации QTP |
| Expansion ratio/slack | 1000 / 64 KiB | Deflate input и payload на member |
| GZIP members / ZIP entries | 1024 / 64 | Включая пустые members/каталоги |
| Одна metadata record / весь просмотр metadata | 256 KiB / 4 MiB | Headers/names/extras/comments/descriptors/directory |
| Work bytes / inflater calls | 256 MiB / 262144 | Physical + inflated + просмотр/перемещение metadata; каждый inflate call |
| Сохраняемые metadata/scratch | 4 MiB | Input scratch, ZIP entry slots и names |
| Allocation zlib | 1 MiB | Одновременно выделенная память с allocator headers |
| Input chunk | 4096 байтов | Конечное чтение вперёд; короткие reads собираются в этот chunk |

Metadata scan резервирует требуемый byte charge заранее и не возвращает его после усечения.
Entry storage растёт лишь при наблюдении entries; большой entry-count allowance не приводит
к предварительному выделению максимума. При росте старые и новые slots одновременно укладываются
в retained allowance, а перемещение добавляет необратимый work charge. Names владеют точными
byte arrays без неучтённой string capacity/terminator. Это логические owned-capacity bounds,
отделённые от фиксированного decoder state, allocator bookkeeping и process RSS.

На точном physical allowance допустима одна одно-байтовая EOF/excess probe. EOF не расходует байт;
лишний байт учитывается диагностически и даёт `COMPRESSED_LIMIT` без дальнейшей подачи. На точном
inflated allowance trailer может завершиться без новых output bytes; максимум один reserved probe
output byte обнаруживает excess и отбрасывается с `INFLATED_LIMIT`. Ненулевой исчерпанный byte-work
allowance аналогично допускает zero-byte EOF/trailer progress; дополнительная byte work даёт отказ
до публикации. Настроенный нулевой work allowance запрещает read/inflater work. Каждый фактический
inflate call по-прежнему требует остатка call allowance.

Byte/member/integrity результат не зависит от коротких физических reads при достаточных
operational work/call limits. Их исчерпание даёт `WORK_LIMIT`, не ошибку формата и не COMPLETE.
Пределы ограничивают принятую работу и память, но не обещают CPU deadline. Ошибка physical I/O
после буферизованного input выдаётся после обработки этого конечного префикса. После обнаружения
ошибки новых physical reads нет.

Разделять `compressedPhysicalRead` с read-ahead, `compressedConsumed`, logical produced/QTP accepted
и member/entry/source ordinal. Точной обратной compressed coordinate для произвольного frame нет.
Companion timestamp positions относятся к inflated QTP bytes, не physical compressed offsets.
Каждый data/companion envelope имеет собственное завершение и конечные пределы.

## Граница зависимости

Необязательный backend — [zlib 1.3.2](https://zlib.net/) под
[лицензией zlib](https://zlib.net/zlib_license.html). Архив:
`https://zlib.net/zlib-1.3.2.tar.gz`, SHA-256
`bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`.
Copyright/license notice сохраняется в source distribution. Используются только in-memory
inflate/checksum API; ZIP envelope validation реализован инструментом, а не самой zlib.

`ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION` по умолчанию OFF. Получение зависимости и включение C
compiler допустимы только при ON и запрошенном compression consumer/test target. OFF не выполняет
fetch, source lookup или поиск system zlib. Core/encoding/protocol/events не получают compression
include, link dependency или macro. Ambient DLL discovery не поддерживается.

Форматы: [RFC 1952](https://www.rfc-editor.org/info/rfc1952/),
[спецификация PKWARE ZIP](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).

## Ограничения production-совместимости

Compatibility references остаются Java QD 3.355
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f` и отдельная 3.356
`f71e4cb390d1586f2d4e3a83d9b9067e3326ef19`. Фактические FileWriter/FileReader показывают равенство
payload NONE/GZIP/ZIP. ZIP wrapper Java выбирает первый файл после каталогов; reader принимает
отсутствующий central directory и хвостовые байты. GZIP continuation может зависеть от read-ahead
JDK 8. FileReader перехватывает поздние container failures и пишет лог: возврат readInto и ноль
parser errors недостаточны для подтверждения завершения.

Выбранный закрытый профиль C++ намеренно проверяет весь конечный envelope и имеет перечисленные
более строгие ограничения. Он не обещает принимать любой Java-архив или live sources.
[Container fixtures](../../tests/fixtures/protocol/tape-containers/README.ru.md) отделяют production
bytes и выполненные проверки от будущей интеграции.
