# Portable developer tools

[Русская версия](README.ru.md). English is authoritative.

The native `qdcxx-run-utc` logger and [CMake formatter](../format/CPP.md) require no Python or
PowerShell in their command trees. Prerequisites are CMake 3.25+, a C++23 compiler and its native
build tools; formatting additionally needs Git and explicitly supplied native Uncrustify 0.83.0
and clang-format 20.1.8. Nothing is installed globally. These developer executables have no
production-library dependencies. Root `BUILD_TESTING=OFF` neither builds them nor fetches doctest.

## First build

Run from the repository root. This bootstrap is the explicit first-build logging exception:
it records UTC phase bounds, argument lengths, commands, exits and separate raw stdout/stderr,
without per-line timestamps. It does not need a prebuilt logger, root CTest or doctest.

Windows with Visual Studio 2022, from an ordinary terminal (PowerShell may be the interactive shell):

```text
cmake "-DBINARY=.work/dev" "-DLOG=.work/dev-bootstrap.log" "-DGENERATOR=Visual Studio 17 2022" -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/Release/qdcxx-run-utc.exe --log .work/configure.log -- cmake -S . -B .work/build "-GVisual Studio 17 2022" -A x64
.work/dev/Release/qdcxx-run-utc.exe --log .work/build.log -- cmake --build .work/build --config Release
.work/dev/Release/qdcxx-run-utc.exe --log .work/test.log -- ctest --test-dir .work/build -C Release --output-on-failure
```

Linux, from a POSIX shell with GCC and Ninja:

```sh
cmake -DBINARY=.work/dev -DLOG=.work/dev-bootstrap.log -DGENERATOR=Ninja -DCONFIG=Release -P tools/dev/Bootstrap.cmake
.work/dev/qdcxx-run-utc --log .work/configure.log -- cmake -S . -B .work/build -G Ninja -DCMAKE_BUILD_TYPE=Release
.work/dev/qdcxx-run-utc --log .work/build.log -- cmake --build .work/build --parallel 6
.work/dev/qdcxx-run-utc --log .work/test.log -- ctest --test-dir .work/build --output-on-failure
```

An existing reviewed doctest source can be supplied with `-DFETCHCONTENT_SOURCE_DIR_DOCTEST=<path>`.
Otherwise the root test build uses its unchanged pinned FetchContent dependency. The standalone
bootstrap never fetches it. `CONFIGURE_ARGS` optionally accepts a JSON array of extra CMake arguments.
Quote complete `-DNAME=value` arguments in PowerShell, especially when passing them after `--`.
The [optional Windows scripts](../format/CPP.md) remain usable for acquisition and historical runs.

## Logger contract

Invocation: `qdcxx-run-utc --log PATH -- COMMAND [ARG ...]`. No arguments and standalone `--help`
print identical stdout, empty stderr and exit 0, without file access. Nonempty invalid arguments
return 2; failed help output returns 1. Executable and argument vector are passed without a shell.
The command inherits cwd and environment with only its `TZ` replaced by `UTC`. Windows uses native
Unicode argv, independently of the active code page. Parent environment and host timezone are unchanged.

Both streams are drained concurrently and decoded independently as incremental UTF-8 with replacement,
without BOM detection. A UTF-8 BOM is retained as text; UTF-16 BOM bytes are invalid UTF-8. CR, LF and
CRLF delimit lines, including boundaries split across reads. Final unterminated fragments remain
separate. Order within a stream is preserved; cross-stream order is unspecified. Each emitted line
has an ISO 8601 UTC millisecond timestamp ending in `Z`. Log and mirrored stdout use UTF-8, LF and
no file BOM. COMMAND contains JSON-escaped argv/cwd; EXIT records a normal child result. These are
text logs, not raw captures; raw inspector output remains separate.

Normal child exits, including 37, propagate. POSIX signal termination maps to `128 + signal`.
Launch, destination, cancellation or cleanup failure returns 1 with `UTC LAUNCHER FAILED:` on stderr
when that destination permits it. Ownership exists before execution: a suspended Windows process
is assigned to a kill-on-close job before resume; a POSIX child establishes its session before exec.
Root exit is observed independently of stream EOF; remaining owned descendants are terminated on
completion and failure. Cleanup has one five-second grace from root exit or detected destination
failure; failure to confirm completion returns 1. Linux adopts/reaps owned orphan descendants.

