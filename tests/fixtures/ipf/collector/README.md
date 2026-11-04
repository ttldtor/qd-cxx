# Collector Java observations

[Русская версия](README.ru.md).

`CollectorOracle.java` records finite actual observations from the locked production
`InstrumentProfileCollector` classes. `provenance.json` identifies source/class/helper
bytes and JDK8. `outcomes-v2.tsv` contains 16 rows; the previous 14-row v1 capture remains unchanged. Existing production classes are used;
this is not a claim that compiling the identified source reproduces their bytes.

Compile the helper with JDK8 and an explicit classpath containing the locked QD module
class directories plus their logging dependencies:

```powershell
New-Item -ItemType Directory -Path $outputClasses | Out-Null
javac -encoding UTF-8 -cp $qdClasspath -d $outputClasses tests/fixtures/ipf/collector/CollectorOracle.java
java "-Dfile.encoding=UTF-8" -cp "$outputClasses;$qdClasspath" CollectorOracle --run
```

`$qdClasspath` is the caller-supplied QD/dependency classpath and `$outputClasses` is a new
caller-owned class output directory. Use `--help`, no arguments or `--help-ru` for safe
input-free help; these do not initialize a collector or start oracle work. Invalid
arguments fail. `--run` prints TSV to stdout, uses no input files/network and creates no
executor threads; it explicitly selects direct or finite queued execution. Loops are
capped at 32 items/turns. Exit 0 means success; exceptions/invalid arguments return nonzero.

The two added rows distinguish unchanged same-instance publication in the reference-retaining base from equal resubmission suppression with the actual defensive-copy hook. C++ follows immutable value equality, independently of object address. Rows cover initial empty/nonempty notifications, standard/custom values, equal-value
generation/time behavior, absent removal, minimal generation tombstones, complete supplied
removals and superseded queued revisions. The C++ conformance tests exercise these contracts
plus owned lifetime, finite admission, allocation failure, reentry and concurrency.
See [normative collector behavior](../../../../spec/ipf/COLLECTOR.md) for the owning snapshot
and scheduling boundary. This capture does not qualify a connection or completion wait.
