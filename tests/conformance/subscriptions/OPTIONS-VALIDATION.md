# Endpoint options validation

[Русская версия](OPTIONS-VALIDATION.ru.md). English is authoritative.

## Scope and supported behavior

The native [`EndpointOptions` API](../../../include/qd/client/endpoint_options.hpp)
provides immutable owned per-endpoint schema/settings snapshots in `qdcxx_client`.
It copies explicit inputs once, preserves schema provenance and has no JSON,
environment, file, Java-property or process-default resolver. The executing
application remains credential-free numeric TCP STREAM. Stored TLS/authentication,
retry and DNS-host handoff values do not activate these facilities.
See the [native guide](../../../docs/SUBSCRIPTIONS.md#endpoint-options) and
[configuration reference](../../../docs/CONFIGURATION.md) for units, read timing,
ownership and selected-driver restrictions.

Generic snapshot validity is separate from sufficient current-driver work guards.
The driver rejects a selected record with fewer than twice its local field count
in dispatch field credits before its own I/O. Only an actual dispatcher WORK_LIMIT
on the first call of a fresh turn before any successful step becomes visible
failure plus same-credit abort. Successful-step/residual-turn yield, native
YIELD/IDLE and ordinary waiting remain ordinary progress. No configured credit is
raised during stop, drain, failure or cancellation; the old overload retains its
original behavior. This is not a universal minimum for `TcpRuntime` or generic
partial-progress/handoff settings.

## Executed checks

Qualification date: 2026-10-31.

| Profile | Windows | Linux |
| --- | --- | --- |
| Qualified build | MSVC 19.42.34444 x64 Release | GCC 13.3 Debug ASan+UBSan |
| Runtime/client/connect ON, JSON OFF | 52/52 CTest passed | 52/52 CTest passed |
| Runtime/client/connect ON, cached JSON ON | 53/53 CTest passed | 53/53 CTest passed |
| Focused immutable options | 4 cases / 55 assertions passed | 4 cases / 55 assertions passed |
| Snapshot driver scenarios | 6 cases / 176 assertions passed | 6 cases / 176 assertions passed |
| Separate threaded snapshot test | Actual threaded case included above | TSan: 1 case / 9 assertions passed |

Linux ASan+UBSan used `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. LeakSanitizer was disabled;
these results do not claim LeakSanitizer coverage. The TSan case copied, read and
released distinct snapshot handles across four actual threads.

[Options tests](../endpoint_options_tests.cpp) cover independent endpoints, copied
schema provenance, input/snapshot lifetime, provider shared ownership without any
credential callback invocation, static secret-free errors, TLS identity shape and
explicit applicable bounds, invalid enums/units, arithmetic overflow and truthful
handoff refusal. Shape-valid synthetic PEM text is not a cryptographic trust test.

[Driver/CLI tests](../connect_tests.cpp) cover both minimum directions and actual
effective operation caps 1, 2 and 3; successful-step/residual-yield continuation;
selected-record preflight; whole-batch pristine WORK_LIMIT followed by visible failure, bounded abort
and discard; snapshot callback graceful stop/throw; queued 16-entry command
cancellation; caller/callback quiescence; and configured native read inactivity.
The final cases returned with local/native terminal evidence and owned peer
retirement. CLI/native effective values, all five new singleton flags, numeric
overflow, unsupported TLS and synthetic secret redaction were checked.
[Help/process checks](../connect_cli.cmake) retain noargs/standalone-help equality,
stdout/stderr/exit behavior and invalid-input rejection; the process runner also
covers interruption and output failure. No external provider endpoint is required.

## Production packages and dependency closure

With `BUILD_TESTING=OFF`, each platform passed these configure/build/install,
relocation and independent consumer checks:

| Package profile | Files per installed/relocated package | Result |
| --- | --- | --- |
| CLIENT ON; runtime/connect/JSON OFF | 40 | Relocated options consumer passed |
| CLIENT/runtime/connect/tools/JSON OFF | 37 | Relocated pure Quote/Profile consumer passed |
| Runtime/client/connect ON; JSON OFF | 45 | Relocated generic/Quote/Profile TCP consumer passed |

Original/relocated file sets and SHA-256 values matched in all six packages.
Final options headers matched the qualified public source. Actual configure traces
and successful build graphs showed no optional transport/TLS/JSON SDK discovery,
fetch or link closure in the OFF profiles; neither OFF profile enabled C. Enabled
runtime packages introduced no OpenSSL or JSON discovery/export.
The [options consumer](../endpoint_options_consumer/main.cpp) includes the complete
headers and `main`; its [CMake project](../endpoint_options_consumer/CMakeLists.txt)
links `qdcxx_client qdcxx_quote` from the installed package without runtime/JSON.
It performed no network work and printed:

```text
Independent endpoint snapshots: 7301, 7302
```

The existing [subscription consumer](../connect_consumer/CMakeLists.txt) additionally
exercised installed generic and complete Quote/Profile applications with controlled
TCP peers. These are explicit package/runtime checks, not public-peer acceptance.

## Style, documentation and limits

The pinned Uncrustify 0.83.0 → clang-format 20.1.8 Format/Check pipeline and fixed-point
comparison passed for ten changed C++ files, including the complete consumer.
Doxygen 1.18.0 generated the new API pages with zero warnings; parameter/result,
provider lifetime/thread rules and the `CALLER_DRIVEN_TCP` enumerator documentation
were inspected. The final enumerator-comment relocation preserved all C++ tokens.

There is no TLS handshake/trust/authentication, reconnect or DNS-driver integration
qualification here. TLS local structural checks do not establish SAN/chain validity;
credential acquisition belongs to the verified TLS adapter and CONFIG never invokes
it. Arbitrary provider state/captures/resources and callback/destructor work are
caller-owned exclusions. There is no secure-erasure, process-RSS or hard elapsed-time
claim. General properties loading/catalogue/live updates and full subscription
acceptance remain separate work. See the [usage guide](../../../docs/SUBSCRIPTIONS.md)
for output, exit codes and preserved native close evidence.
