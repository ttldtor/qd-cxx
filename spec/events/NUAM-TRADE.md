# NuamTrade

[Русская версия](NUAM-TRADE.ru.md). English is authoritative.

NuamTrade extends Trade with statistics maintained independently of the most recent trade. The
statistics comprise TradeStatTime in UTC milliseconds, LastSignificantPrice, LastPriceForAll,
NumberOfTrades and VWAP. The exact source record names are NuamTrade and, when explicitly configured,
NuamTrade followed by a regional exchange suffix.

Last.Price and Last.Size and all five statistics fields are required by the generated Java mapping.
The remaining common Trade fields are optional. The optional Last.Tick supplies direction only
when Last.Flags reports UNDEFINED. The record exchange supplies the exchange when the optional
Last.Exchange field is absent. Event time metadata is independent from trade and statistics time.

The mapper owns its complete output and preserves the original business double for each statistics
price until the corresponding raw decimal changes. It checks the full record context and shape
before producing any output. Field aliases follow the Trade mapping and the five statistics use
their exact names.

The default registry exposes the composite record to ordinary subscriptions on TICKER and STREAM.
A caller-supplied registry may add an explicit regional NuamTrade record. HISTORY does not select
NuamTrade. A regional symbol is routed using its exchange suffix.
The reference is QD3.355 CustomFactoryImpl, NuamTradeMapping and NuamTradeDelegate.

In the measured MSVC x64 Release profile, the complete default TICKER catalog reserves 16,384
precomputed event-selection plans. Each plan occupies 792 bytes and routing metadata accounts for
12,994,848 retained logical bytes. Adding the NuamTrade kind doubles the plan count from 8,192
and increases this table by 6,488,064 bytes on that profile. Sizes depend on the compiler and
architecture. Applications can supply an explicit smaller registry when appropriate.
