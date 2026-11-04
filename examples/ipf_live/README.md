# Live IPF sample

[Русская версия](README.ru.md). English is authoritative.

This finite application uses only the installed public connection/runtime/collector APIs.
It retains an owning prefix of immutable revisions, closes the connection, joins its runtime
on the creator thread, and only then prints TSV. It does not drive transport polling.
No arguments, --help and --help-ru print input-free help before SDK, source I/O or threads.

Build the package with ORG_TTLDTOR_QDCXX_BUILD_IPF_LIVE=ON and its explicit transfer prerequisites.
ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES=ON builds qdcxx-ipf-live. The package also installs this directory
under share/qdcxx/examples/ipf_live for an independent find_package consumer; no ipf_live
component name is required. A package without the target is rejected explicitly.

~~~powershell
cmake -S "$prefix/share/qdcxx/examples/ipf_live" -B "$sampleBuild" -G Ninja "-DCMAKE_PREFIX_PATH=$prefix" "-DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=$opensslPrefix" "-DORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK=$transferSdk"
cmake --build "$sampleBuild" --parallel 1
& "$sampleBuild/qdcxx-ipf-live.exe" --help
& "$sampleBuild/qdcxx-ipf-live.exe" --run --source "http://127.0.0.1:8080/profiles.ipf" --duration-ms 10000 --max-updates 1000
~~~

prefix is the installed package, sampleBuild is caller-owned build storage, and the two SDK
prefixes are the existing matching qualified dependencies. On POSIX run qdcxx-ipf-live without .exe.
The example URL requires an application-owned server; no public service availability is claimed.

--source is required with --run and accepts up to4096 bytes in the reader's source syntax.
--duration-ms is1..600000, default10000; the absolute application deadline begins before start.
--max-updates is1..100000, default1000. The application reserves that many owning-reference slots
before start and retains only the first admitted revisions; remaining output is deliberately omitted.
Public finite model/transfer limits still apply, including retained value units; arbitrary allocator
metadata/RSS is not an exact byte promise. There are no additional trust/auth configuration flags.

The first-snapshot wait and the application condition-variable wait use the same absolute deadline.
A full capture requests cancellation of the first wait, then application close stops the connection.
No hidden polling loop, signal bridge or file-writing policy is installed. Each runtime has one
worker/connection; bootstrap preparation precedes that worker and creator join precedes SDK cleanup.
Output can block after join without retaining an active SDK worker.

Rows contain revision, type, symbol and removed. UTF-16 code units outside printable ASCII, and
backslashes, use lossless Unicode escapes. A summary reports initial snapshot separately from
HTTP/parser/container completion and native retirement. A flushed prefix without COMPLETE remains
incomplete even though it is printed; early stop does not manufacture EOF or successful framing.
State/collector callback failure counts are independent. Diagnostic output omits source/credentials.

Exit0: the first snapshot completed and application stop/drain/output succeeded.
Exit3: orderly application stop with an incomplete first snapshot.
Exit1: preparation/source/callback/drain/output failure; exit2: invalid arguments before source I/O.
The program creates one worker and source handles, changes no files/settings, and installs no signal
handlers. Its retained revisions remain valid after collector/runtime destruction.

Listener removal is advisory: an already selected invocation may finish. Runtime join covers its
own worker and notifications; an application with external collector producers must join those
participants separately. This sample has no external collector producers.
