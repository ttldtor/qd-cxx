# Event callback execution

The default is the existing SDK worker. `Endpoint::setEventExecutor` changes the event-listener default; `Subscription<Event>::setEventExecutor` supplies an override. A null subscription policy inherits the endpoint; a null endpoint policy restores worker delivery. These methods do not change the immutable endpoint configuration, transport properties or wire descriptors. Getters return accepted policy intent, not completion of a previously selected packet. Admission after close fails with `CLOSED`. State and subscription-change listeners retain their existing native channels.

## Selection and serial execution

A listener/policy selection is immutable for one complete admitted batch. The SDK prepares owning typed events on its worker, pauses further selection for that subscription and transfers a move-only `CallbackTask` to `CallbackExecutor::post`. At most one packet for a subscription is preparing, posting, queued, running or retiring. Accepted post is distinct from callback completion. An executor may run inline, on another thread, in a different inter-subscription order, or retain a task for later. Runs of one subscription remain serial and ordered. Concurrent/repeated runs of the same task claim at most once; moved-from, cancelled and older-generation tasks are inert. A run result describes task processing, not a guarantee every listener ran; delivery statistics expose the actual outcome.

The task object's own move/destruction must not race its member calls. Executors must return promptly and progress or destroy accepted work during normal operation. The SDK owns its event packet and borrows executor workers. It never shuts down unrelated host work. Policy replacement affects later selection; old selection retains its policy through post, invocation and final capture destruction. All application calls, policy destruction and selected capture destruction occur outside runtime, endpoint, subscription and pool locks. Throwing destructors violate the normal C++ destruction contract.

## Outcomes and shutdown

| Outcome | Event accounting and diagnostic |
| --- | --- |
| Rejected before run | Selected events discarded once; `EXECUTOR_REJECTED`; subscription fails. |
| Post throws before run | Discarded once; `EXECUTOR_FAILURE`; subscription fails. Exception text is not retained. |
| Accepted task destroyed unrun | Discarded once; `EXECUTOR_DROPPED`; subscription fails. No retry. |
| Reject/throw after run started | Actual callback outcome remains; `EXECUTOR_CONTRACT` additionally fails the subscription. |
| Listener throws | Existing `CALLBACK_FAILURE`, callback failure statistics and subscription isolation apply. |
| Queued cancellation by close/source reset | Discarded once; advisory `CALLBACK_CANCELLED`; records a gap, without inventing a transport or listener failure. |

Close remains a nonblocking request. The SDK worker claims cancellation of not-started packets without requiring the host to run or release its retained task. A task that wins the run claim before cancellation is not preempted. Already running callbacks follow existing close cutoff rules. Await uses the creator's condition wait and includes SDK-owned posting, running and capture retirement; it does not join unrelated host jobs. Arbitrarily blocked post, callbacks or capture destructors cannot be preempted and have no hard completion deadline. Creator affinity and self/reentrant-await rejection remain in force. No caller polling is needed to complete SDK retirement.

## Exact retirement and source changes

A packet owns completion from queue pending-to-in-flight selection through the returned post result, actual user function and final selected listener/policy capture destruction. In-flight counts do not become delivered merely because post accepted or the worker adapter returned. Listener calls/durations count only actual user calls; queue/post wait is excluded. Delivered, discarded and callback-failed terminal counts are charged once.

Automatic source reset first requests cancellation of queued/not-started packets. The facade performs that cancellation on its existing worker; reset then waits on a pure outstanding-packet fence. Explicit reconnect/clear/close also cancels not-started work. RESET and a new positive source epoch cannot overtake old callbacks, last captures or their statistics completion. The embedded fence is borrowed only by the worker-owned recovery and counted packets. Packet quiescence and fence release are published under the same pool lock before registry replacement can release the endpoint owner. No runtime/native ownership escapes into late task tokens.

The passive private completion cut is `{quiescent, retirementRevision}`. Preparing, posting, queued, running and retiring are not quiescent. Revision advances after statistics and final captures; changing execution policy does not reset it. This qualifies history publication together with its endpoint clear/logical/mapping fences. Reading a snapshot never drives progress.

