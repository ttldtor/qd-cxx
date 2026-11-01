# Tape reader production probes

[Русская версия](README.ru.md). English is authoritative.

[Tape contract](../../../spec/protocol/TAPE-READER.md), [coverage](../../../tests/fixtures/protocol/tape-reader/COVERAGE.md).
This Java harness builds bounded diagnostic inputs and feeds the actual unchanged FileReader with
explicit BINARY/NONE compression, MAX_SPEED, cycle=false and the S/R/E local scheme. It does not
reimplement descriptor parsing, subclass the production parser, inject mappings, use reflection or
claim CLI/connector compatibility. Constructed input bytes are stimuli, not production FileWriter
outputs; original.qtp is the unchanged QDCXX-019 constrained FileWriter artifact.

```powershell
$args021 = @{
  Upstream = 'C:/path/to/profile/AppData/Local/Temp/qdcxx-003-qd'
  JavaHome = 'C:/Program Files/Zulu/zulu-8'
  Maven = 'C:/path/to/apache-maven/bin/mvn.cmd'
  Inspector = 'C:/qd-cxx/.work/tape-msvc/Release/qdcxx-qtp-inspect.exe'
}
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Format -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -RunDirectory .work/probes-a
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -RunDirectory .work/probes-b
./tools/upstream/tape-reader-oracle/Compare-Probes.ps1 -Actual .work/probes-a/probes -Expected .work/probes-b/probes
./tools/upstream/tape-reader-oracle/Test-Negatives.ps1 -Expected tests/fixtures/protocol/tape-reader/expected -RunDirectory .work/probes-negative
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -Mode Verify -ExpectedDirectory .work/probes-negative/value -RunDirectory .work/probes-negative-verify
# Previous command MUST fail. Then verify the unchanged baseline:
./tools/upstream/tape-reader-oracle/Invoke-Probes.ps1 @args021 -Mode Verify -RunDirectory .work/probes-verify
pwsh -NoProfile -File tools/format/Format-Java.ps1 -Mode Check -JavaHome 'C:/Program Files/Microsoft/jdk-21.0.4.7-hotspot'
```

All run directories must be new. The launcher first executes the unchanged tape-oracle clean upstream
build and full 22-artifact baseline Verify; this supplies locked source/clean-tree/JDK8/Maven/environment/
classpath/dependency provenance. It then compiles the new Java harness, records source/launcher/class
hashes and invocation/times, runs probes with a 30-second process limit, and validates the exact 102-file
set. Run sequentially against one upstream tree. No new Python or dependency; upstream tests are skipped.
Metadata records real UTC times, no normalization. Timeout fails; it never becomes EOF evidence.

Compare validates every row's kind/arity, bounded observations, record sequence/identity/value domains,
end counts and exactly one final completion, and compares every raw artifact byte hash. Original source
file size is independently checked against the case row. Expected files never update in Capture/Verify.
Ten separate copied mutations must fail, including empty coverage and changed values/types/IDs/positions.
The changed position mutation is the observed input byte count, not a fabricated Java primitive offset.
Production callbacks do not expose primitive offsets; C++ position projections are independently tested.

Java can continue after errors; the observer records actual deliveries/errors/exceptions and a distinct
completion of the experiment. Its `complete` row does not mean that malformed input parsed successfully.
Source grammar references are ProtocolDescriptor.parseFrom/parseMessageListFrom, MessageDescriptor.parseFrom,
BufferedInput.readProperties and BinaryQTPParser.parseDescribeProtocol/parseDescribeRecords at upstream.lock.
No portability or general tape claim beyond the recorded profile. Final source hashes and recorded checks
are in [validation](../../../tests/fixtures/protocol/tape-reader/VALIDATION.md).
