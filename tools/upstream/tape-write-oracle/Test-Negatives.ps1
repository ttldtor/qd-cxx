#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDirectory, [Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$run = [IO.Path]::GetFullPath($RunDirectory)
if (Test-Path -LiteralPath $run) { throw 'RunDirectory must be fresh' }
New-Item -ItemType Directory -Path $run | Out-Null
$mutations = @('byte','type','order','completion','provenance','file-set','hidden-file','hidden-directory','grammar-both-peers')
foreach ($kind in $mutations) {
    $root = Join-Path $run $kind
    Copy-Item -LiteralPath $Expected -Destination $root -Recurse
    $tape = "$root/extras/extras-raw-message.qtp"
    $observations = "$root/extras/extras-raw-message.tsv"
    switch ($kind) {
        byte { $bytes=[IO.File]::ReadAllBytes($tape); $bytes[-1]=$bytes[-1] -bxor 1; [IO.File]::WriteAllBytes($tape,$bytes) }
        type { $value=[IO.File]::ReadAllText($observations); $value=$value.Replace("value`tread`t0`tint`t0`t2`t","value`tread`t0`tint`t0`t8`t"); [IO.File]::WriteAllText($observations,$value) }
        order {
            $rows = [IO.File]::ReadAllLines($observations)
            $first = -1; $second = -1
            for ($index = 0; $index -lt $rows.Length; $index++) {
                if ($rows[$index].StartsWith("record`tread`t0`t")) { $first = $index }
                if ($rows[$index].StartsWith("record`tread`t1`t")) { $second = $index }
            }
            if ($first -lt 0 -or $second -lt 0) { throw 'Order mutation records missing' }
            $saved = $rows[$first]; $rows[$first] = $rows[$second]; $rows[$second] = $saved
            [IO.File]::WriteAllLines($observations,$rows)
        }
        completion { $value=[IO.File]::ReadAllText($observations); $value=$value.Replace("complete`textras-raw-message`n",''); [IO.File]::WriteAllText($observations,$value) }
        provenance { $value=[IO.File]::ReadAllText("$root/provenance.tsv"); $value=$value.Replace('TapeWriteOracle.java','Missing.java'); [IO.File]::WriteAllText("$root/provenance.tsv",$value) }
        file-set { [IO.File]::WriteAllText("$root/unexpected.txt",'extra') }
        hidden-file {
            $hidden = "$root/.unexpected.txt"
            [IO.File]::WriteAllText($hidden,'extra')
            if ($IsWindows) { [IO.File]::SetAttributes($hidden,[IO.FileAttributes]::Hidden) }
        }
        hidden-directory {
            $hidden = "$root/.unexpected"
            [IO.Directory]::CreateDirectory($hidden) | Out-Null
            if ($IsWindows) { [IO.File]::SetAttributes($hidden,[IO.FileAttributes]::Hidden) }
        }
        grammar-both-peers { $value=[IO.File]::ReadAllText($observations); $value=$value.Replace("`tint`t0`t2`t","`tint`t0`t127`t"); [IO.File]::WriteAllText($observations,$value) }
    }
    $against = if ($kind -in 'grammar-both-peers','order') { $root } else { $Expected }
    $executable = (Get-Command pwsh -CommandType Application | Select-Object -First 1).Source
    $info = [Diagnostics.ProcessStartInfo]::new($executable)
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    foreach ($argument in @('-NoProfile','-File',"$PSScriptRoot/Compare-TapeWrite.ps1",'-Actual',$root,'-Expected',$against)) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout=$process.StandardOutput.ReadToEndAsync(); $stderr=$process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) { $process.Kill($true); $process.WaitForExit(); throw "Mutation comparison timeout: $kind" }
        [IO.File]::WriteAllText("$run/$kind.stdout",$stdout.GetAwaiter().GetResult())
        $diagnostic = $stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText("$run/$kind.stderr",$diagnostic)
        if ($process.ExitCode -eq 0) { throw "Mutation escaped: $kind" }
        $required = switch ($kind) {
            order { 'Record order/flags' }
            hidden-file { 'Artifact set mismatch' }
            hidden-directory { 'Directory set mismatch' }
            default { $null }
        }
        if ($required -and !$diagnostic.Contains($required)) { throw "Mutation failed at wrong guard: $kind" }
    } finally { $process.Dispose() }
    [IO.File]::WriteAllText("$run/$kind.result","REJECTED`n")
}
& "$PSScriptRoot/Compare-TapeWrite.ps1" -Actual $Expected -Expected $Expected
"PASS: $($mutations.Count) mutations rejected; unchanged baseline verified"
