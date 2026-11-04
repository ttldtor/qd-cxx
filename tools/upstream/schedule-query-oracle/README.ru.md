# Oracle реальных Schedule-запросов

[English version](README.md) является нормативной.

Отдельный `ScheduleFilterOracle` с помощником `FilterObservation` фиксирует
[корпус фильтров](../../../tests/fixtures/schedule-filter/cases.tsv), не изменяя прежний oracle и его
provenance. Команды компиляции и побайтовой проверки приведены в английской версии; нужны готовые
production classes закреплённого QD и JDK8. Без аргументов, `--help` и `--help-ru` запускается
только справка, без входов и вычисления расписания. Вход ограничен 4096 строками и 16 MiB; вывод —
новый UTF-8 LF файл. Код 0 означает успех, ненулевой — ошибку ввода, записи или Java QD. После
ошибки записи может остаться неполный файл. Входы и upstream classes не изменяются; для захвата
повторите UTC/en и Pacific/Honolulu/fr и сравните байты. UTC-примеры не доказывают совместимость
разных выпусков TZDB.

`ScheduleQueryOracle.java` вызывает закреплённые production Schedule/Day/Session, actual Java TimeZone
raw components, все631identity и полный captured SystemV transition profile; отдельного календарного
алгоритма нет. `Invoke-Oracle.ps1` по умолчанию Verify; Capture пишет новые actual fixture/provenance
только после совпадения UTC/en и Honolulu/fr. Upstream sources/classes доступны только для чтения;
class inventories сравниваются до/после. Проверяются точные JDK8 executable/TZDB hashes из schedule
manifest. JDK форматтера — отдельный инструмент.

```powershell
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -NoProfile -File tools/upstream/schedule-query-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/schedule-query-verify-new
```

Нужен существующий checkout полного revision из `upstream.lock` с собранными dxfeed-api/dxlib/qd-core
и точным production JDK8 из `resources/schedule/manifest.json`. Все пути явные. RunDirectory — новый
каталог внутри `.work`; прежние логи не перезаписываются. У каждого собственного процесса Java
heap128MiB и timeout120s; timeout останавливает только его дерево. Нет downloads, сети, upstream build
и приобретения SDK. Коды:0 успех/справка,1 ошибка входов/provenance/process/comparison.
Без аргументов, `-Help`, `-HelpRu`, `Get-Help -Full` рабочие входы не нужны и Java не запускается.

Шесть ячеек входа: ID, операция, reserved `-`, UTF-16 definition hex, числовой аргумент и auxiliary.
D/Y/T/S — реальные day-id/YMD/time/session queries; R сохраняет actual raw/total/public raw;
H — старые/новые поля и Java live navigation после обновления defaults. C++ отдельно сравнивает
constructor raw component R и явно проверяет свою более строгую immutable navigation H.
[Java fixture/provenance](../../../tests/fixtures/schedule-query/provenance.json) сохраняет TZDB2024a;
[36 source-derived2026f expectations](../../../tests/fixtures/schedule-query/tzdb2026f-provenance.json)
хранятся отдельно для конкретных случаев. Вырожденные Java-операции без завершения не запускаются
как oracle capture: конечный отказ C++ по work budget имеет отдельный bounded test.
