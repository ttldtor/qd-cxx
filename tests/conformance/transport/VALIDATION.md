# Runtime profile feasibility validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

Scope: [ADR-0006](../../../docs/decisions/ADR-0006-live-runtime.md) and [T01–T14](CONTRACTS.md).
This result selects feasible SDK mechanisms and explicit integration requirements. It does not
implement or qualify a production runtime/session/TLS/auth client.

## Final-source native profiles

| Profile | DNS project | TCP/TLS project | Instrumentation |
| --- | --- | --- | --- |
| Windows x64, MSVC 19.42.34444, Release, Ninja | CTest 1/1 | CTest 2/2 | Native Release; existing default `/MD` consumer preserved. |
| Ubuntu 24.04 x86-64, GCC 13.3, Debug, Ninja | CTest 1/1 | CTest 2/2 | ASan+UBSan, `detect_leaks=0:halt_on_error=1`, UBSan halt; DNS C SDK and C++ runner instrumented, separately built OpenSSL SDK uninstrumented. |

The [DNS lock](runtime-feasibility/dns/dependencies.lock.json) binds the original c-ares 1.34.9 archive
and the qualified `1.34.9+qdcxx1` patch; replay with zero fuzz reproduced all 532 source-file hashes.
The upstream original was preserved separately. The [TCP/TLS lock](runtime-feasibility/tls/dependencies.lock.json)
binds Asio 1.38.2 and OpenSSL 3.5.9. OpenSSL was source-built static with assembly and
`no-autoload-config`; the exported macro and compile/runtime versions were checked.
Source/archive hashes and published checksums were verified; independent PGP verification was not performed.

## Observations

- DNS has eight invalid-configuration rejections and 24 wire scenarios per native profile:
  twenty pending cancel/reuse, two pending channel destruction, and two receive-quota/resume cases.
  Owned UDP/TCP peers receive the exact `hold.invalid.` question before retirement. Every expected
  completion occurs once on the owner, with `ARES_ECANCELLED` or `ARES_EDESTRUCTION`, zero retained fds
  and no later callback during the finite observation. The stronger destroy boundary also uses SDK
  source/API evidence; a short observation alone is not a proof about all future time.
- UDP sends six nonmatching transaction IDs and then NXDOMAIN: at most two native reads/two datagrams
  per SDK turn, three quota yields, then one `ARES_ENOTFOUND`. TCP retains a partial response across
  seven-byte reads and completes once after resuming. These are controlled finite workloads, not a
  sustained hostile-network or universal latency benchmark.
- The qualified patch rejects absent/excessive/zero server configuration before channel allocation,
  removes system/env/NSS and hostname/default-server fallback, and returns `ARES_ENOTIMP` for reinit.
  Adversarial child `RES_OPTIONS=use-vc` did not change the explicit UDP scenario. A wrong stock SDK is
  rejected; the no-event-thread channel does not own a DNS worker or configuration monitor.
- TCP connect proof separates immediate cancellation races from genuinely pending connects. The
  512-connection loopback backlog fixture retained and aborted 312 pending operations on Windows and
  510 on Linux; every submitted completion drained. Idle reads and writes to a nonreading peer were
  incomplete before cancellation; a partial write does not imply peer acknowledgment.
- TLS positive cases negotiate 1.2 and 1.3 with DNS SAN/SNI and IP SAN identity. Unknown issuer,
  wrong hostname and expired certificate fail with X509 codes 20, 62 and 10; failed handshakes do not
  report a negotiated session. Stalled handshake and graceful shutdown are cancelled by closing the
  lowest layer, and handlers drain before stream/context/provider/library-context/host destruction.
- The tested TLS contexts enforce a 64 KiB certificate-message cap, verification depth 4, explicit
  17 KiB Asio record buffers, and disabled record/certificate compression and renegotiation. The full
  production key/group/SPKI/trust admission policy and SDK memory/work allowance remain implementation
  requirements; ordinary RSA-2048 certificates do not prove every oversized or hostile-key rejection.
- The Windows IOCP timer helper is visible in measured process-thread counts 4 → 5 → 4; Linux epoll
  remains 1 → 1. These counts describe those processes, not universal OS baselines. Stop is requested
  from a serialized completion; remaining completions are pumped before external host destruction.
- Windows `/MD` and `/MT` probe consumers both link/run against the same static `/MT /Zl` OpenSSL SDK.
  The upstream configuration deliberately suppresses a specific RTL reference; `/MD` imports were
  inspected and no OpenSSL DLL was required. No root CRT policy or global environment was changed.
- No-argument/`--help` output is equal, nonempty, exit 0 and empty stderr. Invalid arguments and missing
  fixtures fail. Linux `/dev/full` verifies nonzero help-output failure. Help does not generate keys,
  open test sockets or require fixture inputs. The DNS executable is an internal no-argument CTest runner.
- Both standalone projects configure/build with their default OFF and an absent SDK; compiler/SDK
  discovery and network acquisition are skipped. This does not prove future production options, which
  do not yet exist. Ordinary root source/targets were not changed by this decision.

## Source facts, constraints and pending work

Stock Asio DNS uses a blocking resolver worker; stock c-ares event-thread destruction can join system
configuration work; stock manual c-ares can drain UDP indefinitely. These are pinned-source findings,
not fabricated hung-OS tests. The qualified caller-driven variant and socket quotas address the
identified mechanism choices. Native OS cryptographic service availability and process-wide bootstrap
coordination remain explicit requirements; no hard real-time guarantee is inferred.

Keep SDK storage inside the resource contract. The DNS [profile](runtime-feasibility/dns/profile.json)
records a conservative input-buffer/reallocation envelope, not a complete measured SDK heap. TLS initial
DER/key decoding can happen before its verification callback; cap inputs and account for that work
before making a full-adapter resource claim. Shared SDK users must obey the documented bootstrap,
allocator, global SSL configuration and custom ENGINE/key-method restrictions.

Independent critical review and native reruns apply to this bounded feasibility result. Production
session/adapter callback barriers, auth order with real protocol state, complete SDK envelopes, consumer
package/install closure, reconnect, cross-run isolation, adversarial DER/key cases, sustained load and
external deployments remain separate required integration evidence. Native root regressions and public
API Doxygen from prior implementation revisions are not relabeled as new transport tests; no public
production API or root build option changed here.
