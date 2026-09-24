#Requires -Version 7.4
<#
.SYNOPSIS
Run fresh platform-specific build/test matrices under a process-tree Python image guard.
.DESCRIPTION
Bootstrap qdcxx_no_python_guard once with the host C++ compiler, then pass its absolute path.
The guard stops interpreter images before execution, including absolute paths; PATH aliases
are also shadowed. No interpreter, timezone or machine setting is removed or changed.
Windows covers MSVC/ClangCL Debug/Release; Linux covers GCC Debug/Release and ASan+UBSan.
Each platform also exercises all eight BUILD_TESTING/QTP/TAPE combinations without exclusions.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Guard,
    [Parameter(Mandatory)][string]$DoctestSource,
    [Parameter(Mandatory)][string]$PythonControl,
    [string]$RunTag = ([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')),
    [string]$EvidenceRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$Guard = (Resolve-Path -LiteralPath $Guard).Path
$DoctestSource = (Resolve-Path -LiteralPath $DoctestSource).Path
$PythonControl = (Resolve-Path -LiteralPath $PythonControl).Path
if ($RunTag -notmatch '^[A-Za-z0-9_-]+$') { throw 'Unsafe run tag' }
$platform = if ($IsWindows) { 'windows' } else { 'linux' }
if (!$EvidenceRoot) { $EvidenceRoot = Join-Path $PSScriptRoot "evidence/$platform-$RunTag" }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw 'Evidence directory must be fresh' }
[IO.Directory]::CreateDirectory($EvidenceRoot) | Out-Null
$work = Join-Path $repo ".work/python-migration-$platform-$RunTag"
if (Test-Path -LiteralPath $work) { throw 'Build root must be fresh' }
[IO.Directory]::CreateDirectory($work) | Out-Null
$cmake = (Get-Command cmake -CommandType Application | Select-Object -First 1).Source
$ctest = (Get-Command ctest -CommandType Application | Select-Object -First 1).Source
$pwsh = (Get-Process -Id $PID).Path
$gcc = if (!$IsWindows) { (Get-Command g++ -CommandType Application | Select-Object -First 1).Source } else { $null }
$logger = Join-Path $repo 'tools/Run-Utc.ps1'
$originalPath = $env:PATH
$originalAsan = $env:ASAN_OPTIONS
$originalLeak = $env:LSAN_OPTIONS
$originalBytecode = $env:PYTHONDONTWRITEBYTECODE
$originalMsbuild = $env:MSBUILDDISABLENODEREUSE
$originalCliTelemetry = $env:DOTNET_CLI_TELEMETRY_OPTOUT
$baseline = (Get-Content (Join-Path $PSScriptRoot 'evidence/baseline-ctest.json') -Raw | ConvertFrom-Json).tests.name
$qtpNames = @('qtp_argument_contract','offline_inspection_cli','offline_inspection_injected_io',
    'human_inspection_cli','human_inspection_injected_io')
$tapeNames = @('profile_inspection_io','profile_inspection_cli','field_type_inspection_io',
    'field_type_inspection_cli','raw_tape_inspection_cli','raw_tape_inspection_io','quote_inspection_io',
    'quote_inspection_cli','tape_inspection_io','tape_inspection_cli','tape_argument_contract')

function Invoke-Guarded([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $inventory = Join-Path $EvidenceRoot "$Name.exec.tsv"
    & $logger -Log (Join-Path $EvidenceRoot "$Name.log") -Executable $Guard -ArgumentList (@($inventory,$Executable) + $Arguments)
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit $LASTEXITCODE" }
    if (!(Test-Path -LiteralPath $inventory) -or (Select-String -LiteralPath $inventory -Pattern '^DENIED')) {
        throw "$Name invoked Python or failed to inventory processes"
    }
}

function Save-TestInventory([string]$Build, [string]$Config, [bool]$Qtp, [bool]$Tape, [string]$Name) {
    $start = [Diagnostics.ProcessStartInfo]::new($Guard)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @((Join-Path $EvidenceRoot "$Name.inventory.exec.tsv"),$ctest,'--test-dir',$Build,
            '-C',$Config,'--show-only=json-v1')) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) { $process.Kill($true); throw 'CTest inventory timeout' }
        $json = $stdout.GetAwaiter().GetResult()
        $errorText = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0 -or $errorText.Length) { throw "CTest inventory failed: $errorText" }
        [IO.File]::WriteAllText((Join-Path $EvidenceRoot "$Name.tests.json"),$json,[Text.UTF8Encoding]::new($false))
        $tests = ($json | ConvertFrom-Json).tests
        $required = @($baseline | Where-Object { ($Qtp -or $_ -notin $qtpNames) -and ($Tape -or $_ -notin $tapeNames) })
        foreach ($test in $required) { if ($test -notin $tests.name) { throw "Missing preserved test: $test" } }
        foreach ($test in $tests) {
            if ($test.command -match '(?i)(^|[/\\])(?:python[0-9.]*|py)(?:\.exe)?$') { throw 'Python in CTest inventory' }
            if ($test.properties | Where-Object { $_.name -eq 'DISABLED' -and $_.value }) { throw 'Disabled CTest test' }
        }
        if ($Qtp) {
            foreach ($name in @('inspection_process_contract','inspection_cli_negatives','inspection_checker_contract')) {
                if ($name -notin $tests.name) { throw "Missing new verification: $name" }
            }
        }
    } finally { $process.Dispose() }
}

