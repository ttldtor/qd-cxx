# Контролируемый STREAM-пример

[English](examples.md) · [Оглавление справочника](../CONFIGURATION-REFERENCE.ru.md)

Контракты потребителей отдельных полей и сведения деклараций/исходников с оговорённой доказательностью явно различаются ниже. НЕИЗВЕСТНО обозначает недостающее сведение. Известные объявленные сведения сохраняются; их семантическая проверка ещё не завершена. Ссылки указывают декларацию или потребителя и не означают нового запуска тестов. Значения описывают текущий API; принятые изменения перечислены в [архитектурном контракте](../CONFIGURATION.ru.md).

- [StreamOptions](examples.ru.md#cfg-0929-owner-ru) — `examples/subscriptions/stream_client.hpp`

## StreamOptions {#cfg-0929-owner-ru}

[examples/subscriptions/stream_client.hpp](../../examples/subscriptions/stream_client.hpp)

### `StreamOptions::host` — CFG-0929 {#cfg-0929-ru}

- Тип: `std::string_view`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: байты заимствованного адреса STREAM источника
- Допустимые значения: Функция runPrepared нижнего уровня требует host длиной 1..45, числовой синтаксис сам не проверяет; TcpRuntime проверяет его позже и DNS без серверов не допускает. Перегрузка с EndpointOptions допускает пустой options.host как выбор settings.host, непустой должен совпасть.
- Момент чтения: Обе перегрузки runStream и runPrepared принимают StreamOptions по значению; validOptions проверяет локальную копию до подготовки. host остаётся представлением внешней строки. Перегрузка с EndpointOptions проверяет согласованность и заменяет host и port значениями конечной точки.
- Изменяемость и время жизни: Изменение исходного StreamOptions после передачи не меняет локальную копию. Хранилища host и symbols должны оставаться действительными до возврата; их изменение не является поддерживаемой перенастройкой вызова. Внешний сигнальный флаг может изменяться по контракту sig_atomic_t.
- Ошибка: Неверная длина/несовпадение конечной точки — StreamFailure::CONFIGURATION. Поздняя ошибка TcpRuntime сохраняется в диагностике подготовки.

### `StreamOptions::port` — CFG-0930 {#cfg-0930-ru}

- Тип: `std::uint16_t`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Числовой порт, 1..65535.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Числовой порт, 1..65535.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: StreamFailure / StreamResult с отдельными локальными DispatchTerminal, TcpTerminal и TcpDiagnostic; ACK peer не подразумевается.

### `StreamOptions::recordId` — CFG-0931 {#cfg-0931-ru}

- Тип: `std::int32_t`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: локальный ID записи схемы
- Допустимые значения: recordId≥0 и &lt;schema.records().size(); выбранная запись обязана иметь hasTime=false. Перегрузка с конечной точкой также требует dispatchTurn.fields≥2×число полей без переполнения.
- Момент чтения: Обе перегрузки runStream и runPrepared принимают StreamOptions по значению; validOptions проверяет локальную копию до подготовки. recordId читается при проверке схемы и подготовке объявлений и подписок.
- Изменяемость и время жизни: Изменение исходного StreamOptions после передачи не меняет локальную копию. Хранилища host и symbols должны оставаться действительными до возврата; их изменение не является поддерживаемой перенастройкой вызова. Внешний сигнальный флаг может изменяться по контракту sig_atomic_t.
- Ошибка: Несуществующий/отрицательный ID или hasTime — StreamFailure::CONFIGURATION. Недостаток полей на один цикл обработки — CONFIGURATION с TcpError::WORK_LIMIT, endpoint-record-work.

### `StreamOptions::symbols` — CFG-0932 {#cfg-0932-ru}

- Тип: `std::span<const std::u16string>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Заимствованные 1..16 точных selectors, не более 128 единиц каждый.
- Единицы: см. декларацию: единицы UTF-16/code units
- Объявленные ограничения: Заимствованные 1..16 точных selectors, не более 128 единиц каждый.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: StreamFailure / StreamResult с отдельными локальными DispatchTerminal, TcpTerminal и TcpDiagnostic; ACK peer не подразумевается.

### `StreamOptions::duration` — CFG-0933 {#cfg-0933-ru}

- Тип: `std::chrono::milliseconds`. Исходное значение: `{5000}`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Интервал допуска, 1..600000 миллисекунд.
- Единицы: миллисекунды
- Объявленные ограничения: Интервал допуска, 1..600000 миллисекунд.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: StreamFailure / StreamResult с отдельными локальными DispatchTerminal, TcpTerminal и TcpDiagnostic; ACK peer не подразумевается.

### `StreamOptions::removeAfter` — CFG-0934 {#cfg-0934-ru}

- Тип: `std::optional<std::chrono::milliseconds>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: миллисекунды удаления всех исходных селекторов или отсутствие
- Допустимые значения: nullopt отключает удаление; задано строго &gt;0 и &lt;duration. Таймер наблюдается вызывающим циклом обработки; удаление запрашивается один раз после addComposed.
- Момент чтения: Обе перегрузки runStream и runPrepared принимают StreamOptions по значению; validOptions проверяет локальную копию до подготовки. removeAfter проверяется относительно duration; цикл обработки сравнивает прошедшее время с сохранённым сроком и однократно готовит удаление.
- Изменяемость и время жизни: Изменение исходного StreamOptions после передачи не меняет локальную копию. Хранилища host и symbols должны оставаться действительными до возврата; их изменение не является поддерживаемой перенастройкой вызова. Внешний сигнальный флаг может изменяться по контракту sig_atomic_t.
- Ошибка: Неверное отношение времени — StreamFailure::CONFIGURATION до работы; поздняя команда remove может отдельно завершиться отказом своей библиотеки.

### `StreamOptions::maximumRecords` — CFG-0935 {#cfg-0935-ru}

- Тип: `std::optional<std::size_t>`. Исходное значение: Инициализатора в классе нет. Это не доказывает обязательность ввода; инициализация значением отличается.
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Положительная цель завершённых записей; допущенный префикс может её превысить.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Положительная цель завершённых записей; допущенный префикс может её превысить.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: StreamFailure / StreamResult с отдельными локальными DispatchTerminal, TcpTerminal и TcpDiagnostic; ACK peer не подразумевается.

### `StreamOptions::interrupted` — CFG-0936 {#cfg-0936-ru}

- Тип: `const volatile std::sig_atomic_t *`. Исходное значение: `= nullptr`
- Источник: явный нативный вход указанного владельца; источник слоя свойств, где он поддержан, определяется resolver endpoint. Семантика отдельного поля установлена по потребителю.
- Единицы: необязательный адрес сигнального флага
- Допустимые значения: nullptr отключает внешнее прерывание; любой наблюдаемый ненулевой *interrupted запрашивает кооперативную остановку. Указатель и объект обязаны жить до возврата; локальной проверки времени жизни нет.
- Момент чтения: Обе перегрузки runStream и runPrepared принимают StreamOptions по значению; validOptions проверяет локальную копию до подготовки. Указатель interrupted копируется; observeCutoff читает внешний флаг в цикле обработки.
- Изменяемость и время жизни: Изменение исходного StreamOptions после передачи не меняет локальную копию. Хранилища host и symbols должны оставаться действительными до возврата; их изменение не является поддерживаемой перенастройкой вызова. Внешний сигнальный флаг может изменяться по контракту sig_atomic_t.
- Ошибка: Само ненулевое значение не является ошибкой: result.interrupted=true и beginStop(false). Недействительный указатель нарушает предусловие вызывающей стороны.

### `StreamOptions::ownerOperationsPerTurn` — CFG-0937 {#cfg-0937-ru}

- Тип: `std::size_t`. Исходное значение: `= 10000`
- Классификация: конечный параметр: ниже приведены сведения с оговорённой доказательностью. Контракт декларации или исходника, без независимой перепроверки. Семантическая проверка поля и его времени жизни ещё не завершена; запуск runtime-тестов не заявляется.
- Источник: указанная декларация. Явный типизированный вход; псевдоним свойства и отдельный приоритет поля не установлены.
- Объявленное назначение: Попытки локального progress, общие для одного хода владельца, 1..10000.
- Единицы: НЕИЗВЕСТНО: см. объявленное назначение; дополнительные единицы из типа не выводятся
- Объявленные ограничения: Попытки локального progress, общие для одного хода владельца, 1..10000.
- Момент чтения: Явный параметр публичных операций: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Изменяемость и время жизни: Момент чтения и владение определяются публичным контрактом владельца; общей гарантии замены во время работы нет. Время жизни конкретного поля отдельно не перепроверено.
- Ошибка: StreamFailure / StreamResult с отдельными локальными DispatchTerminal, TcpTerminal и TcpDiagnostic; ACK peer не подразумевается.
