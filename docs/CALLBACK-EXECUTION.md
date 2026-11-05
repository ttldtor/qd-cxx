# Choosing event callback execution

By default event listeners run on the SDK worker, serially per subscription. To select another scheduling policy, pass an owning `CallbackExecutor` to the endpoint or to a subscription. The SDK retains the policy object; the application owns its workers and queue. No caller polling or SDK thread-pool setup is required.

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

For an application queue, `post` transfers the move-only task into that queue and reports `ACCEPTED` only if the transfer succeeded. Its consumer calls `task.run()`. Return `REJECTED` on refusal; do not run and then reject. Keep `post` prompt. A task object may be run concurrently/repeatedly, but its own move/destruction must not race those member calls. Dropping accepted work is observable as `EXECUTOR_DROPPED`.

A null subscription policy inherits the current endpoint policy; a null endpoint policy restores worker execution. Replacing a policy changes later selection: selected work retains its old listeners and policy through actual invocation, post return and final capture destruction. Inspect accepted policy with `getEventExecutor()`, event scheduling outcomes with `getCallbackDispatchDiagnostic()`, and actual delivery with [statistics](FEED-API.md). Scheduling refusal fails only the affected subscription. Native callbacks still contain listener exceptions.

Each subscription has at most one preparing/queued/running/retiring packet, so a multithreaded or reordering host cannot overlap its callbacks. Nested `run()` can return `DEFERRED`; the existing bounded after-return driver completes it after the outer selected captures retire. Other subscriptions can progress while one custom callback is slow. A serial host queue can still delay its own users, and overlapping data callbacks can delay subscription-change observers. Default slow callbacks retain their existing shared-worker effect.

Close is a request. The SDK cancels not-started work without requiring the host queue to progress, and waits for already running/posting/capture retirement on creator await. It does not shut down host workers or preempt application code. A retained old task becomes inert and holds only fixed detached pool metadata. Release old host tasks when no longer needed; pools from distinct runtimes remain distinct owned memory scopes.

Configure `RuntimeOptions.maximumCustomCallbackSubscriptions` and `maximumCallbackBytes` before creating the runtime. Opt-in admission checks finite logical packet/pool credits; endpoint defaults reserve all inheriting subscriptions transactionally. Selected credits remain reserved through final retirement even if policy is disabled. Opaque application captures and host queue storage are separately application-owned. See the [full execution/lifetime contract](../spec/subscriptions/CALLBACK-EXECUTION.md) for exact bounds, source-epoch fencing and failure accounting.

This is an event-only native policy. State and subscription-change callbacks retain their existing channels. Java's endpoint executor covers more channels and forbids rejecting executors; native null/closed admission and rejection/drop diagnostics intentionally differ. [Actual Java observations](../tools/upstream/callback-execution-oracle/README.md) record these boundaries.