function Invoke-BuildCase([string]$Name, [string]$Compiler, [string]$Config, [bool]$Testing,
                         [bool]$Qtp, [bool]$Tape, [bool]$Sanitize = $false) {
    $build = Join-Path $work $Name
    $testValue = if ($Testing) { 'ON' } else { 'OFF' }
    $qtpValue = if ($Qtp) { 'ON' } else { 'OFF' }
    $tapeValue = if ($Tape) { 'ON' } else { 'OFF' }
    $arguments = @('-S',$repo,'-B',$build,"-DBUILD_TESTING=$testValue",
        "-DORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT=$qtpValue","-DORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT=$tapeValue",
        '-DCMAKE_DISABLE_FIND_PACKAGE_Python=TRUE','-DCMAKE_DISABLE_FIND_PACKAGE_Python2=TRUE',
        '-DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE')
    if ($Testing) { $arguments += "-DFETCHCONTENT_SOURCE_DIR_DOCTEST=$DoctestSource" }
    if ($IsWindows) {
        $arguments += @('-G','Visual Studio 17 2022','-A','x64')
        if ($Compiler -eq 'clang') { $arguments += @('-T','ClangCL') }
    } else {
        $arguments += @('-G','Ninja',"-DCMAKE_BUILD_TYPE=$Config","-DCMAKE_CXX_COMPILER=$gcc")
    }
    if ($Sanitize) {
        $arguments += @('-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie',
            '-DCMAKE_EXE_LINKER_FLAGS=-no-pie')
    }
    Invoke-Guarded "$Name-configure" $cmake $arguments
    $compilerRecords = @(Get-ChildItem -Path (Join-Path $build 'CMakeFiles/*/CMakeCXXCompiler.cmake'))
    if ($compilerRecords.Count -ne 1) { throw 'Compiler identity record missing or ambiguous' }
    $compilerRecord = Get-Content -LiteralPath $compilerRecords[0].FullName -Raw
    $expectedCompiler = if (!$IsWindows) { 'GNU' } elseif ($Compiler -eq 'clang') { 'Clang' } else { 'MSVC' }
    if (!$compilerRecord.Contains("set(CMAKE_CXX_COMPILER_ID `"$expectedCompiler`")")) {
        throw "Unexpected compiler in $Name; expected $expectedCompiler"
    }
    Invoke-Guarded "$Name-build" $cmake @('--build',$build,'--config',$Config,'-j','6')
    $cache = Get-Content (Join-Path $build 'CMakeCache.txt') -Raw
    if ($cache -match '(?im)^(Python[23]?_EXECUTABLE|_Python[23]?_EXECUTABLE):') { throw 'Cached interpreter reference' }
    if ($Testing) {
        Save-TestInventory $build $Config $Qtp $Tape $Name
        Invoke-Guarded "$Name-test" $ctest @('--test-dir',$build,'-C',$Config,'--parallel','4','--verbose','--output-on-failure')
    } else {
        if ((Test-Path -LiteralPath (Join-Path $build '_deps')) -or $cache -match '(?im)^.*doctest.*:') {
            throw 'Production build fetched/discovered test dependency'
        }
    }
}

try {
    Set-Location -LiteralPath $repo
    $aliases = Join-Path $work 'deny-aliases'
    [IO.Directory]::CreateDirectory($aliases) | Out-Null
    foreach ($name in @('python','python3','py')) {
        $file = Join-Path $aliases ($name + $(if ($IsWindows) { '.exe' } else { '' }))
        [IO.File]::Copy($Guard,$file)
        if (!$IsWindows) { & chmod +x $file; if ($LASTEXITCODE -ne 0) { throw 'chmod failed' } }
    }
    $env:PATH = $aliases + [IO.Path]::PathSeparator + $originalPath
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $env:MSBUILDDISABLENODEREUSE = '1'
    $env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
    # LeakSanitizer cannot inspect a ptraced process. ASan and UBSan remain enabled.
    if (!$IsWindows) { $env:ASAN_OPTIONS = 'detect_leaks=0'; $env:LSAN_OPTIONS = 'detect_leaks=0' }
    @{utc=[DateTime]::UtcNow.ToString('o');platform=$platform;guard=$Guard;guardSha256=(Get-FileHash $Guard).Hash;
        cmake=$cmake;ctest=$ctest;pwsh=$pwsh;gcc=$gcc;pythonControl=$PythonControl;path=$env:PATH;
        buildRoot=$work;asanOptions=$env:ASAN_OPTIONS;baselineHead=(git rev-parse HEAD)} |
        ConvertTo-Json | Set-Content -Encoding utf8NoBOM (Join-Path $EvidenceRoot 'environment.json')
    foreach ($name in @('python','python3','py','absolute')) {
        $command = if ($name -eq 'absolute') { $PythonControl } else { (Get-Command $name -CommandType Application | Select-Object -First 1).Source }
        $inventory = Join-Path $EvidenceRoot "control-$name.exec.tsv"
        & $Guard $inventory $command -c 'print("PYTHON_MUST_NOT_RUN")'
        if ($LASTEXITCODE -ne 97 -or !(Select-String -LiteralPath $inventory -Pattern '^DENIED')) {
            throw "Python denial control failed: $name"
        }
    }
    # A swallowed descendant failure must still fail the guard, not only a direct root launch.
    $controlScript = Join-Path $work 'descendant-control.ps1'
    [IO.File]::WriteAllText($controlScript, 'param($Interpreter); & $Interpreter -c ''print("PYTHON_MUST_NOT_RUN")''; exit 0',
        [Text.UTF8Encoding]::new($false))
    $inventory = Join-Path $EvidenceRoot 'control-descendant.exec.tsv'
    & $Guard $inventory $pwsh -NoProfile -File $controlScript $PythonControl
    if ($LASTEXITCODE -ne 97 -or !(Select-String -LiteralPath $inventory -Pattern '^DENIED')) {
        throw 'Descendant Python denial control failed'
    }
    if ($IsWindows) {
        $orphanChild = Join-Path $work 'guard-orphan-child.ps1'
        $orphanRoot = Join-Path $work 'guard-orphan-root.ps1'
        $readyPath = Join-Path $work 'guard-orphan-ready.txt'
        $pidPath = Join-Path $work 'guard-orphan-pid.txt'
        [IO.File]::WriteAllText($orphanChild, 'param($Ready); [IO.File]::WriteAllText($Ready,"ready"); Start-Sleep -Seconds 30')
        [IO.File]::WriteAllText($orphanRoot, @'
param($PowerShell,$Child,$Ready,$PidFile)
$start=[Diagnostics.ProcessStartInfo]::new($PowerShell)
$start.UseShellExecute=$false
$start.CreateNoWindow=$true
foreach($argument in @('-NoProfile','-File',$Child,$Ready)){$start.ArgumentList.Add($argument)}
$process=[Diagnostics.Process]::Start($start)
[IO.File]::WriteAllText($PidFile,[string]$process.Id)
$deadline=[DateTime]::UtcNow.AddSeconds(10)
while(!(Test-Path -LiteralPath $Ready) -and [DateTime]::UtcNow -lt $deadline){Start-Sleep -Milliseconds 10}
if(!(Test-Path -LiteralPath $Ready)){exit 1}
$process.Dispose()
exit 0
'@)
        $watch = [Diagnostics.Stopwatch]::StartNew()
        Invoke-Guarded 'guard-orphan' $pwsh @('-NoProfile','-File',$orphanRoot,$pwsh,$orphanChild,$readyPath,$pidPath)
        if ($watch.Elapsed.TotalSeconds -ge 15 -or (Get-Process -Id ([int][IO.File]::ReadAllText($pidPath)) -ErrorAction SilentlyContinue)) {
            throw 'Windows guard retained a live descendant after root exit'
        }
    }
    Invoke-Guarded 'logger' $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'Test-RunUtc.ps1'),
        '-WorkRoot',(Join-Path $work 'logger'))
    if ($IsWindows) {
        Invoke-Guarded 'setup' $pwsh @('-NoProfile','-File',(Join-Path $PSScriptRoot 'Test-SetupCpp.ps1'))
        Invoke-Guarded 'format-check' $pwsh @('-NoProfile','-File',(Join-Path $repo 'tools/format/Format-Cpp.ps1'),'-Mode','Check')
        foreach ($compiler in @('msvc','clang')) {
            foreach ($config in @('Debug','Release')) { Invoke-BuildCase "$compiler-$config" $compiler $config $true $true $true }
        }
    } else {
        foreach ($config in @('Debug','Release')) { Invoke-BuildCase "gcc-$config" 'gcc' $config $true $true $true }
        Invoke-BuildCase 'gcc-Asan' 'gcc' 'Debug' $true $true $true $true
    }
    foreach ($testing in @($false,$true)) {
        foreach ($qtp in @($false,$true)) {
            foreach ($tape in @($false,$true)) {
                Invoke-BuildCase "matrix-$([int]$testing)-$([int]$qtp)-$([int]$tape)" 'msvc' 'Release' $testing $qtp $tape
            }
        }
    }
    'All applicable tests executed without Python; no exclusions.' |
        Set-Content -Encoding utf8NoBOM (Join-Path $EvidenceRoot 'result.txt')
} finally {
    $env:PATH = $originalPath
    $env:ASAN_OPTIONS = $originalAsan
    $env:LSAN_OPTIONS = $originalLeak
    $env:PYTHONDONTWRITEBYTECODE = $originalBytecode
    $env:MSBUILDDISABLENODEREUSE = $originalMsbuild
    $env:DOTNET_CLI_TELEMETRY_OPTOUT = $originalCliTelemetry
}
