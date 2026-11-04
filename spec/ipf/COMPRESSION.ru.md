# Сжатые источники IPF

[English](COMPRESSION.md). Английская версия нормативная.

URL reader применяет ровно один инкрементальный контейнер перед существующим UTF-8/IPF parser.
Без Content-Encoding или при identity формат NONE/GZIP/ZIP выбирается по magic, независимо от
расширения. Content-Encoding:gzip выбирает тот же единственный gzip слой: результат сразу идёт
в IPF, повторного magic-декодирования нет. Повторные, составные и прочие HTTP codings отклоняются.
Запрос по-прежнему отправляет Accept-Encoding:identity.

GZIP проверяет заголовок, deflate payload, CRC32 и размер trailer; поддерживает optional headers
и concatenated members. После корректного member посторонний хвост и неполный/неверный заголовок
следующего member игнорируются по контракту JDK8. После полного следующего заголовка усечение
или ошибка payload/trailer уже завершают чтение ошибкой.

ZIP пропускает каталоги вместе с их payload и читает первый файл.
STORED требует известных local/ZIP64 размеров; data descriptors поддерживаются только для DEFLATE. Поддерживаются stored/deflate,
локальные ZIP64 size extras и descriptors. CRC и фактические размеры проверяются у пропущенных
каталогов и выбранного файла. Имена — ограниченные UTF-8 metadata, а не пути: извлечения файлов нет.
Шифрование, неподдерживаемый method и отсутствие файла дают ошибку. Последующие entries, central
directory и хвост не интерпретируются; успех не означает проверку всего архива.

Descriptor может иметь или не иметь signature. ZIP64 extra выбирает восьмибайтовые размеры даже
для малого payload; без него используются ZIP32 поля. Unsigned descriptor не требует лишнего
lookahead. Корректные формы, отклоняемые дефектами JDK8, намеренно поддержаны; точные различия
фиксирует [ADR-0007](../../docs/decisions/ADR-0007-ipf-container-interoperability.ru.md). ZIP64 metadata
вместе с32-битным descriptor несогласованы и дают явную ошибку.

IpfTransferLimits::envelope независимо ограничивает вход, распакованный выход, calls, byte-work,
память inflater, ZIP metadata и members/entries. Выход включает отброшенный payload каталогов.
Work списывается до операции за предложенные окна и обнуление памяти, включая ошибки; новый member
или redirect не возвращает кредиты. Фиксированный decoder и весь allowance inflater входят в
resident preflight до allocation. zlib использует ограниченный выровненный allocator операции.
За turn подаётся/производится не более4096 байт; полного буфера ответа/распакованного тела нет.
Отмена проверяется между конечными turns на creator thread SDK, без скрытых workers.

Целостность контейнера, HTTP framing, native/TLS retirement, IPF EOF и IPF COMPLETE учитываются
раздельно. Ошибка trailer сохраняет уже принятый префикс целых профилей и наблюдение COMPLETE,
но не публикует успех. Failure drain читает только собственный decoded cache без source/TLS/native
write/provider работы. Первая причина не заменяется последующими ошибками.

Необязательный IPF transfer target приватно использует прежний точный zlib1.3.2 pin и проверенную
лицензию; установленная статическая поставка включает архив и лицензию. Pure IPF/feed при transfer
OFF не получает и не ищет zlib. Tape tooling не является зависимостью IPF и сохраняет свою политику.

Первичные источники и точные детали совместимости приведены в английской версии. Oracle fixtures
получены через фактические pinned QD production classes на JDK8. Вход сырого InstrumentProfileParser
остаётся UTF-8 IPF.
