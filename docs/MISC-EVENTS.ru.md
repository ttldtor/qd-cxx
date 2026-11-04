# Message и Configuration

[English version](MISC-EVENTS.md). Английская версия определяет контракт.

`qd/events/message.hpp` и `qd/events/configuration.hpp` предоставляют владеющие значения через
`qdcxx_misc_events`. Символ, время FIELD и сериализованное вложение не зависят от времени жизни входного
буфера, mapper и endpoint. Configuration также предоставляет знаковую32-битную версию события.
`getAttachment()` возвращает неизменяемый `SerializedAttachment`; копирование события копирует байты.
Чтобы сохранить значение после возврата callback с заимствованным batch, скопируйте событие.

`PreparedMessage::prepare` и `PreparedConfiguration::prepare` связывают явную позицию записи полной
локальной схемы. Поддерживаются aliases; проверяются все поля, включая невыбранные дополнительные.
Стандартные поля `Message` и `Configuration` имеют точный полный тип249 (`SERIAL_OBJECT`); типы9/233
его не заменяют. `defaultMessageSchema` и `defaultConfigurationSchema` создают независимые схемы.
`map`/`mapView` принимают конечные `AttachmentEventLimits`: число UTF-16 units символа, байты payload
и общий объём копии. Ноль является настоящим пределом. Общий объём — два байта на unit плюс payload.
Проверка предшествует публикации целого владеющего результата; частичное событие не возвращается.

Публичный Feed принимает `Subscription<Message>` в STREAM и `Subscription<Configuration>` в STREAM
или TICKER, включая `RegisteredEvent` и явные `EventSelection`. Каждая запись регистрируется с нужным
`EventType` и точным контекстом схемы. Локальные номера типов7/8 не являются wire record IDs.
Правила listener, закрытия, времени жизни captures и исключений описаны в [Feed API](FEED-API.ru.md).

## Сериализованные байты и явное декодирование

`qd/events/serialized_attachment.hpp` доступен через `qdcxx_attachments`. Null означает отсутствие
optional body; существующее пустое body отличается от null. `wireKind()` сохраняет полный тип,
`bytes()` заимствует точные неизменяемые байты до изменения или разрушения владельца. Создание проверяет
тип и предел длины до выделения памяти. Copy assignment сначала готовит полную замену; ошибка выделения
оставляет прежнее значение целым.

`AttachmentCodecRegistry::create` копирует явные `AttachmentCodecRegistration` и callbacks в неизменяемый
общий snapshot с конечными `AttachmentCodecRegistryLimits`. Приложение задаёт согласованный ключ формата,
версию, полный тип поля и `typeid(T)` результата. Автоматически зарегистрированных production-кодеков нет.
Java serialization остаётся байтами; Java byte-array во вложении содержит сериализованный Java-массив.

`registry.decode<T>(attachment, key, version, limits)` возвращает `expected<optional<T>, AttachmentError>`.
T должен быть копируемым владеющим значением. Указатели, ссылки, стандартные string_view, span и
reference_wrapper отклоняются при компиляции; пользовательские типы должны самостоятельно владеть
динамическими данными. Ключ, версия, тип результата и полный тип поля проверяются даже для null.
Корректный null возвращает отсутствующее значение без callback. Пустое body передаётся как ненулевой
span длины0. Успех требует точного типа и потребления всего body. Неизвестный кодек, неверная версия/тип,
повреждённое body, ресурсный отказ и ошибка callback сохраняют исходное вложение. Обычное исключение
превращается в `CODEC_EXCEPTION`, `bad_alloc` — в `ALLOCATION_FAILURE`. Диагностика состоит из enum,
без текста исключений и дампа payload.

Decode выполняется синхронно. Snapshot удерживает callback captures до конца вызова. Captures и результат
должны владеть данными. Копии registry разделяют gate каждой регистрации. Занятая регистрация сразу
возвращает `CODEC_BUSY`; повторный вход того же потока в ту же регистрацию — `REENTRY`. Ожидания, очереди,
повторы и фоновые callbacks не создаются. Разные регистрации могут выполняться одновременно и вызывать
вложенный decode; немедленный отказ при занятости исключает цикл блокировок SDK. Сам пользовательский
callback остаётся доверенным нативным кодом приложения.

`AttachmentDecodeLimits` проверяет входные байты до callback. Кодек обязан заранее списывать output bytes,
scratch bytes и документированные work units через `AttachmentDecodeContext`. Списания накопительные,
без возврата; проигнорированный отказ всё равно запрещает успешный результат. Output включает логический
объём значения, динамические данные и терминаторы; scratch — временные логические данные. Enum-диагностика
занимает0 diagnostic bytes. Registry проверяет реальные запросы памяти SDK для wrappers, shared state,
entries и ключей плюс объявленные `retainedStateBytes` до выделения. Служебные данные allocator и входы
вызывающей стороны исключены. Динамическое состояние callback/captures объявляется консервативно.
Контекст не ограничивает произвольные внешние allocations, RSS процесса или бесконечный callback.

## Приём версии Configuration

Опциональный `versionField()` mapper выбирает физическое целое поле по действующим mapping rules.
У широкого целого используется первый знаковый32-битный слот. Отсутствие поля отключает фильтр.
EventRegistry сохраняет выбор при единственной подготовке. Только явно зарегистрированные Configuration
устанавливают политику TICKER; generic-запись с таким же именем сохраняет прежнее поведение. Общий binding
collector пригоден и для независимо зарегистрированных versioned-семейств. STREAM сохраняет порядок всех записей.

TICKER сравнивает входную версию с последним staged-значением символа, включая более ранние кандидаты
того же атомарного batch. Меньшие версии не заменяют cache и не попадают в replay/delivery. Равные
допускаются к обычному сравнению данных. Знаковые extrema не используют wraparound. Для новой записи
сравнение идёт с default-zero Java: первая отрицательная версия даёт наблюдаемое событие с version0,
null attachment и eventTime0. Символ остаётся запрошенным. Следующее неотрицательное обновление сравнивается
обычным образом. Без Version эта политика отключена. Версия формата кодека и результат decode на приём не влияют.

Collector разделяет значения проекции и provenance. `TickerValue::sourceRecord` показывает выбранное
нормализованное zero/null представление; `rawSourceRecord` сохраняет исходную запись. MUX удерживает
исходные raw metadata и отдельный `versionDefault`, чтобы typed-проекция использовала eventTime0.
Трассировка сохраняет источник без подмены публичных значений default-события исходными metadata.

[Production Java witness](../tests/fixtures/configuration-version/README.ru.md) проверяет default-zero,
знаковые границы, равные/меньшие версии, batch, отсутствие поля и широкий integer mapping. Native
`misc_feed_tests.cpp` отдельно проверяет поздний replay cache при подписке.
[Attachment fixtures](../tests/fixtures/attachments/README.ru.md) содержат реальные стандартные записи
через delegates, composer/parser и tape. См. [семантику вложений](../spec/events/ATTACHMENTS.ru.md) и
[ADR-0014](decisions/ADR-0014-portable-attachment-semantics.ru.md).
