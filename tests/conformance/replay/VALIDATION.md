# Offline replay validation

[Русская версия](VALIDATION.ru.md). English is authoritative.

## Implemented boundary

The optional `qdcxx_replay` library exposes synchronous `replayTape` for an immutable admitted
source. The caller chooses `IMMEDIATE` or `RELATIVE`, supplies finite independent parse/work/callback/
clock/wait bounds, and owns the observer, cancellation poll and monotonic clock/wait objects. It
preserves generic records and raw timing in wire order. It does not add a command-line program,
filesystem access, transport, background work or a real clock implementation.

The engine uses a fresh reader and requires actual byte exhaustion plus reader finish for clean EOF.
A failed frame result never delivers its completed record prefix. Relative scheduling applies only to
nonempty data frames with present timestamps; empty frames do not affect anchors, elapsed high-water
or range checks. Callback record references expire on return. Earlier external observer effects are
not rolled back. These behavioral and lifetime rules are specified in the
[offline replay contract](../../../spec/protocol/OFFLINE-REPLAY.md).

## Executed focused checks

The author’s final Windows MSVC 19.42 x64 Release target build and focused CTest passed one target
with 9 cases / 1,303 assertions. The test executes all 24 documented replay scenarios, uses native
writer/admission inputs, compares nine Java RAW timestamp/cursor packing observations and covers all
eight exact field types. An independently calculated fixture ledger expects 31 work units across
seven frame attempts, two record attempts, two layout visits, 13 engine polls, three clock samples, one wait and three
observer attempts. Three deliberate mutations failed their intended controls: failed-prefix delivery
(2 cases / 3 assertions), scheduling empty frames (1 / 13), and accepting premature `READY` (1 / 1).
The final C++ format check passed all three owned source files.

An independent focused Windows MSVC review also passed the replay target (9 cases / 1,303
assertions). Its standalone Windows consumer first failed when included without macro guards, then
compiled and ran after defining `NOMINMAX` before Windows headers and undefining `RELATIVE` before
the replay header. This validates that specific consumer sequence only; it is not a general guarantee
for arbitrary Windows include orders. The initial failing compile and guarded pass are both preserved
in the archive.

## Full project checks

The full Windows MSVC x64 Release build and CTest passed 51/51 tests in 311.19 seconds. Three
Windows production-only Release profiles also configured and built successfully with testing and
JSON disabled: replay ON with all tools OFF; replay ON with all three inspection/writer tools ON;
and replay OFF with all tools OFF. The final profile record confirmed `_deps` was absent in each and
that `qdcxx_replay` appeared only when enabled; see the profile record.

The repository-wide C++ format check passed for 100 files. Doxygen 1.18.0 exited successfully and
generated the replay API pages; its warnings file was empty. `EXTRACT_ALL` is enabled, so an empty
warnings file does not certify completeness of all API documentation.

The Linux Debug ASan/UBSan CTest suite passed 51/51 on Ubuntu 24.04 / WSL2 with GCC 13.3 in 869.98 seconds (configure, build, CTest). Linux production-only Release profiles also passed: replay ON with tools OFF, replay ON with all three tools ON, and replay OFF with tools OFF. The artifact checks confirmed `_deps` absent in all profiles and the replay library/target present only when enabled; see the Linux profile record. The replay-OFF build emitted the existing GCC `-Wstringop-overflow` warning at `src/core/wide_decimal.cpp:68`; it exited successfully.

No macOS, real-time clock integration, replay CLI, transport, callback blocking bound, hostile
concurrent mutation or complete tape compatibility is claimed. Caller callbacks and waits must
satisfy the prompt-return contract; the engine cannot bound a callback that blocks indefinitely.