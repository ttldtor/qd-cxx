# Order source and index preparation

[Русская версия](ORDER-INDEX.ru.md). English is authoritative.

Link `qdcxx_order_index` for the independent C++23 component, or `qdcxx::feed`
when using the installed Feed package. Include `<qd/events/order_index.hpp>`.
No runtime or network startup is required.

```cpp
#include <qd/events/order_index.hpp>

namespace q = org::ttldtor::qdcxx;

int main() {
    const auto source = q::events::OrderSource::valueOf(u"NTV");
    const auto index = q::events::changeOrderIndexSource(7, source);
    return q::events::orderIndexSource(index) == source ? 0 : 1;
}
```

For generic decoded records, prepare once with
`PreparedOrderSource::prepare(schema, localId, mappingLimits)`, then call
`mapIndex({context, completeValues})` or `mapIndexView` for complete borrowed fields.
For `Order#NTV`, the prepared source is NTV; the low signed32 Index field is
preserved as unsigned low32 bits. The plan owns the source and context after the
original schema is destroyed. Another schema's otherwise equal values are refused.
The required Index field currently uses the exact standard full type8.

Use finite explicit mapping limits. The record name consumes additional name and
comparison credits; projection performs no allocation and needs no polling loop.
See the [full contract](../spec/events/ORDER-INDEX.md) for errors, source-family
masks, retained exchange-bit quirks and the distinction between record source,
exchange and endpoint origin. This component prepares identity only; complete
typed Order delivery remains a separate part of the client API.
