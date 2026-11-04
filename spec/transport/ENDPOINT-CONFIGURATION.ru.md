# Локальная конфигурация экземпляра endpoint

[English](ENDPOINT-CONFIGURATION.md). Английская версия является нормативной.

Публичный контракт объявлен в qd/api/endpoint_configuration.hpp и объяснён в
[руководстве приложения](../../docs/ENDPOINT-CONFIGURATION.ru.md). Он расширяет существующий
[Feed lifecycle](../../docs/FEED-API.ru.md), сохраняя identity первого адреса, offline desired
subscriptions, конечный recovery и joined retirement native/callback ownership.

## Разрешение, admission и активация

1. Явные слои разрешаются в immutable data-only configuration. Все заимствованные записи
   ограничиваются до сохранения входов. Неизвестные ключи и неподдерживаемые connectors отклоняются.
   Выбранный маршрут использует ту же pure host grammar, что и client endpoint validation.
2. CreateEndpoint удерживает конфигурацию, проверяет/резервирует обычные endpoint resources
   и публикует только полностью подготовленный неактивный endpoint. Ошибка не публикует partial slot.
3. Явный connect проверяет/копирует plain маршрут до существующих runtime/endpoint admission locks.
   Admission revision и state notifications предшествует созданию worker и commit.
4. Политика читается из immutable configuration. Принятый адрес, intent и revision читаются
   вместе под mutex endpoint. Ошибка копирования не меняет lifecycle/admission.
5. Disconnect/close сохраняют immutable policy и историю принятого адреса. Terminal fences
   убирают activation intent из observation. Сохранение snapshot/facade не удерживает native runtime.

Resolver не выполняет I/O или callbacks. Он не добавляет worker, dispatch queue или callback ownership.
Просмотр конфигурации не создаёт и не уничтожает application captures. Прежние data/state/subscription
observer fences и правила closeAndAwaitTermination сохраняются.

## Совместимость и конечное владение

Порядок слоёв, grammar, диапазоны, selected-value validation, конечные count/byte limits и ошибки
нормативно описаны в руководстве. Native name — metadata; потоки SDK она не настраивает.
Requested retry delay сохраняется даже при нулевой effective delay одной попытки.

[Pinned production oracle](../../tools/upstream/endpoint-configuration-oracle/README.ru.md)
сохраняет реальные Java observations. Поддержанный flat reconnectDelay использует проверенные
миллисекунды. Construction-time resolution, явная активация, immutable snapshots, строгие booleans,
отказ неизвестным keys и более узкий route syntax — явные native deviations. Ambient settings
precedence, Java connector extensions или secure credential capability из них не следуют.

Отклонённые credential/userinfo values не попадают в retained effective storage или форматируемые
диагностики. Это exclusion/redaction boundary, а не secure secret store. TLS/authentication и
полный каталог connectors сохраняют отдельные требования возможностей.