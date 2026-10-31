# Requirements and unresolved questions

[Русская версия](REQUIREMENTS.ru.md). English is authoritative. [Knowledge index](README.md).

The confirmed goal is a QD/qd-cxx knowledge base; concrete infrastructure remains separate.
**Explanation goal** identifies what this knowledge base should clarify. **Discussion topic** records
an idea for investigation, not an approved product capability. **QD source fact** records inspected
locked Java behavior; **native contract** and **existing architectural constraint** reference existing
documented boundaries. **Proposal/question** is undecided. The identifiers connect these distinct
categories to evidence; they do not imply release, new feature approval or owner acceptance.

| ID | Requirement or question | Classification and present boundary |
| --- | --- | --- |
| K-01 | Keep protocol and core independent of a particular transport. | **Existing architectural constraint; native contract.** The [live session](../../spec/subscriptions/LIVE-SESSION.md) accepts bytes and explicit security decisions without owning sockets, clocks or threads. The optional TCP adapter is one separate profile. **QD source fact:** the [connector inventory](../upstream/QDS-TOOLS.md) includes socket, file/tape and other installed factories; Java QD is not classified as TCP-only. |
| K-02 | Support reasoning about multiple channels. What does one channel identify? | **Discussion topic; proposal/question.** Logical stream, contract family, configured source, subscription group and transport connection are possible meanings, not synonyms selected by this ledger. Their cardinality, lifecycle and reconnect mapping require a decision. Existing independent sessions do not establish a general channel API. |
| K-03 | Explain event-data routes and subscription-demand routes separately. | **Explanation goal; QD source fact.** Feed/Multiplexor in the [tool inventory](../upstream/QDS-TOOLS.md) describe data and subscription routing. **Native contract:** desired interest, emitted demand and delivery are distinct in [ADR-0005](../decisions/ADR-0005-live-subscriptions.md). A local command or emitted message does not prove remote application or the full reverse path. |
| K-04 | Do not infer event origin from a symbol or a schema-local numeric ID. | **Native contract; question about origin.** [Desired state](../../spec/subscriptions/STATE.md) uses record provenance and exact UTF-16 symbol; directional sender IDs belong to protocol binding. Neither identity is documented as an authenticated producing venue, process or route. The meaning of origin remains to be specified. |
| K-05 | Distinguish a known local input from a complete upstream chain. | **Discussion topic; inference; proposal.** An application may know its configured immediate ingress. That fact alone cannot establish every preceding hop or the original producer. Whether and how to expose local ingress, partial routes and unknown segments is undecided; this is not an existing route-metadata API claim. |
| K-06 | Keep event identity, producer identity and route identity conceptually separate. | **Proposal.** Their scopes, lifetime, uniqueness, fan-in/fan-out and relation to reconnect must be decided before any public types or wire fields. No source-ID bit packing, global event ID, deduplication rule or hop encoding is selected. Existing schema/epoch/membership tokens retain their current meanings. |
| K-07 | Define the trust, privacy and cost of any route metadata. | **Questions.** Who asserts a hop, what is authenticated, what may be omitted/redacted, and how are loops, truncation and unknown segments represented? What finite byte/work limits and retention apply? No complete-route, authenticity, overhead or latency guarantee follows from a proposed field. |
| K-08 | Preserve required HISTORY behavior without requiring Java's matrix implementation. | **Existing compatibility direction; QD evidence; native implementation pending.** [Architecture](../ARCHITECTURE.md) selects observable compatibility rather than class-hierarchy copying. Existing [Java captures](../../tests/fixtures/subscriptions/CAPTURES.md) cover bounded snapshots, raw bounds, transactions and removals. They do not choose a C++ matrix or establish a native HISTORY collector. Any simpler representation still needs the selected observable traces. |
| K-09 | Preserve schema provenance and generic records across configuration and binding. | **Accepted architectural constraint; native contract.** [ADR-0003](../decisions/ADR-0003-configurable-schemas-and-event-mappings.md) separates source resolution, immutable local schemas, directional binding and event projection. Equal numeric IDs or shapes do not make fresh contexts interchangeable. Provenance here concerns schema identity; it is not event-route provenance. |
| K-10 | Keep ownership, failure publication and resource bounds explicit. | **Native contract.** [Subscription contracts](../../spec/subscriptions/CONTRACTS.md) require whole-candidate state mutation, owning queued values and explicit work/peak budgets. [Live admission](../../spec/subscriptions/LIVE-SESSION.md) publishes no malformed-frame prefix. Logical storage bounds do not imply process-RSS or latency guarantees. Future route/channel state must be evaluated against these boundaries. |
| K-11 | Make configuration instance-local and evidence-based. | **Existing architectural constraint.** [Configuration requirements](../CONFIGURATION.md) distinguish sources, precedence, types, scope and read timing. No universal CLI/file/environment precedence is assumed. General configuration loading and proposed channel/route settings are not implemented by this ledger. |
| K-12 | Choose delivery semantics from an explicit contract family and profile. | **Native contracts with measured limits.** [STREAM](../../spec/subscriptions/STREAM-COLLECTOR.md) preserves selected record order; [TICKER](../../spec/subscriptions/TICKER-COLLECTOR.md) keeps latest state with explicit retention and local views. Java HISTORY observations remain a separate evidence level. Decoding, collector delivery, callbacks and upstream aggregation must each be qualified at their own boundary. |

## Decisions still needed

Resolve channel meaning and its relation to connections before proposing a channel interface. Resolve
what “origin” must explain before selecting producer/route/event identities. Then decide whether local
diagnostics, optional application metadata or a compatibility extension can meet that need, including
partial knowledge, trust and finite costs. These are alternatives for evaluation, not an approved design.

Concrete gateways, multiplexor deployment, hostnames and network topology are outside this ledger.
The [decision trace](DECISION-TRACE.md) identifies existing proof and missing evidence for each entry.
