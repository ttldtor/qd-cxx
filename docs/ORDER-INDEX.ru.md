# Подготовка источника и индекса Order

[English version](ORDER-INDEX.md). Английский текст имеет приоритет.

Для независимого компонента C++23 подключите `qdcxx_order_index`, для установленного
Feed — `qdcxx::feed`. Заголовок — `<qd/events/order_index.hpp>`. Запуск runtime и
сетевое подключение не требуются.

```cpp
#include <qd/events/order_index.hpp>

namespace q = org::ttldtor::qdcxx;

int main() {
    const auto source = q::events::OrderSource::valueOf(u"NTV");
    const auto index = q::events::changeOrderIndexSource(7, source);
    return q::events::orderIndexSource(index) == source ? 0 : 1;
}
```

Для общих декодированных записей один раз вызовите
`PreparedOrderSource::prepare(schema, localId, mappingLimits)`, затем
`mapIndex({context, completeValues})` либо `mapIndexView` для полных заимствованных
полей. У `Order#NTV` подготовленный источник — NTV, младшие биты signed32 поля
Index сохраняются как unsigned low32. План владеет источником и контекстом после
уничтожения исходной схемы. Равные значения из другой схемы отвергаются.
Обязательное поле Index пока принимает точный стандартный полный тип8.

Задавайте конечные пределы отображения явно. Имя записи дополнительно расходует
бюджет имён и сравнений. Проекция не выделяет память, цикл опроса не нужен.
[Полный контракт](../spec/events/ORDER-INDEX.ru.md) описывает ошибки, маски
семейств источников, сохранение перекрывающихся битов биржи и различия источника
записи, биржи и физического происхождения. Здесь подготавливается идентичность;
доставка полного typed Order остаётся отдельной частью клиентского API.
