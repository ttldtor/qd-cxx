# Выбор исполнения event callback

По умолчанию listeners исполняются на SDK worker, последовательно для каждой подписки. Для другого планировщика передайте owning `CallbackExecutor` endpoint или подписке. SDK удерживает policy object; workers и очередь принадлежат приложению. Polling клиента и создание SDK thread pool не нужны.

```cpp
#include <qd/api/feed.hpp>

class InlineEvents final : public org::ttldtor::qdcxx::api::CallbackExecutor {
public:
    org::ttldtor::qdcxx::api::CallbackPostResult post(
        org::ttldtor::qdcxx::api::CallbackTask task) override {
        static_cast<void>(task.run());
        return org::ttldtor::qdcxx::api::CallbackPostResult::ACCEPTED;
    }
};

// Create the runtime on its creator thread before starting application workers.
org::ttldtor::qdcxx::api::ApiRuntime runtime;
auto endpoint = runtime.createEndpoint();
endpoint->setEventExecutor(std::make_shared<InlineEvents>());
auto quotes = endpoint->getFeed()->createSubscription<org::ttldtor::qdcxx::events::Quote>();
quotes->addEventListener([](auto values) { /* Copy owning events here when retaining them. */ });
quotes->addSymbols("IBM");
endpoint->connect("localhost:7300");
```

Для application queue `post` передаёт move-only task в очередь и возвращает `ACCEPTED` только при успешном приёме. Consumer вызывает `task.run()`. При отказе верните `REJECTED`; нельзя сначала выполнить и затем отклонить. Post должен быстро возвращаться. Concurrent/repeated run допустим, но move/destruction самой задачи не должны пересекаться с её member calls. Уничтожение принятой невыполненной задачи наблюдается как `EXECUTOR_DROPPED`.

Null у подписки наследует endpoint policy; null у endpoint возвращает worker. Смена policy влияет на следующий выбор: selected work сохраняет старые listeners/policy до invocation, возврата post и последнего capture destructor. `getEventExecutor()` показывает принятый intent, `getCallbackDispatchDiagnostic()` — scheduling outcomes, [статистика](FEED-API.ru.md) — фактическую доставку. Отказ scheduler завершает только затронутую подписку с ошибкой; исключения listeners по-прежнему перехватываются.

На подписку приходится не больше одного preparing/queued/running/retiring пакета. Multithreaded/reordering host не перекрывает её callbacks. Nested run может вернуть `DEFERRED`: существующий конечный after-return driver выполнит задачу после retirement внешних captures. При медленном custom callback другие подписки могут продолжать работу. Общая serial host queue всё ещё задерживает своих пользователей; пересекающиеся data callbacks могут задержать subscription-change observers. Медленный default callback сохраняет прежнее влияние на общий worker.

Close — запрос. SDK отменяет not-started работу без progress host queue, а creator await ждёт running/posting/capture retirement. SDK не останавливает host workers и не прерывает application code. Удержанная старая задача инертна и сохраняет только fixed detached pool metadata. Освобождайте ненужные host tasks; pools разных runtime остаются разными owned memory scopes.

До создания runtime настройте `RuntimeOptions.maximumCustomCallbackSubscriptions` и `maximumCallbackBytes`. Opt-in admission проверяет конечные logical packet/pool credits; endpoint default резервирует inheriting subscriptions транзакционно. Selected credits удерживаются до полного retirement даже после отключения policy. Opaque capture graphs и host queue storage принадлежат приложению. [Полный контракт](../spec/subscriptions/CALLBACK-EXECUTION.ru.md) определяет bounds, source-epoch fences и failure accounting.

Native policy относится только к event listeners; state/subscription-change channels не меняются. Java endpoint executor охватывает больше каналов и запрещает reject; native null/closed admission и reject/drop diagnostics намеренно отличаются. [Actual Java observations](../tools/upstream/callback-execution-oracle/README.ru.md) сохраняют эти границы.
