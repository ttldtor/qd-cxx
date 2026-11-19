# Server demand and client delivery

[Русская версия](SERVER-DEMAND.ru.md). English is authoritative.

A server owner opens one `MuxQueue` for each contract it supports. Every downstream client has a separate registration and a reservation for commands, records and retained batches. The owner serializes queue and demand operations and supplies finite work budgets. There are no background threads in these components.

Decode complete ADD/REMOVE frames with the [incoming subscription decoder](../protocol/INCOMING-SUBSCRIPTIONS.md). Submit the resulting batch to the registration through `MuxQueue::submitIncomingSubscription`. The queue checks that the client belongs to this owner and the message matches its contract. A rejected batch queues no prefix. An accepted ticket copies the entire input. Inspect the ticket after `progress` to distinguish application from refusal or cancellation. Application changes the desired union. If collector synchronization is still pending, source `admit` can return `BUSY` without consuming the event.

`DemandTracker` reconciles a single exact-key desired union from `MuxQueue::aggregateEntries()` or `MuxState::aggregateEntries()` with the demand already admitted to the current upstream output. The key is schema record provenance and exact UTF-16 symbol. The first active client for a key requires one ADD. Additional clients for the same key require no duplicate ADD. The last removal or retirement requires one REMOVE. If a HISTORY key remains active and its minimum raw bound changes, ADD replaces the old bound without a preceding REMOVE. Changes that occur between observations can coalesce into the latest desired state.

`next` returns one change with a non-reused local sequence. While it is outstanding, later polls return that same change with the original text. Feed it to the upstream writer and acknowledge the sequence after the output owner accepts the complete command and will finish sending it in order. Acknowledgment advances only the local output state. On source generation loss, drop its pending output and reset the tracker. The next poll works from empty acknowledged demand and replays the current union. The tracker cannot recover a partially transmitted command on a live connection. Finish that command or retire the connection before reusing the sequence of wire commands.

Both maps are reserved at creation. Poll validation and comparison work are explicit. Reconciliation returns an error without changing either the acknowledged map or an outstanding change. Returned request text is borrowed until acknowledgment, reset, move or destruction. The caller supplies the actual complete unique-key union from the serialized owner. To bound memory across connections and contracts, include two banks per tracker in the server's reservations.

## Two-client demand traces

These are serialized local state traces, not wire or delivery traces. A and B are already
created clients; K is one exact key in the same schema and family. `—` means absent.
Assume each listed `next` succeeds, and each change is acknowledged only after complete
output admission as described above. A poll with no change means local convergence.

| Operation | A | B | Aggregate | Next change |
| --- | --- | --- | --- | --- |
| Initial STREAM/TICKER | — | — | — | none |
| A adds K | K | — | K | ADD K |
| A adds K again | K | — | K | none |
| B adds K | K | K | K | none |
| A removes K | — | K | K | none |
| A removes K again | — | K | K | none |
| B removes K | — | — | — | REMOVE K |

An effective mutation of a client can advance the revision even when the aggregate is
unchanged; a no-op does not. Removing a client also removes its interests. Dropping its
token alone does not. Re-adding a removed membership creates a new activation.

For HISTORY, values below are signed raw 64-bit lower bounds, not converted timestamps.
Selection for each client is inclusive at its own bound; it is not a snapshot or transaction
completion guarantee. Each row starts after the previous change was acknowledged.

| Operation | A | B | Aggregate K | Next change |
| --- | --- | --- | --- | --- |
| A adds K at 32 | 32 | — | 32 | ADD K, 32 |
| B adds K at 10 | 32 | 10 | 10 | ADD K, 10 |
| B adds K at 10 again | 32 | 10 | 10 | none |
| B changes K to 40 | 32 | 40 | 32 | ADD K, 32 |
| A removes K | — | 40 | 40 | ADD K, 40 |
| B removes K | — | — | — | REMOVE K, no bound |

If `ADD K, 32` is still pending when B adds K at 10, `next` returns the original
change and sequence until acknowledgment. Only then can it produce `ADD K, 10`.
If the source generation is lost with that change pending, retire the old output and
successfully reset the tracker: the old sequence cannot be acknowledged, and the next
poll issues a new sequence with the current aggregate, K at 10. A refused reset preserves
the old state. Desired state may change while offline. This reset/replay rule does not
itself establish reconnect, peer receipt, continuous delivery or recovery of a partial
command on a live connection. Multiple keys can require multiple polls; do not infer a
portable ordering between keys from these single-key examples.

The [native state tests](../../tests/conformance/mux_interest_tests.cpp) cover the
two-client union, signed minimum, inclusive selection, pending change, stale sequence,
reset failure and replay. The [validation record](../../tests/conformance/subscriptions/MUX-VALIDATION.md)
distinguishes these checks from the [QD 3.355 production capture](../../tests/fixtures/subscriptions/CAPTURES.md#production-observations):
that capture observes the bounds `4294967298 → 4294967297 → 4294967298 → 4294967299 → -1`
and no aggregate REMOVE during those changes. It does not establish native wire parity or
reconnect. HISTORY All selectors are unsupported by the native policy; QD's observed literal
star behavior is not a claim that Java rejects them. See the capture's wildcard qualification.

A client consumer may retain one `OwnedMuxBatch` for a partial write and return `PAUSE`. After fully sending the batch, release it and request resume. On connection failure release the batch, request abort on that client, and pump the owner until its terminal result is visible. Other registrations retain their own interest and bounded delivery. TICKER may conflate locally pending updates for the same key. With `storeEverything=false`, the latest value remains available to a late client while another registration holds the key and is evicted after the last withdrawal.

The first/last transition follows the Java QD distributor observations. The Java reference for overlapping consumers and distributor demand is [QDCollector](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDCollector.java) and [QDDistributor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDDistributor.java). The adapter does not add QTP message types.
