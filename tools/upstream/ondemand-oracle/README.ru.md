# Синтетические фикстуры OnDemand

[English](README.md) · [Инструменты разработчика](../../dev/README.ru.md)

OnDemandFixtureOracle использует штатные классы `ReplayRequest`,
`ReplayResponse`, `Block`, `QuoteMapping` и `MDRQuote` из ревизии Java QD,
закреплённой в [upstream.lock](../../../upstream.lock). Соберите её вместе
с модулем `dxfeed-ondemand` и сохраните исходные классы и ресурсы сервисов.
Генератор работает с Java 8 и новее.

Команды выполняются из корня qd-cxx в PowerShell. Укажите в `$qd` путь
к проверенному репозиторию QD и при необходимости добавьте зависимости
его сборки в `$qdEntries`. Каталог компиляции должен быть доступен для
записи. Генератор печатает TSV с именем строки и байтами в нижнем регистре
hex. Генератор не открывает сокеты и не записывает файлы.

```powershell
$qd = 'C:/src/QD'
$pin = (Select-String -Path upstream.lock -Pattern '^commit').Line.Split('"')[1]
if ((git -C $qd rev-parse HEAD) -ne $pin) { throw 'Неверная ревизия QD' }
$qdEntries = @(Get-ChildItem -Path "$qd/*/target/classes" -Directory |
    ForEach-Object FullName)
$cp = $qdEntries -join [IO.Path]::PathSeparator
$out = '.work/ondemand-fixture-generator'
New-Item -ItemType Directory -Force $out | Out-Null
javac -encoding UTF-8 -cp $cp -d $out tools/upstream/ondemand-oracle/OnDemandFixtureOracle.java
if ($LASTEXITCODE -ne 0) { throw 'Ошибка компиляции генератора' }
java -cp $out OnDemandFixtureOracle --help
java -cp $out OnDemandFixtureOracle --help-ru
java -cp ($out + [IO.Path]::PathSeparator + $cp) OnDemandFixtureOracle --generate
```

Без аргументов, с `--help` и `--help-ru` достаточно скомпилированного
генератора. Код завершения 0 означает успех, 1 — сбой генерации,
2 — неверные аргументы. Перед обновлением
[фикстуры C++](../../../tests/fixtures/ondemand/fixture.hpp) сверьте SHA-256
декодированных байтов с [таблицей](README.md).

Синтетический запрос выбирает композитный Quote для `MOCK` с
2024-02-12T14:30:05Z. Ответ покрывает период от пяти секунд до точки старта
до 60 секунд после неё. Первая котировка относится к точке старта, имеет
bid 12.25, ask 12.50, размеры 10 и 20 и код биржи Q. Вторая приходит
на указанном смещении с bid 13.25 и ask 13.50.

## Локальная проверка жизненного цикла

Группа CTest `on_demand_feed` использует собственный IPv4 loopback HTTP
сервис, а для возврата в live — собственный обычный QTP сервис. Проверяются
точные байты запроса, типизированные котировки, отказ доступа и ошибки
кадров, покрытие, конечный EOF, скорость и пауза, очистка кэша и возврат
в live с повторным подключением и новой эпохой. Отдельно проверяются
закрытие во время HTTP, отмена ожидающего исторического обработчика и
тайм-аут с выполняющимся обработчиком или до открытия источника.

```powershell
ctest --test-dir <feed-build> -R '^on_demand_feed$' --no-tests=error --output-on-failure
```

Проверенный сервис реализует конечный синтетический профиль через loopback.
Доступ к внешнему OnDemand, права на данные, зашифрованный HTTP и
фактическое покрытие поставщика проверяются отдельно.
