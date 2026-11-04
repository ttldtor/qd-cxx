# Text-event Java oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures or verifies TextMessage and TextConfiguration against the pinned Java production delegates, BinaryQTP composer/parser and FileWriter. It needs existing production classes, JDK 8 and `qdcxx-run-utc`; it does not access the network or build an SDK.

```powershell
pwsh -NoProfile -File tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help tools/upstream/text-events-oracle/Invoke-Oracle.ps1 -Full
```

No arguments or `-Help` print English help; `-HelpRu` prints Russian help without starting Java or creating output. Verify is the default. Capture requires a fresh run directory inside `.work` and creates the fixed new fixture directory; existing evidence is never replaced. The runner limits each process to 128 MiB and 60 seconds, inventories production source/class/resource hashes before and after, and records full UTC command logs and a receipt. Exit 0 means success/help; exit 1 means invalid input, mismatch, timeout or process failure.

Six cases cover null, empty and exact UTF-16 content, including NUL and unpaired surrogates, for each type. The checked-in FileWriter tapes are independently consumed by the native typed-mapper test. These cases do not establish Java live endpoint compatibility or TICKER version behavior.
