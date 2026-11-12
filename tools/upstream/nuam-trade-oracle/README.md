# NuamTrade production oracle

[Русская версия](README.ru.md).

NuamTradeOracle.java captures the pinned Java QD NuamTradeMapping and NuamTradeDelegate
with composite and regional records. The selected cases cover supported physical numeric and raw
types, source business doubles, signed time, trade flags, regional symbols and missing required
fields. The source reuses the Trade capture layout and appends the Nuam statistics.

Run Invoke-Oracle.ps1 with -Help or -HelpRu for the command, inputs, output and failure behavior.
Capture writes fixture and provenance candidates. Verify compares observations and provenance.
Both run the fixed source in UTC and Pacific/Honolulu and require identical output bytes.
