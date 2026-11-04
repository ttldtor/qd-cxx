# Pinned subscription observer evidence

[Русская версия](README.ru.md). English is authoritative.

[oracle.tsv](oracle.tsv) preserves 49 observations from actual pinned Java QD 3.355 production subscription classes; [provenance.json](provenance.json) binds the reference commit, JDK/tool/harness/source hashes and immutable production-class inventory. Reproduction and bounded capture/verify help are in the [oracle runner](../../../tools/upstream/subscription-observer-oracle/README.md).

The scenarios cover ordinary initial/add/replace/remove/clear/close behavior, immutable type/symbol views, equal indexed-source identity, explicit ALL, self-removal with recursive CLOSED, and exception fanout/propagation. Sorting represents set contents only; rows preserve operation/callback order. UTC and Honolulu match. No network source is used.

Native tests compare ordinary membership phases directly. Raw Java monitor ownership, reentry depth2 and propagated exceptions are preserved as evidence; they are not rewritten to match the [native callback profile](../../../spec/subscriptions/OBSERVABLE-SUBSCRIPTIONS.md). This fixture makes no typed history or future event-family claim and does not replace the separate decorated-identity ADR or selector fixture.
