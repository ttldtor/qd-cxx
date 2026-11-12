# NuamOrder receiving

`NuamOrder` is a distinct indexed `Order` projection with matching-engine identifiers, nullable owned strings, trigger and quantity values, and the original Nuam flag word. The standard Java defaults provide HISTORY records `NuamOrder`, `NuamOrder#NUAM` and `NuamOrder#nuam`. Their record suffix determines the high 32 source bits of the indexed event; it is independent of business time and event flags.

Four-bit order and time-in-force codes outside the defined ranges map to `UNDEFINED`; the raw flags remain available. A string field may be null or empty, and all copied text is bounded by the configured event value limits. Numeric getters preserve source business precision while their corresponding raw values remain unchanged. Explicit registries may provide other valid record source suffixes.

A receiving profile supports up to 20 NuamOrder source routes. The default three use a 320 KiB session snapshot arena; each additional registered route reserves 64 KiB. More than 20 routes are refused before activation. The finite resident allowance also governs retained concurrent snapshots.

On the measured MSVC x64 Release profile, the default HISTORY routing metadata retains 14,044,568 logical bytes for 16,384 selection plans; this is not a process RSS measurement.
