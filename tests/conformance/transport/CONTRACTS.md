# Live transport profile and qualification contract

[Русская версия](CONTRACTS.ru.md). English is authoritative.

This contract qualifies [ADR-0006](../../../docs/decisions/ADR-0006-live-runtime.md).
Feasibility probes exercise SDK mechanisms; they are not a production session, subscription client,
TLS/auth adapter, throughput benchmark or provider-deployment qualification.

## Evidence boundaries

Distinguish pinned source facts, isolated executed mechanisms, and later adapter integration. Every
executed claim names OS/compiler/configuration, final source/dependency hashes, inputs, outcomes and a
finite harness deadline. An earlier source revision or a raced completed operation is not evidence for
a different pending operation. Logs/receipts must distinguish infrastructure failures from behavior
failures; rerunning outside a restricted networking environment does not rewrite the first result.

| ID | Required scenario | Mechanism / truthful observation | Later integration obligation |
| --- | --- | --- | --- |
| T01 | Process bootstrap and host lifetime | c-ares main-thread initialization before other threads; global cleanup after hosts/threads/users; Windows IOCP timer helper wake/join identified. | No late uncoordinated SDK initialization, client-side global cleanup, self-join or host release before quiescence. |
| T02 | Pending DNS cancel/retire | Owned silent UDP and TCP peers receive the exact question before cancel; no prior completion; once-only cancel/destruction callback on the owner. | Cancel deadline/command races, typed errors and no later application callback after run retirement. |
| T03 | DNS bounded progress | Shared per-turn receive/byte/syscall quotas inside socket callbacks; unsolicited UDP and fragmented TCP yield then resume. | Charge old/partial/new SDK buffers and all state; cap queries/servers/fds/answers/retries; no loss of stop/control progress. |
| T04 | DNS bounded initialization | Qualified pinned patch disables system/env/NSS and hostname fallback; finite explicit server list, no implicit localhost. | Validate settings before SDK allocation, reject incompatible SDKs and preserve complete peak accounting. |
| T05 | DNS run isolation | Channel cancellation is channel-wide; one channel belongs to one independent retirement domain, with one outstanding wire query. | Cancel one run without changing another run's queries; no cross-run or old-epoch delivery. |
| T06 | Pending TCP connect | Controlled local backlog saturation proves operations remain incomplete before close; count success/race/abort separately and drain every operation. | Session deadlines, repeated attempts, epoch leases, exhaustion and candidate ownership. |
| T07 | Pending TCP read/write | Connected idle peer proves read pending; nonreading peer plus bounded payload proves incomplete partial write before cancellation. | Retain suffixes/buffers through completion; do not equate kernel acceptance with peer acknowledgment. |
| T08 | Pending TLS handshake/shutdown | Controlled TCP peer stalls handshake or `close_notify`; deadline closes lowest layer and drains handlers. | Bounded graceful-to-abort transition, independent read/write state and once-only truthful terminal completion. |
| T09 | TLS trust and identity | Correct SAN DNS/IP succeeds; unknown issuer, wrong identity and expired certificate fail. | Missing/unreadable trust fails before network activation; configured endpoint identity survives DNS/CNAME; no verification bypass or plaintext downgrade. |
| T10 | TLS versions and input limits | TLS 1.2/1.3 negotiated positively; cert-message/depth/record-buffer limits named, SDK build properties checked. | Bound trust bytes/anchors and approved crypto inputs before parsing/verification; derive SDK memory/work allowance rather than treating a deadline as preemption. |
| T11 | Callback requests stop | A completion requests cancel/close, returns, and owner progress drains remaining operations. | Application callback admission/delivery barriers, final-handle release and global close semantics. |
| T12 | Dependency OFF | Standalone probe's OFF configuration avoids SDK discovery/linking; ordinary production libraries retain their existing dependency boundary. | Future production OFF/enabled combinations and independent build/install consumers; prototype OFF is not evidence for unimplemented production options. |
| T13 | Help and owned cleanup | Explicit probe help/no-argument contracts, failing output and invalid input; no network activation during help; harness deadlines and owned resource cleanup. | Every maintained public utility documents options/defaults/results/exits/side effects in EN/RU. |
| T14 | Authentication order | Existing pinned QTP auth/readiness characterization is separate from TLS mechanism probes. | Instance-local credentials only after verified TLS, explicit credential-free plain mode, bounded auth, redacted failure/reconnect diagnostics. |

## Resource interpretation

Library-owned SDK storage is included in the resource contract. Descriptor counts or application payload
limits cannot silently stand in for parser, query/cache, TLS record/BIO/certificate/trust/context storage.
Account for retained capacities, retired owners and simultaneous candidate/reallocation peaks. Logical
payload/item/work ceilings are distinct from allocator overhead, native thread stack reservation,
kernel buffers, observed RSS and elapsed time; state each scope explicitly.

A prototype's finite callback/read counters prove its tested mechanism only. The full adapter must
define and verify its SDK envelope before claiming conformance. An unknown envelope remains unknown,
not zero. OS scheduling and required native cryptographic service availability are environmental
preconditions; no hard real-time bound is extrapolated from loopback microseconds.

## Qualification targets and pending integration

Initial targets are Windows x64/MSVC Release and Linux x86-64/GCC with ASan+UBSan for changed native
probe/adapter code. LeakSanitizer remains explicitly disabled in the recorded Linux profile. State
whether SDKs themselves are instrumented; an instrumented caller is not evidence about every SDK
internal memory access. Keep certificate material synthetic and peers local/owned.

The feasibility result does not implement production adapters. Runtime/session integration, complete
resource envelopes, wrong-SDK handling, exported static link closure, consumer installation, auth,
reconnect and multiplexer isolation remain later required checks. Unsupported OS/architectures,
MinGW/io_uring, arbitrary system SDKs, implicit trust/system-DNS providers, custom global allocators,
late SDK bootstrap and external provider deployments are not qualified by these probes.
