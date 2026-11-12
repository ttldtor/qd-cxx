# Source epochs for detached models

[Русская версия](MODEL-SOURCE-EPOCH.ru.md). English is authoritative.

The [order book](ORDER-BOOK-MODEL.md) and
[time-series model](TIME-SERIES-EVENT-MODEL.md) can qualify owning snapshots
against source continuity. Each model still has one serialized caller. An
application owns the Feed subscription, any callback queue, and the evidence
that identifies which activation produced each batch.

## Drive the model through a source reset

1. Call `beginSourceEpoch(ordinal)` at a source activation boundary. Supply a
   positive ordinal that increases across activations of this model. The
   model clears prior values, incomplete input and order-book correction
   stamps. The time-series symbol, fromTime and selection revision stay set.
2. Capture the returned `ModelSourceEpoch` when handing off each originating
   batch. Use `processSourceEvents` with that token. For the time-series model,
   capture the selected range revision as well. For the order book, the order
   source ID in each indexed event is still a separate argument.
3. After observing complete replay for the selected inputs, call
   `confirmSourceEpoch`. The time-series call also requires the revision
   whose completion was observed. An incomplete local transaction or
   snapshot prevents confirmation. A delayed confirmation for a previous
   range is refused even within the same transport epoch.
4. Call `markSourceGap` with the affected token on an observed loss or reset.
   `RETAIN_COMPLETED` keeps the last committed cut for historical inspection.
   `DISCARD` clears it. Either choice discards unfinished transactions. Begin
   the next ordinal to start fresh delivery. A delayed batch or gap report
   carrying the old token cannot mutate the new cut.
5. Call `closeSource` with the matching token to close the model's source
   lifetime. Choose whether to retain the last completed cut. A closed
   activation cannot accept input or restart.

A token owns a small unique marker and retains no Feed, model or event. Two
different models can use the same numeric ordinal without sharing authority.
The ordinal identifies the activation for diagnostics. The token itself
authorizes input, so a caller must not substitute a later token for delayed
work. Moving the model transfers its token authority. The moved-from model
refuses that old token. Assignment of a moved order book also retires the
previous destination token.

## Read the qualified cut

| Status | Interpretation |
| --- | --- |
| `UNTRACKED` | Legacy input and reads are available without a source continuity claim. |
| `RECOVERING` | New-epoch input is allowed. Completed values are provisional until replay is confirmed. |
| `CURRENT` | The last completed cut has been qualified for this epoch and selection. A later pending batch cannot appear in it. |
| `GAP` | Input is fenced. Any retained cut describes completed data before the gap. |
| `CLOSED` | Input is terminally fenced. Any retained cut is historical. |

Use `getSourceSnapshot` to obtain data and qualification at the same serialized
observation. A copied cut keeps its original status and values after later
updates, gaps or destruction. In tracked mode, the plain snapshot getters
refuse a noncurrent cut. Once tracking starts, the original untagged
`processEvents` refuses input. An order-book `clear()` clears data and fences
tracked input until a fresh epoch. A time-series selection change retires
unfinished fragments and needs new replay evidence while the same source is
receiving. Its range revision independently fences older batches.

## Feed evidence

The model does not attach to Feed or infer a transport epoch. Endpoint
`CONNECTED` only describes the receiving session. A qualified
[HISTORY observation](../subscriptions/HISTORY-RECEIVING.md) can supply
replay evidence when its source epoch, accepted selection and completed
ranges match the data the application forwarded. The observation is an
AS-OF cut. Reading it after an arbitrary callback does not identify that
callback's original source. The application must preserve the identity at
its own delivery boundary or keep the model in recovery until it can
establish that identity and coverage. Use a higher model ordinal if a new
subscription owner restarts its transport counter.
