# Configuration reference

[Русский](CONFIGURATION-REFERENCE.ru.md) · [Configuration architecture](CONFIGURATION.md)

Declaration entries retain known declared or source-anchored facts, including constraints, read timing and lifetime, with the qualification that they have not been independently revalidated. Pending semantic verification does not erase those facts; UNKNOWN is reserved for genuine gaps.

Use the subsystem pages to find a declared C++ field, its default, units, validation, read timing, mutability and failure behavior. Owner names repeat in different namespaces: use the linked source path to select the intended type. Stable CFG identifiers identify entries, not supported Java property names.

Coverage: 937 declarations from 67 public-header, tool and example sources. Of these, 585 have field-specific consumer semantics; 352 are separately classified declaration entries. The latter comprise 107 compositions, 86 work-credit inputs, 103 setting leaves, 8 explicit dependencies/data inputs, 13 fixed constants, 31 returned observations and 4 qualification shapes. UNKNOWN is an explicit information gap, not validation, support or a live-update promise. Constants, observations and qualification shapes are not user settings.

Scope is the declared native settings inventory. It is not an exhaustive list of CMake switches, every CLI spelling or Java QD properties. The [endpoint property reference](ENDPOINT-CONFIGURATION.md) covers supported textual aliases and precedence; the [schema-settings baseline](../spec/schema/CONFIGURED-EVENTS.md) covers observed Java precedence. Other Java setting families remain unimplemented or unresolved unless their component documentation says otherwise. A source declaration matching the reference does not prove runtime behavior; source contracts and actual execution evidence are distinct.

| Subsystem | Field-specific contracts | Declaration entries |
| --- | ---: | ---: |
| [Public API](configuration/api.md) | 113 | 49 |
| [Client composition](configuration/client.md) | 24 | 58 |
| [Collectors](configuration/collectors.md) | 23 | 21 |
| [Schemas and desired subscriptions](configuration/core.md) | 12 | 3 |
| [Encoding](configuration/encoding.md) | 7 | 0 |
| [Events and mappings](configuration/events.md) | 28 | 2 |
| [Controlled stream example](configuration/examples.md) | 4 | 5 |
| [Glossary values](configuration/glossary.md) | 4 | 0 |
| [Instrument profiles](configuration/ipf.md) | 85 | 22 |
| [Multiplexing and demand](configuration/mux.md) | 34 | 33 |
| [Protocol and tapes](configuration/protocol.md) | 75 | 38 |
| [Replay](configuration/replay.md) | 5 | 3 |
| [Schedules](configuration/schedule.md) | 22 | 2 |
| [JSON schema loading](configuration/schema.md) | 5 | 3 |
| [Timezone data](configuration/time.md) | 11 | 0 |
| [QTP inspector](configuration/tools-qtp-inspect.md) | 17 | 1 |
| [Receiving tools](configuration/tools-subscribe.md) | 30 | 16 |
| [Tape inspector](configuration/tools-tape-inspect.md) | 31 | 4 |
| [Tape writer](configuration/tools-tape-write.md) | 8 | 1 |
| [Transport and recovery](configuration/transport.md) | 47 | 91 |

A missing initializer is reported as such; it does not mean an indeterminate value is safe to consume. Zero, absent and empty retain their component-specific meaning. Defaults are source values, not performance recommendations or protocol ceilings. Work credits, retained-capacity bounds and returned requirement measurements remain separate.
