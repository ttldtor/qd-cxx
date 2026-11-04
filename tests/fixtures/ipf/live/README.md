# Connection Java observations

[Русская версия](README.ru.md). English is authoritative.

ConnectionOracle.java records55 finite observations from pinned production QD/JDK8.
connection-provenance.json identifies helper/capture/source bytes and runtime.
Private Handler.checkAndDownload reflection instrumentation proves deterministic
pending carryover, generations, capability/date/conditional headers and period grammar;
a separate actual public start→state-callback close case joins the production Handler.
These scopes are distinct; internal instrumentation is not a public lifecycle proof.

Compile the helper using existing locked QD module target/classes directories plus
their existing jsr305/slf4j/log4j runtime dependencies. No production/dependency rebuild
or acquisition is required. JDK8 is the oracle; the formatter uses a separate JDK21.

~~~powershell
New-Item -ItemType Directory -Path $outputClasses | Out-Null
javac -encoding UTF-8 -cp $qdClasspath -d $outputClasses tests/fixtures/ipf/live/ConnectionOracle.java
java "-Xmx128m" "-Dfile.encoding=UTF-8" "-Dcom.devexperts.io.URLInputStream.connectTimeout=2000" "-Dcom.devexperts.io.URLInputStream.readTimeout=2000" -cp "$outputClasses;$qdClasspath" ConnectionOracle --run
~~~

outputClasses is a new caller-owned class directory; qdClasspath is an explicit existing
production/dependency classpath. No arguments, --help and --help-ru require no working
input and start no sockets/threads. --run opens only owned loopback sockets, bounds
headers at32768bytes, uses finite socket deadlines and joins its server/Handler threads.
Invalid arguments/exceptions return nonzero; exit0 means all finite observations completed.
It prints TSV and ordinary QD logs; retain full logs separately, selecting tab-separated
oracle rows for the committed deterministic capture. The helper writes no source/capture
files itself. Never replace captured values with inferred C++ expectations.

Period rows preserve actual integer Unicode digits, signed64 range, inf, floating
second saturation and invalid forms. Capture also covers failed unflushed carryover,
flush-before-error, conditional304, equal-date skip, exact live capability, live EOF,
one generation across COMPLETE, sticky completion and one-shot public lifecycle.
See [connection contract](../../../../spec/ipf/CONNECTION.md) and
[ownership/metadata adaptation](../../../../docs/decisions/ADR-0009-live-ipf-connection-ownership.md).
