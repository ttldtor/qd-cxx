# Recovery-owned HISTORY range observation

[Русская версия](HISTORY-STATUS-VALIDATION.ru.md). English is authoritative.

This page records the bounded observation added by QDCXX-117. The
[recovery contract](../../../spec/transport/RECOVERY.md) defines the API; the
[receiver guide](../../../docs/SUBSCRIPTION-RECEIVER.md) explains application use.
It does not qualify the complete receiving milestone or a release.

## Executed focused checks

On 2026-11-02, Windows MSVC 19.42 x64 Release with the dynamic CRT and Linux GCC 13.3
x64 Debug with ASan/UBSan each passed these three CTest entries:

| CTest entry | Scope |
|---|---|
| `transport_recovery_history_status` | Seven Core cases using the actual adopted HISTORY queue and collector |
| `transport_recovery_families` | Existing family recovery cases and the public TCP observation case |
| `transport_tls_recovery` | Existing verified TLS recovery cases and the public TLS observation case |

Linux leak detection was disabled. Test assertion totals vary with bounded native poll loops;
equal assertion counts across platforms are not an acceptance criterion.

The Core cases compare forwarded observations and debits with the actual queue query. They cover
complete delivery, an incomplete range, pending transactions, truncation, source uncertainty,
client pause, insufficient/exact work and typed invalid client/key refusals. With two admitted clients
requesting different bounds, the upper range completes while the lower range remains incomplete and
the actual aggregate source state is still RECEIVING.

The eligibility cases exercise held DATA and heartbeat events, retirement before queue reset,
cutoff, wrong-thread and callback reentry, and current backend connection/authentication/generation
changes while the model reports ACTIVE. A separate real Core DEFERRED case changes authentication
while the model still reports ACTIVE. Refused observations leave
caller work and operational stop/failure evidence unchanged.

The native TCP and verified TLS peers wait for the actual client HISTORY subscription before
releasing the snapshot. Public observations cover completed delivery, callback reentry refusal,
move/cutoff handling and detached evidence surviving owner retirement. The one-client native
snapshot reaches aggregate COMPLETE; it does not claim the separate two-client RECEIVING case.

## Installed consumers

On both platforms the current package was installed and moved to a different prefix before configuring
a separate consumer project. The original prefix was absent. Both public recovery headers matched the
tested source; compiler and linker commands used the relocated package's headers and archives.
The external TLS peer was an explicit noninstalled runtime test fixture; it was not linked into the consumer.

Each platform passed two CTest entries: one selected public TCP HISTORY case and one selected public
TLS HISTORY case. Doctest confirmed exactly one executed case per executable, avoiding a zero-match
success. The selected cases produced the real completion, reentry-refusal and detached-lifetime markers.
Other cases in those executables were deliberately filtered out. Linux ASan/UBSan reported no error;
leak detection remained disabled.

## Final regression and reference

The final 2026-11-02 matrix passed on both toolchains above: **73/73 CTest entries with JSON OFF**
and **74/74 with JSON ON** on each OS. All builds and test commands returned zero. The eleven
code/build inputs frozen before the matrix remained byte-identical through completion. These totals
include the new Core suite and the existing transport, family, queue, receiver and compatibility suites.

The ten changed C++ files passed the pinned Uncrustify 0.83.0 then clang-format 20.1.8 check.
Generated-reference checks cover both public `historyRangeStatus` methods, their three parameters,
return documentation, owner-thread and AS-OF boundary, source/asset integrity, local links and navigation.
Visual browser inspection is not established. Overall receiving-milestone qualification remains separate.

## Interpretation limits

A successful query describes the admitted local prefix at that instant. A later gated query is
unknown; an earlier complete result must not be presented as current. Local drain, raw snapshot
flags, a TLS handshake and peer closure do not establish `completeDeliveredRange`.

No peer acknowledgement, unread-input coverage, provider-wide history, future connection health,
process RSS bound, hard latency guarantee, security certification or release follows from these checks.
