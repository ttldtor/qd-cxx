# Production oracle Feed-фильтров

[English version](README.md). Нормативна английская версия.

Launchers выполняют существующие Java QD parser, delegate/category paths, IPF filter и
tracker на версии из `upstream.lock`. SDK не пересобирается. У каждого дочернего процесса
heap128MiB и deadline60s. Процессы последовательны; launcher останавливает только свой
процесс и его дерево при timeout.

| Launcher | Проверяемое поведение | Fixture |
| --- | --- | --- |
| `Invoke-Oracle.ps1` | QD glob/composite grammar, стандартные factory, wildcard, первая категория через реальные делегаты, subscription/data routing, typed selectors | `oracle.tsv` |
| `Invoke-IpfOracle.ps1` | Полная загрузка локального файла, attributes, старые immutable values, lazy catchup, равные/неудачные обновления, throw/remove listener, stop/restart, закрытие endpoint | `ipf-oracle.tsv` |
| `Invoke-ConfigOracle.ps1` | Реальные QD TimePeriod, QDConfig properties и IPF config validation | `config-oracle.tsv` |

Без аргументов, с `-Help`, `-HelpRu` и `Get-Help <launcher> -Full` входы не нужны;
Java, выходные файлы и source work не запускаются. Для рабочего режима нужны четыре пути:

```powershell
pwsh -NoProfile -File tools/upstream/feed-filter-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/qd-reference -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/filter-check-01
```

Для IPF и config используйте соответствующий launcher. Режим по умолчанию `Verify`
сравнивает результат с fixture/provenance. `Capture` заменяет только указанные TSV и
парный provenance JSON; кандидаты требуют просмотра перед публикацией. IPF harness также
создаёт и удаляет ограниченные локальные fixtures внутри RunDirectory, без сетевых
подключений. Предупреждения о неполном файле и намеренные исключения слушателей остаются
в private raw stderr. Чистый EOF и семантический COMPLETE — разные наблюдения.

RunDirectory должен быть новым внутри `.work` checkout, без перенаправляющих предков.
Старые доказательства не перезаписываются. Сохраняются полные UTC argv/exit logs, raw
stdout/stderr, хеши исходников, harness/launcher, фактическая версия Java и отдельные
количества production `.class` и ресурсов. Pre/post inventories каждого используемого
classpath root должны совпасть. TSV для UTC и Pacific/Honolulu должны совпасть. Inventory
не утверждает, что каждый класс был загружен. Public provenance и TSV не содержат
рабочих приватных путей.

Exit0 — успех или справка. Exit1 — неверные входы, несовпадающая/отсутствующая pinned
reference, ошибка/timeout процесса, изменение классов, различие часовых поясов или
fixture/provenance mismatch. Ошибки ссылаются на переведённую справку. Syntax failures —
наблюдения, а не пропущенные случаи. Raw captures сохраняются при исправлении harness.

[Native contract](../../../spec/subscriptions/FEED-FILTERS.ru.md) отличает QD
wildcard query от обычного предиката и ограничивает обещания реальными типами registry.
Один TODO в Java не считается доказанным дефектом. Native C++ tests используют captured
expression fixture; выполнение Java само по себе не доказывает native lifetime, ресурсы
или установку пакета.
