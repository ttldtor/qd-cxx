# Locked qds-tools inventory and implementation priorities

[Русская версия](QDS-TOOLS.ru.md). English is authoritative.

QDCXX-020; source inspection of QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`.
All runtime details below are source-derived, not newly measured. [Validation](QDCXX-020-VALIDATION.md).

## Enumeration, aliases and packaging

The tracked production service registry contains **20** AbstractTool providers; the table below
accounts for all 20. Reproduction: enumerate tracked `*META-INF/services/*AbstractTool*`, inspect
every provider and `@ServiceProvider`/AbstractTool subclass, then reconcile the two tracked
`META-INF/qds/qdshelp.txt` resources and packaging POMs. Test registries are not production commands.
The only tracked AbstractTool registry is in qd-tools. qd-nio adds connector help, not another command.

[Registry](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/resources/META-INF/services/com.devexperts.qd.tools.AbstractTool),
[Tools](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Tools.java),
[Help](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Help.java),
[help articles](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/resources/META-INF/qds/qdshelp.txt),
[qds-tools POM](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qds-tools/pom.xml).

Tools matches each class's simple name case-insensitively; there is no separate registered alias
list. Thus `connect` is a spelling of Connect, not a second utility. All 20 rows inherit that rule.
Class-specific `main` methods dispatch through Tools. `+` separates tools in one JVM; Batch is a
help article, not a provider. Tools/Contents/All, Tape, File, Address, Scheme, Filters, Stripers,
Time format, Text format and connector articles are not additional utilities.
Eleven provider names have dedicated articles: Help, Connect, Compare, Dump, Feed, Multiplexor,
Post, SchemeDump, Time, NetTest, Instruments. Other providers use reflective ToolSummary fallback.

qds-tools is a shaded JAR: Main-Class Tools, merged service resources/help; shaded qd-logger,
qd-nio, qd-stripe, qd-tools, dxfeed-tools. Manifest Class-Path names qds.jar, qds-file.jar,
qds-monitoring.jar, dxlink-websocket.jar, config.jar. This is not proof those files are installed.
No qds-tools shell/batch launcher was found in tracked paths; supported source entry is
`java -jar qds-tools.jar <command> ...` with its dependencies, or Tools on a complete classpath.
The packaged JAR was not built or launched here.

Instruments delegates IPF transformation, OCC parsing and serving to dxfeed-tools/dxfeed-ipf;
it is already counted, not an extra publisher. The additional public main
[MICBuilder](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-tools/src/main/java/com/dxfeed/ipf/tools/MICBuilder.java)
is an unregistered maintenance entry: one directory of ISO MIC CSV versions → database text on
stdout for MIC source regeneration; no QD schema/options, synchronous IO/usage error path.
Defer it as an upstream database-maintenance concern (N); no qdcxx equivalent proposed.
Other dxfeed-tools sources provide libraries rather than registered commands.
qd-sample TestThroughput and dxfeed-samples fileparser/connect/UI launchers are standalone samples,
not Tools aliases; dxfeed-viewer/plotter are separate products, outside this inventory boundary.

## Shared reading key

D = effective QD default/endpoint DataScheme, selected through QDFactory or QDEndpoint and service
providers/properties; it does not imply SampleScheme or full DXFeedScheme is always installed.
No row promises a single environment-independent default scheme. No option means the option is
not selected, unless a default is stated. Detailed transitive properties/error edge cases not
listed here remain **unknown/unmeasured**; links identify the exact getOptions/execute/lifecycle
source for each row. The table inventories exposed option names, not a normative grammar.

Common options where explicitly listed: --log/-l, --stat/-s (period, seconds by default),
--name/-n; management = --html-management/-h and --rmi-management/-r.
--fields filters suffixes, --file uses [filter@]name for startup storage, --write is a storage
write period. These storage options are not aliases of Connect --tape. Option support can depend
on EndpointOption.isSupportedEndpointOption. See
[Options](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Options.java) and
[option sources](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools).
-Dname=value modifies JVM properties during parsing; all tools parse before Services.startup,
then execute, wait for connectors/threads and close declared resources. Normal invocation ends
with System.exit(0), caught failure with 1; swallowed/logged errors need not produce failure.
Empty Tools invocation prints usage to stderr and returns; command no-args can execute work.
Do not transfer this behavior to qdcxx help.

Address grammar is shared through
[MessageConnectors](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/qtp/MessageConnectors.java):
[filter@]address[properties], grouped (address1)(address2), legacy slash-separated alternatives;
property groups contain key=value pairs. Client/server sockets, file/tape, NIO and other installed
factories resolve address-specific forms; IPF URLs and HOCON are separate grammars.
File formats/defaults/options are separately evidenced in [TAPE-FILES](../../spec/protocol/TAPE-FILES.md).
Reconcile help with source: Compare article's --alias/1/2 is stale relative to --names and
compare1/compare2; Scheme's positional scheme claim and NetTest's SampleScheme example do not
match current argument parsing. Tape article's old sidecar defaults are not authoritative over
FileWriterParams/TimestampsType. These are source discrepancies, not new runtime observations.

## Complete discovered command mapping

Priority P0 = immediate offline enabler; P1 = first real-event consumer/publisher tools;
P2 = distribution once subscriptions/core/transport exist; P3 = later diagnostics/IPF/load.
Order is proposed, not authorization. W = preserve selected wire/data semantics after evidence;
F = preserve selected input data/file semantics; C = native qdcxx CLI/configuration proposal;
N = no direct port. **Every W/F row also proposes native names/CLI/output/errors (C)**,
not Java argument, address, properties, exit-code or output-byte parity.

| Command / pinned entry source | Inputs | Purpose, options/defaults, scheme/dependencies, lifecycle | Proposed equivalent / priority / compatibility |
| --- | --- | --- | --- |
| [Compare](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Compare.java) | `address records symbols [date-time]` | Two sources → matching/delay/drop statistics; ticker; --collector/-c, --other-address/-A, --other-collector/-C, --other-record/-R (at least one), required --stat; --fields, --names (compare1/compare2), --log, management. D; connector wait + endpoint close. | P3 qdcxx-compare; W |
| [Connect](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Connect.java) | `address records symbols [date-time] OR address subscription-address` | Receive records → screen text by default, optional tape. ticker; --collector/-c, --stripe, --sticky, --tape/-t, --fields/-f, --quiet/-q, --stamp/-S, --top/-T, --name (connect), --stat, --log, management. D; connector wait + processor/endpoint close. | P1 qdcxx-connect; W |
| [Dump](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Dump.java) | `address [subscription]` | All received data AND subscription messages → text/tape; no collector option. --quiet, --tape, --stamp, --name (dump), --stat, --log, management. D; connector wait + endpoint/writer close. | P1 qdcxx-dump; W |
| [Feed](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Feed.java) | `feed-address agent-address` | Wildcard feed → downstream agents; collector all. --collector, --stripe, --sticky, --file/-f, --write/-w, --raw/-R, --subscribe/-S, paired --delay/-d and --buffer/-b, --name (feed), --stat, --log, management. D; waits for connectors. subscribe on raw uplink can grow subscription indefinitely. | P2 qdcxx-feed; W |
| [Forward](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Forward.java) | `address; required --forward/-F service-names address` | RMI request forwarding, not market-record publication. --name (forward), --log. D endpoint; RMI server/client, wait for connectors. | Deferred RMI / отложено RMI; W |
| [FileAnalysis](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/analysis/FileAnalysis.java) | `file-or-url` | Binary tape → structure/symbol/compression analysis report, stdout unless --out/-o. --symbols/-s, --group/-g properties file, --compression/-c glob, --type-override/-t. FileReader max speed, BINARY only; parser-owned diagnostic schema; synchronous completion. | P0 bounded qdcxx-tape-inspect, P3 analysis / ограниченный qdcxx-tape-inspect, затем анализ; F |
| [GCTimeTransformer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/GCTimeTransformer.java) | `file` | GC log → absolute-time text; --time/-t start, --output/-o (gctimes.txt). No record scheme; synchronous file processing. | Not applicable JVM diagnostic / неприменимо: диагностика JVM; N |
| [Help](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Help.java) | `[article]` | Articles/reflective summaries → stdout; --width/-w default 120, minimum 10. No data scheme required by purpose; construction still uses AbstractTool. Synchronous. | P0 integrated --help / встроенная --help; C |
| [Instruments](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Instruments.java) | `option-driven; no positional args` | IPF/OCC FIXML/live sources → transform/IPF/server. --bizdate/-b, --osi/-o, --generate/-g (100000), --read/-r, --products/-p, --transform/-t, --merge/-m, --exclude/-e, --check/-c, --sort/-s, --write/-w, --script/-i, --performance/-n, --log. IPF schema, not QD record scheme. Server ':port[opts]' implies merge, restricts options; self-wait + server close; file mode returns. writeEmptyResults property defaults false. | P3 qdcxx-instruments, separate IPF / отдельный IPF; F |
| [Invoke](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Invoke.java) | `address service[@id] method[:return-type] [arg[:type] ...]` | RMI invocation → logged result/exception; auto-detect argument types, default return type void, no tool-specific options. RMI schema/marshalling; blocking request. Missing target or caught RMIException may return normally and thus dispatcher success. | Deferred RMI / отложено RMI; W |
| [Multiplexor](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Multiplexor.java) | `distributor-address agent-address` | Upstream data/downstream subscriptions ↔ collector routing. all; --collector, --stripe, --sticky, --file, --write, --delay, --drop/-P (1..100), --name (multiplexor), --forward/-F, --route/-R, --stat, --log, management; multiplexor.qd.subscribe.*. D; connector wait + owned resources close. | P2 qdcxx-multiplexor; W |
| [NetTest](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTest.java) | `p\|c address [address2 ...]` | Producer/consumer load → throughput stats. ticker, one instance, 100000 symbols, stats 10s; --collector, --stripe, --sticky, --symbols/-S, --connections/-C, --wildcard/-w, --name, --stat, --log, rate sequence (-r/--rate-limit). D + synthetic workload; starts side threads, no lifecycle hook override in NetTest itself: do not assume a bounded run. | P3 qdcxx-net-test; W |
| [Post](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Post.java) | `address; stdin text records` | Publish input records to uplink; --collector/-c default raw (ticker/stream/history alternatives), --name (post), --log. D, TextQTPParser/queue; join PostingThread. Not a deterministic event object generator. | P1 qdcxx-post; W |
| [Services](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Services.java) | `address` | RMI service advertisements → log. Only --log is exposed (name field is not in getOptions). D endpoint; wait for connectors. | Deferred RMI / отложено RMI; W |
| [SchemeDump](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SchemeDump.java) | `[records]` | Default scheme/filter → record/field/type/class text on stdout; no tool-specific options, default '*'. D; synchronous. Zero args performs work in Java. | P1 qdcxx-scheme-dump; F |
| [SubscriptionDumpParser](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SubscriptionDumpParser.java) | `files` | Collector-management binary dumps → subscription.txt by default. --columns/-c default sRa, --sort/-s none, --group/-g implies sort/columns, --output/-o. Dump metadata + D record lookup; synchronous. This is not a tape parser. | P3 qdcxx-subscription-dump; F |
| [SubscriptionDumpRepeater](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/SubscriptionDumpRepeater.java) | `address files` | Management dumps → reproduced subscriptions/load, optional received text/tape. all; --collector, --tape, --fields, --quiet, --stamp, --top, --name, --stat, --log, management, --filter/-F agent regex. D + dump parser; connector wait + resources close. | P3 qdcxx-subscription-replay; W |
| [TDP](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/TDP.java) | `files` | JVM full thread dumps → profile.txt; existing TDP profile → shortened file + "~". IOExceptions are printed and swallowed. --locks/-l, --topdepth/-t 50, --methoddepth/-m 50, --threshold/-h 5%, --count/-c, --state/-s, --name/-n, --output/-o; no record scheme; synchronous. | Not applicable JVM diagnostic / неприменимо: диагностика JVM; N |
| [Time](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/Time.java) | `no positional args; --request or --stat required` | Peer JVM time deltas → logs; --request/-R, --verbose/-v, --stat, --log, management. TimeSyncTracker, no record scheme; self-join (long-running). This is not a tape timestamp converter. | Deferred clock observability / отложено наблюдение часов; W |
| [Launcher](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/launcher/Launcher.java) | `config` | Experimental HOCON modules/endpointProperties/launcherConfig → managed services; --check/-c; module factories, Typesafe Config, monitoring/JMX/MARS. Scheme belongs to module/endpoint. Watcher join + close; source defaults startTimeout/pollPeriod 1000ms, moduleEventLogLimit 100000. | Deferred qdcxx service composition / отложена композиция сервисов qdcxx; C |

## Shared requirements and owner decisions

- Schemes/mappings: exact serial identities, loader-independent validated local models, requested
  layouts, name resolution and event conversion. XML/JSON import remains separate.
- Subscriptions/core: add/remove, wildcard, per-contract ticker conflation versus ordered stream,
  history indices/flags, aggregation and per-client isolation before Multiplexor/Feed parity.
- Protocol/transport: negotiation and subscription/data composition, reconnect/re-description,
  partial reads/writes, cancellation, deadlines, ownership and deterministic shutdown.
- Buffering: finite per-client/aggregate budgets, backpressure, slow consumer/drop policy, draining
  and counters; a Java delay option is not a complete resource contract.
- Operations: explicit configuration precedence, credential redaction, useful diagnostics, byte/record/
  queue/reconnect metrics, reliable exit codes and signal handling. MARS/JMX, HOCON, Java RMI and
  JVM-wide properties are dependencies to assess, not automatically portable APIs.

Approve the proposed native CLI/configuration/error/output policy separately for each scoped
utility, and any intentional wire/data deviation through the compatibility process. No drop-in
replacement is claimed. All qdcxx executables must follow no-args = standalone --help, stdout,
empty stderr, exit 0 on successful output, no work/file IO; retain output-failure errors.
The existing inspector's exact TSV/human formats remain untouched.
No networking library, threading model or runtime dependency is selected by this inventory.

For the first deliverable choose bounded qdcxx-tape-inspect (partial FileAnalysis/Connect-file use),
then full default Quote inspection, then bounded Connect/Post record/replay, then Feed/Multiplexor
with real subscription routing. File replay is a connector capability in Java, not an invented
registered Replay utility. Priority does not imply implementing every upstream option.
[Dependency order and one proposed next task](../NEXT-STEPS.md),
[real-event baseline](../REAL-EVENT-BASELINE.md), [Python removal](../PYTHON-MIGRATION.md).

## Coverage limits

No complete tools runtime/classpath was available in the inspected checkout (Tools.class absent).
Dispatcher/help generation can construct providers and the invocation path calls Services.startup;
no arbitrary classpath was assembled or services started merely to get help. Thus dispatcher and
per-command runtime help were not captured. Source articles/annotations/options were inspected
instead; packaged help visibility and optional external providers remain unmeasured.
Completeness means the locked tracked registry, shaded-module public entries and help/packaging
paths above, not every plugin a deployment could install. Detailed transitive connector defaults,
all Instruments script verbs and all configuration/error combinations need focused tasks before parity.
