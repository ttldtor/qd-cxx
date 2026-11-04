# Schedule rules example

[Русская версия](README.ru.md). English is authoritative.

This complete console client creates an explicit calendar context, obtains `XNYS()` through
`Schedule::getInstance` and prints its owning name/timezone identity. The fixed baseline result is:

```text
NYSE    America/New_York
```

The separator is one tab. The fixed example's metadata is ASCII; applications retain the native
owning UTF-16 results or encode them for their own presentation layer.

Build with `ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON` and `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON`, then build
target `qdcxx_schedule_example`. See [C++ build instructions](../../docs/CPP-BUILD.md) for the selected
compiler/profile. No Java, date/tz SDK or runtime connection is needed by this parser example.
An installed application links `qdcxx_schedule` after
`find_package(qdcxx CONFIG REQUIRED COMPONENTS schedule)`.

Run `qdcxx-schedule-example --run` to execute. No arguments and standalone `--help` print identical
English help; `--help-ru` prints Russian help. Help creates no context and starts no parsing work.
The executable accepts no external data/credentials. `--run` uses embedded defaults and the normal
`ScheduleLimits`, allocates only owned in-memory objects, and performs no file/network activity.
Exit 0 means success/help, 1 means operational/output failure, and 2 means invalid arguments.

[main.cpp](main.cpp) contains the complete source and essential error handling. The
[application guide](../../docs/SCHEDULE-RULES.md) explains named portions, typed defaults, limits,
ownership and concurrent generation updates. Day/Session instant queries and filters are separate
features; the example demonstrates parsed schedule rules and metadata.
