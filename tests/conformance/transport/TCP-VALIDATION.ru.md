# Проверка TCP runtime

[English version](TCP-VALIDATION.md). Английская версия нормативная.

Матрица квалифицирует [контракт TCP](../../../spec/transport/TCP-RUNTIME.ru.md),
[границу ресурсов DNS](../../../spec/transport/DNS-RESOURCES.ru.md)
и [набор исходников SDK](../../../cmake/runtime-profile/README.ru.md).
TLS, коллекторы, прикладная callback delivery и reconnect здесь не квалифицируются.

## Поведение

- Bootstrap проверяет реальную native readiness sample и переданный per-call FALSE
  до SDK ownership. Windows проверяет сохранение чужой SDK reference при отказе
  и отсутствие лишней reference. Всегда успешный публичный getter Linux не выдаётся
  за счётчик ссылок; соответствующий порядок владения подтверждён исходниками.
  Реальный OS entropy failure и качество случайности эти случаи не воспроизводят.
- Отказы работы/арен в фабрике сохраняют исходные schema/composer; при отказе подготовки
  endpoint activation, network I/O и DNS query admission ещё не начались.
  Подготовка SDK context/channel уже могла состояться и должна безопасно завершиться.
- Исходные полные descriptions/data стандартной Quote проходят через побайтовые
  фрагменты в prepared event mapping. Неизменяемые владельцы живут после close.
- Записи по одному байту сохраняют порядок и полного владельца выхода; busy slot
  отклоняет новую компоновку без изменения. Socket byte credits дают yield/resume.
- Pending read, действительно pending connect/write выводятся из работы через
  abort/deadline. Harness сначала продвигает owner, затем проверяет сохранение
  pending operation; один первоначальный WOULD_BLOCK этого не доказывает.
- Windows cancellation сохраняет lower bound сообщённых байтов записи и
  неопределённость принятия. Локальный pending suffix не объявляется точно unsent.
- Read prefix/error, EOF/incomplete framing и abort/completion barriers сохраняют
  первую наблюдаемую non-cancellation cause и конечную distinct secondary cause.
- Действия с другого потока отклоняются без изменения; explicit close допускается
  без выделения памяти, свежие запуски не разделяют состояние операций/callbacks.
- Явные DNS A/AAAA, CNAME/case folding в одном ответе, TCP fallback, малые фрагменты,
  deduplication, unsupported names, unrelated answers, CNAME-only, loops
  и endpoint overflow проверяются контролируемыми локальными peers.
- UDP WOULD_BLOCK возобновляется через собственный registry write hint. Caller
  budget больше 128 не увеличивает отдельный per-entry native-attempt cap.
- Отмена/срок silent DNS выводят канал из работы. Совместный пик DNS/session-output
  сохраняется после close; исторический peak не уменьшается.

Тесты используют собственные конечные peers, threads и sockets с timeout и cleanup.
Публичный endpoint не нужен. Внутренние CTest entries: `transport_tcp` и
`transport_cares_ownership`. Второй использует корректные объекты и переданный
queue-admission error status, а не воспроизводит реальный отказ malloc.

## Конфигурации и границы доказательств

Проверяются окончательные исходники Windows x64 MSVC Release и Linux x86-64 GCC
Debug с ASan/UBSan. В Linux инструментируются адаптер и c-ares; явное исключение
LeakSanitizer для основного набора сохраняется. Отдельный малый ownership-unit LSan
run учитывается отдельно и не меняет область основного набора.

Runtime OFF проверяется без корректных SDK roots: не требуются SDK read/find/fetch/
include/link и дополнительный компилятор зависимости. Проверяются enabled production
builds с `BUILD_TESTING=OFF`, установленный/перенесённый consumer и явная native static
link closure. Неверные source hashes/file sets, отсутствующие/несовместимые SDK
и неквалифицированные архитектуры должны явно отклоняться. Исходники и данные
подготовки сохраняются.

Source review и manifests задают условный вывод SDK storage/work. Исполнение
проверяет соблюдение ограничений адаптером и время жизни операций. Логические
credits не превращаются от этого в измеренную CPU latency, а requested capacities —
в физический RSS. До интеграции frozen source, проверки и независимое ревью должны
относиться к одному артефакту.

## Выполненная матрица

| Профиль или проверка | Результат и область |
| --- | --- |
| Windows x64, MSVC 19.42, Release, runtime ON | CTest на окончательных исходниках: 42/42. |
| Linux x86-64, GCC 13.3, Debug ASan/UBSan, runtime ON | CTest на окончательных исходниках: 42/42; адаптер и c-ares инструментированы, LSan основного набора OFF. |
| Runtime OFF, обе платформы | Нет SDK find/file operations runtime и включения языка C; перенесённые production consumers проходят. Windows platform RC setup остаётся штатным поведением CMake. |
| Runtime ON, обе платформы | Production-only build и configure/link/run перенесённого consumer проходят; 30 installed files, нестандартные `lib64` и `public include`, явный `qdcxx_DIR`. Consumer не нужны SDK headers и компилятор C. |
| Квалификация исходников | Совпали 637 Asio и 532 c-ares hashes. Extra/modified Asio header, missing license и старая ревизия c-ares отклонены. Synthetic ARM64 metadata отклонена; реальная ARM64-сборка этим не квалифицируется. |
| Независимое исполнение | Пять критических Windows regressions проходят, 1 245 assertions; четыре valid-object SDK ownership cases проходят. |
| Существующая совместимость протокола | Native controls Windows/GCC совпали с callbacks настоящего закреплённого Java parser; input 344 bytes и callback artifact 4 778 bytes прежние. Оба native captures проверялись Windows JDK. |
| Стиль и API reference | Девять C++ files проходят pinned format/check chain; Doxygen 1.18.0 без предупреждений, generated API content просмотрен. C ownership unit SDK сохраняет upstream C style. |

Обнаружение Windows только через prefix не нашло нестандартный `lib64`;
явный `qdcxx_DIR` задаёт документированный каталог пакета.
