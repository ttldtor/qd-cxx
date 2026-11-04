# IPF compression fixtures and JDK8 oracle

[Русская версия](README.ru.md). English is authoritative.

CompressionOracle.java is an internal bounded fixture/oracle helper. It uses the pinned QD revision
from upstream.lock, production StreamCompression and InstrumentProfileReader, and an existing JDK8.
No QD source, dependency or SDK is rebuilt/acquired by the helper. The captured profile is Zulu
OpenJDK1.8.0_422. provenance.json records exact source/class identities and fixture byte hashes;
cached production class observation is not a claim of fresh source-to-bytecode reproduction.

The helper creates synthetic UTF-8 IPF, uses JDK GZIPOutputStream/raw Deflater, writes small explicit
ZIP local-entry/ZIP64/descriptor records, and includes an ordinary multi-entry ZipOutputStream
archive. The ordinary archive uses a fixed1980 timestamp; invoke Java with user.timezone=UTC.
The malformed cases deliberately truncate or corrupt named headers, payloads, sizes and CRC.
No private credentials, source data, paths or public-network response is present.

outcomes.tsv and outcomes-v2.tsv/outcomes-v3.tsv preserve earlier actual observations.
outcomes-v5.tsv captures the final40 cases from the maintained helper. The original ordinary ZIP
capture used its observation-time metadata; the current fixture has fixed metadata. Its Java
decode/IPF outcome is unchanged. Captures are observations, not mutable C++ expected output.

[ADR-0007](../../../../docs/decisions/ADR-0007-ipf-container-interoperability.md) records intentional
differences for valid unsigned descriptors and small ZIP64 with64-bit descriptor fields.
zip64-descriptor-small has ZIP64 extra with a32-bit descriptor: it is internally inconsistent
under PKWARE4.3.9.2, although the captured JDK8 accepts it. C++ rejects it. JDK8 descriptor bugs are
not copied into the C++ reader. gzip-missing-marker records Java's default warning/allow policy;
C++ explicitly selects ALLOW or REQUIRE. Envelope and IPF outcomes are separate.

## Invocation and help

Compile with an existing pinned production QD classpath: its module target/classes directories
and existing jsr305/slf4j/log4j runtime dependencies. C++ tests replay the committed binaries and
do not require Java. The JDK21 formatter is separate from this JDK8 oracle.

PowerShell, replacing the illustrative absolute tool/classpath values:

    $jdk8 = 'C:/absolute/jdk8'
    $qdClasses = 'C:/absolute/qd/dxlib/target/classes;C:/absolute/qd/dxfeed-api/target/classes;C:/absolute/dependencies.jar'
    & "$jdk8/bin/javac.exe" -encoding UTF-8 -cp $qdClasses -d build/ipf-oracle tests/fixtures/ipf/compression/CompressionOracle.java
    $oracleClasses = 'build/ipf-oracle;' + $qdClasses
    & "$jdk8/bin/java.exe" '-Dfile.encoding=UTF-8' '-Duser.timezone=UTC' -cp $oracleClasses CompressionOracle --help
    & "$jdk8/bin/java.exe" '-Dfile.encoding=UTF-8' '-Duser.timezone=UTC' -cp $oracleClasses CompressionOracle build/ipf-oracle-output

No arguments, --help and --help-ru print help and perform no file/network/native SDK work.
One new output directory writes40 finite .bin files and outcomes.tsv; existing files are refused.
Decoded observations are bounded by1MiB per case. Exit0 means success; nonzero means invalid
arguments, existing output, classpath/tool failure, I/O failure or a probe bound violation.
Do not regenerate over checked captures. Use a new output directory and compare identities first.
