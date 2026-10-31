# Офлайновый эталон подписок на производственных компонентах

[English version](README.md). Английская версия нормативная.

Запускает компоновщики, парсеры, коллекторы и адаптеры соединения закреплённой Java QD
на ограниченных синтетических входах. Сокеты не открывает; нативный клиент подписок не реализует.
Доказательства и границы приведены в [профиле захватов](../../../tests/fixtures/subscriptions/CAPTURES.ru.md).

Нужны CMake 3.25+, PowerShell 7.4+, явно указанный JDK 8 и чистый checkout по
[upstream.lock](../../../upstream.lock). Классы и доверенную квитанцию подготовьте через
[подготовку схемы](../../schema-export/README.ru.md). Подготовка может скачивать зависимости Maven;
сам этот запуск ничего не скачивает и не собирает upstream. Квитанция сверяет байты классов
с доверенной подготовкой, но независимо не доказывает их происхождение. Непустые
JAVA_TOOL_OPTIONS, _JAVA_OPTIONS и JDK_JAVA_OPTIONS запрещены. Глобальные настройки не меняются.

## Запуск

Из корня репозитория в PowerShell; замените иллюстративные пути к установленным инструментам и checkout:

```powershell
cmake -DHELP=ON -P tools/upstream/subscription-oracle/Capture.cmake
$repo = (Get-Location).Path.Replace('\', '/')
$jdk8 = 'C:/tools/jdk8'
$upstream = 'C:/work/QD-locked'
$receipt = "$repo/.work/schema-export/prepare/classpath-receipt.json"
cmake "-DJAVA_HOME=$jdk8" "-DUPSTREAM=$upstream" "-DRECEIPT=$receipt" `
  "-DRUN=$repo/.work/subscription-check" -DMODE=Verify `
  -P tools/upstream/subscription-oracle/Capture.cmake
```

Без аргументов и с отдельным `-DHELP=ON` выводится одинаковая справка без провайдеров и работы.
Для справки скрипта используйте эту настройку CMake: глобальный `--help` обрабатывает сам CMake.
Для работы обязательны JAVA_HOME, UPSTREAM, RECEIPT и RUN. RUN не должен существовать.
MODE по умолчанию Capture (только структурная проверка); Verify дополнительно сравнивает с EXPECTED,
по умолчанию `tests/fixtures/subscriptions/expected`. Эталоны никогда не публикуются и не меняются.
Код 0 означает справку/успех; 1 — ошибку аргументов, подготовки, компиляции, наблюдения или сравнения.

Запуск проверяет исходники и провайдеры до и после, компилирует четыре локальных Java-файла
в RUN/classes и выполняет каждый захват в отдельной JVM с кучей 256 МиБ и таймаутом 60 секунд.
Дополнительные JVM проверки справки/ошибок ограничены 15 секундами. Локаль en/US, часовой пояс UTC
и UTF-8 для захвата/справки применяются к дочерним процессам.
В RUN остаются вывод команд, квитанции и границы фаз UTC. Зависимые от часов кадры
перемещаются без изменения в RUN/session-raw; только RUN/artifacts детерминирован и пригоден
для публикации. Ошибочные запуски сохраняют частичные доказательства; повтор требует нового RUN.

## Прямые инструменты и результаты

Скомпилированным main нужен подготовленный classpath. WireOracle принимает `--capture NEW_DIRECTORY`,
CollectorOracle — `--output NEW_DIRECTORY`, SessionOracle — `--out NEW_DIRECTORY`.
Все поддерживают запуск без аргументов и отдельный `--help` без работы провайдеров.
Wire возвращает 3 при ошибке синтаксиса, 2 при ошибке наблюдения/ввода-вывода (включая справку);
Collector — 3 при ошибке синтаксиса/существующем выходе и 1 при ошибке наблюдения/ввода-вывода;
Session — 3 при ошибке синтаксиса и 1 при ошибке наблюдения/ввода-вывода/существующем выходе.
Существующие выходы не перезаписываются. Session меняет серверный таймаут AuthManager лишь внутри своей JVM.

```powershell
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 `
  -Actual .work/subscription-check/artifacts -Expected tests/fixtures/subscriptions/expected
Get-Help ./tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Full
```

Сравнение только читает, проверяет канонический типизированный TSV и точные байты артефактов,
включая допустимые пустые бинарные дельты. Без Expected проверяется лишь структура.
Ограничения и коды завершения даны во встроенной справке. Байты не нормализуются.
Внутренний tests/HelpFailureProbe подставляет неисправный stdout для проверки вывода справки;
производственную модель QD он не заменяет.

Обычные нативные сборки и CTest не запускают этот необязательный инструмент и не требуют Java.

## Регрессионные проверки сравнения

`pwsh -NoProfile -File tools/upstream/subscription-oracle/tests/Test-Comparison.ps1 -Help`
выводит отдельную справку. Для позитивных/негативных проверок задайте
`-Artifacts RUN/artifacts -RunDirectory NEW_TEST_DIRECTORY`; родитель нового каталога должен существовать.
Сохраняются приватные изменённые копии, результаты и логи; Artifacts не меняется.
Каждый дочерний процесс ограничен 30 секундами. Код 0 — справка/успех, 2 — ошибка проверки/ввода-вывода,
3 — неправильные сочетания аргументов; parameter-binding ошибки PowerShell могут вернуть 1.
Недоступность создания reparse point на хосте фиксируется явным пропуском соответствующего теста.
Без аргументов также выводится справка без создания результатов.
