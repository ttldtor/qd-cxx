# Эталон исполнения callback

Runner наблюдает настоящие API executor закреплённой Java-реализации endpoint/subscription с конечными планировщиками приложения. Внешняя сеть и пересборка эталона не используются. Ручной, inline и двухпоточный варианты проверяют наследование/override, различие принятия задачи и завершения callback, inline-доставку и последовательность callback одной подписки. Исходные результаты Java для null/closed сохраняются и при отличиях native-профиля.

```powershell
pwsh -File tools/upstream/callback-execution-oracle/Invoke-Oracle.ps1 -HelpRu
pwsh -File tools/upstream/callback-execution-oracle/Invoke-Oracle.ps1 -Help
```

`-Mode Capture` записывает только fixture/provenance, `-Mode Verify` (по умолчанию) сравнивает их. Нужны `-Upstream` с закреплённой ревизией и готовыми production-классами, `-JavaHome` JDK8, `-Logger` UTC runner и новый `-RunDirectory` внутри `.work`. Heap каждого процесса ограничен 128 МиБ, внешний deadline — 60 секунд; ожидания callback и завершение собственных workers конечны. Полные команды, exit codes, raw-потоки и class inventories сохраняются приватно. Exit 0 означает успех/справку; exit 1 — ошибку входных данных, deadline, процесса, эталона или наблюдения. Справка не требует рабочих путей и не запускает работу.

Launcher связывает hashes harness, launcher, fixture, Java tools, четырёх первичных исходников и полного inventory классов/ресурсов. Capture требует совпадения UTC и Pacific/Honolulu; Verify также проверяет неизменность привязок. Reject/drop — native-контракт с C++-тестами; Java требует executor без отказа принятия. См. [native-контракт исполнения](../../../spec/subscriptions/CALLBACK-EXECUTION.ru.md).
