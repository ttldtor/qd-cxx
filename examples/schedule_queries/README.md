# Schedule query example

[Русская версия](README.ru.md). English is authoritative.

Build with `ORG_TTLDTOR_QDCXX_BUILD_SCHEDULE=ON` and `ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON`,
then build target `qdcxx_schedule_queries_example`. No runtime SDK or download is needed.
The example explicitly initializes the controlled data, queries XNYS on2024-11-04 and prints its
Day boundaries and ordered Session records. It terminates without a worker or manual window closure.

```powershell
.\build\examples\schedule_queries\qdcxx-schedule-queries.exe --help
.\build\examples\schedule_queries\qdcxx-schedule-queries.exe --run --data C:\sdk\share\qdcxx\time-zone-data\tzdb-2026f
```

On Linux use `./build/examples/schedule_queries/qdcxx-schedule-queries --run --data /opt/sdk/share/qdcxx/time-zone-data/tzdb-2026f`.
No arguments, `--help` and `--help-ru` perform no file/native work. The only workload syntax is
`--run --data ABSOLUTE_DIRECTORY`; the fixed date/venue and raw-reference instant are deterministic
example inputs. The exact versioned installed data subtree can be copied beside an application or
relocated with its SDK prefix; pass the resulting absolute directory explicitly. Windows uses wide
native path arguments. The backend validates exact bytes, not arbitrary local timezone files.
Exit0 means help/success,1 means data/query failure,2 means invalid arguments.

For a separate installed consumer, use `find_package(qdcxx REQUIRED COMPONENTS schedule time_zone_data_backend)`
and `target_link_libraries(app PRIVATE qdcxx_schedule)`. Compile this same `main.cpp` with C++23;
MSVC also needs `/utf-8`. No private header, Date type or source-tree directory is required.
See [ownership/ranges](../../docs/SCHEDULE-QUERIES.md) and [dependency/data flow](../../docs/TIME-ZONE-DATA.md).
