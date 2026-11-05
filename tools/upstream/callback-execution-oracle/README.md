# Callback execution reference

This runner observes the pinned production Java endpoint/subscription executor APIs with finite application-owned schedulers. It does not use a network endpoint or rebuild the reference. Manual, inline and two-worker execution establish policy inheritance/override, acceptance before callback completion, inline delivery and serial delivery per subscription. The raw Java null/closed behavior is retained even where the native profile differs.

```powershell
pwsh -File tools/upstream/callback-execution-oracle/Invoke-Oracle.ps1 -Help
pwsh -File tools/upstream/callback-execution-oracle/Invoke-Oracle.ps1 -HelpRu
```

Use `-Mode Capture` to write the two fixture/provenance files, or `-Mode Verify` (default) to compare them. Supply `-Upstream` at the lock revision with existing production classes, `-JavaHome` for JDK8, `-Logger` for the UTC runner, and a fresh contained `-RunDirectory` under `.work`. Each process has a 128 MiB heap and a 60 second outer deadline; owned callback waits and worker shutdown are finite. Full commands, exits, raw streams and class inventories are private outputs. Exit 0 means success/help; exit 1 reports invalid input, timeout, process, reference or observation failure. Help needs no operational paths and starts no workload.

The launcher binds the harness, launcher, fixture, Java tools, four primary source files and complete production class/resource inventory. Capture requires matching UTC and Pacific/Honolulu output; Verify also requires unchanged source bindings. Rejection/drop behavior is a native contract tested by C++; Java requires a nonrejecting custom executor. See [the native execution contract](../../../spec/subscriptions/CALLBACK-EXECUTION.md).
