# OptionSale events

`events::OptionSale` owns an option transaction associated with an underlying symbol. Use `Feed::createSubscription<events::OptionSale>()` with a HISTORY endpoint. The default HISTORY registry includes this event and supports the default indexed source. Plain symbols preserve their exact text, including exchange-looking suffixes. A positive source ID selects no OptionSale route.

The `Index` identifies the event for indexed delivery and removal. The independent `Time` and `Sequence` describe the business timestamp. `eventFlags` describes indexed delivery and `flags` encodes sale properties. The delivered event owns its values and strings. A nullable `OptionSymbol` preserves both null and empty values.
