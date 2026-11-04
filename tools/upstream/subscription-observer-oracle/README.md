# Subscription observer production oracle

[Русская версия](README.ru.md). English is authoritative.

This bounded offline runner calls actual pinned Java `DXFeedSubscription` and `ObservableSubscriptionChangeListener` production classes. It records symbol/type introspection, initial state, duplicate/no-op mutations, removal-before-addition, actual decorated values, self-removal/close, reentry depth and propagated observer exceptions. Set contents are sorted for deterministic representation; callback and operation order remain observable. It does not emulate subscription logic or open a feed connection.

`Invoke-Oracle.ps1 -Mode Capture|Verify -Upstream <pinned-checkout> -JavaHome <jdk8> -Logger <qdcxx-run-utc> -RunDirectory <fresh-private-run>` requires an already built reference at [upstream.lock](../../../upstream.lock). The run directory must be a fresh contained nonredirected directory under the checkout's private work area. No dependency, SDK or network acquisition is performed. Existing production classes are hashed before/after and never rebuilt. One child runs at a time, heap128MiB, deadline60s; only an owned timed-out child can be stopped.

Capture replaces only the [new fixture and provenance](../../../tests/fixtures/subscription-observer/README.md). Verify compares output and source/tool/class provenance with those files. Complete UTC commands/exits, raw streams, inventories and receipts remain in the supplied private run directory. UTC and Honolulu outputs must match. Exit0 means success/help; Exit1 means invalid input, reference/provenance mismatch, timeout, process or observation failure.

No arguments, `-Help`, `-HelpRu` and `Get-Help -Full` display purpose, parameters, limits, output, exits and side effects without requiring operational paths or starting Java/source work. Incomplete non-help invocations remain errors. Repository Java uses the pinned formatter with JDK21+, separately from oracle JDK8.

The [native observation contract](../../../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.md) explicitly distinguishes raw Java monitor/reentry/exception outcomes from native outside-lock finite callback semantics. Existing selector identity fixtures and ADR-0012 remain unchanged.
