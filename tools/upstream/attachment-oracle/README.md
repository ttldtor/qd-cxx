# Serialized attachment oracle

[Русская версия](README.ru.md). English is authoritative.

`Invoke-Oracle.ps1` captures/verifies pinned Message/Configuration Object setters, standard
production records and delegates, BinaryQTPComposer/BinaryQTPParser callbacks and FileWriter tapes.
It requires existing production classes at upstream.lock, Java 8 and `qdcxx-run-utc`.
There is no network access, SDK build or dependency acquisition.

```powershell
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Help
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -HelpRu
Get-Help tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Full
pwsh -NoProfile -File tools/upstream/attachment-oracle/Invoke-Oracle.ps1 -Mode Verify -Upstream C:/QD -JavaHome C:/jdk8 -Logger C:/tools/qdcxx-run-utc.exe -RunDirectory .work/attachment-verify-new
```

No arguments/`-Help` show EN help; `-HelpRu` shows RU. These modes and `Get-Help` need no inputs,
create no outputs and start no Java/QD initialization. Mode defaults to Verify. All four paths
are required for work. RunDirectory must be fresh inside the current checkout's `.work`, with
no reparse ancestors. Existing evidence is refused. Capture creates the fixed new candidate
directory `tests/fixtures/attachments/capture-v1` and provenance; an existing capture is refused.
Verify reads unchanged fixtures and compares every artifact and provenance.

The launcher inventories all classes and resources in 14 explicit classpath roots separately
and verifies unchanged production source/class/resource hashes before/after. Each subprocess
has a 128 MiB heap and 60-second timeout; only an owned timed-out process tree is killed.
UTF-8 observations use LF. UTC and Pacific/Honolulu artifacts must match. The fresh run retains
full timestamped argv/output/exit logs, captured logger stdout/stderr, inventories and receipt.
Exit 0 means successful work/help; exit 1 means invalid input/reference, mismatch, failure or timeout.

Fourteen event cases cover null, String, empty/content Java byte arrays, a benign Serializable,
engaged-empty raw and malformed raw for both events. Two rows witness standard Text field 10 only.
This does not qualify complete Text APIs, Configuration TICKER filtering or a native codec registry.
See [fixtures](../../../tests/fixtures/attachments/README.md) and
[contract](../../../spec/events/ATTACHMENTS.md).
