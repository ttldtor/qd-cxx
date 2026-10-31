# Controlled STREAM client validation

[Русская версия](CONNECT-VALIDATION.ru.md). English is authoritative.

The [controlled client](../../../docs/SUBSCRIPTIONS.md) is qualified against owned
synthetic TCP peers and the existing complete standard schemas. No public feed,
credentials or external service is required for these checks.

## Executed profiles

- Windows x64 MSVC 19.42 Release: **47/47 root CTests** pass. The direct STREAM
  application test has **10 cases / 177 assertions**.
- Linux x86-64 GCC 13.3 Debug: **47/47 root CTests** pass with C and C++ ASan/UBSan
  instrumentation, including the qualified C SDK. Main LeakSanitizer is OFF;
  ASan and UBSan halt-on-error options are explicit. The direct STREAM test has
  **10 cases / 171 assertions**; platform-specific retirement checks differ.
- The focused `connect_stream`, `connect_cli_contract` and `connect_process`
  groups pass on both platforms. The native process tests launch real children,
  exercise output failure and retire their owned peers/processes automatically.

## Observable behavior

- Actual TCP carries readiness, full descriptions, subscription output and data.
  Fragmented input, exact UTF-16 selection, initial add, full-set removal and later
  withdrawn data are checked independently of packet boundaries.
- Full standard Quote and Profile reach their existing prepared mappings.
  Generic rows retain extra fields, aliases, nullable text, opaque bytes and raw
  provenance metadata. Prepared summaries explicitly cover fewer fields.
- A one-operation owner turn forces accepted pending-event work to yield. The
  next turn stages/releases it before removal or another source poll; selected
  records are still delivered. Callback abort is observed before new admission.
- A target of one with a three-record admitted prefix documents the admission
  target semantics: the eligible prefix finishes rather than being silently cut.
- Clean early EOF and truncated-frame EOF remain distinct from an intentional
  cutoff. Callback sink failure is not retried; both owner domains retire.
- Empty feed, unreached target, interruption and native close failures preserve
  separate application/local/transport evidence and defined exit precedence.
- No-args/standalone help equality, Russian UTF-8 help, invalid arguments and
  incompatible profiles are tested before work. Real broken stdout/stderr paths
  return failure. POSIX process tests also deliver an actual SIGINT; this is not
  a claim of native Windows console-control qualification.
- The tracked programmatic example executes against the controlled peer.
  An initially occupied published-handle slot is rejected; published control
  lifetime and detached terminal observations follow the documented contract.

The Windows runtime may retain `READ_FAILURE/socket-read/1236` on an owned close.
Only that exact documented abort variant is accepted as an expected failure
outcome in the relevant tests, with exit 1 and all cause/count checks retained.
It is not reported as happy-path success. Independent Quote/Profile data-target
cases also exercise exit 0. Loopback allowances are not a latency benchmark.

## Packaging, dependency scope and documentation

Both production-only builds and relocated installed consumers pass. Each enabled
package contains **40 files**, with every file hash preserved after relocation
through custom `lib64` and `public include` layouts. Independent consumers use
installed headers/targets for generic runtime/dispatch and prepared Quote/Profile;
the standalone callable example builds too. Consumers do not fetch SDKs or doctest.

CLIENT/RUNTIME/CONNECT OFF remains the default. Pure installed consumers on both
platforms expose event mapping/Quote/Profile without runtime/client/SDK/doctest/
Threads dependencies. Enabled CONNECT without its explicit prerequisites fails
configuration. The actual private default/negative checks run with contained paths.

Pinned Uncrustify 0.83.0 then clang-format 20.1.8, check/idempotence and real formatter
scope tests cover the new example/tool roots and adjacent/protected refusals.
Doxygen 1.18 emits zero warnings; generated driver/options/result/sink pages are
inspected for parameters, ownership, lifetime, separate outcomes and blocking limits.
Fresh actual Java control Capture/Verify accepts both native captures; the existing
344-byte input and 4778-byte callback output are unchanged, with source-bound
provenance updated for the new root CMake configuration.

Local graceful completion, deliberate TCP abort, sink flush and peer acknowledgement
remain separate facts. TLS, authentication, reconnect, wildcard, shared MUX and full
subscriptions are outside this profile. No hard preemption, process-RSS bound,
exactly-once or complete-schema/tape roundtrip claim follows from these results.
