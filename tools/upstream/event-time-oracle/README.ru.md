# Oracle времени событий и регионального Quote

[English version](README.md). Английская версия является основной.

`EventTimeOracle.java` вызывает закреплённые production `DXFeedScheme`, `MarketFactoryImpl`,
отображения Quote/Profile, delegate, выбор delegate set и методы значений. Их вычисления
не копируются. [Контракт](../../../spec/events/EVENT-TIME.ru.md) задаёт native-подмножество.
[Описание захвата](../../../tests/fixtures/event-times/CAPTURES.ru.md) перечисляет исходные строки и companion.

Нужны существующие собранные production classes точной ревизии QD3.355 из `upstream.lock`,
JDK8, PowerShell7.4+ и существующий `qdcxx-run-utc.exe`. Maven, сеть, загрузка, установка,
сборка SDK и endpoint не запускаются. Java отдельно форматируется закреплённым
google-java-format с JDK21.

```powershell
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/event-time-oracle/Invoke-Oracle.ps1 `
    -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 `
    -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/event-time-verify-1
```

Без аргументов и с `-Help` выводится полная английская справка; `-HelpRu` выводит русскую.
Доступен `Get-Help ./tools/upstream/event-time-oracle/Invoke-Oracle.ps1 -Full`. Справка
игнорирует несуществующие рабочие пути, не создаёт каталоги и Java-процессы. Для работы
обязательны четыре пути. `Mode` по умолчанию равен `Verify`: сохранённые fixture/provenance
сравниваются без замены. `Capture` заменяет только `tests/fixtures/event-times/oracle.tsv`
и дополнительно `representability.tsv` и `provenance.json` кандидатами для ревью.

`RunDirectory` должен быть новым каталогом внутри `.work` текущего checkout; прежние попытки
не перезаписываются. Там сохраняются классы harness, исходные stdout/stderr, полные UTC-логи
команд и exit, production class inventories и receipt. Каждый compiler/JVM ограничен Java heap
128MiB и timeout60 секунд; останавливается только собственное дерево зависшего процесса.
Production classes инвентаризируются до и после. Provenance содержит revision, хеши исходников/
инструментов, class inventory hash, fixture hash, число строк и совпадение часовых поясов
без личных абсолютных путей.

Отдельные JVM наблюдают `default`, `sequence`, `nano`, `both`, `millis`, `millis-default`, `minimal`, `reordered`,
`missing`. Quote, Quote&Q, Quote&z и Profile берутся из настоящих сгенерированных production
records. Minimal/missing/reordered клонируют их поля и сохраняют отдельные стандартные witnesses.
Пять исходных cursor samples каждой принятой записи покрывают отрицательное время, границы
signed32/signed64, необязательные исходные Sequence/nano, независимые timestamp-sequence metadata
и строки Profile null/empty/nonempty. Сохранены ошибки factory при отсутствии обязательных полей.
Дополнительные вызовы настоящих delegate/delegate set покрывают существующие региональные суффиксы,
завершающие атрибуты, точный выбор регионального маршрута и composite fallback.

Корпус содержит2152 TSV-строки с LF. Исходные наблюдения сохраняют точные байты; JSON provenance
имеет UTF-8 без BOM с LF. Результаты UTC и Pacific/Honolulu должны побайтно совпадать. Native
conformance владеющего/заимствованного пути: `ctest -R '^event_time_java$' --output-on-failure`.
Обычные C++-сборки используют сохранённые fixtures без Java. Loopback-проверки установленного
Feed отдельно доказывают реальную доставку `Quote&Q`/base symbol; oracle не доказывает
live-приём публичного сервиса.

Exit0 означает справку либо полный успешный capture/verify. Exit1 означает ошибку аргументов,
reference/JDK, недоступный вход, внешний/повторный run directory, ошибку/timeout процесса,
изменение production classes, расхождение часовых поясов, fixture/provenance или ошибку вывода.
Завершённые логи остаются при ошибке; неполный запуск не имеет PASS receipt.

Отдельный четырёхстрочный `representability.tsv` вызывает production Quote&Q delegate
в обоих направлениях. Сохранены два необратимых написания и два представимых варианта
с атрибутами без изменения исходного корпуса2152 строк времени. Выбранные native-региональные
маршруты явно отказывают непредставимым selectors до публикации изменения. Длинный атрибутный
символ также проверяет одно выделение памяти вместо growing-prefix transient. Это native-пределы
и политика подмножества.