Linux cleanup enumerates the original session through `/proc`, including descendants that call
`setpgid` without leaving that session. The exited root remains unreaped until the session is empty,
reserving its PID/session ID. Signals and descendant waits use process descriptors to prevent PID reuse
from redirecting cleanup. This requires readable `/proc`, a waitable root (`SIGCHLD` must not be ignored
or use `SA_NOCLDWAIT`), and native `pidfd_open`, `pidfd_send_signal`, and `waitid(P_PIDFD)` support
(Linux 5.4 or later with those capabilities permitted). Missing capabilities fail before child execution;
there is no unsafe numeric-PID fallback. The kernel interfaces are documented in
[pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
[pidfd_send_signal(2)](https://man7.org/linux/man-pages/man2/pidfd_send_signal.2.html), and
[waitid(2)](https://man7.org/linux/man-pages/man2/waitid.2.html).

The deadline remains active through final EXIT output and log closure. A stalled destination is
cancelled after four seconds, reserving the last second for confirmed cleanup. Native log writes
have no userspace buffer and do not request durable `fsync`; a POSIX FIFO log requires an existing
reader before launch. Stalled stdout pipes and a stalled Linux log FIFO have executed regression tests.
Failure/usage diagnostics on stderr are best effort: the launcher waits at most 100 milliseconds for
an owned native writer, then exits with the authoritative status even if stderr remains full.
The writer uses no stdio locks and is discarded by immediate process exit if still blocked.

SIGINT/SIGTERM and Windows CTRL_C/CTRL_BREAK request bounded cleanup. Deliberately escaped POSIX
sessions, externally forced launcher termination and uninterruptible kernel/device failures are
outside this guarantee. Windows console-close/forced termination is not graceful cancellation.
macOS has received source inspection only; no build or runtime support claim is made.
The unqualified non-Linux POSIX branch retains process-group cleanup; it does not implement the Linux
same-session guarantee and is not a supported/qualified launcher platform in this task.

## Exact native Linux formatters

Caller-provided exact executables are the portable route. The following local archive route was used
on Ubuntu 24.04 x86_64. It uses `curl`, `sha256sum` and native CMake archive extraction; neither
conda, pip, Python nor PowerShell is invoked. Archive packaging does not add a runtime dependency.
The binaries use the host C/C++ runtime; other distributions/architectures need separately verified
exact binaries. URLs, archive and executable hashes are in the tool receipt.

```sh
mkdir -p .work/native-format/uncrustify .work/native-format/clang
cd .work/native-format
curl -fL https://api.anaconda.org/download/conda-forge/uncrustify/0.83.0/linux-64/uncrustify-0.83.0-h54a6638_0.conda -o uncrustify.conda
echo '556ea427c375c2b9eb3122b19a1ce0891d535ad51a196f25c73786bb1d7c5156  uncrustify.conda' | sha256sum -c -
cd uncrustify
cmake -E tar xf ../uncrustify.conda
cmake -E tar xf pkg-uncrustify-0.83.0-h54a6638_0.tar.zst
cd ..
curl -fL https://files.pythonhosted.org/packages/a6/77/786aa0fc8a75d8ce94966bb33e44c63fec1964cbf343ee862ed6a5be38c1/clang_format-20.1.8-py2.py3-none-manylinux_2_27_x86_64.manylinux_2_28_x86_64.whl -o clang-format.whl
echo '7c6bcb7e01ba4f05a4c980fda147b330f7e4833c2aea8c92a0c2df9573ae7afe  clang-format.whl' | sha256sum -c -
cd clang
cmake -E tar xf ../clang-format.whl
chmod +x clang_format/data/bin/clang-format
cd ../../..
cmake -DUNCRUSTIFY=.work/native-format/uncrustify/bin/uncrustify -DCLANG_FORMAT=.work/native-format/clang/clang_format/data/bin/clang-format -DMODE=Check -P tools/format/Format-Cpp.cmake
```

Use a shell with failure checking (`set -e`) or stop after any failed download/hash/extraction command.
Acquisition is separate from formatter execution; the driver neither downloads tools nor substitutes versions.

## Verification entry points

Root CTest retains the old suites and adds logger, formatter, coverage and trace-audit contracts.
The normal formatter test uses native fake tools for negative controls; supply both
`ORG_TTLDTOR_QDCXX_UNCRUSTIFY` and `ORG_TTLDTOR_QDCXX_CLANG_FORMAT` at configure time to add real
pipeline checks. Supplying only one is an error. The full acceptance matrices supply both.

`tests/conformance/tooling/matrix.cmake` takes ROOT, fresh RUN/EVIDENCE paths, native GUARD/LOGGER,
DOCTEST source and UNCRUSTIFY/CLANG_FORMAT paths. It runs all required compiler profiles and all
eight testing/QTP/TAPE combinations through `--deny-interpreters`. `guard.cmake` separately checks
real interpreter paths, PATH lookup, swallowing descendants and an interpreter-launching formatter;
debuggers cannot nest, so these guard controls are not run inside another tracer. The original
guard interface remains Python-only for historical reproduction. This is image-denial workflow
evidence, not a security sandbox against renamed or embedded interpreters.
`recheck.cmake` rebuilds existing matrix profiles and reruns their complete applicable CTest inventories
after tooling corrections, keeping the original evidence intact. It also repeats standalone bootstrap,
real formatting and exact trace audits; fresh RUN/EVIDENCE directories are required.

[Coverage](../../tests/conformance/tooling/COVERAGE.md) maps named obligations;
[validation](../../tests/conformance/tooling/VALIDATION.md) records executed commands, results and
limitations. Author verification and self-review do not close independent acceptance. Java tooling,
remaining manual Python checks and historical artifacts retain their existing requirements.

## Offline Java producer tooling

The [production tape corpus](../upstream/real-tape-corpus/README.md) provides separately pinned
baseline/current producers, deterministic offline FileWriter/FileReader fixtures, Capture/Verify,
bounded processes and negative checks. Windows working modes require PowerShell, JDK 8, Maven
and an explicit native UTC runner; help is portable. It is independent of native configure/build/CTest.
Its launcher, help regression and process-lifetime regression scripts document their parameters,
outputs, exit codes and side effects with `-Help` and `Get-Help -Full`.
