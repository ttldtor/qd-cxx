# Practical guides {#practical-guides}

[Русская версия](guides.ru.md) · [Documentation home](index.md)

| Task | Guide | Choose this when |
| --- | --- | --- |
| Read binary tapes | [Tape inspection](TAPE-INSPECT.md) | Input is an existing file; select a supported finite source profile |
| Write a tape | [Tape writing](TAPE-WRITE.md) | You already own validated records and an explicit schema |
| Inspect protocol frames | [QTP inspection](QTP-INSPECT.md) | You need framing and record diagnostics offline |
| Inspect Profile events | [Profile inspection](PROFILE-INSPECT.md) | You need the selected Profile mapping |
| Configure records | [Schemas and settings](CONFIGURATION.md) | Built-in Quote/Profile defaults do not describe your input |
| Export a Java schema | [Schema exporter](../tools/schema-export/README.md) | You need an exported schema from locked Java QD |
| Receive controlled STREAM data | [STREAM subscriptions](SUBSCRIPTIONS.md) | A controlled plain-TCP peer and exact selectors meet your needs |
| Compose a STREAM application | [Programmatic example](../examples/subscriptions/README.md) | Your application owns runtime progress and callbacks |

The STREAM program has a finite cutoff and uses plain TCP. The separate TLS runtime has its own
[trust, identity, credentials and lifetime contract](../spec/transport/TLS-RUNTIME.md); selecting TLS
in a stored endpoint does not turn the plain program into a TLS client. Reconnect and a complete
shared subscription facade remain separate capabilities. Check [current support](CURRENT-STATE.md)
before choosing an integration.
