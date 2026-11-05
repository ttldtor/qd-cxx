# Production oracle серверной агрегации

[English](README.md) · [Инструменты разработчика](../../dev/README.ru.md)

Invoke-Oracle.ps1 захватывает/проверяет actual QDS 3.355 classes из upstream.lock. 68 raw-строк
покрывают публичную DXEndpoint setter/validation chain, binary QTP full/delta реальных
AgentAdapter/DistributorAdapter, reset, server clamp, malformed/omitted information, legacy
unavailability, STREAM и настоящий SocketConnector stop/start reconnect с собственным loopback.
Для TimePeriodInfo записаны точные rounding, saturation и принятые extensions.

aggregationPeriodInfo остаётся отдельным peer report и может относиться к предыдущему request.
Clamp сервера и UNKNOWN legacy peer наблюдаются отдельно. Setter после close меняет локальное
Java намерение без перезапуска соединения; новый ACK не придумывается.

~~~powershell
pwsh -NoProfile -File tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -HelpRu
$inputs = @{
    Upstream = 'C:/reference/QD'
    JavaHome = 'C:/tools/jdk8'
    Logger = 'C:/tools/qdcxx-run-utc.exe'
    RunDirectory = '.work/server-aggregation-verify'
}
& tools/upstream/server-aggregation-oracle/Invoke-Oracle.ps1 -Mode Verify @inputs
~~~

Capture использует те же явные входы и новый RunDirectory; заменяет только
tests/fixtures/server-aggregation/oracle.tsv и provenance.json. Verify сверяет raw bytes и привязки
harness/launcher/upstream. Noargs, -Help, -HelpRu и Get-Help не требуют рабочих путей и не запускают
Java. Exit0 означает успех/справку; exit1 — неверный вход, mismatch, timeout или process failure.
Каждый собственный child ограничен 60 секундами, Java heap — 128 MiB. UTC и Pacific/Honolulu идут
последовательно. Полные inventories production classes/resources должны остаться неизменными.
Команды, exit codes, потоки и inventories сохраняются в явном private run directory.
Останавливаются только собственные sockets/children. Внешняя сеть, upstream/SDK build не нужны.

[Native контракт](../../../spec/transport/SERVER-AGGREGATION.ru.md) описывает finite parser,
immutable input и timing отправки. Источник поведения — production Java.
