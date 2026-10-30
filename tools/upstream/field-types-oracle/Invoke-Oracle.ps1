#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [Parameter(Mandatory)][string]$Upstream,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Maven,
    [Parameter(Mandatory)][string]$Inspector,
    [ValidateSet('Capture','Verify')][string]$Mode = 'Capture',
    [string]$ExpectedDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path "$PSScriptRoot/../../..").Path
$run=[IO.Path]::GetFullPath($RunDirectory)
# The existing launcher verifies the locked clean source, rebuilds the full reactor clean,
# records classpath/provider/build provenance and checks every unchanged tape baseline artifact.
& "$PSScriptRoot/../tape-oracle/Invoke-Oracle.ps1" -Mode Verify -Upstream $Upstream -JavaHome $JavaHome -Maven $Maven -Inspector $Inspector -RunDirectory $run
if ($LASTEXITCODE) { throw 'Baseline clean Verify failed' }
$meta=Get-Content "$run/metadata.json" -Raw | ConvertFrom-Json
$java=Join-Path $JavaHome 'bin/java.exe'
$javac=Join-Path $JavaHome 'bin/javac.exe'
& $javac -encoding UTF-8 -cp $meta.classpath -d "$run/harness" "$PSScriptRoot/FieldTypesOracle.java" "$PSScriptRoot/ObjectFieldsOracle.java" "$PSScriptRoot/IndexedSchemasOracle.java" "$PSScriptRoot/FlagsOracle.java" "$PSScriptRoot/DescriptorOracle.java" "$PSScriptRoot/FieldTimeOracle.java" "$repo/tools/schema-export/ResolvedSchemaExporter.java" 1> "$run/field-types-javac.stdout" 2> "$run/field-types-javac.stderr"
if ($LASTEXITCODE) { throw 'Field types compile failed' }
$info=[Diagnostics.ProcessStartInfo]::new($java)
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
foreach ($arg in @('-Xmx128m','-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC','-cp',$meta.classpath,'qdcxx.oracle.FieldTypesOracle',"$run/field-types")) { $info.ArgumentList.Add($arg) }
$started=[DateTime]::UtcNow.ToString('o')
$p=[Diagnostics.Process]::Start($info)
$o=$p.StandardOutput.ReadToEndAsync()
$e=$p.StandardError.ReadToEndAsync()
if (!$p.WaitForExit(30000)) { $p.Kill($true); throw 'Field types timeout is not EOF evidence' }
[IO.File]::WriteAllText("$run/field-types.stdout",$o.GetAwaiter().GetResult())
[IO.File]::WriteAllText("$run/field-types.stderr",$e.GetAwaiter().GetResult())
if ($p.ExitCode) { throw 'Field types run failed' }
$headerlessArguments=@('-Xmx128m','-Dfile.encoding=UTF-8','-Duser.language=en','-Duser.country=US','-Duser.timezone=UTC',
    '-Dcom.devexperts.qd.qtp.file.FileWriterImpl.noHeaderAndFooter=true','-cp',$meta.classpath,
    'qdcxx.oracle.DescriptorOracle',"$run/field-types/dispatch/headerless",'headerless')
$child=[Diagnostics.ProcessStartInfo]::new($java)
$child.UseShellExecute=$false
$child.CreateNoWindow=$true
$child.RedirectStandardOutput=$true
$child.RedirectStandardError=$true
foreach($argument in $headerlessArguments){$child.ArgumentList.Add($argument)}
$headerlessStart=[DateTime]::UtcNow.ToString('o')
$headerless=[Diagnostics.Process]::Start($child)
$childOut=$headerless.StandardOutput.ReadToEndAsync()
$childErr=$headerless.StandardError.ReadToEndAsync()
$childTimeout=!$headerless.WaitForExit(30000)
if($childTimeout){$headerless.Kill($true);$headerless.WaitForExit()}
[IO.File]::WriteAllText("$run/descriptor-headerless.stdout",$childOut.GetAwaiter().GetResult())
[IO.File]::WriteAllText("$run/descriptor-headerless.stderr",$childErr.GetAwaiter().GetResult())
$headerlessReceipt=[ordered]@{arguments=$headerlessArguments;startedUtc=$headerlessStart;finishedUtc=[DateTime]::UtcNow.ToString('o');timeoutSeconds=30;timedOut=$childTimeout;exitCode=$headerless.ExitCode}
$headerless.Dispose()
if($childTimeout -or $headerlessReceipt.exitCode -ne 0){throw 'Headerless production capture failed'}
$evidence=[ordered]@{
    format='qdcxx-field-types-oracle-v1'; mode=$Mode; startedUtc=$started; finishedUtc=[DateTime]::UtcNow.ToString('o')
    executable=$java; arguments=@($info.ArgumentList); exit=$p.ExitCode; baselineMetadata='metadata.json'
    sourceSha256=(Get-FileHash "$PSScriptRoot/FieldTypesOracle.java").Hash
    objectSourceSha256=(Get-FileHash "$PSScriptRoot/ObjectFieldsOracle.java").Hash
    indexedSourceSha256=(Get-FileHash "$PSScriptRoot/IndexedSchemasOracle.java").Hash
    flagsSourceSha256=(Get-FileHash "$PSScriptRoot/FlagsOracle.java").Hash
    descriptorSourceSha256=(Get-FileHash "$PSScriptRoot/DescriptorOracle.java").Hash
    fieldTimeSourceSha256=(Get-FileHash "$PSScriptRoot/FieldTimeOracle.java").Hash
    headerless=$headerlessReceipt
    exporterSourceSha256=(Get-FileHash "$repo/tools/schema-export/ResolvedSchemaExporter.java").Hash
    harnessClasses=@(Get-ChildItem "$run/harness" -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath("$run/harness",$_.FullName);sha256=(Get-FileHash $_.FullName).Hash} })
    tools=@(Get-ChildItem $PSScriptRoot -File | Where-Object Extension -in '.java','.ps1' | Sort-Object Name | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash $_.FullName).Hash} })
    artifacts=@(Get-ChildItem "$run/field-types" -File -Recurse | Sort-Object FullName | ForEach-Object { @{name=[IO.Path]::GetRelativePath("$run/field-types",$_.FullName); sha256=(Get-FileHash $_.FullName).Hash} })
}
$evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 "$run/field-types-metadata.json"
if (!$ExpectedDirectory) { $ExpectedDirectory=Join-Path $repo 'tests/fixtures/encoding/field-types/expected' }
& "$PSScriptRoot/Compare-FieldTypes.ps1" -Actual "$run/field-types" -Expected $(if ($Mode -eq 'Verify') { $ExpectedDirectory } else { "$run/field-types" }) 1> "$run/field-types-verify.stdout" 2> "$run/field-types-verify.stderr"
[IO.File]::WriteAllText("$run/field-types-result.txt", "$Mode PASS`n")
Get-Content "$run/field-types.stdout","$run/field-types-verify.stdout"
