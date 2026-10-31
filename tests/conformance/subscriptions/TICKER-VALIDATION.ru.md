# Квалификация TICKER-коллектора

[English version](TICKER-VALIDATION.md). Английская версия нормативная.

[Контракт коллектора](../../../spec/subscriptions/TICKER-COLLECTOR.ru.md) задаёт
границу чистого latest-кеша и локальных представлений. Проверки используют
контролируемый офлайновый ввод и зафиксированную production Java QD 3.355.
Они не доказывают полный сетевой клиент подписок, upstream multiplexing
или произвольное отображение бизнес-данных.

## Нативные профили

| Профиль | Результат |
| --- | --- |
| Windows x64, MSVC 19.42, Release; client ON, runtime/JSON OFF | Полный CTest 43/43; focused TICKER 30 случаев / 6808 assertions. |
| Linux x64, GCC 13.3, Debug, ASan/UBSan; client ON, runtime/JSON OFF | Полный CTest 43/43; focused TICKER 30/6808. `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer исключён. |
| Изолированный Linux ThreadSanitizer | Один случай / 32 assertions; четыре потока по 10000 итераций с независимыми токенами после завершения collector/protocol owners, включая последнее освобождение маркера рабочим потоком. `TSAN_OPTIONS=halt_on_error=1`. |
| Production-only install и relocated consumer, обе системы | `BUILD_TESTING=OFF`, client/runtime/JSON OFF; настоящий generic TICKER consumer успешно работает после переноса. Все 34 файла каждой системы сохраняют хеши при переносе пакета с layout `lib64` / `public include`. В чистой production-конфигурации нет Threads/SDK discovery. |
| Закреплённое C++ форматирование | Uncrustify 0.83.0, затем clang-format 20.1.8, Check и побайтовое сравнение fixed point для всех трёх изменённых публичных C++ файлов. |
| Doxygen 1.18 | Ноль предупреждений; сгенерированные страницы коллектора, токена, выдачи и reset notice проверены на параметры, результаты, владение, инвалидирование, потоки и ошибки. |

Между потоками теста маркера передаются только отдельные объекты фиксированных
токенов. Коллектор, исходные события, snapshots и intrusive protocol identities
остаются сериализованными. Конкурентные операции коллектора и освобождение
protocol lease другим потоком не квалифицированы. Байт-лимиты означают requested
backing, а не RSS процесса или гарантию задержки.

## Поведенческие границы

Именованные нативные случаи покрывают:

- Оба режима хранения; ввод до add, latest/equal данные, поздние представления,
  pending conflation, remove/re-add с другим держателем и вытеснение последнего интереса.
- Частичную initial/live выдачу, измеренную границу default quantum 100,
  малый настраиваемый quantum и отказ от подготовленного предложения.
- Исключение целого ошибочного кадра, отказы schema/epoch, исходную binding provenance
  и точные nullable text/opaque значения после завершения драйвера.
- Полные fixture bodies стандартных Quote с восемью полями и Profile с шестнадцатью
  через generic-хранилище и существующие prepared mappings. Пустое и null различаются.
- Пики old-plus-candidate bytes/frames, точную и на единицу меньшую ёмкость,
  нулевые лимиты, проверенную арифметику, admission/front/pop с фиксированным
  backing и независимые представления.
- Резервные shared stale/reset evidence, точное подтверждение владельца,
  чужие/старые/повторно использованные токены и перенос владения.
- Work-отказ до инициализации массивов, общего обхода представлений, повторных
  desired/key/text сканирований и публикации prepared offer. Отказ сохраняет
  прежнее предложение; последующий pop подтверждает одну запись.

Независимые пробы замороженных исходников также проходят два случая / 140 assertions
для недостаточного pop-кредита и повторных abandoned/zero-work front операций.
Они дополняют полные нативные наборы.

## Реальное сравнение с Java

[Профиль захватов](../../fixtures/subscriptions/CAPTURES.ru.md) содержит 83 случая /
8255 наблюдений. Семь новых TICKER-сценариев сохраняют все прежние случаи;
wire/session artifacts побайтово неизменны. Настоящие Capture и свежий Verify
исполняются на Windows JDK 8; Linux JVM этим не заявляется.

Нативное сравнение связывает 2162 точные координаты девяти именованных случаев
и 58 фаз выдачи. Сравниваются фактические post-`LiveSession` входные кортежи,
локальные команды, canonical UTF-16 record/symbol identities, raw time/value/flags,
выданные записи, количества и применимые availability/capacity/more/cumulative
показатели. После явной проекции наборы координат должны совпасть полностью;
сравнения только их пересечения недостаточно.

Полное выбранное поведение сравнивается для двух прежних
`ticker-latest-retention-*` и пяти новых случаев:

- `ticker-equal-value-retention-false` и `ticker-equal-value-retention-true`;
- `ticker-partial-initial-live-cutover`;
- `ticker-pending-readd-with-keeper`;
- `ticker-initial-update-quantum-boundary`.

Для двух `ticker-raw-remove-retention-*` сравниваются представимые фазы.
Их direct-distributor `data-input-batch-1` передаёт ненулевое обычное значение
с REMOVE; wire REMOVE задаёт обычным полям значения по умолчанию. Этот ввод,
`remove-existing-same-payload` и запрос `same-payload-availability` исключены
из нативного сравнения. Java не наблюдает там изменения состояния, поэтому
последующее выбранное состояние сопоставимо. Отдельные нативные тесты проверяют
raw REMOVE flags, indexed removal time, default fields и нормализованные флаги выдачи.

Cipher/raw-symbol encoding details и `more` при опустошении вспомогательного
RecordBuffer исключены. Canonical symbols и настоящий результат ограниченной
provider-выдачи разделены и сравниваются там, где выведены. Случай на 104 ключах
наблюдает обновлённый Q000 после первых 100 initial-записей перед остатком
Q100…Q103. Малый настраиваемый quantum — нативный тест по исходнику;
это не заявление об исполнении такой Java-конфигурации.

Обычный trace: 52361 байт, SHA-256
`cd34fc4e658cd6b75cc30ae3fa733121ae5d361a520975bdabc40d030e8a548c`.
Quantum trace: 122625 байт, SHA-256
`3858cc5a22561c526496cefb2a331dec1480c9f2ec1a5082d7cad18c6eded03d`.
Оба побайтово совпадают между Windows и Linux.

Отдельный production oracle live-session также проходит по обоим нативным
захватам. Поток controls на 344 байта и callbacks на 4778 байт неизменны;
обновлённое происхождение содержит фактический хеш корневого build-файла.

## Consumer и границы

[Скомпилированный фрагмент драйвера](../../../spec/subscriptions/TICKER-COLLECTOR.ru.md#скомпилированный-фрагмент-драйвера)
совпадает с функцией обоих installed consumers. Он требует готового внешнего
источника и совпадающей схемы; wire-интересы и закрытие I/O остаются драйверу.
Пакет экспортирует `qdcxx_collectors` и TICKER header без runtime или test framework.

Custom Java equality/converters, полная wire-эквивалентность каждого прямого
distributor operand, TICKER callbacks, HISTORY, общий upstream demand, reconnect,
TLS, публичные endpoint и production workload этим не квалифицированы.
