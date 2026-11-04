# Production oracle конфигурации endpoint

[English](README.md) · [Инструменты разработчика](../../dev/README.ru.md)

Invoke-Oracle.ps1 захватывает или проверяет реальные production consumers QDS 3.355,
закреплённые upstream.lock. Используются существующие class directories и JDK8;
upstream не пересобирается, его classes/resources не меняются. Fixture из 44 строк фиксирует:

- SocketUtil address-list parsing, включая более широкое принятие портов/списков hosts в Java.
- Создание MessageConnectors, порядок свойств, регистр, миллисекунды reconnectDelay и inactive state.
- Приоритет builder properties DXEndpoint, владение входами и видимость изменений implementation map.
- Явные file defaults против system defaults и explicit builder overrides.
- FEED auto-connect и момент применения inline properties через один loopback listener harness.
  Для STREAM_FEED при build фиксируется ноль автоматически созданных connectors.

Dummy ApplicationConnectionFactory служит только guard неактивных parser cases:
неожиданная активация отклоняется, application connection не имитируется. Parser exceptions
фиксируются только вокруг production parse call; ошибки harness/getter/I/O завершают процесс.
Credentials в синтетических cases — постоянные sample markers; выход содержит только booleans,
без credentials, динамических портов и приватных путей. Это не квалификация authentication/TLS.

## Запуск и справка

~~~powershell
pwsh -NoProfile -File tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -HelpRu
$oracleInputs = @{
    Upstream = 'C:/reference/QD'
    JavaHome = 'C:/tools/jdk8'
    Logger = 'C:/tools/qdcxx-run-utc.exe'
    RunDirectory = '.work/endpoint-configuration-verify'
}
& tools/upstream/endpoint-configuration-oracle/Invoke-Oracle.ps1 -Mode Verify @oracleInputs
~~~

Capture использует те же явные входы, -Mode Capture и другой новый каталог.
Он заменяет только tests/fixtures/endpoint-configuration/oracle.tsv и provenance.json.
Verify сравнивает точную fixture и hashes harness/launcher/upstream. Перед публикацией
проверьте кандидаты. Без аргументов, с -Help, -HelpRu и Get-Help рабочие входы не нужны,
Java не запускается.

Каждый собственный child ограничен 60 секундами, heap Java — 128 MiB. Runner последовательно
запускает UTC и Pacific/Honolulu, сравнивает TSV bytes и полные class inventories до/после.
RunDirectory должен быть новым, внутри ignored .work checkout, без redirecting ancestors.
Полные commands/UTC/exits, raw streams, inventories и receipt остаются приватными.
Останавливается только собственный child по timeout. Exit0 — успех/справка; exit1 —
некорректный вход, reference drift, timeout, process/I/O failure или mismatch.
Внешние endpoints не используются.

## Native boundary

[Конфигурация endpoint](../../../docs/ENDPOINT-CONFIGURATION.ru.md) задаёт native профиль
и явные отличия от Java. Native conformance сравнивает принятые flat reconnectDelay aliases
со значениями fixture и проверяет выбранные намеренные отказы. Принятие raw Java не выдаётся
за native support. Defaults/properties не добавляют ambient lookup в native core.
Ресурсы и wire semantics существующего [recovery](../../../spec/transport/RECOVERY.ru.md) сохраняются.