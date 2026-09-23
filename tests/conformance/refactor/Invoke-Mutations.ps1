param([string]$Run = '.work/refactor-026-mutations')
$ErrorActionPreference = 'Stop'
$env:PYTHONDONTWRITEBYTECODE = '1'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$runPath = [IO.Path]::GetFullPath($Run, $root)
if (!$runPath.StartsWith((Join-Path $root '.work/'), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Mutation copies must be under repository .work'
}
$source = Join-Path $runPath 'source'
$build = Join-Path $runPath 'build'
New-Item -ItemType Directory -Force $source | Out-Null
# Copy authored build inputs only. Immutable fixtures are supplied separately to each executable.
$paths = git -C $root ls-files --cached --others --exclude-standard -- CMakeLists.txt 'include/*.hpp' 'src/*.hpp' 'src/*.cpp' 'tests/conformance/*.hpp' 'tests/conformance/*.cpp' 'tests/conformance/*.cmake' 'tools/qtp-inspect/*.hpp' 'tools/qtp-inspect/*.cpp' 'tools/tape-inspect/*.hpp' 'tools/tape-inspect/*.cpp'
if ($LASTEXITCODE -ne 0) { throw 'Source inventory failed' }
foreach ($path in $paths) {
    $target = Join-Path $source $path
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $path) -Destination $target
}
cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON "-DFETCHCONTENT_SOURCE_DIR_DOCTEST=$root/.work/msvc/_deps/doctest-src"
if ($LASTEXITCODE -ne 0) { throw 'Mutation configure failed' }
function Build-Target([string]$Target) {
    cmake --build $build --config Release --target $Target -j 6
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Target" }
}
function Check-Run([string]$Target, [string[]]$Arguments, [bool]$Reject, [string]$Signal) {
    $output = & (Join-Path $build "Release/$Target.exe") @Arguments 2>&1 | Out-String
    $code = $LASTEXITCODE
    Write-Output $output
    if ($Reject) {
        if ($code -ne 1 -or !$output.Contains($Signal)) { throw "Expected assertion rejection: $Target, exit=$code" }
    } elseif ($code -ne 0) { throw "Baseline failed: $Target, exit=$code" }
    Write-Output "CHECK target=$Target reject=$Reject exit=$code"
}
$cases = @(
    @{ path = 'include/qd/core/serial_field_type.hpp'; before = 'SHORT_STRING = 0x20'; after = 'SHORT_STRING = 0x28';
       target = 'qdcxx_field_type_value_tests'; arguments = @("$root/tests/fixtures/encoding/field-types/expected/primitives.tsv"); signal = 'R::SHORT_STRING' },
    @{ path = 'tests/conformance/compact_tests.cpp'; before = 'check("01c280", {1, 1}, Status::INVALID_ENCODING, 3);'; after = 'check("01c280", {1, 1}, Status::INVALID_ENCODING, 2);';
       target = 'qdcxx_compact_tests'; arguments = @("$root/tests/fixtures/encoding/expected"); signal = 'result.consumed == consumed' },
    @{ path = 'tests/conformance/profile_tests.cpp'; before = 'decimalBits{-549755813760LL, 128, 549755813760LL}'; after = 'decimalBits{-549755813760LL, 0, 549755813760LL}';
       target = 'qdcxx_profile_tests'; arguments = @(); signal = 'result->beta.raw == decimalBits' }
)
foreach ($case in $cases) {
    $path = Join-Path $source $case.path
    $original = [IO.File]::ReadAllText($path)
    if (!$original.Contains($case.before)) { throw "Missing mutation target: $($case.path)" }
    Build-Target $case.target
    Check-Run $case.target $case.arguments $false ''
    try {
        [IO.File]::WriteAllText($path, $original.Replace($case.before, $case.after))
        Build-Target $case.target
        Check-Run $case.target $case.arguments $true $case.signal
    } finally {
        [IO.File]::WriteAllText($path, $original)
    }
    Build-Target $case.target
    Check-Run $case.target $case.arguments $false ''
}
Write-Output 'Three temporary-copy mutations rejected; every restored baseline passed.'