## Reentry and finite state

Actual data callback scopes join a counted runtime cohort: only first entry starts the data fence and only last exit clears it. Queued tasks do not hold an executing cohort. Notifications admitted from post/run/selected destruction additionally depend on an exact pool/slot/generation packet barrier. Thus a thread-scope exit cannot release them before packet retirement.

A task run nested in another callback/capture turn reserves a fixed after-return continuation on the existing subscription-change driver and returns `DEFERRED`. It executes iteratively after the exact source retirement, without recursive event delivery, waiting for another callback, a new worker, reposting or host polling. A→B→A and cross-runtime mutations share finite turn admission credits. Exhaustion is explicit `WORK_LIMIT`; it does not silently grow a queue. Cancelled targets leave only inert pure identities for late continuation visits. Parallel callbacks may conservatively delay subscription-change delivery until their overlapping data cohort ends.

## Resources and slow subscriptions

The pure pool is allocated lazily on the first custom policy. There are exactly `maximumEndpoints * maximumSubscriptionsPerEndpoint` reusable physical identities. Generations never wrap. The pool/control-block allocation remains owned through its actual final deallocation, including stale host tasks. A stale task retains fixed detached metadata, not an event packet, callable, endpoint, bootstrap, socket or thread. Retaining tasks across many runtime instances retains one still-owned pool per instance; there is no process-global quota claim.

`maximumCustomCallbackSubscriptions` bounds simultaneous custom/selected reservations. `maximumCallbackBytes` bounds logical pool storage and packet reservations. One packet reserves `1 MiB + 4096 + 4 * sizeof(RegisteredEvent)` logical bytes. The 1 MiB allowance covers complete owning projection under the current four-record receiving profile (aggregate per-record text/opaque fields and 64 KiB record arena); the named control allowance covers packet/vector/policy-reference metadata. Existing listener ledgers separately keep selected callables charged through destruction. Pool metadata is `sizeof(pool) + slotCount * sizeof(slot) + 4 * sizeof(void*)` logical bytes. These are conservative logical credits, not allocator overhead or process RSS measurements. Opaque application capture graphs and host scheduler allocations remain application-owned.

Endpoint changes preflight every inheriting subscription before committing; overrides remain independent. New inheriting subscriptions reserve before publication. Disabling custom execution keeps an already selected reservation until exact retirement. Pool initialization is O(slot count), endpoint changes O(subscription count), and event work is bounded by current projection/listener/turn limits. No SDK thread pool or unbounded SDK task queue is added.

The default still blocks the shared SDK worker when a listener is slow. Custom execution pauses that subscription while allowing the worker and siblings to progress. An endpoint source change waits its outstanding old packet retirement. A shared serial host executor can impose head-of-line delay; separate policies help only when host resources allow. This policy does not alter existing queue conflation, finite overload behavior or transport flow control, and promises no throughput or callback latency.

## Java comparison

The [pinned public Java endpoint executor contract](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXEndpoint.java#L657) covers all asynchronous user interactions and forbids executor rejection. Native `EventExecutor` is deliberately event-only. The [subscription executor API](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/api/DXFeedSubscription.java#L600) provides override/null inheritance. Actual [reference observations](../../tests/fixtures/callback-execution/oracle.tsv) preserve these supported selection and serial-delivery behaviors.

Native null endpoint restoration and rejection of policy admission after close differ from Java's endpoint null rejection and closed-subscription setter assignment. Native rejection/throw/drop/cancel diagnostics are explicit extensions to Java's nonrejecting-executor requirement. These timing/error differences do not claim broader Java endpoint executor parity. The [runner](../../tools/upstream/callback-execution-oracle/README.md) records actual supported production behavior; C++ causal tests establish native failure and lifetime rules.

After-return task and target identities share the existing finite causal-driver allocation, sized by `maximumSubscriptionChangesPerTurn`. No extra default custom pool or per-task control block is allocated.
